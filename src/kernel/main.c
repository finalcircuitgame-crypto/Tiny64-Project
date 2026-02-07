#include "../include/bootinfo.h"
#include "../include/gdt.h"
#include "../include/idt.h"
#include "../include/pmm.h"
#include "../include/vmm.h"
#include "../include/heap.h"
#include "../include/task.h"
#include <stddef.h>

// Defined in other files
void ConsoleInit(BootInfo *bootInfo);
void PrintString(const char *str, uint32_t color);
void SetupGDT();
void SetupIDT();
void PMM_FreePages(void* addr, uint64_t count);

// Serial Port output
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

void serial_print(const char *str) {
    while (*str) {
        outb(0x3F8, *str++);
    }
}

void taskA() {
    while(1) {
        PrintString(" [A] ", 0x00FFFF);
        serial_print("A");
        for(int i=0; i<1000000; i++) __asm__ volatile("nop");
        Task_Yield();
    }
}

void taskB() {
    while(1) {
        PrintString(" [B] ", 0xFFFF00);
        serial_print("B");
        for(int i=0; i<1000000; i++) __asm__ volatile("nop");
        Task_Yield();
    }
}

void kernel_main(BootInfo *bootInfo) {
    ConsoleInit(bootInfo);

    uint32_t *fb = bootInfo->framebuffer;
    for (uint32_t i = 0; i < bootInfo->width * bootInfo->height; i++) {
        fb[i] = 0x001122; 
    }

    PrintString("Tiny64 Kernel Loaded!\n", 0xFFFFFF);
    SetupGDT();
    SetupIDT();

    // PMM
    void* bitmap_addr = (void*)bootInfo->LargestFreeRegion.Base;
    uint64_t mem_size = bootInfo->LargestFreeRegion.Size;
    PMM_Init(mem_size, bitmap_addr);
    PMM_FreePages((void*)(bootInfo->LargestFreeRegion.Base + 4096), (mem_size / 4096) - 1);
    PrintString("PMM Initialized.\n", 0x00FF00);

    // VMM
    VMM_Init();
    for (uint64_t i = 0; i < 0x8000000; i += 4096) VMM_MapPage((void*)i, (void*)i, PAGE_WRITE);
    uint64_t fb_base = (uint64_t)bootInfo->framebuffer;