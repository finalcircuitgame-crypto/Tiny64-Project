#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void heap_init(void);
void* malloc(size_t size);
void free(void* ptr);

#ifdef __cplusplus
}
#endif

#endif
