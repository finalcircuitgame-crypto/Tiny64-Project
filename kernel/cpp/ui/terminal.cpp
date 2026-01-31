#include "../../c/drivers/hid/input.h"
#include "terminal.h"
#include "../../c/lib/string.h"
#include "../../c/graphics/colors.h"
#include "../../c/fs/ramfs.h"
#include "../../c/core/commands.h"

// Static reference for the callback
static TerminalWindow* g_active_terminal = nullptr;

static void terminal_output_callback(const char* text, uint32_t color) {
    if (g_active_terminal) {
        g_active_terminal->AddHistory(text, color);
    }
}

TerminalWindow::TerminalWindow(int x, int y, int w, int h)
    : Window("Terminal", x, y, w, h), input_idx(0), history_count(0) {
    strcpy(prompt, "Tiny64> ");
    AddHistory("Welcome to Tiny64 Terminal", COLOR_TINY_GREEN);
    AddHistory("Type 'help' for commands.", COLOR_GREY);
}

void TerminalWindow::AddHistory(const char* text, uint32_t color) {
    if (history_count < 16) {
        strcpy(history[history_count].text, text);
        history[history_count].color = color;
        history_count++;
    } else {
        for (int i = 0; i < 15; i++) history[i] = history[i+1];
        strcpy(history[15].text, text);
        history[15].color = color;
    }
}

void TerminalWindow::Draw(Renderer& r) {
    if (closed || minimized) return;
    Window::Draw(r); // Draw frame

    // Content Area
    r.fill_rect(x + 4, y + 32, w - 8, h - 36, 0xFF111111);

    int cur_y = y + 40;
    for (int i = 0; i < history_count; i++) {
        r.draw_string(history[i].text, x + 10, cur_y, history[i].color, 1);
        cur_y += 20;
    }

    // Input line
    char full_line[256];
    strcpy(full_line, prompt);
    strncat(full_line, input_buf, input_idx);
    strcat(full_line, "_");
    r.draw_string(full_line, x + 10, cur_y, COLOR_WHITE, 1);
}

void TerminalWindow::OnMouseEvent(int mx, int my, int type) {
    Window::OnMouseEvent(mx, my, type);
}

void TerminalWindow::HandleKey(uint32_t key_event) {
    char c = (char)(key_event & 0xFF);
    // Only ignore if CTRL or ALT is pressed, but allow SHIFT
    if ((key_event & KB_MOD_CTRL) || (key_event & KB_MOD_ALT)) return;

    if (c == '\n' || c == '\r') {
        input_buf[input_idx] = 0;
        char echo[160];
        strcpy(echo, prompt);
        strcat(echo, input_buf);
        AddHistory(echo, COLOR_WHITE);
        
        ExecuteCommand(input_buf);
        
        input_idx = 0;
    } else if (c == '\b' || c == 0x7F) {
        if (input_idx > 0) input_idx--;
    } else if (input_idx < 127) {
        input_buf[input_idx++] = c;
    }
}

void TerminalWindow::ExecuteCommand(const char* cmd) {
    g_active_terminal = this;
    if (strcmp(cmd, "exit") == 0) {
        closed = true;
    } else {
        commands_execute(cmd, terminal_output_callback);
    }
    g_active_terminal = nullptr;
}
