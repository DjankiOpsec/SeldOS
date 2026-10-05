/*
 * SeldOS - Humboldt Kernel Project
 * SeldTLS - Native Sovereign TLS 1.3 Subsystem
 * Curve25519 / X25519 Elliptic Curve Key Exchange (RFC 7748)
 * Pure C Freestanding Implementation.
 * GPLv3 Licensed.
 */

#ifndef _SELD_X25519_H_
#define _SELD_X25519_H_

#include <stdint.h>
#include <stddef.h>

void seld_x25519(uint8_t out[32], const uint8_t scalar[32], const uint8_t u_point[32]);
void seld_x25519_base(uint8_t out[32], const uint8_t scalar[32]);

#endif /* _SELD_X25519_H_ */
