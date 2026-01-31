#ifndef DESKTOP_H
#define DESKTOP_H

#include "../graphics/renderer.h"
#include "../../c/graphics/framebuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

void desktop_loop(Framebuffer* fb);

#ifdef __cplusplus
}
#endif

#endif
