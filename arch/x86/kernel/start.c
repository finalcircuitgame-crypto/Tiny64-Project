#include <kernel/printk.h>
#include <kernel/serial.h>
#include <kernel/bootinfo.h>
#include <kernel/pci.h>
#include <kernel/pmm.h>
#include <kernel/heap.h>
#include <kernel/net.h>
#include <drivers/e1000.h>
#include <lib/string.h>

void console_init(BootInfo *boot_info);
void console_draw_desktop();

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
    while (!net_arp_lookup(gateway_ip, gateway_mac)) {
        net_send_arp_request(gateway_ip);
        
        // Process incoming packets for a bit to catch the reply
        for (int i = 0; i < 500; i++) {
            uint8_t rx_buf[2048];
            int received = e1000_receive_packet(rx_buf, 2048);
            if (received > 0) net_handle_packet(rx_buf, received);
            for(volatile int k=0; k<10000; k++); // Micro-delay between polls
        }
        for(volatile int j=0; j<10000000; j++); // Short wait
    }

    printk("Gateway Resolved! Sending UDP to 10.0.2.2:60001...\n");
    int count = 1;
    for (;;) {
        printk("[%d] Sending UDP packet to 10.0.2.2:60001 ...\n", count++);
        
        // Send 5 UDP packets
        for (int i = 0; i < 5; i++) {
            net_send_udp(gateway_mac, target_ip, 1234, 60001, udp_msg, strlen(udp_msg));
            for(volatile int j=0; j<1000000; j++); 
        }

        // Send 5 TCP SYN packets
        /* 
        for (int i = 0; i < 5; i++) {
            net_send_tcp_syn(target_mac, target_ip, 1234, 5001);
            for(volatile int j=0; j<1000000; j++);
        }
        */

        // Process incoming packets
        uint8_t rx_buf[2048];
        for (int i = 0; i < 100; i++) {
            int received = e1000_receive_packet(rx_buf, 2048);
            if (received > 0) {
                net_handle_packet(rx_buf, received);
            }
        }

        for(volatile int j=0; j<200000000; j++); // 2 second delay
    }
}