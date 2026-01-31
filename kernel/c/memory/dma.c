#include "dma.h"
#include "pmm.h"
#include "../lib/string.h"

#define PAGE_SIZE 4096

void* dma_alloc_coherent(size_t size, uint64_t* out_phys) {
    if (size == 0) return 0;
    
    uint64_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    void* ptr = pmm_alloc_pages(pages);
    
    if (ptr) {
        *out_phys = (uint64_t)ptr;
        // Zero the memory as it might contain sensitive or random data
        memset(ptr, 0, size);
        // Ensure the zeroing is visible to the device
        dma_sync_for_device(ptr, size);
    }
    
    return ptr;
}

void dma_free_coherent(void* v, uint64_t phys, size_t size) {
    if (!v) return;
    
    uint64_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint64_t i = 0; i < pages; i++) {
        pmm_free_page((void*)((uintptr_t)v + i * PAGE_SIZE));
    }
}

uint64_t virt_to_phys(void* v) {
    // Tiny64 uses identity mapping for kernel memory
    return (uint64_t)v;
}

void dma_sync_for_device(void* v, size_t size) {
    if (!v || size == 0) return;
    
    uintptr_t start = (uintptr_t)v;
    uintptr_t end = start + size;
    
    // Align start to 64-byte cache line
    start &= ~63UL;
    
    for (uintptr_t p = start; p < end; p += 64) {
        __asm__ volatile("clflush (%0)" : : "r"(p) : "memory");
    }
    
    __asm__ volatile("mfence" : : : "memory");
}

void dma_sync_for_cpu(void* v, size_t size) {
    // On x86, mfence is generally sufficient for ensuring ordering
    // In more complex scenarios with non-coherent DMA, more aggressive 
    // invalidation might be needed.
    (void)v;
    (void)size;
    __asm__ volatile("mfence" : : : "memory");
}
