#include <drivers/net/e1000.h>
#include <kernel/pci.h>
#include <kernel/mm/pmm.h>
#include <kernel/printk.h>
#include <kernel/serial.h>
#include <lib/string.h>

static uintptr_t mmio_base;
static volatile struct e1000_rx_desc *rx_descs;
static volatile struct e1000_tx_desc *tx_descs;
static uint8_t *rx_buffers;
static uint8_t *tx_buffers;
static uint16_t rx_cur = 0;
static uint16_t tx_cur = 0;
static uint8_t mac[6];

static inline void e1000_write(uint32_t reg, uint32_t val) {
    volatile uint32_t *dest = (volatile uint32_t*)(mmio_base + reg);
    *dest = val;
}

static inline uint32_t e1000_read(uint32_t reg) {
    volatile uint32_t *src = (volatile uint32_t*)(mmio_base + reg);
    return *src;
}

static void e1000_read_mac() {
    uint32_t low = e1000_read(REG_RAL);
    uint32_t high = e1000_read(REG_RAH);

    mac[0] = low & 0xFF;
    mac[1] = (low >> 8) & 0xFF;
    mac[2] = (low >> 16) & 0xFF;
    mac[3] = (low >> 24) & 0xFF;
    mac[4] = high & 0xFF;
    mac[5] = (high >> 8) & 0xFF;
}

void e1000_get_mac(uint8_t* out_mac) {
    for (int i = 0; i < 6; i++) out_mac[i] = mac[i];
}

void e1000_init(uint8_t bus, uint8_t slot, uint8_t func) {
    printk("Initializing e1000...\n");

    // Enable PCI Bus Mastering and Memory Space
    uint32_t pci_command = pci_config_read32(bus, slot, func, 0x04);
    pci_command |= (1 << 1) | (1 << 2); // Memory Space | Bus Master
    pci_config_write32(bus, slot, func, 0x04, pci_command);

    // Get BAR0 (Memory Mapped I/O)
    uint32_t bar0 = pci_config_read32(bus, slot, func, 0x10);
    mmio_base = (uintptr_t)(bar0 & ~0xF);
    if ((bar0 & 0x6) == 0x4) { // 64-bit BAR
        mmio_base |= ((uint64_t)pci_config_read32(bus, slot, func, 0x14)) << 32;
    }

    printk("  MMIO Base: %p\n", (void*)mmio_base);

    // Reset the device
    e1000_write(REG_CTRL, e1000_read(REG_CTRL) | (1 << 26)); // RST bit
    for(volatile int i=0; i<10000; i++); // Wait for reset

    e1000_read_mac();
    printk("  MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // Set MAC Address Filter
    uint32_t low = mac[0] | (mac[1] << 8) | (mac[2] << 16) | (mac[3] << 24);
    uint32_t high = mac[4] | (mac[5] << 8) | (1 << 31); // 1 << 31 is Address Valid (AV) bit
    e1000_write(REG_RAL, low);
    e1000_write(REG_RAH, high);

    // Enable Link
    e1000_write(REG_CTRL, e1000_read(REG_CTRL) | CTRL_SLU);
    
    // Wait for link up
    while (!(e1000_read(REG_STATUS) & (1 << 1))); 

    // Multicast Table Array (zero out)
    for (int i = 0; i < 128; i++) {
        e1000_write(REG_MTA + (i * 4), 0);
    }

    // Initialize RX
    rx_descs = (struct e1000_rx_desc*)pmm_alloc_page(); // Needs to be 16-byte aligned, pmm_alloc_page is page-aligned
    rx_buffers = (uint8_t*)pmm_alloc_page(); // Simplification: one page for buffers
    // Actually we need more pages for buffers if we have 128 descriptors of 2048 bytes each
    // 128 * 2048 = 256KB = 64 pages.
    for (int i = 0; i < 63; i++) pmm_alloc_page(); // Reserve enough pages

    memset(rx_descs, 0, sizeof(struct e1000_rx_desc) * E1000_NUM_RX_DESC);
    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        rx_descs[i].addr = (uint64_t)rx_buffers + (i * 2048);
        rx_descs[i].status = 0;
    }

    e1000_write(REG_RDBAL, (uint32_t)(uint64_t)rx_descs);
    e1000_write(REG_RDBAH, (uint32_t)((uint64_t)rx_descs >> 32));
    e1000_write(REG_RDLEN, E1000_NUM_RX_DESC * sizeof(struct e1000_rx_desc));
    e1000_write(REG_RDH, 0);
    e1000_write(REG_RDT, E1000_NUM_RX_DESC - 1);
    e1000_write(REG_RCTL, RCTL_EN | RCTL_BAM | RCTL_SECRC | RCTL_BSIZE_2048);

    // Initialize TX
    tx_descs = (struct e1000_tx_desc*)pmm_alloc_page();
    tx_buffers = (uint8_t*)pmm_alloc_page();
    for (int i = 0; i < 63; i++) pmm_alloc_page(); // Reserve enough pages

    memset(tx_descs, 0, sizeof(struct e1000_tx_desc) * E1000_NUM_TX_DESC);
    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        tx_descs[i].addr = (uint64_t)tx_buffers + (i * 2048);
        tx_descs[i].status = 0x1; // Done
    }

    e1000_write(REG_TDBAL, (uint32_t)(uint64_t)tx_descs);
    e1000_write(REG_TDBAH, (uint32_t)((uint64_t)tx_descs >> 32));
    e1000_write(REG_TDLEN, E1000_NUM_TX_DESC * sizeof(struct e1000_tx_desc));
    e1000_write(REG_TDH, 0);
    e1000_write(REG_TDT, 0);
    
    // TCTL: EN (1), PSP (3), CT=0x0F (4-11), COLD=0x3F (12-21)
    e1000_write(REG_TCTL, TCTL_EN | TCTL_PSP | (0x0F << 4) | (0x3F << 12));
    e1000_write(REG_TIPG, 0x0060200A); // IPGT=10, IPGR1=8, IPGR2=6

    printk("  e1000 initialized.\n");
}

void e1000_send_packet(const void* data, uint16_t len) {
    while (!(tx_descs[tx_cur].status & 0x1)); // Wait for DD (Descriptor Done) bit

    memcpy((void*)tx_descs[tx_cur].addr, data, len);
    tx_descs[tx_cur].length = len;
    tx_descs[tx_cur].status = 0;
    tx_descs[tx_cur].cmd = (1 << 0) | (1 << 1) | (1 << 3); // EOP | IFCS | RS (Report Status)

    __asm__ volatile("" : : : "memory"); // Compiler barrier

    tx_cur = (tx_cur + 1) % E1000_NUM_TX_DESC;
    e1000_write(REG_TDT, tx_cur);
}

int e1000_receive_packet(void* buffer, uint16_t max_len) {
    if (!(rx_descs[rx_cur].status & 0x1)) {
        return -1;
    }

    uint16_t len = rx_descs[rx_cur].length;
    if (len > max_len) len = max_len;

    memcpy(buffer, (void*)rx_descs[rx_cur].addr, len);

    rx_descs[rx_cur].status = 0;
    uint16_t old_rx = rx_cur;
    rx_cur = (rx_cur + 1) % E1000_NUM_RX_DESC;
    e1000_write(REG_RDT, old_rx);

    return len;
}

