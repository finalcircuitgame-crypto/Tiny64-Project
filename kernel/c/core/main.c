#include "../../cpp/ui/wizard.h"
#include "../drivers/pci/pci.h"
#include "../drivers/platform/rtc.h"
#include "../drivers/platform/serial.h"
#include "../drivers/storage/ata.h"
#include "../fs/ramfs.h"
#include "../graphics/framebuffer.h"
#include "../lib/string.h"
#include "../memory/pmm.h"
#include "../panic/panic.h"
#include "../scheduler/scheduler.h"
#include "serial_terminal.h"
#include "shell.h"
#include "../graphics/ttf.h"
#include "../drivers/usb/xhci/xhci.h"

Framebuffer *g_kernel_fb = 0;

extern void _optimize_system();
extern xhci_controller_t xhc;

#ifndef NULL
#define NULL ((void *)0)
#endif

static char *local_strstr(const char *haystack, const char *needle) {
  if (!*needle)
    return (char *)haystack;
  for (; *haystack; haystack++) {
    if (*haystack == *needle) {
      const char *h = haystack, *n = needle;
      while (*h && *n && *h == *n) {
        h++;
        n++;
      }
      if (!*n)
        return (char *)haystack;
    }
  }
  return NULL;
}

int is_qemu() {
  uint32_t eax, ebx, ecx, edx;
  
  // 1. Check Hypervisor Bit (CPUID 1, ECX bit 31)
  __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
  if (!(ecx & (1u << 31))) {
      return 0; // Not a hypervisor
  }

  // 2. Check Vendor ID (CPUID 0)
  char vendor[13];
  __asm__ volatile("cpuid"
                   : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                   : "a"(0));
  memcpy(vendor, &ebx, 4);
  memcpy(vendor + 4, &edx, 4);
  memcpy(vendor + 8, &ecx, 4);
  vendor[12] = '\0';

  if (strcmp(vendor, "TCGTCGTCGTCG") == 0 ||
      strcmp(vendor, "KVMKVMKVMKVM") == 0 ||
      strcmp(vendor, "VMwareVMware") == 0)
    return 1;

  // 3. Check Brand String for "QEMU" or "Virtual"
  char brand[49];
  for (uint32_t i = 0x80000002; i <= 0x80000004; i++) {
    __asm__ volatile("cpuid"
                     : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(i));
    memcpy(brand + (i - 0x80000002) * 16, &eax, 4);
    memcpy(brand + (i - 0x80000002) * 16 + 4, &ebx, 4);
    memcpy(brand + (i - 0x80000002) * 16 + 8, &ecx, 4);
    memcpy(brand + (i - 0x80000002) * 16 + 12, &edx, 4);
  }
  brand[48] = '\0';
  if (local_strstr(brand, "QEMU") != NULL || local_strstr(brand, "Virtual") != NULL)
    return 1;

  return 0;
}

void kmain(Framebuffer *fb, void *map, uint64_t map_size, uint64_t desc_size,
           uint64_t boot_flags) {
  g_kernel_fb = fb;
  init_serial();

  extern int usb_hid_detected;

  // Memory initialization MUST happen before any kmalloc (like
  // serial_terminal_init)
  pmm_init(map, map_size, desc_size);
  
  extern void vmm_init();
  vmm_init();

  ttf_init();
  serial_terminal_init(fb);
  serial_write_str("\r\n=== Tiny64 Kernel Booting ===\r\n");

  // Driver initialization - First Priority: Network
  pci_init();

  // Display boot time from RTC
  rtc_time_t boot_time;
  rtc_get_time(&boot_time);
  char boot_msg[64];
  ksprintf(boot_msg, "System Time: %04d-%02d-%02d %02d:%02d:%02d\r\n",
           boot_time.year, boot_time.month, boot_time.day, boot_time.hour,
           boot_time.minute, boot_time.second);
  serial_write_str(boot_msg);

  serial_write_str("Framebuffer: ");
  serial_write_hex((uint64_t)fb->base_address);
  serial_write_str(" (");
  serial_write_dec(fb->width);
  serial_write_str("x");
  serial_write_dec(fb->height);
  serial_write_str(")\r\n");

  if (boot_flags & 1) {
    serial_write_str("Kernel: Booting into Recovery Mode...\r\n");
    extern void start_recovery_mode(void *fb);
    start_recovery_mode(fb);
  }

  // Filesystem initialization
  serial_write_str("FS: Initializing RamFS...\r\n");
  ramfs_init();

  serial_write_str("Hardware: Initializing Legacy ATA...\r\n");
  ata_init(); // Legacy fallback

  // Check if USB was detected (simple check)
  if (!usb_hid_detected) {
    // Give it a few polls to enumerate if they are just being discovered
    for (int i = 0; i < 5; i++) {
      usb_poll();
      for (volatile int j = 0; j < 2000000; j++)
        ;
    }

    // If xHCI has detected and addressed devices but hid_detected isn't set yet,
    // give it a manual scan here before showing the troubleshooting block.
    if (xhc.base_addr && xhc.slots) {
      for (int i = 1; i <= (int)xhc.max_slots; i++) {
        if (xhc.slots[i].device_type == 1 || xhc.slots[i].device_type == 2) {
          usb_hid_detected = 1;
          break;
        }
      }
    }
  }

  if (!usb_hid_detected) {
    serial_write_str("\r\n=== USB TROUBLESHOOTING (REAL HARDWARE) ===\r\n");
    serial_write_str(
        "No USB HID devices (Keyboard/Mouse) were detected immediately.\r\n");
    serial_write_str(
        "If they do not appear within 10 seconds, try the following:\r\n");
    serial_write_str(
        "  1. Connect directly to Motherboard ports (No Hubs).\r\n");
    serial_write_str("  2. Use USB 2.0 ports (Black) if available.\r\n");
    serial_write_str("  3. Ensure 'USB Legacy Support' & 'XHCI Handoff' are "
                     "ENABLED in BIOS.\r\n");
    serial_write_str(
        "  4. If using a gaming keyboard, switch it to 'BIOS Mode'.\r\n");
    serial_write_str("Background polling is active...\r\n\r\n");
  }

  // Initialize Interrupts & Scheduler
  extern void init_idt();
  init_idt();
  serial_write_str("Scheduler: Initializing...\r\n");
  scheduler_init();

  serial_write_str("Kernel initialization complete.\r\n\r\n");

  if (is_qemu()) { // QEMU / KVM
    draw_splash(fb);
    serial_terminal_stop();
    run_wizard(fb);   // interactive setup
    desktop_loop(fb); // continue to desktop
  } else {            // real hardware
    shell_loop();     // stay in serial terminal until exit
    serial_terminal_stop();
    desktop_loop(fb); // then continue to desktop
  }

  while (1) {
    __asm__ volatile("hlt");
  }
}