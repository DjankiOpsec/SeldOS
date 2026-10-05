/*
 * SeldOS - Humboldt Kernel Project
 * SeldTLS - Native Sovereign TLS 1.3 Subsystem
 * HMAC-SHA256 and HKDF (RFC 5869 / RFC 8446) Engine
 * GPLv3 Licensed.
 */

#ifndef _SELD_HKDF_H_
#define _SELD_HKDF_H_

#include <stdint.h>
#include <stddef.h>

void seld_hmac_sha256(const uint8_t* key, size_t key_len,
                      const uint8_t* data, size_t data_len,
                      uint8_t out[32]);

void seld_hkdf_extract(const uint8_t* salt, size_t salt_len,
                       const uint8_t* ikm, size_t ikm_len,
                       uint8_t prk[32]);

void seld_hkdf_expand_label(const uint8_t prk[32],
                            const char* label,
                            const uint8_t* context, size_t context_len,
                            uint16_t length,
                            uint8_t* out);

void seld_tls13_derive_secret(const uint8_t secret[32],
                              const char* label,
                              const uint8_t* transcript_hash,
                              uint8_t out[32]);

#endif /* _SELD_HKDF_H_ */
