#include <kernel/mm/heap.h>
#include <kernel/mm/pmm.h>
#include <lib/string.h>

#define HEAP_PAGE_COUNT 8192
static void* heap_start = NULL;
static size_t heap_used = 0;
static size_t heap_size = 0;

void heap_init(void) {
    // Allocate some pages for the heap
    heap_size = HEAP_PAGE_COUNT * 4096;
    heap_start = pmm_alloc_page(); // We should really allocate HEAP_PAGE_COUNT pages
    // But our PMM is a bump allocator, so calling it multiple times is fine
    for (int i = 1; i < HEAP_PAGE_COUNT; i++) {
        pmm_alloc_page();
    }
    heap_used = 0;
}

typedef struct {
    size_t size;
    size_t padding; // Ensure 16-byte alignment
} heap_header;

void* malloc(size_t size) {
    // Align to 16 bytes
    size = (size + 15) & ~15;
    
    if (heap_used + size + sizeof(heap_header) > heap_size) {
        return NULL;
    }
    
    heap_header* header = (heap_header*)((char*)heap_start + heap_used);
    header->size = size;
    
    void* ptr = (char*)header + sizeof(heap_header);
    heap_used += size + sizeof(heap_header);
    
    memset(ptr, 0, size);
    return ptr;
}

void free(void* ptr) {
    if (!ptr) return;
    
    heap_header* header = (heap_header*)((char*)ptr - sizeof(heap_header));
    
    // If this was the last allocation, we can "free" it by moving the bump pointer back
    if ((char*)ptr + header->size == (char*)heap_start + heap_used) {
        heap_used -= (header->size + sizeof(heap_header));
    }
}
