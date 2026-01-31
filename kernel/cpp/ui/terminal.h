#ifndef TERMINAL_H
#define TERMINAL_H

#include "window.h"

class TerminalWindow : public Window {
public:
    TerminalWindow(int x, int y, int w, int h);
    void Draw(Renderer& r) override;
    void OnMouseEvent(int mx, int my, int type) override;
    void HandleKey(uint32_t key_event) override;
    void AddHistory(const char* text, uint32_t color);

private:
    char input_buf[128];
    int input_idx;
    char prompt[128];
    
    struct Line {
        char text[128];
        uint32_t color;
    };
    Line history[16];
    int history_count;

    void ExecuteCommand(const char* cmd);
};

#endif
