#include "recovery.h"
#include "../graphics/renderer.h"
#include "../../c/graphics/framebuffer.h"
#include "../../c/graphics/colors.h"
#include "../../c/drivers/hid/input.h"
#include "../../c/drivers/usb/usb.h"
#include "../../c/drivers/platform/serial.h"
#include "../utils/persistence.h"

extern "C" void start_recovery_mode(void* fb_ptr) {
    Framebuffer* fb = (Framebuffer*)fb_ptr;
    Renderer r(fb);
    
    serial_write_str("Recovery: Entering Recovery Mode (BSOD Style)...\r\n");

    int mouse_x = fb->width / 2;
    int mouse_y = fb->height / 2;
    input_mouse_state_t ms;

    bool running = true;
    while (running) {
        usb_poll();
        input_mouse_get_state(&ms);
        
        if (ms.has_abs) {
            mouse_x = (ms.x_abs * fb->width) / 32768;
            mouse_y = (ms.y_abs * fb->height) / 32768;
        } else {
            mouse_x += ms.x_diff;
            mouse_y += ms.y_diff;
        }
        if (mouse_x < 0) mouse_x = 0; if (mouse_x >= (int)fb->width) mouse_x = fb->width - 1;
        if (mouse_y < 0) mouse_y = 0; if (mouse_y >= (int)fb->height) mouse_y = fb->height - 1;

        r.clear(COLOR_TINY_BLUE); // Tiny64 Blue BSOD
        
        // BSOD Elements
        r.draw_string(":(", 80, 80, COLOR_WHITE, 10);
        r.draw_string("TINY64 RECOVERY", 80, 250, COLOR_WHITE, 3);
        
        r.draw_string("Your system has encountered a critical corruption and needs repair.", 80, 310, COLOR_WHITE, 1);
        r.draw_string("We're providing you with some tools to fix this.", 80, 330, COLOR_WHITE, 1);

        // Menu area
        r.fill_rect(80, 360, 600, 2, 0x44FFFFFF);

        // Options
        const char* options[] = {
            "Reset System Settings (Clear persistence)",
            "Re-initialize RAMFS (Wipe files)",
            "Reboot System",
            "Exit & Continue to Tiny64"
        };

        for (int i = 0; i < 4; i++) {
            int rect_y = 380 + (i * 50);
            uint32_t box_col = 0x22FFFFFF;
            
            // Hover effect
            if (mouse_x >= 80 && mouse_x <= 580 && mouse_y >= rect_y && mouse_y <= rect_y + 40) {
                box_col = 0x44FFFFFF;
                if (ms.buttons & 1) {
                    // Click logic
                    if (i == 0) {
                        Persistence::SaveSettings("DefaultUser");
                        Persistence::SaveTheme(0);
                        r.draw_string("Settings Reset!", 600, rect_y + 12, COLOR_TINY_GREEN, 1);
                    }
                    if (i == 2) {
                        __asm__ volatile ("outb %%al, %%dx" : : "a"(0xFE), "d"(0x64));
                    }
                    if (i == 3) running = false;
                }
            }

            r.draw_rounded_rect(80, rect_y, 500, 40, 5, box_col);
            r.draw_string(options[i], 100, rect_y + 12, COLOR_WHITE, 1);
        }
        
        r.draw_string("Stop Code: TINY64_BOOT_FAILURE", 80, 600, 0x88FFFFFF, 1);
        r.draw_string("Support: https://tiny64.org/support", 80, 620, 0x88FFFFFF, 1);

        r.draw_cursor(mouse_x, mouse_y);
        r.show();

        if (input_keyboard_available()) {
            char c = input_keyboard_getc();
            if (c == 27 || c == 'q') running = false;
        }
    }
    
    serial_write_str("Recovery: Exiting to Normal Mode...\r\n");
}
