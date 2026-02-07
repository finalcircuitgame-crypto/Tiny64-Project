#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

typedef struct {
  uint64_t FramebufferBase;
  uint64_t FramebufferSize;
  uint32_t HorizontalResolution;
  uint32_t VerticalResolution;
  uint32_t PixelsPerScanLine;
  uint32_t Pad; // Alignment padding
} FramebufferInfo;

typedef struct {
  uint32_t Type;
  uint32_t Pad;
  uint64_t PhysicalStart;
  uint64_t VirtualStart;
  uint64_t NumberOfPages;
  uint64_t Attribute;
} MemoryDescriptor;

typedef struct {
  FramebufferInfo fb;
  uint64_t mmap_addr;
  uint64_t mmap_size;
  uint64_t mmap_desc_size;
} BootInfo;

#endif
