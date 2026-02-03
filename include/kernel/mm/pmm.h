#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <kernel/bootinfo.h>

void pmm_init(BootInfo *boot_info);
void* pmm_alloc_page(void);
void pmm_free_page(void* ptr);

#endif
