/*
 * SeldOS - Humboldt Kernel Project
 * Sovereign VLESS Client Engine (Native C99 Freestanding)
 * Anti-Censorship & Stealth Protocol Implementation
 * GPLv3 Licensed.
 */

#ifndef _SELD_VLESS_H_
#define _SELD_VLESS_H_

#include <stdint.h>
#include <stddef.h>
#include "seld_tls.h"

#define VLESS_CMD_TCP 0x01
#define VLESS_CMD_UDP 0x02

#define VLESS_ADDR_IPV4   0x01
#define VLESS_ADDR_DOMAIN 0x02
#define VLESS_ADDR_IPV6   0x03

struct seld_vless_conn {
    int                  tcp_sock;
    int                  use_tls;
    struct seld_tls_conn tls;
    uint8_t              uuid[16];
    uint32_t             server_ip;
    uint16_t             server_port;
    char                 target_host[128];
    uint16_t             target_port;
    int                  connected;
};

int  seld_vless_parse_uuid(const char* uuid_str, uint8_t uuid_out[16]);
int  seld_vless_connect(struct seld_vless_conn* conn,
                        uint32_t server_ip, uint16_t server_port,
                        const uint8_t uuid[16],
                        const char* target_host, uint16_t target_port,
                        const char* sni, int use_tls);
int  seld_vless_send(struct seld_vless_conn* conn, const void* data, size_t len);
int  seld_vless_recv(struct seld_vless_conn* conn, void* buf, size_t max_len, uint32_t timeout_ms);
void seld_vless_close(struct seld_vless_conn* conn);

#endif /* _SELD_VLESS_H_ */
