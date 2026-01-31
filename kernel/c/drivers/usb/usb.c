#include "usb.h"
#include "../pci/pci.h"
#include "../platform/serial.h"
#include "../../memory/heap.h"
#include "../../lib/string.h"

int usb_hid_detected = 0;
static usb_device_t *device_list = NULL;
static usb_callback_t connect_callback = NULL;
static usb_callback_t disconnect_callback = NULL;

int g_using_qemu_xhci = 0;

void usb_init() {
    serial_write_str("USB: Initializing Foundation...\r\n");
    device_list = NULL;
    connect_callback = NULL;
    disconnect_callback = NULL;
}

void usb_poll() {
    if (g_using_qemu_xhci) {
        qemu_usb_poll();
    } else {
        xhci_poll();
    }
}

int xhci_find_msc_slot() {
    if (g_using_qemu_xhci) {
        return qemu_xhci_find_msc_slot();
    } else {
        return generic_xhci_find_msc_slot();
    }
}

int xhci_msc_read_sectors(uint8_t slot_id, uint32_t lba, uint16_t count, void* buf) {
    if (g_using_qemu_xhci) {
        return qemu_xhci_msc_read_sectors(slot_id, lba, count, buf);
    } else {
        return generic_xhci_msc_read_sectors(slot_id, lba, count, buf);
    }
}

int xhci_msc_write_sectors(uint8_t slot_id, uint32_t lba, uint16_t count, const void* buf) {
    if (g_using_qemu_xhci) {
        return qemu_xhci_msc_write_sectors(slot_id, lba, count, buf);
    } else {
        return generic_xhci_msc_write_sectors(slot_id, lba, count, buf);
    }
}

void usb_set_connect_callback(usb_callback_t cb) {
    connect_callback = cb;
}

void usb_set_disconnect_callback(usb_callback_t cb) {
    disconnect_callback = cb;
}

void usb_attach_device(uint8_t slot_id, uint8_t port_num, uint8_t speed) {
    serial_write_str("USB: Attaching device to slot ");
    serial_write_dec(slot_id);
    serial_write_str(", port ");
    serial_write_dec(port_num);
    serial_write_str("\r\n");

    usb_device_t *dev = (usb_device_t *)kmalloc(sizeof(usb_device_t));
    if (!dev) {
        serial_write_str("USB: Failed to allocate memory for new device\r\n");
        return;
    }

    memset(dev, 0, sizeof(usb_device_t));
    dev->slot_id = slot_id;
    dev->port_num = port_num;
    dev->speed = speed;
    
    // Add to list
    dev->next = device_list;
    device_list = dev;

    if (connect_callback) {
        connect_callback(dev);
    }
}

void usb_detach_device(uint8_t slot_id) {
    serial_write_str("USB: Detaching device from slot ");
    serial_write_dec(slot_id);
    serial_write_str("\r\n");

    usb_device_t **curr = &device_list;
    while (*curr) {
        if ((*curr)->slot_id == slot_id) {
            usb_device_t *to_remove = *curr;
            *curr = to_remove->next;
            
            if (disconnect_callback) {
                disconnect_callback(to_remove);
            }

            kfree(to_remove);
            serial_write_str("USB: Device detached successfully\r\n");
            return;
        }
        curr = &((*curr)->next);
    }
    serial_write_str("USB: Warning: Device with slot ");
    serial_write_dec(slot_id);
    serial_write_str(" not found in device list\r\n");
}

usb_device_t* usb_find_device_by_slot(uint8_t slot_id) {
    usb_device_t *curr = device_list;
    while (curr) {
        if (curr->slot_id == slot_id) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

usb_device_t* usb_get_device_list() {
    return device_list;
}

