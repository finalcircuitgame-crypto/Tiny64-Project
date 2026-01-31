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
    if (new_ptr && ptr) memcpy(new_ptr, ptr, size); 
    if (ptr) kfree(ptr);
    return new_ptr;
}

void exit(int status) { while(1); }
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

// Helper for minimal printf
static void serial_printf(const char* format, __builtin_va_list ap) {
    // This is a VERY minimal implementation, but enough for Doom's startup logs
    char buf[1024];
    // We don't have vsnprintf working yet, so let's just use a stub or implement a simple one
    // Actually, Tiny64 might have a vsnprintf in kernel/c/lib/string.c or similar
    // For now, let's just print the format string if it doesn't have too many %
    serial_write_str(format); 
}

int printf(const char* format, ...) {
    __builtin_va_list ap;
    va_start(ap, format);
    // For now just write the format string to see where we are
    serial_write_str(format);
    va_end(ap);
    return 0;
}

int fprintf(void* stream, const char* format, ...) {
    __builtin_va_list ap;
    va_start(ap, format);
    serial_write_str(format);
    va_end(ap);
    return 0;
}

int vfprintf(void* stream, const char* format, __builtin_va_list ap) {
    serial_write_str(format);
    return 0;
}

int vsnprintf(char* str, size_t size, const char* format, __builtin_va_list ap) {
    if (size > 0) {
        strncpy(str, format, size - 1);
        str[size-1] = '\0';
    }
    return strlen(str);
}

int snprintf(char* str, size_t size, const char* format, ...) {
    if (size > 0) {
        strncpy(str, format, size - 1);
        str[size-1] = '\0';
    }
    return strlen(str);
}

int sscanf(const char* str, const char* format, ...) { return 0; }
int __isoc99_sscanf(const char* str, const char* format, ...) { return 0; }

void* fopen(const char* filename, const char* mode) {
    // Search in RamFS
    RamNode* cur = ramfs_get_root()->child;
    while (cur) {
        if (strcasecmp(cur->name, filename) == 0) {
            FAKE_FILE* f = (FAKE_FILE*)kmalloc(sizeof(FAKE_FILE));
            f->node = cur;
            f->pos = 0;
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
    if (f->pos + total > f->node->size) {
        total = f->node->size - f->pos;
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
    
    if (whence == 0) f->pos = offset; // SEEK_SET
    else if (whence == 1) f->pos += offset; // SEEK_CUR
    else if (whence == 2) f->pos = f->node->size + offset; // SEEK_END
    
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