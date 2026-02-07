#ifndef PMM_H
#define PMM_H

#include "common.h"
#include <stdint.h>


#define PAGE_SIZE 4096

void pmm_init(BootInfo *boot_info);
void *pmm_alloc_page();
void pmm_free_page(void *page);

uint64_t pmm_get_free_memory();
uint64_t pmm_get_used_memory();
uint64_t pmm_get_total_memory();

#endif
