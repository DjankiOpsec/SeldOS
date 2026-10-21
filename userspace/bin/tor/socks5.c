/*
 * SeldOS - Humboldt Kernel Project
 * Tor Browser (OpSec Sovereign Edition)
 * SOCKS5 Onion Protocol Client (RFC 1928 + RFC 1929 Tor Stream Isolation)
 * RFC 1928 ATYP=0x03 Domain Name Framing & IsolateSOCKSAuth Circuit Protection
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#include "socks5.h"
#include <seld.h>
#include <string.h>
#include <stdio.h>

static uint32_t s_session_salt = 0;

void socks5_set_session_salt(uint32_t salt) {
    s_session_salt = salt;
}

int socks5_connect_isolated(uint32_t proxy_ip, uint16_t proxy_port, const char* target_host, uint16_t target_port, const char* isolation_token) {
    if (!target_host || target_port == 0) return -1;
    size_t host_len = strlen(target_host);
    if (host_len == 0 || host_len > 255) return -1;

    // Step 1: Open raw TCP connection to Tor SOCKS5 proxy daemon
    int sock = seld_tcp_connect(proxy_ip, proxy_port);
    if (sock < 0) {
        return -10; // Proxy offline or connection refused
    }

    // Step 2: SOCKS5 Authentication Negotiation offering both:
    // Method 0x00: No Authentication
    // Method 0x02: Username/Password (RFC 1929) for Tor Circuit Stream Isolation
    uint8_t auth_req[4] = { 0x05, 0x02, 0x00, 0x02 };
    if (seld_tcp_send(sock, auth_req, sizeof(auth_req)) != sizeof(auth_req)) {
        seld_tcp_close(sock);
        return -11;
    }

    uint8_t auth_resp[2];
    int n = seld_tcp_recv(sock, auth_resp, sizeof(auth_resp), 2000);
    if (n != 2 || auth_resp[0] != 0x05) {
        seld_tcp_close(sock);
        return -11; // Auth negotiation failed
    }

    if (auth_resp[1] == 0x02) {
        // Method 0x02 selected by Tor daemon: negotiate RFC 1929 username/password
        // Tor uses (User, Pass) as the stream isolation token (IsolateSOCKSAuth).
        char iso_id[64];
        const char* token = isolation_token ? isolation_token : target_host;
        size_t tlen = strlen(token);
        if (tlen > 44) tlen = 44;
        memcpy(iso_id, token, tlen);
        iso_id[tlen] = '\0';

        if (s_session_salt != 0) {
            char salt_buf[16];
            snprintf(salt_buf, sizeof(salt_buf), "_%u", (unsigned int)s_session_salt);
            strncat(iso_id, salt_buf, sizeof(iso_id) - strlen(iso_id) - 1);
        }

        size_t id_len = strlen(iso_id);
        if (id_len > 255) id_len = 255;

        uint8_t up_req[256];
        up_req[0] = 0x01; // RFC 1929 version 1
        up_req[1] = (uint8_t)id_len;
        memcpy(up_req + 2, iso_id, id_len);
        up_req[2 + id_len] = (uint8_t)id_len;
        memcpy(up_req + 2 + id_len + 1, iso_id, id_len);

        size_t up_total = 1 + 1 + id_len + 1 + id_len;
        if (seld_tcp_send(sock, up_req, up_total) != (int)up_total) {
            seld_tcp_close(sock);
            return -11;
        }

        uint8_t up_resp[2];
        if (seld_tcp_recv(sock, up_resp, 2, 2000) != 2 || up_resp[1] != 0x00) {
            seld_tcp_close(sock);
            return -11; // Auth rejected
        }
    } else if (auth_resp[1] != 0x00) {
        seld_tcp_close(sock);
        return -11; // Unsupported auth method
    }

    // Step 3: SOCKS5 CONNECT Request with ATYP=0x03 (Domain Name)
    // OpSec: Target hostname is forwarded directly to Tor Exit Relay.
    // Zero local DNS requests are generated.
    uint8_t req[512];
    req[0] = 0x05; // VER: SOCKS5
    req[1] = 0x01; // CMD: CONNECT
    req[2] = 0x00; // RSV
    req[3] = 0x03; // ATYP: DOMAINNAME
    req[4] = (uint8_t)host_len;
    memcpy(req + 5, target_host, host_len);
    req[5 + host_len]     = (uint8_t)((target_port >> 8) & 0xFF);
    req[5 + host_len + 1] = (uint8_t)(target_port & 0xFF);

    size_t req_len = 5 + host_len + 2;
    if (seld_tcp_send(sock, req, req_len) != (int)req_len) {
        seld_tcp_close(sock);
        return -12;
    }

    // Step 4: Await SOCKS5 Response Header (4 bytes: VER, REP, RSV, ATYP)
    uint8_t resp_hdr[4];
    n = seld_tcp_recv(sock, resp_hdr, sizeof(resp_hdr), 15000);
    if (n != 4 || resp_hdr[0] != 0x05) {
        seld_tcp_close(sock);
        return -12; // Invalid response
    }

    if (resp_hdr[1] != 0x00) {
        // Tor daemon reported error
        seld_tcp_close(sock);
        return -13;
    }

    // Drain bound address and port based on ATYP so HTTP stream is completely clean
    uint8_t drain_buf[256];
    size_t to_drain = 0;
    if (resp_hdr[3] == 0x01) {
        to_drain = 4 + 2; // IPv4 (4) + Port (2)
    } else if (resp_hdr[3] == 0x04) {
        to_drain = 16 + 2; // IPv6 (16) + Port (2)
    } else if (resp_hdr[3] == 0x03) {
        uint8_t dlen = 0;
        if (seld_tcp_recv(sock, &dlen, 1, 2000) == 1) {
            to_drain = (size_t)dlen + 2;
        }
    }

    if (to_drain > 0 && to_drain <= sizeof(drain_buf)) {
        seld_tcp_recv(sock, drain_buf, to_drain, 3000);
    }

    // Handshake successful! Stream is cleanly isolated and tunneled through Tor circuit.
    return sock;
}

int socks5_connect(uint32_t proxy_ip, uint16_t proxy_port, const char* target_host, uint16_t target_port) {
    return socks5_connect_isolated(proxy_ip, proxy_port, target_host, target_port, target_host);
}
