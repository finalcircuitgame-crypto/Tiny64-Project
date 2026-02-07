#include <stdint.h>
#include "../include/bootinfo.h"

// Embedded font symbols from objcopy
extern uint8_t _binary_CGA_F08_start[];
extern uint8_t _binary_CGA_F08_end[];

static BootInfo *g_BootInfo;
static uint32_t g_CursorX = 0;
static uint32_t g_CursorY = 0;

void ConsoleInit(BootInfo *bootInfo) {
    g_BootInfo = bootInfo;
}

void PutChar(char c, uint32_t color) {
    if (c == '\n') {
        g_CursorX = 0;
        g_CursorY += 8;
        return;
    }

    uint32_t *fb = g_BootInfo->framebuffer;
    uint8_t *glyph = &(_binary_CGA_F08_start[(uint8_t)c * 8]);

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if ((glyph[y] >> (7 - x)) & 1) {
                fb[(g_CursorY + y) * g_BootInfo->pitch + (g_CursorX + x)] = color;
            }
        }
    }

    g_CursorX += 8;
    if (g_CursorX + 8 > g_BootInfo->width) {
        g_CursorX = 0;
        g_CursorY += 8;
    }
}

void PrintString(const char *str, uint32_t color) {
    while (*str) {
        PutChar(*str++, color);
    }
}