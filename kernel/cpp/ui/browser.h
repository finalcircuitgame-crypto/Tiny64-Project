#ifndef BROWSER_H
#define BROWSER_H

#include "window.h"

class BrowserWindow : public Window {
public:
    BrowserWindow(int x, int y, int w, int h);
    void Draw(Renderer& r) override;
    void OnMouseEvent(int mx, int my, int type) override;
    void HandleKey(uint32_t key_event) override;
    void Tick() override;

private:
    char url[256];
    int url_len;
    int url_cursor;
    int url_active;

    char history[16][256];
    int history_count;
    int history_index;

    char page_text[8192];
    int page_scroll;

    int cursor_blink;
    int cursor_on;

    void Navigate();
    void NavigateNoHistory();
    void GoBack();
    void GoForward();
    void Reload();
    void PushHistory();
    void SetPageTextFromHtml(const char* html);
};

#endif
