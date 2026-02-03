#ifndef BOOTINFO_H
#define BOOTINFO_H

#include <stdint.h>

typedef struct {
    void* fb_base;
    uint64_t fb_size;
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t fb_pixels_per_scanline;
    
    void* mmap;
    uint64_t mmap_size;
    uint64_t mmap_desc_size;
} BootInfo;

#endif
