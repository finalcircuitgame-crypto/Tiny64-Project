#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

#define PORT 0x3f8 /* COM1 */

static inline uint8_t inb(uint16_t port) {
  uint8_t ret;
  asm volatile("inb %1, %0" : "=a"(ret) : "dN"(port));
  return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
  asm volatile("outb %0, %1" : : "a"(val), "dN"(port));
}

static inline int is_transmit_empty() { return inb(PORT + 5) & 0x20; }

static inline void write_serial(char a) {
  while (is_transmit_empty() == 0)
    ;
  outb(PORT, a);
}

static inline void serial_init() {
  outb(PORT + 1, 0x00); // Disable all interrupts
  outb(PORT + 3, 0x80); // Enable DLAB (set baud rate divisor)
  outb(PORT + 0, 0x03); // Set divisor to 3 (lo byte) 38400 baud
  outb(PORT + 1, 0x00); //                  (hi byte)
  outb(PORT + 3, 0x03); // 8 bits, no parity, one stop bit
  outb(PORT + 4, 0x0B); // IRQs enabled, RTS/DSR set
}

static inline void serial_print(const char *s) {
  while (*s) {
    write_serial(*s++);
  }
}

static inline void serial_println(const char *s) {
  serial_print(s);
  serial_print("\r\n");
}

static inline void serial_print_hex(uint64_t val) {
  char *hex = "0123456789ABCDEF";
  serial_print("0x");
  for (int i = 60; i >= 0; i -= 4) {
    write_serial(hex[(val >> i) & 0xF]);
  }
}

#endif
