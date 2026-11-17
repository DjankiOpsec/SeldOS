/*
 * SeldOS - Humboldt Kernel Project
 * SeldTLS - SHA-384 / SHA-512 (FIPS 180-4) Interface
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#ifndef _SELD_SHA512_H_
#define _SELD_SHA512_H_

#include <stdint.h>
#include <stddef.h>

#define SHA384_DIGEST_SIZE 48
#define SHA512_DIGEST_SIZE 64

struct seld_sha512_ctx {
    uint64_t state[8];
    uint64_t bitlen_lo;
    uint64_t bitlen_hi;
    size_t   datalen;
    uint8_t  data[128];
    int      is_384;
};

void seld_sha512_init(struct seld_sha512_ctx* ctx);
void seld_sha384_init(struct seld_sha512_ctx* ctx);
void seld_sha512_update(struct seld_sha512_ctx* ctx, const void* data, size_t len);
void seld_sha512_final(struct seld_sha512_ctx* ctx, uint8_t* out);

void seld_sha384_hash(const void* data, size_t len, uint8_t out[SHA384_DIGEST_SIZE]);
void seld_sha512_hash(const void* data, size_t len, uint8_t out[SHA512_DIGEST_SIZE]);

#endif /* _SELD_SHA512_H_ */
