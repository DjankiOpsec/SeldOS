/*
 * SeldOS - Humboldt Kernel Project
 * Tor Browser (OpSec Sovereign Edition)
 * Privacy-Hardened HTTP/1.1 Client Engine
 * GPLv3 Licensed.
 */

#ifndef SELD_TOR_HTTP_H
#define SELD_TOR_HTTP_H

#include <stdint.h>
#include <stddef.h>
#include <seld_tls.h>

#define HTTP_MAX_URL_LEN 256
#define HTTP_MAX_RESP_LEN (128 * 1024) // 128 KiB HTML buffer
#define DDG_ONION_HOST "duckduckgogg42xjoc72x3sjasowoarfbgcmvfimaftt6twagswzczad.onion"

struct http_response {
    int   status_code;
    int   used_tor;
    char  final_url[HTTP_MAX_URL_LEN];
    char  content_type[64];
    char* body;
    size_t body_len;
    struct seld_tls_cert cert;
};


/*
 * Fetches an HTTP resource via Tor SOCKS5 or direct fallback.
 * Automatically sanitizes outgoing headers to prevent fingerprinting.
 * Follows HTTP 301/302 redirects up to 3 times.
 * Returns 0 on success, negative error code on failure.
 */
int http_fetch(const char* url, uint32_t proxy_ip, uint16_t proxy_port, struct http_response* resp);

void http_response_free(struct http_response* resp);
void http_reset_tor_state(void);

#endif /* SELD_TOR_HTTP_H */
