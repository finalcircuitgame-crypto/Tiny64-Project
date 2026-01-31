#ifndef PS2_CONTROLLER_H
#define PS2_CONTROLLER_H

#include <stdint.h>

#define PS2_DATA_PORT    0x60
#define PS2_STATUS_PORT  0x64
#define PS2_CMD_PORT     0x64

// Status Register Bits
#define PS2_STATUS_OUTPUT_BUFFER 1
#define PS2_STATUS_INPUT_BUFFER  2

// Commands
#define PS2_CMD_DISABLE_PORT1    0xAD
#define PS2_CMD_ENABLE_PORT1     0xAE
#define PS2_CMD_DISABLE_PORT2    0xA7
#define PS2_CMD_ENABLE_PORT2     0xA8
#define PS2_CMD_READ_CONFIG      0x20
#define PS2_CMD_WRITE_CONFIG     0x60
#define PS2_CMD_TEST_CONTROLLER  0xAA
#define PS2_CMD_TEST_PORT1       0xAB
#define PS2_CMD_TEST_PORT2       0xA9

void ps2_controller_wait_write();
void ps2_controller_wait_read();
int ps2_controller_init(); // Returns 1 if initialized, 0 if not present/failed

// Shared I/O wrappers
static inline void ps2_outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t ps2_inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

#endif
