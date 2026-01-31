#ifndef SETTINGS_WINDOW_H
#define SETTINGS_WINDOW_H

#include "window.h"

class SettingsWindow : public Window {
public:
    SettingsWindow(int x, int y, int w, int h);
    void Draw(Renderer& r) override;
    void OnMouseEvent(int mx, int my, int type) override;

private:
    int active_tab; // 0=About, 1=Display, 2=Personalization, 3=System, 4=Network
    int selected_wallpaper;
    int selected_theme;
    bool hardware_accel;
    bool show_fps;
    char username[32];
};

#endif