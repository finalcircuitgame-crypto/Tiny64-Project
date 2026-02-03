#include <kernel/gui/Desktop.hpp>
#include <lib/string.h>
#include <kernel/printk.h>

Desktop::Desktop(BootInfo* boot_info) : bi(boot_info) {}

void Desktop::draw_rect(int x, int y, int w, int h, uint32_t color) {
    for (int i = y; i < y + h; i++) {
        for (int j = x; j < x + w; j++) {
            draw_pixel(j, i, color);
        }
    }
}

void Desktop::draw_gradient(int x, int y, int w, int h, uint32_t color1, uint32_t color2) {
    // Simple vertical gradient
    uint8_t r1 = (color1 >> 16) & 0xFF;
    uint8_t g1 = (color1 >> 8) & 0xFF;
    uint8_t b1 = (color1) & 0xFF;

    uint8_t r2 = (color2 >> 16) & 0xFF;
    uint8_t g2 = (color2 >> 8) & 0xFF;
    uint8_t b2 = (color2) & 0xFF;

    for (int i = 0; i < h; i++) {
        float factor = (float)i / (float)h;
        uint8_t r = (uint8_t)(r1 + factor * (r2 - r1));
        uint8_t g = (uint8_t)(g1 + factor * (g2 - g1));
        uint8_t b = (uint8_t)(b1 + factor * (b2 - b1));
        uint32_t color = (r << 16) | (g << 8) | b;
        
        for (int j = x; j < x + w; j++) {
            draw_pixel(j, y + i, color);
        }
    }
}

void Desktop::draw_text(int x, int y, const char* s, uint32_t color, float scale) {
    int cur_x = x;
    while (*s) {
        draw_char_ttf(cur_x, y, *s, color, scale);
        // Approximation for spacing since we don't have per-glyph advance here easily
        // but we can use string width or just fixed spacing for now
        cur_x += (int)(gui_get_font_scale() * 0.6f); 
        s++;
    }
}

void Desktop::draw_window(int x, int y, int w, int h, const char* title) {
    // Window Shadow (simple)
    draw_rect(x + 5, y + 5, w, h, 0x111111);
    
    // Window Body
    draw_rect(x, y, w, h, 0x000000);
    
    // Title Bar
    draw_rect(x, y, w, 22, 0x333333);
    draw_text(x + 10, y + 18, title, 0xFFFFFF, 16.0f);
    
    // Outline
    for(int i=0; i<w; i++){ draw_pixel(x+i, y, 0x666666); draw_pixel(x+i, y+h-1, 0x666666); }
    for(int i=0; i<h; i++){ draw_pixel(x, y+i, 0x666666); draw_pixel(x+w-1, y+i, 0x666666); }
}

void Desktop::draw() {
    // Wallpaper: Nice modern gradient (Purple to Blue)
    draw_gradient(0, 0, bi->fb_width, bi->fb_height, 0x1A2980, 0x26D0CE);

    // Taskbar: Glassy look
    draw_rect(0, bi->fb_height - 40, bi->fb_width, 40, 0x222222);
    
    // "Start" button placeholder
    draw_rect(5, bi->fb_height - 35, 80, 30, 0x444444);
    draw_text(15, bi->fb_height - 12, "Start", 0xFFFFFF, 20.0f);
    
    // Clock: Real-time from RTC
    RTCTime time;
    rtc_read_time(&time);
    char time_str[16];
    
    // Simple format: HH:MM AM/PM
    int hour = time.hour;
    const char* am_pm = (hour >= 12) ? "PM" : "AM";
    if (hour == 0) hour = 12;
    if (hour > 12) hour -= 12;

    time_str[0] = (hour / 10) + '0';
    time_str[1] = (hour % 10) + '0';
    time_str[2] = ':';
    time_str[3] = (time.minute / 10) + '0';
    time_str[4] = (time.minute % 10) + '0';
    time_str[5] = ' ';
    time_str[6] = am_pm[0];
    time_str[7] = am_pm[1];
    time_str[8] = '\0';
    
    if (time_str[0] == '0') {
        // Shift left if leading zero in hour
        for(int i=0; i<8; i++) time_str[i] = time_str[i+1];
    }

    draw_text(bi->fb_width - 120, bi->fb_height - 12, time_str, 0xAAAAAA, 18.0f);

    // Terminal Window
    draw_window(50, 50, bi->fb_width - 100, bi->fb_height - 150, "Tiny64 Terminal");
}

extern "C" void init_desktop_cpp(BootInfo* boot_info) {
    Desktop desktop(boot_info);
    desktop.draw();
}
