#include "e1000.h"
#include "../pci/pci.h"
#include "../../memory/heap.h"
#include "../platform/serial.h"
#include "../../lib/string.h"
#include "../../network/protocol.h"
#include "../../network/tcp_client.h"
#include "../../memory/dma.h"

// Checksum implementation
static uint32_t sum16_bytes(const void* data, int len) {
    const uint8_t* b = (const uint8_t*)data;
    uint32_t sum = 0;
    while (len > 1) {
        sum += ((uint16_t)b[0] << 8) | (uint16_t)b[1];
        b += 2;
        len -= 2;
    }
    if (len) sum += ((uint16_t)b[0] << 8);
    return sum;
}

static uint16_t checksum16_be(const void* data, int len) {
    uint32_t sum = sum16_bytes(data, len);
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

static uint16_t checksum16_net_order(const void* data, int len) {
    // Returns a host-order 16-bit checksum computed over network-ordered bytes.
    // Store into headers with swap16().
    return checksum16_be(data, len);
}

static uint32_t ip4_to_u32_be(const uint8_t ip[4]) {
    return ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16) | ((uint32_t)ip[2] << 8) | (uint32_t)ip[3];
}

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
static uint64_t rx_descs_phys;
static uint64_t tx_descs_phys;
static uint64_t rx_buffers_phys;
static uint64_t tx_buffers_phys;
static uint32_t rx_ptr = 0;
static uint32_t tx_ptr = 0;
static uint8_t mac_addr[6];
static e1000_rx_callback_t g_rx_cb = 0;

static int e1000_resolve_next_hop_mac(const uint8_t dst_ip[4], uint8_t out_mac[6]);

// ---- Minimal ARP + next-hop resolution (QEMU user-net defaults) ----
typedef struct {
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t oper;
    uint8_t sha[6];
    uint8_t spa[4];
    uint8_t tha[6];
    uint8_t tpa[4];
} __attribute__((packed)) arp_hdr_t;

typedef struct {
    uint8_t ip[4];
    uint8_t mac[6];
    uint32_t last_seen_ticks;
    uint8_t valid;
} arp_entry_t;

#define ARP_CACHE_SIZE 8
static arp_entry_t g_arp_cache[ARP_CACHE_SIZE];
static uint32_t g_net_ticks = 0;
static const uint8_t g_ip_addr[4] = {10, 0, 2, 15};
static const uint8_t g_netmask[4] = {255, 255, 255, 0};
static const uint8_t g_gateway_ip[4] = {10, 0, 2, 2};
static int g_logged_gateway_mac = 0;
static int g_logged_first_rx = 0;
static int g_logged_first_arp = 0;

static int ip_equal(const uint8_t a[4], const uint8_t b[4]) {
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}

static void arp_cache_put(const uint8_t ip[4], const uint8_t mac[6]) {
    int free_idx = -1;
    int oldest_idx = 0;
    uint32_t oldest_tick = 0xFFFFFFFFu;

    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (g_arp_cache[i].valid) {
            if (ip_equal(g_arp_cache[i].ip, ip)) {
                memcpy(g_arp_cache[i].mac, mac, 6);
                g_arp_cache[i].last_seen_ticks = g_net_ticks;
                return;
            }
            if (g_arp_cache[i].last_seen_ticks < oldest_tick) {
                oldest_tick = g_arp_cache[i].last_seen_ticks;
                oldest_idx = i;
            }
        } else if (free_idx < 0) {
            free_idx = i;
        }
    }

    int idx = (free_idx >= 0) ? free_idx : oldest_idx;
    memcpy(g_arp_cache[idx].ip, ip, 4);
    memcpy(g_arp_cache[idx].mac, mac, 6);
    g_arp_cache[idx].last_seen_ticks = g_net_ticks;
    g_arp_cache[idx].valid = 1;

    if (!g_logged_gateway_mac && ip_equal(ip, g_gateway_ip)) {
        g_logged_gateway_mac = 1;
        serial_write_str("e1000: ARP learned gateway MAC: ");
        for (int i = 0; i < 6; i++) {
            serial_write_hex(mac[i]);
            if (i < 5) serial_write_str(":");
        }
        serial_write_str("\r\n");
    }
}

uint32_t e1000_tcp_get_ticks(void) { return g_net_ticks; }

static int arp_cache_lookup(const uint8_t ip[4], uint8_t out_mac[6]) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!g_arp_cache[i].valid) continue;
        if (ip_equal(g_arp_cache[i].ip, ip)) {
            memcpy(out_mac, g_arp_cache[i].mac, 6);
            return 1;
        }
    }
    return 0;
}

static int ip_is_same_subnet(const uint8_t a[4], const uint8_t b[4]) {
    for (int i = 0; i < 4; i++) {
        if ((a[i] & g_netmask[i]) != (b[i] & g_netmask[i])) return 0;
    }
    return 1;
}

static void e1000_send_arp_request(const uint8_t target_ip[4]) {
    uint8_t* buffer = tx_buffers + tx_ptr * PACKET_SIZE;
    memset(buffer, 0, PACKET_SIZE);

    eth_hdr_t* eth = (eth_hdr_t*)buffer;
    arp_hdr_t* arp = (arp_hdr_t*)(buffer + sizeof(eth_hdr_t));

    memset(eth->dst, 0xFF, 6);
    memcpy(eth->src, mac_addr, 6);
    eth->type = swap16(ETH_P_ARP);

    arp->htype = swap16(1);
    arp->ptype = swap16(ETH_P_IP);
    arp->hlen = 6;
    arp->plen = 4;
    arp->oper = swap16(1);
    memcpy(arp->sha, mac_addr, 6);
    memcpy(arp->spa, g_ip_addr, 4);
    memset(arp->tha, 0, 6);
    memcpy(arp->tpa, target_ip, 4);

    uint16_t total_len = sizeof(eth_hdr_t) + sizeof(arp_hdr_t);
    if (total_len < 60) total_len = 60;
    e1000_send_packet(buffer, total_len);
}

void e1000_send_tcp_packet_ex(const uint8_t dst_ip[4], uint16_t src_port, uint16_t dst_port,
                              uint32_t seq, uint32_t ack, uint8_t flags,
                              const void* data, uint16_t len) {
    uint8_t* buffer = tx_buffers + tx_ptr * PACKET_SIZE;
    memset(buffer, 0, PACKET_SIZE);

    eth_hdr_t* eth = (eth_hdr_t*)buffer;
    ip_hdr_t* ip = (ip_hdr_t*)(buffer + sizeof(eth_hdr_t));
    tcp_hdr_t* tcp = (tcp_hdr_t*)(buffer + sizeof(eth_hdr_t) + sizeof(ip_hdr_t));
    uint8_t* payload = (uint8_t*)(buffer + sizeof(eth_hdr_t) + sizeof(ip_hdr_t) + sizeof(tcp_hdr_t));

    if (len && data) memcpy(payload, data, len);

    // TCP
    tcp->src = swap16(src_port);
    tcp->dst = swap16(dst_port);
    tcp->seq = swap32(seq);
    tcp->ack = swap32(ack);
    tcp->res_off = (sizeof(tcp_hdr_t) / 4) << 4;
    tcp->flags = flags;
    tcp->window = swap16(8192);
    tcp->csum = 0;
    tcp->urg = 0;

    // IP
    ip->ver_ihl = 0x45;
    ip->tos = 0;
    ip->len = swap16(sizeof(ip_hdr_t) + sizeof(tcp_hdr_t) + len);
    ip->id = swap16(3);
    ip->frag = 0;
    ip->ttl = 64;
    ip->proto = IP_PROTO_TCP;
    memcpy(&ip->dst, dst_ip, 4);
    memcpy(&ip->src, g_ip_addr, 4);

    ip->csum = 0;
    ip->csum = swap16(checksum16_net_order(ip, sizeof(ip_hdr_t)));

    // TCP checksum (pseudo header)
    struct {
        uint32_t src;
        uint32_t dst;
        uint8_t zero;
        uint8_t proto;
        uint16_t len;
    } __attribute__((packed)) pseudo;

    uint8_t src_ip_bytes[4];
    memcpy(src_ip_bytes, g_ip_addr, 4);
    pseudo.src = *(uint32_t*)src_ip_bytes;
    pseudo.dst = *(uint32_t*)dst_ip;
    pseudo.zero = 0;
    pseudo.proto = IP_PROTO_TCP;
    pseudo.len = swap16(sizeof(tcp_hdr_t) + len);

    uint32_t sum = 0;
    sum += sum16_bytes(&pseudo, sizeof(pseudo));
    sum += sum16_bytes(tcp, (int)sizeof(tcp_hdr_t) + (int)len);
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    tcp->csum = swap16((uint16_t)(~sum));

    // Ethernet
    uint8_t dst_mac[6];
    if (!e1000_resolve_next_hop_mac(dst_ip, dst_mac)) return;
    memcpy(eth->dst, dst_mac, 6);
    memcpy(eth->src, mac_addr, 6);
    eth->type = swap16(ETH_P_IP);

    uint16_t total_len = sizeof(eth_hdr_t) + sizeof(ip_hdr_t) + sizeof(tcp_hdr_t) + len;
    if (total_len < 60) total_len = 60;
    e1000_send_packet(buffer, total_len);
}

void e1000_arp_probe_gateway(void) {
    if (!mmio_base || !tx_buffers) return;
    e1000_send_arp_request(g_gateway_ip);
}

static void e1000_send_arp_reply(const uint8_t target_mac[6], const uint8_t target_ip[4]) {
    uint8_t* buffer = tx_buffers + tx_ptr * PACKET_SIZE;
    memset(buffer, 0, PACKET_SIZE);

    eth_hdr_t* eth = (eth_hdr_t*)buffer;
    arp_hdr_t* arp = (arp_hdr_t*)(buffer + sizeof(eth_hdr_t));

    memcpy(eth->dst, target_mac, 6);
    memcpy(eth->src, mac_addr, 6);
    eth->type = swap16(ETH_P_ARP);

    arp->htype = swap16(1);
    arp->ptype = swap16(ETH_P_IP);
    arp->hlen = 6;
    arp->plen = 4;
    arp->oper = swap16(2);
    memcpy(arp->sha, mac_addr, 6);
    memcpy(arp->spa, g_ip_addr, 4);
    memcpy(arp->tha, target_mac, 6);
    memcpy(arp->tpa, target_ip, 4);

    uint16_t total_len = sizeof(eth_hdr_t) + sizeof(arp_hdr_t);
    if (total_len < 60) total_len = 60;
    e1000_send_packet(buffer, total_len);
}

static int e1000_resolve_next_hop_mac(const uint8_t dst_ip[4], uint8_t out_mac[6]) {
    const uint8_t* nh_ip = dst_ip;
    if (!ip_is_same_subnet(g_ip_addr, dst_ip)) nh_ip = g_gateway_ip;

    if (arp_cache_lookup(nh_ip, out_mac)) return 1;

    // Trigger ARP resolution, drop this packet for now.
    e1000_send_arp_request(nh_ip);
    return 0;
}

static void e1000_rx_handle_frame(const void* frame, uint16_t len) {
    if (len < sizeof(eth_hdr_t)) return;

    const eth_hdr_t* eth = (const eth_hdr_t*)frame;
    uint16_t et = swap16(eth->type);

    if (!g_logged_first_rx) {
        g_logged_first_rx = 1;
        serial_write_str("e1000: RX first frame len=");
        serial_write_dec(len);
        serial_write_str(" ethertype=");
        serial_write_hex(et);
        serial_write_str("\r\n");
    }

    if (et == ETH_P_ARP) {
        if (!g_logged_first_arp) {
            g_logged_first_arp = 1;
            serial_write_str("e1000: RX first ARP frame\r\n");
        }
        if (len < (uint16_t)(sizeof(eth_hdr_t) + sizeof(arp_hdr_t))) return;
        const arp_hdr_t* arp = (const arp_hdr_t*)((const uint8_t*)frame + sizeof(eth_hdr_t));

        if (swap16(arp->htype) != 1) return;
        if (swap16(arp->ptype) != ETH_P_IP) return;
        if (arp->hlen != 6 || arp->plen != 4) return;

        // Learn sender
        arp_cache_put(arp->spa, arp->sha);

        // Reply if request is for us
        if (swap16(arp->oper) == 1 && ip_equal(arp->tpa, g_ip_addr)) {
            e1000_send_arp_reply(arp->sha, arp->spa);
        }
        return;
    }

    if (et == ETH_P_IP) {
        if (len < (uint16_t)(sizeof(eth_hdr_t) + sizeof(ip_hdr_t))) return;
        const ip_hdr_t* ip = (const ip_hdr_t*)((const uint8_t*)frame + sizeof(eth_hdr_t));
        uint8_t ihl = (ip->ver_ihl & 0x0F);
        if (ihl < 5) return;
        uint16_t ip_hdr_len = (uint16_t)ihl * 4;
        if (len < (uint16_t)(sizeof(eth_hdr_t) + ip_hdr_len)) return;

        uint16_t total_len = swap16(ip->len);
        if (total_len < ip_hdr_len) return;
        if (total_len > (uint16_t)(len - sizeof(eth_hdr_t))) total_len = (uint16_t)(len - sizeof(eth_hdr_t));

        const uint8_t* src_ip = (const uint8_t*)&ip->src;
        const uint8_t* payload = (const uint8_t*)ip + ip_hdr_len;
        uint16_t payload_len = total_len - ip_hdr_len;

        if (ip->proto == IP_PROTO_TCP) {
            tcp_on_packet(src_ip, payload, payload_len);
        }
        return;
    }

    // TODO: IP demux (UDP/TCP) will be layered here next.
}
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

static void e1000_program_rar0_from_mac(void) {
    uint32_t ral = (uint32_t)mac_addr[0] |
                   ((uint32_t)mac_addr[1] << 8) |
                   ((uint32_t)mac_addr[2] << 16) |
                   ((uint32_t)mac_addr[3] << 24);
    uint32_t rah = (uint32_t)mac_addr[4] |
                   ((uint32_t)mac_addr[5] << 8) |
                   (1u << 31); // AV
    e1000_write(E1000_REG_RAL, ral);
    e1000_write(E1000_REG_RAH, rah);
}

uint8_t* e1000_get_mac_addr() {
    return mac_addr;
}

uint32_t e1000_get_status_reg() {
    if (!mmio_base) return 0;
    return e1000_read(E1000_REG_STATUS);
}

void e1000_set_rx_callback(e1000_rx_callback_t cb) { g_rx_cb = cb; }

void e1000_poll_rx(void) {
    if (!mmio_base || !rx_descs || !rx_buffers) return;

    g_net_ticks++;

    // Process all completed descriptors starting at rx_ptr.
    while (1) {
        dma_sync_for_cpu(&rx_descs[rx_ptr], sizeof(e1000_rx_desc_t));
        if (!(rx_descs[rx_ptr].status & 0x1)) break;
        uint16_t len = rx_descs[rx_ptr].length;
        void* frame = (void*)(rx_buffers + (rx_ptr * PACKET_SIZE));

        if (len > 0) {
            dma_sync_for_cpu(frame, len);
            // Always feed our minimal RX handler first.
            e1000_rx_handle_frame(frame, len);
            if (g_rx_cb) g_rx_cb(frame, len);
        }

        rx_descs[rx_ptr].status = 0;
        dma_sync_for_device(&rx_descs[rx_ptr], sizeof(e1000_rx_desc_t));

        // Give descriptor back to hardware.
        e1000_write(E1000_REG_RDT, rx_ptr);
        rx_ptr = (rx_ptr + 1) % RX_DESC_COUNT;
    }
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

    // Reset clears receive address filters; re-program our MAC into RAR0.
    e1000_program_rar0_from_mac();

    e1000_write(E1000_REG_CTRL, e1000_read(E1000_REG_CTRL) | E1000_CTRL_SLU | E1000_CTRL_ASDE);

    // Initialize Multicast Table Array (zero it out)
    for(int i = 0; i < 128; i++) {
        e1000_write(0x5200 + (i * 4), 0);
    }

    // Allocate Rings
    rx_descs = (e1000_rx_desc_t*)dma_alloc_coherent(sizeof(e1000_rx_desc_t) * RX_DESC_COUNT, &rx_descs_phys);
    tx_descs = (e1000_tx_desc_t*)dma_alloc_coherent(sizeof(e1000_tx_desc_t) * TX_DESC_COUNT, &tx_descs_phys);

    // Allocate Buffers
    rx_buffers = (uint8_t*)dma_alloc_coherent(PACKET_SIZE * RX_DESC_COUNT, &rx_buffers_phys);
    tx_buffers = (uint8_t*)dma_alloc_coherent(PACKET_SIZE * TX_DESC_COUNT, &tx_buffers_phys);

    if (!rx_descs || !tx_descs || !rx_buffers || !tx_buffers) {
        serial_write_str("e1000: DMA alloc failed!\r\n");
        return;
    }

    memset(rx_descs, 0, sizeof(e1000_rx_desc_t) * RX_DESC_COUNT);
    memset(tx_descs, 0, sizeof(e1000_tx_desc_t) * TX_DESC_COUNT);

    for (int i = 0; i < RX_DESC_COUNT; i++) {
        rx_descs[i].addr = rx_buffers_phys + (uint64_t)(i * PACKET_SIZE);
        rx_descs[i].status = 0;
    }

    dma_sync_for_device(rx_descs, sizeof(e1000_rx_desc_t) * RX_DESC_COUNT);
    dma_sync_for_device(tx_descs, sizeof(e1000_tx_desc_t) * TX_DESC_COUNT);

    // Configure RX
    e1000_write(E1000_REG_RDBAL, (uint32_t)rx_descs_phys);
    e1000_write(E1000_REG_RDBAH, (uint32_t)(rx_descs_phys >> 32));
    e1000_write(E1000_REG_RDLEN, RX_DESC_COUNT * sizeof(e1000_rx_desc_t));
    e1000_write(E1000_REG_RDH, 0);
    e1000_write(E1000_REG_RDT, RX_DESC_COUNT - 1);
    e1000_write(E1000_REG_RCTL, E1000_RCTL_EN | E1000_RCTL_SBP | E1000_RCTL_BAM | E1000_RCTL_SECRC);

    // Configure TX
    e1000_write(E1000_REG_TDBAL, (uint32_t)tx_descs_phys);
    e1000_write(E1000_REG_TDBAH, (uint32_t)(tx_descs_phys >> 32));
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
    // Ensure packet visible to device and use physical address.
    dma_sync_for_device((void*)data, length);
    uint64_t phys = virt_to_phys((void*)data);

    tx_descs[tx_ptr].addr = phys;
    tx_descs[tx_ptr].length = length;
    tx_descs[tx_ptr].cmd = (1 << 0) | (1 << 1) | (1 << 3); // EOP, IFCS, RS
    tx_descs[tx_ptr].status = 0;

    dma_sync_for_device(&tx_descs[tx_ptr], sizeof(e1000_tx_desc_t));

    uint32_t old_ptr = tx_ptr;
    tx_ptr = (tx_ptr + 1) % TX_DESC_COUNT;
    e1000_write(E1000_REG_TDT, tx_ptr);

    // Wait for transmit with timeout
    int timeout = 1000000;
    while(!(tx_descs[old_ptr].status & 0xF) && timeout > 0) {
        dma_sync_for_cpu(&tx_descs[old_ptr], sizeof(e1000_tx_desc_t));
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
    uint8_t src_ip[4] = {10, 0, 2, 15};
    memcpy(&ip->src, src_ip, 4);
    
    ip->csum = 0;
    ip->csum = swap16(checksum16_net_order(ip, sizeof(ip_hdr_t)));
    
    // Ethernet
    uint8_t dst_mac[6];
    if (!e1000_resolve_next_hop_mac(dst_ip, dst_mac)) return;
    memcpy(eth->dst, dst_mac, 6);
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
    uint8_t src_ip[4] = {10, 0, 2, 15};
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
    sum += sum16_bytes(&pseudo, sizeof(pseudo));
    sum += sum16_bytes(tcp, (int)sizeof(tcp_hdr_t) + (int)len);

    while(sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    tcp->csum = swap16((uint16_t)(~sum));
    
    // Ethernet
    uint8_t dst_mac[6];
    if (!e1000_resolve_next_hop_mac(dst_ip, dst_mac)) return;
    memcpy(eth->dst, dst_mac, 6);
    memcpy(eth->src, mac_addr, 6);
    eth->type = swap16(ETH_P_IP);
    
    uint16_t total_len = sizeof(eth_hdr_t) + sizeof(ip_hdr_t) + sizeof(tcp_hdr_t) + len;
    if (total_len < 60) total_len = 60;
    e1000_send_packet(buffer, total_len);
}

