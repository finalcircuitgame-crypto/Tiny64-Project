#pragma once

#include <stddef.h>
#include <stdint.h>

#define PAGE_PRESENT (1ULL << 0)
#define PAGE_WRITE (1ULL << 1)
#define PAGE_NOCACHE (1ULL << 2)

void *map_mmio(uintptr_t phys, size_t size);
void *vmm_alloc_region(size_t size);
void map_page_2m(uintptr_t virt, uint64_t phys, uint64_t flags);
void vmm_init();

extern uint64_t *g_pml4; // defined in your paging/bootstrap code
