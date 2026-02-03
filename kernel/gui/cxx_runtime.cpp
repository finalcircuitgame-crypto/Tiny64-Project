#include <stddef.h>
#include <kernel/mm/heap.h>

// Minimal C++ runtime support for freestanding kernel

void* operator new(size_t size) {
    return malloc(size);
}

void* operator new[](size_t size) {
    return malloc(size);
}

void operator delete(void* p) {
    free(p);
}

void operator delete[](void* p) {
    free(p);
}

void operator delete(void* p, size_t size) {
    (void)size;
    free(p);
}

void operator delete[](void* p, size_t size) {
    (void)size;
    free(p);
}

extern "C" void __cxa_pure_virtual() {
    // Hang or serial error
    while (1);
}
