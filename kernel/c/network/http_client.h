#ifndef TINY64_HTTP_CLIENT_H
#define TINY64_HTTP_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

int http_get_url(const char* url, char* out_body, int out_body_sz);

#ifdef __cplusplus
}
#endif

#endif
