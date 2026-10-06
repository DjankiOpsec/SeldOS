/*
 * SeldOS - Humboldt Kernel Project
 * Sovereign VLESS Client Engine (Native C99 Freestanding)
 * Implementation of VLESS protocol client with optional SeldTLS 1.3
 * GPLv3 Licensed.
 */

#include "seld_vless.h"
#include <seld.h>
#include <string.h>

static int hex_char_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int seld_vless_parse_uuid(const char* uuid_str, uint8_t uuid_out[16]) {
    if (!uuid_str || !uuid_out) return -1;
    size_t byte_idx = 0;
    int high = -1;

    for (size_t i = 0; uuid_str[i] != '\0' && byte_idx < 16; i++) {
        char c = uuid_str[i];
        if (c == '-') continue;
        int val = hex_char_val(c);
        if (val < 0) return -1;

        if (high < 0) {
            high = val;
        } else {
            uuid_out[byte_idx++] = (uint8_t)((high << 4) | val);
            high = -1;
        }
    }

    return (byte_idx == 16) ? 0 : -1;
}

int seld_vless_connect(struct seld_vless_conn* conn,
                        uint32_t server_ip, uint16_t server_port,
                        const uint8_t uuid[16],
                        const char* target_host, uint16_t target_port,
                        const char* sni, int use_tls) {
    if (!conn || !uuid || !target_host) return -1;
    memset(conn, 0, sizeof(*conn));

    conn->server_ip = server_ip;
    conn->server_port = server_port;
    memcpy(conn->uuid, uuid, 16);
    strncpy(conn->target_host, target_host, sizeof(conn->target_host) - 1);
    conn->target_port = target_port;
    conn->use_tls = use_tls;

    // 1. Establish raw TCP connection to VLESS endpoint
    int sock = seld_tcp_connect(server_ip, server_port);
    if (sock < 0) return -2;
    conn->tcp_sock = sock;

    // 2. Perform TLS 1.3 handshake if requested (with In-Kernel TCP Desync protection)
    if (use_tls) {
        const char* sni_val = (sni && sni[0] != '\0') ? sni : target_host;
        if (seld_tls_handshake(&conn->tls, sock, sni_val) != 0) {
            seld_tcp_close(sock);
            return -3;
        }
    }

    // 3. Build VLESS Request Header
    uint8_t hdr[256];
    size_t hp = 0;

    hdr[hp++] = 0x00; // VLESS Protocol Version 0
    memcpy(hdr + hp, uuid, 16); hp += 16;
    hdr[hp++] = 0x00; // Addons length: 0
    hdr[hp++] = VLESS_CMD_TCP; // Command: 0x01 (TCP stream)
    hdr[hp++] = (uint8_t)(target_port >> 8);
    hdr[hp++] = (uint8_t)(target_port & 0xFF);

    // Target address encoding
    int is_ipv4 = 0;
    uint32_t tip = 0;
    int dots = 0;
    for (size_t i = 0; target_host[i]; i++) {
        if (target_host[i] == '.') dots++;
    }
    if (dots == 3) {
        unsigned int a=0, b=0, c=0, d=0;
        const char* s = target_host;
        while (*s >= '0' && *s <= '9') { a = a * 10 + (*s - '0'); s++; }
        if (*s == '.') s++;
        while (*s >= '0' && *s <= '9') { b = b * 10 + (*s - '0'); s++; }
        if (*s == '.') s++;
        while (*s >= '0' && *s <= '9') { c = c * 10 + (*s - '0'); s++; }
        if (*s == '.') s++;
        while (*s >= '0' && *s <= '9') { d = d * 10 + (*s - '0'); s++; }
        if (a <= 255 && b <= 255 && c <= 255 && d <= 255 && *s == '\0') {
            is_ipv4 = 1;
            tip = (uint32_t)(a | (b << 8) | (c << 16) | (d << 24));
        }
    }

    if (is_ipv4) {
        hdr[hp++] = VLESS_ADDR_IPV4;
        memcpy(hdr + hp, &tip, 4);
        hp += 4;
    } else {
        hdr[hp++] = VLESS_ADDR_DOMAIN;
        size_t dlen = strlen(target_host);
        if (dlen > 200) dlen = 200;
        hdr[hp++] = (uint8_t)dlen;
        memcpy(hdr + hp, target_host, dlen);
        hp += dlen;
    }

    // 4. Send VLESS Request Header
    if (use_tls) {
        if (seld_tls_write(&conn->tls, hdr, hp) != (int)hp) {
            seld_tls_close(&conn->tls);
            seld_tcp_close(sock);
            return -4;
        }
    } else {
        if (seld_tcp_send(sock, hdr, hp) != (int)hp) {
            seld_tcp_close(sock);
            return -4;
        }
    }

    // 5. Receive VLESS Server Response Header (Version 0x00, Addons Len 0x00)
    uint8_t resp[2];
    int rlen = 0;
    if (use_tls) {
        rlen = seld_tls_read(&conn->tls, resp, 2, 5000);
    } else {
        rlen = seld_tcp_recv(sock, resp, 2, 5000);
    }

    if (rlen < 2 || resp[0] != 0x00) {
        if (use_tls) seld_tls_close(&conn->tls);
        seld_tcp_close(sock);
        return -5;
    }

    uint8_t addon_len = resp[1];
    if (addon_len > 0) {
        uint8_t skip[256];
        if (use_tls) {
            seld_tls_read(&conn->tls, skip, addon_len, 2000);
        } else {
            seld_tcp_recv(sock, skip, addon_len, 2000);
        }
    }

    conn->connected = 1;
    return 0;
}

int seld_vless_send(struct seld_vless_conn* conn, const void* data, size_t len) {
    if (!conn || !conn->connected || !data || len == 0) return -1;
    if (conn->use_tls) {
        return seld_tls_write(&conn->tls, data, len);
    } else {
        return seld_tcp_send(conn->tcp_sock, data, len);
    }
}

int seld_vless_recv(struct seld_vless_conn* conn, void* buf, size_t max_len, uint32_t timeout_ms) {
    if (!conn || !conn->connected || !buf || max_len == 0) return -1;
    if (conn->use_tls) {
        return seld_tls_read(&conn->tls, buf, max_len, timeout_ms);
    } else {
        return seld_tcp_recv(conn->tcp_sock, buf, max_len, timeout_ms);
    }
}

void seld_vless_close(struct seld_vless_conn* conn) {
    if (!conn) return;
    if (conn->connected) {
        if (conn->use_tls) {
            seld_tls_close(&conn->tls);
        }
        seld_tcp_close(conn->tcp_sock);
        conn->connected = 0;
    }
}
