#include <kernel/bootinfo.h>
#include <kernel/printk.h>
#include <kernel/mm/heap.h>
#include <kernel/serial.h>
#include <kernel/font.h>
#include <lib/string.h>
#include <lib/ttf.h>
#include <stdint.h>

static BootInfo *bi;
static uint32_t *backbuffer = NULL;
static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;

static ttf_font main_font;
static int has_ttf = 0;
static float font_scale = 32.0f;
static const int margin_left = 20;
static const int margin_right = 20;

extern const uint8_t ttf_font_data_inter_regular[];
extern const uint32_t ttf_font_data_inter_regular_len;

extern void init_desktop_cpp(BootInfo* boot_info);

#define TERM_HISTORY 4096
#define TERM_MAX_COLS 256
static char* term_history = NULL; // Flat buffer: TERM_HISTORY * TERM_MAX_COLS
static int term_line_count = 0;
static int term_current_line = 0;
static int term_current_col = 0;

static int term_x, term_y, term_w, term_h;

void console_init(BootInfo *boot_info) {
    bi = boot_info;
    
    // Allocate backbuffer for double buffering
    size_t fb_size = bi->fb_height * bi->fb_pixels_per_scanline * sizeof(uint32_t);
    backbuffer = (uint32_t*)malloc(fb_size);
    if (!backbuffer) {
        serial_write("CRITICAL: Failed to allocate console backbuffer!\n");
        // Fallback to direct drawing (bi->fb_base) if needed, 
        // OR just fail gracefully. For now, we just warn.
    } else {
        memset(backbuffer, 0, fb_size);
    }

    // Initialize TTF before drawing desktop so labels work
    if (ttf_init(&main_font, (uint8_t*)ttf_font_data_inter_regular)) {
        has_ttf = 1;
    }

    // Terminal boundaries (match Desktop.cpp)
    term_x = 51;
    term_y = 73; // 50 + 22 + 1
    term_w = bi->fb_width - 100 - 2;
    term_h = bi->fb_height - 150 - 23 - 2;

    // Draw background using C++ Desktop
    init_desktop_cpp(boot_info);
    
    memset(term_history, 0, sizeof(term_history));

    console_flush();
}

void draw_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= (int)bi->fb_width || y < 0 || y >= (int)bi->fb_height) return;
    backbuffer[y * bi->fb_pixels_per_scanline + x] = color;
}

void console_flush() {
    if (!bi || !bi->fb_base || !backbuffer) return;
    memcpy((void*)bi->fb_base, backbuffer, bi->fb_height * bi->fb_pixels_per_scanline * sizeof(uint32_t));
}

static void ttf_callback(int x, int y, void *userdata) {
    uint32_t color = (uint32_t)(uintptr_t)userdata;
    draw_pixel(x, y, color);
}

void draw_char_ttf(int x, int y, char c, uint32_t color, float scale_override) {
    int glyph = ttf_find_glyph(&main_font, c);
    if (!glyph) return;
    
    float scale = (scale_override > 0 ? scale_override : font_scale) / 2048.0f;
    ttf_render_glyph(&main_font, glyph, (float)x, (float)y, scale, ttf_callback, (void*)(uintptr_t)color);
}

int gui_get_font_scale() { return (int)font_scale; }
float gui_get_string_width(const char* s) {
    if (!has_ttf) return strlen(s) * 8.0f;
    return ttf_get_string_width(&main_font, s, font_scale / 2048.0f);
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

// Debug overlay removed

static void console_redraw_terminal() {
    // Clear terminal area
    for (int y = term_y; y < term_y + term_h; y++) {
        for (int x = term_x; x < term_x + term_w; x++) {
            draw_pixel(x, y, 0x000000);
        }
    }

    int line_height = (int)(has_ttf ? font_scale : 16);
    int max_visible_lines = term_h / line_height;
    
    int start_line = term_line_count - max_visible_lines + 1;
    if (start_line < 0) start_line = 0;

    for (int i = 0; i < max_visible_lines; i++) {
        int line_idx = start_line + i;
        if (line_idx > term_line_count) break;
        
        const char *line = term_history[line_idx % TERM_HISTORY];
        int draw_y = term_y + i * line_height;
        if (has_ttf) draw_y += (int)(font_scale * 0.8f); // Base line adjustment
        
        int cur_x = term_x + 5;
        // ... (rest of the drawing loop) ...
        while (*line) {
            if (has_ttf) {
                int glyph = ttf_find_glyph(&main_font, *line);
                if (glyph > 0) {
                    draw_char_ttf(cur_x, draw_y, *line, 0xFFFFFF, -1.0f);
                    int advance, lsb;
                    ttf_get_glyph_h_metrics(&main_font, glyph, &advance, &lsb);
                    cur_x += (int)(advance * (font_scale / 2048.0f));
                } else {
                    draw_char_fallback(cur_x, draw_y - 12, *line, 0xFFFFFF);
                    cur_x += 8;
                }
            } else {
                draw_char_fallback(cur_x, draw_y, *line, 0xFFFFFF);
                cur_x += 8;
            }
            line++;
            if (cur_x > term_x + term_w - 5) break; // Clip
        }
    }
}

void console_update() {
    if (!bi) return;
    init_desktop_cpp(bi);
    console_redraw_terminal();
    console_flush();
}

void console_putchar(char c) {
    if (!term_history) return;

    if (c == '\n') {
        term_line_count++;
        term_current_line = term_line_count;
        term_current_col = 0;
        
        // Clear new line
        memset(&term_history[(term_line_count % TERM_HISTORY) * TERM_MAX_COLS], 0, TERM_MAX_COLS);
        
        console_redraw_terminal();
        console_flush();
    } else {
        // Auto-wrap if we hit the limit of our line buffer
        if (term_current_col >= TERM_MAX_COLS - 1) {
             term_line_count++;
             term_current_line = term_line_count;
             term_current_col = 0;
             memset(&term_history[(term_line_count % TERM_HISTORY) * TERM_MAX_COLS], 0, TERM_MAX_COLS);
             console_redraw_terminal(); // Redraw on wrap
        }

        term_history[(term_line_count % TERM_HISTORY) * TERM_MAX_COLS + term_current_col++] = c;
            
        // Optimization: Draw character immediately if we have a valid position
        int line_height = (int)(has_ttf ? font_scale : 16);
        int max_visible_lines = term_h / line_height;

        // Only draw if we are on a visible line
        if (term_line_count >= term_line_count - max_visible_lines) {
             // Calculate visual Y
            int current_visible_line = max_visible_lines - 1; // We are always at the bottom if scrolling
            if (term_line_count < max_visible_lines) current_visible_line = term_line_count;

            int draw_y = term_y + current_visible_line * line_height;
            if (has_ttf) draw_y += (int)(font_scale * 0.8f);

            // Calculate X position
            int cur_x = term_x + 5;
            const char* line_ptr = &term_history[(term_line_count % TERM_HISTORY) * TERM_MAX_COLS];
            for(int i=0; i<term_current_col-1; i++) {
                if (has_ttf) {
                    int glyph = ttf_find_glyph(&main_font, line_ptr[i]);
                    int advance, lsb;
                    ttf_get_glyph_h_metrics(&main_font, glyph, &advance, &lsb);
                    cur_x += (int)(advance * (font_scale / 2048.0f));
                } else cur_x += 8;
            }

            // Clip
            if (cur_x < term_x + term_w - 10) {
                if (has_ttf) draw_char_ttf(cur_x, draw_y, c, 0xFFFFFF, -1.0f);
                else draw_char_fallback(cur_x, draw_y, c, 0xFFFFFF);
                console_flush();
            }
        }
    }
}

void console_write_scaled(const char *s, uint32_t color) {
    (void)color; // We use white for terminal history for now
    while (*s) console_putchar(*s++);
}

void console_draw_desktop() {
    // This is now purely a placeholder, actual desktop drawing 
    // is in Desktop.cpp and called via init_desktop_cpp.
}
