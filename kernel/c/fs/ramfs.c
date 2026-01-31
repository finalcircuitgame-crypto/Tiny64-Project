#include "ramfs.h"
#include "../lib/string.h"

// Simple static allocator for this prototype
static RamNode node_pool[128];
static int pool_idx = 0;
static RamNode* root_dir;

void ramfs_init() {
    pool_idx = 0;
    root_dir = &node_pool[pool_idx++];
    strcpy(root_dir->name, "/");
    root_dir->type = FS_DIR;
    root_dir->parent = root_dir; // Parent of root is root
    root_dir->child = 0;
    root_dir->next = 0;
    
    // Create some default files
    RamNode* readme = ramfs_create("readme.txt", FS_FILE);
    if (readme) ramfs_write_file(readme, "Welcome to Tiny64 OS!");
    
    RamNode* cmdlist = ramfs_create("commands.list", FS_FILE);
    if (cmdlist) ramfs_write_file(cmdlist, "ls, cat, touch, explorer");

    // Add Downloads directory
    ramfs_create("Downloads", FS_DIR);
}

RamNode* ramfs_alloc_node() {
    if (pool_idx >= 128) return 0;
    return &node_pool[pool_idx++];
}

RamNode* ramfs_create(const char* name, uint8_t type) {
    RamNode* node = ramfs_alloc_node();
    if (!node) return 0;
    strcpy(node->name, name);
    node->type = type;
    node->size = 0;
    node->content = 0;
    node->child = 0;
    
    // Add to root (simple flat FS for now)
    if (!root_dir->child) {
        root_dir->child = node;
    } else {
        RamNode* cur = root_dir->child;
        while (cur->next) cur = cur->next;
        cur->next = node;
    }
    node->parent = root_dir;
    node->next = 0;
    return node;
}

void ramfs_write_file(RamNode* file, const char* data) {
    size_t len = strlen(data);
    file->size = len;
    
    // Allocate space for the content
    extern void* kmalloc(size_t);
    file->content = (uint8_t*)kmalloc(len + 1);
    if (file->content) {
        memcpy(file->content, data, len + 1);
    }
}

RamNode* ramfs_get_root() { return root_dir; }
