#include "ahci.h"
#include "ata.h"
#include "../../memory/pmm.h"
#include "../../memory/heap.h"
#include "../../lib/string.h"
#include "../platform/serial.h"

static hba_regs_t* g_ahci_regs = NULL;

static int ahci_find_cmd_slot(hba_port_t* port) {
    uint32_t slots = (port->sact | port->ci);
    for (int i = 0; i < 32; i++) {
        if ((slots & 1) == 0) return i;
        slots >>= 1;
    }
    return -1;
}

void ahci_init(uintptr_t base_addr) {
    serial_write_str("[AHCI] Initializing...\r\n");
    g_ahci_regs = (hba_regs_t*)base_addr;

    // Enable AHCI
    g_ahci_regs->ghc |= (1u << 31);

    uint32_t pi = g_ahci_regs->pi;
    for (int i = 0; i < 32; i++) {
        if (pi & (1 << i)) {
            hba_port_t* port = &g_ahci_regs->ports[i];
            
            // Check if SATA drive is present
            uint32_t ssts = port->ssts;
            uint8_t det = ssts & 0xF;
            uint8_t ipm = (ssts >> 8) & 0xF;

            if (det == 3 && ipm == 1) { // Device detected and active
                serial_write_str("[AHCI] Found SATA drive on port ");
                // crude int to string
                char pstr[3] = { (char)('0' + (i/10)), (char)('0' + (i%10)), 0 };
                serial_write_str(pstr);
                serial_write_str("\r\n");

                // Initialize port
                serial_write_str("[AHCI] Resetting port...\r\n");
                port->cmd &= ~1; // ST = 0
                port->cmd &= ~(1 << 4); // FRE = 0
                
                int timeout = 1000000;
                while((port->cmd & (1 << 15) || port->cmd & (1 << 14)) && timeout > 0) {
                    timeout--;
                }
                if (timeout <= 0) {
                    serial_write_str("[AHCI] Port reset timed out!\r\n");
                    continue;
                }
                serial_write_str("[AHCI] Port reset successful.\r\n");

                uintptr_t clb_phys = (uintptr_t)pmm_alloc_page();
                uintptr_t fb_phys = (uintptr_t)pmm_alloc_page();
                
                port->clb = (uint32_t)clb_phys;
                port->clbu = (uint32_t)(clb_phys >> 32);
                port->fb = (uint32_t)fb_phys;
                port->fbu = (uint32_t)(fb_phys >> 32);

                memset((void*)clb_phys, 0, 4096);
                memset((void*)fb_phys, 0, 4096);

                port->cmd |= (1 << 4); // FRE = 1
                port->cmd |= 1; // ST = 1
                serial_write_str("[AHCI] Port initialized.\r\n");
            }
        }
    }
}

int ahci_read(int port_no, uint64_t lba, uint32_t count, void* buf) {
    hba_port_t* port = &g_ahci_regs->ports[port_no];
    port->is = 0xFFFFFFFF; // Clear interrupts

    int slot = ahci_find_cmd_slot(port);
    if (slot == -1) return -1;

    uintptr_t clb_addr = ((uintptr_t)port->clbu << 32) | port->clb;
    hba_cmd_header_t* hdr = (hba_cmd_header_t*)clb_addr + slot;
    hdr->cfl = sizeof(fis_reg_h2d_t) / 4;
    hdr->w = 0; // Read
    hdr->prdtl = (uint16_t)((count + 15) / 16); // One PRDT entry can handle 4MB, but we'll use one per page or similar

    uintptr_t tbl_phys = (uintptr_t)pmm_alloc_page();
    hba_cmd_tbl_t* tbl = (hba_cmd_tbl_t*)tbl_phys;
    memset(tbl, 0, 4096);
    hdr->ctba = (uint32_t)tbl_phys;
    hdr->ctbau = (uint32_t)(tbl_phys >> 32);

    // PRDT
    tbl->prdt_entry[0].dba = (uint32_t)(uintptr_t)buf;
    tbl->prdt_entry[0].dbau = (uint32_t)((uintptr_t)buf >> 32);
    tbl->prdt_entry[0].dbc = (count * 512) - 1;
    tbl->prdt_entry[0].i = 1;

    // FIS
    fis_reg_h2d_t* fis = (fis_reg_h2d_t*)(&tbl->cfis);
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1;
    fis->command = 0x25; // READ DMA EXT

    fis->lba0 = (uint8_t)lba;
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->device = 1 << 6; // LBA mode

    fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32);
    fis->lba5 = (uint8_t)(lba >> 40);

    fis->countl = (uint8_t)count;
    fis->counth = (uint8_t)(count >> 8);

    int timeout = 1000000;
    while ((port->tfd & (ATA_STATUS_BSY | ATA_STATUS_DRQ)) && timeout-- > 0);
    if (timeout <= 0) return -1;

    port->ci = (1 << slot);

    while (1) {
        if ((port->ci & (1 << slot)) == 0) break;
        if (port->is & (1 << 30)) return -1; // Task file error
    }

    pmm_free_page(tbl);
    return 0;
}

int ahci_write(int port_no, uint64_t lba, uint32_t count, const void* buf) {
    hba_port_t* port = &g_ahci_regs->ports[port_no];
    port->is = 0xFFFFFFFF;

    int slot = ahci_find_cmd_slot(port);
    if (slot == -1) return -1;

    uintptr_t clb_addr = ((uintptr_t)port->clbu << 32) | port->clb;
    hba_cmd_header_t* hdr = (hba_cmd_header_t*)clb_addr + slot;
    hdr->cfl = sizeof(fis_reg_h2d_t) / 4;
    hdr->w = 1; // Write
    hdr->prdtl = 1;

    uintptr_t tbl_phys = (uintptr_t)pmm_alloc_page();
    hba_cmd_tbl_t* tbl = (hba_cmd_tbl_t*)tbl_phys;
    memset(tbl, 0, 4096);
    hdr->ctba = (uint32_t)tbl_phys;
    hdr->ctbau = (uint32_t)(tbl_phys >> 32);

    tbl->prdt_entry[0].dba = (uint32_t)(uintptr_t)buf;
    tbl->prdt_entry[0].dbau = (uint32_t)((uintptr_t)buf >> 32);
    tbl->prdt_entry[0].dbc = (count * 512) - 1;
    tbl->prdt_entry[0].i = 1;

    fis_reg_h2d_t* fis = (fis_reg_h2d_t*)(&tbl->cfis);
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1;
    fis->command = 0x35; // WRITE DMA EXT

    fis->lba0 = (uint8_t)lba;
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->device = 1 << 6;

    fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32);
    fis->lba5 = (uint8_t)(lba >> 40);

    fis->countl = (uint8_t)count;
    fis->counth = (uint8_t)(count >> 8);

    int timeout = 1000000;
    while ((port->tfd & (ATA_STATUS_BSY | ATA_STATUS_DRQ)) && timeout-- > 0);
    if (timeout <= 0) return -1;

    port->ci = (1 << slot);

    while (1) {
        if ((port->ci & (1 << slot)) == 0) break;
        if (port->is & (1 << 30)) return -1;
    }

    pmm_free_page(tbl);
    return 0;
}
