#include "ttf.h"
#include "../lib/math.h"
#include "../lib/string.h"
#include "../memory/heap.h"
#include "font_data.h"

// STBTT Configuration
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_malloc(x,u)  kmalloc(x)
#define STBTT_free(x,u)    kfree(x)
#define STBTT_sqrt(x)      sqrt(x)
#define STBTT_pow(x,y)     0 // Not strictly needed for simple rendering
#define STBTT_fabs(x)      fabs(x)
#define STBTT_floor(x)     floor(x)
#define STBTT_ceil(x)      ceil(x)
#define STBTT_assert(x)    

#include "../lib/stb_truetype.h"

static stbtt_fontinfo font_infos[FONT_MAX];
static uint8_t char_bitmap_buffer[256 * 256];

void ttf_init() {
    // Regular
    stbtt_InitFont(&font_infos[FONT_REGULAR], font_ttf_data, 0);
    
    // Bold
    stbtt_InitFont(&font_infos[FONT_BOLD], font_bold_data, 0);

    // Italic
    stbtt_InitFont(&font_infos[FONT_ITALIC], font_italic_data, 0);

    // Bold Italic
    stbtt_InitFont(&font_infos[FONT_BOLD_ITALIC], font_bold_italic_data, 0);

    // Roboto Regular
    stbtt_InitFont(&font_infos[FONT_ROBOTO_REGULAR], font_roboto_regular_data, 0);

    // Roboto Bold
    stbtt_InitFont(&font_infos[FONT_ROBOTO_BOLD], font_roboto_bold_data, 0);

    // Roboto Italic
    stbtt_InitFont(&font_infos[FONT_ROBOTO_ITALIC], font_roboto_italic_data, 0);

    // Roboto Bold Italic
    stbtt_InitFont(&font_infos[FONT_ROBOTO_BOLD_ITALIC], font_roboto_bold_italic_data, 0);
}

static uint32_t blend_alpha(uint32_t bg, uint32_t fg, uint8_t alpha) {
    if (alpha == 0) return bg;
    if (alpha == 255) return fg;

    // Fast blending using bitwise operations
    uint32_t inv_alpha = 255 - alpha;
    uint32_t rb = (bg & 0xFF00FF) * inv_alpha;
    uint32_t g = (bg & 0x00FF00) * inv_alpha;
    rb += (fg & 0xFF00FF) * alpha;
    g += (fg & 0x00FF00) * alpha;

    return 0xFF000000 | ((rb >> 8) & 0xFF00FF) | ((g >> 8) & 0x00FF00);
}

void ttf_draw_string(Framebuffer* fb, int x, int y, const char* str, uint32_t color, float size, font_style_t style) {
    if (style >= FONT_MAX) style = FONT_REGULAR;
    stbtt_fontinfo* info = &font_infos[style];
    
    float scale = stbtt_ScaleForPixelHeight(info, size);
    
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(info, &ascent, &descent, &lineGap);
    
    int baseline = (int)(ascent * scale);
    
    int cur_x = x;
    for (int i = 0; str[i]; i++) {
        int advance, lsb;
        stbtt_GetCodepointHMetrics(info, str[i], &advance, &lsb);
        
        int x1, y1, x2, y2;
        stbtt_GetCodepointBitmapBox(info, str[i], scale, scale, &x1, &y1, &x2, &y2);
        
        int out_x = cur_x + (int)(lsb * scale);
        int out_y = y + baseline + y1;
        
        int w = x2 - x1;
        int h = y2 - y1;
        
        if (w > 0 && h > 0) {
            if (w <= 256 && h <= 256) {
                stbtt_MakeCodepointBitmap(info, char_bitmap_buffer, w, h, w, scale, scale, str[i]);
                
                uint32_t* pixels = (uint32_t*)fb->base_address;
                for (int j = 0; j < h; j++) {
                    for (int k = 0; k < w; k++) {
                        int px = out_x + k;
                        int py = out_y + j;
                        if (px >= 0 && px < (int)fb->width && py >= 0 && py < (int)fb->height) {
                            uint8_t alpha = char_bitmap_buffer[j * w + k];
                            if (alpha > 0) {
                                uint32_t bg = pixels[py * (fb->pitch / 4) + px];
                                pixels[py * (fb->pitch / 4) + px] = blend_alpha(bg, color, alpha);
                            }
                        }
                    }
                }
            }
        }
        
        cur_x += (int)(advance * scale);
        
        // Kerning
        if (str[i+1]) {
            cur_x += (int)(stbtt_GetCodepointKernAdvance(info, str[i], str[i+1]) * scale);
        }
    }
}
