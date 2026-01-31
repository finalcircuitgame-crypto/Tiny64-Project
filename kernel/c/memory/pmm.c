#include "pmm.h"
#include "../drivers/platform/serial.h"
#include <stdint.h>

#define PAGE_SIZE 4096
#define MAX_MEMORY_SIZE 0x800000000ULL
#define PMM_MIN_FRAME 0x1000 // Start at 16MB to skip kernel and heap

static const uint64_t max_frame = MAX_MEMORY_SIZE / PAGE_SIZE;
static uint8_t memory_bitmap[(MAX_MEMORY_SIZE / PAGE_SIZE) / 8];

static uint64_t total_memory = 0;
static uint64_t free_memory = 0;

static inline void bitmap_set(uint64_t bit) {
    memory_bitmap[bit / 8] |= (uint8_t)(1u << (bit % 8));
}

static inline void bitmap_unset(uint64_t bit) {
    memory_bitmap[bit / 8] &= (uint8_t)~(1u << (bit % 8));
}

static inline int bitmap_test(uint64_t bit) {
    return (memory_bitmap[bit / 8] & (uint8_t)(1u << (bit % 8))) != 0;
}

void pmm_init(void* map, uint64_t map_size, uint64_t desc_size) {
    EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)map;
    uint64_t entries = map_size / desc_size;

    for (uint64_t i = 0; i < sizeof(memory_bitmap); i++) {
        memory_bitmap[i] = 0xFF;
    }

    total_memory = 0;
    free_memory = 0;

    for (uint64_t i = 0; i < entries; i++) {
        EFI_MEMORY_DESCRIPTOR* d = (EFI_MEMORY_DESCRIPTOR*)((uint64_t)map + (i * desc_size));

        if (d->Type == 7) {
            uint64_t start_frame = d->PhysicalStart / PAGE_SIZE;
            uint64_t page_count = d->NumberOfPages;

            if (start_frame >= max_frame) {
                continue;
            }

            if (start_frame + page_count > max_frame) {
                page_count = max_frame - start_frame;
            }

            total_memory += page_count * PAGE_SIZE;
            free_memory += page_count * PAGE_SIZE;

            for (uint64_t j = 0; j < page_count; j++) {
                uint64_t frame = start_frame + j;
                if (frame < PMM_MIN_FRAME) {
                    continue;
                }
                bitmap_unset(frame);
            }
        }
    }

    serial_write_str("[PMM] Initialized\r\n");
    serial_write_str("[PMM] Total: ");
    serial_write_dec(total_memory);
    serial_write_str(" bytes\r\n");
    serial_write_str("[PMM] Free:  ");
    serial_write_dec(free_memory);
    serial_write_str(" bytes\r\n");
}

void* pmm_alloc_page() {
    return pmm_alloc_pages(1);
}

void* pmm_alloc_pages(uint64_t count) {
    if (count == 0) return 0;
    
    for (uint64_t i = PMM_MIN_FRAME; i <= max_frame - count; i++) {
        int found = 1;
        for (uint64_t j = 0; j < count; j++) {
            if (bitmap_test(i + j)) {
                found = 0;
                i += j; // Skip checked bits
                break;
            }
        }
        
        if (found) {
            for (uint64_t j = 0; j < count; j++) {
                bitmap_set(i + j);
            }
            free_memory -= count * PAGE_SIZE;
            return (void*)(i * PAGE_SIZE);
        }
    }
    return 0;
}

void pmm_free_page(void* address) {
    uint64_t frame = (uint64_t)address / PAGE_SIZE;
    if (frame < PMM_MIN_FRAME || frame >= max_frame) {
        return;
    }
    if (bitmap_test(frame)) {
        bitmap_unset(frame);
        free_memory += PAGE_SIZE;
    }
}

uint64_t pmm_get_free_memory() {
    return free_memory;
}

uint64_t pmm_get_total_memory() {
    return total_memory;
}
