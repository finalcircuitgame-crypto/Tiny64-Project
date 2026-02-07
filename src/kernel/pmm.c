#include "../include/pmm.h"
#include <stddef.h>

#define PAGE_SIZE 4096

static uint8_t* bitmap;
static uint64_t max_pages;

void PMM_Init(uint64_t mem_size, void* bitmap_addr) {
    bitmap = (uint8_t*)bitmap_addr;
    max_pages = mem_size / PAGE_SIZE;

    // Initialize bitmap (mark all as used/reserved initially)
    for (uint64_t i = 0; i < max_pages / 8; i++) {
        bitmap[i] = 0xFF;
    }
}

// Mark a range of pages as free
void PMM_FreePages(void* addr, uint64_t count) {
    uint64_t start_page = (uint64_t)addr / PAGE_SIZE;
    for (uint64_t i = 0; i < count; i++) {
        uint64_t page = start_page + i;
        bitmap[page / 8] &= ~(1 << (page % 8));
    }
}

void* PMM_AllocatePage() {
    for (uint64_t i = 0; i < max_pages; i++) {
        if (!(bitmap[i / 8] & (1 << (i % 8)))) {
            bitmap[i / 8] |= (1 << (i % 8)); // Mark as used
            return (void*)(i * PAGE_SIZE);
        }
    }
    return NULL; // Out of memory
}

void PMM_FreePage(void* addr) {
    uint64_t page = (uint64_t)addr / PAGE_SIZE;
    bitmap[page / 8] &= ~(1 << (page % 8));
}
