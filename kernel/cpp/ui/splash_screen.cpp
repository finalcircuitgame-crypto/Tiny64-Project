#include "splash_screen.h"
#include "../graphics/renderer.h"
#include "../graphics/animation.h"
#include "../../c/graphics/colors.h"
#include "../../c/graphics/logo_data.h"
#include "../../c/drivers/platform/serial.h"
#include "../../c/lib/string.h"
#include "../utils/hardware.h"

extern "C" void draw_logo(Framebuffer *fb, int cx, int cy);

// Simple busy wait for timing (temporary)
static void sleep_ms(int ms)
{
    for (volatile int i = 0; i < ms * 10000; i++)
        ;
}

// Global animation state
static int current_progress = 0;
static const char *current_message = "";
static const char *last_message = "";

extern "C" void draw_splash(Framebuffer *fb)
{
    serial_write_str("Splash: Starting animation...\r\n");
    Renderer r(fb);
    r.clear(COLOR_BLACK);

    int cx = fb->width / 2;
    int cy = fb->height / 2;

    // Hardware Info
    Hardware::CPUInfo cpu = Hardware::GetCPUInfo();
    Hardware::GPUInfo gpu = Hardware::GetGPUInfo();

    Animation scaleAnim(0.1f, 1.0f, 2000, EasingType::EaseOutBounce);
    scaleAnim.Start(0);

    for (int t = 0; t <= 1000; t += 16) // Shorter intro
    {
        scaleAnim.Update(t);
        r.clear(COLOR_BLACK);
        float scale = scaleAnim.GetValue();
        if (scale < 1.0f)
        {
            int size = (int)(200 * scale);
            r.draw_rounded_rect(cx - size / 2, cy - size / 2, size, size, 20, COLOR_TINY_BLUE);
        }
        r.show();
        sleep_ms(1);
    }

    r.clear(COLOR_BLACK);
    draw_logo(fb, cx, cy);

    r.draw_string("Tiny64", cx - 24, cy + 60, COLOR_WHITE, 2);
    
    // Show Hardware Info
    r.draw_string(cpu.brand, cx - (strlen(cpu.brand) * 8) / 2, cy + 90, COLOR_TINY_GREY, 1);
    
    char hw_buf[64];
    strcpy(hw_buf, "GPU: ");
    strcat(hw_buf, gpu.name);
    strcat(hw_buf, " | Power: AC");
    r.draw_string(hw_buf, cx - (strlen(hw_buf) * 8) / 2, cy + 105, COLOR_TINY_GREY, 1);

    r.draw_rounded_rect(cx - 150, cy + 130, 300, 10, 5, 0xFF333333);
    r.show();

    // Simulated slow boot sequence (reduced wait for better UX)
    update_splash(fb, 10, "Loading System Files...");
    sleep_ms(500);
    update_splash(fb, 25, "Initializing Kernel...");
    sleep_ms(500);
    update_splash(fb, 45, "Loading Drivers (xHCI, NVMe, AHCI)...");
    sleep_ms(1000);
    update_splash(fb, 70, "Starting System Services...");
    sleep_ms(500);
    update_splash(fb, 90, "Preparing Desktop Environment...");
    sleep_ms(500);
    update_splash(fb, 100, "Boot Complete.");
    sleep_ms(500);

    serial_write_str("Splash: Animation finished.\r\n");
}

extern "C" void update_splash(Framebuffer *fb, int target_progress, const char *message)
{
    serial_write_str("Splash Update: ");
    serial_write_str(message);
    serial_write_str("\r\n");

    last_message = message;

    Renderer r(fb);
    int cx = fb->width / 2;
    int cy = fb->height / 2;

    Animation progressAnim((float)current_progress, (float)target_progress, 500, EasingType::EaseOutQuad);
    progressAnim.Start(0);

    for (int t = 0; t <= 500; t += 16)
    {
        progressAnim.Update(t);
        int val = (int)progressAnim.GetValue();

        r.clear(COLOR_BLACK);
        draw_logo(fb, cx, cy);
        r.draw_string("Tiny64", cx - 24, cy + 60, COLOR_WHITE, 2);

        r.fill_rect(cx - 150, cy + 130, 300, 10, 0xFF333333);

        int barWidth = (int)(300 * (val / 100.0f));
        r.draw_rounded_rect(cx - 150, cy + 130, barWidth, 10, 5, COLOR_TINY_GREEN);

        int msgLen = 0;
        while (message[msgLen])
            msgLen++;
        r.draw_string(message, cx - (msgLen * 8) / 2, cy + 155, COLOR_WHITE, 1);

        r.show();
        sleep_ms(1);
    }

    current_progress = target_progress;
    current_message = message;
}

extern "C" void report_error(Framebuffer *fb, const char *error_msg)
{
    serial_write_str("ERROR: ");
    serial_write_str(error_msg);
    serial_write_str("\r\n");

    Renderer r(fb);
    r.clear(COLOR_BLACK);
    
    int cx = fb->width / 2;
    int cy = fb->height / 2;

    r.draw_string("BOOT FAILURE", cx - (12 * 16) / 2, cy - 60, COLOR_RED, 2);
    
    int len = 0;
    while(error_msg[len]) len++;
    r.draw_string(error_msg, cx - (len * 8) / 2, cy, COLOR_WHITE, 1);
    
    r.draw_string("Press any key to halt...", cx - (24 * 8) / 2, cy + 60, COLOR_TINY_GREY, 1);
    r.show();

    while(1) {
        if (serial_received()) break;
        // Potential for keyboard wait here if drivers work
        __asm__ volatile("hlt");
    }
}
