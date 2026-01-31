#include "shell.h"
#include "commands.h"
#include "../drivers/platform/serial.h"
#include "../drivers/platform/rtc.h"
#include "../lib/string.h"
#include "../memory/pmm.h"
#include "../scheduler/scheduler.h"
#include "../drivers/pci/pci.h"
#include "../drivers/usb/xhci/xhci.h"
#include "../panic/panic.h"
#include "../drivers/hid/input.h"

extern xhci_controller_t xhc;

static void shell_output(const char* text, uint32_t color) {
    (void)color; // Serial doesn't use color for now
    if (strcmp(text, "__CLEAR__") == 0) {
        serial_write_str("\033[2J\033[H");
        return;
    }
    serial_write_str(text);
    serial_write_str("\r\n");
}

static char g_cmd_buffer[256];
static int g_cmd_ptr = 0;
static int g_shell_running = 1;

static void shell_prompt() {
    serial_write_str("Tiny64> ");
}

static void shell_exec(const char* cmd) {
    if (strcmp(cmd, "exit") == 0) {
        serial_write_str("Exiting serial shell...\r\n");
        g_shell_running = 0;
        return;
    }
    commands_execute(cmd, shell_output);
}

void shell_loop(void) {
    serial_write_str("\r\nTiny64 Serial Shell\r\nType 'help' for commands.\r\n");
    shell_prompt();

    g_cmd_ptr = 0;
    g_cmd_buffer[0] = '\0';
    g_shell_running = 1;

    while (g_shell_running) {
        if (input_keyboard_available()) {
            uint32_t e = input_keyboard_get_event();
            char c = (char)(e & 0xFF);
            if (e & 0xFF000000) continue; // Ignore modifiers

            if (c == '\r' || c == '\n') {
                serial_write_str("\r\n");
                if (g_cmd_ptr > 0) {
                    shell_exec(g_cmd_buffer);
                    g_cmd_ptr = 0;
                    g_cmd_buffer[0] = '\0';
                }
                if (g_shell_running) shell_prompt();
            } else if (c == '\b' || c == 0x7F) {
                if (g_cmd_ptr > 0) {
                    g_cmd_buffer[--g_cmd_ptr] = '\0';
                    serial_write_str("\b \b");
                }
            } else if (c >= 32 && c <= 126) {
                if (g_cmd_ptr < (int)sizeof(g_cmd_buffer) - 1) {
                    g_cmd_buffer[g_cmd_ptr++] = c;
                    g_cmd_buffer[g_cmd_ptr] = '\0';
                    serial_putc(c);
                }
            }
        }
        
        __asm__ volatile("pause");
    }
}
