#ifndef WINDOW_H
#define WINDOW_H

#include "../graphics/renderer.h"

// Mouse Event Types
#define MOUSE_EVENT_MOVE 0
#define MOUSE_EVENT_DOWN 1
#define MOUSE_EVENT_UP   2

class Window {
public:
    Window(const char* title, int x, int y, int w, int h);
    virtual ~Window() {}

    virtual void Draw(Renderer& r);
    // Replaces HandleClick and HandleMouseMove
    virtual void OnMouseEvent(int mx, int my, int type);
    virtual void HandleKey(uint32_t key_event) {}
    virtual void Tick() {}

    bool IsClosed() const { return closed; }
    void SetClosed(bool c) { closed = c; }
    bool IsMinimized() const { return minimized; }
    void SetMinimized(bool m) { minimized = m; }
    const char* GetTitle() const { return title; }

    int GetX() const { return x; }
    int GetY() const { return y; }
    int GetW() const { return w; }
    int GetH() const { return h; }

protected:
    char title[64];
    int x, y, w, h;
    bool closed;
    bool minimized;
    
    // Dragging state
    bool dragging;
    int drag_off_x, drag_off_y;
    
    // Widget states
    bool close_btn_down;
};

#endif
