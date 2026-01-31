#include "../../scheduler/tick.h"
#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

// This function should be called by the timer interrupt handler
void tick_handler() {
    timer_tick_handler();
    
    // Send EOI to Master PIC
    outb(0x20, 0x20);
}
