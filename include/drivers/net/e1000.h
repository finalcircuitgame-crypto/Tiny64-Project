#ifndef E1000_H
#define E1000_H

#include <stdint.h>

#define E1000_VENDOR_ID 0x8086
#define E1000_DEVICE_ID_82540EM 0x100E
#define E1000_DEVICE_ID_82571EB 0x105E
#define E1000_DEVICE_ID_82574L  0x10D3
#define E1000_DEVICE_ID_I217_V  0x153B
#define E1000_DEVICE_ID_I218_V  0x15A1
#define E1000_DEVICE_ID_I219_V  0x15B8
#define E1000_DEVICE_ID_I219_LM 0x15B7

// Register offsets
#define REG_CTRL          0x0000
#define REG_STATUS        0x0008
#define REG_EECD          0x0010
#define REG_EERD          0x0014
#define REG_ICR           0x00C0
#define REG_IMS           0x00D0
#define REG_IMC           0x00D8
#define REG_RCTL          0x0100
#define REG_TCTL          0x0400
#define REG_TIPG          0x0410
#define REG_RDBAL         0x2800
#define REG_RDBAH         0x2804
#define REG_RDLEN         0x2808
#define REG_RDH           0x2810
#define REG_RDT           0x2818
#define REG_TDBAL         0x3800
#define REG_TDBAH         0x3804
#define REG_TDLEN         0x3808
#define REG_TDH           0x3810
#define REG_TDT           0x3818
#define REG_MTA           0x5200
#define REG_RAL           0x5400
#define REG_RAH           0x5404

// Control Register Bits
#define CTRL_SLU          (1 << 6)
#define CTRL_ASDE         (1 << 5)

// Receive Control Register Bits
#define RCTL_EN           (1 << 1)
#define RCTL_SBP          (1 << 2)
#define RCTL_UPE          (1 << 3)
#define RCTL_MPE          (1 << 4)
#define RCTL_LPE          (1 << 5)
#define RCTL_BAM          (1 << 15)
#define RCTL_BSIZE_2048   (0 << 16)
#define RCTL_SECRC        (1 << 26)

// Transmit Control Register Bits
#define TCTL_EN           (1 << 1)
#define TCTL_PSP          (1 << 3)

// Descriptors
struct e1000_rx_desc {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} __attribute__((packed));

struct e1000_tx_desc {
    uint64_t addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} __attribute__((packed));

#define E1000_NUM_RX_DESC 128
#define E1000_NUM_TX_DESC 128

void e1000_init(uint8_t bus, uint8_t slot, uint8_t func);
void e1000_get_mac(uint8_t* out_mac);
void e1000_send_packet(const void* data, uint16_t len);
int e1000_receive_packet(void* buffer, uint16_t max_len);

#endif
