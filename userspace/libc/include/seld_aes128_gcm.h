/*
 * SeldOS - Humboldt Kernel Project
 * SeldTLS - Native Sovereign TLS 1.3 Subsystem
 * AES-128 and AES-GCM (NIST SP 800-38D) Engine
 * GPLv3 Licensed.
 */

#ifndef _SELD_AES128_GCM_H_
#define _SELD_AES128_GCM_H_

#include <stdint.h>
#include <stddef.h>

struct seld_aes128_ctx {
    uint32_t round_keys[44];
};

void seld_aes128_init(struct seld_aes128_ctx* ctx, const uint8_t key[16]);
void seld_aes128_encrypt_block(const struct seld_aes128_ctx* ctx, const uint8_t in[16], uint8_t out[16]);

/* AES-128-GCM AEAD encryption and decryption with 12-byte IV and 16-byte tag */
void seld_aes128_gcm_encrypt(const uint8_t key[16], const uint8_t iv[12],
                             const uint8_t* aad, size_t aad_len,
                             const uint8_t* plaintext, size_t pt_len,
                             uint8_t* ciphertext, uint8_t tag[16]);

int seld_aes128_gcm_decrypt(const uint8_t key[16], const uint8_t iv[12],
                            const uint8_t* aad, size_t aad_len,
                            const uint8_t* ciphertext, size_t ct_len,
                            const uint8_t tag[16], uint8_t* plaintext);

#endif /* _SELD_AES128_GCM_H_ */
