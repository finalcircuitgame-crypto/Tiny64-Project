#include "input.h"
#include "../platform/serial.h"
#include <stdbool.h>

// Simple Ring Buffer for Keyboard
#define KB_BUFFER_SIZE 128
static uint32_t kb_buffer[KB_BUFFER_SIZE];
static int kb_read_ptr = 0;
static int kb_write_ptr = 0;

// Mouse Accumulator
static input_mouse_state_t global_mouse_state = {0, 0, 0, 0, 0, 0, 0};

// Key Repeat State
static uint32_t g_last_key = 0;
static uint64_t g_key_press_time = 0;
static uint64_t g_last_repeat_time = 0;
static bool g_key_down = false;

extern uint64_t timer_get_ms();

int input_keyboard_available() {
    // Check for repeats before reporting availability
    if (g_key_down && g_last_key == 8) { // Only repeat backspace for now
        uint64_t now = timer_get_ms();
        if (now - g_key_press_time > 500) { // 500ms initial delay
            if (now - g_last_repeat_time > 50) { // 50ms repeat interval
                input_keyboard_push(8);
                g_last_repeat_time = now;
            }
        }
    }
    return kb_read_ptr != kb_write_ptr;
}

uint32_t input_keyboard_get_event() {
    if (kb_read_ptr == kb_write_ptr) return 0;
    
    uint32_t e = kb_buffer[kb_read_ptr];
    kb_read_ptr = (kb_read_ptr + 1) % KB_BUFFER_SIZE;
    return e;
}

char input_keyboard_getc() {
    uint32_t e = input_keyboard_get_event();
    if (e & 0xFF000000) return 0; // It's a special key or modifier only
    return (char)(e & 0xFF);
}

void input_keyboard_push(uint32_t event) {
    // Handle key down/up for repetition
    if (event & (1 << 31)) {
        // Key Release
        uint32_t key = event & 0xFF;
        if (key == (g_last_key & 0xFF)) {
            g_key_down = false;
        }
    } else {
        // Key Press
        g_last_key = event;
        g_key_press_time = timer_get_ms();
        g_last_repeat_time = g_key_press_time;
        g_key_down = true;
    }

    int next_write = (kb_write_ptr + 1) % KB_BUFFER_SIZE;
    if (next_write != kb_read_ptr) { // Not full
        kb_buffer[kb_write_ptr] = event;
        kb_write_ptr = next_write;
        
        // Debug print pressed key to serial if it's a printable char
        if (!(event & 0xFF000000)) {
            char c = (char)(event & 0xFF);
            serial_write_str("key: ");
            serial_putc(c);
            serial_write_str("\r\n");
        }
    }
}

void input_mouse_get_state(input_mouse_state_t* state) {
    if (state) {
        state->x_diff = global_mouse_state.x_diff;
        state->y_diff = global_mouse_state.y_diff;
        state->x_abs = global_mouse_state.x_abs;
        state->y_abs = global_mouse_state.y_abs;
        state->has_abs = global_mouse_state.has_abs;
        state->buttons = global_mouse_state.buttons;
        state->updated = global_mouse_state.updated;
        
        // Clear diffs and updated flag
        global_mouse_state.x_diff = 0;
        global_mouse_state.y_diff = 0;
        global_mouse_state.updated = 0;
    }
}

void input_mouse_update(int dx, int dy, uint8_t buttons) {
    global_mouse_state.x_diff += dx;
    global_mouse_state.y_diff += dy;
    global_mouse_state.buttons = buttons;
    global_mouse_state.has_abs = 0;
    global_mouse_state.updated = 1;
}

void input_mouse_update_abs(int x, int y, uint8_t buttons) {

    global_mouse_state.x_abs = x;

    global_mouse_state.y_abs = y;

    global_mouse_state.buttons = buttons;

    global_mouse_state.has_abs = 1;

    global_mouse_state.updated = 1;

}



// Force mouse update for UI polling (even if no movement/button change)

void input_mouse_force_update() {

    global_mouse_state.updated = 1;

}
