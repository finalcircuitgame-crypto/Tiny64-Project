#include "pmm.h"
#include "serial.h"

static uint8_t *bitmap;
static uint64_t total_pages;
static uint64_t free_pages;
static uint64_t bitmap_size;

void pmm_init(BootInfo *boot_info) {
  uintn_t entry_count = boot_info->mmap_size / boot_info->mmap_desc_size;
  uint64_t max_addr = 0;

  // First pass: Find the highest memory address to determine total pages
  for (uintn_t i = 0; i < entry_count; i++) {
    MemoryDescriptor *desc =
        (MemoryDescriptor *)(boot_info->mmap_addr +
                             (i * boot_info->mmap_desc_size));
    uint64_t end_addr = desc->PhysicalStart + (desc->NumberOfPages * PAGE_SIZE);
    if (end_addr > max_addr)
      max_addr = end_addr;
  }

  total_pages = max_addr / PAGE_SIZE;
  bitmap_size = total_pages / 8;
  if (total_pages % 8 != 0)
    bitmap_size++;

  // Find a spot for the bitmap. We need a conventional memory region large
  // enough.
  bitmap = 0;
  for (uintn_t i = 0; i < entry_count; i++) {
    MemoryDescriptor *desc =
        (MemoryDescriptor *)(boot_info->mmap_addr +
                             (i * boot_info->mmap_desc_size));
    if (desc->Type == 7 && desc->NumberOfPages * PAGE_SIZE >= bitmap_size) {
      bitmap = (uint8_t *)desc->PhysicalStart;
      // Mark these pages as used in the second pass
      break;
    }
  }

  if (!bitmap) {
    serial_println("PMM Error: Could not find space for bitmap!");
    while (1)
      ;
  }

  // Initialize bitmap: Mark everything as used (1)
  for (uint64_t i = 0; i < bitmap_size; i++) {
    bitmap[i] = 0xFF;
  }

  // Second pass: Mark conventional memory as free (0)
  free_pages = 0;
  for (uintn_t i = 0; i < entry_count; i++) {
    MemoryDescriptor *desc =
        (MemoryDescriptor *)(boot_info->mmap_addr +
                             (i * boot_info->mmap_desc_size));
    if (desc->Type == 7) {
      uint64_t start_page = desc->PhysicalStart / PAGE_SIZE;
      for (uint64_t p = 0; p < desc->NumberOfPages; p++) {
        uint64_t page = start_page + p;
        bitmap[page / 8] &= ~(1 << (page % 8));
        free_pages++;
      }
    }
  }

  // Mark bitmap's own pages as used
  uint64_t bitmap_start_page = (uint64_t)bitmap / PAGE_SIZE;
  uint64_t bitmap_pages = bitmap_size / PAGE_SIZE;
  if (bitmap_size % PAGE_SIZE != 0)
    bitmap_pages++;

  for (uint64_t p = 0; p < bitmap_pages; p++) {
    uint64_t page = bitmap_start_page + p;
    if (!(bitmap[page / 8] & (1 << (page % 8)))) {
      bitmap[page / 8] |= (1 << (page % 8));
      free_pages--;
    }
  }

  // Mark Page 0 as used (usually bad to allocate)
  if (!(bitmap[0] & 1)) {
    bitmap[0] |= 1;
    free_pages--;
  }

  serial_print("PMM Initialized. Free Pages: ");
  serial_print_hex(free_pages);
  serial_println("");
}

void *pmm_alloc_page() {
  for (uint64_t i = 0; i < total_pages; i++) {
    if (!(bitmap[i / 8] & (1 << (i % 8)))) {
      bitmap[i / 8] |= (1 << (i % 8));
      free_pages--;
      return (void *)(i * PAGE_SIZE);
    }
  }
  return 0;
}

void pmm_free_page(void *page) {
  uint64_t p = (uint64_t)page / PAGE_SIZE;
  if (bitmap[p / 8] & (1 << (p % 8))) {
    bitmap[p / 8] &= ~(1 << (p % 8));
    free_pages++;
  }
}

uint64_t pmm_get_free_memory() { return free_pages * PAGE_SIZE; }
uint64_t pmm_get_total_memory() { return total_pages * PAGE_SIZE; }
