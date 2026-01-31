#ifndef HARDWARE_H
#define HARDWARE_H

#include <stdint.h>
#include "../../c/drivers/platform/serial.h"
#include "../../c/lib/string.h"
#include "../../c/graphics/framebuffer.h"

extern "C" char g_gpu_model[64];

class Hardware {
public:
    struct CPUInfo {
        char brand[49];
        uint32_t cores;
        bool has_sse;
        bool has_avx;
    };

    struct GPUInfo {
        char name[64];
        uint16_t vendor_id;
        uint16_t device_id;
    };

    static CPUInfo GetCPUInfo() {
        CPUInfo info;
        memset(info.brand, 0, sizeof(info.brand));
        
        uint32_t eax, ebx, ecx, edx;

        // Get Brand String
        for (uint32_t i = 0x80000002; i <= 0x80000004; i++) {
            __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(i));
            memcpy(info.brand + (i - 0x80000002) * 16, &eax, 4);
            memcpy(info.brand + (i - 0x80000002) * 16 + 4, &ebx, 4);
            memcpy(info.brand + (i - 0x80000002) * 16 + 8, &ecx, 4);
            memcpy(info.brand + (i - 0x80000002) * 16 + 12, &edx, 4);
        }

        // Get Core Count (Logical)
        __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
        info.cores = (ebx >> 16) & 0xFF;
        if (info.cores == 0) info.cores = 1; // Fallback
        
        info.has_sse = (edx & (1 << 25));
        info.has_avx = (ecx & (1 << 28));

        return info;
    }

    static GPUInfo GetGPUInfo() {
        GPUInfo info;
        strcpy(info.name, g_gpu_model);
        info.vendor_id = 0;
        info.device_id = 0;
        return info;
    }

    static void OptimizeSystem() {
        CPUInfo info = GetCPUInfo();
        serial_write_str("Hardware: Optimizing for ");
        serial_write_str(info.brand);
        serial_write_str("\r\n");

        // 1. Enable MTRR Write-Combining for Framebuffer
        // Check for MTRR support (CPUID.01h:EDX bit 12)
        uint32_t eax, ebx, ecx, edx;
        __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
        bool has_mtrr = (edx & (1 << 12));

        extern Framebuffer* g_kernel_fb;
        if (g_kernel_fb && has_mtrr) {
            uint64_t base = g_kernel_fb->base_address;
            uint64_t size = g_kernel_fb->buffer_size;
            
            // MTRR base must be at least 4KB aligned
            if ((base & 0xFFF) == 0) {
                // Get physical address bits to create a safe mask (prevents setting reserved bits)
                uint32_t eax8, ebx8, ecx8, edx8;
                __asm__ volatile ("cpuid" : "=a"(eax8), "=b"(ebx8), "=c"(ecx8), "=d"(edx8) : "a"(0x80000008));
                uint8_t phys_bits = eax8 & 0xFF;
                if (phys_bits == 0) phys_bits = 36; // Fallback for some CPUs
                uint64_t phys_mask = ((1ULL << phys_bits) - 1) & ~0xFFFULL;

                // Align size to next power of 2 for MTRR (simplified)
                uint64_t mtrr_size = 1;
                while(mtrr_size < size) mtrr_size <<= 1;

                serial_write_str("Hardware: Enabling MTRR WC for Framebuffer...\r\n");
                serial_write_str("  Base: "); serial_write_hex(base);
                serial_write_str("  Size: "); serial_write_hex(mtrr_size);
                serial_write_str("\r\n");
                
                // IA32_MTRR_PHYSBASE0 = 0x200, IA32_MTRR_PHYSMASK0 = 0x201
                // Type 1 = Write-Combining (WC)
                uint64_t base_reg = (base & phys_mask) | 1;
                uint64_t mask_reg = (~(mtrr_size - 1) & phys_mask) | 0x800; // 0x800 = Valid bit

                // --- SAFE MTRR UPDATE SEQUENCE ---
                __asm__ volatile (
                    "cli\n"
                    // 1. Disable Caching (CR0.CD=1, CR0.NW=0)
                    "mov %%cr0, %%rax\n"
                    "or $(1 << 30), %%rax\n"
                    "and $~(1 << 29), %%rax\n"
                    "mov %%rax, %%cr0\n"
                    "wbinvd\n"
                    
                    // 2. Disable MTRRs (Clear E bit in IA32_MTRR_DEF_TYPE)
                    "mov $0x2FF, %%ecx\n"
                    "rdmsr\n"
                    "push %%rax\n" // Save current MTRR_DEF_TYPE
                    "and $~0x800, %%eax\n"
                    "wrmsr\n"
                    
                    // 3. Update MTRR Base & Mask
                    "mov $0x200, %%ecx\n"
                    "mov %0, %%eax\n"
                    "mov %1, %%edx\n"
                    "wrmsr\n"
                    "mov $0x201, %%ecx\n"
                    "mov %2, %%eax\n"
                    "mov %3, %%edx\n"
                    "wrmsr\n"
                    
                    // 4. Re-enable MTRRs
                    "pop %%rax\n"
                    "mov $0x2FF, %%ecx\n"
                    "or $0x800, %%eax\n"
                    "wrmsr\n"
                    
                    // 5. Re-enable Caching
                    "wbinvd\n"
                    "mov %%cr0, %%rax\n"
                    "and $~(1 << 30), %%rax\n"
                    "mov %%rax, %%cr0\n"
                    "sti\n"
                    : : "r"((uint32_t)base_reg), "r"((uint32_t)(base_reg >> 32)),
                        "r"((uint32_t)mask_reg), "r"((uint32_t)(mask_reg >> 32)) 
                    : "rax", "rcx", "rdx", "memory"
                );
            } else {
                serial_write_str("Hardware: FB not aligned for MTRR.\r\n");
            }
        } else if (!has_mtrr) {
            serial_write_str("Hardware: MTRR not supported.\r\n");
        }

        serial_write_str("Hardware: Cores: ");
        serial_write_dec(info.cores);
        serial_write_str(info.has_avx ? " (AVX Detected)\r\n" : " (SSE Detected)\r\n");
    }
};

#endif
