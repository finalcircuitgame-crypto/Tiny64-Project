#ifndef SPLASH_SCREEN_H
#define SPLASH_SCREEN_H

#include "../../c/graphics/framebuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

void draw_splash(Framebuffer* fb);
void update_splash(Framebuffer* fb, int progress, const char* message);
void report_error(Framebuffer* fb, const char* error_msg);

#ifdef __cplusplus
}
#endif

#endif
