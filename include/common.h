#pragma once

#include <stdint.h>

typedef struct {
    void* framebuffer_base;
    uint64_t framebuffer_size;
    uint32_t width;
    uint32_t height;
    uint32_t pixels_per_scanline;
} Framebuffer;

typedef struct {
    Framebuffer framebuffer;
} BootInfo;
