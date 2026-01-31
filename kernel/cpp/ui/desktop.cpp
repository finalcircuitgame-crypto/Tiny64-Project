#include "desktop.h"
#include "explorer.h"
#include "terminal.h"
#include "notepad.h"
#include "doom.h"
#include "task_manager.h"
#include "device_manager.h"
#include "window_manager.h"
#include "settings.h"
#include "explorer_icon.h"
#include "app_icons.h"
#include "../graphics/renderer.h"
#include "../../c/drivers/hid/input.h"
#include "../../c/drivers/usb/usb.h"
#include "../../c/scheduler/scheduler.h"
#include "../../c/drivers/platform/rtc.h"
#include "../utils/persistence.h"
#include "../../c/graphics/colors.h"
#include "../../c/fs/ramfs.h"
#include "../../c/lib/string.h"

#define TINY64_VERSION "v1.3.0 Platinum"

extern "C" void start_recovery_mode(void* fb);

extern "C" void desktop_loop(Framebuffer* fb) {
    Renderer renderer(fb);
    
    int mouseX = fb->width / 2;
    int mouseY = fb->height / 2;
    
    // Load Settings
    const char* user = Persistence::LoadUsername();
    int theme = Persistence::LoadTheme();
    int wallpaper = Persistence::LoadWallpaper();

    // UI Colors & Background
    uint32_t bg_color = (theme == 0) ? COLOR_TINY_BLUE : (theme == 2 ? 0xFF888888 : 0xFF000000);
    if (wallpaper == 1) bg_color = 0xFF1A1A2E; // Deep Space
    if (wallpaper == 2) bg_color = 0xFF2D3436; // Modern Dark
    
    uint32_t taskbar_color = 0xCC2C3E50; 
    
    // Window Management
    WindowManager wm;
    FileExplorer explorer(150, 100, 500, 350);
    TerminalWindow terminal(100, 150, 600, 400);
    Notepad notepad(200, 100, 450, 350);
    DoomWindow doom(180, 120, 640, 480);
    TaskManager taskmgr(200, 200, 450, 300);
    DeviceManager devmgr(50, 50, 500, 400);
    SettingsWindow settings(250, 50, 500, 400);
    
    // Start closed
    explorer.SetClosed(true);
    terminal.SetClosed(true);
    notepad.SetClosed(true);
    doom.SetClosed(true);
    taskmgr.SetClosed(true);
    devmgr.SetClosed(true);
    settings.SetClosed(true);

    wm.AddWindow(&explorer);
    wm.AddWindow(&terminal);
    wm.AddWindow(&notepad);
    wm.AddWindow(&doom);
    wm.AddWindow(&taskmgr);
    wm.AddWindow(&devmgr);
    wm.AddWindow(&settings);

    bool start_menu_open = false;
    float menu_anim = 0.0f; // 0.0 to 1.0
    bool uac_prompt_active = false;
    
    input_mouse_state_t ms;
    bool prev_left_btn = false;

    while (true) {
        usb_poll();
        input_mouse_get_state(&ms);
        
        bool left_btn = (ms.buttons & 1);
        bool btn_pressed = left_btn && !prev_left_btn; // Rising edge
        bool btn_released = !left_btn && prev_left_btn; // Falling edge
        
        // Refresh settings periodically or on certain events
        wallpaper = Persistence::LoadWallpaper();
        if (wallpaper == 0) bg_color = COLOR_TINY_BLUE;
        else if (wallpaper == 1) bg_color = 0xFF1A1A2E;
        else if (wallpaper == 2) bg_color = 0xFF2D3436;

        // Always update mouse position
        if (ms.has_abs) {
            mouseX = (ms.x_abs * fb->width) / 32768;
            mouseY = (ms.y_abs * fb->height) / 32768;
        } else {
            mouseX += ms.x_diff;
            mouseY += ms.y_diff;
        }
        
        // Clamp mouse
        if (mouseX < 0) mouseX = 0; if (mouseX > (int)fb->width - 1) mouseX = fb->width - 1;
        if (mouseY < 0) mouseY = 0; if (mouseY > (int)fb->height - 1) mouseY = fb->height - 1;

        // Start Menu Animation Logic
        if (start_menu_open) {
            if (menu_anim < 1.0f) menu_anim += 0.15f;
            if (menu_anim > 1.0f) menu_anim = 1.0f;
        } else {
            if (menu_anim > 0.0f) menu_anim -= 0.15f;
            if (menu_anim < 0.0f) menu_anim = 0.0f;
        }

        // Determine Window Manager Event
        int wm_event = MOUSE_EVENT_MOVE;
        if (btn_pressed) wm_event = MOUSE_EVENT_DOWN;
        else if (btn_released) wm_event = MOUSE_EVENT_UP;
        
        // Handle Start Menu & Taskbar (Only on Release to fix hover bug)
        bool handled_by_shell = false;

        if (btn_released) {
             // Taskbar Clicks
            if (mouseY >= (int)fb->height - 45) {
                handled_by_shell = true;
                if (mouseX >= 10 && mouseX <= 45) {
                    start_menu_open = !start_menu_open;
                } else {
                    // Check app icons
                    int ix = 55;
                    auto check_app_click = [&](Window& w) {
                        if (w.IsClosed()) return false;
                        if (mouseX >= ix && mouseX <= ix + 120) {
                            w.SetMinimized(!w.IsMinimized());
                            start_menu_open = false;
                            return true;
                        }
                        ix += 125;
                        return false;
                    };
                                        if (!check_app_click(terminal))
                                            if (!check_app_click(explorer))
                                                if (!check_app_click(notepad))
                                                    if (!check_app_click(doom))
                                                        if (!check_app_click(taskmgr))
                                                            if (!check_app_click(devmgr))
                                                                check_app_click(settings);                }
            } else if (start_menu_open) {
                // Menu Clicks (use animated Y)
                int current_menu_y = fb->height - 45 - (int)(450 * menu_anim);
                if (mouseX >= 10 && mouseX <= 230 && mouseY >= current_menu_y && mouseY <= current_menu_y + 450) {
                     handled_by_shell = true;
                } else {
                    start_menu_open = false;
                }
            }
        }
        
        if (!handled_by_shell) {
             wm.HandleMouse(mouseX, mouseY, wm_event);
        }

        prev_left_btn = left_btn;

        if (input_keyboard_available()) {
            uint32_t e = input_keyboard_get_event();
            char c = (char)(e & 0xFF);
            if (c == 27) start_menu_open = !start_menu_open;
            else wm.HandleKey(e);
        }

        // --- RENDER PASS ---
        renderer.clear(bg_color);
        
        // Windows
        wm.DrawAll(renderer);

        // Taskbar
        int tb_h = 45;
        int tb_y = fb->height - tb_h;
        renderer.fill_rect(0, tb_y, fb->width, tb_h, taskbar_color);
        
        int icon_x = 10;
        // Start Button
        renderer.draw_rounded_rect(icon_x, tb_y + 5, 35, 35, 5, start_menu_open ? 0x88FFFFFF : 0xFF3498DB);
        renderer.draw_string("T", icon_x + 12, tb_y + 12, 0xFFFFFFFF, 2);
        icon_x += 45;

        // App Icons with Logos
        auto draw_app_icon = [&](bool closed, bool minimized, const char* label, const uint32_t* icon_data, uint32_t logo_col, int& ix) {
            if (closed) return;
            bool active = !minimized;
            bool hover = (mouseX >= ix && mouseX <= ix + 120 && mouseY >= tb_y);
            uint32_t col = active ? 0x88FFFFFF : (hover ? 0x66FFFFFF : 0x44FFFFFF);
            renderer.draw_rounded_rect(ix, tb_y + 5, 120, 35, 5, col);
            
            // Draw Mini Logo
            if (icon_data) {
                renderer.draw_image(icon_data, ix + 5, tb_y + 7, 32, 32);
            } else {
                renderer.draw_rounded_rect(ix + 5, tb_y + 10, 25, 25, 3, logo_col);
            }
            
            renderer.draw_string(label, ix + 40, tb_y + 15, 0xFFFFFFFF, 1);
            renderer.fill_rect(ix + 65, tb_y + 38, 4, 3, minimized ? 0xFF999999 : 0xFF3498DB);
            ix += 125;
        };

        draw_app_icon(terminal.IsClosed(), terminal.IsMinimized(), "Terminal", icon_terminal_data, 0xFF273C75, icon_x);
        draw_app_icon(explorer.IsClosed(), explorer.IsMinimized(), "Explorer", icon_explorer_data, 0xFFE67E22, icon_x);
        draw_app_icon(notepad.IsClosed(), notepad.IsMinimized(), "Notepad", icon_notepad_data, 0xFFF1C40F, icon_x);
        draw_app_icon(doom.IsClosed(), doom.IsMinimized(), "DOOM", nullptr, 0xFFC0392B, icon_x);
        draw_app_icon(taskmgr.IsClosed(), taskmgr.IsMinimized(), "Task Mgr", icon_taskmgr_data, 0xFFC0392B, icon_x);
        draw_app_icon(devmgr.IsClosed(), devmgr.IsMinimized(), "Devices", icon_devmgr_data, 0xFF16A085, icon_x);
        draw_app_icon(settings.IsClosed(), settings.IsMinimized(), "Settings", icon_settings_data, 0xFF7F8C8D, icon_x);
        
        // Network Status Icon
        renderer.draw_image(icon_ethernet_data, fb->width - 130, tb_y + 7, 32, 32);

        // Clock (Real RTC)
        rtc_time_t now;
        rtc_get_time(&now);
        char time_buf[16];
        ksprintf(time_buf, "%02d:%02d:%02d", now.hour, now.minute, now.second);
        renderer.draw_string(time_buf, fb->width - 90, tb_y + 15, 0xFFFFFFFF, 1);

        // Start Menu (Animated)
        if (menu_anim > 0.01f) {
            int menu_h = 450;
            int menu_y = fb->height - 45 - (int)(menu_h * menu_anim);
            renderer.draw_rounded_rect(10, menu_y, 220, menu_h, 10, 0xEE2C3E50);
            renderer.draw_string("TINY64 MENU", 30, menu_y + 15, 0xFF3498DB, 1);
            renderer.fill_rect(20, menu_y + 35, 200, 1, 0xFF444444);
            
            struct MenuItem {
                const char* label;
                int type; // 0 = category, 1 = app, 2 = separator
                int action_id;
            };

            MenuItem items[] = {
                {"APPLICATIONS", 0, -1},
                {"File Explorer", 1, 0},
                {"Terminal", 1, 1},
                {"Notepad", 1, 5},
                {"DOOM Platinum", 1, 6},
                {"", 2, -1},
                {"UTILITIES", 0, -1},
                {"Task Manager", 1, 2},
                {"Device Manager", 1, 3},
                {"", 2, -1},
                {"SYSTEM", 0, -1},
                {"Settings", 1, 4},
                {"Recovery", 1, 8},
                {"Shutdown", 1, 9}
            };

            int current_y = menu_y + 45;
            for (int i = 0; i < 15; i++) {
                if (items[i].type == 0) {
                    renderer.draw_string(items[i].label, 25, current_y, 0xFF95A5A6, 1);
                    current_y += 20;
                } else if (items[i].type == 1) {
                    bool hover = (mouseX >= 20 && mouseX <= 210 && mouseY >= current_y - 5 && mouseY <= current_y + 25);
                    if (hover) {
                        renderer.draw_rounded_rect(20, current_y - 5, 190, 30, 5, 0x44FFFFFF);
                        if (btn_released) {
                            int aid = items[i].action_id;
                            if (aid == 0) { explorer.SetClosed(false); explorer.SetMinimized(false); }
                            if (aid == 1) { terminal.SetClosed(false); terminal.SetMinimized(false); }
                            if (aid == 5) { notepad.SetClosed(false); notepad.SetMinimized(false); }
                            if (aid == 6) { doom.SetClosed(false); doom.SetMinimized(false); }
                            if (aid == 2) { taskmgr.SetClosed(false); taskmgr.SetMinimized(false); }
                            if (aid == 3) { devmgr.SetClosed(false); devmgr.SetMinimized(false); }
                            if (aid == 4) { settings.SetClosed(false); settings.SetMinimized(false); }
                            if (aid == 8) { start_recovery_mode(fb); }
                            start_menu_open = false;
                        }
                    }
                    renderer.draw_string(items[i].label, 35, current_y, 0xFFFFFFFF, 1);
                    current_y += 32;
                } else if (items[i].type == 2) {
                    renderer.fill_rect(25, current_y + 5, 180, 1, 0xFF333333);
                    current_y += 15;
                }
            }
        }

        // Desktop Shortcuts
        struct Shortcut {
            const char* name;
            int x, y;
            int action_id;
            const uint32_t* icon_data;
            uint32_t color;
        };
        Shortcut shortcuts[] = {
            {"Computer", 20, 20, 0, icon_explorer_data, 0xFF3498DB},
            {"Terminal", 20, 100, 1, icon_terminal_data, 0xFF273C75},
            {"Notepad", 20, 180, 5, icon_notepad_data, 0xFFF1C40F},
            {"DOOM", 20, 260, 6, nullptr, 0xFFC0392B},
            {"Network", 20, 340, 7, icon_ethernet_data, 0xFF3498DB},
            {"Settings", 20, 420, 4, icon_settings_data, 0xFF7F8C8D}
        };

        for(int i=0; i<6; i++) {
            bool hover = (mouseX >= shortcuts[i].x && mouseX <= shortcuts[i].x + 60 && mouseY >= shortcuts[i].y && mouseY <= shortcuts[i].y + 70);
            if (hover) {
                renderer.draw_rounded_rect(shortcuts[i].x - 5, shortcuts[i].y - 5, 70, 80, 5, 0x33FFFFFF);
                if (btn_released && !handled_by_shell) {
                   int aid = shortcuts[i].action_id;
                   if (aid == 0) { explorer.SetClosed(false); explorer.SetMinimized(false); }
                   if (aid == 1) { terminal.SetClosed(false); terminal.SetMinimized(false); }
                   if (aid == 5) { notepad.SetClosed(false); notepad.SetMinimized(false); }
                   if (aid == 6) { doom.SetClosed(false); doom.SetMinimized(false); }
                   if (aid == 4) { settings.SetClosed(false); settings.SetMinimized(false); }
                   if (aid == 7) { devmgr.SetClosed(false); devmgr.SetMinimized(false); }
                }
            }
            if (shortcuts[i].icon_data) {
                renderer.draw_image(shortcuts[i].icon_data, shortcuts[i].x + 14, shortcuts[i].y + 5, 32, 32);
            } else {
                renderer.draw_rounded_rect(shortcuts[i].x + 10, shortcuts[i].y + 5, 40, 40, 8, shortcuts[i].color);
            }
            renderer.draw_string(shortcuts[i].name, shortcuts[i].x, shortcuts[i].y + 50, 0xFFFFFFFF, 1);
        }

        // UAC Prompt Overlay
        if (uac_prompt_active) {
            renderer.fill_rect(0, 0, fb->width, fb->height, 0x88000000);
            int dialog_w = 300;
            int dialog_h = 150;
            int dialog_x = fb->width/2 - dialog_w/2;
            int dialog_y = fb->height/2 - dialog_h/2;
            renderer.draw_rounded_rect(dialog_x, dialog_y, dialog_w, dialog_h, 10, 0xFF2C3E50);
            renderer.draw_string("USER ACCOUNT CONTROL", dialog_x + 20, dialog_y + 15, 0xFFE74C3C, 1);
            renderer.draw_string("Allow this app to make changes?", dialog_x + 20, dialog_y + 50, 0xFFFFFFFF, 1);
            
            // Buttons
            renderer.draw_rounded_rect(dialog_x + 40, dialog_y + 100, 90, 30, 5, 0xFF2ECC71);
            renderer.draw_string("YES", dialog_x + 75, dialog_y + 108, 0xFFFFFFFF, 1);
            
            renderer.draw_rounded_rect(dialog_x + 170, dialog_y + 100, 90, 30, 5, 0xFFE74C3C);
            renderer.draw_string("NO", dialog_x + 205, dialog_y + 108, 0xFFFFFFFF, 1);
        }

        renderer.draw_cursor(mouseX, mouseY);
        renderer.show();

        for(volatile int i=0; i<3000; i++);
    }
}
