#ifndef XHCI_H
#define XHCI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- Register Structures ---

typedef struct {
  volatile uint8_t cap_length;
  volatile uint8_t reserved;
  volatile uint16_t hci_version;
  volatile uint32_t hcs_params1;
  volatile uint32_t hcs_params2;
  volatile uint32_t hcs_params3;
  volatile uint32_t hcc_params1;
  volatile uint32_t db_off;
  volatile uint32_t rts_off;
  volatile uint32_t hcc_params2;
} __attribute__((packed)) xhci_cap_regs_t;

typedef struct {
  volatile uint32_t usb_cmd;
  volatile uint32_t usb_sts;
  volatile uint32_t page_size;
  volatile uint32_t reserved1[2];
  volatile uint32_t dn_ctrl;
  volatile uint64_t crcr;
  volatile uint32_t reserved2[4];
  volatile uint64_t dcbaap;
  volatile uint32_t config;
} __attribute__((packed)) xhci_op_regs_t;

typedef struct {
  volatile uint32_t iman;
  volatile uint32_t imod;
  volatile uint32_t erstsz;
  volatile uint32_t reserved;
  volatile uint64_t erstba;
  volatile uint64_t erdp;
} __attribute__((packed)) xhci_interrupter_regs_t;

typedef struct {
  volatile uint32_t mfindex;
  volatile uint8_t reserved[0x1C];
  xhci_interrupter_regs_t ir[1024];
} __attribute__((packed)) xhci_runtime_regs_t;

typedef struct {
  volatile uint32_t target;
} __attribute__((packed)) xhci_doorbell_regs_t;

// --- Data Structures ---

typedef struct {
  uint64_t parameter;
  uint32_t status;
  uint32_t control;
} __attribute__((packed)) xhci_trb_t;

typedef struct {
  uint64_t base;
  uint32_t size;
  uint32_t reserved;
} __attribute__((packed)) xhci_erst_entry_t;

// Contexts
// Slot Context Fields
#define XHCI_CTX_SLOT_ROUTE_STRING(x) ((x) & 0xFFFFF)
#define XHCI_CTX_SLOT_SPEED(x) (((x) & 0xF) << 20)
#define XHCI_CTX_SLOT_MTT (1 << 25)
#define XHCI_CTX_SLOT_HUB (1 << 26)
#define XHCI_CTX_SLOT_CONTEXT_ENTRIES(x) (((x) & 0x1F) << 27)

#define XHCI_CTX_SLOT_ROOT_HUB_PORT(x) (((x) & 0xFF) << 16)
#define XHCI_CTX_SLOT_NUM_PORTS(x) (((x) & 0xFF) << 24)

#define XHCI_CTX_SLOT_DEVICE_ADDR(x) ((x) & 0xFF)
#define XHCI_CTX_SLOT_STATE(x) (((x) & 0x1F) << 27)

// Endpoint Context Fields
#define XHCI_CTX_EP_STATE(x) ((x) & 0x7)
#define XHCI_CTX_EP_INTERVAL(x) (((x) & 0xFF) << 16)
#define XHCI_CTX_EP_MAX_P_STREAMS(x) (((x) & 0x1F) << 10)
#define XHCI_CTX_EP_MULT(x) (((x) & 0x3) << 8)
#define XHCI_CTX_EP_ERROR_COUNT(x) (((x) & 0x3) << 1)

#define XHCI_CTX_EP_TYPE(x) (((x) & 0x7) << 3)
#define XHCI_CTX_EP_MAX_BURST_SIZE(x) (((x) & 0xFF) << 8)
#define XHCI_CTX_EP_MAX_PACKET_SIZE(x) (((x) & 0xFFFF) << 16)

#define XHCI_CTX_EP_DCS (1 << 0)
#define XHCI_CTX_EP_TR_DR_PTR(x) ((x) & 0xFFFFFFFFFFFFFFF0)

#define XHCI_CTX_EP_AVG_TRB_LEN(x) ((x) & 0xFFFF)

typedef struct {
  uint32_t ctx_info[8]; // 32 bytes
  uint32_t reserved[8]; // Padding for 64-byte context size compatibility
} __attribute__((packed)) xhci_slot_ctx_t;

typedef struct {
  uint32_t ctx_info[8]; // 32 bytes
  uint32_t reserved[8]; // Padding for 64-byte context size compatibility
} __attribute__((packed)) xhci_ep_ctx_t;

// Input Control Context
typedef struct {
  uint32_t drop_context_flags;
  uint32_t add_context_flags;
  uint32_t reserved1[5];
  uint32_t config_value; // 7
} __attribute__((packed)) xhci_input_control_ctx_t;

// Input Context Wrapper (Variable size, but we can define a max structure)
// Note: The Input Context is: Input Control Context + (Context Entries + 1) *
// Context Size We will manage offsets manually based on context_size

typedef struct {
  volatile uint32_t portsc;
  volatile uint32_t portpmsc;
  volatile uint32_t portli;
  volatile uint32_t porthlpmc;
} __attribute__((packed)) xhci_port_regs_t;

// --- Software State Tracking ---

typedef struct {
  uint64_t *dev_ctx;    // Pointer to Device Context (in DCBAA)
  xhci_trb_t *ep0_ring; // Endpoint 0 Transfer Ring
  uint32_t ep0_enqueue_ptr;
  uint32_t ep0_cycle_state;

  // Endpoint Rings
  uint64_t ring1_ptr; // Interrupt IN (HID) or Bulk OUT (MSC)
  uint32_t ring1_enqueue;
  uint32_t ring1_cycle;

  uint64_t ring2_ptr; // Bulk IN (MSC)
  uint32_t ring2_enqueue;
  uint32_t ring2_cycle;

  // Device Metadata
  uint32_t device_type; // 0=Kbd, 1=Mouse, 2=Tablet, 3=Mass Storage
  uint32_t dci_out;     // Output DCI (for MSC)
  uint32_t dci_in;      // Input DCI (for MSC)
  uint32_t mps_out;
  uint32_t mps_in;

  uint64_t last_report; // Last HID report
  uint32_t last_abs_x;
  uint32_t last_abs_y;

  // MSC specific
  uint32_t msc_max_lba;
  uint32_t msc_block_size;
} xhci_slot_data_t;

// --- Main Driver Structure ---

typedef struct {
  uintptr_t base_addr;
  xhci_cap_regs_t *cap_regs;
  xhci_op_regs_t *op_regs;
  xhci_runtime_regs_t *run_regs;
  xhci_doorbell_regs_t *db_regs;
  xhci_port_regs_t *port_regs;

  uint8_t max_slots;
  uint32_t max_scratchpad_bufs;
  uint64_t *dcbaa; // Virtual pointer to DCBAA

  xhci_slot_data_t
      *slots; // Array of software state for each slot (index 1..MaxSlots)

  // Command Ring
  xhci_trb_t *cmd_ring;
  uint64_t cmd_ring_phys;
  uint32_t cmd_ring_cycle_state;
  uint32_t cmd_ring_enqueue_ptr;

  // Event Ring (Interrupter 0)
  xhci_erst_entry_t *erst; // Segment Table
  xhci_trb_t *event_ring;  // The ring itself
  uint64_t event_ring_phys;
  uint32_t event_ring_cycle_state;
  uint32_t event_ring_dequeue_ptr;

  uint32_t context_size; // 32 or 64 bytes (CSZ bit in HCCPARAMS1)

} xhci_controller_t;

// Defines
#define XHCI_CMD_RUN (1 << 0)
#define XHCI_CMD_RESET (1 << 1)

#define XHCI_STS_HCH (1 << 0)

#define XHCI_TRB_TYPE_LINK 6
#define XHCI_TRB_TYPE_ENABLE_SLOT 9
#define XHCI_TRB_TYPE_ADDRESS_DEVICE 11
#define XHCI_TRB_TYPE_CONFIGURE_EP 12
#define XHCI_TRB_TYPE_SETUP_STAGE 2
#define XHCI_TRB_TYPE_DATA_STAGE 3
#define XHCI_TRB_TYPE_STATUS_STAGE 4

// USB Setup Packet (Standard)
typedef struct {
  uint8_t request_type;
  uint8_t request;
  uint16_t value;
  uint16_t index;
  uint16_t length;
} __attribute__((packed)) usb_setup_packet_t;

int xhci_init(uintptr_t base_addr, uint8_t bus, uint8_t dev, uint8_t func);
int xhci_address_device(uint8_t slot_id, uint8_t root_hub_port_num,
                        uint8_t port_speed);

// Mass Storage Support
int xhci_msc_read_sectors(uint8_t slot_id, uint32_t lba, uint16_t count,
                          void *buf);
int xhci_msc_write_sectors(uint8_t slot_id, uint32_t lba, uint16_t count,
                           const void *buf);
int xhci_find_msc_slot();

#ifdef __cplusplus
}
#endif

#endif