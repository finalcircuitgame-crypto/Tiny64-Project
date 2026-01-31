#ifndef VECTOR_FONT_H
#define VECTOR_FONT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int8_t x1, y1, x2, y2;
} stroke_t;

typedef struct {
    const stroke_t* strokes;
    uint8_t count;
} glyph_t;

extern const glyph_t vector_font[128];

#ifdef __cplusplus
}
#endif

#endif
