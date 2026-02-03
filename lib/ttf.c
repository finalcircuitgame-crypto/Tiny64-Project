#include <kernel/mm/heap.h>
#include <lib/string.h>

// Math functions from lib/math.c
double sqrt(double x);
double pow(double x, double y);
double fmod(double x, double y);
double cos(double x);
double acos(double x);
double fabs(double x);
double floor(double x);
double ceil(double x);

#define STBTT_malloc(x,u)  ((void)(u),malloc(x))
#define STBTT_free(x,u)    ((void)(u),free(x))
#define STBTT_assert(x)    ((void)(x))

#define STBTT_memcpy       memcpy
#define STBTT_memset       memset
#define STBTT_strlen       strlen

#define STBTT_sqrt(x)      sqrt(x)
#define STBTT_pow(x,y)     pow(x,y)
#define STBTT_fmod(x,y)    fmod(x,y)
#define STBTT_cos(x)       cos(x)
#define STBTT_acos(x)      acos(x)
#define STBTT_fabs(x)      fabs(x)
#define STBTT_ifloor(x)    ((int)floor(x))
#define STBTT_iceil(x)     ((int)ceil(x))

#define STB_TRUETYPE_IMPLEMENTATION
#include <lib/ttf.h>
#include <kernel/serial.h>

int ttf_init(ttf_font *font, uint8_t *data) {
    font->data = data;
    if (!stbtt_InitFont(&font->info, data, 0)) {
        return 0;
    }
    return 1;
}

int ttf_find_glyph(ttf_font *font, int unicode) {
    return stbtt_FindGlyphIndex(&font->info, unicode);
}

void ttf_get_glyph_h_metrics(ttf_font *font, int glyph_index, int *advance_width, int *left_side_bearing) {
    stbtt_GetGlyphHMetrics(&font->info, glyph_index, advance_width, left_side_bearing);
}

void ttf_get_font_v_metrics(ttf_font *font, int *ascent, int *descent, int *line_gap) {
    stbtt_GetFontVMetrics(&font->info, ascent, descent, line_gap);
}

int ttf_get_glyph_box(ttf_font *font, int glyph_index, int *x0, int *y0, int *x1, int *y1) {
    return stbtt_GetGlyphBox(&font->info, glyph_index, x0, y0, x1, y1);
}

float ttf_get_string_width(ttf_font *font, const char *s, float scale) {
    float width = 0;
    while (*s) {
        int glyph = stbtt_FindGlyphIndex(&font->info, *s);
        if (glyph > 0) {
            int advance, lsb;
            stbtt_GetGlyphHMetrics(&font->info, glyph, &advance, &lsb);
            width += advance * scale;
        }
        s++;
    }
    return width;
}

void ttf_render_glyph(ttf_font *font, int glyph_index, float x_pos, float y_pos, float scale, ttf_draw_callback callback, void *userdata) {
    int x0, y0, x1, y1;
    // Use subpixel version and add a bit of padding to avoid rounding cut-offs
    float x_shift = x_pos - (float)((int)x_pos);
    float y_shift = y_pos - (float)((int)y_pos);
    
    stbtt_GetGlyphBitmapBoxSubpixel(&font->info, glyph_index, scale, scale, x_shift, y_shift, &x0, &y0, &x1, &y1);
    
    // Add 1 pixel padding to ensure we don't cut off any anti-aliased edges
    x1++; y1++;
    
    int width = x1 - x0;
    int height = y1 - y0;
    
    if (width <= 0 || height <= 0) return;
    
    static uint8_t bitmap[128 * 128]; // Max 128x128 glyphs
    if (width > 128 || height > 128) return;
    
    memset(bitmap, 0, width * height);
    stbtt_MakeGlyphBitmapSubpixel(&font->info, bitmap, width, height, width, scale, scale, x_shift, y_shift, glyph_index);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Lower threshold to 20 to capture more of the anti-aliased shape
            if (bitmap[y * width + x] > 20) {
                callback((int)((int)x_pos + x0 + x), (int)((int)y_pos + y0 + y), userdata);
            }
        }
    }
}