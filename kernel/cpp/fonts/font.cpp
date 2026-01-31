#include "font.h"
#include "../../c/graphics/font8x16.h"
#include <stdint.h>
#include <string.h>

void get_font_glyph(char c, uint8_t* output) {
    const uint8_t* src;
    uint8_t index = (uint8_t)c;
    
    // Bounds check for our subset
    if (index >= 128) {
        index = '?';
    }

    src = font8x16_basic[index];
    
    // Copy the glyph data to output (16 bytes)
    for (int i = 0; i < 16; i++) {
        output[i] = src[i];
    }
}
