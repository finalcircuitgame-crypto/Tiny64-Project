#include "doom.h"
#include "../../c/graphics/colors.h"
#include "../../c/lib/string.h"
#include "../../c/drivers/platform/serial.h"

extern "C" {
    void doomgeneric_Create(int argc, char **argv);
    void doomgeneric_Tick();
    void DG_SetFramebuffer(void* fb);
    void DG_SetBackBuffer(uint32_t* bb);
    void DG_SetWindowPos(int x, int y);
}

DoomWindow::DoomWindow(int x, int y, int w, int h)
    : Window("DOOM Platinum", x, y, w, h), initialized(false) {
    doom_buffer = nullptr;
}

void DoomWindow::Draw(Renderer& r) {
    if (closed || minimized) return;
    Window::Draw(r);

    if (!initialized) {
        // Content Area
        r.fill_rect(x + 4, y + 32, w - 8, h - 36, 0xFF000000);
        
        // Draw Doom-like Title
        r.draw_string("DOOM", x + w/2 - 40, y + h/2 - 40, 0xFFE74C3C, 3);
        r.draw_string("GENERIC PORT", x + w/2 - 60, y + h/2 + 10, COLOR_GREY, 1);
        
        // Start Button
        r.draw_rounded_rect(x + w/2 - 50, y + h/2 + 40, 100, 30, 5, 0xFFC0392B);
        r.draw_string("START GAME", x + w/2 - 42, y + h/2 + 48, COLOR_WHITE, 1);
    } else {
        // Doom will draw directly to the framebuffer for now via DG_DrawFrame
        // But we should tell it where the framebuffer is
        DG_SetFramebuffer(r.get_fb());
        DG_SetBackBuffer(r.get_back_buffer());
        DG_SetWindowPos(x + 4, y + 32);
        doomgeneric_Tick();
    }
}

void DoomWindow::InitDoom() {
    serial_write_str("DOOM: Initializing Engine...\r\n");
    
    char* argv[] = {(char*)"doomgeneric", (char*)"-iwad", (char*)"doom1.wad"};
    doomgeneric_Create(3, argv);
    
    initialized = true;
}

void DoomWindow::OnMouseEvent(int mx, int my, int type) {
    Window::OnMouseEvent(mx, my, type);
    if (!initialized && type == MOUSE_EVENT_UP) {
        if (mx >= x + w/2 - 50 && mx <= x + w/2 + 50 && my >= y + h/2 + 40 && my <= y + h/2 + 70) {
            InitDoom();
        }
    }
}

void DoomWindow::HandleKey(uint32_t key_event) {
    if (!initialized) return;
    // Pass keys to Doom engine
}

