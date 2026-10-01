/*
 * SeldOS - Humboldt Kernel Project
 * SNL C Standard Library (libsnl)
 * SHA-256 Cryptographic Hash Header
 * GPLv3 Licensed.
 */

#ifndef _SNL_SHA256_H_
#define _SNL_SHA256_H_

#include <stdint.h>
#include <stddef.h>

#define SHA256_BLOCK_SIZE 64
#define SHA256_DIGEST_SIZE 32

struct sha256_ctx {
    uint32_t state[8];
    uint64_t count;
    uint8_t  buffer[SHA256_BLOCK_SIZE];
};

void sha256_init(struct sha256_ctx* ctx);
void sha256_update(struct sha256_ctx* ctx, const void* data, size_t len);
void sha256_final(struct sha256_ctx* ctx, uint8_t digest[SHA256_DIGEST_SIZE]);
void sha256_hash(const void* data, size_t len, uint8_t digest[SHA256_DIGEST_SIZE]);

#endif /* _SNL_SHA256_H_ */
