#ifndef TTF_H
#define TTF_H

#include "framebuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FONT_REGULAR = 0,
    FONT_BOLD,
    FONT_ITALIC,
    FONT_BOLD_ITALIC,
    FONT_ROBOTO_REGULAR,
    FONT_ROBOTO_BOLD,
    FONT_ROBOTO_ITALIC,
    FONT_ROBOTO_BOLD_ITALIC,
    FONT_MAX
} font_style_t;

void ttf_init();
void ttf_draw_string(Framebuffer* fb, int x, int y, const char* str, uint32_t color, float size, font_style_t style);

#ifdef __cplusplus
}
#endif

#endif
