#include "ata.h"
#include "../platform/serial.h"

static int g_ata_drive_present = 0;

// Helper for port I/O (assuming these are defined elsewhere, but adding inline here for safety)
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static int ata_wait_bsy() {
    int timeout = 100000;
    while (inb(ATA_PRIMARY_STATUS) & ATA_STATUS_BSY) {
        if (--timeout == 0) return -1;
    }
    return 0;
}

static int ata_wait_drq() {
    int timeout = 100000;
    while (!(inb(ATA_PRIMARY_STATUS) & ATA_STATUS_DRQ)) {
        if (--timeout == 0) return -1;
    }
    return 0;
}

void ata_init() {
    serial_write_str("[ATA] Initializing Primary Master...\r\n");
    
    // Select Drive (Primary Master)
    outb(ATA_PRIMARY_DRIVE_SEL, 0xA0);
    
    // Send Identify Command
    outb(ATA_PRIMARY_SECCOUNT, 0);
    outb(ATA_PRIMARY_LBA_LO, 0);
    outb(ATA_PRIMARY_LBA_MID, 0);
    outb(ATA_PRIMARY_LBA_HI, 0);
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_IDENTIFY);
    
    uint8_t status = inb(ATA_PRIMARY_STATUS);
    if (status == 0) {
        serial_write_str("[ATA] No drive detected on primary bus.\r\n");
        return;
    }
    
    ata_wait_bsy();
    
    // Check for error
    if (inb(ATA_PRIMARY_STATUS) & ATA_STATUS_ERR) {
        serial_write_str("[ATA] Error during IDENTIFY.\r\n");
        return;
    }
    
    ata_wait_drq();
    
    // Read 256 words (512 bytes) of identify data
    uint16_t identify_data[256];
    for (int i = 0; i < 256; i++) {
        identify_data[i] = inw(ATA_PRIMARY_DATA);
    }
    
    g_ata_drive_present = 1;
    serial_write_str("[ATA] Drive Identified.\r\n");
}

int ata_read_sectors(uint32_t lba, uint8_t count, uint8_t* buf) {
    ata_wait_bsy();
    
    outb(ATA_PRIMARY_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_SECCOUNT, count);
    outb(ATA_PRIMARY_LBA_LO, (uint8_t)lba);
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HI, (uint8_t)(lba >> 16));
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_READ_PIO);
    
    uint16_t* ptr = (uint16_t*)buf;
    for (int i = 0; i < count; i++) {
        ata_wait_bsy();
        ata_wait_drq();
        
        for (int j = 0; j < 256; j++) {
            *ptr++ = inw(ATA_PRIMARY_DATA);
        }
    }
    
    return 0;
}

int ata_write_sectors(uint32_t lba, uint8_t count, const uint8_t* buf) {
    ata_wait_bsy();
    
    outb(ATA_PRIMARY_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_SECCOUNT, count);
    outb(ATA_PRIMARY_LBA_LO, (uint8_t)lba);
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HI, (uint8_t)(lba >> 16));
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_WRITE_PIO);
    
    uint16_t* ptr = (uint16_t*)buf;
    for (int i = 0; i < count; i++) {
        ata_wait_bsy();
        ata_wait_drq();
        
        for (int j = 0; j < 256; j++) {
            outw(ATA_PRIMARY_DATA, *ptr++);
        }
    }
    
    // Flush Cache
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_CACHE_FLUSH);
    ata_wait_bsy();
    
    return 0;
}

int ata_is_drive_present() {
    return g_ata_drive_present;
}
