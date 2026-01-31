#include "draw.h"
#include "font8x16.h"
#include "logo_data.h"
#include "colors.h"

int g_font_scale = 1;

void draw_rect(Framebuffer* fb, int x, int y, int w, int h, uint32_t color) {
    uint32_t* pixels = (uint32_t*)fb->base_address;
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            if ((x + j) < fb->width && (y + i) < fb->height) {
                pixels[(y + i) * (fb->pitch / 4) + (x + j)] = color;
            }
        }
    }
}

void draw_char_scaled(Framebuffer* fb, char c, int x, int y, uint32_t color, int scale) {
    uint32_t* pixels = (uint32_t*)fb->base_address;
    const uint8_t* glyph = font8x16_basic[(uint8_t)c];
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 8; j++) {
            if (glyph[i] & (1 << (7-j))) {
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        int pixelX = x + j * scale + sx;
                        int pixelY = y + i * scale + sy;
                        if (pixelX >= 0 && pixelX < fb->width && pixelY >= 0 && pixelY < fb->height) {
                            pixels[pixelY * (fb->pitch / 4) + pixelX] = color;
                        }
                    }
                }
            }
        }
    }
}

void draw_char(Framebuffer* fb, char c, int x, int y, uint32_t color) {
    draw_char_scaled(fb, c, x, y, color, g_font_scale);
}

void draw_char_to_buffer(void* buffer, uint32_t pitch, uint32_t width, uint32_t height, char c, int x, int y, uint32_t color) {
    uint32_t* pixels = (uint32_t*)buffer;
    const uint8_t* glyph = font8x16_basic[(uint8_t)c];
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 8; j++) {
            if (glyph[i] & (1 << (7-j))) {
                int pixelX = x + j;
                int pixelY = y + i;
                if (pixelX >= 0 && (uint32_t)pixelX < width && pixelY >= 0 && (uint32_t)pixelY < height) {
                    pixels[pixelY * (pitch / 4) + pixelX] = color;
                }
            }
        }
    }
}

void draw_string_scaled(Framebuffer* fb, const char* str, int x, int y, uint32_t color, int scale) {
    for (int i = 0; str[i]; i++) {
        draw_char_scaled(fb, str[i], x + i * 8 * scale, y, color, scale);
    }
}

void draw_string(Framebuffer* fb, const char* str, int x, int y, uint32_t color) {
    draw_string_scaled(fb, str, x, y, color, g_font_scale);
}

void draw_logo(Framebuffer* fb, int cx, int cy) {
    int start_x = cx - logo_width / 2;
    int start_y = cy - logo_height / 2;
    
    uint32_t* pixels = (uint32_t*)fb->base_address;
    for (uint32_t i = 0; i < logo_height; i++) {
        for (uint32_t j = 0; j < logo_width; j++) {
            int draw_x = start_x + j;
            int draw_y = start_y + i;
            if (draw_x >= 0 && draw_x < fb->width && draw_y >= 0 && draw_y < fb->height) {
                uint32_t pixel = logo_data[i * logo_width + j];
                uint32_t r = (pixel >> 16) & 0xFF;
                uint32_t g = (pixel >> 8) & 0xFF;
                uint32_t b = (pixel & 0xFF);
                uint32_t corrected = (b << 16) | (g << 8) | r;
                
                pixels[draw_y * (fb->pitch / 4) + draw_x] = corrected;
            }
        }
    }
}

void put_pixel(Framebuffer* fb, uint32_t x, uint32_t y, uint32_t color) {
    if (x >= fb->width || y >= fb->height) return;
    uint32_t* pixels = (uint32_t*)fb->base_address;
    pixels[y * (fb->pitch / 4) + x] = color;
}

void clear_screen(Framebuffer* fb, uint32_t color) {
    uint32_t* pixels = (uint32_t*)fb->base_address;
    uint32_t count = (fb->pitch / 4) * fb->height;
    for (uint32_t i = 0; i < count; i++) {
        pixels[i] = color;
    }
}

void draw_mouse_cursor(Framebuffer* fb, int x, int y) {
    static const uint16_t cursor_mask[] = {
        0b1000000000000000,
        0b1100000000000000,
        0b1110000000000000,
        0b1111000000000000,
        0b1111100000000000,
        0b1111110000000000,
        0b1111111000000000,
        0b1111111100000000,
        0b1111111110000000,
        0b1111111111000000,
        0b1111110000000000,
        0b1101110000000000,
        0b1000110000000000,
        0b0000110000000000,
        0b0000011000000000,
        0b0000011000000000,
    };
    uint32_t* pixels = (uint32_t*)fb->base_address;
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            if (cursor_mask[i] & (1 << (15 - j))) {
                int px = x + j;
                int py = y + i;
                if (px >= 0 && (uint32_t)px < fb->width && py >= 0 && (uint32_t)py < fb->height) {
                    pixels[py * (fb->pitch / 4) + px] ^= 0xFFFFFF;
                }
            }
        }
    }
}
