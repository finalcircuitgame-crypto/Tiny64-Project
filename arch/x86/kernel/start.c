#include <kernel/printk.h>
#include <kernel/serial.h>
#include <kernel/bootinfo.h>
#include <kernel/pci.h>
#include <kernel/mm/pmm.h>
#include <kernel/mm/heap.h>
#include <kernel/net/net.h>
#include <drivers/net/e1000.h>
#include <lib/string.h>

void console_init(BootInfo *boot_info);
void console_draw_desktop();
void console_flush();
void console_update();
#include <drivers/usb/xhci.h>

void kernel_main(BootInfo *boot_info) {
    serial_init();
    pmm_init(boot_info);
    heap_init();
    
    if (boot_info->fb_base) {
        console_init(boot_info);
    }

    pci_enumerate();

    serial_write("Kernel Main Reached!\n");
    printk("\nTiny64 Kernel Started!\n");

    console_write_scaled("Welcome to Tiny64 - A visually polished UEFI hobby OS featuring high-quality TTF rendering and a functional e1000 networking stack.", 0x00FF00);

    // Networking Setup
    uint32_t gateway_ip = 0x0A000202; // 10.0.2.2 (SLIRP Gateway)
    uint32_t target_ip  = 0x0A000202; // 10.0.2.2 (socket backend, port 60001)
    uint8_t gateway_mac[6];
    const char* udp_msg = "Hello from Tiny64 UDP!";

    printk("\nResolving MAC for Gateway 10.0.2.2 via ARP...\n");
    int arp_attempts = 0;
    while (!net_arp_lookup(gateway_ip, gateway_mac)) {
        if (arp_attempts++ > 10) {
            printk("ARP timeout! Continuing without network...\n");
            break;
        }
        net_send_arp_request(gateway_ip);
        
        // Process incoming packets for a bit to catch the reply
        for (int i = 0; i < 500; i++) {
            uint8_t rx_buf[2048];
            int received = e1000_receive_packet(rx_buf, 2048);
            if (received > 0) net_handle_packet(rx_buf, received);
            for(volatile int k=0; k<10000; k++); // Micro-delay between polls
        }
        for(volatile int j=0; j<1000000; j++); // Wait a bit
    }

    if (arp_attempts <= 10) {
        printk("Gateway Resolved! Sending UDP to 10.0.2.2:60001...\n");
    }
    printk("Kernel Main Reached! Entering desktop loop...\n");
    for (;;) {
        // Coordinated UI refresh
        console_update();

        // Poll USB Keyboard
        xhci_poll_keyboard();

        // Process incoming network packets
        uint8_t rx_buf[2048];
        for (int i = 0; i < 20; i++) {
            int received = e1000_receive_packet(rx_buf, 2048);
            if (received > 0) {
                net_handle_packet(rx_buf, received);
            }
        }

        // Basic delay
        for(volatile int j=0; j<2000000; j++); 
    }
}