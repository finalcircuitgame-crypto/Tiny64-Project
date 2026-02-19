#include "idt.h"
#include "../drivers/platform/serial.h"

static idt_entry_t idt[256];
static idtr_t idtr;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

void set_idt_gate(int n, uint64_t handler) {
    idt[n].isr_low = handler & 0xFFFF;
    idt[n].kernel_cs = 0x08; // Kernel Code Segment in GDT
    idt[n].ist = 0;
    idt[n].attributes = 0x8E; // 64-bit Interrupt Gate: P=1, DPL=00, Type=1110
    idt[n].isr_mid = (handler >> 16) & 0xFFFF;
    idt[n].isr_high = (handler >> 32) & 0xFFFFFFFF;
    idt[n].reserved = 0;
}

void init_pic() {
    serial_write_str("[PIC] Remapping...\r\n");
    // ICW1: Start initialization in cascade mode
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    // ICW2: Vector offsets (Master: 0x20, Slave: 0x28)
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    // ICW3: Cascading
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    // ICW4: 8086 mode
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    
    // Mask all but Timer and PS/2 (IRQ 0, IRQ 1, IRQ 12)
    // IRQ 0: Timer, IRQ 1: Keyboard, IRQ 12: Mouse (Slave IRQ 4)
    // Slave needs IRQ 2 enabled on Master.
#if !DISABLE_PS2
    uint8_t master_mask = 0xF8; // 11111000 (IRQ 0, 1, 2 enabled)
    uint8_t slave_mask = 0xEF;  // 11101111 (IRQ 12 enabled)
#else
    uint8_t master_mask = 0xFE; // Only Timer enabled
    uint8_t slave_mask = 0xFF;
#endif
    
    outb(0x21, master_mask);
    outb(0xA1, slave_mask);
    
#if !DISABLE_PS2
    serial_write_str("[PIC] Master Mask: 0xF8 (IRQ0/Timer, IRQ1/KB Enabled)\r\n");
    serial_write_str("[PIC] Slave Mask: 0xEF (IRQ12/Mouse Enabled)\r\n");
#else
    serial_write_str("[PIC] Master Mask: 0xFE (IRQ0/Timer Enabled)\r\n");
    serial_write_str("[PIC] Slave Mask: 0xFF (All Disabled)\r\n");
#endif
}

extern void isr32();
extern void isr33();
extern void isr44();

extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();

void init_idt() {
    serial_write_str("[IDT] Initializing...\r\n");
    idtr.base = (uint64_t)&idt;
    idtr.limit = sizeof(idt) - 1;

    for (int i = 0; i < 256; i++) {
        // Initialize all gates to zero/ignored if needed, but for now we just set what we need
    }

    // CPU exceptions (0-31)
    set_idt_gate(0,  (uint64_t)isr0);
    set_idt_gate(1,  (uint64_t)isr1);
    set_idt_gate(2,  (uint64_t)isr2);
    set_idt_gate(3,  (uint64_t)isr3);
    set_idt_gate(4,  (uint64_t)isr4);
    set_idt_gate(5,  (uint64_t)isr5);
    set_idt_gate(6,  (uint64_t)isr6);
    set_idt_gate(7,  (uint64_t)isr7);
    set_idt_gate(8,  (uint64_t)isr8);
    set_idt_gate(9,  (uint64_t)isr9);
    set_idt_gate(10, (uint64_t)isr10);
    set_idt_gate(11, (uint64_t)isr11);
    set_idt_gate(12, (uint64_t)isr12);
    set_idt_gate(13, (uint64_t)isr13);
    set_idt_gate(14, (uint64_t)isr14);
    set_idt_gate(15, (uint64_t)isr15);
    set_idt_gate(16, (uint64_t)isr16);
    set_idt_gate(17, (uint64_t)isr17);
    set_idt_gate(18, (uint64_t)isr18);
    set_idt_gate(19, (uint64_t)isr19);
    set_idt_gate(20, (uint64_t)isr20);
    set_idt_gate(21, (uint64_t)isr21);
    set_idt_gate(22, (uint64_t)isr22);
    set_idt_gate(23, (uint64_t)isr23);
    set_idt_gate(24, (uint64_t)isr24);
    set_idt_gate(25, (uint64_t)isr25);
    set_idt_gate(26, (uint64_t)isr26);
    set_idt_gate(27, (uint64_t)isr27);
    set_idt_gate(28, (uint64_t)isr28);
    set_idt_gate(29, (uint64_t)isr29);
    set_idt_gate(30, (uint64_t)isr30);
    set_idt_gate(31, (uint64_t)isr31);

    // Set timer tick handler (IRQ0, vector 32)
    set_idt_gate(32, (uint64_t)isr32);
#if !DISABLE_PS2
    set_idt_gate(33, (uint64_t)isr33);
    set_idt_gate(44, (uint64_t)isr44);
    serial_write_str("[IDT] Gate 33 (KB) Set.\r\n");
    serial_write_str("[IDT] Gate 44 (Mouse) Set.\r\n");
#endif

    init_pic();

    __asm__ volatile ("lidt %0" : : "m"(idtr));
    serial_write_str("[IDT] LIDT Loaded.\r\n");
}
