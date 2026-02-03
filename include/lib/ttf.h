#ifndef TTF_H
#define TTF_H

#include <stdint.h>
#include <stddef.h>
#include <kernel/stb_truetype.h>

typedef struct {
    stbtt_fontinfo info;
    uint8_t *data;
} ttf_font;

int ttf_init(ttf_font *font, uint8_t *data);
int ttf_find_glyph(ttf_font *font, int unicode);
void ttf_get_glyph_h_metrics(ttf_font *font, int glyph_index, int *advance_width, int *left_side_bearing);
void ttf_get_font_v_metrics(ttf_font *font, int *ascent, int *descent, int *line_gap);
int ttf_get_glyph_box(ttf_font *font, int glyph_index, int *x0, int *y0, int *x1, int *y1);
float ttf_get_string_width(ttf_font *font, const char *s, float scale);

typedef void (*ttf_draw_callback)(int x, int y, void *userdata);
void ttf_render_glyph(ttf_font *font, int glyph_index, float x_pos, float y_pos, float scale, ttf_draw_callback callback, void *userdata);

#endif