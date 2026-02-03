#ifndef XHCI_H
#define XHCI_H

#include <stdint.h>

// Capability Registers Offsets
#define XHCI_CAPLENGTH    0x00
#define XHCI_HCIVERSION   0x02
#define XHCI_HCSPARAMS1   0x04
#define XHCI_HCSPARAMS2   0x08
#define XHCI_HCSPARAMS3   0x0C
#define XHCI_HCCPARAMS1   0x10
#define XHCI_DBOFF        0x14
#define XHCI_RTSOFF       0x18
#define XHCI_HCCPARAMS2   0x1C

// Operational Registers Offsets
#define XHCI_USBCMD       0x00
#define XHCI_USBSTS       0x04
#define XHCI_PAGESIZE     0x08
#define XHCI_DNCTRL       0x14
#define XHCI_CRCR         0x18
#define XHCI_DCBAAP       0x30
#define XHCI_CONFIG       0x38
#define XHCI_PORTSC_BASE  0x400

// PORTSC Bits
#define PORTSC_CCS        (1 << 0)  // Current Connect Status
#define PORTSC_PED        (1 << 1)  // Port Enabled/Disabled
#define PORTSC_PR         (1 << 4)  // Port Reset
#define PORTSC_PLS_MASK   (0xF << 5) // Port Link State
#define PORTSC_PP         (1 << 9)  // Port Power
#define PORTSC_SPEED_MASK (0xF << 10)
#define PORTSC_CSC        (1 << 17) // Connect Status Change
#define PORTSC_PRC        (1 << 21) // Port Reset Change

// USBCMD Bits
#define USBCMD_RUN        (1 << 0)
#define USBCMD_RESET      (1 << 1)
#define USBCMD_INTE       (1 << 2)
#define USBCMD_HSEE       (1 << 3)

// USBSTS Bits
#define USBSTS_HCH        (1 << 0) // HC Halted
#define USBSTS_HSE        (1 << 2) // Host System Error
#define USBSTS_EINT       (1 << 3) // Event Interrupt
#define USBSTS_PCD        (1 << 4) // Port Change Detect
#define USBSTS_CNR        (1 << 11) // Controller Not Ready

// HCSPARAMS1 Bits
#define XHCI_MAX_SLOTS(p) ((p) & 0xFF)

// TRB Types
#define TRB_NORMAL        1
#define TRB_SETUP_STAGE   2
#define TRB_DATA_STAGE    3
#define TRB_STATUS_STAGE  4
#define TRB_LINK          6
#define TRB_COMMAND_COMPL 33
#define TRB_TRANSFER_EV   32
#define TRB_PORT_STATUS_EV 34

// Commands
#define TRB_ENABLE_SLOT   9
#define TRB_ADDRESS_DEVICE 11
#define TRB_CONFIGURE_ENDPOINT 12

// Transfer Request Block (TRB) structure
struct xhci_trb {
    uint64_t param;
    uint32_t status;
    uint32_t control;
} __attribute__((packed));

#define TRB_CYCLE (1 << 0)
#define TRB_TYPE_GET(c) (((c) >> 10) & 0x3F)
#define TRB_TYPE(t) ((t) << 10)
#define TRB_IDT (1 << 6)
#define TRB_TRT_OUT (2 << 16)
#define TRB_TRT_IN (3 << 16)
#define TRB_DIR_IN (1 << 16)

// Context Structures (xHCI 6.2)
struct xhci_slot_ctx {
    uint32_t info;
    uint32_t info2;
    uint32_t tt_info;
    uint32_t state;
    uint32_t reserved[4];
} __attribute__((packed));

struct xhci_ep_ctx {
    uint32_t info;
    uint32_t info2;
    uint64_t tr_ptr;
    uint32_t average_trb_len;
    uint32_t reserved[3];
} __attribute__((packed));

struct xhci_device_ctx {
    struct xhci_slot_ctx slot;
    struct xhci_ep_ctx ep[31];
} __attribute__((packed));

struct xhci_input_ctx {
    uint32_t drop_flags;
    uint32_t add_flags;
    uint32_t reserved[6];
    struct xhci_device_ctx dev;
} __attribute__((packed));

// Software Ring Structure
struct xhci_ring {
    struct xhci_trb* trbs;
    uint32_t size;
    uint32_t enqueue_idx;
    uint8_t cycle_bit;
};

// HCSPARAMS1 Bits
#define XHCI_MAX_SLOTS(p) ((p) & 0xFF)
#define XHCI_MAX_PORTS(p) (((p) >> 24) & 0xFF)
#define XHCI_MAX_INTRS(p) (((p) >> 8) & 0x7FF)

// HCSPARAMS2 Bits
#define XHCI_MAX_SCRATCHPADS(p) (((p) >> 21) & 0x1F)

// Interrupter Registers (xHCI 5.5.2)
#define XHCI_IMAN         0x00
#define XHCI_IMOD         0x04
#define XHCI_ERSTSZ       0x08
#define XHCI_ERSTBA       0x10
#define XHCI_ERDP         0x18

struct xhci_erst_entry {
    uint64_t ba;
    uint32_t size;
    uint32_t reserved;
} __attribute__((packed));

#define TRB_IOC (1 << 5)

struct usb_device_descriptor {
    uint8_t len;
    uint8_t type;
    uint16_t usb_version;
    uint8_t device_class;
    uint8_t device_subclass;
    uint8_t device_protocol;
    uint8_t max_packet_size;
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t device_version;
    uint8_t manufacturer_idx;
    uint8_t product_idx;
    uint8_t serial_idx;
    uint8_t num_configs;
} __attribute__((packed));

struct usb_config_descriptor {
    uint8_t len;
    uint8_t type;
    uint16_t total_len;
    uint8_t num_interfaces;
    uint8_t config_val;
    uint8_t config_idx;
    uint8_t attributes;
    uint8_t max_power;
} __attribute__((packed));

struct usb_interface_descriptor {
    uint8_t len;
    uint8_t type;
    uint8_t interface_num;
    uint8_t alternate_setting;
    uint8_t num_endpoints;
    uint8_t interface_class;
    uint8_t interface_subclass;
    uint8_t interface_protocol;
    uint8_t interface_idx;
} __attribute__((packed));

struct usb_endpoint_descriptor {
    uint8_t len;
    uint8_t type;
    uint8_t endpoint_address;
    uint8_t attributes;
    uint16_t max_packet_size;
    uint8_t interval;
} __attribute__((packed));

void xhci_init(uint8_t bus, uint8_t slot, uint8_t func);
void xhci_poll_keyboard();

#endif
