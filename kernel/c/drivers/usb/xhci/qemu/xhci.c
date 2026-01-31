#include "../../../../lib/math.h"
#include "../../../../lib/string.h"
#include "../../../../memory/dma.h"
#include "../../../../memory/heap.h"
#include "../../../hid/input.h" // For pushing keys
#include "../../../pci/pci.h"
#include "../../../platform/serial.h"
#include "qemu_xhci.h"

static inline void clflush(volatile void *p) {
  __asm__ volatile("clflush (%0)" ::"r"(p) : "memory");
}

static qemu_xhci_controller_t qemu_xhc;

// Keyboard lookup table (Scan Code Set 1/2 mapping simplified for Boot
// Protocol) This is a very basic US-QWERTY map for HID usage IDs
static char hid_keymap_lower[] = {
    0,   0,    0,   0,   'a',  'b', 'c', 'd',  'e', 'f', 'g', 'h',
    'i', 'j',  'k', 'l', 'm',  'n', 'o', 'p',  'q', 'r', 's', 't',
    'u', 'v',  'w', 'x', 'y',  'z', '1', '2',  '3', '4', '5', '6',
    '7', '8',  '9', '0', '\n', 27,  8,   '\t', ' ', '-', '=', '[',
    ']', '\\', 0,   ';', '\'', '`', ',', '.',  '/'};
static char hid_keymap_upper[] = {
    0,   0,   0,   0,   'A', 'B', 'C', 'D', 'E', 'F', 'G',  'H', 'I', 'J',  'K',
    'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',  'W', 'X', 'Y',  'Z',
    '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '\n', 27,  8,   '\t', ' ',
    '_', '+', '{', '}', '|', 0,   ':', '"', '~', '<', '>',  '?'};

// Forward Declarations
static void *xhci_get_context(void *base, int index);
static void xhci_ring_doorbell(uint8_t slot, uint8_t target);
static void xhci_send_command(uint32_t type, uint64_t param,
                              uint32_t control_flags);
static xhci_trb_t *xhci_wait_for_event(uint32_t target_type);
int qemu_xhci_address_device(uint8_t slot_id, uint8_t root_hub_port_num,
                             uint8_t port_speed);
static void xhci_get_descriptor(uint8_t slot_id);
static void xhci_scan_port(int i);
static void xhci_check_ports();
static void xhci_configure_endpoint(uint8_t slot_id, int dci,
                                    int max_packet_size, int interval);
static void xhci_configure_device(uint8_t slot_id);

// --- Helpers ---
static void xhci_reset_controller() {
  qemu_xhc.op_regs->usb_cmd &= ~XHCI_CMD_RUN;
  int timeout = 1000;
  while (!(qemu_xhc.op_regs->usb_sts & XHCI_STS_HCH) && timeout-- > 0)
    for (volatile int i = 0; i < 10000; i++)
      ;

  qemu_xhc.op_regs->usb_cmd |= XHCI_CMD_RESET;
  timeout = 1000;
  while ((qemu_xhc.op_regs->usb_cmd & XHCI_CMD_RESET) && timeout-- > 0)
    for (volatile int i = 0; i < 10000; i++)
      ;

  timeout = 1000;
  while ((qemu_xhc.op_regs->usb_sts & (1 << 11)) && timeout-- > 0)
    for (volatile int i = 0; i < 10000; i++)
      ;
}

static void xhci_alloc_dcbaa() {
  uint32_t hcsparams1 = qemu_xhc.cap_regs->hcs_params1;
  qemu_xhc.max_slots = hcsparams1 & 0xFF;

  serial_write_str("xHCI: Max Slots: ");
  serial_write_dec(qemu_xhc.max_slots);
  serial_write_str("\r\n");

  uint32_t hcsparams2 = qemu_xhc.cap_regs->hcs_params2;
  uint32_t max_scratchpad_bufs =
      ((hcsparams2 >> 21) & 0x1F) | (((hcsparams2 >> 27) & 0x1F) << 5);

  size_t size = (qemu_xhc.max_slots + 1) * sizeof(uint64_t);
  uint64_t dcbaa_phys = 0;
  qemu_xhc.dcbaa = (uint64_t *)dma_alloc_coherent(size, &dcbaa_phys);
  if (!qemu_xhc.dcbaa)
    return;

  if (max_scratchpad_bufs > 0) {
    uint64_t scratch_arr_phys = 0;
    uint64_t *scratch_arr = (uint64_t *)dma_alloc_coherent(
        max_scratchpad_bufs * sizeof(uint64_t), &scratch_arr_phys);
    for (uint32_t i = 0; i < max_scratchpad_bufs; i++) {
      uint64_t buf_phys = 0;
      void *buf = dma_alloc_coherent(qemu_xhc.op_regs->page_size, &buf_phys);
      scratch_arr[i] = buf_phys;
      (void)buf;
    }
    qemu_xhc.dcbaa[0] = scratch_arr_phys;
    dma_sync_for_device(scratch_arr, max_scratchpad_bufs * sizeof(uint64_t));
  }

  dma_sync_for_device(qemu_xhc.dcbaa, size);
  qemu_xhc.op_regs->dcbaap = dcbaa_phys;
  qemu_xhc.slots = (xhci_slot_data_t *)kmalloc((qemu_xhc.max_slots + 1) *
                                               sizeof(xhci_slot_data_t));
  memset(qemu_xhc.slots, 0,
         (qemu_xhc.max_slots + 1) * sizeof(xhci_slot_data_t));
}

static void xhci_alloc_cmd_ring() {
  uint64_t phys = 0;
  qemu_xhc.cmd_ring = (xhci_trb_t *)dma_alloc_coherent(1024, &phys);
  qemu_xhc.cmd_ring_phys = phys;
  qemu_xhc.cmd_ring_cycle_state = 1;
  qemu_xhc.cmd_ring_enqueue_ptr = 0;
  qemu_xhc.op_regs->crcr = qemu_xhc.cmd_ring_phys | 1;
}

static void xhci_alloc_event_ring() {
  uint64_t erst_phys = 0;
  qemu_xhc.erst = (xhci_erst_entry_t *)dma_alloc_coherent(
      sizeof(xhci_erst_entry_t), &erst_phys);
  uint64_t er_phys = 0;
  qemu_xhc.event_ring = (xhci_trb_t *)dma_alloc_coherent(1024, &er_phys);
  qemu_xhc.event_ring_phys = er_phys;
  qemu_xhc.erst[0].base = qemu_xhc.event_ring_phys;
  qemu_xhc.erst[0].size = 64;
  qemu_xhc.erst[0].reserved = 0;
  dma_sync_for_device(qemu_xhc.erst, sizeof(xhci_erst_entry_t));
  qemu_xhc.run_regs->ir[0].erstsz = 1;
  qemu_xhc.run_regs->ir[0].erdp = qemu_xhc.event_ring_phys;
  qemu_xhc.run_regs->ir[0].erstba = erst_phys;
  qemu_xhc.run_regs->ir[0].iman |= 2;
  qemu_xhc.run_regs->ir[0].imod = 4000;
  qemu_xhc.event_ring_cycle_state = 1;
  qemu_xhc.event_ring_dequeue_ptr = 0;
}

static void xhci_ring_doorbell(uint8_t slot, uint8_t target) {
  __asm__ volatile("mfence" ::: "memory");
  qemu_xhc.db_regs[slot].target = target;
}

static void xhci_send_command(uint32_t type, uint64_t param,
                              uint32_t control_flags) {
  xhci_trb_t *trb = &qemu_xhc.cmd_ring[qemu_xhc.cmd_ring_enqueue_ptr];
  trb->parameter = param;
  trb->status = 0;
  trb->control = (type << 10) | control_flags;
  if (qemu_xhc.cmd_ring_cycle_state)
    trb->control |= 1;
  else
    trb->control &= ~1;
  clflush(trb);
  qemu_xhc.cmd_ring_enqueue_ptr++;
  if (qemu_xhc.cmd_ring_enqueue_ptr >= 63) {
    xhci_trb_t *link = &qemu_xhc.cmd_ring[63];
    link->parameter = qemu_xhc.cmd_ring_phys;
    link->control =
        (6 << 10) | (1 << 1) | (qemu_xhc.cmd_ring_cycle_state ? 1 : 0);
    clflush(link);
    qemu_xhc.cmd_ring_enqueue_ptr = 0;
    qemu_xhc.cmd_ring_cycle_state = !qemu_xhc.cmd_ring_cycle_state;
  }
  xhci_ring_doorbell(0, 0);
}

static xhci_trb_t *xhci_poll_event() {
  xhci_trb_t *trb = &qemu_xhc.event_ring[qemu_xhc.event_ring_dequeue_ptr];
  clflush(trb);
  if ((trb->control & 1) == qemu_xhc.event_ring_cycle_state) {
    qemu_xhc.event_ring_dequeue_ptr++;
    if (qemu_xhc.event_ring_dequeue_ptr >= 64) {
      qemu_xhc.event_ring_dequeue_ptr = 0;
      qemu_xhc.event_ring_cycle_state = !qemu_xhc.event_ring_cycle_state;
    }
    qemu_xhc.run_regs->ir[0].erdp =
        (qemu_xhc.event_ring_phys + (qemu_xhc.event_ring_dequeue_ptr * 16)) | 8;
    return trb;
  }
  return 0;
}

static xhci_trb_t *xhci_wait_for_event(uint32_t target_type) {
  for (volatile int k = 0; k < 500000000; k++) {
    xhci_trb_t *evt = xhci_poll_event();
    if (evt) {
      uint32_t type = (evt->control >> 10) & 0x3F;
      if (type == target_type)
        return evt;
      if (type == 34) {
        uint32_t port = (evt->parameter >> 24) & 0xFF;
        if (port > 0)
          qemu_xhc.port_regs[port - 1].portsc =
              qemu_xhc.port_regs[port - 1].portsc;
      }
    }
  }
  return 0;
}

static void *xhci_get_context(void *base, int index) {
  return (void *)((uintptr_t)base + (index * qemu_xhc.context_size));
}

void xhci_ring_ep_doorbell(uint8_t slot_id, uint8_t doorbell_target) {
  qemu_xhc.db_regs[slot_id].target = doorbell_target;
}

void xhci_transfer_enqueue(uint8_t slot_id, uint8_t ep_index, uint64_t param,
                           uint32_t status, uint32_t control) {
  xhci_slot_data_t *slot = &qemu_xhc.slots[slot_id];
  xhci_trb_t *ring = 0;
  uint32_t *enqueue = 0, *cycle = 0;
  if (ep_index == 0) {
    ring = slot->ep0_ring;
    enqueue = &slot->ep0_enqueue_ptr;
    cycle = &slot->ep0_cycle_state;
  } else if (ep_index == 1) {
    ring = (xhci_trb_t *)slot->ring1_ptr;
    enqueue = &slot->ring1_enqueue;
    cycle = &slot->ring1_cycle;
  } else if (ep_index == 2) {
    ring = (xhci_trb_t *)slot->ring2_ptr;
    enqueue = &slot->ring2_enqueue;
    cycle = &slot->ring2_cycle;
  }
  if (!ring)
    return;
  xhci_trb_t *trb = &ring[*enqueue];
  trb->parameter = param;
  trb->status = status;
  trb->control = control | (*cycle ? 1 : 0);
  clflush(trb);
  (*enqueue)++;
  if (*enqueue >= 63) {
    xhci_trb_t *link = &ring[63];
    link->parameter = (uint64_t)ring;
    link->control = (6 << 10) | (1 << 1) | (*cycle ? 1 : 0);
    clflush(link);
    *enqueue = 0;
    *cycle = !(*cycle);
  }
}

void xhci_ep0_enqueue(uint8_t slot_id, uint64_t param, uint32_t status,
                      uint32_t control) {
  xhci_transfer_enqueue(slot_id, 0, param, status, control);
}

static int xhci_send_control_transfer(uint8_t slot_id,
                                      usb_setup_packet_t *setup, void *data,
                                      uint32_t length) {
  uint32_t trt = (length > 0) ? (setup->request_type & 0x80 ? 3 : 2) : 0;
  xhci_ep0_enqueue(slot_id, *(uint64_t *)setup, 8,
                   (2 << 10) | (1 << 6) | (trt << 16));
  if (length > 0 && data)
    xhci_ep0_enqueue(slot_id, (uint64_t)data, length,
                     (3 << 10) | ((setup->request_type & 0x80 ? 1 : 0) << 16));
  xhci_ep0_enqueue(slot_id, 0, 0,
                   (4 << 10) | (1 << 5) |
                       (!(setup->request_type & 0x80) ? (1 << 16) : 0));
  xhci_ring_ep_doorbell(slot_id, 1);
  xhci_trb_t *evt = xhci_wait_for_event(32);
  return (evt && ((evt->status >> 24) & 0xFF) == 1) ? 0 : -1;
}

static int xhci_bulk_transfer(uint8_t slot_id, uint8_t dci, void *data,
                              uint32_t len, int is_in) {
  uint8_t ep_index = (dci == qemu_xhc.slots[slot_id].dci_out) ? 1 : 2;
  xhci_transfer_enqueue(slot_id, ep_index, (uint64_t)data, len,
                        (1 << 10) | (1 << 5));
  xhci_ring_ep_doorbell(slot_id, dci);
  xhci_trb_t *evt = xhci_wait_for_event(32);
  return (evt && ((evt->status >> 24) & 0xFF) == 1) ? 0 : -1;
}

static int xhci_msc_send_scsi(uint8_t slot_id, void *cmd, uint8_t cmd_len,
                              void *data, uint32_t data_len, int is_in) {
  static uint32_t tag = 0x12345678;
  usb_msc_cbw_t cbw = {0x43425355,       tag++, data_len,
                       is_in ? 0x80 : 0, 0,     cmd_len};
  memcpy(cbw.cb, cmd, cmd_len);
  if (xhci_bulk_transfer(slot_id, qemu_xhc.slots[slot_id].dci_out, &cbw, 31,
                         0) != 0)
    return -1;
  if (data_len > 0 &&
      xhci_bulk_transfer(slot_id,
                         is_in ? qemu_xhc.slots[slot_id].dci_in
                               : qemu_xhc.slots[slot_id].dci_out,
                         data, data_len, is_in) != 0)
    return -1;
  usb_msc_csw_t csw = {0};
  if (xhci_bulk_transfer(slot_id, qemu_xhc.slots[slot_id].dci_in, &csw, 13,
                         1) != 0)
    return -1;
  return (csw.status == 0) ? 0 : -1;
}

static void xhci_configure_endpoint(uint8_t slot_id, int dci,
                                    int max_packet_size, int interval) {
  void *tr_raw = kmalloc(1024 + 64);
  uintptr_t tr_phys = ((uintptr_t)tr_raw + 63) & ~63;
  memset((void *)tr_phys, 0, 1024);
  xhci_trb_t *ring = (xhci_trb_t *)tr_phys;
  ring[63].parameter = (uint64_t)tr_phys;
  ring[63].control = (6 << 10) | (1 << 1) | 1;
  dma_sync_for_device(ring, 1024);
  if (dci == 3 || dci == 2) {
    qemu_xhc.slots[slot_id].ring1_ptr = tr_phys;
    qemu_xhc.slots[slot_id].ring1_enqueue = 0;
    qemu_xhc.slots[slot_id].ring1_cycle = 1;
  } else {
    qemu_xhc.slots[slot_id].ring2_ptr = tr_phys;
    qemu_xhc.slots[slot_id].ring2_enqueue = 0;
    qemu_xhc.slots[slot_id].ring2_cycle = 1;
  }
  size_t ctx_sz = 33 * qemu_xhc.context_size;
  void *ictx = kmalloc(ctx_sz + 64);
  uintptr_t ictx_ph = ((uintptr_t)ictx + 63) & ~63;
  memset((void *)ictx_ph, 0, ctx_sz);
  ((xhci_input_control_ctx_t *)ictx_ph)->add_context_flags =
      (1 << 0) | (1 << dci);
  xhci_slot_ctx_t *sctx =
      (xhci_slot_ctx_t *)xhci_get_context((void *)ictx_ph, 1);
  sctx->ctx_info[0] |= XHCI_CTX_SLOT_CONTEXT_ENTRIES(dci);
  xhci_ep_ctx_t *epctx =
      (xhci_ep_ctx_t *)xhci_get_context((void *)ictx_ph, dci + 1);
  int type = (interval > 0) ? 7 : (dci % 2 == 0 ? 2 : 3);
  epctx->ctx_info[1] |= XHCI_CTX_EP_TYPE(type) |
                        XHCI_CTX_EP_MAX_PACKET_SIZE(max_packet_size) |
                        XHCI_CTX_EP_ERROR_COUNT(3);
  epctx->ctx_info[2] = (uint32_t)tr_phys | 1;
  epctx->ctx_info[3] = (uint32_t)(tr_phys >> 32);
  epctx->ctx_info[4] |= XHCI_CTX_EP_AVG_TRB_LEN(max_packet_size);
  if (interval > 0)
    epctx->ctx_info[0] |=
        XHCI_CTX_EP_INTERVAL(interval >= 16 ? 7 : (interval >= 8 ? 6 : 4));
  dma_sync_for_device((void *)ictx_ph, ctx_sz);
  xhci_send_command(12, ictx_ph, (slot_id << 24));
  xhci_wait_for_event(33);
}

static void xhci_configure_device(uint8_t slot_id) {
  usb_setup_packet_t setup = {0x80, 6, (2 << 8), 0, 9};
  void *buf = kmalloc(512);
  uintptr_t buf_ph = ((uintptr_t)buf + 63) & ~63;
  if (xhci_send_control_transfer(slot_id, &setup, (void *)buf_ph, 9) != 0)
    return;
  uint16_t tlen = ((usb_config_descriptor_t *)buf_ph)->total_length;
  setup.length = tlen;
  if (xhci_send_control_transfer(slot_id, &setup, (void *)buf_ph, tlen) != 0)
    return;
  uint8_t *p = (uint8_t *)buf_ph, *e = p + tlen;
  int epi = 0, mpsi = 0, inti = 0, epo = 0, mpso = 0, msc = 0, mouse = 0,
      iface = 0;
  while (p < e) {
    if (p[1] == 4) {
      usb_interface_descriptor_t *i = (usb_interface_descriptor_t *)p;
      iface = i->interface_number;
      if (i->interface_class == 3)
        mouse =
            (i->interface_protocol == 2 ? 1
                                        : (i->interface_protocol == 1 ? 0 : 2));
      else if (i->interface_class == 8)
        msc = 1;
    } else if (p[1] == 5) {
      usb_endpoint_descriptor_t *ep = (usb_endpoint_descriptor_t *)p;
      if (ep->endpoint_address & 0x80) {
        epi = ep->endpoint_address;
        mpsi = ep->max_packet_size;
        inti = ep->interval;
      } else {
        epo = ep->endpoint_address;
        mpso = ep->max_packet_size;
      }
    }
    p += p[0];
  }
  usb_setup_packet_t scfg = {0, 9, 1, 0, 0};
  xhci_send_control_transfer(slot_id, &scfg, 0, 0);
  if (msc) {
    int di = ((epi & 0xF) * 2) + 1, do_ = ((epo & 0xF) * 2);
    qemu_xhc.slots[slot_id].device_type = 3;
    qemu_xhc.slots[slot_id].dci_in = di;
    qemu_xhc.slots[slot_id].dci_out = do_;
    xhci_configure_endpoint(slot_id, do_, mpso, 0);
    xhci_configure_endpoint(slot_id, di, mpsi, 0);
    scsi_inquiry_data_t inq = {0};
    uint8_t icmd[6] = {0x12, 0, 0, 0, 36, 0};
    if (xhci_msc_send_scsi(slot_id, icmd, 6, &inq, 36, 1) == 0) {
      serial_write_str("xHCI: MSC Vendor: ");
      for (int i = 0; i < 8; i++)
        serial_putc(inq.vendor_id[i]);
      serial_write_str(" Product: ");
      for (int i = 0; i < 16; i++)
        serial_putc(inq.product_id[i]);
      serial_write_str("\r\n");
    }
    scsi_read_capacity_data_t cap = {0};
    uint8_t ccmd[10] = {0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    if (xhci_msc_send_scsi(slot_id, ccmd, 10, &cap, 8, 1) == 0) {
      qemu_xhc.slots[slot_id].msc_max_lba = __builtin_bswap32(cap.max_lba);
      qemu_xhc.slots[slot_id].msc_block_size =
          __builtin_bswap32(cap.block_size);
    }
  } else if (epi) {
    int d = ((epi & 0xF) * 2) + 1;
    qemu_xhc.slots[slot_id].device_type = mouse;
    qemu_xhc.slots[slot_id].dci_in = d;
    xhci_configure_endpoint(slot_id, d, mpsi, inti);
    for (int i = 0; i < 8; i++) {
      void *d_raw = kmalloc(mpsi + 64);
      uintptr_t dp = ((uintptr_t)d_raw + 63) & ~63;
      xhci_transfer_enqueue(slot_id, 1, dp, mpsi, (1 << 10) | (1 << 5));
    }
    xhci_ring_ep_doorbell(slot_id, d);
  }
}

int qemu_xhci_msc_read_sectors(uint8_t slot_id, uint32_t lba, uint16_t count,
                               void *buf) {
  uint8_t cmd[10] = {0x28, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  uint32_t blba = __builtin_bswap32(lba);
  uint16_t bcnt = __builtin_bswap16(count);
  memcpy(&cmd[2], &blba, 4);
  memcpy(&cmd[7], &bcnt, 2);
  uint32_t len = count * qemu_xhc.slots[slot_id].msc_block_size;
  dma_sync_for_device(buf, len);
  int r = xhci_msc_send_scsi(slot_id, cmd, 10, buf, len, 1);
  dma_sync_for_device(buf, len);
  return r;
}

int qemu_xhci_msc_write_sectors(uint8_t slot_id, uint32_t lba, uint16_t count,
                                const void *buf) {
  uint8_t cmd[10] = {0x2A, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  uint32_t blba = __builtin_bswap32(lba);
  uint16_t bcnt = __builtin_bswap16(count);
  memcpy(&cmd[2], &blba, 4);
  memcpy(&cmd[7], &bcnt, 2);
  uint32_t len = count * qemu_xhc.slots[slot_id].msc_block_size;
  dma_sync_for_device((void *)buf, len);
  return xhci_msc_send_scsi(slot_id, cmd, 10, (void *)buf, len, 0);
}

static void xhci_scan_port(int i) {
  uint32_t psc = qemu_xhc.port_regs[i].portsc;
  if (psc & 1) {
    serial_write_str("xHCI: Device detected on port ");
    serial_write_dec(i + 1);
    serial_write_str(" (Status: ");
    serial_write_hex(psc);
    serial_write_str(")\r\n");

    if (psc & 2)
      return;
    qemu_xhc.port_regs[i].portsc = (psc & 0x0E000C00) | 0x00FE0000 | (1 << 9);
    uint8_t spd = (qemu_xhc.port_regs[i].portsc >> 10) & 0xF;
    if (spd == 4)
      qemu_xhc.port_regs[i].portsc =
          (qemu_xhc.port_regs[i].portsc & 0x0E000C00) | (1u << 31) | (1 << 9);
    else
      qemu_xhc.port_regs[i].portsc =
          (qemu_xhc.port_regs[i].portsc & 0x0E000C00) | (1 << 4) | (1 << 9);
    for (volatile int k = 0; k < 2000; k++) {
      if (qemu_xhc.port_regs[i].portsc & (1 << 21))
        break;
    }
    qemu_xhc.port_regs[i].portsc =
        (qemu_xhc.port_regs[i].portsc & 0x0E000C00) | 0x00FE0000 | (1 << 9);
    if (!(qemu_xhc.port_regs[i].portsc & 2))
      return;
    spd = (qemu_xhc.port_regs[i].portsc >> 10) & 0xF;
    xhci_send_command(9, 0, 0);
    xhci_trb_t *evt = xhci_wait_for_event(33);
    if (evt) {
      uint32_t sid = (evt->control >> 24) & 0xFF;
      for (volatile int k = 0; k < 10000000; k++)
        ;
      if (qemu_xhci_address_device(sid, i + 1, spd) == 0)
        xhci_get_descriptor(sid);
    }
  }
}

static void xhci_check_ports() {
  uint32_t mp = (qemu_xhc.cap_regs->hcs_params1 >> 24) & 0xFF;
  qemu_xhc.port_regs =
      (xhci_port_regs_t *)((uintptr_t)qemu_xhc.op_regs + 0x400);

  serial_write_str("xHCI: Power cycling ");
  serial_write_dec(mp);
  serial_write_str(" ports...\r\n");

  // 1. Power OFF all ports
  for (uint32_t i = 0; i < mp; i++) {
    // Clear Port Power (bit 9) and clear status change bits
    qemu_xhc.port_regs[i].portsc =
        (qemu_xhc.port_regs[i].portsc & 0x0E000C00 & ~(1 << 9)) | 0x00FE0000;
  }

  // Wait for power to drop (approx 100ms)
  for (volatile int k = 0; k < 10000000; k++)
    ;

  // 2. Power ON all ports
  for (uint32_t i = 0; i < mp; i++) {
    // Set Port Power (bit 9) and clear status change bits
    qemu_xhc.port_regs[i].portsc =
        (qemu_xhc.port_regs[i].portsc & 0x0E000C00) | (1 << 9) | 0x00FE0000;
  }

  // Wait for stability and connection detection (approx 500ms)
  for (volatile int k = 0; k < 50000000; k++)
    ;

  for (uint32_t i = 0; i < mp; i++) {
    xhci_scan_port(i);
  }
}

void qemu_usb_poll() {
  if (!qemu_xhc.base_addr)
    return;
  static int pc = 0;
  pc++;
  if (pc % 5000 == 0) {
    uint32_t mp = (qemu_xhc.cap_regs->hcs_params1 >> 24) & 0xFF;
    for (uint32_t i = 0; i < mp; i++)
      if ((qemu_xhc.port_regs[i].portsc & 1) &&
          !(qemu_xhc.port_regs[i].portsc & 2))
        xhci_scan_port(i);
  }
  while (1) {
    xhci_trb_t *evt = xhci_poll_event();
    if (!evt)
      break;
    uint32_t type = (evt->control >> 10) & 0x3F;
    if (type == 34) {
      uint32_t p = (evt->parameter >> 24) & 0xFF;
      if (p)
        xhci_scan_port(p - 1);
    } else if (type == 32) {
      uint32_t cc = (evt->status >> 24) & 0xFF;
      if (cc == 1 || cc == 13) {
        uint8_t sid = (evt->control >> 24) & 0xFF;
        uint32_t dt = qemu_xhc.slots[sid].device_type;
        if (dt == 3)
          continue;
        xhci_trb_t *trb = (xhci_trb_t *)evt->parameter;
        uint8_t *b = (uint8_t *)trb->parameter;
        clflush(b);
        uint64_t pr = qemu_xhc.slots[sid].last_report;
        if (dt == 1) {
          input_mouse_update((int8_t)b[1] * 4, (int8_t)b[2] * 4, b[0]);
          xhci_transfer_enqueue(sid, 1, (uintptr_t)b, 8, (1 << 10) | (1 << 5));
          xhci_ring_ep_doorbell(sid, qemu_xhc.slots[sid].dci_in);
        } else if (dt == 2) {
          uint16_t x = b[1] | (b[2] << 8), y = b[3] | (b[4] << 8);
          input_mouse_update_abs(x, y, b[0]);
          xhci_transfer_enqueue(sid, 1, (uintptr_t)b, 8, (1 << 10) | (1 << 5));
          xhci_ring_ep_doorbell(sid, qemu_xhc.slots[sid].dci_in);
        } else if (dt == 0) {
          uint64_t cr = *(uint64_t *)b;
          if (cr != pr) {
            qemu_xhc.slots[sid].last_report = cr;
            uint8_t *ck = b + 2, *pk = (uint8_t *)&pr + 2;
            for (int i = 0; i < 6; i++) {
              uint8_t sc = ck[i];
              if (!sc)
                continue;
              int wp = 0;
              for (int j = 0; j < 6; j++)
                if (pk[j] == sc)
                  wp = 1;
              if (!wp && sc < 58) {
                int shift = (b[0] & 0x22) ? 1 : 0; // LShift or RShift
                char c = shift ? hid_keymap_upper[sc] : hid_keymap_lower[sc];
                if (c) {
                  uint32_t event = (uint8_t)c;
                  if (shift)
                    event |= (1 << 25); // KB_MOD_SHIFT
                  input_keyboard_push(event);
                }
              }
            }
          }
          xhci_transfer_enqueue(sid, 1, (uintptr_t)b, 8, (1 << 10) | (1 << 5));
          xhci_ring_ep_doorbell(sid, qemu_xhc.slots[sid].dci_in);
        }
      }
    }
  }
}

int qemu_xhci_find_msc_slot() {
  for (int i = 1; i <= qemu_xhc.max_slots; i++)
    if (qemu_xhc.slots[i].device_type == 3)
      return i;
  return -1;
}

int qemu_xhci_address_device(uint8_t slot_id, uint8_t root_hub_port_num,
                             uint8_t port_speed) {
  size_t dsz = 32 * qemu_xhc.context_size;
  void *dctx = kmalloc(dsz + 64);
  uintptr_t dph = ((uintptr_t)dctx + 63) & ~63;
  memset((void *)dph, 0, dsz);
  qemu_xhc.dcbaa[slot_id] = dph;
  dma_sync_for_device(qemu_xhc.dcbaa, (qemu_xhc.max_slots + 1) * 8);
  void *traw = kmalloc(1024 + 64);
  uintptr_t tph = ((uintptr_t)traw + 63) & ~63;
  memset((void *)tph, 0, 1024);
  xhci_trb_t *r = (xhci_trb_t *)tph;
  r[63].parameter = tph;
  r[63].control = (6 << 10) | (1 << 1) | 1;
  dma_sync_for_device(r, 1024);
  qemu_xhc.slots[slot_id].ep0_ring = r;
  qemu_xhc.slots[slot_id].ep0_cycle_state = 1;
  qemu_xhc.slots[slot_id].ep0_enqueue_ptr = 0;
  size_t isz = 33 * qemu_xhc.context_size;
  void *ictx = kmalloc(isz + 64);
  uintptr_t iph = ((uintptr_t)ictx + 63) & ~63;
  memset((void *)iph, 0, isz);
  ((xhci_input_control_ctx_t *)iph)->add_context_flags = 3;
  xhci_slot_ctx_t *s = (xhci_slot_ctx_t *)xhci_get_context((void *)iph, 1);
  s->ctx_info[0] |=
      XHCI_CTX_SLOT_SPEED(port_speed) | XHCI_CTX_SLOT_CONTEXT_ENTRIES(1);
  s->ctx_info[1] |= XHCI_CTX_SLOT_ROOT_HUB_PORT(root_hub_port_num);
  xhci_ep_ctx_t *e = (xhci_ep_ctx_t *)xhci_get_context((void *)iph, 2);
  uint32_t mps = (port_speed == 4 ? 512 : (port_speed >= 2 ? 64 : 8));
  e->ctx_info[1] |= XHCI_CTX_EP_TYPE(4) | XHCI_CTX_EP_MAX_PACKET_SIZE(mps) |
                    XHCI_CTX_EP_ERROR_COUNT(3);
  e->ctx_info[2] = (uint32_t)tph | 1;
  e->ctx_info[3] = (uint32_t)(tph >> 32);
  e->ctx_info[4] |= XHCI_CTX_EP_AVG_TRB_LEN(8);
  dma_sync_for_device((void *)iph, isz);
  xhci_send_command(11, iph, (slot_id << 24));
  xhci_trb_t *evt = xhci_wait_for_event(33);
  if (evt && ((evt->status >> 24) & 0xFF) == 1) {
    xhci_configure_device(slot_id);
    return 0;
  }
  return -1;
}

static void xhci_get_descriptor(uint8_t sid) {
  usb_setup_packet_t s = {0x80, 6, (1 << 8), 0, 18};
  void *b = kmalloc(512);
  uintptr_t bp = ((uintptr_t)b + 63) & ~63;
  xhci_send_control_transfer(sid, &s, (void *)bp, 18);
}

int qemu_xhci_init(uintptr_t base, uint8_t bus, uint8_t dev, uint8_t func) {

  serial_write_str("hi from xhci");
  serial_write_str("\r\n");

  uint32_t id = pci_read_config(bus, dev, func, 0);
  if ((id & 0xFFFF) == 0x8086) {
    pci_write_config(bus, dev, func, 0xD4, 0xFFFFFFFF);
    pci_write_config(bus, dev, func, 0xD0, 0xFFFFFFFF);
    pci_write_config(bus, dev, func, 0xE0, 0xFFFFFFFF);
    pci_write_config(bus, dev, func, 0xDC, 0xFFFFFFFF);
    for (volatile int i = 0; i < 100000000; i++)
      ;
  }
  qemu_xhc.base_addr = base;
  qemu_xhc.cap_regs = (xhci_cap_regs_t *)base;
  qemu_xhc.op_regs = (xhci_op_regs_t *)(base + qemu_xhc.cap_regs->cap_length);
  qemu_xhc.run_regs =
      (xhci_runtime_regs_t *)(base + qemu_xhc.cap_regs->rts_off);
  qemu_xhc.db_regs = (xhci_doorbell_regs_t *)(base + qemu_xhc.cap_regs->db_off);
  qemu_xhc.context_size = (qemu_xhc.cap_regs->hcc_params1 & 4) ? 64 : 32;
  xhci_reset_controller();
  int t = 10000;
  while ((qemu_xhc.op_regs->usb_sts & (1 << 11)) && t-- > 0)
    for (volatile int i = 0; i < 100000; i++)
      ;
  xhci_alloc_dcbaa();
  xhci_alloc_cmd_ring();
  xhci_alloc_event_ring();
  qemu_xhc.op_regs->config = qemu_xhc.max_slots;
  qemu_xhc.op_regs->usb_cmd |= (1 | 4);
  t = 10000;
  while ((qemu_xhc.op_regs->usb_sts & 1) && t-- > 0)
    for (volatile int i = 0; i < 10000; i++)
      ;
  xhci_check_ports();
  return 0;
}
