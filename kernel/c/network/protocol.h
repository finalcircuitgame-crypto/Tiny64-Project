#ifndef NETWORK_PROTOCOL_H
#define NETWORK_PROTOCOL_H

#include <stdint.h>

#define ETH_P_IP   0x0800
#define ETH_P_ARP  0x0806

#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6
#define IP_PROTO_UDP  17

typedef struct {
    uint8_t dst[6];
    uint8_t src[6];
    uint16_t type;
} __attribute__((packed)) eth_hdr_t;

typedef struct {
    uint8_t  ver_ihl;
    uint8_t  tos;
    uint16_t len;
    uint16_t id;
    uint16_t frag;
    uint8_t  ttl;
    uint8_t  proto;
    uint16_t csum;
    uint32_t src;
    uint32_t dst;
} __attribute__((packed)) ip_hdr_t;

typedef struct {
    uint16_t src;
    uint16_t dst;
    uint16_t len;
    uint16_t csum;
} __attribute__((packed)) udp_hdr_t;

typedef struct {
    uint16_t src;
    uint16_t dst;
    uint32_t seq;
    uint32_t ack;
    uint8_t  res_off; // Data offset (high 4) + Reserved (low 4)
    uint8_t  flags;   // CWR, ECE, URG, ACK, PSH, RST, SYN, FIN
    uint16_t window;
    uint16_t csum;
    uint16_t urg;
} __attribute__((packed)) tcp_hdr_t;

typedef struct {
    uint8_t type;
    uint8_t code;
    uint16_t csum;
    uint16_t id;
    uint16_t seq;
} __attribute__((packed)) icmp_hdr_t;

static inline uint16_t swap16(uint16_t v) {
    return (v << 8) | (v >> 8);
}

static inline uint32_t swap32(uint32_t v) {
    return ((v & 0xFF) << 24) | ((v & 0xFF00) << 8) | ((v & 0xFF0000) >> 8) | ((v >> 24) & 0xFF);
}

#endif
