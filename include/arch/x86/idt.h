#ifndef IDT_H
#define IDT_H

#include <stdint.h>

typedef struct {
    uint16_t isr_low;      // Lower 16 bits of ISR's address
    uint16_t kernel_cs;    // Kernel code segment
    uint8_t  ist;          // Interrupt Stack Table offset
    uint8_t  attributes;   // Type and attributes
    uint16_t isr_mid;      // Middle 16 bits of ISR's address
    uint32_t isr_high;     // Higher 32 bits of ISR's address
    uint32_t reserved;     // Set to zero
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idtr_t;

void idt_init(void);
void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags);

#endif
