#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#include "window.h"

#define MAX_WINDOWS 8

class WindowManager {
public:
    WindowManager();
    void AddWindow(Window* win);
    void DrawAll(Renderer& r);
    // Uses MOUSE_EVENT_... constants from window.h
    void HandleMouse(int mx, int my, int event_type);
    void HandleKey(uint32_t key_event);
    
    Window* GetWindowByTitle(const char* title);
    int GetWindowCount() const { return window_count; }

private:
    Window* windows[MAX_WINDOWS];
    int window_count;
    int focused_idx;

    void BringToFront(int idx);
};

#endif
