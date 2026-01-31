#ifndef EFI_MEMORY_H
#define EFI_MEMORY_H

#include <stdint.h>

typedef struct {
    uint32_t Type;
    uint32_t Pad;
    uint64_t PhysicalStart;
    uint64_t VirtualStart;
    uint64_t NumberOfPages;
    uint64_t Attribute;
} EFI_MEMORY_DESCRIPTOR;

extern const char* EFI_MEMORY_TYPE_STRINGS[];

#endif
