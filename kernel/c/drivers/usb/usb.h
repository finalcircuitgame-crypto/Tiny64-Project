#ifndef USB_H
#define USB_H

#include <stdint.h>

// USB Mass Storage Class
#define USB_CLASS_MSC           0x08
#define USB_SUBCLASS_SCSI       0x06
#define USB_PROTOCOL_BOT        0x50

// SCSI Commands
#define SCSI_CMD_INQUIRY          0x12
#define SCSI_CMD_READ_CAPACITY_10 0x25
#define SCSI_CMD_READ_10          0x28
#define SCSI_CMD_WRITE_10         0x2A
#define SCSI_CMD_TEST_UNIT_READY  0x00

// BOT Command Block Wrapper (CBW)
typedef struct {
    uint32_t signature;          // 'USBC' (0x43425355)
    uint32_t tag;
    uint32_t transfer_length;
    uint8_t  flags;              // Bit 7: Direction (1=IN, 0=OUT)
    uint8_t  lun;
    uint8_t  cmd_len;
    uint8_t  cb[16];
} __attribute__((packed)) usb_msc_cbw_t;

// BOT Command Status Wrapper (CSW)
typedef struct {
    uint32_t signature;          // 'USBS' (0x53425355)
    uint32_t tag;
    uint32_t data_residue;
    uint8_t  status;             // 0=Success, 1=Fail, 2=Phase Error
} __attribute__((packed)) usb_msc_csw_t;

typedef struct {
    uint8_t  device_type;
    uint8_t  removable;
    uint8_t  version;
    uint8_t  response_format;
    uint8_t  additional_length;
    uint8_t  reserved[3];
    char     vendor_id[8];
    char     product_id[16];
    char     product_rev[4];
} __attribute__((packed)) scsi_inquiry_data_t;

typedef struct {
    uint32_t max_lba;
    uint32_t block_size;
} __attribute__((packed)) scsi_read_capacity_data_t;

#ifdef __cplusplus
extern "C" {
#endif

// USB Descriptor Types
#define USB_DESC_DEVICE         0x01
#define USB_DESC_CONFIG         0x02
#define USB_DESC_STRING         0x03
#define USB_DESC_INTERFACE      0x04
#define USB_DESC_ENDPOINT       0x05

typedef struct {
    uint8_t  length;
    uint8_t  descriptor_type;
    uint16_t bcd_usb;
    uint8_t  device_class;
    uint8_t  device_subclass;
    uint8_t  device_protocol;
    uint8_t  max_packet_size0;
    uint16_t id_vendor;
    uint16_t id_product;
    uint16_t bcd_device;
    uint8_t  i_manufacturer;
    uint8_t  i_product;
    uint8_t  i_serial_number;
    uint8_t  num_configurations;
} __attribute__((packed)) usb_device_descriptor_t;

typedef struct {
    uint8_t  request_type;
    uint8_t  request;
    uint16_t value;
    uint16_t index;
    uint16_t length;
} __attribute__((packed)) usb_setup_packet_t;

typedef struct {
    uint8_t  length;
    uint8_t  descriptor_type;
    uint16_t total_length;
    uint8_t  num_interfaces;
    uint8_t  configuration_value;
    uint8_t  configuration_index;
    uint8_t  attributes;
    uint8_t  max_power;
} __attribute__((packed)) usb_config_descriptor_t;

typedef struct {
    uint8_t  length;
    uint8_t  descriptor_type;
    uint8_t  interface_number;
    uint8_t  alternate_setting;
    uint8_t  num_endpoints;
    uint8_t  interface_class;
    uint8_t  interface_subclass;
    uint8_t  interface_protocol;
    uint8_t  i_interface;
} __attribute__((packed)) usb_interface_descriptor_t;

typedef struct {
    uint8_t  length;
    uint8_t  descriptor_type;
    uint8_t  endpoint_address;
    uint8_t  attributes;
    uint16_t max_packet_size;
    uint8_t  interval;
} __attribute__((packed)) usb_endpoint_descriptor_t;

typedef struct usb_device {
    uint8_t  address;
    uint8_t  speed;
    uint8_t  slot_id;
    uint8_t  port_num;
    usb_device_descriptor_t desc;
    struct usb_device *next;
} usb_device_t;

void usb_init();
void usb_poll();

// MSC Dispatchers
int xhci_find_msc_slot();
int xhci_msc_read_sectors(uint8_t slot_id, uint32_t lba, uint16_t count, void* buf);
int xhci_msc_write_sectors(uint8_t slot_id, uint32_t lba, uint16_t count, const void* buf);

// Generic xHCI
void xhci_poll();
int generic_xhci_find_msc_slot();
int generic_xhci_msc_read_sectors(uint8_t slot_id, uint32_t lba, uint16_t count, void* buf);
int generic_xhci_msc_write_sectors(uint8_t slot_id, uint32_t lba, uint16_t count, const void* buf);

// QEMU xHCI
void qemu_usb_poll();
int qemu_xhci_find_msc_slot();
int qemu_xhci_msc_read_sectors(uint8_t slot_id, uint32_t lba, uint16_t count, void* buf);
int qemu_xhci_msc_write_sectors(uint8_t slot_id, uint32_t lba, uint16_t count, const void* buf);

// Hot-plugging support
typedef void (*usb_callback_t)(usb_device_t *dev);

void usb_attach_device(uint8_t slot_id, uint8_t port_num, uint8_t speed);
void usb_detach_device(uint8_t slot_id);
usb_device_t* usb_find_device_by_slot(uint8_t slot_id);
usb_device_t* usb_get_device_list();

void usb_set_connect_callback(usb_callback_t cb);
void usb_set_disconnect_callback(usb_callback_t cb);

extern int usb_hid_detected;

#ifdef __cplusplus
}
#endif

#endif
