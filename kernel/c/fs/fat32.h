#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t  jmp[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_entries; // 0 for FAT32
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;

    // FAT32 Extended fields
    uint32_t sectors_per_fat_32;
    uint16_t flags;
    uint16_t version;
    uint32_t root_cluster;
    uint16_t fs_info_sector;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     file_system_type[8];
} __attribute__((packed)) fat32_bpb_t;

typedef struct {
    char     filename[8];
    char     ext[3];
    uint8_t  attributes;
    uint8_t  reserved;
    uint8_t  creation_time_ms;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t cluster_hi;
    uint16_t last_mod_time;
    uint16_t last_mod_date;
    uint16_t cluster_lo;
    uint32_t file_size;
} __attribute__((packed)) fat32_directory_entry_t;

typedef struct {
    uint8_t  slot_id;
    uint32_t partition_lba;
    fat32_bpb_t bpb;
    uint32_t fat_lba;
    uint32_t data_lba;
} fat32_fs_t;

// Initialize FAT32 on a specific USB slot
int fat32_init(uint8_t slot_id, fat32_fs_t* fs);

// Save a file to the root directory
int fat32_save_file(fat32_fs_t* fs, const char* name_8_3, const char* data, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif