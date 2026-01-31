#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NET_STATUS_NONE,
    NET_STATUS_DETECTING,
    NET_STATUS_LINK_DOWN,
    NET_STATUS_LINK_UP,
    NET_STATUS_CONNECTED // IP obtained (mocked for now)
} network_status_t;

network_status_t network_get_status();
void network_get_mac(uint8_t mac[6]);
void network_get_ip(uint8_t ip[4]);

#ifdef __cplusplus
}
#endif

#endif
