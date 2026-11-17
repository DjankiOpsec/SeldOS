/*
 * SeldOS - Humboldt Kernel Project
 * SeldTLS - SHA-384 / SHA-512 (FIPS 180-4) Implementation
 * Pure C Freestanding Engine.
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#include "seld_sha512.h"
#include <string.h>

static void explicit_zero(void* ptr, size_t len) {
    volatile uint8_t* p = (volatile uint8_t*)ptr;
    while (len--) {
        *p++ = 0;
    }
}

static inline uint64_t rotr64(uint64_t x, int n) {
    return (x >> n) | (x << (64 - n));
}

static const uint64_t K512[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

static void sha512_transform(struct seld_sha512_ctx* ctx, const uint8_t block[128]) {
    uint64_t w[80];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint64_t)block[i * 8]     << 56) |
               ((uint64_t)block[i * 8 + 1] << 48) |
               ((uint64_t)block[i * 8 + 2] << 40) |
               ((uint64_t)block[i * 8 + 3] << 32) |
               ((uint64_t)block[i * 8 + 4] << 24) |
               ((uint64_t)block[i * 8 + 5] << 16) |
               ((uint64_t)block[i * 8 + 6] << 8)  |
               ((uint64_t)block[i * 8 + 7]);
    }
    for (int i = 16; i < 80; i++) {
        uint64_t s0 = rotr64(w[i - 15], 1) ^ rotr64(w[i - 15], 8) ^ (w[i - 15] >> 7);
        uint64_t s1 = rotr64(w[i - 2], 19) ^ rotr64(w[i - 2], 61) ^ (w[i - 2] >> 6);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint64_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
    uint64_t e = ctx->state[4], f = ctx->state[5], g = ctx->state[6], h = ctx->state[7];

    for (int i = 0; i < 80; i++) {
        uint64_t S1 = rotr64(e, 14) ^ rotr64(e, 18) ^ rotr64(e, 41);
        uint64_t ch = (e & f) ^ ((~e) & g);
        uint64_t temp1 = h + S1 + ch + K512[i] + w[i];
        uint64_t S0 = rotr64(a, 28) ^ rotr64(a, 34) ^ rotr64(a, 39);
        uint64_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint64_t temp2 = S0 + maj;

        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }

    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
    explicit_zero(w, sizeof(w));
}

void seld_sha512_init(struct seld_sha512_ctx* ctx) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->is_384 = 0;
    ctx->state[0] = 0x6a09e667f3bcc908ULL;
    ctx->state[1] = 0xbb67ae8584caa73bULL;
    ctx->state[2] = 0x3c6ef372fe94f82bULL;
    ctx->state[3] = 0xa54ff53a5f1d36f1ULL;
    ctx->state[4] = 0x510e527fade682d1ULL;
    ctx->state[5] = 0x9b05688c2b3e6c1fULL;
    ctx->state[6] = 0x1f83d9abfb41bd6bULL;
    ctx->state[7] = 0x5be0cd19137e2179ULL;
}

void seld_sha384_init(struct seld_sha512_ctx* ctx) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->is_384 = 1;
    ctx->state[0] = 0xcbbb9d5dc1059ed8ULL;
    ctx->state[1] = 0x629a292a367cd507ULL;
    ctx->state[2] = 0x9159015a3070dd17ULL;
    ctx->state[3] = 0x152fecd8f70e5939ULL;
    ctx->state[4] = 0x67332667ffc00b31ULL;
    ctx->state[5] = 0x8eb44a8768581511ULL;
    ctx->state[6] = 0xdb0c2e0d64f98fa7ULL;
    ctx->state[7] = 0x47b5481dbefa4fa4ULL;
}

void seld_sha512_update(struct seld_sha512_ctx* ctx, const void* data, size_t len) {
    if (!ctx || (!data && len > 0)) return;
    const uint8_t* p = (const uint8_t*)data;
    for (size_t i = 0; i < len; i++) {
        ctx->data[ctx->datalen++] = p[i];
        if (ctx->datalen == 128) {
            sha512_transform(ctx, ctx->data);
            uint64_t prev = ctx->bitlen_lo;
            ctx->bitlen_lo += 1024;
            if (ctx->bitlen_lo < prev) ctx->bitlen_hi++;
            ctx->datalen = 0;
        }
    }
}

void seld_sha512_final(struct seld_sha512_ctx* ctx, uint8_t* out) {
    uint64_t add_bits = (uint64_t)ctx->datalen * 8;
    uint64_t prev = ctx->bitlen_lo;
    ctx->bitlen_lo += add_bits;
    if (ctx->bitlen_lo < prev) ctx->bitlen_hi++;

    size_t i = ctx->datalen;
    ctx->data[i++] = 0x80;
    if (i > 112) {
        while (i < 128) ctx->data[i++] = 0x00;
        sha512_transform(ctx, ctx->data);
        i = 0;
    }
    while (i < 112) ctx->data[i++] = 0x00;

    for (int b = 0; b < 8; b++) {
        ctx->data[112 + b] = (uint8_t)(ctx->bitlen_hi >> (56 - b * 8));
        ctx->data[120 + b] = (uint8_t)(ctx->bitlen_lo >> (56 - b * 8));
    }
    sha512_transform(ctx, ctx->data);

    int words = ctx->is_384 ? 6 : 8;
    for (int w = 0; w < words; w++) {
        for (int b = 0; b < 8; b++) {
            out[w * 8 + b] = (uint8_t)(ctx->state[w] >> (56 - b * 8));
        }
    }
    explicit_zero(ctx, sizeof(*ctx));
}

void seld_sha384_hash(const void* data, size_t len, uint8_t out[SHA384_DIGEST_SIZE]) {
    struct seld_sha512_ctx ctx;
    seld_sha384_init(&ctx);
    seld_sha512_update(&ctx, data, len);
    seld_sha512_final(&ctx, out);
}

void seld_sha512_hash(const void* data, size_t len, uint8_t out[SHA512_DIGEST_SIZE]) {
    struct seld_sha512_ctx ctx;
    seld_sha512_init(&ctx);
    seld_sha512_update(&ctx, data, len);
    seld_sha512_final(&ctx, out);
}
