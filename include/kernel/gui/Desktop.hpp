#pragma once

#include <stdint.h>
#include <kernel/bootinfo.h>

extern "C" {
    void draw_char_ttf(int x, int y, char c, uint32_t color, float scale_override);
    void draw_pixel(int x, int y, uint32_t color);
    int gui_get_font_scale();
    float gui_get_string_width(const char* s);
    
    typedef struct {
        uint8_t second;
        uint8_t minute;
        uint8_t hour;
        uint8_t day;
        uint8_t month;
        uint32_t year;
    } RTCTime;
    void rtc_read_time(RTCTime *time);
}

class Desktop {
public:
    Desktop(BootInfo* boot_info);
    
    void draw();
    void draw_window(int x, int y, int w, int h, const char* title);
    void draw_text(int x, int y, const char* s, uint32_t color, float scale = -1.0f);

private:
    void draw_rect(int x, int y, int w, int h, uint32_t color);
    void draw_gradient(int x, int y, int w, int h, uint32_t color1, uint32_t color2);
    
    BootInfo* bi;
};

extern "C" void init_desktop_cpp(BootInfo* boot_info);
