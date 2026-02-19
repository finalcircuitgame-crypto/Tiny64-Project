#include "tcp_client.h"

#include "../drivers/network/e1000.h"
#include "protocol.h"
#include "../drivers/platform/serial.h"
#include "../lib/string.h"

#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10

static tcp_state_t g_state = TCP_CLOSED;
static uint8_t g_dst_ip[4];
static uint16_t g_dst_port = 0;
static uint16_t g_src_port = 40000;

static uint32_t g_seq = 0x1000;
static uint32_t g_ack = 0;

static uint8_t g_rx_buf[16384];
static int g_rx_head = 0;
static int g_rx_tail = 0;

static uint32_t g_last_tx_ticks = 0;
static uint32_t g_ticks = 0;
static int g_logged_rst = 0;
static uint32_t g_timewait_start = 0;

static void tcp_reset_state(void) {
    g_state = TCP_CLOSED;
    g_dst_port = 0;
    g_ack = 0;
    g_seq = 0x1000;
    g_rx_head = g_rx_tail = 0;
    g_logged_rst = 0;
    g_timewait_start = 0;
}

static void rx_push(const uint8_t* data, int len) {
    for (int i = 0; i < len; i++) {
        int next = (g_rx_head + 1) % (int)sizeof(g_rx_buf);
        if (next == g_rx_tail) break;
        g_rx_buf[g_rx_head] = data[i];
        g_rx_head = next;
    }
}

int tcp_is_established(void) { return g_state == TCP_ESTABLISHED; }

int tcp_recv_available(void) {
    if (g_rx_head >= g_rx_tail) return g_rx_head - g_rx_tail;
    return (int)sizeof(g_rx_buf) - g_rx_tail + g_rx_head;
}

int tcp_recv_read(void* out, int out_len) {
    uint8_t* o = (uint8_t*)out;
    int n = 0;
    while (n < out_len && g_rx_tail != g_rx_head) {
        o[n++] = g_rx_buf[g_rx_tail];
        g_rx_tail = (g_rx_tail + 1) % (int)sizeof(g_rx_buf);
    }
    return n;
}

static void tcp_tx(uint8_t flags, const void* data, uint16_t len) {
    e1000_send_tcp_packet_ex(g_dst_ip, g_src_port, g_dst_port, g_seq, g_ack, flags, data, len);
    g_last_tx_ticks = g_ticks;
}

int tcp_connect_ipv4(const uint8_t dst_ip[4], uint16_t dst_port) {
    // If a previous connection is still around, forcefully reset our local state.
    // We don't yet support multiple simultaneous sockets.
    if (g_state != TCP_CLOSED) {
        tcp_reset_state();
    }

    memcpy(g_dst_ip, dst_ip, 4);
    g_dst_port = dst_port;
    g_state = TCP_SYN_SENT;
    g_ack = 0;
    g_seq = 0x1000;
    g_rx_head = g_rx_tail = 0;
    g_logged_rst = 0;

    // Pick a new ephemeral source port each connect to avoid TIME_WAIT collisions.
    g_src_port++;
    if (g_src_port < 40000) g_src_port = 40000;

    serial_write_str("TCP: SYN -> ");
    serial_write_dec(dst_ip[0]); serial_write_str(".");
    serial_write_dec(dst_ip[1]); serial_write_str(".");
    serial_write_dec(dst_ip[2]); serial_write_str(".");
    serial_write_dec(dst_ip[3]);
    serial_write_str(":");
    serial_write_dec(dst_port);
    serial_write_str(" (src=");
    serial_write_dec(g_src_port);
    serial_write_str(")");
    serial_write_str("\r\n");

    tcp_tx(TCP_FLAG_SYN, 0, 0);
    return 0;
}

int tcp_send_data(const void* data, uint16_t len) {
    if (g_state != TCP_ESTABLISHED) return -1;
    tcp_tx(TCP_FLAG_ACK | TCP_FLAG_PSH, data, len);
    g_seq += len;
    return 0;
}

int tcp_close(void) {
    if (g_state != TCP_ESTABLISHED) return -1;
    g_state = TCP_FIN_WAIT_1;
    tcp_tx(TCP_FLAG_ACK | TCP_FLAG_FIN, 0, 0);
    g_seq += 1;
    return 0;
}

void tcp_on_packet(const uint8_t src_ip[4], const void* tcp_segment, uint16_t tcp_len) {
    if (tcp_len < (uint16_t)sizeof(tcp_hdr_t)) return;
    if (memcmp(src_ip, g_dst_ip, 4) != 0) return;

    const tcp_hdr_t* tcp = (const tcp_hdr_t*)tcp_segment;
    uint16_t src_port = swap16(tcp->src);
    uint16_t dst_port = swap16(tcp->dst);
    if (src_port != g_dst_port || dst_port != g_src_port) return;

    uint32_t seq = swap32(tcp->seq);
    uint32_t ack = swap32(tcp->ack);
    uint8_t flags = tcp->flags;

    uint8_t off_words = (tcp->res_off >> 4) & 0xF;
    uint16_t hdr_len = (uint16_t)off_words * 4;
    if (hdr_len < sizeof(tcp_hdr_t) || hdr_len > tcp_len) return;

    const uint8_t* payload = (const uint8_t*)tcp_segment + hdr_len;
    uint16_t payload_len = tcp_len - hdr_len;

    (void)ack;

    if (flags & TCP_FLAG_RST) {
        if (!g_logged_rst) {
            g_logged_rst = 1;
            serial_write_str("TCP: RST received\r\n");
        }
        g_state = TCP_CLOSED;
        return;
    }

    if (g_state == TCP_SYN_SENT) {
        if ((flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) == (TCP_FLAG_SYN | TCP_FLAG_ACK)) {
            g_ack = seq + 1;
            tcp_tx(TCP_FLAG_ACK, 0, 0);
            g_state = TCP_ESTABLISHED;
            serial_write_str("TCP: ESTABLISHED\r\n");
            return;
        }
    }

    if (g_state == TCP_ESTABLISHED || g_state == TCP_FIN_WAIT_1 || g_state == TCP_FIN_WAIT_2) {
        if (payload_len > 0) {
            if (seq == g_ack) {
                rx_push(payload, payload_len);
                g_ack += payload_len;
                tcp_tx(TCP_FLAG_ACK, 0, 0);
            } else {
                tcp_tx(TCP_FLAG_ACK, 0, 0);
            }
        }

        if (flags & TCP_FLAG_FIN) {
            if (seq + payload_len == g_ack) {
                g_ack += 1;
            }
            tcp_tx(TCP_FLAG_ACK, 0, 0);
            g_state = TCP_TIME_WAIT;
            g_timewait_start = g_ticks;
            return;
        }

        if (g_state == TCP_FIN_WAIT_1 && (flags & TCP_FLAG_ACK)) {
            g_state = TCP_FIN_WAIT_2;
        }
    }
}

void tcp_poll(void) {
    g_ticks = e1000_tcp_get_ticks();

    if (g_state == TCP_SYN_SENT) {
        if ((g_ticks - g_last_tx_ticks) > 2000) {
            tcp_tx(TCP_FLAG_SYN, 0, 0);
        }
    }

    if (g_state == TCP_TIME_WAIT) {
        if ((g_ticks - g_timewait_start) > 2000) {
            tcp_reset_state();
        }
    }
}
