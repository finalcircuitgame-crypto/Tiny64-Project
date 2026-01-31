#include "controller.h"
#include "../../platform/serial.h"

// Wait until the Input Buffer is empty (we can write to controller)
void ps2_controller_wait_write() {
    int timeout = 100000;
    while (timeout--) {
        if ((ps2_inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT_BUFFER) == 0) return;
    }
}

// Wait until the Output Buffer is full (we can read from controller)
void ps2_controller_wait_read() {
    int timeout = 100000;
    while (timeout--) {
        if ((ps2_inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_BUFFER) == 1) return;
    }
}

int ps2_controller_init() {
    serial_write_str("[PS2] Initializing Controller...\r\n");

    // 1. Disable devices
    ps2_controller_wait_write();
    ps2_outb(PS2_CMD_PORT, PS2_CMD_DISABLE_PORT1);
    ps2_controller_wait_write();
    ps2_outb(PS2_CMD_PORT, PS2_CMD_DISABLE_PORT2);

    // 2. Flush Output Buffer
    ps2_inb(PS2_DATA_PORT);

    // 3. Set Configuration Byte
    ps2_controller_wait_write();
    ps2_outb(PS2_CMD_PORT, PS2_CMD_READ_CONFIG);
    ps2_controller_wait_read();
    uint8_t config = ps2_inb(PS2_DATA_PORT);
    
    // Enable IRQs (bit 0=Port1, bit 1=Port2), Disable Translation (bit 6) for now? 
    // Actually, Translation (bit 6) is usually good for Set 1 compatibility.
    config |= (1 << 0) | (1 << 1) | (1 << 6); 
    
    ps2_controller_wait_write();
    ps2_outb(PS2_CMD_PORT, PS2_CMD_WRITE_CONFIG);
    ps2_controller_wait_write();
    ps2_outb(PS2_DATA_PORT, config);

    // 4. Controller Self Test
    ps2_controller_wait_write();
    ps2_outb(PS2_CMD_PORT, PS2_CMD_TEST_CONTROLLER);
    ps2_controller_wait_read();
    uint8_t test_res = ps2_inb(PS2_DATA_PORT);
    
    if (test_res != 0x55) {
        serial_write_str("[PS2] Controller Self-Test Failed.\r\n");
        return 0;
    }

    // 5. Enable Devices
    ps2_controller_wait_write();
    ps2_outb(PS2_CMD_PORT, PS2_CMD_ENABLE_PORT1);
    ps2_controller_wait_write();
    ps2_outb(PS2_CMD_PORT, PS2_CMD_ENABLE_PORT2);

    serial_write_str("[PS2] Controller Ready.\r\n");
    return 1;
}
