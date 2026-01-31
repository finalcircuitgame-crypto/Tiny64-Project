#include "mouse.h"
#include "controller.h"
#include "../input.h"
#include "../../platform/serial.h"
#include "../../../core/idt.h"

extern void isr44(); // Defined in interrupts.S

static uint8_t mouse_cycle = 0;
static int8_t mouse_packet[3]; // Signed packet bytes

void mouse_handler(void* frame) {
    uint8_t data = ps2_inb(PS2_DATA_PORT);
    
    switch(mouse_cycle) {
        case 0:
            if ((data & 0x08) == 0x08) { // Bit 3 must be 1
                mouse_packet[0] = data;
                mouse_cycle++;
            }
            break;
        case 1:
            mouse_packet[1] = data;
            mouse_cycle++;
            break;
        case 2:
            mouse_packet[2] = data;
            mouse_cycle = 0;
            
            // Process Packet
            int dx = mouse_packet[1];
            int dy = mouse_packet[2];
            uint8_t btns = mouse_packet[0] & 0x7;
            
            // PS/2 Y is separate from screen Y logic usually, but here we just pass delta.
            // Typical PS/2: Y increases UP. Screen: Y increases DOWN.
            // So we might need to invert DY here or in desktop. 
            // Standard PS/2: +Y is Up. 
            // input_mouse_update expects generic deltas. Let's send raw and let desktop decide,
            // OR invert here to match typical "Screen Space" (+Y Down).
            // Let's invert here to be "Screen Space" friendly: +Y = Down.
            
            input_mouse_update(dx, -dy, btns);
            break;
    }
    
    // Send EOI to Slave (0xA0) and Master (0x20) PICs
    ps2_outb(0xA0, 0x20);
    ps2_outb(0x20, 0x20);
}

void ps2_mouse_write(uint8_t byte) {
    ps2_controller_wait_write();
    ps2_outb(PS2_CMD_PORT, 0xD4); // Write to Aux Device
    ps2_controller_wait_write();
    ps2_outb(PS2_DATA_PORT, byte);
}

void ps2_mouse_init() {
    serial_write_str("[PS2] Initializing Mouse...\r\n");
    
    set_idt_gate(44, (uint64_t)isr44); // IRQ 12 = 32 + 12 = 44
    
    // Enable Defaults
    ps2_mouse_write(0xF6);
    ps2_controller_wait_read();
    ps2_inb(PS2_DATA_PORT); // ACK
    
    // Enable Streaming
    ps2_mouse_write(0xF4);
    ps2_controller_wait_read();
    ps2_inb(PS2_DATA_PORT); // ACK
    
    serial_write_str("[PS2] Mouse Initialized.\r\n");
}
