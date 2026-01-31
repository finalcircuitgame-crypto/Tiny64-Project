#ifndef E1000_H
#define E1000_H

#include <stdint.h>

// E1000 Registers
#define E1000_REG_CTRL      0x0000
#define E1000_REG_STATUS    0x0008
#define E1000_REG_EEPROM    0x0014
#define E1000_REG_ICR       0x00C0
#define E1000_REG_IMS       0x00D0
#define E1000_REG_IMC       0x00D8
#define E1000_REG_RCTL      0x0100
#define E1000_REG_TCTL      0x0400
#define E1000_REG_TIPG      0x0410
#define E1000_REG_RDBAL     0x2800
#define E1000_REG_RDBAH     0x2804
#define E1000_REG_RDLEN     0x2808
#define E1000_REG_RDH       0x2810
#define E1000_REG_RDT       0x2818
#define E1000_REG_TDBAL     0x3800
#define E1000_REG_TDBAH     0x3804
#define E1000_REG_TDLEN     0x3808
#define E1000_REG_TDH       0x3810
#define E1000_REG_TDT       0x3818
#define E1000_REG_RAL       0x5400
#define E1000_REG_RAH       0x5404

// Control Register Bits
#define E1000_CTRL_SLU      (1 << 6)
#define E1000_CTRL_ASDE     (1 << 5)
#define E1000_CTRL_RST      (1 << 26)

// Receive Control Register Bits
#define E1000_RCTL_EN       (1 << 1)
#define E1000_RCTL_SBP      (1 << 2)
#define E1000_RCTL_UPE      (1 << 3)
#define E1000_RCTL_MPE      (1 << 4)
#define E1000_RCTL_LBM_NONE (0 << 6)
#define E1000_RCTL_RDMTS_HALF (0 << 8)
#define E1000_RCTL_BAM      (1 << 15)
#define E1000_RCTL_BSIZE_2048 (0 << 16)
#define E1000_RCTL_SECRC    (1 << 26)

// Transmit Control Register Bits
#define E1000_TCTL_EN       (1 << 1)
#define E1000_TCTL_PSP      (1 << 3)
#define E1000_TCTL_CT       (0xF << 4)   // Collision Threshold
#define E1000_TCTL_COLD     (0x3F << 12) // Collision Distance

typedef struct {
    uint64_t addr;
    uint16_t length;
    uint8_t  errors;
    uint8_t  status;
} __attribute__((packed)) e1000_rx_desc_t;

typedef struct {
    uint64_t addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} __attribute__((packed)) e1000_tx_desc_t;

void e1000_init(uintptr_t base_addr);
void e1000_send_packet(const void* data, uint16_t length);
void e1000_enable_busmaster(uint8_t bus, uint8_t dev, uint8_t func);
void e1000_send_raw_broadcast_test(void);
void e1000_net_log_callback(char c);
void e1000_send_udp_packet(uint8_t* dst_ip, uint16_t dst_port, const void* data, uint16_t len);
void e1000_send_tcp_packet(uint8_t* dst_ip, uint16_t dst_port, const void* data, uint16_t len, uint8_t flags);
uint32_t e1000_get_status_reg();

#endif
