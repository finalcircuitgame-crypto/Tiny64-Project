#include "nvme.h"
#include "../../memory/pmm.h"
#include "../../memory/heap.h"
#include "../../lib/string.h"
#include "../platform/serial.h"

static nvme_device_t* g_nvme_dev = NULL;

static void nvme_write_sq_doorbell(nvme_device_t* dev, uint16_t qid, uint16_t tail) {
    volatile uint32_t* db = (volatile uint32_t*)((uintptr_t)dev->regs + 0x1000 + (2 * qid) * (4 << dev->db_stride));
    *db = tail;
}

static void nvme_write_cq_doorbell(nvme_device_t* dev, uint16_t qid, uint16_t head) {
    volatile uint32_t* db = (volatile uint32_t*)((uintptr_t)dev->regs + 0x1000 + (2 * qid + 1) * (4 << dev->db_stride));
    *db = head;
}

static int nvme_wait_csts_rdy(nvme_device_t* dev, int rdy) {
    int timeout = 1000000;
    while (((dev->regs->csts & 1) != rdy) && timeout-- > 0);
    return (timeout > 0) ? 0 : -1;
}

static int nvme_submit_admin_cmd(nvme_device_t* dev, nvme_command_t* cmd, nvme_completion_t* cpl) {
    uint16_t tail = dev->admin_sq_tail;
    memcpy(&dev->admin_sq[tail], cmd, sizeof(nvme_command_t));
    tail = (tail + 1) % 64;
    dev->admin_sq_tail = tail;
    nvme_write_sq_doorbell(dev, 0, tail);

    int timeout = 1000000;
    while (((dev->admin_cq[dev->admin_cq_head].status & 1) != dev->admin_cq_phase) && timeout-- > 0);
    
    if (timeout <= 0) return -1;

    if (cpl) memcpy(cpl, &dev->admin_cq[dev->admin_cq_head], sizeof(nvme_completion_t));
    
    uint16_t head = dev->admin_cq_head;
    uint16_t status = dev->admin_cq[head].status;
    head = (head + 1) % 64;
    if (head == 0) dev->admin_cq_phase ^= 1;
    dev->admin_cq_head = head;
    nvme_write_cq_doorbell(dev, 0, head);

    return (status >> 1) == 0 ? 0 : -1;
}

void nvme_init(uintptr_t base_addr) {
    serial_write_str("[NVMe] Initializing...\r\n");
    nvme_device_t* dev = (nvme_device_t*)kmalloc(sizeof(nvme_device_t));
    memset(dev, 0, sizeof(nvme_device_t));
    dev->base = base_addr;
    dev->regs = (nvme_regs_t*)base_addr;

    // 1. Reset Controller
    if (dev->regs->cc & 1) {
        dev->regs->cc &= ~1;
        nvme_wait_csts_rdy(dev, 0);
    }

    dev->db_stride = (dev->regs->cap >> 32) & 0xF;

    // 2. Setup Admin Queues (64 entries each)
    dev->admin_sq = (nvme_command_t*)pmm_alloc_page();
    dev->admin_cq = (nvme_completion_t*)pmm_alloc_page();
    memset(dev->admin_sq, 0, 4096);
    memset(dev->admin_cq, 0, 4096);
    dev->admin_cq_phase = 1;

    dev->regs->aqa = (63 << 16) | 63; // 64 entries each
    dev->regs->asq = (uintptr_t)dev->admin_sq;
    dev->regs->acq = (uintptr_t)dev->admin_cq;

    // 3. Enable Controller
    dev->regs->cc = (0 << 16) | (0 << 14) | (0 << 11) | (0 << 7) | (4 << 4) | 1; // CSS=NVM, AMS=RR, MPS=4KB, EN=1
    if (nvme_wait_csts_rdy(dev, 1) < 0) {
        serial_write_str("[NVMe] Fatal: Controller Ready Timeout\r\n");
        return;
    }

    // 4. Identify Controller
    nvme_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.opc = 0x06; // Identify
    cmd.nsid = 0;
    cmd.prp1 = (uintptr_t)pmm_alloc_page();
    cmd.cdw10 = 1; // Controller
    
    if (nvme_submit_admin_cmd(dev, &cmd, NULL) < 0) {
        serial_write_str("[NVMe] Identify Controller Failed\r\n");
        return;
    }
    pmm_free_page((void*)(uintptr_t)cmd.prp1);

    // 5. Create I/O Completion Queue
    dev->io_cq = (nvme_completion_t*)pmm_alloc_page();
    memset(dev->io_cq, 0, 4096);
    dev->io_cq_phase = 1;

    memset(&cmd, 0, sizeof(cmd));
    cmd.opc = 0x05; // Create IO CQ
    cmd.prp1 = (uintptr_t)dev->io_cq;
    cmd.cdw10 = (1023 << 16) | 1; // 1024 entries, QID=1
    cmd.cdw11 = 1; // Phys contiguous, no interrupts
    nvme_submit_admin_cmd(dev, &cmd, NULL);

    // 6. Create I/O Submission Queue
    dev->io_sq = (nvme_command_t*)pmm_alloc_page();
    memset(dev->io_sq, 0, 4096);

    memset(&cmd, 0, sizeof(cmd));
    cmd.opc = 0x01; // Create IO SQ
    cmd.prp1 = (uintptr_t)dev->io_sq;
    cmd.cdw10 = (1023 << 16) | 1; // 1024 entries, QID=1
    cmd.cdw11 = (1 << 16) | 1; // CQID=1, Phys contiguous
    nvme_submit_admin_cmd(dev, &cmd, NULL);

    // 7. Identify Namespace 1
    memset(&cmd, 0, sizeof(cmd));
    cmd.opc = 0x06;
    cmd.nsid = 1;
    uint8_t* ns_data = (uint8_t*)pmm_alloc_page();
    cmd.prp1 = (uintptr_t)ns_data;
    cmd.cdw10 = 0; // Namespace
    
    if (nvme_submit_admin_cmd(dev, &cmd, NULL) == 0) {
        dev->nsid = 1;
        dev->block_count = *(uint64_t*)(ns_data + 0);
        uint8_t flbas = ns_data[26];
        uint8_t lbaf_idx = flbas & 0xF;
        uint32_t lbaf = *(uint32_t*)(ns_data + 128 + lbaf_idx * 4);
        dev->block_size = 1 << ((lbaf >> 16) & 0xFF);
        serial_write_str("[NVMe] Found Namespace 1\r\n");
    }
    pmm_free_page(ns_data);

    g_nvme_dev = dev;
}

int nvme_read(uint64_t lba, uint32_t count, void* buf) {
    if (!g_nvme_dev) return -1;
    nvme_device_t* dev = g_nvme_dev;

    nvme_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.opc = 0x02; // Read
    cmd.nsid = dev->nsid;
    cmd.prp1 = (uintptr_t)buf;
    // PRP2 handling for more than 1-2 pages would be needed here for large reads
    // For simplicity, we assume small reads or contiguous physical memory if buffer > 4K
    if (count * dev->block_size > 4096) {
         // Tiny64 minimal: we only handle up to 4KB in one go or assume physical contiguity
         // In a real driver we'd build a PRP list.
         cmd.prp2 = (uintptr_t)buf + 4096;
    }
    
    cmd.cdw10 = (uint32_t)lba;
    cmd.cdw11 = (uint32_t)(lba >> 32);
    cmd.cdw12 = (count - 1); // 0-based count

    uint16_t tail = dev->io_sq_tail;
    memcpy(&dev->io_sq[tail], &cmd, sizeof(nvme_command_t));
    tail = (tail + 1) % 1024;
    dev->io_sq_tail = tail;
    nvme_write_sq_doorbell(dev, 1, tail);

    int timeout = 1000000;
    while (((dev->io_cq[dev->io_cq_head].status & 1) != dev->io_cq_phase) && timeout-- > 0);
    
    if (timeout <= 0) return -1;

    uint16_t status = dev->io_cq[dev->io_cq_head].status;
    uint16_t head = dev->io_cq_head;
    head = (head + 1) % 1024;
    if (head == 0) dev->io_cq_phase ^= 1;
    dev->io_cq_head = head;
    nvme_write_cq_doorbell(dev, 1, head);

    return (status >> 1) == 0 ? 0 : -1;
}

int nvme_write(uint64_t lba, uint32_t count, const void* buf) {
    if (!g_nvme_dev) return -1;
    nvme_device_t* dev = g_nvme_dev;

    nvme_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.opc = 0x01; // Write
    cmd.nsid = dev->nsid;
    cmd.prp1 = (uintptr_t)buf;
    if (count * dev->block_size > 4096) {
         cmd.prp2 = (uintptr_t)buf + 4096;
    }
    
    cmd.cdw10 = (uint32_t)lba;
    cmd.cdw11 = (uint32_t)(lba >> 32);
    cmd.cdw12 = (count - 1);

    uint16_t tail = dev->io_sq_tail;
    memcpy(&dev->io_sq[tail], &cmd, sizeof(nvme_command_t));
    tail = (tail + 1) % 1024;
    dev->io_sq_tail = tail;
    nvme_write_sq_doorbell(dev, 1, tail);

    int timeout = 1000000;
    while (((dev->io_cq[dev->io_cq_head].status & 1) != dev->io_cq_phase) && timeout-- > 0);
    
    if (timeout <= 0) return -1;

    uint16_t status = dev->io_cq[dev->io_cq_head].status;
    uint16_t head = dev->io_cq_head;
    head = (head + 1) % 1024;
    if (head == 0) dev->io_cq_phase ^= 1;
    dev->io_cq_head = head;
    nvme_write_cq_doorbell(dev, 1, head);

    return (status >> 1) == 0 ? 0 : -1;
}
