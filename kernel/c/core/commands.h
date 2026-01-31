#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Function pointer for command output
// This allows the shared command logic to work with serial or GUI terminal
typedef void (*cmd_output_fn)(const char* text, uint32_t color);

// Standard colors (matching those used in terminal.cpp)
#define CMD_COLOR_WHITE  0xFFFFFFFF
#define CMD_COLOR_RED    0xFFFF0000
#define CMD_COLOR_GREEN  0xFF00FF00
#define CMD_COLOR_BLUE   0xFF0000FF
#define CMD_COLOR_GREY   0xFFAAAAAA
#define CMD_COLOR_CYAN   0xFF00FFFF
#define CMD_COLOR_YELLOW 0xFFFFFF00

void commands_init();
void commands_execute(const char* cmd, cmd_output_fn output);

#ifdef __cplusplus
}
#endif

#endif
