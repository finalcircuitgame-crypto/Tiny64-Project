#ifndef PCI_H
#define PCI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
} pci_device_t;

#define MAX_PCI_DEVICES 64
extern pci_device_t g_pci_devices[MAX_PCI_DEVICES];
extern int g_pci_device_count;

#define PCI_COMMAND 0x04

void pci_init();
extern char g_gpu_model[64];

uint32_t pci_read_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint16_t pci_read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void     pci_write16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value);
void     pci_write_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
void     pci_enable_bus_master_and_mem(uint8_t bus, uint8_t device, uint8_t function);

#ifdef __cplusplus
}
#endif

#endif
