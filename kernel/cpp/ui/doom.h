#ifndef DOOM_H
#define DOOM_H

#include "window.h"

class DoomWindow : public Window {
public:
    DoomWindow(int x, int y, int w, int h);
    void Draw(Renderer& r) override;
    void OnMouseEvent(int mx, int my, int type) override;
    void HandleKey(uint32_t key_event) override;

private:
    uint32_t* doom_buffer;
    bool initialized;
    void InitDoom();
};

#endif
