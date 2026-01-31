#include "e1000e.h"
#include "../../memory/dma.h"
#include "../platform/serial.h"
#include "../../lib/string.h"
#include "../pci/pci.h"
#include "../../network/protocol.h"

// Checksum implementation
static uint16_t checksum(void *data, int len) {
  uint32_t sum = 0;
  uint16_t *p = (uint16_t *)data;
  while (len > 1) {
    sum += *p++;
    len -= 2;
  }
  if (len) sum += *(uint8_t *)p;
  while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
  return ~sum;
}

void e1000e_send_udp_packet(uint8_t *dst_ip, uint16_t dst_port, const void *data, uint16_t len);
void e1000e_send_tcp_packet(uint8_t *dst_ip, uint16_t dst_port, const void *data, uint16_t len, uint8_t flags);

#define RX_DESC_COUNT 128
#define TX_DESC_COUNT 128
#define PACKET_SIZE 2048

static uintptr_t mmio_base;
static e1000e_rx_desc_t *rx_descs;
static e1000e_tx_desc_t *tx_descs;
static uint8_t *rx_buffers;
static uint8_t *tx_buffers;
static uint32_t rx_ptr = 0;
static uint32_t tx_ptr = 0;
static uint8_t mac_addr[6];
static char net_log_buf[1024];
static int net_log_idx = 0;
static int in_net_log = 0;

static inline void e1000e_write(uint32_t reg, uint32_t val)
{
  *(volatile uint32_t *)(mmio_base + reg) = val;
}

static inline uint32_t e1000e_read(uint32_t reg)
{
  return *(volatile uint32_t *)(mmio_base + reg);
}

void e1000e_spam_udp_test(void)
{
    uint8_t ip[4] = {172, 16, 6, 110};
    const char msg[] = "hello from tiny64";

    while (1) {
        e1000e_send_udp_packet(ip, 7654, msg, sizeof(msg)-1);
        for (volatile int j = 0; j < 50000000; j++);
    }
}


void e1000e_net_log_callback(char c)
{
  if (in_net_log)
    return;
  in_net_log = 1;

  net_log_buf[net_log_idx++] = c;
  if (c == '\n' || net_log_idx >= 1000)
  {
    uint8_t target_ip[4] = {172, 16, 6, 110};
    e1000e_send_udp_packet(target_ip, 7654, net_log_buf, net_log_idx);
    net_log_idx = 0;
  }

  in_net_log = 0;
}

void e1000e_enable_busmaster(uint8_t bus, uint8_t dev, uint8_t func)
{
  uint16_t cmd = pci_read16(bus, dev, func, PCI_COMMAND);
  cmd |= (1 << 1) | (1 << 2); // MEM + BUSMASTER
  pci_write16(bus, dev, func, PCI_COMMAND, cmd);
}

void e1000e_send_raw_broadcast(const void *payload, uint16_t len)
{
    // Use the same per-descriptor buffer scheme as UDP
    uint8_t *buffer = tx_buffers + tx_ptr * PACKET_SIZE;

    // Clear the whole buffer so we don't leak junk
    memset(buffer, 0, PACKET_SIZE);

    eth_hdr_t *eth = (eth_hdr_t *)buffer;
    uint8_t *data = buffer + sizeof(eth_hdr_t);

    // Copy user payload
    if (len > PACKET_SIZE - sizeof(eth_hdr_t))
        len = PACKET_SIZE - sizeof(eth_hdr_t);

    memcpy(data, payload, len);

    // Ethernet header: broadcast, our MAC, "IP" ethertype so Wireshark is happy
    memset(eth->dst, 0xFF, 6);     // broadcast
    memcpy(eth->src, mac_addr, 6); // our MAC
    eth->type = swap16(0x0800);     // pretend it's IPv4

    // Enforce minimum Ethernet frame size (without FCS)
    uint16_t total_len = sizeof(eth_hdr_t) + len;
    if (total_len < 60)
        total_len = 60;

    e1000e_send_packet(buffer, total_len);
}

void e1000e_send_raw_broadcast_test(void)
{
    uint8_t junk[32];
    for (int i = 0; i < (int)sizeof(junk); i++)
        junk[i] = (uint8_t)i;

    e1000e_send_raw_broadcast(junk, sizeof(junk));
}


static void e1000e_read_mac_and_program_rar0(void)
{
  uint32_t ral = e1000e_read(E1000E_REG_RAL);
  uint32_t rah = e1000e_read(E1000E_REG_RAH);

  mac_addr[0] = ral & 0xFF;
  mac_addr[1] = (ral >> 8) & 0xFF;
  mac_addr[2] = (ral >> 16) & 0xFF;
  mac_addr[3] = (ral >> 24) & 0xFF;
  mac_addr[4] = rah & 0xFF;
  mac_addr[5] = (rah >> 8) & 0xFF;

  /* Program RAR0 with MAC and set AV bit */
  uint32_t ral_val = (mac_addr[3] << 24) | (mac_addr[2] << 16) |
                     (mac_addr[1] << 8) | mac_addr[0];
  uint32_t rah_val = (mac_addr[5] << 8) | mac_addr[4] | (1u << 31);
  e1000e_write(E1000E_REG_RAL, ral_val);
  e1000e_write(E1000E_REG_RAH, rah_val);

  serial_write_str("e1000e: MAC: ");
  for (int i = 0; i < 6; i++)
  {
    serial_write_hex(mac_addr[i]);
    if (i < 5)
      serial_write_str(":");
  }
  serial_write_str("\r\n");
}

uint8_t* e1000e_get_mac_addr() {
    return mac_addr;
}

uint32_t e1000e_get_status_reg() {
    if (!mmio_base) return 0;
    return e1000e_read(E1000E_REG_STATUS);
}

void e1000e_setup_rings(void)
{
  uint64_t rx_desc_phys, tx_desc_phys;
  uint64_t rx_buf_phys, tx_buf_phys;

  rx_descs = dma_alloc_coherent(sizeof(e1000e_rx_desc_t) * RX_DESC_COUNT,
                                &rx_desc_phys);
  tx_descs = dma_alloc_coherent(sizeof(e1000e_tx_desc_t) * TX_DESC_COUNT,
                                &tx_desc_phys);

  rx_buffers = dma_alloc_coherent(PACKET_SIZE * RX_DESC_COUNT, &rx_buf_phys);
  tx_buffers = dma_alloc_coherent(PACKET_SIZE * TX_DESC_COUNT, &tx_buf_phys);

  memset(rx_descs, 0, sizeof(e1000e_rx_desc_t) * RX_DESC_COUNT);
  memset(tx_descs, 0, sizeof(e1000e_tx_desc_t) * TX_DESC_COUNT);

  for (int i = 0; i < RX_DESC_COUNT; i++)
  {
    rx_descs[i].addr = rx_buf_phys + (uint64_t)i * PACKET_SIZE;
    rx_descs[i].status = 0;
  }

  e1000e_write(E1000E_REG_RDBAL, (uint32_t)rx_desc_phys);
  e1000e_write(E1000E_REG_RDBAH, (uint32_t)(rx_desc_phys >> 32));
  e1000e_write(E1000E_REG_RDLEN, RX_DESC_COUNT * sizeof(e1000e_rx_desc_t));
  e1000e_write(E1000E_REG_RDH, 0);
  e1000e_write(E1000E_REG_RDT, RX_DESC_COUNT - 1);

  e1000e_write(E1000E_REG_TDBAL, (uint32_t)tx_desc_phys);
  e1000e_write(E1000E_REG_TDBAH, (uint32_t)(tx_desc_phys >> 32));
  e1000e_write(E1000E_REG_TDLEN, TX_DESC_COUNT * sizeof(e1000e_tx_desc_t));
  e1000e_write(E1000E_REG_TDH, 0);
  e1000e_write(E1000E_REG_TDT, 0);
}

void e1000e_init(uintptr_t base)
{
  mmio_base = base;

  /* 1) Reset first */
  uint32_t ctrl = e1000e_read(E1000E_REG_CTRL);
  e1000e_write(E1000E_REG_CTRL, ctrl | E1000E_CTRL_RST);
  for (volatile int i = 0; i < 100000; i++)
    ;

  /* 2) Now read MAC + program RAR0 after reset */
  e1000e_read_mac_and_program_rar0();

  /* 3) Setup rings */
  e1000e_setup_rings();

  /* 4) Enable link + autoneg */
  ctrl = e1000e_read(E1000E_REG_CTRL);
  ctrl |= E1000E_CTRL_SLU | E1000E_CTRL_ASDE;
  e1000e_write(E1000E_REG_CTRL, ctrl);

  /* 5) RX/TX control */
  e1000e_write(E1000E_REG_RCTL,
               E1000E_RCTL_EN | E1000E_RCTL_BAM | E1000E_RCTL_SECRC);
  e1000e_write(E1000E_REG_TCTL,
               E1000E_TCTL_EN | E1000E_TCTL_PSP | (0x0F << 4) | (0x40 << 12));
  e1000e_write(E1000E_REG_TIPG, 0x0060200A);

  /* 6) Wait for link */
  int timeout = 200000;
  while (!(e1000e_read(E1000E_REG_STATUS) & (1 << 1)) && timeout--)
  {
    for (volatile int i = 0; i < 1000; i++)
      ;
  }

  if (!timeout)
    serial_write_str("e1000e: Link-up timeout\r\n");
  else
    serial_write_str("e1000e: Link UP\r\n");

  uint32_t status = e1000e_read(E1000E_REG_STATUS);
  serial_write_str("e1000e: STATUS=");
  serial_write_hex(status);
  serial_write_str("\r\n");
}

static uint64_t g_tx_packets = 0;

void e1000e_send_packet(const void *data, uint16_t length)
{
  serial_write_str("e1000e: TX #");
  serial_write_dec(g_tx_packets);
  serial_write_str(" ptr=");
  serial_write_dec(tx_ptr);
  serial_write_str(" len=");
  serial_write_dec(length);
  serial_write_str("\r\n");

  /* Ensure packet buffer visible to device */
  dma_sync_for_device((void *)data, length);
  uint64_t phys = virt_to_phys((void *)data);

  tx_descs[tx_ptr].addr = phys;
  tx_descs[tx_ptr].length = length;
  tx_descs[tx_ptr].cmd = (1 << 0) | (1 << 1) | (1 << 3); // EOP | IFCS | RS
  tx_descs[tx_ptr].status = 0;

  /* Flush descriptor itself */
  dma_sync_for_device(&tx_descs[tx_ptr], sizeof(e1000e_tx_desc_t));

  uint32_t old = tx_ptr;
  tx_ptr = (tx_ptr + 1) % TX_DESC_COUNT;

  e1000e_write(E1000E_REG_TDT, tx_ptr);

  int t = 1000000;
  while (t--)
  {
    dma_sync_for_cpu(&tx_descs[old], sizeof(e1000e_tx_desc_t));
    if (tx_descs[old].status & 0x1)
      break;
    __asm__ volatile("" ::: "memory");
  }

  if (!(tx_descs[old].status & 0x1))
    serial_write_str("e1000e: TX timeout\r\n");
  else
  {
    serial_write_str("e1000e: TX done status=");
    serial_write_hex(tx_descs[old].status);
    serial_write_str("\r\n");
  }
  g_tx_packets++;
}

void e1000e_send_udp_packet(uint8_t *dst_ip, uint16_t dst_port,
                            const void *data, uint16_t len)
{
  uint8_t *buffer = tx_buffers + tx_ptr * PACKET_SIZE;

  memset(buffer, 0, PACKET_SIZE);

  eth_hdr_t *eth = (eth_hdr_t *)buffer;
  ip_hdr_t *ip = (ip_hdr_t *)(buffer + sizeof(eth_hdr_t));
  udp_hdr_t *udp = (udp_hdr_t *)(buffer + sizeof(eth_hdr_t) + sizeof(ip_hdr_t));
  uint8_t *payload =
      buffer + sizeof(eth_hdr_t) + sizeof(ip_hdr_t) + sizeof(udp_hdr_t);

  memcpy(payload, data, len);

  udp->src = swap16(1234);
  udp->dst = swap16(dst_port);
  udp->len = swap16(sizeof(udp_hdr_t) + len);
  udp->csum = 0;

  uint32_t dip =
      (dst_ip[0] << 24) | (dst_ip[1] << 16) | (dst_ip[2] << 8) | dst_ip[3];
  uint32_t sip = (172 << 24) | (16 << 16) | (6 << 8) | 222;

  ip->ver_ihl = 0x45;
  ip->tos = 0;
  ip->len = swap16(sizeof(ip_hdr_t) + sizeof(udp_hdr_t) + len);
  ip->id = swap16(1);
  ip->frag = 0;
  ip->ttl = 64;
  ip->proto = IP_PROTO_UDP;
  ip->src = swap32(sip);
  ip->dst = swap32(dip);
  ip->csum = checksum(ip, sizeof(ip_hdr_t));

  memset(eth->dst, 0xFF, 6);
  memcpy(eth->src, mac_addr, 6);
  eth->type = swap16(ETH_P_IP);

  uint16_t total =
      sizeof(eth_hdr_t) + sizeof(ip_hdr_t) + sizeof(udp_hdr_t) + len;

  e1000e_send_packet(buffer, total);
}

void e1000e_send_tcp_packet(uint8_t *dst_ip, uint16_t dst_port, const void *data, uint16_t len, uint8_t flags)
{
  uint8_t *buffer = tx_buffers + tx_ptr * PACKET_SIZE;
  memset(buffer, 0, PACKET_SIZE);

  eth_hdr_t *eth = (eth_hdr_t *)buffer;
  ip_hdr_t *ip = (ip_hdr_t *)(buffer + sizeof(eth_hdr_t));
  tcp_hdr_t *tcp = (tcp_hdr_t *)(buffer + sizeof(eth_hdr_t) + sizeof(ip_hdr_t));
  uint8_t *payload = buffer + sizeof(eth_hdr_t) + sizeof(ip_hdr_t) + sizeof(tcp_hdr_t);

  memcpy(payload, data, len);

  tcp->src = swap16(4321);
  tcp->dst = swap16(dst_port);
  tcp->seq = swap32(100);
  tcp->ack = 0;
  tcp->res_off = (sizeof(tcp_hdr_t) / 4) << 4;
  tcp->flags = flags;
  tcp->window = swap16(8192);
  tcp->csum = 0;
  tcp->urg = 0;

  uint32_t dip =
      (dst_ip[0] << 24) | (dst_ip[1] << 16) | (dst_ip[2] << 8) | dst_ip[3];
  uint32_t sip = (172 << 24) | (16 << 16) | (6 << 8) | 222;

  ip->ver_ihl = 0x45;
  ip->tos = 0;
  ip->len = swap16(sizeof(ip_hdr_t) + sizeof(tcp_hdr_t) + len);
  ip->id = swap16(2);
  ip->frag = 0;
  ip->ttl = 64;
  ip->proto = IP_PROTO_TCP;
  ip->src = swap32(sip);
  ip->dst = swap32(dip);
  ip->csum = checksum(ip, sizeof(ip_hdr_t));

  // TCP Checksum
  struct {
      uint32_t src;
      uint32_t dst;
      uint8_t zero;
      uint8_t proto;
      uint16_t len;
  } __attribute__((packed)) pseudo;

  pseudo.src = ip->src; // Already network byte order in ip header? Wait, swap32 was used.
  // swap32 converts to Big Endian (Network Order) if host is Little Endian.
  // ip->src is uint32_t.
  pseudo.src = ip->src; 
  pseudo.dst = ip->dst;
  pseudo.zero = 0;
  pseudo.proto = IP_PROTO_TCP;
  pseudo.len = swap16(sizeof(tcp_hdr_t) + len);

  uint32_t sum = 0;
  uint16_t* p = (uint16_t*)&pseudo;
  for(int i=0; i<6; i++) sum += p[i];

  p = (uint16_t*)tcp;
  int tcp_len = sizeof(tcp_hdr_t) + len;
  while(tcp_len > 1) {
      sum += *p++;
      tcp_len -= 2;
  }
  if(tcp_len) sum += *(uint8_t*)p;

  while(sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
  tcp->csum = ~sum;

  memset(eth->dst, 0xFF, 6);
  memcpy(eth->src, mac_addr, 6);
  eth->type = swap16(ETH_P_IP);

  uint16_t total = sizeof(eth_hdr_t) + sizeof(ip_hdr_t) + sizeof(tcp_hdr_t) + len;
  e1000e_send_packet(buffer, total);
}