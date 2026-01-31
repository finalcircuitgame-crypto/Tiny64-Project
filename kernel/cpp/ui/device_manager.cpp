#include "device_manager.h"
#include "../../c/drivers/pci/pci.h"
#include "../../c/drivers/usb/usb.h"
#include "../../c/lib/string.h"
#include "../../c/graphics/colors.h"

static const char* get_pci_vendor_name(uint16_t vendor_id) {
    switch (vendor_id) {
        case 0x8086: return "Intel Corp.";
        case 0x10DE: return "NVIDIA Corp.";
        case 0x1002: return "AMD ATI";
        case 0x1234: return "QEMU Virtual";
        case 0x15AD: return "VMware";
        case 0x1AF4: return "VirtIO / Red Hat";
        case 0x106B: return "Apple Inc.";
        case 0x1414: return "Microsoft";
        case 0x1B36: return "QEMU / Red Hat";
        case 0x80EE: return "VirtualBox";
        default: return "Unknown Vendor";
    }
}

static const char* get_pci_class_name(uint8_t class_code) {
    switch (class_code) {
        case 0x00: return "Legacy";
        case 0x01: return "Mass Storage";
        case 0x02: return "Network";
        case 0x03: return "Display";
        case 0x04: return "Multimedia";
        case 0x05: return "Memory";
        case 0x06: return "Bridge";
        case 0x07: return "Comm.";
        case 0x08: return "System";
        case 0x09: return "Input";
        case 0x0A: return "Docking";
        case 0x0B: return "Processor";
        case 0x0C: return "Serial Bus";
        case 0x0D: return "Wireless";
        case 0x0E: return "Intelligent IO";
        case 0x0F: return "Satellite";
        case 0x10: return "Encryption";
        case 0x11: return "Signal Proc";
        default: return "Misc";
    }
}

static const char* get_usb_vendor_name(uint16_t vendor_id) {
    switch (vendor_id) {
        case 0x8086: return "Intel";
        case 0x046D: return "Logitech";
        case 0x045E: return "Microsoft";
        case 0x05AC: return "Apple";
        case 0x17EF: return "Lenovo";
        case 0x0951: return "Kingston";
        case 0x0BDA: return "Realtek";
        default: return "Generic";
    }
}

DeviceManager::DeviceManager(int x, int y, int w, int h)
    : Window("Device Manager", x, y, w, h) {}

void DeviceManager::Draw(Renderer& r) {
    if (closed || minimized) return;
    Window::Draw(r);

    int rowY = y + 40;
    
    // PCI Section
    r.draw_string("PCI HARDWARE", x + 10, rowY, 0xFF3498DB, 1);
    rowY += 20;
    r.fill_rect(x + 10, rowY, w - 20, 1, 0xFFCCCCCC);
    rowY += 10;

    for (int i = 0; i < g_pci_device_count; i++) {
        if (rowY + 20 > y + h / 2 + 40) break;
        
        char buf[128];
        const char* vname = get_pci_vendor_name(g_pci_devices[i].vendor_id);
        const char* cname = get_pci_class_name(g_pci_devices[i].class_code);

        ksprintf(buf, "%s %s (%04x:%04x)", vname, cname, 
                g_pci_devices[i].vendor_id, g_pci_devices[i].device_id);
        
        r.draw_string(buf, x + 20, rowY, COLOR_BLACK, 1);
        rowY += 18;
    }

    // USB Section
    rowY = y + h / 2 + 20;
    r.draw_string("USB PERIPHERALS", x + 10, rowY, 0xFF27AE60, 1);
    rowY += 20;
    r.fill_rect(x + 10, rowY, w - 20, 1, 0xFFCCCCCC);
    rowY += 10;

    usb_device_t* usb_dev = usb_get_device_list();
    while (usb_dev) {
        if (rowY + 20 > y + h - 10) break;

        char buf[128];
        const char* vname = get_usb_vendor_name(usb_dev->desc.id_vendor);
        const char* type_str = "Generic Device";
        if (usb_dev->desc.device_class == 0x03) type_str = "Keyboard/Mouse";
        else if (usb_dev->desc.device_class == 0x08) type_str = "Mass Storage";
        else if (usb_dev->desc.device_class == 0x09) type_str = "USB Hub";

        ksprintf(buf, "%s %s (Slot %d)", vname, type_str, usb_dev->slot_id);
        
        r.draw_string(buf, x + 20, rowY, COLOR_BLACK, 1);
        rowY += 18;
        usb_dev = usb_dev->next;
    }
}
