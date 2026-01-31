#include "string.h"

void* memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;

    // Use SSE for large, 16-byte aligned blocks
    if (n >= 64 && ((uintptr_t)d % 16 == 0) && ((uintptr_t)s % 16 == 0)) {
        while (n >= 64) {
            __asm__ volatile (
                "movups (%0), %%xmm0\n"
                "movups 16(%0), %%xmm1\n"
                "movups 32(%0), %%xmm2\n"
                "movups 48(%0), %%xmm3\n"
                "movups %%xmm0, (%1)\n"
                "movups %%xmm1, 16(%1)\n"
                "movups %%xmm2, 32(%1)\n"
                "movups %%xmm3, 48(%1)\n"
                : : "r"(s), "r"(d) : "xmm0", "xmm1", "xmm2", "xmm3", "memory"
            );
            d += 64;
            s += 64;
            n -= 64;
        }
    }

    // Fallback for smaller or unaligned blocks
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

void* memset(void* s, int c, size_t n) {
    unsigned char* p = (unsigned char*)s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}

// Fallback for compiler-generated calls when fortification is partially enabled
void* __memset_chk(void* dest, int c, size_t n, size_t destlen) {
    return memset(dest, c, n);
}

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

char* strcpy(char* dest, const char* src) {
    char* temp = dest;
    while ((*dest++ = *src++));
    return temp;
}

char* strncpy(char* dest, const char* src, size_t n) {
    char* temp = dest;
    while (n && (*dest++ = *src++)) {
        n--;
    }
    while (n--) {
        *dest++ = '\0';
    }
    return temp;
}

char* strcat(char* dest, const char* src) {
    char* temp = dest;
    while (*dest) dest++;
    while ((*dest++ = *src++));
    return temp;
}

char* strncat(char* dest, const char* src, size_t n) {
    char* temp = dest;
    while (*dest) dest++;
    while (n-- && *src) {
        *dest++ = *src++;
    }
    *dest = '\0';
    return temp;
}

static void reverse(char* s) {
    int i, j;
    char c;
    for (i = 0, j = strlen(s)-1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

static void itoa(int n, char* s) {
    int i, sign;
    if ((sign = n) < 0) n = -n;
    i = 0;
    do {
        s[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);
    if (sign < 0) s[i++] = '-';
    s[i] = '\0';
    reverse(s);
}

int ksprintf(char* buf, const char* fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    
    char* p = buf;
    const char* f;
    
    for (f = fmt; *f != '\0'; f++) {
        if (*f != '%') {
            *p++ = *f;
            continue;
        }
        
        f++;
        switch (*f) {
            case 's': {
                char* s = __builtin_va_arg(args, char*);
                while (*s) *p++ = *s++;
                break;
            }
            case 'd': {
                int d = __builtin_va_arg(args, int);
                char s[16];
                itoa(d, s);
                char* sp = s;
                while (*sp) *p++ = *sp++;
                break;
            }
            case 'x': {
                uint64_t x = __builtin_va_arg(args, uint64_t);
                const char* hex = "0123456789ABCDEF";
                for (int i = 7; i >= 0; i--) {
                    *p++ = hex[(x >> (i * 4)) & 0xF];
                }
                break;
            }
            default:
                *p++ = *f;
                break;
        }
    }
    *p = '\0';
    __builtin_va_end(args);
    return (int)(p - buf);
}