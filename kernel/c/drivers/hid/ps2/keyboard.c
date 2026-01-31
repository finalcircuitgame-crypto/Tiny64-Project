#include "keyboard.h"
#include "controller.h"
#include "../input.h"
#include "../../platform/serial.h"
#include "../../../core/idt.h"

extern void isr33(); // Defined in interrupts.S

// Scan Code Set 1 (Standard Translation)
const char scancode_map[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

const char scancode_map_shift[] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '
};

static int g_shift_pressed = 0;
static int g_caps_lock = 0;
static int g_ctrl_pressed = 0;

// Interrupt Handler for IRQ 1
void keyboard_handler(void* frame) {
    uint8_t scancode = ps2_inb(PS2_DATA_PORT);
    
    if (scancode == 0x2A || scancode == 0x36) {
        g_shift_pressed = 1;
    } else if (scancode == 0xAA || scancode == 0xB6) {
        g_shift_pressed = 0;
    } else if (scancode == 0x1D) {
        g_ctrl_pressed = 1;
    } else if (scancode == 0x9D) {
        g_ctrl_pressed = 0;
    } else if (scancode == 0x3A) {
        g_caps_lock = !g_caps_lock;
    } else if (scancode < 58) {
        char c = 0;
        int shift = g_shift_pressed;
        
        // Handle Caps Lock for letters
        char base_c = scancode_map[scancode];
        if (g_caps_lock && base_c >= 'a' && base_c <= 'z') {
            shift = !shift;
        }

        if (shift) {
            c = scancode_map_shift[scancode];
        } else {
            c = scancode_map[scancode];
        }

        if (c || g_ctrl_pressed) {
            uint32_t event = (uint8_t)c;
            if (g_ctrl_pressed) event |= KB_MOD_CTRL;
            if (g_shift_pressed) event |= KB_MOD_SHIFT;
            input_keyboard_push(event);
        }
    } else if (scancode >= 0x80 && scancode < 0x80 + 58) {
        // Key Release
        uint8_t released_scancode = scancode - 0x80;
        char c = scancode_map[released_scancode];
        if (c || g_ctrl_pressed) {
            uint32_t event = (uint8_t)c | (1 << 31); // Mark as release
            input_keyboard_push(event);
        }
    }
    
    // Send EOI to Master PIC (IRQ 1 is on Master)
    ps2_outb(0x20, 0x20); 
}

void ps2_keyboard_init() {
    serial_write_str("[PS2] Initializing Keyboard...\r\n");
    
    // Register ISR (Assuming idt_register_handler exists and maps IRQ1 -> INT 33)
    // Tiny64 usually maps IRQs 0-15 to 32-47. IRQ1 = 33.
    set_idt_gate(33, (uint64_t)isr33);
    
    // Reset Device
    ps2_controller_wait_write();
    ps2_outb(PS2_DATA_PORT, 0xFF);
    ps2_controller_wait_read();
    uint8_t ack = ps2_inb(PS2_DATA_PORT); // Should be 0xFA (ACK)
    ps2_controller_wait_read();
    uint8_t res = ps2_inb(PS2_DATA_PORT); // Should be 0xAA (Success)
    
    if (res == 0xAA) {
        serial_write_str("[PS2] Keyboard Detected & Reset.\r\n");
        
        // Set Typematic Rate/Delay (0x00: 250ms delay, 30Hz repeat)
        ps2_controller_wait_write();
        ps2_outb(PS2_DATA_PORT, 0xF3);
        ps2_controller_wait_read();
        ps2_inb(PS2_DATA_PORT); // ACK
        
        ps2_controller_wait_write();
        ps2_outb(PS2_DATA_PORT, 0x00);
        ps2_controller_wait_read();
        ps2_inb(PS2_DATA_PORT); // ACK
    } else {
        serial_write_str("[PS2] Keyboard Reset Failed or No Device.\r\n");
    }
}
