#ifndef NET_H
#define NET_H

#include <stdint.h>

struct eth_header {
    uint8_t dest[6];
    uint8_t src[6];
    uint16_t type;
} __attribute__((packed));

struct ipv4_header {
    uint8_t version_ihl;
    uint8_t dscp_ecn;
    uint16_t length;
    uint16_t id;
    uint16_t flags_fragment;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dest_ip;
} __attribute__((packed));

struct udp_header {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed));

struct tcp_header {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq;
    uint32_t ack;
    uint16_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
} __attribute__((packed));

struct arp_header {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t hw_len;
    uint8_t proto_len;
    uint16_t opcode;
    uint8_t src_mac[6];
    uint32_t src_ip;
    uint8_t dest_mac[6];
    uint32_t dest_ip;
} __attribute__((packed));

struct icmp_header {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint32_t rest;
} __attribute__((packed));

#define ICMP_TYPE_DEST_UNREACHABLE 3

#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY   2

#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP 6
#define IP_PROTO_UDP 17

#define ETH_TYPE_IPV4 0x0800
#define ETH_TYPE_ARP  0x0806

#define htons(n) (((((unsigned short)(n) & 0xFF)) << 8) | (((unsigned short)(n) & 0xFF00) >> 8))
#define htonl(n) (((((unsigned long)(n) & 0xFF)) << 24) | \
                  ((((unsigned long)(n) & 0xFF00)) << 8) | \
                  ((((unsigned long)(n) & 0xFF0000)) >> 8) | \
                  ((((unsigned long)(n) & 0xFF000000)) >> 24))

typedef struct {
    uint32_t ip;
    uint8_t mac[6];
    uint8_t resolved;
} arp_entry;

uint16_t net_checksum(void* data, uint32_t len);

void net_handle_packet(void* data, uint16_t len);
void net_send_udp(uint8_t* dest_mac, uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, const void* data, uint16_t len);
void net_send_tcp_syn(uint8_t* dest_mac, uint32_t dest_ip, uint16_t src_port, uint16_t dest_port);
void net_send_tcp_ack(uint8_t* dest_mac, uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, uint32_t seq, uint32_t ack);

void net_send_arp_request(uint32_t target_ip);
int net_arp_lookup(uint32_t ip, uint8_t* out_mac);

#endif
