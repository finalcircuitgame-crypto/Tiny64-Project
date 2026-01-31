#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void init_serial();
void serial_putc(char c);
uint32_t serial_get_log_size();
const char* serial_get_log_buffer();
uint64_t serial_get_last_log_time();
void serial_write_str(const char* str);
void serial_write_hex(uint64_t n);
void serial_write_dec(uint64_t n);
void serial_write_int(int64_t n);
int serial_received();
char serial_getc();

// Autosave feature
typedef void (*serial_autosave_callback_t)(const char* data, uint32_t len);
void serial_set_autosave_callback(serial_autosave_callback_t cb, uint32_t threshold);
void serial_set_net_log_hook(void (*hook)(char));

#ifdef __cplusplus
}
#endif

#endif
