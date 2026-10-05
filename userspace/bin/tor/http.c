/*
 * SeldOS - Humboldt Kernel Project
 * Tor Browser (OpSec Sovereign Edition)
 * Privacy-Hardened HTTP/1.1 Client Engine
 * GPLv3 Licensed.
 */

#include "http.h"
#include "socks5.h"
#include <seld.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_url(const char* url, char* host_out, size_t host_sz,
                     uint16_t* port_out, char* path_out, size_t path_sz) {
    if (!url) return -1;
    const char* p = url;

    // Skip leading whitespace / control characters
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (!*p) return -1;

    int default_port = 80;
    if (strncmp(p, "http://", 7) == 0) {
        p += 7;
        default_port = 80;
    } else if (strncmp(p, "https://", 8) == 0) {
        p += 8;
        default_port = 443;
    } else if (strncmp(p, "onion://", 8) == 0) {
        p += 8;
        default_port = 80;
    }

    size_t hlen = 0;
    while (*p && *p != ':' && *p != '/' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && hlen < host_sz - 1) {
        host_out[hlen++] = *p++;
    }
    host_out[hlen] = '\0';
    while (hlen > 0 && host_out[hlen - 1] == '.') {
        host_out[--hlen] = '\0';
    }
    if (hlen == 0) return -1;

    uint16_t port = (uint16_t)default_port;
    if (*p == ':') {
        p++;
        port = 0;
        while (*p >= '0' && *p <= '9') {
            port = (uint16_t)(port * 10 + (*p++ - '0'));
        }
        if (port == 0) port = (uint16_t)default_port;
    }
    *port_out = port;

    if (*p == '/') {
        size_t plen = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && plen < path_sz - 1) {
            path_out[plen++] = *p++;
        }
        path_out[plen] = '\0';
    } else {
        strncpy(path_out, "/", path_sz - 1);
        path_out[path_sz - 1] = '\0';
    }

    return 0;
}

static uint32_t parse_ipv4(const char* str) {
    unsigned int o1 = 0, o2 = 0, o3 = 0, o4 = 0;
    const char* p = str;
    while (*p >= '0' && *p <= '9') o1 = o1 * 10 + (*p++ - '0');
    if (*p == '.') p++; else return 0;
    while (*p >= '0' && *p <= '9') o2 = o2 * 10 + (*p++ - '0');
    if (*p == '.') p++; else return 0;
    while (*p >= '0' && *p <= '9') o3 = o3 * 10 + (*p++ - '0');
    if (*p == '.') p++; else return 0;
    while (*p >= '0' && *p <= '9') o4 = o4 * 10 + (*p++ - '0');
    if (o1 <= 255 && o2 <= 255 && o3 <= 255 && o4 <= 255) {
        return (uint32_t)(o1 | (o2 << 8) | (o3 << 16) | (o4 << 24));
    }
    return 0;
}

static long hex_to_long(const char* s) {
    long val = 0;
    while (*s) {
        if (*s >= '0' && *s <= '9') val = val * 16 + (*s - '0');
        else if (*s >= 'a' && *s <= 'f') val = val * 16 + (*s - 'a' + 10);
        else if (*s >= 'A' && *s <= 'F') val = val * 16 + (*s - 'A' + 10);
        else break;
        s++;
    }
    return val;
}

static void dechunk_payload(char* body, size_t* body_len) {
    if (!body || *body_len == 0) return;
    char* src = body;
    char* dst = body;
    size_t remaining = *body_len;

    while (remaining > 0) {
        // Read chunk hex length
        char* end_line = strstr(src, "\r\n");
        if (!end_line) break;
        long chunk_size = hex_to_long(src);
        if (chunk_size <= 0) break; // 0 chunk means EOF

        src = end_line + 2;
        if ((size_t)(src - body) + (size_t)chunk_size > *body_len) break;

        memmove(dst, src, (size_t)chunk_size);
        dst += chunk_size;
        src += chunk_size;

        if (strncmp(src, "\r\n", 2) == 0) {
            src += 2;
        }
        remaining = *body_len - (size_t)(src - body);
    }

    *dst = '\0';
    *body_len = (size_t)(dst - body);
}

static int s_tor_socks_down = 0;

void http_reset_tor_state(void) {
    s_tor_socks_down = 0;
}

int http_fetch(const char* url, uint32_t proxy_ip, uint16_t proxy_port, struct http_response* resp) {
    if (!url || !resp) return -1;
    memset(resp, 0, sizeof(*resp));

    if (proxy_ip == 0) {
        proxy_ip = (10 | (0 << 8) | (2 << 16) | (2 << 24)); // Default QEMU Gateway 10.0.2.2
    }

    char cur_url[HTTP_MAX_URL_LEN];
    strncpy(cur_url, url, sizeof(cur_url) - 1);
    cur_url[sizeof(cur_url) - 1] = '\0';

    int redirects = 0;

    while (redirects++ < 3) {
        char host[128];
        char path[128];
        uint16_t port = 80;

        if (parse_url(cur_url, host, sizeof(host), &port, path, sizeof(path)) != 0) {
            return -2; // Invalid URL
        }

        strncpy(resp->final_url, cur_url, sizeof(resp->final_url) - 1);

        int sock = -1;
        int used_tor = 0;
        int is_https = (port == 443 || strncmp(cur_url, "https://", 8) == 0);
        int is_onion = (strstr(host, ".onion") != NULL);
        struct seld_tls_conn* tls = NULL;

        if (is_onion) {
            // Tor Onion Service via SOCKS5
            if (!s_tor_socks_down) {
                sock = socks5_connect(proxy_ip, proxy_port, host, port);
                if (sock >= 0) {
                    used_tor = 1;
                } else {
                    return -14; // Onion service unreachable / Tor daemon offline
                }
            } else {
                return -14;
            }
        } else if (is_https) {
            // Sovereign HTTPS: Direct TLS 1.3 negotiation inside SeldOS
            uint32_t ip = parse_ipv4(host);
            if (ip == 0) {
                struct seld_net_info ninfo;
                if (seld_net_info(&ninfo) == 0 && !ninfo.link_up) {
                    return -15; // Network interface offline
                }
                int dns_err = seld_dns_resolve(host, &ip);
                if (dns_err != 0 || ip == 0) {
                    return (dns_err == -4) ? -15 : -3; // DNS resolution failed
                }
            }

            sock = seld_tcp_connect(ip, port);
            if (sock < 0) {
                return -4; // TCP connection failed
            }

            tls = (struct seld_tls_conn*)malloc(sizeof(struct seld_tls_conn));
            if (!tls) {
                seld_tcp_close(sock);
                return -6;
            }

            int hs = seld_tls_handshake(tls, sock, host);
            if (hs != 0) {
                seld_tls_close(tls);
                free(tls);
                return -8; // SeldTLS 1.3 handshake failed
            }

            seld_tls_get_cert(tls, &resp->cert);
            used_tor = 2; // SeldTLS 1.3 Native Sovereign Connection
        } else {
            // Standard Clearnet HTTP: Try Tor SOCKS5 first, then Native DNS & Direct TCP
            if (!s_tor_socks_down) {
                sock = socks5_connect(proxy_ip, proxy_port, host, port);
                if (sock >= 0) {
                    used_tor = 1;
                } else {
                    s_tor_socks_down = 1; // SOCKS daemon not responding, use native clearnet
                }
            }

            if (sock < 0) {
                uint32_t ip = parse_ipv4(host);
                if (ip == 0 && (strcmp(host, "gateway") == 0 || strcmp(host, "localhost") == 0)) {
                    struct seld_net_info ninfo;
                    if (seld_net_info(&ninfo) == 0) {
                        ip = (strcmp(host, "gateway") == 0) ? ninfo.gateway : ninfo.ip;
                    }
                }

                if (ip == 0) {
                    struct seld_net_info ninfo;
                    if (seld_net_info(&ninfo) == 0 && !ninfo.link_up) {
                        return -15; // Network interface offline
                    }
                    int dns_err = seld_dns_resolve(host, &ip);
                    if (dns_err != 0 || ip == 0) {
                        return (dns_err == -4) ? -15 : -3; // DNS resolution failed
                    }
                }

                if (sock < 0 && ip != 0) {
                    sock = seld_tcp_connect(ip, port);
                    if (sock >= 0) {
                        used_tor = 0; // Direct Native Clearnet
                    }
                }
            }
        }

        if (sock < 0 && !tls) {
            return -4; // Connection failed
        }

        resp->used_tor = used_tor;

        // Step 2: Build standardized privacy HTTP/1.1 request
        char req[1024];
        snprintf(req, sizeof(req),
                 "GET %s HTTP/1.1\r\n"
                 "Host: %s\r\n"
                 "User-Agent: Mozilla/5.0 (Windows NT 10.0; rv:109.0) Gecko/20100101 Firefox/115.0\r\n"
                 "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
                 "Accept-Language: en-US,en;q=0.5\r\n"
                 "DNT: 1\r\n"
                 "Sec-GPC: 1\r\n"
                 "Connection: close\r\n\r\n",
                 path, host);

        size_t req_len = strlen(req);
        if (is_https && tls) {
            if (seld_tls_write(tls, req, req_len) <= 0) {
                seld_tls_close(tls);
                free(tls);
                return -5;
            }
        } else {
            if (seld_tcp_send(sock, req, req_len) != (int)req_len) {
                seld_tcp_close(sock);
                return -5;
            }
        }

        // Step 3: Receive HTTP response into dynamic buffer
        size_t buf_cap = HTTP_MAX_RESP_LEN;
        char* raw_buf = (char*)malloc(buf_cap);
        if (!raw_buf) {
            if (is_https && tls) {
                seld_tls_close(tls);
                free(tls);
            } else if (sock >= 0) {
                seld_tcp_close(sock);
            }
            return -6;
        }

        size_t raw_len = 0;
        if (is_https && tls) {
            while (raw_len + 1 < buf_cap) {
                int n = seld_tls_read(tls, (uint8_t*)raw_buf + raw_len, buf_cap - raw_len - 1, 8000);
                if (n > 0) {
                    raw_len += (size_t)n;
                } else if (n == 0) {
                    break; // EOF or TLS close_notify
                } else {
                    break;
                }
            }
            seld_tls_close(tls);
            free(tls);
            tls = NULL;
        } else {
            uint32_t first_timeout = used_tor ? 15000 : 5000;
            while (raw_len + 1 < buf_cap) {
                uint32_t to_ms = (raw_len == 0) ? first_timeout : 3000;
                int n = seld_tcp_recv(sock, raw_buf + raw_len, buf_cap - raw_len - 1, to_ms);
                if (n > 0) {
                    raw_len += (size_t)n;
                } else if (n == 0 || n == -2) {
                    break; // EOF or timeout
                }
            }
            seld_tcp_close(sock);
            sock = -1;
        }
        raw_buf[raw_len] = '\0';


        if (raw_len == 0) {
            free(raw_buf);
            return -7; // Empty response
        }

        // Step 4: Parse HTTP Status Line
        int status = 0;
        if (strncmp(raw_buf, "HTTP/1.0 ", 9) == 0 || strncmp(raw_buf, "HTTP/1.1 ", 9) == 0) {
            status = atoi(raw_buf + 9);
        }
        resp->status_code = status;

        // Step 5: Check Headers & Locate Body
        char* hdr_end = strstr(raw_buf, "\r\n\r\n");
        size_t body_offset = 0;
        if (hdr_end) {
            body_offset = (size_t)(hdr_end + 4 - raw_buf);
        } else {
            hdr_end = strstr(raw_buf, "\n\n");
            if (hdr_end) body_offset = (size_t)(hdr_end + 2 - raw_buf);
            else body_offset = 0;
        }

        // Handle Redirects (301, 302, 303, 307)
        if (status == 301 || status == 302 || status == 303 || status == 307) {
            char* loc = strstr(raw_buf, "Location: ");
            if (!loc) loc = strstr(raw_buf, "location: ");
            if (loc) {
                loc += 10;
                char* loc_end = strstr(loc, "\r\n");
                if (!loc_end) loc_end = strchr(loc, '\n');
                if (loc_end) {
                    size_t loc_len = (size_t)(loc_end - loc);
                    if (loc[0] == '/') {
                        snprintf(cur_url, sizeof(cur_url), "http%s://%s%.*s",
                                 (port == 443 || used_tor == 2) ? "s" : "",
                                 host, (int)loc_len, loc);
                    } else if (loc_len < sizeof(cur_url)) {
                        memcpy(cur_url, loc, loc_len);
                        cur_url[loc_len] = '\0';
                    }
                    free(raw_buf);
                    continue; // Follow redirect
                }
            }
        }

        // Check for chunked encoding
        int is_chunked = (strstr(raw_buf, "Transfer-Encoding: chunked") != NULL ||
                          strstr(raw_buf, "transfer-encoding: chunked") != NULL);

        // Extract body
        size_t body_len = (raw_len > body_offset) ? (raw_len - body_offset) : 0;
        char* body_buf = (char*)malloc(body_len + 1);
        if (!body_buf) {
            free(raw_buf);
            return -8;
        }
        memcpy(body_buf, raw_buf + body_offset, body_len);
        body_buf[body_len] = '\0';
        free(raw_buf);

        if (is_chunked) {
            dechunk_payload(body_buf, &body_len);
        }

        resp->body = body_buf;
        resp->body_len = body_len;
        return 0; // Success
    }

    return -9; // Exceeded redirect limit
}

void http_response_free(struct http_response* resp) {
    if (resp && resp->body) {
        memset(resp->body, 0, resp->body_len); // Clean memory (OpSec zero-trace)
        free(resp->body);
        resp->body = NULL;
        resp->body_len = 0;
    }
}
