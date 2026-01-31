#ifndef SHELL_H
#define SHELL_H

#include "../graphics/framebuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

void draw_splash(Framebuffer* fb);
void update_splash(Framebuffer* fb, int percent, const char* status);
void terminal_loop(Framebuffer* fb);
void desktop_loop(Framebuffer* fb);
void shell_loop(void);
int load_autoboot_setting();

#ifdef __cplusplus
}
#endif

#endif
