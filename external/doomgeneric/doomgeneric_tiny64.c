#include "doomgeneric.h"
#include "../../kernel/c/graphics/framebuffer.h"
#include "../../kernel/c/scheduler/scheduler.h"
#include "../../kernel/c/drivers/hid/input.h"
#include "../../kernel/c/drivers/platform/serial.h"
#include "../../kernel/c/lib/string.h"
#include "../../kernel/c/fs/ramfs.h"
#include "doomkeys.h"

// DG_ScreenBuffer is defined in doomgeneric.c
static Framebuffer* g_fb = NULL;
static uint32_t* g_back_buffer = NULL;
static int g_window_x = 0;
static int g_window_y = 0;

extern void* kmalloc(size_t size);
extern void kfree(void* ptr);
extern size_t ksize(void* ptr);

void DG_Init() {
    serial_write_str("DOOM: DG_Init\r\n");
}

void DG_DrawFrame() {
    if (!g_fb || !DG_ScreenBuffer || !g_back_buffer) return;

    uint32_t* dst = g_back_buffer;
    uint32_t fb_width = g_fb->width;
    
    for (int y = 0; y < DOOMGENERIC_RESY; y++) {
        int dst_y = g_window_y + y;
        if (dst_y < 0 || dst_y >= (int)g_fb->height) continue;
        
        for (int x = 0; x < DOOMGENERIC_RESX; x++) {
            int dst_x = g_window_x + x;
            if (dst_x < 0 || dst_x >= (int)fb_width) continue;
            
            dst[dst_y * fb_width + dst_x] = DG_ScreenBuffer[y * DOOMGENERIC_RESX + x];
        }
    }
}

void DG_SleepMs(uint32_t ms) {
    task_sleep(ms);
}

uint32_t DG_GetTicksMs() {
    return (uint32_t)timer_get_ms();
}

int DG_GetKey(int* pressed, unsigned char* key) {
    if (input_keyboard_available()) {
        uint32_t e = input_keyboard_get_event();
        *pressed = !(e & (1 << 31)); // 1 for press, 0 for release
        char c = (char)(e & 0xFF);
        
        if (c == 8) *key = KEY_BACKSPACE;
        else if (c == 13 || c == '\n') *key = KEY_ENTER;
        else if (c == 27) *key = KEY_ESCAPE;
        else if (c >= 'a' && c <= 'z') *key = c;
        else if (c >= 'A' && c <= 'Z') *key = c - 'A' + 'a';
        else *key = c;
        
        return 1;
    }
    return 0;
}

void DG_SetWindowTitle(const char * title) {
    serial_write_str("DOOM Title: ");
    serial_write_str(title);
    serial_write_str("\r\n");
}

// --- Standard Library Stubs ---

void* malloc(size_t size) { return kmalloc(size); }
void free(void* ptr) { if (ptr) kfree(ptr); }
void* calloc(size_t nmemb, size_t size) {
    void* p = kmalloc(nmemb * size);
    if (p) memset(p, 0, nmemb * size);
    return p;
}
void* realloc(void* ptr, size_t size) { 
    if (!ptr) return kmalloc(size);
    if (size == 0) { kfree(ptr); return NULL; }
    void* new_ptr = kmalloc(size);
    if (!new_ptr) {
        // Standard realloc semantics: keep old allocation on failure.
        return NULL;
    }

    size_t old_sz = ksize(ptr);
    size_t copy_sz = old_sz < size ? old_sz : size;
    if (copy_sz) memcpy(new_ptr, ptr, copy_sz);
    kfree(ptr);
    return new_ptr;
}

void exit(int status) {  }
char* getenv(const char* name) { return NULL; }

char* strrchr(const char* s, int c) {
    char* last = NULL;
    while (*s) {
        if (*s == (char)c) last = (char*)s;
        s++;
    }
    return last;
}

char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    for (; *haystack; haystack++) {
        if (*haystack == *needle) {
            const char *h = haystack, *n = needle;
            while (*h && *n && *h == *n) { h++; n++; }
            if (!*n) return (char*)haystack;
        }
    }
    return NULL;
}

char* strchr(const char* s, int c) {
    while (*s != (char)c) {
        if (!*s++) return NULL;
    }
    return (char*)s;
}

char* strdup(const char* s) {
    size_t len = strlen(s) + 1;
    void* new_s = kmalloc(len);
    if (new_s) memcpy(new_s, s, len);
    return (char*)new_s;
}

int tolower(int c) { if (c >= 'A' && c <= 'Z') return c + 32; return c; }
int toupper(int c) { if (c >= 'a' && c <= 'z') return c - 32; return c; }
int isupper(int c) { return (c >= 'A' && c <= 'Z'); }

int strcasecmp(const char* s1, const char* s2) {
    while (*s1 && (tolower(*s1) == tolower(*s2))) {
        s1++; s2++;
    }
    return tolower(*(unsigned char*)s1) - tolower(*(unsigned char*)s2);
}

int strncasecmp(const char* s1, const char* s2, size_t n) {
    if (n == 0) return 0;
    while (n-- > 0 && tolower(*s1) == tolower(*s2)) {
        if (n == 0 || *s1 == '\0') break;
        s1++; s2++;
    }
    return tolower(*(unsigned char*)s1) - tolower(*(unsigned char*)s2);
}

long strtol(const char* nptr, char** endptr, int base) {
    long res = 0;
    while (*nptr >= '0' && *nptr <= '9') {
        res = res * base + (*nptr - '0');
        nptr++;
    }
    if (endptr) *endptr = (char*)nptr;
    return res;
}

double strtod(const char* nptr, char** endptr) { return 0.0; }
int atoi(const char* str) { return (int)strtol(str, NULL, 10); }

// --- I/O Stubs ---

void* stdout = (void*)1;
void* stderr = (void*)2;

typedef struct {
    RamNode* node;
    uint32_t pos;
} FAKE_FILE;

extern void serial_write_str(const char* str);
extern void serial_putc(char c);

extern int ksprintf(char* buf, const char* fmt, ...);

int vsnprintf(char* str, size_t size, const char* format, __builtin_va_list ap);

static int tiny_is_digit(char c) { return (c >= '0' && c <= '9'); }

static int tiny_hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static int tiny_parse_int_auto_base(const char* s, int* out_val) {
    if (!s || !out_val) return 0;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;

    int sign = 1;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') { s++; }

    int base = 10;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { base = 16; s += 2; }
    else if (s[0] == '0') { base = 8; s += 1; }

    int any = 0;
    long res = 0;
    for (;;) {
        int d;
        if (base == 16) d = tiny_hex_val(*s);
        else if (base == 10) d = (*s >= '0' && *s <= '9') ? (*s - '0') : -1;
        else d = (*s >= '0' && *s <= '7') ? (*s - '0') : -1;
        if (d < 0) break;
        any = 1;
        res = res * base + d;
        s++;
    }

    if (!any) return 0;
    *out_val = (int)(res * sign);
    return 1;
}

int printf(const char* format, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, format);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), format, ap);
    serial_write_str(buf);
    __builtin_va_end(ap);
    return 0;
}

int fprintf(void* stream, const char* format, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, format);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), format, ap);
    serial_write_str(buf);
    __builtin_va_end(ap);
    return 0;
}

int vfprintf(void* stream, const char* format, __builtin_va_list ap) {
    char buf[1024];
    vsnprintf(buf, sizeof(buf), format, ap);
    serial_write_str(buf);
    return 0;
}

typedef struct {
    char* buf;
    size_t size;
    size_t out;
} tiny_outbuf_t;

static void tiny_out_putc(tiny_outbuf_t* o, char c) {
    if (!o || !o->buf || o->size == 0) return;
    if (o->out + 1 < o->size) {
        o->buf[o->out++] = c;
        o->buf[o->out] = 0;
    }
}

static void tiny_out_puts(tiny_outbuf_t* o, const char* s) {
    if (!s) s = "(null)";
    while (*s) {
        tiny_out_putc(o, *s++);
    }
}

static void tiny_out_put_uint_dec(tiny_outbuf_t* o, unsigned int v) {
    char tmp[16];
    int n = 0;
    if (v == 0) {
        tiny_out_putc(o, '0');
        return;
    }
    while (v && n < (int)sizeof(tmp)) {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (n-- > 0) tiny_out_putc(o, tmp[n]);
}

static void tiny_out_put_int_dec(tiny_outbuf_t* o, int v, int width, int zero_pad, int precision) {
    char tmp[16];
    unsigned int uv;
    int negative = 0;
    int n = 0;

    if (v < 0) {
        negative = 1;
        uv = (unsigned int)(-v);
    } else {
        uv = (unsigned int)v;
    }

    if (uv == 0) {
        tmp[n++] = '0';
    } else {
        while (uv && n < (int)sizeof(tmp)) {
            tmp[n++] = (char)('0' + (uv % 10));
            uv /= 10;
        }
    }

    int digits = n;
    int prec_zeros = 0;
    if (precision >= 0 && precision > digits) {
        prec_zeros = precision - digits;
    }

    int total_len = digits + prec_zeros + (negative ? 1 : 0);
    int pad = 0;
    if (width > total_len) {
        pad = width - total_len;
    }

    // If precision is specified, '0' flag is ignored for the padding.
    char pad_ch = (zero_pad && precision < 0) ? '0' : ' ';

    if (pad_ch == '0') {
        if (negative) tiny_out_putc(o, '-');
        for (int i = 0; i < pad; i++) tiny_out_putc(o, '0');
    } else {
        for (int i = 0; i < pad; i++) tiny_out_putc(o, ' ');
        if (negative) tiny_out_putc(o, '-');
    }

    for (int i = 0; i < prec_zeros; i++) tiny_out_putc(o, '0');
    while (n-- > 0) tiny_out_putc(o, tmp[n]);
}

int vsnprintf(char* str, size_t size, const char* format, __builtin_va_list ap) {
    if (!str || size == 0) return 0;
    tiny_outbuf_t o;
    o.buf = str;
    o.size = size;
    o.out = 0;
    o.buf[0] = 0;

    for (size_t i = 0; format[i] && o.out + 1 < o.size; i++) {
        if (format[i] != '%') { tiny_out_putc(&o, format[i]); continue; }
        i++;
        if (format[i] == '%') { tiny_out_putc(&o, '%'); continue; }

        // Parse width / zero-pad
        int zero_pad = 0;
        int width = 0;
        int precision = -1;
        if (format[i] == '0') { zero_pad = 1; i++; }
        while (tiny_is_digit(format[i])) {
            width = width * 10 + (format[i] - '0');
            i++;
        }

        // Parse optional precision: .<digits>
        if (format[i] == '.') {
            i++;
            precision = 0;
            while (tiny_is_digit(format[i])) {
                precision = precision * 10 + (format[i] - '0');
                i++;
            }
        }

        char spec = format[i];
        if (spec == 's') {
            const char* s = __builtin_va_arg(ap, const char*);
            if (!s) s = "(null)";
            int len = (int)strlen(s);
            if (precision >= 0 && len > precision) len = precision;
            for (int j = len; j < width; j++) tiny_out_putc(&o, ' ');
            for (int j = 0; j < len; j++) tiny_out_putc(&o, s[j]);
        } else if (spec == 'c') {
            char c = (char)__builtin_va_arg(ap, int);
            tiny_out_putc(&o, c);
        } else if (spec == 'd' || spec == 'i') {
            int v = __builtin_va_arg(ap, int);
            tiny_out_put_int_dec(&o, v, width, zero_pad, precision);
        } else if (spec == 'u') {
            unsigned int v = __builtin_va_arg(ap, unsigned int);
            tiny_out_put_uint_dec(&o, v);
        } else if (spec == 'x') {
            unsigned int v = __builtin_va_arg(ap, unsigned int);
            char tmp[32];
            ksprintf(tmp, "%x", (uint64_t)v);
            int len = (int)strlen(tmp);
            for (int j = len; j < width; j++) tiny_out_putc(&o, zero_pad ? '0' : ' ');
            tiny_out_puts(&o, tmp);
        } else if (spec == 'p') {
            uintptr_t v = (uintptr_t)__builtin_va_arg(ap, void*);
            char tmp[32];
            ksprintf(tmp, "%x", (uint64_t)v);
            tiny_out_puts(&o, "0x");
            tiny_out_puts(&o, tmp);
        } else {
            tiny_out_putc(&o, '%');
            tiny_out_putc(&o, spec);
        }
    }

    if (o.out < o.size) o.buf[o.out] = 0;
    else o.buf[o.size - 1] = 0;
    return (int)o.out;
}

int snprintf(char* str, size_t size, const char* format, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, format);
    int r = vsnprintf(str, size, format, ap);
    __builtin_va_end(ap);
    return r;
}

int sscanf(const char* str, const char* format, ...) {
    if (!str || !format) return 0;
    __builtin_va_list ap;
    __builtin_va_start(ap, format);

    // Doom uses this for integer parsing in config code: "%x" and "%i".
    int parsed = 0;
    if (strstr(format, "%x")) {
        int* out = __builtin_va_arg(ap, int*);
        int v = 0;
        const char* s = str;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        // Accept optional 0x prefix
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
        int any = 0;
        while (1) {
            int d = tiny_hex_val(*s);
            if (d < 0) break;
            any = 1;
            v = (v << 4) | d;
            s++;
        }
        if (any) {
            *out = v;
            parsed = 1;
        }
    } else if (strstr(format, "%i") || strstr(format, "%d")) {
        int* out = __builtin_va_arg(ap, int*);
        int v;
        if (tiny_parse_int_auto_base(str, &v)) {
            *out = v;
            parsed = 1;
        }
    }

    __builtin_va_end(ap);
    return parsed;
}

int __isoc99_sscanf(const char* str, const char* format, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, format);
    // Forward to sscanf by reconstructing isn't trivial; handle common case directly.
    int parsed = 0;
    if (strstr(format, "%x") || strstr(format, "%i") || strstr(format, "%d")) {
        int* out = __builtin_va_arg(ap, int*);
        if (strstr(format, "%x")) {
            int v = 0;
            const char* s = str;
            while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
            if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
            int any = 0;
            while (1) {
                int d = tiny_hex_val(*s);
                if (d < 0) break;
                any = 1;
                v = (v << 4) | d;
                s++;
            }
            if (any) { *out = v; parsed = 1; }
        } else {
            int v;
            if (tiny_parse_int_auto_base(str, &v)) { *out = v; parsed = 1; }
        }
    }
    __builtin_va_end(ap);
    return parsed;
}

void* fopen(const char* filename, const char* mode) {
    // Search in RamFS
    RamNode* cur = ramfs_get_root()->child;
    while (cur) {
        if (strcasecmp(cur->name, filename) == 0) {
            FAKE_FILE* f = (FAKE_FILE*)kmalloc(sizeof(FAKE_FILE));
            f->node = cur;
            f->pos = 0;

            if (strcasecmp(cur->name, "doom1.wad") == 0) {
                uint32_t off = 2588260;
                unsigned char *b = cur->content;
                if (b && cur->size > off + 4) {
                    unsigned int v = (unsigned int)b[off] | ((unsigned int)b[off + 1] << 8) |
                                     ((unsigned int)b[off + 2] << 16) | ((unsigned int)b[off + 3] << 24);
                    char msg[160];
                    ksprintf(msg, "FAKE fopen: doom1.wad node=%x size=%u PNAMES_u32=%x bytes=%x %x %x %x\r\n",
                             (uint64_t)(uintptr_t)cur, (unsigned int)cur->size, (uint64_t)v,
                             (uint64_t)b[off], (uint64_t)b[off + 1], (uint64_t)b[off + 2], (uint64_t)b[off + 3]);
                    serial_write_str(msg);
                } else {
                    char msg[160];
                    ksprintf(msg, "FAKE fopen: doom1.wad node=%x size=%u (no content/off out of range)\r\n",
                             (uint64_t)(uintptr_t)cur, (unsigned int)cur->size);
                    serial_write_str(msg);
                }
            }

            return f;
        }
        cur = cur->next;
    }
    return NULL;
}

size_t fread(void* ptr, size_t size, size_t nmemb, void* stream) {
    FAKE_FILE* f = (FAKE_FILE*)stream;
    if (!f || !f->node->content) return 0;
    
    size_t total = size * nmemb;

    // If seek put us past EOF, behave like EOF.
    if (f->pos >= f->node->size) return 0;

    // Clamp read to remaining bytes.
    size_t remain = (size_t)(f->node->size - f->pos);
    if (total > remain) total = remain;

    if (strcasecmp(f->node->name, "doom1.wad") == 0) {
        uint32_t off = 2588260;
        if (f->pos <= off + 4 && f->pos + total >= off) {
            unsigned char *b = f->node->content;
            if (b && f->node->size > off + 4) {
                unsigned int v = (unsigned int)b[off] | ((unsigned int)b[off + 1] << 8) |
                                 ((unsigned int)b[off + 2] << 16) | ((unsigned int)b[off + 3] << 24);
                char msg[200];
                ksprintf(msg, "FAKE fread: pos=%u total=%u PNAMES_u32=%x bytes=%x %x %x %x\r\n",
                         (unsigned int)f->pos, (unsigned int)total, (uint64_t)v,
                         (uint64_t)b[off], (uint64_t)b[off + 1], (uint64_t)b[off + 2], (uint64_t)b[off + 3]);
                serial_write_str(msg);
            }
        }
    }
    
    memcpy(ptr, f->node->content + f->pos, total);
    f->pos += total;
    return total / size;
}

size_t fwrite(const void* ptr, size_t size, size_t nmemb, void* stream) { return nmemb; }

int fclose(void* stream) {
    if (stream && stream != stdout && stream != stderr) kfree(stream);
    return 0;
}

int fseek(void* stream, long offset, int whence) {
    FAKE_FILE* f = (FAKE_FILE*)stream;
    if (!f) return -1;

    int64_t base = 0;
    if (whence == 0) base = 0;                 // SEEK_SET
    else if (whence == 1) base = (int64_t)f->pos; // SEEK_CUR
    else if (whence == 2) base = (int64_t)f->node->size; // SEEK_END
    else return -1;

    int64_t newpos = base + (int64_t)offset;
    if (newpos < 0) newpos = 0;
    if (newpos > (int64_t)f->node->size) newpos = (int64_t)f->node->size;
    f->pos = (uint32_t)newpos;

    return 0;
}

long ftell(void* stream) {
    FAKE_FILE* f = (FAKE_FILE*)stream;
    return f ? f->pos : -1;
}

int feof(void* stream) {
    FAKE_FILE* f = (FAKE_FILE*)stream;
    return f ? (f->pos >= f->node->size) : 1;
}

int fflush(void* stream) { return 0; }
int puts(const char* s) { serial_write_str(s); serial_write_str("\n"); return 0; }
int putc(int c, void* stream) { serial_putc(c); return c; }
int putchar(int c) { serial_putc(c); return c; }

int remove(const char* pathname) { return -1; }
int rename(const char* oldpath, const char* newpath) { return -1; }
int system(const char* command) { return -1; }
int mkdir(const char* pathname, int mode) { return -1; }

long time(long* t) { if (t) *t = DG_GetTicksMs() / 1000; return DG_GetTicksMs() / 1000; }

// --- Locale / Ctype Stubs ---
const unsigned short* __ctype_b_loc(void) {
    static const unsigned short table[384] = {0}; // Dummy table
    return &table[128];
}
const int** __ctype_toupper_loc(void) {
    static const int table[384] = {0};
    static const int* p_table = &table[128];
    return &p_table;
}
int* __errno_location(void) { static int e; return &e; }

// --- Missing Doom Symbols (Networking/Joystick) ---
int drone = 0;
int net_client_connected = 0;
void I_BindJoystickVariables(void) {}
void I_InitJoystick(void) {}

// --- Tiny64 Helpers ---
void DG_SetFramebuffer(Framebuffer* fb) { g_fb = fb; }
void DG_SetBackBuffer(uint32_t* bb) { g_back_buffer = bb; }
void DG_SetWindowPos(int x, int y) { g_window_x = x; g_window_y = y; }