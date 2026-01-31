#include "hardware.h"
#include "../../c/memory/heap.h"

extern "C" void _optimize_system() {
    Hardware::OptimizeSystem();
}

void* operator new(size_t size) {
    return kmalloc(size);
}

void* operator new[](size_t size) {
    return kmalloc(size);
}

void operator delete(void* p) noexcept {
    kfree(p);
}

void operator delete[](void* p) noexcept {
    kfree(p);
}

void operator delete(void* p, size_t size) noexcept {
    (void)size;
    kfree(p);
}

void operator delete[](void* p, size_t size) noexcept {
    (void)size;
    kfree(p);
}

// Basic pure virtual handler
extern "C" void __cxa_pure_virtual() {
    // Hang or panic
    while (1);
}
