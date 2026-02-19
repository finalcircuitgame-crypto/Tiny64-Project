#include "usb.h"
#include "xhci/xhci.h"
#include "../hid/input.h"
#include "../../memory/heap.h"
#include "../../lib/string.h"
#include "../platform/serial.h"

// HID Keymaps
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

extern xhci_controller_t xhc;

// We need to track the last report for each slot to detect changes
static uint8_t last_reports[16][8];
static int kbd_initialized[16];

void usb_hid_init_device(usb_device_t* dev) {
    if (dev->slot_id >= 16) return;
    
    // Check if it's a keyboard (simplified: we'd normally check interface class)
    // For now, let's assume if it has a kbd_ring, it's a keyboard
    if (xhc.slots[dev->slot_id].ring1_ptr) {
        serial_write_str("USB HID: Initializing keyboard on slot ");
        serial_write_dec(dev->slot_id);
        serial_write_str("\r\n");
        
        memset(last_reports[dev->slot_id], 0, 8);
        
        // Enqueue first transfer
        xhci_slot_data_t *slot = &xhc.slots[dev->slot_id];
        uint32_t idx = slot->ring1_enqueue;
        xhci_trb_t *trb = &((xhci_trb_t *)(uintptr_t)slot->ring1_ptr)[idx];
        
        // Use a static buffer for reports (one per slot)
        static uint8_t report_bufs[16][8];
        
        trb->parameter = (uintptr_t)report_bufs[dev->slot_id];
        trb->status = 8;
        trb->control = (1 << 10) | (1 << 5) | (slot->ring1_cycle ? 1 : 0);
        
        slot->ring1_enqueue = (idx + 1) % 64;
        if (slot->ring1_enqueue == 63) {
            // Link TRB handled by xhci.c init, but we should be careful
            slot->ring1_enqueue = 0;
            slot->ring1_cycle = !slot->ring1_cycle;
        }
        
        // Ring doorbell
        xhc.db_regs[dev->slot_id].target = slot->dci_in;
        
        kbd_initialized[dev->slot_id] = 1;
        usb_hid_detected = 1;
    }
}

void usb_hid_poll() {
    for (int sid = 1; sid <= xhc.max_slots; sid++) {
        if (!kbd_initialized[sid]) continue;
        
        xhci_slot_data_t *slot = &xhc.slots[sid];
        if (!slot->ring1_ptr) continue;
        
    }
}
