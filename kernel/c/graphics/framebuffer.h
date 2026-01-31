#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t base_address;
    uint64_t buffer_size;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
} Framebuffer;

void put_pixel(Framebuffer* fb, uint32_t x, uint32_t y, uint32_t color);
void clear_screen(Framebuffer* fb, uint32_t color);

#ifdef __cplusplus
}
#endif

#endif
