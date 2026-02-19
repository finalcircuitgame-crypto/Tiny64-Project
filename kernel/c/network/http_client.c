#include "http_client.h"

#include "tcp_client.h"
#include "../drivers/network/e1000.h"

#include "../lib/string.h"


static int starts_with(const char* s, const char* p) {
    while (*p) {
        if (*s != *p) return 0;
        s++; p++;
    }
    return 1;
}

static void parse_url(const char* url, char* scheme, int scheme_sz, char* host, int host_sz, char* path, int path_sz) {
    scheme[0] = 0;
    host[0] = 0;
    path[0] = 0;

    const char* p = url;
    if (starts_with(p, "http://")) {
        strncpy(scheme, "http", scheme_sz - 1);
        p += 7;
    } else if (starts_with(p, "https://")) {
        strncpy(scheme, "https", scheme_sz - 1);
        p += 8;
    } else {
        strncpy(scheme, "http", scheme_sz - 1);
    }

    const char* host_start = p;
    while (*p && *p != '/' && *p != ' ') p++;
    int hlen = (int)(p - host_start);
    if (hlen > host_sz - 1) hlen = host_sz - 1;
    memcpy(host, host_start, hlen);
    host[hlen] = 0;

    if (*p == '/') {
        strncpy(path, p, path_sz - 1);
    } else {
        strncpy(path, "/", path_sz - 1);
    }
    path[path_sz - 1] = 0;
}

int http_get_url(const char* url, char* out_body, int out_body_sz) {
    if (!url || !out_body || out_body_sz <= 1) return -1;

    char scheme[8];
    char host[128];
    char path[256];
    parse_url(url, scheme, sizeof(scheme), host, sizeof(host), path, sizeof(path));

    // QEMU user-net gateway (host-side) used as a simple HTTP gateway / TLS offload target.
    // This avoids explicit proxy semantics (absolute-form request lines).
    uint8_t gw_ip[4] = {10, 0, 2, 2};
    uint16_t gw_port = 80;
    if (!starts_with(scheme, "http")) {
        strncpy(scheme, "http", sizeof(scheme) - 1);
        scheme[sizeof(scheme) - 1] = 0;
    }
    if (starts_with(scheme, "https")) {
        gw_port = 443;
    }

    tcp_connect_ipv4(gw_ip, gw_port);

    uint32_t start = e1000_tcp_get_ticks();
    while ((e1000_tcp_get_ticks() - start) < 10000) {
        e1000_poll_rx();
        tcp_poll();
        if (tcp_is_established()) break;
    }

    if (!tcp_is_established()) {
        strncpy(out_body, "[TCP] connect failed", out_body_sz - 1);
        out_body[out_body_sz - 1] = 0;
        return -1;
    }

    char req[1024];
    memset(req, 0, sizeof(req));
    strcpy(req, "GET ");
    strncat(req, path, sizeof(req) - strlen(req) - 1);
    strncat(req, " HTTP/1.0\r\nHost: ", sizeof(req) - strlen(req) - 1);
    strncat(req, host, sizeof(req) - strlen(req) - 1);
    strncat(req, "\r\nUser-Agent: Tiny64\r\nConnection: close\r\n\r\n", sizeof(req) - strlen(req) - 1);

    tcp_send_data(req, (uint16_t)strlen(req));

    char resp[16384];
    int resp_len = 0;

    uint32_t t0 = e1000_tcp_get_ticks();
    while ((e1000_tcp_get_ticks() - t0) < 20000 && resp_len < (int)sizeof(resp) - 1) {
        e1000_poll_rx();
        tcp_poll();

        int avail = tcp_recv_available();
        if (avail > 0) {
            int n = tcp_recv_read(resp + resp_len, (int)sizeof(resp) - 1 - resp_len);
            if (n > 0) resp_len += n;
            t0 = e1000_tcp_get_ticks();
        }
    }
    resp[resp_len] = 0;

    tcp_close();

    // Strip headers
    const char* body = resp;
    for (int i = 0; i + 3 < resp_len; i++) {
        if (resp[i] == '\r' && resp[i + 1] == '\n' && resp[i + 2] == '\r' && resp[i + 3] == '\n') {
            body = &resp[i + 4];
            break;
        }
    }

    // Fallback: LF-only separator
    if (body == resp) {
        for (int i = 0; i + 1 < resp_len; i++) {
            if (resp[i] == '\n' && resp[i + 1] == '\n') {
                body = &resp[i + 2];
                break;
            }
        }
    }

    strncpy(out_body, body, out_body_sz - 1);
    out_body[out_body_sz - 1] = 0;
    return 0;
}
