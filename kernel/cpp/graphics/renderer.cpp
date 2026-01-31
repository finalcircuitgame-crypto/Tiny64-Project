#include "renderer.h"
#include "../../c/drivers/platform/serial.h"
#include "../../c/graphics/font8x16.h"
#include "../../c/graphics/ttf.h"
#include "../../c/lib/string.h"
#include "../../c/memory/heap.h"
#include <stdint.h>

static uint32_t *g_back_buffer = nullptr;
static size_t g_back_buffer_size = 0;

Renderer::Renderer(Framebuffer *fb) : framebuffer(fb) {
  size_t needed_size = fb->height * fb->pitch;

  if (!g_back_buffer || g_back_buffer_size < needed_size) {
    if (g_back_buffer) {
        kfree(g_back_buffer);
    }
    g_back_buffer = (uint32_t *)kmalloc(needed_size);
    if (g_back_buffer) {
      g_back_buffer_size = needed_size;
      serial_write_str("Renderer: Allocated global back buffer at ");
      serial_write_hex((uint64_t)g_back_buffer);
      serial_write_str("\r\n");
    } else {
      serial_write_str("Renderer: Failed to allocate back buffer!\r\n");
    }
  }

  back_buffer = g_back_buffer;

  if (back_buffer && framebuffer->base_address) {
    memcpy(back_buffer, (void *)framebuffer->base_address, needed_size);
  }
}

Renderer::~Renderer() {
  // We don't free the global back buffer
}

void Renderer::show() {
  if (back_buffer) {
    uint8_t *dest = (uint8_t *)framebuffer->base_address;
    uint8_t *src = (uint8_t *)back_buffer;
    size_t size = framebuffer->height * framebuffer->pitch;
    
    // Hardware accelerated copy using SSE (if size is multiple of 16)
    size_t i = 0;
    for (; i + 15 < size; i += 16) {
        __asm__ volatile (
            "movups (%0), %%xmm0\n"
            "movntps %%xmm0, (%1)\n"
            : : "r"(src + i), "r"(dest + i) : "memory", "xmm0"
        );
    }
    // Copy remaining bytes
    for (; i < size; i++) {
        dest[i] = src[i];
    }
  }
}

void Renderer::clear(uint32_t color) {
  if (back_buffer) {
    uint64_t *pixels = (uint64_t *)back_buffer;
    uint32_t count = (framebuffer->pitch / 4) * framebuffer->height;
    uint64_t color64 = ((uint64_t)color << 32) | color;
    for (uint32_t i = 0; i < count / 2; i++) {
      pixels[i] = color64;
    }
    if (count % 2) {
        ((uint32_t*)pixels)[count-1] = color;
    }
  } else {
    uint64_t *pixels = (uint64_t *)framebuffer->base_address;
    uint32_t count = (framebuffer->pitch / 4) * framebuffer->height;
    uint64_t color64 = ((uint64_t)color << 32) | color;
    for (uint32_t i = 0; i < count / 2; i++) {
      pixels[i] = color64;
    }
    if (count % 2) {
        ((uint32_t*)pixels)[count-1] = color;
    }
  }
}

void Renderer::put_pixel(int x, int y, uint32_t color) {
  if (x < 0 || x >= (int)framebuffer->width || y < 0 ||
      y >= (int)framebuffer->height)
    return;

  uint32_t alpha = (color >> 24) & 0xFF;
  if (alpha == 255) {
    uint32_t *target;
    if (back_buffer) {
      target = &back_buffer[y * (framebuffer->pitch / 4) + x];
    } else {
      target =
          &((uint32_t *)
                framebuffer->base_address)[y * (framebuffer->pitch / 4) + x];
    }
    *target = color;
    return;
  }

  if (alpha == 0)
    return;

  uint32_t *target;
  uint32_t bg;

  if (back_buffer) {
    target = &back_buffer[y * (framebuffer->pitch / 4) + x];
    bg = *target;
  } else {
    target = &((uint32_t *)
                   framebuffer->base_address)[y * (framebuffer->pitch / 4) + x];
    bg = *target;
  }

  *target = blend(bg, color);
}

uint32_t Renderer::blend(uint32_t bg, uint32_t fg) {
  uint32_t alpha = (fg >> 24) & 0xFF;
  uint32_t inv_alpha = 255 - alpha;

  // Faster blending using bitwise operations to avoid / 255
  uint32_t rb = (bg & 0xFF00FF) * inv_alpha;
  uint32_t g = (bg & 0x00FF00) * inv_alpha;
  rb += (fg & 0xFF00FF) * alpha;
  g += (fg & 0x00FF00) * alpha;

  return 0xFF000000 | ((rb >> 8) & 0xFF00FF) | ((g >> 8) & 0x00FF00);
}

void Renderer::fill_rect(int x, int y, int w, int h, uint32_t color) {
  // Fast path for opaque rectangles
  if (((color >> 24) & 0xFF) == 255) {
    uint64_t color64 = ((uint64_t)color << 32) | color;
    for (int i = 0; i < h; i++) {
      int cur_y = y + i;
      if (cur_y < 0 || cur_y >= (int)framebuffer->height)
        continue;
      uint32_t *row;
      if (back_buffer) {
        row = &back_buffer[cur_y * (framebuffer->pitch / 4)];
      } else {
        row = &((uint32_t *)framebuffer->base_address)[cur_y * (framebuffer->pitch / 4)];
      }
      
      int j = 0;
      // Align to 8 bytes if needed
      if (((uintptr_t)&row[x]) & 7) {
        if (x >= 0 && x < (int)framebuffer->width) {
            row[x] = color;
            j++;
        }
      }
      
      for (; j < w - 1; j += 2) {
        int cur_x = x + j;
        if (cur_x < 0 || cur_x + 1 >= (int)framebuffer->width) {
            // Fallback for edge cases
            if (cur_x >= 0 && cur_x < (int)framebuffer->width) row[cur_x] = color;
            if (cur_x + 1 >= 0 && cur_x + 1 < (int)framebuffer->width) row[cur_x+1] = color;
            continue;
        }
        *(uint64_t*)&row[cur_x] = color64;
      }
      for (; j < w; j++) {
        int cur_x = x + j;
        if (cur_x >= 0 && cur_x < (int)framebuffer->width) {
            row[cur_x] = color;
        }
      }
    }
    return;
  }

  for (int i = 0; i < h; i++) {
    for (int j = 0; j < w; j++) {
      put_pixel(x + j, y + i, color);
    }
  }
}

void Renderer::draw_rounded_rect(int x, int y, int w, int h, int r,
                                 uint32_t color) {
  if (r <= 0) {
    fill_rect(x, y, w, h, color);
    return;
  }

  // Optimization: Draw the 3 central rectangles first
  fill_rect(x + r, y, w - 2 * r, h, color);         // Middle vertical
  fill_rect(x, y + r, r, h - 2 * r, color);         // Left middle
  fill_rect(x + w - r, y + r, r, h - 2 * r, color); // Right middle

  // Draw corners
  for (int i = 0; i < r; i++) {
    for (int j = 0; j < r; j++) {
      int dx = r - j;
      int dy = r - i;
      if (dx * dx + dy * dy <= r * r) {
        put_pixel(x + j, y + i, color);                 // Top Left
        put_pixel(x + w - j - 1, y + i, color);         // Top Right
        put_pixel(x + j, y + h - i - 1, color);         // Bottom Left
        put_pixel(x + w - j - 1, y + h - i - 1, color); // Bottom Right
      }
    }
  }
}

void Renderer::draw_cursor(int x, int y) {
  static const char *cursor_map[] = {
      "#               ", "##              ", "#.#             ",
      "#..#            ", "#...#           ", "#....#          ",
      "#.....#         ", "#......#        ", "#.......#       ",
      "#........#      ", "#.....#####     ", "#..#..#         ",
      "#.# #..#        ", "##   #..#       ", "#     #..#      ",
      "       #..#     ", "        ##      "};

  uint32_t white = 0xFFFFFFFF;
  uint32_t black = 0xFF000000;
  uint32_t shadow = 0x55000000; // translucent dark shadow

  // Draw shadow first (offset by +2, +2)
  for (int i = 0; i < 17; i++) {
    for (int j = 0; j < 16; j++) {
      char c = cursor_map[i][j];
      if (c == '#' || c == '.') {
        put_pixel(x + j + 2, y + i + 2, shadow);
      }
    }
  }

  // Draw cursor on top
  for (int i = 0; i < 17; i++) {
    for (int j = 0; j < 16; j++) {
      char c = cursor_map[i][j];
      if (c == '#') {
        put_pixel(x + j, y + i, black);
      } else if (c == '.') {
        put_pixel(x + j, y + i, white);
      }
    }
  }
}

static inline int abs(int n) { return n < 0 ? -n : n; }

void Renderer::draw_char(char c, int x, int y, uint32_t color, int scale) {
  if (c < 0 || (uint8_t)c >= 128)
    return;
  const uint8_t *glyph = font8x16_basic[(uint8_t)c];

  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 8; j++) {
      if (glyph[i] & (1 << (7 - j))) {
        if (scale == 1) {
          put_pixel(x + j, y + i, color);
        } else {
          fill_rect(x + j * scale, y + i * scale, scale, scale, color);
        }
      }
    }
  }
}

void Renderer::draw_string(const char *str, int x, int y, uint32_t color,
                           int scale, font_style_t style) {
  if (scale == 1) {
      draw_string_ttf(str, x, y, color, 16.0f, style);
      return;
  }
  for (int i = 0; str[i]; i++) {
    draw_char(str[i], x + i * 8 * scale, y, color, scale);
  }
}

void Renderer::draw_string_ttf(const char* str, int x, int y, uint32_t color, float size, font_style_t style) {
    if (!back_buffer) return;
    
    // Create a temporary framebuffer object for the ttf renderer
    Framebuffer tmp_fb = *framebuffer;
    tmp_fb.base_address = (uint64_t)back_buffer;
    
    ttf_draw_string(&tmp_fb, x, y, str, color, size, style);
}

void Renderer::draw_image(const uint32_t *data, int x, int y, int w, int h) {
  if (!data)
    return;
  for (int i = 0; i < h; i++) {
    for (int j = 0; j < w; j++) {
      uint32_t color = data[i * w + j];
      if (((color >> 24) & 0xFF) > 0) {
          put_pixel(x + j, y + i, color);
      }
    }
  }
}
