// Force mouse update for UI polling (even if no movement/button change)
void input_mouse_force_update();
#ifndef HID_INPUT_H
#define HID_INPUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Common Input Interface
// This allows the Shell/Desktop to be agnostic of the source (PS/2 vs USB)

// --- Keyboard ---
// Returns 1 if a key is waiting, 0 otherwise
int input_keyboard_available();

// Returns the next character, or 0 if none
char input_keyboard_getc();

// Returns raw event (char | modifiers)
uint32_t input_keyboard_get_event();

// Internal: Drivers call this to push a key
void input_keyboard_push(uint32_t e);

// Modifier flags for uint32_t event
#define KB_MOD_CTRL  (1 << 24)
#define KB_MOD_SHIFT (1 << 25)
#define KB_MOD_ALT   (1 << 26)


// --- Mouse ---
typedef struct {
    int x_diff;
    int y_diff;
    int x_abs;     // 0 to 32767
    int y_abs;     // 0 to 32767
    uint8_t has_abs;
    uint8_t buttons; // Bit 0: Left, Bit 1: Right, Bit 2: Middle
    uint8_t updated;
} input_mouse_state_t;

// Returns current accumulated state and clears internal accumulators
// Safe to call every frame.
void input_mouse_get_state(input_mouse_state_t* state);

// Internal: Drivers call this to update state (Relative)
void input_mouse_update(int dx, int dy, uint8_t buttons);

// Internal: Drivers call this to update state (Absolute)
void input_mouse_update_abs(int x, int y, uint8_t buttons);

// Flag to indicate if USB HID devices are present
extern int usb_hid_detected;

#ifdef __cplusplus
}
#endif

#endif
