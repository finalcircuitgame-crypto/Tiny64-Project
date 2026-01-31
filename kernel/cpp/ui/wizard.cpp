#include "wizard.h"
#include "../graphics/renderer.h"
#include "../graphics/animation.h"
#include "../../c/graphics/colors.h"
#include "../../c/drivers/hid/input.h"
#include "../../c/drivers/usb/usb.h"
#include "../../c/fs/fat32.h"
#include "../../c/drivers/network/network.h"
#include "../utils/persistence.h"
#include "../../c/drivers/platform/serial.h" // For debug logs

static void sleep_ms(int ms) {
    // Poll USB while sleeping to prevent ring overflow and input lag
    for (int i = 0; i < ms; i++) {
        usb_poll();
        for (volatile int j = 0; j < 10000; j++);
    }
}

extern "C" void run_wizard(Framebuffer* fb) {
    size_t wizard_ckpt = kmalloc_checkpoint();
    serial_write_str("Wizard: Starting...\r\n");

    // --- AUTO-SAVE LOGS TO USB ---
    int msc_slot = xhci_find_msc_slot();
    if (msc_slot >= 0) {
        fat32_fs_t usb_fs;
        if (fat32_init((uint8_t)msc_slot, &usb_fs) == 0) {
            const char* logs = serial_get_log_buffer();
            uint32_t log_sz = serial_get_log_size();
            serial_write_str("Wizard: Auto-saving logs to USB...\r\n");
            // 8.3 format: "BOOTLOGSTXT"
            if (fat32_save_file(&usb_fs, "BOOTLOGSTXT", logs, log_sz) == 0) {
                serial_write_str("Wizard: Logs saved successfully.\r\n");
            }
        }
    }

    Renderer r(fb);
    r.clear(COLOR_TINY_BLUE); // Blue background for setup
    
    int cx = fb->width / 2;
    int cy = fb->height / 2;
    
    // Animation for Wizard Window
    Animation winAnim(0, 1, 800, EasingType::EaseOutElastic);
    winAnim.Start(0);
    
    // Window Rect
    int w = 550;
    int h = 350;
    int x = cx - w/2;
    int y = cy - h/2;
    
    serial_write_str("Wizard: Running intro animation...\r\n");
    // Intro Loop
    for (int t = 0; t <= 800; t += 16) {
        winAnim.Update(t);
        float scale = winAnim.GetValue();
        
        if (scale < 0) scale = 0;
        if (scale > 2) scale = 2;

        r.clear(COLOR_TINY_BLUE);
        
        int curW = (int)(w * scale);
        int curH = (int)(h * scale);
        
        if (curW > fb->width) curW = fb->width;
        if (curH > fb->height) curH = fb->height;

        int curX = cx - curW/2;
        int curY = cy - curH/2;
        
        int radius = 12;
        if (curW < 20 || curH < 20) radius = 0;

        if (curW > 0 && curH > 0) {
            r.draw_rounded_rect(curX, curY, curW, curH, radius, COLOR_WHITE);
        }
        
        r.show();
        usb_poll();
        sleep_ms(1);
    }
    
    serial_write_str("Wizard: Entering main loop...\r\n");
    // Multi-step Wizard State
    int current_page = 0;
    const int total_pages = 8; 
    bool done = false;
    
    char username[32] = {0};
    int cursor = 0;
    int selected_theme = 0; 
    bool enable_animations = true;
    
    // Network Step State
    network_status_t net_status = NET_STATUS_DETECTING;
    int net_timer = 0;
    uint8_t mac[6] = {0};
    uint8_t ip[4] = {0};

    // Step State
    float step_progress = 0.0f;
    const char* step_status = "";

    int mouse_x = cx;
    int mouse_y = cy;
    bool dragging = false;
    int drag_off_x = 0;
    int drag_off_y = 0;
    bool prev_left_btn = false;

    uint32_t theme_bg = COLOR_TINY_BLUE;
    int t_global = 0;

    while (!done) {
        usb_poll();
        t_global++;

        input_mouse_state_t mstate;
        input_mouse_get_state(&mstate);
        
        bool left_btn = (mstate.buttons & 1);
        bool btn_pressed = left_btn && !prev_left_btn;
        bool btn_released = !left_btn && prev_left_btn;

        if (mstate.updated) {
            if (mstate.has_abs) {
                mouse_x = (mstate.x_abs * fb->width) / 32768;
                mouse_y = (mstate.y_abs * fb->height) / 32768;
            } else {
                mouse_x += mstate.x_diff;
                mouse_y += mstate.y_diff;
            }
            
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_x >= (int)fb->width) mouse_x = fb->width - 1;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_y >= (int)fb->height) mouse_y = fb->height - 1;

            if (btn_pressed) {
                if (mouse_x >= x && mouse_x <= x + w && mouse_y >= y && mouse_y <= y + 40) {
                    dragging = true;
                    drag_off_x = mouse_x - x;
                    drag_off_y = mouse_y - y;
                }
            }
        }

        if (dragging) {
            if (left_btn) {
                x = mouse_x - drag_off_x;
                y = mouse_y - drag_off_y;
                if (y < 0) y = 0;
            } else {
                dragging = false;
            }
        }

        // Handle Clicks
        if (btn_released && !dragging) { 
            // Navigation Buttons (Next/Finish)
            // Steps 3, 4, 6, 7 are automatic and unskippable
            if (current_page != 3 && current_page != 4 && current_page != 6 && current_page != 7) {
                // Next Button
                if (mouse_x >= x + w - 120 && mouse_x <= x + w - 20 &&
                    mouse_y >= y + h - 50 && mouse_y <= y + h - 20) {
                    
                    if (current_page < total_pages - 1) {
                        current_page++;
                        step_progress = 0; // Reset progress for auto steps
                    } 
                }
                
                // Back Button
                if (current_page > 0 && current_page < 3 && mouse_x >= x + 20 && mouse_x <= x + 120 &&
                    mouse_y >= y + h - 50 && mouse_y <= y + h - 20) {
                    current_page--;
                }

                // Skip Button for Network (Page 2)
                if (current_page == 2) {
                    if (mouse_x >= x + w - 230 && mouse_x <= x + w - 130 &&
                        mouse_y >= y + h - 50 && mouse_y <= y + h - 20) {
                        current_page++;
                        step_progress = 0;
                    }
                }
            }
            
            // Theme Selection
            if (current_page == 1) { 
                for (int i = 0; i < 3; i++) {
                    int tx = x + 20 + (i * 100);
                    int ty = y + 150;
                    if (mouse_x >= tx && mouse_x <= tx + 80 && mouse_y >= ty && mouse_y <= ty + 30) {
                        selected_theme = i;
                    }
                }
            }
        }
        
        prev_left_btn = left_btn;

        theme_bg = (selected_theme == 0) ? COLOR_TINY_BLUE : (selected_theme == 1 ? 0xFF111111 : 0xFF888888);
        r.clear(theme_bg);

        // Window Shadow
        r.fill_rect(x + 5, y + 5, w, h, 0x44000000);
        r.draw_rounded_rect(x, y, w, h, 12, COLOR_WHITE);
        
        // Header
        r.fill_rect(x, y, w, 45, 0xFFF0F0F0);
        r.draw_rounded_rect(x, y, w, 45, 12, 0xFFF0F0F0);
        r.fill_rect(x, y + 30, w, 15, 0xFFF0F0F0); // Fill rounding at bottom of header
        r.draw_string("Tiny64 OS Setup", x + 20, y + 14, 0xFF333333, 1, FONT_BOLD);
        
        // Progress Bar (Top)
        int progress_w = (int)(w * ((float)(current_page + 1) / total_pages));
        r.fill_rect(x, y + 45, progress_w, 3, COLOR_TINY_GREEN);

        // --- Step Logic ---
        if (current_page == 2) { // Network Detection
            net_status = network_get_status();
            if (net_status == NET_STATUS_LINK_UP) {
                network_get_mac(mac);
                network_get_ip(ip);
            }
        }
        else if (current_page == 3) { // Driver Setup (Auto)
            step_progress += 0.8f;
            if (step_progress < 40) step_status = "Scanning for PCI Express devices...";
            else if (step_progress < 80) step_status = "Initializing xHCI Host Controller...";
            else if (step_progress < 120) step_status = "Mapping AHCI storage ports...";
            else step_status = "Loading universal HID descriptors...";
            
            if (step_progress >= 160.0f) { current_page++; step_progress = 0; }
        }
        else if (current_page == 4) { // Optimization (Auto)
            step_progress += 0.6f;
            if (step_progress < 50) step_status = "Optimizing VMM page tables...";
            else if (step_progress < 100) step_status = "Tuning scheduler time slices...";
            else step_status = "Calibrating APIC timers...";
            
            if (step_progress >= 150.0f) { current_page++; step_progress = 0; }
        }
        else if (current_page == 6) { // Download (Auto)
            step_progress += 1.2f;
            if (step_progress < 60) step_status = "Fetching system metadata...";
            else if (step_progress < 140) step_status = "Downloading core UI assets...";
            else step_status = "Verifying package signatures...";
            
            if (step_progress >= 200.0f) { current_page++; step_progress = 0; }
        }
        else if (current_page == 7) { // Install (Auto)
            step_progress += 0.5f; 
            if (step_progress < 80) step_status = "Extracting system binaries...";
            else if (step_progress < 160) step_status = "Registering kernel modules...";
            else step_status = "Finalizing system configuration...";

            if (step_progress >= 250.0f) done = true; 
        }

        // --- Content Rendering ---
        int text_x = x + 30;
        int text_y = y + 75;

        if (current_page == 0) {
            r.draw_string("Welcome to Tiny64", text_x, text_y, COLOR_TINY_BLUE, 2, FONT_BOLD);
            r.draw_string("Thank you for choosing Tiny64 OS.", text_x, text_y + 40, COLOR_BLACK, 1);
            r.draw_string("To begin, please set a username for this device:", text_x, text_y + 65, COLOR_BLACK, 1);
            
            r.draw_rounded_rect(text_x, text_y + 90, 350, 40, 6, 0xFFF5F5F5);
            r.draw_string(username, text_x + 15, text_y + 102, COLOR_BLACK, 1);
            if ((t_global / 20) % 2 == 0) {
                r.fill_rect(text_x + 15 + (cursor * 9), text_y + 102, 2, 16, COLOR_TINY_BLUE);
            }
        } 
        else if (current_page == 1) {
            r.draw_string("Personalize your experience", text_x, text_y, COLOR_TINY_BLUE, 2, FONT_BOLD);
            r.draw_string("Select a theme that matches your style:", text_x, text_y + 40, COLOR_BLACK, 1);
            
            const char* themes[] = {"Modern", "Dark", "Classic"};
            for (int i = 0; i < 3; i++) {
                uint32_t box_col = (selected_theme == i) ? COLOR_TINY_BLUE : 0xFFDDDDDD;
                uint32_t txt_col = (selected_theme == i) ? COLOR_WHITE : 0xFF666666;
                r.draw_rounded_rect(text_x + (i * 120), text_y + 80, 100, 40, 8, box_col);
                r.draw_string(themes[i], text_x + 25 + (i * 120), text_y + 92, txt_col, 1);
            }
        }
        else if (current_page == 2) {
            r.draw_string("Connectivity", text_x, text_y, COLOR_TINY_BLUE, 2, FONT_BOLD);
            r.draw_string("Checking for an active Ethernet connection...", text_x, text_y + 40, COLOR_BLACK, 1);
            
            if (net_status == NET_STATUS_LINK_UP) {
                r.fill_rect(text_x, text_y + 80, 490, 100, 0xFFF0FFF0);
                r.draw_rounded_rect(text_x, text_y + 80, 490, 100, 8, COLOR_TINY_GREEN);
                r.draw_string("Connected Successfully", text_x + 20, text_y + 100, COLOR_TINY_GREEN, 1, FONT_BOLD);
                
                char mac_str[32], ip_str[32];
                ksprintf(mac_str, "MAC: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                ksprintf(ip_str, "IP:  %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
                r.draw_string(mac_str, text_x + 20, text_y + 130, 0xFF666666, 1);
                r.draw_string(ip_str, text_x + 20, text_y + 150, 0xFF666666, 1);
            } else {
                r.draw_string("Looking for hardware...", text_x, text_y + 90, 0xFF888888, 1);
                int spin = (t_global / 5) % 8;
                const char* frames[] = {".", "..", "...", "....", "...", "..", ".", ""};
                r.draw_string(frames[spin], text_x + 200, text_y + 90, COLOR_TINY_BLUE, 1);
            }
        }
        else if (current_page == 3 || current_page == 4 || current_page == 6 || current_page == 7) {
            const char* titles[] = {"Configuring Hardware", "Optimizing Kernel", "", "", "Downloading Assets", "Installing System"};
            r.draw_string(titles[current_page - 3], text_x, text_y, COLOR_TINY_BLUE, 2, FONT_BOLD);
            r.draw_string(step_status, text_x, text_y + 60, COLOR_BLACK, 1);
            
            // Progress Bar for auto steps
            r.fill_rect(text_x, text_y + 100, 490, 20, 0xFFEEEEEE);
            float limit = (current_page == 3) ? 160.0f : (current_page == 4 ? 150.0f : (current_page == 6 ? 200.0f : 250.0f));
            int fw = (int)(step_progress * 490 / limit);
            if (fw > 490) fw = 490;
            r.fill_rect(text_x, text_y + 100, fw, 20, (current_page == 4 || current_page == 7) ? COLOR_TINY_GREEN : COLOR_TINY_BLUE);
            
            r.draw_string("Please do not turn off your computer.", text_x, text_y + 140, 0xFF888888, 1, FONT_ITALIC);
        }
        else if (current_page == 5) {
            r.draw_string("Ready to go!", text_x, text_y, COLOR_TINY_BLUE, 2, FONT_BOLD);
            r.draw_string("We've collected all the information we need.", text_x, text_y + 40, COLOR_BLACK, 1);
            r.draw_string("Click Install to begin the setup process.", text_x, text_y + 65, COLOR_BLACK, 1);
            
            r.draw_string("Username:", text_x + 20, text_y + 110, 0xFF666666, 1, FONT_BOLD);
            r.draw_string(username[0] ? username : "Guest", text_x + 110, text_y + 110, COLOR_BLACK, 1);
            r.draw_string("Theme:   ", text_x + 20, text_y + 135, 0xFF666666, 1, FONT_BOLD);
            const char* themes[] = {"Modern", "Dark", "Classic"};
            r.draw_string(themes[selected_theme], text_x + 110, text_y + 135, COLOR_BLACK, 1);
        }

        // Navigation Buttons
        if (current_page != 3 && current_page != 4 && current_page != 6 && current_page != 7) {
            // Back
            if (current_page > 0 && current_page < 3) {
                r.draw_rounded_rect(x + 20, y + h - 50, 100, 30, 6, 0xFFEEEEEE);
                r.draw_string("Back", x + 48, y + h - 40, 0xFF666666, 1);
            }
            
            // Skip for Network
            if (current_page == 2) {
                r.draw_rounded_rect(x + w - 230, y + h - 50, 100, 30, 6, 0xFFEEEEEE);
                r.draw_string("Skip", x + w - 195, y + h - 40, 0xFF666666, 1);
            }

            // Next / Install
            uint32_t btn_col = (current_page == 5) ? COLOR_TINY_GREEN : COLOR_TINY_BLUE;
            r.draw_rounded_rect(x + w - 120, y + h - 50, 100, 30, 6, btn_col);
            const char* btn_text = (current_page == 5) ? "Install" : "Next";
            r.draw_string(btn_text, x + w - (current_page == 5 ? 100 : 92), y + h - 40, COLOR_WHITE, 1);
        }
        
        r.draw_cursor(mouse_x, mouse_y);

        if (input_keyboard_available()) {
            char c = input_keyboard_getc();
            if (current_page == 0) {
                if (c == 8 && cursor > 0) { cursor--; username[cursor] = 0; } 
                else if (c >= 32 && c <= 126 && cursor < 30) { username[cursor++] = c; username[cursor] = 0; }
            }
            if (c == '\n' && (current_page < 3 || current_page == 5)) {
                current_page++;
                step_progress = 0;
            }
        }
        
        r.show();
        sleep_ms(1);
    }
    
    // --- Transition to OS ---
    r.clear(theme_bg);
    r.draw_rounded_rect(cx - 150, cy - 50, 300, 100, 10, COLOR_WHITE);
    r.draw_string("Finalizing Setup...", cx - 85, cy - 10, COLOR_BLACK, 1);
    r.show();
    sleep_ms(800);
    
    Persistence::SaveSettings(username); 
    Persistence::SaveTheme(selected_theme);
    
    // Smooth transition to "Booting"
    Animation fadeOut(1.0f, 0.0f, 1000, EasingType::EaseInQuad);
    fadeOut.Start(0);
    for (int t = 0; t <= 1000; t += 32) {
        fadeOut.Update(t);
        float alpha = fadeOut.GetValue();
        r.clear(theme_bg);
        // Draw window fading out
        int curW = (int)(w * alpha);
        int curH = (int)(h * alpha);
        if (curW > 10 && curH > 10) {
            r.draw_rounded_rect(cx - curW/2, cy - curH/2, curW, curH, 10, COLOR_WHITE);
        }
        r.show();
        sleep_ms(1);
    }

    // Modern Boot Sequence
    r.clear(COLOR_BLACK);
    for (int i = 0; i <= 100; i += 2) {
        r.clear(COLOR_BLACK);
        
        // Draw a small loading ring or bar
        int barW = 200;
        int barH = 4;
        r.fill_rect(cx - barW/2, cy + 50, barW, barH, 0xFF333333);
        r.fill_rect(cx - barW/2, cy + 50, (barW * i) / 100, barH, COLOR_TINY_GREEN);
        
        r.draw_string("Booting Tiny64...", cx - 70, cy + 20, COLOR_WHITE, 1);
        
        // Optional: show some "kernel logs" style text
        if (i > 10) r.draw_string("> Initializing GUI...", 20, fb->height - 60, 0xFF00FF00, 1);
        if (i > 40) r.draw_string("> Loading Desktop Environment...", 20, fb->height - 40, 0xFF00FF00, 1);
        if (i > 80) r.draw_string("> Starting Session Manager...", 20, fb->height - 20, 0xFF00FF00, 1);

        r.show();
        usb_poll();
        sleep_ms(1);
    }
    sleep_ms(500);
    // Release all wizard heap allocations
    kmalloc_rewind(wizard_ckpt);
}
