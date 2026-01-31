#ifndef NOTEPAD_H
#define NOTEPAD_H

#include "window.h"

class Notepad : public Window {
public:
    Notepad(int x, int y, int w, int h);
    void Draw(Renderer& r) override;
    void OnMouseEvent(int mx, int my, int type) override;
    void HandleKey(uint32_t key_event) override;

private:
    char buffer[2048];
    int cursor;
    int selection_start;
    int selection_end;
    int active_menu; // -1 = none, 0 = File, 1 = Edit, etc.
    void SaveToFile();
};

#endif
