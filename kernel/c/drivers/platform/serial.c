#include "serial.h"
#include "../../core/serial_terminal.h"

#define PORT 0x3f8          // COM1
#define LOG_BUFFER_SIZE (512 * 1024) // 512KB log buffer

static char g_log_buffer[LOG_BUFFER_SIZE];
static uint32_t g_log_ptr = 0;
static serial_autosave_callback_t g_autosave_cb = 0;
static uint32_t g_autosave_threshold = 1024;
static uint32_t g_last_autosave_ptr = 0;
static int g_in_autosave = 0;
static uint64_t g_last_log_time = 0;
static void (*g_net_log_hook)(char) = 0;

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %1, %0"
                   : "=a"(ret)
                   : "Nd"(port) );
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

void init_serial() {
    outb(PORT + 1, 0x00);    // Disable all interrupts
    outb(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(PORT + 1, 0x00);    //                  (hi byte)
    outb(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    
    // Initialize log buffer
    for(int i = 0; i < LOG_BUFFER_SIZE; i++) g_log_buffer[i] = 0;
    g_log_ptr = 0;
}

int is_transmit_empty() {
    return inb(PORT + 5) & 0x20;
}

extern uint64_t g_ticks;

void serial_putc(char c) {
    g_last_log_time = g_ticks;
    while (is_transmit_empty() == 0);
    outb(PORT, c);
    serial_terminal_putc(c);

    // Save to log buffer
    if (g_log_ptr < LOG_BUFFER_SIZE - 1) {
        g_log_buffer[g_log_ptr++] = c;
    }

    if (g_net_log_hook) {
        g_net_log_hook(c);
    }

    // Check autosave
    if (g_autosave_cb && !g_in_autosave) {
        if (g_log_ptr - g_last_autosave_ptr >= g_autosave_threshold) {
            g_in_autosave = 1;
            // Call callback with exactly threshold bytes
            g_autosave_cb(g_log_buffer + g_last_autosave_ptr, g_autosave_threshold);
            g_last_autosave_ptr += g_autosave_threshold;
            g_in_autosave = 0;
        }
    }
}

uint64_t serial_get_last_log_time() {
    return g_last_log_time;
}

const char* serial_get_log_buffer() {
    return g_log_buffer;
}

uint32_t serial_get_log_size() {
    return g_log_ptr;
}

void serial_write_str(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        serial_putc(str[i]);
    }
}

void serial_write_hex(uint64_t n) {
    serial_write_str("0x");
    static const char hex_chars[] = "0123456789ABCDEF";
    for (int i = 60; i >= 0; i -= 4) {
        serial_putc(hex_chars[(n >> i) & 0xF]);
    }
}

void serial_write_dec(uint64_t n) {
    if (n == 0) {
        serial_putc('0');
        return;
    }
    char buffer[20];
    int i = 0;
    while (n > 0) {
        buffer[i++] = (n % 10) + '0';
        n /= 10;
    }
    for (int j = i - 1; j >= 0; j--) {
        serial_putc(buffer[j]);
    }
}

void serial_write_int(int64_t n) {
    if (n < 0) {
        serial_putc('-');
        serial_write_dec((uint64_t)-n);
    } else {
        serial_write_dec((uint64_t)n);
    }
}

int serial_received() {
    return inb(PORT + 5) & 1;
}

char serial_getc() {
    while (serial_received() == 0);
    return inb(PORT);
}

void serial_set_autosave_callback(serial_autosave_callback_t cb, uint32_t threshold) {
    g_autosave_cb = cb;
    g_autosave_threshold = threshold;
    g_last_autosave_ptr = g_log_ptr;
}

void serial_set_net_log_hook(void (*hook)(char)) {
    g_net_log_hook = hook;
}

