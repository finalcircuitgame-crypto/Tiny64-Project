#ifndef RENDERER_H
#define RENDERER_H

#include "framebuffer.h"
#include "../../c/graphics/ttf.h"
#include <stdint.h>

#ifdef __cplusplus
class Renderer {
public:
    Renderer(Framebuffer* fb);
    ~Renderer();
    void clear(uint32_t color);
    void put_pixel(int x, int y, uint32_t color);
    void fill_rect(int x, int y, int w, int h, uint32_t color);
    void draw_rounded_rect(int x, int y, int w, int h, int r, uint32_t color);
    
    // Scalable Font
    void draw_char(char c, int x, int y, uint32_t color, int scale);
    void draw_string(const char* str, int x, int y, uint32_t color, int scale, font_style_t style = FONT_REGULAR);
    void draw_string_ttf(const char* str, int x, int y, uint32_t color, float size, font_style_t style = FONT_REGULAR);

    void draw_image(const uint32_t* data, int x, int y, int w, int h);

    void draw_cursor(int x, int y);

    // Double Buffering
    void show();

    Framebuffer* get_fb() { return framebuffer; }
    uint32_t* get_back_buffer() { return back_buffer; }

private:
    Framebuffer* framebuffer;
    uint32_t* back_buffer;
    uint32_t blend(uint32_t background, uint32_t foreground);
};
#endif

#endif
