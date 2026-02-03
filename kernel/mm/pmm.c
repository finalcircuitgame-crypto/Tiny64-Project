#include <kernel/mm/pmm.h>
#include <kernel/efi_types.h>
#include <kernel/printk.h>

static uint64_t free_memory_start = 0;
static uint64_t free_memory_end = 0;
static uint64_t current_ptr = 0;

void pmm_init(BootInfo *boot_info) {
    uint64_t mmap_addr = (uint64_t)boot_info->mmap;
    uint64_t mmap_size = boot_info->mmap_size;
    uint64_t desc_size = boot_info->mmap_desc_size;
    uint64_t largest_free_block = 0;

    for (uint64_t i = 0; i < mmap_size; i += desc_size) {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR*)(mmap_addr + i);
        if (desc->Type == EfiConventionalMemory) {
            uint64_t size = desc->NumberOfPages * 4096;
            if (size > largest_free_block) {
                largest_free_block = size;
                free_memory_start = desc->PhysicalStart;
                free_memory_end = desc->PhysicalStart + size;
            }
        }
    }

    current_ptr = free_memory_start;
    printk("PMM initialized. Free memory: %d MB at %p\n", largest_free_block / (1024*1024), free_memory_start);
}

void* pmm_alloc_page(void) {
    if (current_ptr + 4096 > free_memory_end) {
        return 0; // Out of memory
    }
    void* ptr = (void*)current_ptr;
    current_ptr += 4096;
    return ptr;
}

void pmm_free_page(void* ptr) {
    // Simple bump allocator doesn't support freeing
    (void)ptr;
}
