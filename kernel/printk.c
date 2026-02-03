#include <kernel/printk.h>
#include <kernel/serial.h>
#include <stdarg.h>

void console_putchar(char c);

static void print_uint(unsigned long n, int base, int width) {
    char buf[32];
    int i = 0;
    const char *digits = "0123456789abcdef";
    if (n == 0) {
        int w = (width > 0) ? width : 1;
        while (w--) {
            serial_putc('0');
            console_putchar('0');
        }
        return;
    }
    while (n > 0) {
        buf[i++] = digits[n % base];
        n /= base;
    }
    while (i < width) {
        buf[i++] = '0';
    }
    while (--i >= 0) {
        serial_putc(buf[i]);
        console_putchar(buf[i]);
    }
}

void printk(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (const char *p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            serial_putc(*p);
            console_putchar(*p);
            continue;
        }
        p++; // skip %
        
        int width = 0;
        if (*p == '0') {
            p++;
            while (*p >= '0' && *p <= '9') {
                width = width * 10 + (*p - '0');
                p++;
            }
        }

        switch (*p) {
            case 's': {
                const char *s = va_arg(args, const char *);
                while (*s) {
                    serial_putc(*s);
                    console_putchar(*s);
                    s++;
                }
                break;
            }
            case 'd': {
                long n = va_arg(args, long);
                if (n < 0) {
                    serial_putc('-');
                    console_putchar('-');
                    n = -n;
                }
                print_uint(n, 10, width);
                break;
            }
            case 'x': {
                unsigned long n = va_arg(args, unsigned long);
                print_uint(n, 16, width);
                break;
            }
            case 'p': {
                unsigned long n = (unsigned long)va_arg(args, void*);
                serial_write("0x");
                console_putchar('0');
                console_putchar('x');
                print_uint(n, 16, 8);
                break;
            }
            case '%':
                serial_putc('%');
                console_putchar('%');
                break;
            default:
                serial_putc(*p);
                console_putchar(*p);
                break;
        }
    }
    va_end(args);
}