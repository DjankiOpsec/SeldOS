/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * In-Kernel AES-128 and Sector Encryption Engine
 * Constant-time AES-128 implementation for Full Disk Encryption (FDE) and CTR mode.
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#ifndef SELD_AES_H
#define SELD_AES_H

#include <stdint.h>
#include <stddef.h>

#define AES128_BLOCK_SIZE 16
#define AES128_KEY_SIZE   16
#define AES128_ROUNDS     10

struct aes128_ctx {
    uint32_t round_keys[44];
};

/* Core AES-128 Primitive API */
void aes128_init(struct aes128_ctx* ctx, const uint8_t key[16]);
void aes128_encrypt_block(const struct aes128_ctx* ctx, const uint8_t in[16], uint8_t out[16]);
void aes128_wipe_ctx(struct aes128_ctx* ctx);

/* High-Level Stream and Sector Encryption Modes */
void aes128_ctr_crypt(const uint8_t key[16], uint64_t nonce, uint32_t counter_start,
                      const uint8_t* in, uint8_t* out, size_t len);

/* Sector-level LBA-tweaked encryption (512 bytes per sector) */
void aes128_crypt_sector(const uint8_t key[16], uint32_t lba,
                         const uint8_t in[512], uint8_t out[512]);

#endif /* SELD_AES_H */
