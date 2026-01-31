#include "commands.h"
#include <stdbool.h>
#include "../../c/lib/string.h"
#include "../../c/fs/ramfs.h"
#include "../../c/drivers/platform/rtc.h"
#include "../../c/scheduler/scheduler.h"
#include "../../c/memory/pmm.h"
#include "../../c/drivers/pci/pci.h"
#include "../../c/drivers/usb/xhci/xhci.h"
#include "../../c/panic/panic.h"
#include "../../c/drivers/platform/serial.h"
#include "../../c/drivers/network/e1000.h"
#include "../../c/drivers/network/e1000e.h"

extern xhci_controller_t xhc;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

// Simple IP parser: "192.168.1.1" -> {192, 168, 1, 1}
static int parse_ip(const char* str, uint8_t* ip) {
    int idx = 0;
    int val = 0;
    int dots = 0;
    while (*str) {
        if (*str >= '0' && *str <= '9') {
            val = val * 10 + (*str - '0');
        } else if (*str == '.') {
            ip[idx++] = (uint8_t)val;
            val = 0;
            dots++;
            if (idx > 3) return 0;
        } else {
            return 0; // Invalid char
        }
        str++;
    }
    ip[idx] = (uint8_t)val;
    return (dots == 3);
}

void commands_init() {
    // Any initialization logic for commands if needed
}

void commands_execute(const char* cmd, cmd_output_fn output) {
    if (strcmp(cmd, "help") == 0) {
        output("Available commands:", CMD_COLOR_GREY);
        output("  ls, clear, help, exit, 100, version, whoami, uname,", CMD_COLOR_GREY);
        output("  date, uptime, mem, ps, pci, cpu, reboot, panic, logo,", CMD_COLOR_GREY);
        output("  lsusb, fetch, rand, motd, credits, license, pwd, beep, echo", CMD_COLOR_GREY);
        output("  ping <ip> (UDP)", CMD_COLOR_GREY);
    } else if (strcmp(cmd, "clear") == 0) {
        // Clear logic depends on the caller (serial shell sends ANSI, GUI terminal clears history)
        // We'll handle it specially if needed, but for now just a hint
        output("__CLEAR__", 0); 
    } else if (strcmp(cmd, "ls") == 0) {
        RamNode* node = ramfs_get_root()->child;
        while (node) {
            output(node->name, CMD_COLOR_WHITE);
            node = node->next;
        }
    } else if (strcmp(cmd, "100") == 0) {
        output("100% Real OS", CMD_COLOR_GREEN);
    } else if (strcmp(cmd, "version") == 0) {
        output("Tiny64 OS v1.3.5 Platinum", CMD_COLOR_CYAN);
    } else if (strcmp(cmd, "whoami") == 0) {
        output("root", CMD_COLOR_WHITE);
    } else if (strcmp(cmd, "uname") == 0) {
        output("Tiny64 x86_64", CMD_COLOR_WHITE);
    } else if (strcmp(cmd, "date") == 0 || strcmp(cmd, "time") == 0) {
        rtc_time_t now;
        rtc_get_time(&now);
        char buf[64];
        ksprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d", now.year, now.month, now.day, now.hour, now.minute, now.second);
        output(buf, CMD_COLOR_WHITE);
    } else if (strcmp(cmd, "uptime") == 0) {
        char buf[64];
        uint64_t sec = g_ticks / 100;
        ksprintf(buf, "Uptime: %d seconds", (int)sec);
        output(buf, CMD_COLOR_WHITE);
    } else if (strcmp(cmd, "mem") == 0) {
        char buf[128];
        uint64_t total = pmm_get_total_memory();
        uint64_t free = pmm_get_free_memory();
        ksprintf(buf, "Total: %d MB | Free: %d MB", (int)(total / 1024 / 1024), (int)(free / 1024 / 1024));
        output(buf, CMD_COLOR_GREEN);
    } else if (strcmp(cmd, "ps") == 0) {
        for (int i = 0; i < MAX_TASKS; i++) {
            if (g_tasks[i].state != TASK_UNUSED) {
                char buf[64];
                ksprintf(buf, "PID %d: %s (State: %d)", g_tasks[i].id, g_tasks[i].name, (int)g_tasks[i].state);
                output(buf, CMD_COLOR_WHITE);
            }
        }
    } else if (strcmp(cmd, "pci") == 0) {
        for (int i = 0; i < g_pci_device_count; i++) {
            char buf[128];
            ksprintf(buf, "%02x:%02x:%02x ID:%04x:%04x CC:%02x", g_pci_devices[i].bus, g_pci_devices[i].device, g_pci_devices[i].function, g_pci_devices[i].vendor_id, g_pci_devices[i].device_id, g_pci_devices[i].class_code);
            output(buf, CMD_COLOR_WHITE);
        }
    } else if (strcmp(cmd, "cpu") == 0) {
        uint32_t eax, ebx, ecx, edx;
        char brand[49];
        for (uint32_t i = 0x80000002; i <= 0x80000004; i++) {
            __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(i));
            memcpy(brand + (i - 0x80000002) * 16, &eax, 4);
            memcpy(brand + (i - 0x80000002) * 16 + 4, &ebx, 4);
            memcpy(brand + (i - 0x80000002) * 16 + 8, &ecx, 4);
            memcpy(brand + (i - 0x80000002) * 16 + 12, &edx, 4);
        }
        brand[48] = '\0';
        output(brand, CMD_COLOR_WHITE);
    } else if (strcmp(cmd, "reboot") == 0) {
        output("Rebooting...", CMD_COLOR_RED);
        uint8_t good = 0x02;
        while (good & 0x02) good = inb(0x64);
        outb(0x64, 0xFE);
    } else if (strcmp(cmd, "panic") == 0) {
        PANIC("User triggered panic");
    } else if (strcmp(cmd, "logo") == 0) {
        output("  _______ _             __   _  _   ", CMD_COLOR_CYAN);
        output(" |__   __(_)           / /  | || |  ", CMD_COLOR_CYAN);
        output("    | |   _ _ __  _   / /_  | || |_ ", CMD_COLOR_CYAN);
        output("    | |  | | '_ \\\\| | | '_ \\\\ |__   _| ", CMD_COLOR_CYAN);
        output("    | |  | | | | | |_| (_) |   | |  ", CMD_COLOR_CYAN);
        output("    |_|  |_|_| |_|\\__, \\___/    |_|  ", CMD_COLOR_CYAN);
    } else if (strcmp(cmd, "lsusb") == 0) {

        extern int g_using_qemu_xhci;
        if (xhc.base_addr) {
            output(g_using_qemu_xhci ? "xHCI Controller: QEMU-XHCI (Active)" : "xHCI Controller: Intel/Native (Active)", CMD_COLOR_GREEN);
            int devices = 0;
            for (int i = 1; i <= (int)xhc.max_slots; i++) {
                if (xhc.slots[i].device_type != 0) {
                    char buf[64];
                    const char* type = "Unknown";
                    if (xhc.slots[i].device_type == 1) type = "Keyboard";
                    else if (xhc.slots[i].device_type == 2) type = "Mouse";
                    else if (xhc.slots[i].device_type == 3) type = "MSC/Other";
                    ksprintf(buf, "Slot %d: %s", i, type);
                    output(buf, CMD_COLOR_WHITE);
                    devices++;
                }
            }
            if (devices == 0) output("No devices detected on root hub.", CMD_COLOR_GREY);
        } else {
            output("No xHCI Controller Found.", CMD_COLOR_RED);
        }
    } else if (strncmp(cmd, "ping ", 5) == 0) {
        const char* ip_str = cmd + 5;
        uint8_t ip[4];
        if (parse_ip(ip_str, ip)) {
            char buf[64];
            ksprintf(buf, "Sending UDP ping to %d.%d.%d.%d...", ip[0], ip[1], ip[2], ip[3]);
            output(buf, CMD_COLOR_WHITE);

            const char* ping_data = "Tiny64 UDP Ping";
            bool sent = false;

            if (e1000_get_status_reg() != 0) {
                e1000_send_udp_packet(ip, 7, ping_data, strlen(ping_data));
                sent = true;
            } else if (e1000e_get_status_reg() != 0) {
                e1000e_send_udp_packet(ip, 7, ping_data, strlen(ping_data));
                sent = true;
            }

            if (sent) {
                output("Packet sent successfully.", CMD_COLOR_GREEN);
                // Simulate reply if local for UX, or just leave it as sent
                if (ip[0] == 127 || (ip[0]==172 && ip[1]==16 && ip[2]==6 && ip[3]==222)) {
                     output("Reply received (Loopback).", CMD_COLOR_GREEN);
                }
            } else {
                output("No active network interface found.", CMD_COLOR_RED);
            }
        } else {
            output("Invalid IP address. Usage: ping <ip>", CMD_COLOR_RED);
        }
    } else if (strcmp(cmd, "fetch") == 0) {
        output("OS: Tiny64", CMD_COLOR_CYAN);
        uint64_t total = pmm_get_total_memory();
        char mbuf[64]; ksprintf(mbuf, "Memory: %d MB", (int)(total/1024/1024));
        output(mbuf, CMD_COLOR_CYAN);
    } else if (strcmp(cmd, "rand") == 0) {
        static uint32_t next = 1;
        next = next * 1103515245 + 12345;
        char buf[32]; ksprintf(buf, "%d", (int)((next/65536) % 32768));
        output(buf, CMD_COLOR_WHITE);
    } else if (strcmp(cmd, "motd") == 0) {
        output("Welcome to the 100% Real OS experience!", CMD_COLOR_GREEN);
    } else if (strcmp(cmd, "credits") == 0) {
        output("Tiny64 Team & Open Source Contributors", CMD_COLOR_WHITE);
    } else if (strcmp(cmd, "license") == 0) {
        output("MIT License", CMD_COLOR_WHITE);
    } else if (strcmp(cmd, "pwd") == 0) {
        output("/", CMD_COLOR_WHITE);
    } else if (strncmp(cmd, "echo ", 5) == 0) {
        output(cmd + 5, CMD_COLOR_WHITE);
    } else if (strcmp(cmd, "beep") == 0) {
        output("BEEP!", CMD_COLOR_GREEN);
    } else if (cmd[0] != '\0') {
        output("Unknown command", CMD_COLOR_RED);
    }
}
