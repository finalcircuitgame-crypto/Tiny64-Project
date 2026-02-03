#include <kernel/net/net.h>
#include <drivers/net/e1000.h>
#include <lib/string.h>
#include <kernel/printk.h>

static uint32_t my_ip = 0xAC10060F; // 172.16.6.15
static arp_entry arp_table[16];
static int arp_table_count = 0;

uint16_t net_checksum(void* data, uint32_t len) {
    // ... (checksum logic remains same)
    uint32_t sum = 0;
    uint16_t* ptr = (uint16_t*)data;
    while (len > 1) { sum += *ptr++; len -= 2; }
    if (len > 0) sum += *(uint8_t*)ptr;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return ~((uint16_t)sum);
}

void net_send_arp_request(uint32_t target_ip) {
    uint8_t packet[64];
    memset(packet, 0, 64);
    struct eth_header* eth = (struct eth_header*)packet;
    struct arp_header* arp = (struct arp_header*)(packet + sizeof(struct eth_header));

    memset(eth->dest, 0xFF, 6); // Broadcast
    e1000_get_mac(eth->src);
    eth->type = htons(ETH_TYPE_ARP);

    arp->hw_type = htons(1);
    arp->proto_type = htons(ETH_TYPE_IPV4);
    arp->hw_len = 6;
    arp->proto_len = 4;
    arp->opcode = htons(ARP_OP_REQUEST);
    e1000_get_mac(arp->src_mac);
    arp->src_ip = htonl(my_ip);
    memset(arp->dest_mac, 0x00, 6);
    arp->dest_ip = htonl(target_ip);

    e1000_send_packet(packet, 60); // Pad to 60 bytes (Ethernet minimum)
}

int net_arp_lookup(uint32_t ip, uint8_t* out_mac) {
    uint32_t ip_net = htonl(ip);
    for (int i = 0; i < arp_table_count; i++) {
        if (arp_table[i].ip == ip_net) {
            memcpy(out_mac, arp_table[i].mac, 6);
            return 1;
        }
    }
    return 0;
}

void net_send_udp(uint8_t* dest_mac, uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, const void* data, uint16_t len) {
    uint8_t packet[1514];
    struct eth_header* eth = (struct eth_header*)packet;
    struct ipv4_header* ip = (struct ipv4_header*)(packet + sizeof(struct eth_header));
    struct udp_header* udp = (struct udp_header*)(packet + sizeof(struct eth_header) + sizeof(struct ipv4_header));
    uint8_t* payload = packet + sizeof(struct eth_header) + sizeof(struct ipv4_header) + sizeof(struct udp_header);

    e1000_get_mac(eth->src);
    for(int i=0; i<6; i++) eth->dest[i] = dest_mac[i];
    eth->type = htons(ETH_TYPE_IPV4);

    ip->version_ihl = 0x45;
    ip->dscp_ecn = 0;
    ip->length = htons(sizeof(struct ipv4_header) + sizeof(struct udp_header) + len);
    ip->id = htons(0);
    ip->flags_fragment = htons(0x4000); // Don't fragment
    ip->ttl = 64;
    ip->protocol = IP_PROTO_UDP;
    ip->src_ip = htonl(my_ip);
    ip->dest_ip = htonl(dest_ip);
    ip->checksum = 0;
    ip->checksum = net_checksum(ip, sizeof(struct ipv4_header));

    udp->src_port = htons(src_port);
    udp->dest_port = htons(dest_port);
    udp->length = htons(sizeof(struct udp_header) + len);
    udp->checksum = 0;

    memcpy(payload, data, len);

    // UDP Checksum with pseudo-header
    /*
    struct {
        uint32_t src;
        uint32_t dest;
        uint8_t zero;
        uint8_t proto;
        uint16_t len;
    } __attribute__((packed)) pseudo;
    pseudo.src = ip->src_ip;
    pseudo.dest = ip->dest_ip;
    pseudo.zero = 0;
    pseudo.proto = IP_PROTO_UDP;
    pseudo.len = udp->length;

    uint8_t checksum_buf[1514];
    memcpy(checksum_buf, &pseudo, sizeof(pseudo));
    memcpy(checksum_buf + sizeof(pseudo), udp, sizeof(struct udp_header) + len);
    udp->checksum = net_checksum(checksum_buf, sizeof(pseudo) + sizeof(struct udp_header) + len);
    */
    udp->checksum = 0; // Optional for UDP (0 means none)

    e1000_send_packet(packet, sizeof(struct eth_header) + sizeof(struct ipv4_header) + sizeof(struct udp_header) + len);
}

void net_send_tcp_ack(uint8_t* dest_mac, uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, uint32_t seq, uint32_t ack) {
    uint8_t packet[1514];
    struct eth_header* eth = (struct eth_header*)packet;
    struct ipv4_header* ip = (struct ipv4_header*)(packet + sizeof(struct eth_header));
    struct tcp_header* tcp = (struct tcp_header*)(packet + sizeof(struct eth_header) + sizeof(struct ipv4_header));

    e1000_get_mac(eth->src);
    for(int i=0; i<6; i++) eth->dest[i] = dest_mac[i];
    eth->type = htons(ETH_TYPE_IPV4);

    ip->version_ihl = 0x45;
    ip->dscp_ecn = 0;
    ip->length = htons(sizeof(struct ipv4_header) + sizeof(struct tcp_header));
    ip->id = htons(0);
    ip->flags_fragment = htons(0x4000);
    ip->ttl = 64;
    ip->protocol = IP_PROTO_TCP;
    ip->src_ip = htonl(my_ip);
    ip->dest_ip = htonl(dest_ip);
    ip->checksum = 0;
    ip->checksum = net_checksum(ip, sizeof(struct ipv4_header));

    tcp->src_port = htons(src_port);
    tcp->dest_port = htons(dest_port);
    tcp->seq = htonl(seq);
    tcp->ack = htonl(ack);
    tcp->flags = htons(0x5010); // Offset 5, ACK flag
    tcp->window = htons(1024);
    tcp->checksum = 0;
    tcp->urgent = 0;

    struct {
        uint32_t src;
        uint32_t dest;
        uint8_t zero;
        uint8_t proto;
        uint16_t len;
        struct tcp_header tcp;
    } __attribute__((packed)) pseudo;
    pseudo.src = ip->src_ip;
    pseudo.dest = ip->dest_ip;
    pseudo.zero = 0;
    pseudo.proto = IP_PROTO_TCP;
    pseudo.len = htons(sizeof(struct tcp_header));
    memcpy(&pseudo.tcp, tcp, sizeof(struct tcp_header));
    tcp->checksum = net_checksum(&pseudo, sizeof(pseudo));

    e1000_send_packet(packet, sizeof(struct eth_header) + sizeof(struct ipv4_header) + sizeof(struct tcp_header));
}

void net_handle_packet(void* data, uint16_t len) {
    struct eth_header* eth = (struct eth_header*)data;
    uint16_t type = htons(eth->type);

    if (type == ETH_TYPE_ARP) {
        struct arp_header* arp = (struct arp_header*)((uint8_t*)data + sizeof(struct eth_header));
        
        // serial_write("a"); // ARP packet seen in stack

        // Learn/Update ARP Table from any ARP packet (Request or Reply)
        int found = 0;
        for (int i = 0; i < arp_table_count; i++) {
            if (arp_table[i].ip == arp->src_ip) {
                memcpy(arp_table[i].mac, arp->src_mac, 6);
                found = 1; break;
            }
        }
        if (!found && arp_table_count < 16) {
            arp_table[arp_table_count].ip = arp->src_ip;
            memcpy(arp_table[arp_table_count].mac, arp->src_mac, 6);
            arp_table[arp_table_count].resolved = 1;
            arp_table_count++;
            printk("[ARP] Learned %d.%d.%d.%d at %02x:%02x:%02x:%02x:%02x:%02x\n",
                   (arp->src_ip >> 0) & 0xFF, (arp->src_ip >> 8) & 0xFF, (arp->src_ip >> 16) & 0xFF, (arp->src_ip >> 24) & 0xFF,
                   arp->src_mac[0], arp->src_mac[1], arp->src_mac[2], arp->src_mac[3], arp->src_mac[4], arp->src_mac[5]);
        }

        if (htons(arp->opcode) == ARP_OP_REQUEST && arp->dest_ip == htonl(my_ip)) {
            // Send ARP Reply
            uint8_t packet[64];
            memset(packet, 0, 64);
            struct eth_header* reply_eth = (struct eth_header*)packet;
            struct arp_header* reply_arp = (struct arp_header*)(packet + sizeof(struct eth_header));

            e1000_get_mac(reply_eth->src);
            memcpy(reply_eth->dest, eth->src, 6);
            reply_eth->type = htons(ETH_TYPE_ARP);

            reply_arp->hw_type = htons(1);
            reply_arp->proto_type = htons(ETH_TYPE_IPV4);
            reply_arp->hw_len = 6;
            reply_arp->proto_len = 4;
            reply_arp->opcode = htons(ARP_OP_REPLY);
            e1000_get_mac(reply_arp->src_mac);
            reply_arp->src_ip = htonl(my_ip);
            memcpy(reply_arp->dest_mac, arp->src_mac, 6);
            reply_arp->dest_ip = arp->src_ip;

            e1000_send_packet(packet, 60);
        }
    } else if (type == ETH_TYPE_IPV4) {
        struct ipv4_header* ip = (struct ipv4_header*)((uint8_t*)data + sizeof(struct eth_header));
        uint32_t src = ip->src_ip;
        const char* proto = "Unknown";
        uint16_t src_p = 0, dest_p = 0;
        char payload_preview[17];
        payload_preview[0] = '\0';

        if (ip->protocol == IP_PROTO_UDP) {
            proto = "UDP";
            struct udp_header* udp = (struct udp_header*)((uint8_t*)ip + sizeof(struct ipv4_header));
            src_p = htons(udp->src_port);
            dest_p = htons(udp->dest_port);
            uint8_t* data_ptr = (uint8_t*)udp + sizeof(struct udp_header);
            int p_len = htons(udp->length) - 8;
            if (p_len > 0) {
                int copy_len = p_len > 16 ? 16 : p_len;
                memcpy(payload_preview, data_ptr, copy_len);
                payload_preview[copy_len] = '\0';
            }
        } else if (ip->protocol == IP_PROTO_TCP) {
            proto = "TCP";
            struct tcp_header* tcp = (struct tcp_header*)((uint8_t*)ip + sizeof(struct ipv4_header));
            src_p = htons(tcp->src_port);
            dest_p = htons(tcp->dest_port);
            
            uint16_t flags = htons(tcp->flags);
            printk("[TCP] Flags: 0x%x ", flags);
            if ((flags & 0x12) == 0x12) { // SYN + ACK
                printk("(SYN+ACK) received, sending ACK...\n");
                net_send_tcp_ack(eth->src, htonl(src), dest_p, src_p, htonl(tcp->ack), htonl(tcp->seq) + 1);
            } else if (flags & 0x04) {
                 printk("(RST) Reset received.\n");
            } else if (flags & 0x10) {
                 printk("(ACK) received.\n");
            } else {
                 printk("\n");
            }
        } else if (ip->protocol == IP_PROTO_ICMP) {
            proto = "ICMP";
            struct icmp_header* icmp = (struct icmp_header*)((uint8_t*)ip + sizeof(struct ipv4_header));
            printk("[ICMP] Type: %d Code: %d\n", icmp->type, icmp->code);
        }

        printk("[IPv4] From %d.%d.%d.%d:%d to :%d Proto: %s Data: %s\n", 
               (src >> 0) & 0xFF, (src >> 8) & 0xFF, (src >> 16) & 0xFF, (src >> 24) & 0xFF, 
               src_p, dest_p, proto, payload_preview);
    }
}

void net_send_tcp_syn(uint8_t* dest_mac, uint32_t dest_ip, uint16_t src_port, uint16_t dest_port) {
    uint8_t packet[1514];
    struct eth_header* eth = (struct eth_header*)packet;
    struct ipv4_header* ip = (struct ipv4_header*)(packet + sizeof(struct eth_header));
    struct tcp_header* tcp = (struct tcp_header*)(packet + sizeof(struct eth_header) + sizeof(struct ipv4_header));

    e1000_get_mac(eth->src);
    for(int i=0; i<6; i++) eth->dest[i] = dest_mac[i];
    eth->type = htons(ETH_TYPE_IPV4);

    ip->version_ihl = 0x45;
    ip->dscp_ecn = 0;
    ip->length = htons(sizeof(struct ipv4_header) + sizeof(struct tcp_header));
    ip->id = htons(0);
    ip->flags_fragment = htons(0x4000);
    ip->ttl = 64;
    ip->protocol = IP_PROTO_TCP;
    ip->src_ip = htonl(my_ip);
    ip->dest_ip = htonl(dest_ip);
    ip->checksum = 0;
    ip->checksum = net_checksum(ip, sizeof(struct ipv4_header));

    tcp->src_port = htons(src_port);
    tcp->dest_port = htons(dest_port);
    tcp->seq = htonl(12345);
    tcp->ack = 0;
    tcp->flags = htons(0x5002); // Header length 5 (20 bytes) + SYN flag
    tcp->window = htons(1024);
    tcp->checksum = 0;
    tcp->urgent = 0;

    // TCP checksum needs pseudo-header, but many stacks accept 0 if we are lucky or we can implement it.
    // Let's implement it for correctness.
    struct {
        uint32_t src;
        uint32_t dest;
        uint8_t zero;
        uint8_t proto;
        uint16_t len;
        struct tcp_header tcp;
    } __attribute__((packed)) pseudo;

    pseudo.src = ip->src_ip;
    pseudo.dest = ip->dest_ip;
    pseudo.zero = 0;
    pseudo.proto = IP_PROTO_TCP;
    pseudo.len = htons(sizeof(struct tcp_header));
    memcpy(&pseudo.tcp, tcp, sizeof(struct tcp_header));
    tcp->checksum = net_checksum(&pseudo, sizeof(pseudo));

    e1000_send_packet(packet, sizeof(struct eth_header) + sizeof(struct ipv4_header) + sizeof(struct tcp_header));
}
