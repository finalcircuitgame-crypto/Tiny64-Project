#ifndef TINY64_TCP_CLIENT_H
#define TINY64_TCP_CLIENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TCP_CLOSED = 0,
    TCP_SYN_SENT = 1,
    TCP_ESTABLISHED = 2,
    TCP_FIN_WAIT_1 = 3,
    TCP_FIN_WAIT_2 = 4,
    TCP_TIME_WAIT = 5
} tcp_state_t;

int tcp_connect_ipv4(const uint8_t dst_ip[4], uint16_t dst_port);
int tcp_send_data(const void* data, uint16_t len);
int tcp_close(void);

int tcp_is_established(void);

int tcp_recv_available(void);
int tcp_recv_read(void* out, int out_len);

void tcp_on_packet(const uint8_t src_ip[4], const void* tcp_segment, uint16_t tcp_len);
void tcp_poll(void);

#ifdef __cplusplus
}
#endif

#endif
