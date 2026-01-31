#include "e1000.h"
#include "../pci/pci.h"
#include "../../memory/heap.h"
#include "../platform/serial.h"
#include "../../lib/string.h"
#include "../../network/protocol.h"

// Checksum implementation
static uint16_t checksum(void* data, int len) {
    uint32_t sum = 0;
    uint16_t* p = (uint16_t*)data;
    while(len > 1) {
        sum += *p++;
        len -= 2;
    }
    if(len) sum += *(uint8_t*)p;
    while(sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return ~sum;
}

void e1000_send_udp_packet(uint8_t* dst_ip, uint16_t dst_port, const void* data, uint16_t len);
void e1000_send_tcp_packet(uint8_t* dst_ip, uint16_t dst_port, const void* data, uint16_t len, uint8_t flags);

#define RX_DESC_COUNT 128
#define TX_DESC_COUNT 128
#define PACKET_SIZE   2048

static uintptr_t mmio_base;
static e1000_rx_desc_t* rx_descs;
static e1000_tx_desc_t* tx_descs;
static uint8_t* rx_buffers;
static uint8_t* tx_buffers;
static uint32_t rx_ptr = 0;
static uint32_t tx_ptr = 0;
static uint8_t mac_addr[6];
static char net_log_buf[1024];
static int net_log_idx = 0;
static int in_net_log = 0;

void e1000_net_log_callback(char c) {
    if (in_net_log) return;
    in_net_log = 1;
    
    net_log_buf[net_log_idx++] = c;
    if (c == '\n' || net_log_idx >= 1000) {
        uint8_t target_ip[4] = {172, 16, 6, 110};
        e1000_send_udp_packet(target_ip, 7654, net_log_buf, net_log_idx);
        net_log_idx = 0;
    }
    
    in_net_log = 0;
}

static inline void e1000_write(uint32_t reg, uint32_t val) {
    *(volatile uint32_t*)(mmio_base + reg) = val;
}

static inline uint32_t e1000_read(uint32_t reg) {
    return *(volatile uint32_t*)(mmio_base + reg);
}

static void e1000_read_mac() {
    uint32_t ral = e1000_read(E1000_REG_RAL);
    uint32_t rah = e1000_read(E1000_REG_RAH);
    
    mac_addr[0] = ral & 0xFF;
    mac_addr[1] = (ral >> 8) & 0xFF;
    mac_addr[2] = (ral >> 16) & 0xFF;
    mac_addr[3] = (ral >> 24) & 0xFF;
    mac_addr[4] = rah & 0xFF;
    mac_addr[5] = (rah >> 8) & 0xFF;
    
    serial_write_str("e1000: MAC: ");
    for(int i=0; i<6; i++) {
        serial_write_hex(mac_addr[i]);
        if(i < 5) serial_write_str(":");
    }
    serial_write_str("\r\n");
}

uint8_t* e1000_get_mac_addr() {
    return mac_addr;
}

uint32_t e1000_get_status_reg() {
    if (!mmio_base) return 0;
    return e1000_read(E1000_REG_STATUS);
}

void e1000_init(uintptr_t base_addr) {
    mmio_base = base_addr;
    serial_write_str("e1000: Initializing at ");
    serial_write_hex(base_addr);
    serial_write_str("\r\n");

    e1000_read_mac();

    // Reset Controller
    serial_write_str("e1000: Resetting controller...\r\n");
    e1000_write(E1000_REG_CTRL, e1000_read(E1000_REG_CTRL) | E1000_CTRL_RST);
    
    // Intel Recommendation: Wait 1us after setting RST before first read
    for(volatile int i=0; i<10000; i++); 

    // Wait for reset to clear
    int timeout = 10000;
    while((e1000_read(E1000_REG_CTRL) & E1000_CTRL_RST) && timeout > 0) {
        for(volatile int i=0; i<1000; i++);
        timeout--;
    }

    if (timeout == 0) {
        serial_write_str("e1000: Reset timed out!\r\n");
    } else {
        serial_write_str("e1000: Reset successful.\r\n");
    }

    e1000_write(E1000_REG_CTRL, e1000_read(E1000_REG_CTRL) | E1000_CTRL_SLU | E1000_CTRL_ASDE);

    // Initialize Multicast Table Array (zero it out)
    for(int i = 0; i < 128; i++) {
        e1000_write(0x5200 + (i * 4), 0);
    }

    // Allocate Rings
    rx_descs = (e1000_rx_desc_t*)kmalloc(sizeof(e1000_rx_desc_t) * RX_DESC_COUNT + 16);
    tx_descs = (e1000_tx_desc_t*)kmalloc(sizeof(e1000_tx_desc_t) * TX_DESC_COUNT + 16);
    
    // Align to 16 bytes
    rx_descs = (e1000_rx_desc_t*)(((uintptr_t)rx_descs + 15) & ~15);
    tx_descs = (e1000_tx_desc_t*)(((uintptr_t)tx_descs + 15) & ~15);

    memset(rx_descs, 0, sizeof(e1000_rx_desc_t) * RX_DESC_COUNT);
    memset(tx_descs, 0, sizeof(e1000_tx_desc_t) * TX_DESC_COUNT);

    // Allocate Buffers
    rx_buffers = (uint8_t*)kmalloc(PACKET_SIZE * RX_DESC_COUNT);
    tx_buffers = (uint8_t*)kmalloc(PACKET_SIZE * TX_DESC_COUNT);

    for (int i = 0; i < RX_DESC_COUNT; i++) {
        rx_descs[i].addr = (uint64_t)(rx_buffers + (i * PACKET_SIZE));
        rx_descs[i].status = 0;
    }

    // Configure RX
    e1000_write(E1000_REG_RDBAL, (uint32_t)(uintptr_t)rx_descs);
    e1000_write(E1000_REG_RDBAH, (uint32_t)((uintptr_t)rx_descs >> 32));
    e1000_write(E1000_REG_RDLEN, RX_DESC_COUNT * sizeof(e1000_rx_desc_t));
    e1000_write(E1000_REG_RDH, 0);
    e1000_write(E1000_REG_RDT, RX_DESC_COUNT - 1);
    e1000_write(E1000_REG_RCTL, E1000_RCTL_EN | E1000_RCTL_SBP | E1000_RCTL_BAM | E1000_RCTL_SECRC);

    // Configure TX
    e1000_write(E1000_REG_TDBAL, (uint32_t)(uintptr_t)tx_descs);
    e1000_write(E1000_REG_TDBAH, (uint32_t)((uintptr_t)tx_descs >> 32));
    e1000_write(E1000_REG_TDLEN, TX_DESC_COUNT * sizeof(e1000_tx_desc_t));
    e1000_write(E1000_REG_TDH, 0);
    e1000_write(E1000_REG_TDT, 0);
    e1000_write(E1000_REG_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP | (0x0F << 4) | (0x40 << 12));
    // Inter-packet gap
    e1000_write(E1000_REG_TIPG, 0x0060200A); 

    serial_write_str("e1000: Initialization Complete.\r\n");
}

void e1000_enable_busmaster(uint8_t bus, uint8_t dev, uint8_t func) {
    uint16_t cmd = pci_read16(bus, dev, func, PCI_COMMAND);
    cmd |= (1 << 1) | (1 << 2); // MEM + BUSMASTER
    pci_write16(bus, dev, func, PCI_COMMAND, cmd);
}

void e1000_send_packet(const void* data, uint16_t length) {
    // Flush cache to ensure hardware sees the data
    __asm__ volatile("wbinvd" ::: "memory");

    tx_descs[tx_ptr].addr = (uint64_t)data; 
    tx_descs[tx_ptr].length = length;
    tx_descs[tx_ptr].cmd = (1 << 0) | (1 << 1) | (1 << 3); // EOP, IFCS, RS
    tx_descs[tx_ptr].status = 0;

    __asm__ volatile("wbinvd" ::: "memory");

    uint32_t old_ptr = tx_ptr;
    tx_ptr = (tx_ptr + 1) % TX_DESC_COUNT;
    e1000_write(E1000_REG_TDT, tx_ptr);

    // Wait for transmit with timeout
    int timeout = 1000000;
    while(!(tx_descs[old_ptr].status & 0xF) && timeout > 0) {
        timeout--;
        for(volatile int i=0; i<100; i++); // Short delay
    }
    
    if (timeout == 0) {
        serial_write_str("e1000: TX Timed Out!\r\n");
    }
}

void e1000_send_raw_broadcast_test(void) {
    uint8_t dummy[32];
    for (int i = 0; i < 32; i++) dummy[i] = (uint8_t)i;
    uint8_t buf[64] = {0};
    eth_hdr_t* eth = (eth_hdr_t*)buf;
    memset(eth->dst, 0xFF, 6);
    memcpy(eth->src, mac_addr, 6);
    eth->type = swap16(ETH_P_IP);
    memcpy(buf + sizeof(eth_hdr_t), dummy, sizeof(dummy));
    uint16_t pkt_len = sizeof(eth_hdr_t) + sizeof(dummy);
    if (pkt_len < 60) pkt_len = 60;
    e1000_send_packet(buf, pkt_len);
}

void e1000_send_udp_packet(uint8_t* dst_ip, uint16_t dst_port, const void* data, uint16_t len) {
    uint8_t* buffer = tx_buffers + tx_ptr * PACKET_SIZE;
    memset(buffer, 0, PACKET_SIZE);
    
    eth_hdr_t* eth = (eth_hdr_t*)buffer;
    ip_hdr_t* ip = (ip_hdr_t*)(buffer + sizeof(eth_hdr_t));
    udp_hdr_t* udp = (udp_hdr_t*)(buffer + sizeof(eth_hdr_t) + sizeof(ip_hdr_t));
    uint8_t* payload = (uint8_t*)(buffer + sizeof(eth_hdr_t) + sizeof(ip_hdr_t) + sizeof(udp_hdr_t));
    
    // Payload
    memcpy(payload, data, len);
    
    // UDP
    udp->src = swap16(1234);
    udp->dst = swap16(dst_port);
    udp->len = swap16(sizeof(udp_hdr_t) + len);
    udp->csum = 0; 
    
    // IP
    ip->ver_ihl = 0x45;
    ip->tos = 0;
    ip->len = swap16(sizeof(ip_hdr_t) + sizeof(udp_hdr_t) + len);
    ip->id = swap16(1);
    ip->frag = 0;
    ip->ttl = 64;
    ip->proto = IP_PROTO_UDP;
    memcpy(&ip->dst, dst_ip, 4);
    uint8_t src_ip[4] = {172, 16, 6, 222};
    memcpy(&ip->src, src_ip, 4);
    
    ip->csum = 0;
    ip->csum = checksum(ip, sizeof(ip_hdr_t));
    
    // Ethernet
    memset(eth->dst, 0xFF, 6); // Broadcast
    memcpy(eth->src, mac_addr, 6);
    eth->type = swap16(ETH_P_IP); 
    
    uint16_t total_len = sizeof(eth_hdr_t) + sizeof(ip_hdr_t) + sizeof(udp_hdr_t) + len;
    if (total_len < 60) total_len = 60;
    e1000_send_packet(buffer, total_len);
}

void e1000_send_tcp_packet(uint8_t* dst_ip, uint16_t dst_port, const void* data, uint16_t len, uint8_t flags) {
    uint8_t* buffer = tx_buffers + tx_ptr * PACKET_SIZE;
    memset(buffer, 0, PACKET_SIZE);
    
    eth_hdr_t* eth = (eth_hdr_t*)buffer;
    ip_hdr_t* ip = (ip_hdr_t*)(buffer + sizeof(eth_hdr_t));
    tcp_hdr_t* tcp = (tcp_hdr_t*)(buffer + sizeof(eth_hdr_t) + sizeof(ip_hdr_t));
    uint8_t* payload = (uint8_t*)(buffer + sizeof(eth_hdr_t) + sizeof(ip_hdr_t) + sizeof(tcp_hdr_t));
    
    memcpy(payload, data, len);
    
    // TCP
    tcp->src = swap16(4321);
    tcp->dst = swap16(dst_port);
    tcp->seq = swap32(100);
    tcp->ack = 0;
    tcp->res_off = (sizeof(tcp_hdr_t) / 4) << 4;
    tcp->flags = flags;
    tcp->window = swap16(8192);
    tcp->csum = 0;
    tcp->urg = 0;
    
    // IP
    ip->ver_ihl = 0x45;
    ip->tos = 0;
    ip->len = swap16(sizeof(ip_hdr_t) + sizeof(tcp_hdr_t) + len);
    ip->id = swap16(2);
    ip->frag = 0;
    ip->ttl = 64;
    ip->proto = IP_PROTO_TCP;
    memcpy(&ip->dst, dst_ip, 4);
    uint8_t src_ip[4] = {172, 16, 6, 222};
    memcpy(&ip->src, src_ip, 4);
    
    ip->csum = 0;
    ip->csum = checksum(ip, sizeof(ip_hdr_t));

    // TCP Checksum (Pseudo-header)
    struct {
        uint32_t src;
        uint32_t dst;
        uint8_t zero;
        uint8_t proto;
        uint16_t len;
    } __attribute__((packed)) pseudo;

    pseudo.src = *(uint32_t*)src_ip;
    pseudo.dst = *(uint32_t*)dst_ip;
    pseudo.zero = 0;
    pseudo.proto = IP_PROTO_TCP;
    pseudo.len = swap16(sizeof(tcp_hdr_t) + len);

    uint32_t sum = 0;
    uint16_t* p = (uint16_t*)&pseudo;
    for(int i=0; i<6; i++) sum += p[i]; // 12 bytes

    p = (uint16_t*)tcp;
    int tcp_len = sizeof(tcp_hdr_t) + len;
    while(tcp_len > 1) {
        sum += *p++;
        tcp_len -= 2;
    }
    if(tcp_len) sum += *(uint8_t*)p;

    while(sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    tcp->csum = ~sum;
    
    // Ethernet
    memset(eth->dst, 0xFF, 6);
    memcpy(eth->src, mac_addr, 6);
    eth->type = swap16(ETH_P_IP);
    
    uint16_t total_len = sizeof(eth_hdr_t) + sizeof(ip_hdr_t) + sizeof(tcp_hdr_t) + len;
    if (total_len < 60) total_len = 60;
    e1000_send_packet(buffer, total_len);
}

