#include "../drivers/platform/serial.h"
#include "../drivers/storage/ata.h"
#include <stdint.h>

static uint32_t g_disk_log_lba = 262144; // Start at 128MB (safe for >256MB drives)

void disk_log_autosave(const char* data, uint32_t len) {
    if (!ata_is_drive_present()) return;

    // Write in 512-byte chunks
    uint32_t sectors = len / 512;
    if (sectors > 0) {
        ata_write_sectors(g_disk_log_lba, (uint8_t)sectors, (const uint8_t*)data);
        g_disk_log_lba += sectors;
    }
}
