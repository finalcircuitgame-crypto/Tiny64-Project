#ifndef SERIAL_TERMINAL_H
#define SERIAL_TERMINAL_H

#include "../graphics/framebuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

void serial_terminal_init(Framebuffer* fb);
void serial_terminal_putc(char c);
void serial_terminal_render();
void serial_terminal_stop();

#ifdef __cplusplus
}
#endif

#endif
