#include "vmm.h"
#include "pmm.h"
#include "../lib/string.h"
#include "../drivers/platform/serial.h"

// Global pointer to the top-level PML4; will be patched with CR3 at early boot
uint64_t *g_pml4 = 0;

static inline uint64_t *get_pml4() {
  if (!g_pml4) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    g_pml4 = (uint64_t *)(cr3 & ~0xFFFULL); // assume identity-mapped
  }
  return g_pml4;
}

void vmm_init() {
    // 1. Allocate a new PML4 page
    uint64_t *new_pml4 = (uint64_t *)pmm_alloc_page();
    memset(new_pml4, 0, 4096);
    
    // 2. Get the current (UEFI) PML4
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    uint64_t *old_pml4 = (uint64_t *)(cr3 & ~0xFFFULL);
    
    // 3. Copy the existing PML4 entries to preserve current mappings
    // (like the kernel, UEFI structures, etc.)
    for (int i = 0; i < 512; i++) {
        new_pml4[i] = old_pml4[i];
    }
    
    // 4. Ensure identity mapping for the first 4GB
    // (This is a safety measure to ensure pmm_alloc_page addresses are reachable)
    // We'll reuse existing entries if possible, but let's just make sure index 0,1,2,3 
    // are covered if they aren't already.
    // Actually, copying all 512 entries should be enough if UEFI mapped everything.
    
    // 5. Switch to the new PML4
    g_pml4 = new_pml4;
    __asm__ volatile("mov %0, %%cr3" : : "r"(new_pml4));
    
    serial_write_str("[VMM] Switched to writable PML4 at ");
    serial_write_hex((uint64_t)new_pml4);
    serial_write_str("\r\n");
}

#define PTE_PRESENT (1ULL << 0)
#define PTE_WRITE (1ULL << 1)
#define PTE_PS (1ULL << 7)  // 2 MiB page
#define PTE_PCD (1ULL << 4) // cache disable
#define PTE_PWT (1ULL << 3) // write-through

void map_page_2m(uintptr_t virt, uint64_t phys, uint64_t flags) {
  uint64_t *pml4 = get_pml4();
  uint64_t pml4_idx = (virt >> 39) & 0x1FF;
  uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
  uint64_t pd_idx = (virt >> 21) & 0x1FF;

  if (!(pml4[pml4_idx] & PTE_PRESENT)) {
    uint64_t *pdpt = (uint64_t *)pmm_alloc_page();
    memset(pdpt, 0, 4096);
    pml4[pml4_idx] = (uint64_t)pdpt | PTE_PRESENT | PTE_WRITE;
  }
  uint64_t *pdpt = (uint64_t *)(pml4[pml4_idx] & ~0xFFFULL);

  if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
    uint64_t *pd = (uint64_t *)pmm_alloc_page();
    memset(pd, 0, 4096);
    pdpt[pdpt_idx] = (uint64_t)pd | PTE_PRESENT | PTE_WRITE;
  }
  uint64_t *pd = (uint64_t *)(pdpt[pdpt_idx] & ~0xFFFULL);

  pd[pd_idx] = (phys & ~0x1FFFFFULL) | PTE_PRESENT | PTE_WRITE | PTE_PS | flags;
  
  __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

uintptr_t g_vmm_mmio_cursor = 0xFFFFC00000000000;

void *vmm_alloc_region(size_t size) {
    uintptr_t ret = g_vmm_mmio_cursor;
    // Align to 2MB page
    size = (size + 0x1FFFFF) & ~0x1FFFFFULL;
    g_vmm_mmio_cursor += size;
    return (void *)ret;
}

void *map_mmio(uintptr_t phys, size_t size) {
  // For small MMIO regions, we can just identity map them if they are high
  // or use a dedicated virtual range.
  // To keep it simple and avoid conflicts with lower identity mapping,
  // we'll map them into the high-half virtual space.
  
  uintptr_t virt_base = (uintptr_t)vmm_alloc_region(size);
  uintptr_t curr_virt = virt_base;
  uint64_t curr_phys = phys & ~0x1FFFFFULL;
  uint64_t offset = phys & 0x1FFFFF;
  
  size_t mapped = 0;
  while (mapped < size + offset) {
    map_page_2m(curr_virt, curr_phys, PAGE_NOCACHE);
    curr_virt += 0x200000;
    curr_phys += 0x200000;
    mapped += 0x200000;
  }
  
  return (void *)(virt_base + offset);
}
