/*
 * SeldOS - Humboldt Kernel Project
 * Tor Browser (OpSec Sovereign Edition)
 * SOCKS5 Onion Protocol Client (RFC 1928 + Tor Extensions)
 * GPLv3 Licensed.
 */

#ifndef SELD_TOR_SOCKS5_H
#define SELD_TOR_SOCKS5_H

#include <stdint.h>
#include <stddef.h>

/*
 * Establishes a TCP stream to target_host:target_port through a Tor SOCKS5 proxy.
 * Uses ATYP=0x03 (Domain Name) to delegate DNS resolution directly to the upstream proxy.
 * Returns connected socket fd on success, negative error code on failure.
 */
int socks5_connect(uint32_t proxy_ip, uint16_t proxy_port, const char* target_host, uint16_t target_port);

#endif /* SELD_TOR_SOCKS5_H */
