#ifndef DMA_H
#define DMA_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Allocates a physically contiguous region of memory suitable for DMA.
 * Returns the virtual pointer and stores the physical address in out_phys.
 * The memory is zero-initialized and synchronized for device access.
 */
void* dma_alloc_coherent(size_t size, uint64_t* out_phys);

/**
 * Frees a DMA region previously allocated with dma_alloc_coherent.
 */
void dma_free_coherent(void* v, uint64_t phys, size_t size);

/**
 * Returns the physical address of a virtual pointer in the DMA region.
 * Reliable only for buffers allocated through the DMA allocator or identity-mapped regions.
 */
uint64_t virt_to_phys(void* v);

/**
 * Flushes CPU caches for the specified range to ensure the device sees the latest CPU writes.
 */
void dma_sync_for_device(void* v, size_t size);

/**
 * Ensures the CPU sees the latest writes from the device.
 */
void dma_sync_for_cpu(void* v, size_t size);

#ifdef __cplusplus
}
#endif

#endif
