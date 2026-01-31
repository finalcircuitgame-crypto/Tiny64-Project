#include "gdt.h"

struct GDTEntry gdt[5];
struct GDTDescriptor gdt_ptr;

void set_gdt_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].access = access;
    gdt[num].granularity = (limit >> 16) & 0x0F;
    gdt[num].granularity |= gran & 0xF0;
    gdt[num].base_high = (base >> 24) & 0xFF;
}

void init_gdt() {
    gdt_ptr.limit = (sizeof(struct GDTEntry) * 5) - 1;
    gdt_ptr.base  = (uint64_t)&gdt;
    
    set_gdt_gate(0, 0, 0, 0, 0);                // Null
    set_gdt_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xA0); // Kernel Code (Ring 0)
    set_gdt_gate(2, 0, 0xFFFFFFFF, 0x92, 0xA0); // Kernel Data (Ring 0)
    set_gdt_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xA0); // User Code (Ring 3)
    set_gdt_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xA0); // User Data (Ring 3)
    
    gdt_flush(&gdt_ptr);
}
