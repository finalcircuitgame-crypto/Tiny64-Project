#ifndef DRAW_H
#define DRAW_H

#include <stdint.h>
#include "framebuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int g_font_scale;

void draw_rect(Framebuffer* fb, int x, int y, int w, int h, uint32_t color);
void draw_char_scaled(Framebuffer* fb, char c, int x, int y, uint32_t color, int scale);
void draw_char(Framebuffer* fb, char c, int x, int y, uint32_t color);
void draw_char_to_buffer(void* buffer, uint32_t pitch, uint32_t width, uint32_t height, char c, int x, int y, uint32_t color);
void draw_string_scaled(Framebuffer* fb, const char* str, int x, int y, uint32_t color, int scale);
void draw_string(Framebuffer* fb, const char* str, int x, int y, uint32_t color);
void draw_logo(Framebuffer* fb, int cx, int cy);
void draw_mouse_cursor(Framebuffer* fb, int x, int y);

#ifdef __cplusplus
}
#endif

#endif
