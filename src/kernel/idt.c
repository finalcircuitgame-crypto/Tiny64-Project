#include "../include/idt.h"
#include <stdint.h>

__attribute__((aligned(0x10)))
static struct IDTEntry idt[256];
static struct IDTR idtr;

extern void* isr_stub_table[];

void SetIDTGate(uint8_t vector, void* handler, uint8_t type_attr) {
    uint64_t offset = (uint64_t)handler;
    idt[vector].Offset0 = (uint16_t)offset;
    idt[vector].Selector = 0x08; // Kernel Code Segment
    idt[vector].IST = 0;
    idt[vector].TypeAttributes = type_attr;
    idt[vector].Offset1 = (uint16_t)(offset >> 16);
    idt[vector].Offset2 = (uint32_t)(offset >> 32);
    idt[vector].Reserved = 0;
}

// Simple handler for now
void exception_handler() {
    // We'll improve this later to print which exception happened
    __asm__ volatile ("cli; hlt");
}

void SetupIDT() {
    idtr.Limit = sizeof(idt) - 1;
    idtr.Offset = (uint64_t)&idt;

    // Fill all with a default handler for now
    for (int i = 0; i < 32; i++) {
        SetIDTGate(i, exception_handler, 0x8E); // Interrupt Gate, Present, Ring 0
    }

    __asm__ volatile ("lidt %0" : : "m"(idtr));
}
