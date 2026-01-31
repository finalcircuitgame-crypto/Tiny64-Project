#ifndef DISK_H
#define DISK_H

#include <stdint.h>

typedef enum {
    DISK_TYPE_ATA,
    DISK_TYPE_AHCI,
    DISK_TYPE_NVME
} disk_type_t;

typedef struct {
    disk_type_t type;
    int port_or_id;
    uint64_t size_in_sectors;
    uint32_t block_size;
    const char* name;
} disk_info_t;

void disk_register(disk_info_t info);
int disk_read(int disk_id, uint64_t lba, uint32_t count, void* buf);
int disk_get_count();
disk_info_t* disk_get_info(int disk_id);

#endif
