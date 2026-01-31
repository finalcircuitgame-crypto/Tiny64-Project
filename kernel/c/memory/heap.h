#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* kmalloc(size_t size);
void kfree(void* ptr);
size_t kmalloc_checkpoint(void);
void kmalloc_rewind(size_t ckpt);

#ifdef __cplusplus
}
#endif

#endif
