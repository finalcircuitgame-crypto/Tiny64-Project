#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include <stdint.h>

#include "../../c/fs/ramfs.h"
#include "../../c/lib/string.h"
#include "../../c/memory/heap.h"

class Persistence {
public:
    static bool IsVirtualMachine() {
        uint32_t eax, ebx, ecx, edx;
        eax = 1;
        __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(eax));
        return (ecx & (1 << 31)) != 0;
    }

    static bool SaveSettings(const char* username) {
        RamNode* cfg = ramfs_create("user.name", FS_FILE);
        if (cfg) ramfs_write_file(cfg, username);
        return true;
    }

    static const char* LoadUsername() {
        RamNode* root = ramfs_get_root();
        RamNode* cur = root->child;
        while(cur) {
            if (strcmp(cur->name, "user.name") == 0) return (const char*)cur->content;
            cur = cur->next;
        }
        return "Guest";
    }

    static void SaveTheme(int theme) {
        char buf[2];
        buf[0] = '0' + theme;
        buf[1] = 0;
        RamNode* cfg = ramfs_create("theme.cfg", FS_FILE);
        if (cfg) ramfs_write_file(cfg, buf);
    }

    static int LoadTheme() {
        RamNode* root = ramfs_get_root();
        RamNode* cur = root->child;
        while(cur) {
            if (strcmp(cur->name, "theme.cfg") == 0) {
                if (cur->content) return cur->content[0] - '0';
            }
            cur = cur->next;
        }
        return 0; // Default
    }

    static void SaveWallpaper(int wp) {
        char buf[2]; buf[0] = '0' + wp; buf[1] = 0;
        RamNode* cfg = ramfs_create("wallpaper.cfg", FS_FILE);
        if (cfg) ramfs_write_file(cfg, buf);
    }

    static int LoadWallpaper() {
        RamNode* root = ramfs_get_root();
        RamNode* cur = root->child;
        while(cur) {
            if (strcmp(cur->name, "wallpaper.cfg") == 0) {
                return cur->content ? cur->content[0] - '0' : 0;
            }
            cur = cur->next;
        }
        return 0;
    }
};

#endif
