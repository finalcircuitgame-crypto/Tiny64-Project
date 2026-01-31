#include "../../c/drivers/hid/input.h"
#include "notepad.h"
#include "../../c/graphics/colors.h"
#include "../../c/lib/string.h"
#include "../../c/drivers/platform/serial.h"
#include "../../c/fs/ramfs.h"

Notepad::Notepad(int x, int y, int w, int h) : Window("Notepad", x, y, w, h) {
    memset(buffer, 0, sizeof(buffer));
    cursor = 0;
    selection_start = -1;
    selection_end = -1;
    active_menu = -1;
    strcpy(buffer, "Welcome to Tiny64 Notepad!\nType here...");
    cursor = strlen(buffer);
}

void Notepad::Draw(Renderer& r) {
    if (closed || minimized) return;
    Window::Draw(r);

    // Menu Bar
    r.fill_rect(x + 5, y + 35, w - 10, 25, 0xFFF0F0F0);
    const char* menus[] = {"File", "Edit", "Format", "View", "Help"};
    for(int i=0; i<5; i++) {
        uint32_t col = (active_menu == i) ? 0xFF3498DB : COLOR_BLACK;
        if (active_menu == i) {
            r.draw_rounded_rect(x + 10 + i*55, y + 37, 50, 20, 3, 0x443498DB);
        }
        r.draw_string(menus[i], x + 15 + i*55, y + 40, col, 1);
    }

    // Text Area
    r.fill_rect(x + 5, y + 60, w - 10, h - 95, COLOR_WHITE);
    
    // Draw text with selection highlight
    int start_x = x + 10;
    int start_y = y + 70;
    
    if (selection_start != -1 && selection_end != -1) {
        // Very simple selection highlight (blue box behind text)
        // In a real editor we'd calculate line heights and widths
        r.fill_rect(x + 8, y + 68, w - 16, h - 110, 0x333498DB);
    }
    
    r.draw_string(buffer, start_x, start_y, COLOR_BLACK, 1);

    // Draw cursor
    int char_w = 8;
    int char_h = 16;
    // Simple cursor position estimation
    int cur_x = start_x;
    int cur_y = start_y;
    for(int i=0; i<cursor; i++) {
        if (buffer[i] == '\n') {
            cur_x = start_x;
            cur_y += char_h;
        } else {
            cur_x += char_w;
            if (cur_x > x + w - 20) {
                cur_x = start_x;
                cur_y += char_h;
            }
        }
    }
    r.fill_rect(cur_x, cur_y, 2, char_h, 0xFFE74C3C);

    // Status Bar
    r.fill_rect(x + 5, y + h - 35, w - 10, 30, 0xFFF5F5F5);
    char status[64];
    ksprintf(status, "Characters: %d | Encoding: UTF-8", (int)strlen(buffer));
    r.draw_string(status, x + 15, y + h - 25, 0xFF7F8C8D, 1);

    // Action Buttons
    r.draw_rounded_rect(x + w - 70, y + h - 30, 60, 22, 4, 0xFF3498DB);
    r.draw_string("SAVE", x + w - 55, y + h - 26, COLOR_WHITE, 1);
    
    r.draw_rounded_rect(x + w - 140, y + h - 30, 60, 22, 4, 0xFF2ECC71);
    r.draw_string("NEW", x + w - 125, y + h - 26, COLOR_WHITE, 1);
}

void Notepad::OnMouseEvent(int mx, int my, int type) {
    Window::OnMouseEvent(mx, my, type);
    if (type == MOUSE_EVENT_UP) {
        // Menu Bar Clicks
        if (my >= y + 35 && my <= y + 60) {
            for(int i=0; i<5; i++) {
                if (mx >= x + 10 + i*55 && mx <= x + 60 + i*55) {
                    active_menu = i;
                    serial_write_str("Notepad: Clicked menu ");
                    serial_write_dec(i);
                    serial_write_str("\r\n");
                    
                    if (i == 0) { // File
                        // Just show Save message for now as a "work"
                        SaveToFile();
                    }
                    if (i == 1) { // Edit
                        selection_start = 0;
                        selection_end = strlen(buffer);
                        serial_write_str("Notepad: Edit -> Select All\r\n");
                    }
                    return;
                }
            }
        } else {
            active_menu = -1;
        }

        // Save Button
        if (mx >= x + w - 70 && mx <= x + w - 10 && my >= y + h - 30 && my <= y + h - 8) {
            SaveToFile();
        }
        // New Button
        if (mx >= x + w - 140 && mx <= x + w - 80 && my >= y + h - 30 && my <= y + h - 8) {
            memset(buffer, 0, sizeof(buffer));
            cursor = 0;
            selection_start = -1;
            selection_end = -1;
            serial_write_str("Notepad: New File\r\n");
        }
    }
}

void Notepad::HandleKey(uint32_t key_event) {
    if (key_event & KB_MOD_CTRL) {
        char c = (char)(key_event & 0xFF);
        if (c == 'a' || c == 'A') {
            serial_write_str("Notepad: Select All (Ctrl+A)\r\n");
            selection_start = 0;
            selection_end = strlen(buffer);
        }
        return;
    }

    // Reset selection on any key press
    if (!(key_event & KB_MOD_SHIFT)) {
        selection_start = -1;
        selection_end = -1;
    }

    char c = (char)(key_event & 0xFF);
    // Only ignore if ALT is pressed, but allow SHIFT
    if (key_event & KB_MOD_ALT) return;

    if (c == 8) { // Backspace
        if (cursor > 0) {
            cursor--;
            buffer[cursor] = 0;
        }
    } else if (c >= 32 && c <= 126 || c == '\n') {
        if (cursor < (int)sizeof(buffer) - 1) {
            buffer[cursor++] = c;
            buffer[cursor] = 0;
        }
    }
}

void Notepad::SaveToFile() {
    serial_write_str("Notepad: Saving to /note.txt\r\n");
    RamNode* node = ramfs_create("note.txt", FS_FILE);
    if (node) {
        ramfs_write_file(node, buffer);
        serial_write_str("Notepad: Saved successfully.\r\n");
    }
}
