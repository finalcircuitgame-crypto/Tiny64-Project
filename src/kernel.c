#include "common.h"
#include "pmm.h"
#include "serial.h"


typedef uint64_t uintn_t;

__attribute__((section(".text._start"))) void _start(BootInfo *boot_info) {
  serial_println("--- Tiny64 Kernel Started ---");

  if (boot_info == 0) {
    serial_println("Error: boot_info is NULL!");
    while (1)
      ;
  }

  FramebufferInfo *fb = &boot_info->fb;
  serial_print("Framebuffer Base: ");
  serial_print_hex(fb->FramebufferBase);
  serial_println("");

  serial_print("Memory Map Addr: ");
  serial_print_hex(boot_info->mmap_addr);
  serial_println("");

  // Calculate total memory
  uint64_t total_mem = 0;
  uintn_t entry_count = boot_info->mmap_size / boot_info->mmap_desc_size;

  for (uintn_t i = 0; i < entry_count; i++) {
    MemoryDescriptor *desc =
        (MemoryDescriptor *)(boot_info->mmap_addr +
                             (i * boot_info->mmap_desc_size));
    // Type 7 is EfiConventionalMemory
    if (desc->Type == 7) {
      total_mem += desc->NumberOfPages * 4096;
    }
  }

  serial_print("Total Usable Memory: ");
  serial_print_hex(total_mem);
  serial_println(" bytes.");

  uint32_t *framebuffer = (uint32_t *)fb->FramebufferBase;
  for (uint64_t i = 0; i < fb->HorizontalResolution * fb->VerticalResolution;
       i++) {
    framebuffer[i] = 0x00003366;
  }

  uint32_t color = 0xFFFFFFFF;
  for (uint32_t y = 100; y < 200; y++) {
    for (uint32_t x = 100; x < 200; x++) {
      framebuffer[y * fb->PixelsPerScanLine + x] = color;
    }
  }

  serial_println("Kernel execution finished (looping).");
  while (1)
    ;
}
