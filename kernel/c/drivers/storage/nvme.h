#ifndef NVME_H
#define NVME_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// NVMe Controller Registers (MMIO)
typedef struct {
    uint64_t cap;       // Controller Capabilities
    uint32_t vs;        // Version
    uint32_t intms;     // Interrupt Mask Set
    uint32_t intmc;     // Interrupt Mask Clear
    uint32_t cc;        // Controller Configuration
    uint32_t reserved0;
    uint32_t csts;      // Controller Status
    uint32_t nssr;      // NVM Subsystem Reset
    uint32_t aqa;       // Admin Queue Attributes
    uint64_t asq;       // Admin Submission Queue Base Address
    uint64_t acq;       // Admin Completion Queue Base Address
    uint32_t cmbloc;    // Controller Memory Buffer Location
    uint32_t cmbsz;     // Controller Memory Buffer Size
    // Doorbeels start at offset 0x1000
} __attribute__((packed)) nvme_regs_t;

typedef struct {
    uint32_t opc : 8;
    uint32_t fuse : 2;
    uint32_t reserved0 : 4;
    uint32_t psdt : 2;
    uint32_t cid : 16;
    uint32_t nsid;
    uint32_t reserved1[2];
    uint64_t mptr;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} __attribute__((packed)) nvme_command_t;

typedef struct {
    uint32_t dw0;
    uint32_t reserved;
    uint16_t sqhd;
    uint16_t sqid;
    uint16_t cid;
    uint16_t status;
} __attribute__((packed)) nvme_completion_t;

typedef struct {
    uintptr_t base;
    nvme_regs_t* regs;
    
    nvme_command_t* admin_sq;
    nvme_completion_t* admin_cq;
    uint16_t admin_sq_tail;
    uint16_t admin_cq_head;
    uint16_t admin_cq_phase;
    
    nvme_command_t* io_sq;
    nvme_completion_t* io_cq;
    uint16_t io_sq_tail;
    uint16_t io_cq_head;
    uint16_t io_cq_phase;
    
    uint32_t db_stride;
    uint32_t nsid;
    uint64_t block_count;
    uint32_t block_size;
} nvme_device_t;

void nvme_init(uintptr_t base_addr);
int nvme_read(uint64_t lba, uint32_t count, void* buf);
int nvme_write(uint64_t lba, uint32_t count, const void* buf);

#ifdef __cplusplus
}
#endif

#endif
