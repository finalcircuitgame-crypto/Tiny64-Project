#include <kernel/bootinfo.h>
#include <kernel/printk.h>
#include <kernel/heap.h>
#include <kernel/serial.h>
#include <kernel/font.h>
#include <lib/string.h>
#include <lib/ttf.h>
#include <stdint.h>

static BootInfo *bi;
static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;

static ttf_font main_font;
static int has_ttf = 0;
static float font_scale = 32.0f;
static const int margin_left = 20;
static const int margin_right = 20;

extern const uint8_t ttf_font_data_inter_regular[];
extern const uint32_t ttf_font_data_inter_regular_len;

void console_draw_desktop();
void console_putchar(char c);
void console_write_scaled(const char *s, uint32_t color);

void console_init(BootInfo *boot_info) {
    bi = boot_info;
    
    // Draw background before anything else
    console_draw_desktop();

    cursor_x = margin_left;
    cursor_y = 60; // Start below the title bar

    if (ttf_init(&main_font, (uint8_t*)ttf_font_data_inter_regular)) {
        has_ttf = 1;
        serial_write("TTF font (Inter Regular) initialized.\n");
    } else {
        serial_write("Failed to initialize TTF font.\n");
    }
}

static void draw_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= (int)bi->fb_width || y < 0 || y >= (int)bi->fb_height) return;
    uint32_t *fb = (uint32_t*)bi->fb_base;
    fb[y * bi->fb_pixels_per_scanline + x] = color;
}

static void ttf_callback(int x, int y, void *userdata) {
    uint32_t color = (uint32_t)(uintptr_t)userdata;
    draw_pixel(x, y, color);
}

static void draw_char_ttf(int x, int y, char c, uint32_t color, float scale_override) {
    int glyph = ttf_find_glyph(&main_font, c);
    if (!glyph) return;
    
    float scale = (scale_override > 0 ? scale_override : font_scale) / 2048.0f;
    ttf_render_glyph(&main_font, glyph, (float)x, (float)y, scale, ttf_callback, (void*)(uintptr_t)color);
}

static void draw_char_fallback(int x, int y, char c, uint32_t color) {
    for (int i = 0; i < 16; i++) {
        uint8_t row = vga_font[(uint8_t)c][i];
        for (int j = 0; j < 8; j++) {
            if (row & (0x80 >> j)) {
                draw_pixel(x + j, y + i, color);
            }
        }
    }
}

static void console_scroll() {
    uint32_t *fb = (uint32_t*)bi->fb_base;
    int line_height = (int)(has_ttf ? font_scale : 16);
    int top = 30 + 10; // Start of text area
    int bottom = bi->fb_height - 30;
    
    // Copy each row up
    for (int y = top; y < bottom - line_height; y++) {
        memcpy(&fb[y * bi->fb_pixels_per_scanline], 
               &fb[(y + line_height) * bi->fb_pixels_per_scanline], 
               bi->fb_width * 4);
    }
    
    // Clear bottom line
    for (int y = bottom - line_height; y < bottom; y++) {
        for (uint32_t x = 10; x < bi->fb_width - 10; x++) {
            fb[y * bi->fb_pixels_per_scanline + x] = 0x000000;
        }
    }
    
    cursor_y -= line_height;
}

void console_write_scaled(const char *s, uint32_t color) {
    if (!has_ttf) {
        while (*s) console_putchar(*s++);
        return;
    }

    int available_width = bi->fb_width - margin_left - margin_right;
    float base_scale = font_scale;
    float text_width = ttf_get_string_width(&main_font, s, base_scale / 2048.0f);

    float scale_to_use = base_scale;
    if (text_width > available_width) {
        scale_to_use = (base_scale * (float)available_width) / text_width;
    }

    while (*s) {
        char c = *s++;
        if (c == '\n') {
            cursor_x = margin_left;
            cursor_y += (int)scale_to_use;
        } else {
            int glyph = ttf_find_glyph(&main_font, c);
            if (glyph > 0) {
                draw_char_ttf(cursor_x, cursor_y, c, color, scale_to_use);
                int advance, lsb;
                ttf_get_glyph_h_metrics(&main_font, glyph, &advance, &lsb);
                cursor_x += (int)(advance * (scale_to_use / 2048.0f));
            }
        }

        if (cursor_x > bi->fb_width - margin_right) {
            cursor_x = margin_left;
            cursor_y += (int)scale_to_use;
        }

        if (cursor_y > (int)bi->fb_height - 40) {
            console_scroll();
        }
    }
    
    // After a scaled string, move to next line for safety
    cursor_x = margin_left;
    cursor_y += (int)scale_to_use;

    if (cursor_y > (int)bi->fb_height - 40) {
        console_scroll();
    }
}

void console_putchar(char c) {
    if (!bi || !bi->fb_base) return;

    if (c == '\n') {
        cursor_x = margin_left;
        cursor_y += (int)(has_ttf ? font_scale : 16);
    } else {
        if (has_ttf) {
            int glyph = ttf_find_glyph(&main_font, c);
            if (glyph > 0) {
                draw_char_ttf(cursor_x, cursor_y, c, 0xFFFFFF, -1.0f);
                int advance, lsb;
                ttf_get_glyph_h_metrics(&main_font, glyph, &advance, &lsb);
                cursor_x += (int)(advance * (font_scale / 2048.0f));
            } else {
                draw_char_fallback(cursor_x, cursor_y, c, 0xFFFFFF);
                cursor_x += 8;
            }
        } else {
            draw_char_fallback(cursor_x, cursor_y, c, 0xFFFFFF);
            cursor_x += 8;
        }
    }

    if (cursor_x > bi->fb_width - margin_right) {
        cursor_x = margin_left;
        cursor_y += (int)(has_ttf ? font_scale : 16);
    }
    
    if (cursor_y > (int)bi->fb_height - 40) {
        console_scroll();
    }
}

void console_draw_desktop() {
    if (!bi || !bi->fb_base) return;
    uint32_t *fb = (uint32_t*)bi->fb_base;

    // Background (Dark Blue-ish)
    for (uint32_t i = 0; i < bi->fb_height * bi->fb_pixels_per_scanline; i++) {
        fb[i] = 0x003366;
    }

    // "Terminal Window" background (Black)
    for (uint32_t y = 30; y < bi->fb_height - 30; y++) {
        for (uint32_t x = 10; x < bi->fb_width - 10; x++) {
            fb[y * bi->fb_pixels_per_scanline + x] = 0x000000;
        }
    }

    // "Terminal Title Bar" (Gray)
    for (uint32_t y = 10; y < 30; y++) {
        for (uint32_t x = 10; x < bi->fb_width - 10; x++) {
            fb[y * bi->fb_pixels_per_scanline + x] = 0xAAAAAA;
        }
    }
}
