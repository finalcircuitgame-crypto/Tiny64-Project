#include "panic.h"
#include "../graphics/draw.h"
#include "../graphics/font.h"
#include "../graphics/framebuffer.h"
#include "../graphics/colors.h"
#include "../drivers/platform/serial.h"
#include "../drivers/hid/input.h"
#include "../lib/string.h"
#include <stdint.h>

extern Framebuffer* g_kernel_fb;

static void halt_cpu() {
    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}

static void itoa_hex(uint64_t n, char* str) {
    const char* hex = "0123456789ABCDEF";
    for (int i = 15; i >= 0; i--) {
        str[i] = hex[n & 0xF];
        n >>= 4;
    }
    str[16] = 0;
}

void panic_with_regs(const char *msg, const char *file, int line, registers_t* regs) {
    __asm__ volatile ("cli");
    serial_write_str("\r\n!!! CRITICAL SYSTEM ERROR !!!\r\n");
    serial_write_str("Message: "); serial_write_str(msg); serial_write_str("\r\n");
    serial_write_str("Location: "); serial_write_str(file); serial_write_str("\r\n");

    if (g_kernel_fb) {
        clear_screen(g_kernel_fb, COLOR_TINY_BLUE); 
        
        // Large :( face or Error Icon
        draw_string(g_kernel_fb, ":(", 50, 50, COLOR_WHITE); // Needs scaling really, but let's stick to simple strings
        
        draw_string(g_kernel_fb, "TINY64 OS ran into a problem and needs to restart.", 50, 150, COLOR_WHITE);
        draw_string(g_kernel_fb, "We're just collecting some error info, and then you can enter Recovery Mode.", 50, 180, COLOR_WHITE);

        char buf[128];
        ksprintf(buf, "Stop Code: %s", msg);
        draw_string(g_kernel_fb, buf, 50, 250, COLOR_WHITE);
        
        ksprintf(buf, "File: %s | Line: %d", file, line);
        draw_string(g_kernel_fb, buf, 50, 280, 0xFFDDDDDD);

        if (regs) {
            draw_string(g_kernel_fb, "Registers:", 50, 320, COLOR_WHITE);
            char reg_buf[64];
            ksprintf(reg_buf, "RAX: %x", regs->rax); draw_string(g_kernel_fb, reg_buf, 50, 350, COLOR_WHITE);
            ksprintf(reg_buf, "RIP: %x", regs->rip); draw_string(g_kernel_fb, reg_buf, 250, 350, COLOR_YELLOW);
        }

        draw_string(g_kernel_fb, "Press [R] to jump to Recovery Kernel / Mode", 50, 450, COLOR_WHITE);
    }

    // Wait for 'R' key
    while (1) {
        if (input_keyboard_available()) {
            char c = input_keyboard_getc();
            if (c == 'r' || c == 'R') {
                serial_write_str("Panic: User requested Recovery Mode.\r\n");
                // Reset or Jump to Recovery
                // For now, let's just jump to a recovery function if it existed
                // but since we are in C, we'll try to re-enter kmain with a flag
                // Or just call a recovery loop
                extern void start_recovery_mode(void* fb);
                start_recovery_mode(g_kernel_fb);
            }
        }
        for(volatile int i=0; i<10000; i++);
    }
}

void panic(const char *msg, const char *file, int line) {
    panic_with_regs(msg, file, line, 0);
}
