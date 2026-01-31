#ifndef RAMFS_H
#define RAMFS_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FS_FILE 0
#define FS_DIR 1

typedef struct RamNode {
    char name[32];
    uint8_t type;
    struct RamNode* parent;
    struct RamNode* child; // First child (if dir)
    struct RamNode* next;  // Next sibling
    uint8_t* content;
    uint32_t size;
} RamNode;

void ramfs_init();
RamNode* ramfs_create(const char* name, uint8_t type);
RamNode* ramfs_get_root();
void ramfs_write_file(RamNode* file, const char* data);

#ifdef __cplusplus
}
#endif

#endif
