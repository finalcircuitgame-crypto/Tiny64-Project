#include "fat32.h"
#include "xhci.h"
#include "../drivers/platform/serial.h"
#include "../memory/heap.h"
#include "../lib/string.h"

static int fat32_read_sectors(fat32_fs_t* fs, uint32_t lba, uint16_t count, void* buf) {
    return xhci_msc_read_sectors(fs->slot_id, fs->partition_lba + lba, count, buf);
}

static int fat32_write_sectors(fat32_fs_t* fs, uint32_t lba, uint16_t count, const void* buf) {
    return xhci_msc_write_sectors(fs->slot_id, fs->partition_lba + lba, count, buf);
}

int fat32_init(uint8_t slot_id, fat32_fs_t* fs) {
    fs->slot_id = slot_id;
    fs->partition_lba = 0;

    uint8_t mbr[512];
    if (xhci_msc_read_sectors(slot_id, 0, 1, mbr) != 0) return -1;

    // Check MBR Signature
    if (mbr[510] != 0x55 || mbr[511] != 0xAA) return -1;

    // Check Partition 1 (Offset 446)
    uint32_t p1_lba = *(uint32_t*)&mbr[446 + 8];
    uint8_t p1_type = mbr[446 + 4];

    if (p1_type == 0x0C || p1_type == 0x0B) { // FAT32 LBA or CHS
        fs->partition_lba = p1_lba;
        serial_write_str("FAT32: Found Partition at LBA ");
        serial_write_dec(p1_lba);
        serial_write_str("\r\n");
    }

    if (fat32_read_sectors(fs, 0, 1, &fs->bpb) != 0) return -1;

    if (strncmp(fs->bpb.file_system_type, "FAT32", 5) != 0) {
        // Some FAT32 formatted drives don't have "FAT32" here, check total_sectors_32
        if (fs->bpb.sectors_per_fat_32 == 0) return -1;
    }

    fs->fat_lba = fs->bpb.reserved_sectors;
    fs->data_lba = fs->fat_lba + (fs->bpb.fat_count * fs->bpb.sectors_per_fat_32);

    serial_write_str("FAT32: Initialized. Root Cluster: ");
    serial_write_dec(fs->bpb.root_cluster);
    serial_write_str("\r\n");

    return 0;
}

static uint32_t fat32_get_free_cluster(fat32_fs_t* fs) {
    uint32_t* fat = (uint32_t*)kmalloc(fs->bpb.bytes_per_sector);
    for (uint32_t i = 0; i < fs->bpb.sectors_per_fat_32; i++) {
        fat32_read_sectors(fs, fs->fat_lba + i, 1, fat);
        for (uint32_t j = 0; j < fs->bpb.bytes_per_sector / 4; j++) {
            if ((fat[j] & 0x0FFFFFFF) == 0) {
                uint32_t cluster = (i * (fs->bpb.bytes_per_sector / 4)) + j;
                if (cluster >= 2) {
                    kfree(fat);
                    return cluster;
                }
            }
        }
    }
    kfree(fat);
    return 0;
}

static void fat32_set_cluster(fat32_fs_t* fs, uint32_t cluster, uint32_t value) {
    uint32_t* fat = (uint32_t*)kmalloc(fs->bpb.bytes_per_sector);
    uint32_t sector = fs->fat_lba + (cluster * 4 / fs->bpb.bytes_per_sector);
    uint32_t offset = (cluster * 4 % fs->bpb.bytes_per_sector) / 4;

    fat32_read_sectors(fs, sector, 1, fat);
    fat[offset] = (fat[offset] & 0xF0000000) | (value & 0x0FFFFFFF);
    fat32_write_sectors(fs, sector, 1, fat);
    
    // Update second FAT if present
    if (fs->bpb.fat_count > 1) {
        fat32_write_sectors(fs, sector + fs->bpb.sectors_per_fat_32, 1, fat);
    }
    kfree(fat);
}

int fat32_save_file(fat32_fs_t* fs, const char* name_8_3, const char* data, uint32_t size) {
    // 1. Find a free cluster
    uint32_t cluster = fat32_get_free_cluster(fs);
    if (cluster == 0) return -1;

    // 2. Write data to cluster
    uint32_t cluster_lba = fs->data_lba + (cluster - 2) * fs->bpb.sectors_per_cluster;
    uint32_t sectors_needed = (size + 511) / 512;
    
    // Simple implementation: file must fit in one cluster or we only write one cluster's worth
    uint32_t sectors_to_write = (sectors_needed > fs->bpb.sectors_per_cluster) ? fs->bpb.sectors_per_cluster : sectors_needed;
    
    // Pad data with zeros to full sectors if needed
    char* padded_data = (char*)kmalloc(sectors_to_write * 512);
    memset(padded_data, 0, sectors_to_write * 512);
    memcpy(padded_data, data, size > sectors_to_write * 512 ? sectors_to_write * 512 : size);

    if (fat32_write_sectors(fs, cluster_lba, sectors_to_write, padded_data) != 0) {
        kfree(padded_data);
        return -1;
    }
    kfree(padded_data);

    // 3. Mark cluster as EOF
    fat32_set_cluster(fs, cluster, 0x0FFFFFFF);

    // 4. Create Directory Entry in Root Directory (simplified: assuming root is in one sector)
    fat32_directory_entry_t* root = (fat32_directory_entry_t*)kmalloc(fs->bpb.bytes_per_sector);
    uint32_t root_lba = fs->data_lba + (fs->bpb.root_cluster - 2) * fs->bpb.sectors_per_cluster;
    
    fat32_read_sectors(fs, root_lba, 1, root);

    int entry_index = -1;
    for (int i = 0; i < fs->bpb.bytes_per_sector / sizeof(fat32_directory_entry_t); i++) {
        if (root[i].filename[0] == 0x00 || root[i].filename[0] == 0xE5) {
            entry_index = i;
            break;
        }
    }

    if (entry_index == -1) {
        kfree(root);
        return -1;
    }

    memset(&root[entry_index], 0, sizeof(fat32_directory_entry_t));
    
    // Parse name (simplified: expects "FILENAMEEXT")
    memcpy(root[entry_index].filename, name_8_3, 8);
    memcpy(root[entry_index].ext, name_8_3 + 8, 3);
    root[entry_index].attributes = 0x20; // Archive
    root[entry_index].cluster_hi = (uint16_t)(cluster >> 16);
    root[entry_index].cluster_lo = (uint16_t)(cluster & 0xFFFF);
    root[entry_index].file_size = size;

    fat32_write_sectors(fs, root_lba, 1, root);
    kfree(root);

    return 0;
}
