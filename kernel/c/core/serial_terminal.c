#include "serial_terminal.h"
#include "../graphics/draw.h"
#include "../graphics/colors.h"
#include "../lib/string.h"

static Framebuffer* term_fb = 0;
static uint8_t* back_buffer = 0;
static int cursor_x = 0;
static int cursor_y = 0;
static int term_cols = 0;
static int term_rows = 0;

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 16
#define BG_COLOR 0xFF000000
#define FG_COLOR 0xFF00FF00 // Classic Matrix/Serial Green

static int term_active = 1;

void serial_terminal_init(Framebuffer* fb) {
    term_fb = fb;
    cursor_x = 0;
    cursor_y = 0;
    term_cols = fb->width / CHAR_WIDTH;
    term_rows = fb->height / CHAR_HEIGHT;
    term_active = 1;
    
    // Allocate back buffer in RAM
    extern void* kmalloc(size_t size);
    back_buffer = (uint8_t*)kmalloc(fb->buffer_size);
    if (back_buffer) {
        memset(back_buffer, 0, fb->buffer_size);
    }

    // Clear screen
    draw_rect(fb, 0, 0, fb->width, fb->height, BG_COLOR);
}

void serial_terminal_stop() {
    term_active = 0;
}

static void scroll() {
    if (!term_active) return;
    if (cursor_y >= term_rows) {
        if (term_rows <= 1) {
            draw_rect(term_fb, 0, 0, term_fb->width, term_fb->height, BG_COLOR);
            cursor_y = 0;
            return;
        }

        uint32_t line_pitch = term_fb->pitch; // Bytes per row of pixels
        uint32_t scroll_bytes = line_pitch * CHAR_HEIGHT;
        uint32_t total_bytes = line_pitch * term_fb->height;
        uint8_t* fb_base = (uint8_t*)term_fb->base_address;

        if (back_buffer) {
            // Use back buffer for scrolling (fast RAM copy)
            memcpy(back_buffer, back_buffer + scroll_bytes, total_bytes - scroll_bytes);
            memset(back_buffer + (total_bytes - scroll_bytes), 0, scroll_bytes);
            
            // Sync to framebuffer (write-only to MMIO)
            memcpy(fb_base, back_buffer, total_bytes);
        } else {
            // Fallback to slow MMIO read-back if no back buffer
            memcpy(fb_base, fb_base + scroll_bytes, total_bytes - scroll_bytes);
            draw_rect(term_fb, 0, (term_rows - 1) * CHAR_HEIGHT, term_fb->width, CHAR_HEIGHT, BG_COLOR);
        }

        cursor_y = term_rows - 1;
    }
}

void serial_terminal_putc(char c) {
    if (!term_fb || !term_active) return;

    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
        scroll();
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\t') {
        cursor_x += 4;
        if (cursor_x >= term_cols) {
            cursor_x = 0;
            cursor_y++;
            scroll();
        }
    } else {
        draw_char(term_fb, c, cursor_x * CHAR_WIDTH, cursor_y * CHAR_HEIGHT, FG_COLOR);
        
        // Also draw to back buffer if available
        if (back_buffer) {
            draw_char_to_buffer(back_buffer, term_fb->pitch, term_fb->width, term_fb->height, c, cursor_x * CHAR_WIDTH, cursor_y * CHAR_HEIGHT, FG_COLOR);
        }

        cursor_x++;
        if (cursor_x >= term_cols) {
            cursor_x = 0;
            cursor_y++;
            scroll();
        }
    }
}
