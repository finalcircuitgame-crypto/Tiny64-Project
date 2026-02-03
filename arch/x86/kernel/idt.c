#include <arch/x86/idt.h>
#include <kernel/printk.h>

static idt_entry_t idt[256];
static idtr_t idtr;

extern void* isr_stub_table[];

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) {
    idt_entry_t* descriptor = &idt[vector];
    uint64_t addr = (uint64_t)isr;

    descriptor->isr_low    = addr & 0xFFFF;
    descriptor->kernel_cs  = 0x08; // Assuming 0x08 is the kernel code segment
    descriptor->ist        = 0;
    descriptor->attributes = flags;
    descriptor->isr_mid    = (addr >> 16) & 0xFFFF;
    descriptor->isr_high   = (addr >> 32) & 0xFFFFFFFF;
    descriptor->reserved   = 0;
}

void idt_init(void) {
    idtr.base = (uint64_t)&idt;
    idtr.limit = sizeof(idt) - 1;

    for (int i = 0; i < 256; i++) {
        idt_set_descriptor(i, isr_stub_table[i], 0x8E);
    }

    __asm__ volatile ("lidt %0" : : "m"(idtr));
    printk("IDT initialized.\n");
}

void isr_handler(uint64_t vector) {
    printk("Received interrupt: %d\n", vector);
}
