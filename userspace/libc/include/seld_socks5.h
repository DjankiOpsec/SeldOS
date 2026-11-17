/*
 * SeldOS - Humboldt Kernel Project
 * SOCKS5 Onion Protocol Client (RFC 1928 + RFC 1929 Tor Stream Isolation)
 * RFC 1928 ATYP=0x03 Domain Name Framing & IsolateSOCKSAuth Circuit Protection
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#ifndef _SELD_SOCKS5_H_
#define _SELD_SOCKS5_H_

#include <stdint.h>
#include <stddef.h>

/*
 * Establishes a TCP stream to target_host:target_port through a Tor SOCKS5 proxy.
 * Automatically provisions isolated circuits per target domain (IsolateSOCKSAuth).
 * Uses ATYP=0x03 (Domain Name) to delegate DNS resolution directly to the upstream proxy.
 * Returns connected socket fd on success, negative error code on failure.
 */
int seld_socks5_connect(uint32_t proxy_ip, uint16_t proxy_port, const char* target_host, uint16_t target_port);

/*
 * Explicit Stream-Isolated SOCKS5 connection with custom isolation token (eTLD+1).
 */
int seld_socks5_connect_isolated(uint32_t proxy_ip, uint16_t proxy_port, const char* target_host, uint16_t target_port, const char* isolation_token);

/*
 * Rotates the session isolation salt (e.g. on New Identity 'N' press)
 * to ensure all future connections use brand-new, uncorrelated circuits.
 */
void seld_socks5_set_session_salt(uint32_t salt);

/* Legacy Tor Browser aliases */
int socks5_connect(uint32_t proxy_ip, uint16_t proxy_port, const char* target_host, uint16_t target_port);
int socks5_connect_isolated(uint32_t proxy_ip, uint16_t proxy_port, const char* target_host, uint16_t target_port, const char* isolation_token);
void socks5_set_session_salt(uint32_t salt);

#endif /* _SELD_SOCKS5_H_ */
