#include "heap.h"
#include "pmm.h"
#include "../drivers/platform/serial.h"
#include <stdint.h>

#define HEAP_SIZE (128 * 1024 * 1024) /* 128 MB */

typedef struct heap_node {
    size_t size;
    struct heap_node* next;
    int free;
} heap_node_t;

static uint8_t* heap_base = 0;
static size_t heap_offset = 0;
static heap_node_t* free_list = 0;

void* kmalloc(size_t size) {
    if (!heap_base) {
        size_t pages = HEAP_SIZE / 4096;
        heap_base = (uint8_t*)pmm_alloc_pages(pages);
        if (!heap_base) {
            serial_write_str("kmalloc: CRITICAL: Failed to allocate contiguous heap!\r\n");
            return 0;
        }
        serial_write_str("kmalloc: Heap initialized at ");
        serial_write_hex((uint64_t)heap_base);
        serial_write_str(" size ");
        serial_write_dec(HEAP_SIZE);
        serial_write_str("\r\n");
        heap_offset = 0;
        free_list = 0;
    }

    // Align to 16 bytes
    size = (size + 15) & ~15;
    size_t total_size = size + sizeof(heap_node_t);

    // Try to find a free block
    heap_node_t* curr = free_list;
    heap_node_t* prev = 0;
    while (curr) {
        if (curr->free && curr->size >= size) {
            // Found a block. Split it if it's too big
            if (curr->size >= size + sizeof(heap_node_t) + 16) {
                heap_node_t* next_node = (heap_node_t*)((uint8_t*)curr + total_size);
                next_node->size = curr->size - total_size;
                next_node->next = curr->next;
                next_node->free = 1;
                
                curr->size = size;
                curr->next = next_node;
            }
            curr->free = 0;
            return (void*)((uint8_t*)curr + sizeof(heap_node_t));
        }
        prev = curr;
        curr = curr->next;
    }

    // No suitable free block found, use bump allocator
    if (heap_offset + total_size > HEAP_SIZE) {
        serial_write_str("kmalloc: Out of memory! Requested: ");
        serial_write_dec(size);
        serial_write_str(" Remaining: ");
        serial_write_dec(HEAP_SIZE - heap_offset);
        serial_write_str("\r\n");
        return 0; 
    }

    heap_node_t* node = (heap_node_t*)(heap_base + heap_offset);
    node->size = size;
    node->next = 0;
    node->free = 0;

    if (prev) {
        prev->next = node;
    } else if (!free_list) {
        free_list = node;
    }

    heap_offset += total_size;
    return (void*)((uint8_t*)node + sizeof(heap_node_t));
}

void kfree(void* ptr) {
    if (!ptr) return;
    
    heap_node_t* node = (heap_node_t*)((uint8_t*)ptr - sizeof(heap_node_t));
    node->free = 1;

    // Coalesce adjacent free blocks
    heap_node_t* curr = free_list;
    while (curr) {
        if (curr->free && curr->next && curr->next->free) {
            curr->size += curr->next->size + sizeof(heap_node_t);
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}

size_t kmalloc_checkpoint(void) {
    return heap_offset;
}

void kmalloc_rewind(size_t ckpt) {
    if (ckpt <= heap_offset) {
        heap_offset = ckpt;
    }
}

void* kcalloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void* ptr = kmalloc(total);
    if (ptr) {
        uint8_t* p = (uint8_t*)ptr;
        for (size_t i = 0; i < total; i++) p[i] = 0;
    }
    return ptr;
}
