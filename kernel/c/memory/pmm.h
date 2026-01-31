#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include "efi_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

void pmm_init(void* map, uint64_t map_size, uint64_t desc_size);
void* pmm_alloc_page();
void* pmm_alloc_pages(uint64_t count);
void pmm_free_page(void* address);
uint64_t pmm_get_free_memory();
uint64_t pmm_get_total_memory();

#ifdef __cplusplus
}
#endif

#endif
