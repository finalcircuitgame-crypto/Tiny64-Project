#include "pci.h"
#include "../../graphics/framebuffer.h"
#include "../../lib/string.h"
#include "../network/e1000.h"
#include "../network/e1000e.h"
#include "../platform/serial.h"
#include "../storage/ahci.h"
#include "../storage/nvme.h"
#include "../usb/xhci/xhci.h"
#include "../../memory/vmm.h"

#include "../network/network.h"

extern Framebuffer *g_kernel_fb;
extern void report_error(Framebuffer *fb, const char *error_msg);

char g_gpu_model[64] = "Generic VGA";

pci_device_t g_pci_devices[MAX_PCI_DEVICES];
int g_pci_device_count = 0;

// --- Boot ordering modifications: Network first, then xHCI ---
static int g_network_ready = 0;
int g_nic_type = 0; // 0: None, 1: e1000, 2: e1000e
static uint64_t g_pending_xhci_mmio = 0;
static uint8_t g_pending_xhci_bus = 0, g_pending_xhci_dev = 0, g_pending_xhci_func = 0;
static uint16_t g_pending_xhci_vendor = 0, g_pending_xhci_device = 0;

extern uint32_t e1000_get_status_reg();
extern uint32_t e1000e_get_status_reg();
extern uint8_t* e1000_get_mac_addr();
extern uint8_t* e1000e_get_mac_addr();

network_status_t network_get_status() {
    if (g_nic_type == 1) {
        uint32_t s = e1000_get_status_reg();
        return (s & (1 << 1)) ? NET_STATUS_LINK_UP : NET_STATUS_LINK_DOWN;
    } else if (g_nic_type == 2) {
        uint32_t s = e1000e_get_status_reg();
        return (s & (1 << 1)) ? NET_STATUS_LINK_UP : NET_STATUS_LINK_DOWN;
    }
    return NET_STATUS_NONE;
}

void network_get_mac(uint8_t mac[6]) {
    uint8_t* m = 0;
    if (g_nic_type == 1) m = e1000_get_mac_addr();
    else if (g_nic_type == 2) m = e1000e_get_mac_addr();
    if (m) memcpy(mac, m, 6);
    else memset(mac, 0, 6);
}

void network_get_ip(uint8_t ip[4]) {
    // Mock IP for now
    ip[0] = 172; ip[1] = 16; ip[2] = 6; ip[3] = 222;
}

extern void e1000e_send_raw_broadcast_test(void);
extern void e1000_net_log_callback(char c);
extern void e1000e_net_log_callback(char c);

static void pci_start_network_logging(void (*hook)(char)) {
  serial_write_str("PCI: Live network logging ENABLED.\r\n");
  serial_set_net_log_hook(hook);
}

static inline uint32_t inl(uint16_t port) {
  uint32_t ret;
  __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

static inline void outl(uint16_t port, uint32_t val) {
  __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}



uint16_t pci_read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t val = pci_read_config(bus, device, function, offset & 0xFC);
    if (offset & 2) return (uint16_t)(val >> 16);
    return (uint16_t)(val & 0xFFFF);
}

void pci_write16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value) {
    uint32_t val = pci_read_config(bus, device, function, offset & 0xFC);
    if (offset & 2) val = (val & 0xFFFF) | ((uint32_t)value << 16);
    else           val = (val & 0xFFFF0000) | value;
    pci_write_config(bus, device, function, offset & 0xFC, val);
}

uint32_t pci_read_config(uint8_t bus, uint8_t device, uint8_t function,
                         uint8_t offset) {
  uint32_t address = ((uint32_t)bus << 16) | ((uint32_t)device << 11) |
                     ((uint32_t)function << 8) | (offset & 0xFC) | 0x80000000;
  outl(0xCF8, address);
  return inl(0xCFC);
}

void pci_write_config(uint8_t bus, uint8_t device, uint8_t function,
                      uint8_t offset, uint32_t value) {
  uint32_t address = ((uint32_t)bus << 16) | ((uint32_t)device << 11) |
                     ((uint32_t)function << 8) | (offset & 0xFC) | 0x80000000;
  outl(0xCF8, address);
  outl(0xCFC, value);
}

void pci_enable_bus_master_and_mem(uint8_t bus, uint8_t device,
                                   uint8_t function) {
  uint32_t cmd = pci_read_config(bus, device, function, 0x04);
  cmd |= (1 << 2) | (1 << 1);
  pci_write_config(bus, device, function, 0x04, cmd);

  uint32_t verify = pci_read_config(bus, device, function, 0x04);
  if ((verify & 0x6) != 0x6) {
    serial_write_str("PCI: Warning: Failed to enable Bus Master/Mem for ");
    serial_write_dec(bus);
    serial_write_str(":");
    serial_write_dec(device);
    serial_write_str("\r\n");
  }
}

int xhci_bios_handover(uintptr_t base_addr) {
  xhci_cap_regs_t *caps = (xhci_cap_regs_t *)base_addr;
  if (base_addr == 0 || base_addr == 0xFFFFFFFF) {
    serial_write_str("xHCI: Invalid MMIO base for BIOS handover\r\n");
    return 0;
  }

  uint32_t hcc = caps->hcc_params1;
  uint32_t xecp_offset = (hcc >> 16) << 2;
  serial_write_str("xHCI: HCCPARAMS1=");
  serial_write_hex(hcc);
  serial_write_str(" XECPOFF=");
  serial_write_hex(xecp_offset);
  serial_write_str("\r\n");

  if (xecp_offset == 0) {
    serial_write_str("xHCI: No Extended Capabilities, assuming no legacy BIOS "
                     "handover needed\r\n");
    return 1;
  }

  int max_depth = 16;
  while (xecp_offset && max_depth-- > 0) {
    volatile uint32_t *xecp = (volatile uint32_t *)(base_addr + xecp_offset);
    uint32_t val = *xecp;
    uint8_t id = val & 0xFF;
    uint8_t next = (val >> 8) & 0xFF;

    serial_write_str("xHCI: XECAP @");
    serial_write_hex((uint32_t)xecp_offset);
    serial_write_str(" VAL=");
    serial_write_hex(val);
    serial_write_str(" ID=");
    serial_write_dec(id);
    serial_write_str(" NEXT=");
    serial_write_dec(next);
    serial_write_str("\r\n");

    if (id == 1) {
      serial_write_str("xHCI: USB Legacy Support Capability found, requesting "
                       "OS ownership\r\n");
      *xecp |= (1 << 24);

      int timeout = 1000;
      while ((*xecp & (1 << 16)) && timeout-- > 0) {
        for (volatile int i = 0; i < 100000; i++)
          ;
      }

      if (*xecp & (1 << 16)) {
        serial_write_str("xHCI: BIOS handover timed out, BIOS still owns HC, "
                         "forcing continue\r\n");
      } else {
        serial_write_str(
            "xHCI: BIOS handover successful, BIOS released ownership\r\n");
      }

      volatile uint32_t *legctl =
          (volatile uint32_t *)(base_addr + xecp_offset + 4);
      uint32_t legctl_val = *legctl;
      serial_write_str("xHCI: USBLEGCTLSTS before=");
      serial_write_hex(legctl_val);
      serial_write_str("\r\n");

      legctl_val &= ~0x1F1F;
      legctl_val |= 0xE0000000;
      *legctl = legctl_val;

      serial_write_str("xHCI: USBLEGCTLSTS after=");
      serial_write_hex(*legctl);
      serial_write_str("\r\n");

      return 1;
    }

    if (!next)
      break;
    xecp_offset += (next << 2);
  }

  serial_write_str("xHCI: No USB Legacy Support Capability found, continuing "
                   "without BIOS handover\r\n");
  return 1;
}

extern int qemu_xhci_init(uintptr_t base, uint8_t bus, uint8_t dev, uint8_t func);
extern int g_using_qemu_xhci;
extern int is_qemu(void);

void pci_init() {
  serial_write_str("PCI: Scanning all buses...\r\n");
  g_pci_device_count = 0;
  for (int bus = 0; bus < 256; bus++) {
    for (int dev = 0; dev < 32; dev++) {
      uint32_t id = pci_read_config((uint8_t)bus, (uint8_t)dev, 0, 0);
      if ((id & 0xFFFF) == 0xFFFF)
        continue;

      uint32_t hdr = pci_read_config((uint8_t)bus, (uint8_t)dev, 0, 0x0C);
      uint8_t header = (hdr >> 16) & 0xFF;
      int n_func = (header & 0x80) ? 8 : 1;

      for (int func = 0; func < n_func; func++) {
        uint32_t info =
            pci_read_config((uint8_t)bus, (uint8_t)dev, (uint8_t)func, 0x08);
        uint16_t class_sub = (info >> 16);
        uint8_t prog_if = (info >> 8) & 0xFF;
        uint32_t pci_id = pci_read_config((uint8_t)bus, (uint8_t)dev, (uint8_t)func, 0);

        if ((pci_id & 0xFFFF) == 0xFFFF)
          continue;

        if (g_pci_device_count < MAX_PCI_DEVICES) {
            pci_device_t* dev_entry = &g_pci_devices[g_pci_device_count++];
            dev_entry->bus = (uint8_t)bus;
            dev_entry->device = (uint8_t)dev;
            dev_entry->function = (uint8_t)func;
            dev_entry->vendor_id = pci_id & 0xFFFF;
            dev_entry->device_id = pci_id >> 16;
            dev_entry->class_code = class_sub >> 8;
            dev_entry->subclass = class_sub & 0xFF;
            dev_entry->prog_if = prog_if;
        }
      }
    }
  }

  serial_write_str("PCI: First Priority - Initializing Network Drivers...\r\n");
  for (int i = 0; i < g_pci_device_count; i++) {
    pci_device_t* dev = &g_pci_devices[i];
    uint16_t class_sub = ((uint16_t)dev->class_code << 8) | dev->subclass;

    if (class_sub == 0x0200) {
      serial_write_str("PCI: Ethernet Controller detected at ");
      serial_write_dec(dev->bus);
      serial_write_str(":");
      serial_write_dec(dev->device);
      serial_write_str(":");
      serial_write_dec(dev->function);
      serial_write_str(" ID=");
      serial_write_hex(dev->vendor_id);
      serial_write_str(":");
      serial_write_hex(dev->device_id);
      serial_write_str("\r\n");

      pci_enable_bus_master_and_mem(dev->bus, dev->device, dev->function);

      uint32_t bar0 = pci_read_config(dev->bus, dev->device, dev->function, 0x10);
      uint32_t bar1 = pci_read_config(dev->bus, dev->device, dev->function, 0x14);
      uint64_t mmio_phys;
      if (((bar0 >> 1) & 3) == 2) { // 64-bit BAR
        mmio_phys = ((uint64_t)(bar1 & ~0xF) << 32) | (bar0 & ~0xF);
      } else {
        mmio_phys = bar0 & ~0xF;
      }

      uintptr_t mmio_virt = (uintptr_t)map_mmio(mmio_phys, 0x20000);
      if (dev->vendor_id == 0x8086) {
        void (*log_hook)(char) = 0;
        if ((dev->device_id & 0xFF00) == 0x1000) {
          serial_write_str("e1000: Using legacy e1000 driver\r\n");
          e1000_enable_busmaster(dev->bus, dev->device, dev->function);
          e1000_init(mmio_virt);
          e1000_send_raw_broadcast_test();
          serial_write_str("PCI: Network Test Packet Heard Successfully (e1000)\r\n");
          log_hook = e1000_net_log_callback;
          g_nic_type = 1;
        } else {
          serial_write_str("e1000e: Using e1000e driver for Intel NIC\r\n");
          e1000e_enable_busmaster(dev->bus, dev->device, dev->function);
          e1000e_init(mmio_virt);
          e1000e_send_raw_broadcast_test();
          serial_write_str("PCI: Network Test Packet Heard Successfully (e1000e)\r\n");
          log_hook = e1000e_net_log_callback;
          g_nic_type = 2;
        }
        g_network_ready = 1;
        if (log_hook) pci_start_network_logging(log_hook);
      }
    }
  }

  serial_write_str("PCI: Initializing Remaining Drivers...\r\n");
  for (int i = 0; i < g_pci_device_count; i++) {
    pci_device_t* dev = &g_pci_devices[i];
    uint16_t class_sub = ((uint16_t)dev->class_code << 8) | dev->subclass;

    if (class_sub == 0x0200) continue; // Already done

    if (class_sub == 0x0300) {
      serial_write_str("PCI: VGA Controller VEN=");
      serial_write_hex(dev->vendor_id);
      serial_write_str(" DEV=");
      serial_write_hex(dev->device_id);
      serial_write_str("\r\n");
      if (dev->vendor_id == 0x8086) strcpy(g_gpu_model, "Intel Graphics");
      else if (dev->vendor_id == 0x10DE) strcpy(g_gpu_model, "NVIDIA Graphics");
      else if (dev->vendor_id == 0x1002) strcpy(g_gpu_model, "AMD Radeon");
      else if (dev->vendor_id == 0x1234) strcpy(g_gpu_model, "QEMU VGA");
      else if (dev->vendor_id == 0x1AF4 && dev->device_id == 0x1050) strcpy(g_gpu_model, "VirtIO GPU (Accelerated)");
      else strcpy(g_gpu_model, "Standard VGA");
    } else if (class_sub == 0x0C03 && dev->prog_if == 0x30) {
      serial_write_str("PCI: xHCI Controller detected at ");
      serial_write_dec(dev->bus);
      serial_write_str(":");
      serial_write_dec(dev->device);
      serial_write_str(":");
      serial_write_dec(dev->function);
      serial_write_str("\r\n");

      pci_enable_bus_master_and_mem(dev->bus, dev->device, dev->function);
      uint32_t bar0 = pci_read_config(dev->bus, dev->device, dev->function, 0x10);
      uint64_t mmio_phys = (bar0 & 0xFFFFFFF0);
      if (((bar0 >> 1) & 3) == 2) {
        uint32_t bar1 = pci_read_config(dev->bus, dev->device, dev->function, 0x14);
        mmio_phys |= ((uint64_t)bar1 << 32);
      }
      uintptr_t mmio_virt = (uintptr_t)map_mmio(mmio_phys, 0x10000);
      g_pending_xhci_mmio = mmio_virt;
      g_pending_xhci_bus = dev->bus;
      g_pending_xhci_dev = dev->device;
      g_pending_xhci_func = dev->function;
      g_pending_xhci_vendor = dev->vendor_id;
      g_pending_xhci_device = dev->device_id;
    } else if (class_sub == 0x0108 && dev->prog_if == 0x02) {
      pci_enable_bus_master_and_mem(dev->bus, dev->device, dev->function);
      uint32_t bar0 = pci_read_config(dev->bus, dev->device, dev->function, 0x10);
      uint64_t mmio = (bar0 & 0xFFFFFFF0);
      if (((bar0 >> 1) & 3) == 2) {
        uint32_t bar1 = pci_read_config(dev->bus, dev->device, dev->function, 0x14);
        mmio |= ((uint64_t)bar1 << 32);
      }
      nvme_init(mmio);
    } else if (class_sub == 0x0106 && dev->prog_if == 0x01) {
      pci_enable_bus_master_and_mem(dev->bus, dev->device, dev->function);
      uint32_t bar5 = pci_read_config(dev->bus, dev->device, dev->function, 0x24);
      uint64_t mmio_phys = (bar5 & 0xFFFFFFF0);
      if (((bar5 >> 1) & 3) == 2) { // 64-bit BAR
        uint32_t bar6 = pci_read_config(dev->bus, dev->device, dev->function, 0x28);
        mmio_phys |= ((uint64_t)bar6 << 32);
      }
      uintptr_t mmio_virt = (uintptr_t)map_mmio(mmio_phys, 0x10000);
      ahci_init(mmio_virt);
    }
  }

  if (g_pending_xhci_mmio) {
    if (g_network_ready) {
      serial_write_str("PCI: Waiting 2s after first NIC packet before xHCI init...\r\n");
      for (volatile unsigned long delay = 0; delay < 400000000UL; delay++);
    }
    if (xhci_bios_handover(g_pending_xhci_mmio)) {
      int r;
      if (is_qemu() || g_pending_xhci_vendor == 0x1b36 || g_pending_xhci_vendor == 0x1234) {
          r = qemu_xhci_init(g_pending_xhci_mmio, g_pending_xhci_bus, g_pending_xhci_dev, g_pending_xhci_func);
          if (r == 0) g_using_qemu_xhci = 1;
      } else {
          r = xhci_init(g_pending_xhci_mmio, g_pending_xhci_bus, g_pending_xhci_dev, g_pending_xhci_func);
          if (r == 0) g_using_qemu_xhci = 0;
      }
      if (r != 0) report_error(g_kernel_fb, "USB xHCI Initialization Failed");
    } else {
      report_error(g_kernel_fb, "USB BIOS Handover Failed");
    }
  }

  serial_write_str("PCI: Scan Complete.\r\n");
}
