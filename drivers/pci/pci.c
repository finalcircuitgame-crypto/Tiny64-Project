#include <arch/x86/io.h>
#include <kernel/pci.h>
#include <kernel/printk.h>
#include <drivers/net/e1000.h>
#include <drivers/usb/xhci.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func,
                           uint8_t offset) {
  uint32_t address;
  uint32_t lbus = (uint32_t)bus;
  uint32_t lslot = (uint32_t)slot;
  uint32_t lfunc = (uint32_t)func;

  address = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) |
                       (offset & 0xfc) | ((uint32_t)0x80000000));

  outl(PCI_CONFIG_ADDRESS, address);
  return inl(PCI_CONFIG_DATA);
}

uint16_t pci_config_read16(uint8_t bus, uint8_t slot, uint8_t func,
                           uint8_t offset) {
  uint32_t address;
  uint32_t lbus = (uint32_t)bus;
  uint32_t lslot = (uint32_t)slot;
  uint32_t lfunc = (uint32_t)func;

  address = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) |
                       (offset & 0xfc) | ((uint32_t)0x80000000));

  outl(PCI_CONFIG_ADDRESS, address);
  return (uint16_t)((inl(PCI_CONFIG_DATA) >> ((offset & 2) * 8)) & 0xffff);
}

void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t func,
                        uint8_t offset, uint32_t value) {
  uint32_t address;
  uint32_t lbus = (uint32_t)bus;
  uint32_t lslot = (uint32_t)slot;
  uint32_t lfunc = (uint32_t)func;

  address = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) |
                       (offset & 0xfc) | ((uint32_t)0x80000000));

  outl(PCI_CONFIG_ADDRESS, address);
  outl(PCI_CONFIG_DATA, value);
}

void pci_enumerate(void) {
  printk("Scanning PCI Bus...\n");
  for (int bus = 0; bus < 256; bus++) {
    for (int slot = 0; slot < 32; slot++) {
      for (int func = 0; func < 8; func++) {
        uint32_t vendor_device = pci_config_read32(bus, slot, func, 0);
        if ((vendor_device & 0xFFFF) == 0xFFFF)
          continue;

        uint16_t vendor = vendor_device & 0xFFFF;
        uint16_t device = vendor_device >> 16;

        uint32_t class_info = pci_config_read32(bus, slot, func, 8);
        uint8_t class_code = class_info >> 24;
        uint8_t subclass = (class_info >> 16) & 0xFF;
        uint8_t prog_if = (class_info >> 8) & 0xFF;

        printk("  PCI %d:%d:%d - ID:%x:%x Class:%x.%x IF:%x\n", bus, slot, func,
               vendor, device, class_code, subclass, prog_if);

        if (vendor == 0x8086 && class_code == 0x02 && subclass == 0x00) {
          e1000_init(bus, slot, func);
        } else if (class_code == 0x0C && subclass == 0x03 && prog_if == 0x30) {
          xhci_init(bus, slot, func);
        }

        // If not a multi-function device, don't check other functions
        if (func == 0) {
          uint32_t header_type = pci_config_read32(bus, slot, func, 12);
          if (!(header_type & 0x800000))
            break;
        }
      }
    }
  }
}