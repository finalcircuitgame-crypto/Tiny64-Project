#include "window.h"
#include "../../c/lib/string.h"
#include "../../c/graphics/colors.h"

Window::Window(const char* t, int x, int y, int w, int h)
    : x(x), y(y), w(w), h(h), closed(false), minimized(false), 
      dragging(false), close_btn_down(false) {
    strcpy(this->title, t);
}

void Window::Draw(Renderer& r) {
    if (closed || minimized) return;

    // Window Shadow/Border
    r.draw_rounded_rect(x, y, w, h, 8, 0xFF000000);
    r.draw_rounded_rect(x + 2, y + 2, w - 4, h - 4, 6, 0xFFFFFFFF);

    // Title Bar
    r.fill_rect(x + 4, y + 4, w - 8, 28, 0xFFCCCCCC);
    r.draw_string(title, x + 12, y + 12, COLOR_BLACK, 1);

    // Close Button
    int btn_x = x + w - 30;
    // Visual feedback for press
    uint32_t btn_col = close_btn_down ? 0xFFA93226 : 0xFFE74C3C;
    r.draw_rounded_rect(btn_x, y + 8, 20, 20, 4, btn_col);
    r.draw_string("x", btn_x + 6, y + 10, COLOR_WHITE, 1);
}

void Window::OnMouseEvent(int mx, int my, int type) {
    if (closed || minimized) return;

    int btn_x = x + w - 30;
    int btn_y = y + 8;
    bool over_close = (mx >= btn_x && mx <= btn_x + 20 && my >= btn_y && my <= btn_y + 20);

    if (type == MOUSE_EVENT_DOWN) {
        if (mx >= x && mx <= x + w && my >= y && my <= y + h) {
            // Close Button Logic
            if (over_close) {
                close_btn_down = true;
            }
            // Title Bar Dragging Logic (only if NOT on close button)
            else if (my >= y && my <= y + 32) {
                dragging = true;
                drag_off_x = mx - x;
                drag_off_y = my - y;
            }
        }
    }
    else if (type == MOUSE_EVENT_UP) {
        dragging = false;
        
        if (close_btn_down && over_close) {
            closed = true;
        }
        close_btn_down = false;
    }
    else if (type == MOUSE_EVENT_MOVE) {
        if (dragging) {
            x = mx - drag_off_x;
            y = my - drag_off_y;
            
            // Constrain to top of screen
            if (y < 0) y = 0;
            // Optional: Constrain other bounds if desired, but top is critical
        }
    }
}
