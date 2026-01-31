#ifndef ATA_H
#define ATA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ATA Ports
#define ATA_PRIMARY_DATA         0x1F0
#define ATA_PRIMARY_ERR          0x1F1
#define ATA_PRIMARY_SECCOUNT     0x1F2
#define ATA_PRIMARY_LBA_LO       0x1F3
#define ATA_PRIMARY_LBA_MID      0x1F4
#define ATA_PRIMARY_LBA_HI       0x1F5
#define ATA_PRIMARY_DRIVE_SEL    0x1F6
#define ATA_PRIMARY_COMMAND      0x1F7
#define ATA_PRIMARY_STATUS       0x1F7
#define ATA_PRIMARY_CONTROL      0x3F6

// ATA Commands
#define ATA_CMD_READ_PIO          0x20
#define ATA_CMD_WRITE_PIO         0x30
#define ATA_CMD_CACHE_FLUSH       0xE7
#define ATA_CMD_IDENTIFY          0xEC

// ATA Status Bits
#define ATA_STATUS_ERR            0x01
#define ATA_STATUS_DRQ            0x08
#define ATA_STATUS_SRV            0x10
#define ATA_STATUS_DF             0x20
#define ATA_STATUS_RDY            0x40
#define ATA_STATUS_BSY            0x80

void ata_init();
int ata_read_sectors(uint32_t lba, uint8_t count, uint8_t* buf);
int ata_write_sectors(uint32_t lba, uint8_t count, const uint8_t* buf);
int ata_is_drive_present();

#ifdef __cplusplus
}
#endif

#endif
