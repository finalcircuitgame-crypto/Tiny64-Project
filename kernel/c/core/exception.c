#include "../drivers/platform/serial.h"
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint64_t int_no;
    uint64_t err_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} exception_frame_t;

static void halt_forever(void) {
    while (1) {
        __asm__ volatile("cli; hlt");
    }
}

void exception_handler(void *frame_ptr) {
    exception_frame_t *f = (exception_frame_t *) frame_ptr;

    serial_write_str("\r\n[EXCEPTION] vector=");
    serial_write_dec((uint32_t) f->int_no);
    serial_write_str(" err=0x");
    serial_write_hex(f->err_code);
    serial_write_str(" rip=0x");
    serial_write_hex(f->rip);
    serial_write_str(" cs=0x");
    serial_write_hex(f->cs);
    serial_write_str(" rflags=0x");
    serial_write_hex(f->rflags);
    serial_write_str("\r\n");

    halt_forever();
}
