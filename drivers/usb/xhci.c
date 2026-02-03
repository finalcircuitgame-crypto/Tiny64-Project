#include <drivers/usb/xhci.h>
#include <kernel/pci.h>
#include <kernel/printk.h>
#include <kernel/mm/pmm.h>
#include <kernel/mm/heap.h>
#include <kernel/serial.h>
#include <lib/string.h>

static uintptr_t xhci_base;
static uintptr_t op_base;
static uintptr_t db_base;
static uintptr_t runtime_base;

static struct xhci_ring cmd_ring;
static struct xhci_ring event_ring;
static uint64_t* dcbaap;
static struct xhci_erst_entry* erst;

// Keyboard State
static struct xhci_ring* kbd_ring = NULL;
static uint32_t kbd_slot = 0;
static uint32_t kbd_ep_idx = 0;
static uint8_t kbd_report[8];

static const char hid_map[] = {
    0, 0, 0, 0, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p',
    'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
    '\n', 27, 8, '\t', ' ', '-', '=', '[', ']', '\\', 0, ';', '\'', '`', ',', '.', '/', 0
};

// --- Register Accessors ---

static inline uint32_t xhci_read_cap(uint32_t reg) {
    return *(volatile uint32_t*)(xhci_base + reg);
}

static inline uint32_t xhci_read_op(uint32_t reg) {
    return *(volatile uint32_t*)(op_base + reg);
}

static inline void xhci_write_op(uint32_t reg, uint32_t val) {
    *(volatile uint32_t*)(op_base + reg) = val;
}

static inline void xhci_write_runtime(uint32_t reg, uint32_t val) {
    *(volatile uint32_t*)(runtime_base + reg) = val;
}

static inline void xhci_ring_doorbell(uint32_t slot, uint32_t target) {
    *(volatile uint32_t*)(db_base + slot * 4) = target;
}

// --- Helper Macros ---
#define TRB_GET_SLOT(c) (((c) >> 24) & 0xFF)
#define TRB_GET_COMPL(s) (((s) >> 24) & 0xFF)

// --- Internal Functions ---

static void ring_init(struct xhci_ring* ring, uint32_t size) {
    ring->size = size;
    ring->trbs = (struct xhci_trb*)pmm_alloc_page();
    memset(ring->trbs, 0, 4096);
    ring->enqueue_idx = 0;
    ring->cycle_bit = 1;

    struct xhci_trb* link = &ring->trbs[size - 1];
    link->param = (uintptr_t)ring->trbs;
    link->control = TRB_TYPE(TRB_LINK) | (1 << 1); // TC
}

void xhci_send_command(uint64_t param, uint32_t status, uint32_t control) {
    struct xhci_trb* trb = &cmd_ring.trbs[cmd_ring.enqueue_idx];
    trb->param = param;
    trb->status = status;
    trb->control = control | cmd_ring.cycle_bit;

    cmd_ring.enqueue_idx++;
    if (cmd_ring.enqueue_idx == cmd_ring.size - 1) {
        struct xhci_trb* link = &cmd_ring.trbs[cmd_ring.enqueue_idx];
        link->control = TRB_TYPE(TRB_LINK) | (1 << 1) | cmd_ring.cycle_bit;
        cmd_ring.enqueue_idx = 0;
        cmd_ring.cycle_bit ^= 1;
    }

    xhci_ring_doorbell(0, 0); // Command Doorbell
}

static struct xhci_trb* xhci_poll_event(uint8_t type) {
    uint32_t timeout = 1000000;
    while (timeout--) {
        struct xhci_trb* ev = &event_ring.trbs[event_ring.enqueue_idx];
        if ((ev->control & TRB_CYCLE) == event_ring.cycle_bit) {
            if (TRB_TYPE_GET(ev->control) == type) {
                return ev;
            }
            // Advance event ring (simplistic)
            event_ring.enqueue_idx++;
            if (event_ring.enqueue_idx == event_ring.size) {
                event_ring.enqueue_idx = 0;
                event_ring.cycle_bit ^= 1;
            }
            // Update ERDP
            xhci_write_runtime(XHCI_ERDP + 0x20, (uint32_t)(uintptr_t)&event_ring.trbs[event_ring.enqueue_idx] | 0x8);
        }
    }
    return NULL;
}

static void xhci_transfer(struct xhci_ring* ring, uint64_t param, uint32_t status, uint32_t control) {
    struct xhci_trb* trb = &ring->trbs[ring->enqueue_idx];
    trb->param = param;
    trb->status = status;
    trb->control = control | ring->cycle_bit;

    ring->enqueue_idx++;
    if (ring->enqueue_idx == ring->size - 1) {
        struct xhci_trb* link = &ring->trbs[ring->enqueue_idx];
        link->control = TRB_TYPE(TRB_LINK) | (1 << 1) | ring->cycle_bit;
        ring->enqueue_idx = 0;
        ring->cycle_bit ^= 1;
    }
}

static void xhci_control_transfer(uint32_t slot_id, struct xhci_ring* ring, void* setup, void* data, uint32_t data_len) {
    // 1. Setup Stage
    xhci_transfer(ring, *(uint64_t*)setup, 8, TRB_TYPE(TRB_SETUP_STAGE) | TRB_IDT | (data_len ? TRB_TRT_IN : TRB_TRT_OUT));
    
    // 2. Data Stage (Optional)
    if (data_len) {
        xhci_transfer(ring, (uintptr_t)data, data_len, TRB_TYPE(TRB_DATA_STAGE) | TRB_DIR_IN | TRB_IOC);
    }

    // 3. Status Stage
    xhci_transfer(ring, 0, 0, TRB_TYPE(TRB_STATUS_STAGE) | (data_len ? 0 : TRB_DIR_IN) | TRB_IOC);

    // 4. Doorbell
    xhci_ring_doorbell(slot_id, 1); // EP0
}

void xhci_poll_keyboard() {
    if (!kbd_ring) return;

    struct xhci_trb* ev = &event_ring.trbs[event_ring.enqueue_idx];
    if ((ev->control & TRB_CYCLE) == event_ring.cycle_bit) {
        if (TRB_TYPE_GET(ev->control) == TRB_TRANSFER_EV) {
            uint32_t slot_id = TRB_GET_SLOT(ev->control);
            uint32_t ep_id = (ev->control >> 16) & 0x1F;
            
            if (slot_id == kbd_slot && ep_id == kbd_ep_idx + 1) {
                for (int i = 2; i < 8; i++) {
                    uint8_t key = kbd_report[i];
                    if (key > 0 && key < sizeof(hid_map)) {
                        char c = hid_map[key];
                        if (c) printk("%c", c);
                        // Optional: serial_putc(c);
                    }
                }
                memset(kbd_report, 0, 8);
                xhci_transfer(kbd_ring, (uintptr_t)kbd_report, 8, TRB_TYPE(TRB_NORMAL) | TRB_IOC | (1 << 16));
                xhci_ring_doorbell(kbd_slot, kbd_ep_idx + 1);
            }
        }

        event_ring.enqueue_idx++;
        if (event_ring.enqueue_idx == event_ring.size) {
            event_ring.enqueue_idx = 0;
            event_ring.cycle_bit ^= 1;
        }
        xhci_write_runtime(XHCI_ERDP + 0x20, (uint32_t)(uintptr_t)&event_ring.trbs[event_ring.enqueue_idx] | 0x8);
    }
}

static void xhci_address_device(uint32_t port_id) {
    // ... (Enable Slot as before)
    xhci_send_command(0, 0, TRB_TYPE(TRB_ENABLE_SLOT));
    struct xhci_trb* ev = xhci_poll_event(TRB_COMMAND_COMPL);
    if (!ev || TRB_GET_COMPL(ev->status) != 1) {
        printk("    Enable Slot failed\n");
        return;
    }
    uint32_t slot_id = TRB_GET_SLOT(ev->control);
    printk("    Slot ID: %d enabled\n", slot_id);

    // 2. Allocate contexts
    struct xhci_device_ctx* output_ctx = (struct xhci_device_ctx*)pmm_alloc_page();
    struct xhci_input_ctx* input_ctx = (struct xhci_input_ctx*)pmm_alloc_page();
    memset(output_ctx, 0, 4096);
    memset(input_ctx, 0, 4096);
    dcbaap[slot_id] = (uintptr_t)output_ctx;

    // 3. Setup Input Context
    input_ctx->add_flags = 0x3; // Slot and EP0
    input_ctx->dev.slot.info = (1 << 27) | (port_id << 16); // 1 context, root port
    input_ctx->dev.ep[0].info2 = (3 << 1) | (8 << 16); // Control EP, Max Packet 8
    
    struct xhci_ring* ep0_ring = (struct xhci_ring*)malloc(sizeof(struct xhci_ring));
    ring_init(ep0_ring, 32);
    input_ctx->dev.ep[0].tr_ptr = (uintptr_t)ep0_ring->trbs | 1;

    // 4. Address Device
    xhci_send_command((uintptr_t)input_ctx, 0, TRB_TYPE(TRB_ADDRESS_DEVICE) | (slot_id << 24));
    ev = xhci_poll_event(TRB_COMMAND_COMPL);
    if (!ev || TRB_GET_COMPL(ev->status) != 1) {
        printk("    Address Device failed (Status: %d)\n", TRB_GET_COMPL(ev->status));
        return;
    }
    printk("    Device Adressed!\n");

    // 5. Get Device Descriptor
    struct usb_device_descriptor* dev_desc = (struct usb_device_descriptor*)malloc(sizeof(struct usb_device_descriptor));
    uint8_t setup_dev[8] = { 0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 18, 0x00 };
    xhci_control_transfer(slot_id, ep0_ring, setup_dev, dev_desc, 18);
    
    ev = xhci_poll_event(TRB_TRANSFER_EV);
    if (ev && TRB_GET_COMPL(ev->status) == 1) {
        printk("    USB Device: ID %x:%x Class:%d\n", dev_desc->vendor_id, dev_desc->product_id, dev_desc->device_class);
    } else {
        printk("    Failed to get device descriptor\n");
        return;
    }

    // 6. Get Config Descriptor (Header)
    struct usb_config_descriptor* conf_desc_base = (struct usb_config_descriptor*)malloc(sizeof(struct usb_config_descriptor));
    uint8_t setup_conf[8] = { 0x80, 0x06, 0x00, 0x02, 0x00, 0x00, 9, 0x00 };
    xhci_control_transfer(slot_id, ep0_ring, setup_conf, conf_desc_base, 9);
    ev = xhci_poll_event(TRB_TRANSFER_EV);
    if (!ev || TRB_GET_COMPL(ev->status) != 1) {
        printk("    Failed to get config descriptor header\n");
        return;
    }

    // 7. Get Full Config Descriptor
    uint16_t total_len = conf_desc_base->total_len;
    uint8_t* conf_full = (uint8_t*)malloc(total_len);
    setup_conf[6] = (uint8_t)(total_len & 0xFF);
    setup_conf[7] = (uint8_t)(total_len >> 8);
    xhci_control_transfer(slot_id, ep0_ring, setup_conf, conf_full, total_len);
    ev = xhci_poll_event(TRB_TRANSFER_EV);
    if (!ev || TRB_GET_COMPL(ev->status) != 1) {
        printk("    Failed to get full config descriptor\n");
        return;
    }

    // 8. Parse Descriptor for Keyboard
    uint8_t* ptr = conf_full;
    while (ptr < conf_full + total_len) {
        uint8_t len = ptr[0];
        uint8_t type = ptr[1];
        if (type == 4) { // INTERFACE
            struct usb_interface_descriptor* iface = (struct usb_interface_descriptor*)ptr;
            printk("    Interface: Class %d Sub %d Prot %d\n", iface->interface_class, iface->interface_subclass, iface->interface_protocol);
            if (iface->interface_class == 3 && iface->interface_subclass == 1 && iface->interface_protocol == 1) {
                printk("    !!! USB KEYBOARD DETECTED !!!\n");
                
                // 9. Find Endpoint Descriptor
                uint8_t* ep_ptr = ptr + len;
                struct usb_endpoint_descriptor* ep_desc = NULL;
                while (ep_ptr < conf_full + total_len) {
                    if (ep_ptr[1] == 4) break; // Next interface
                    if (ep_ptr[1] == 5) { // ENDPOINT
                        ep_desc = (struct usb_endpoint_descriptor*)ep_ptr;
                        if (ep_desc->endpoint_address & 0x80) break; // IN endpoint
                    }
                    ep_ptr += ep_ptr[0];
                }

                if (ep_desc) {
                    printk("    Found Interrupt IN EP: %x\n", ep_desc->endpoint_address);
                    
                    // 10. Set Configuration
                    uint8_t setup_setcfg[8] = { 0x00, 0x09, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 }; // SET_CONFIGURATION 1
                    xhci_control_transfer(slot_id, ep0_ring, setup_setcfg, NULL, 0);
                    ev = xhci_poll_event(TRB_TRANSFER_EV);

                    // 11. Configure Endpoint in xHCI
                    uint8_t ep_idx = (ep_desc->endpoint_address & 0xF) * 2 + ((ep_desc->endpoint_address & 0x80) ? 1 : 0) - 1;
                    printk("    xHCI EP Index: %d\n", ep_idx);

                    struct xhci_input_ctx* config_ctx = (struct xhci_input_ctx*)pmm_alloc_page();
                    memset(config_ctx, 0, 4096);
                    config_ctx->add_flags = (1 << (ep_idx + 1)) | (1 << 0); // Add EP and Slot
                    
                    // Copy Slot Context from Output
                    memcpy(&config_ctx->dev.slot, &output_ctx->slot, sizeof(struct xhci_slot_ctx));
                    config_ctx->dev.slot.info = (output_ctx->slot.info & ~(0x1F << 27)) | (31 << 27); // Max Contexts

                    // Setup EP Context
                    config_ctx->dev.ep[ep_idx].info2 = (3 << 1) | (ep_desc->max_packet_size << 16); // Interrupt EP
                    struct xhci_ring* ep_ring = (struct xhci_ring*)malloc(sizeof(struct xhci_ring));
                    ring_init(ep_ring, 32);
                    config_ctx->dev.ep[ep_idx].tr_ptr = (uintptr_t)ep_ring->trbs | 1;
                    config_ctx->dev.ep[ep_idx].average_trb_len = 8;

                    xhci_send_command((uintptr_t)config_ctx, 0, TRB_TYPE(TRB_CONFIGURE_ENDPOINT) | (slot_id << 24));
                    ev = xhci_poll_event(TRB_COMMAND_COMPL);
                    if (ev && TRB_GET_COMPL(ev->status) == 1) {
                        printk("    Endpoint Configured!\n");
                        
                        // Save for polling
                        kbd_ring = ep_ring;
                        kbd_slot = slot_id;
                        kbd_ep_idx = ep_idx;
                        
                        // Submit first request
                        xhci_transfer(kbd_ring, (uintptr_t)kbd_report, 8, TRB_TYPE(TRB_NORMAL) | TRB_IOC | (1 << 16)); // ISP
                        xhci_ring_doorbell(slot_id, ep_idx + 1);
                    } else {
                        printk("    Endpoint Config Failed\n");
                    }
                }
            }
        }
        ptr += len;
        if (len == 0) break;
    }
}

// --- Main Init ---

void xhci_init(uint8_t bus, uint8_t slot, uint8_t func) {
    printk("Initializing xHCI...\n");

    uint32_t pci_cmd = pci_config_read32(bus, slot, func, 0x04);
    pci_cmd |= (1 << 1) | (1 << 2);
    pci_config_write32(bus, slot, func, 0x04, pci_cmd);

    uint32_t bar0 = pci_config_read32(bus, slot, func, 0x10);
    uint32_t bar1 = pci_config_read32(bus, slot, func, 0x14);
    xhci_base = (bar0 & ~0xF) | ((uint64_t)bar1 << 32);

    uint8_t cap_len = *(volatile uint8_t*)(xhci_base + XHCI_CAPLENGTH);
    op_base = xhci_base + cap_len;
    db_base = xhci_base + xhci_read_cap(XHCI_DBOFF);
    runtime_base = xhci_base + xhci_read_cap(XHCI_RTSOFF);

    // Reset
    xhci_write_op(XHCI_USBCMD, USBCMD_RESET);
    while (xhci_read_op(XHCI_USBCMD) & USBCMD_RESET);
    while (xhci_read_op(XHCI_USBSTS) & USBSTS_CNR);

    // Config Max Slots
    uint32_t hcsparams1 = xhci_read_cap(XHCI_HCSPARAMS1);
    uint32_t max_slots = XHCI_MAX_SLOTS(hcsparams1);
    xhci_write_op(XHCI_CONFIG, max_slots);

    // DCBAAP
    dcbaap = (uint64_t*)pmm_alloc_page();
    memset(dcbaap, 0, 4096);
    xhci_write_op(XHCI_DCBAAP + 0, (uint32_t)(uintptr_t)dcbaap);
    xhci_write_op(XHCI_DCBAAP + 4, (uint32_t)((uintptr_t)dcbaap >> 32));

    // Command Ring
    ring_init(&cmd_ring, 64);
    xhci_write_op(XHCI_CRCR + 0, (uint32_t)(uintptr_t)cmd_ring.trbs | 1);
    xhci_write_op(XHCI_CRCR + 4, (uint32_t)((uintptr_t)cmd_ring.trbs >> 32));

    // Event Ring
    ring_init(&event_ring, 64);
    erst = (struct xhci_erst_entry*)pmm_alloc_page();
    memset(erst, 0, 4096);
    erst[0].ba = (uintptr_t)event_ring.trbs;
    erst[0].size = 64;

    // Interrupter 0
    xhci_write_runtime(XHCI_ERSTSZ + 0x20, 1);
    xhci_write_runtime(XHCI_ERDP + 0x20, (uint32_t)(uintptr_t)event_ring.trbs | 0x8);
    xhci_write_runtime(XHCI_ERDP + 4 + 0x20, (uint32_t)((uintptr_t)event_ring.trbs >> 32));
    xhci_write_runtime(XHCI_ERSTBA + 0x20, (uint32_t)(uintptr_t)erst);
    xhci_write_runtime(XHCI_ERSTBA + 4 + 0x20, (uint32_t)((uintptr_t)erst >> 32));
    xhci_write_runtime(XHCI_IMAN + 0x20, 0x3);

    // Start
    serial_write("xHCI: Starting controller...\n");
    xhci_write_op(XHCI_USBCMD, USBCMD_RUN | USBCMD_INTE);
    while (xhci_read_op(XHCI_USBSTS) & USBSTS_HCH);
    serial_write("xHCI: Running.\n");
    printk("  xHCI Running and Interrupter 0 Active.\n");

    // Port Enumeration
    uint32_t max_ports = XHCI_MAX_PORTS(hcsparams1);
    printk("  Enumerating %d ports...\n", max_ports);

    for (uint32_t i = 1; i <= max_ports; i++) {
        uint32_t port_reg = XHCI_PORTSC_BASE + (0x10 * (i - 1));
        uint32_t portsc = xhci_read_op(port_reg);

        if (portsc & PORTSC_CCS) {
            printk("    Port %d: Device connected. Resetting...\n", i);
            
            // Trigger Port Reset
            portsc |= PORTSC_PR;
            xhci_write_op(port_reg, portsc);

            // Wait for Reset to complete
            while (xhci_read_op(port_reg) & PORTSC_PR);
            
            printk("    Port %d: Reset complete.\n", i);
            xhci_address_device(i);
        }
    }
}
