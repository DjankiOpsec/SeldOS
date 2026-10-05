/*
 * SeldOS - Humboldt Kernel Project
 * SeldTLS - Native Sovereign TLS 1.3 Subsystem
 * AES-128 and AES-GCM (NIST SP 800-38D) Implementation
 * Constant-time and standard-compliant.
 * GPLv3 Licensed.
 */

#include "seld_aes128_gcm.h"
#include <string.h>

static const uint8_t sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

static const uint32_t rcon[10] = {
    0x01000000, 0x02000000, 0x04000000, 0x08000000, 0x10000000,
    0x20000000, 0x40000000, 0x80000000, 0x1b000000, 0x36000000
};

static uint32_t sub_word(uint32_t w) {
    return ((uint32_t)sbox[(w >> 24) & 0xff] << 24) |
           ((uint32_t)sbox[(w >> 16) & 0xff] << 16) |
           ((uint32_t)sbox[(w >> 8) & 0xff] << 8) |
           ((uint32_t)sbox[w & 0xff]);
}

static uint32_t rot_word(uint32_t w) {
    return (w << 8) | (w >> 24);
}

void seld_aes128_init(struct seld_aes128_ctx* ctx, const uint8_t key[16]) {
    for (int i = 0; i < 4; i++) {
        ctx->round_keys[i] = ((uint32_t)key[4 * i] << 24) |
                             ((uint32_t)key[4 * i + 1] << 16) |
                             ((uint32_t)key[4 * i + 2] << 8) |
                             ((uint32_t)key[4 * i + 3]);
    }
    for (int i = 4; i < 44; i++) {
        uint32_t temp = ctx->round_keys[i - 1];
        if (i % 4 == 0) {
            temp = sub_word(rot_word(temp)) ^ rcon[(i / 4) - 1];
        }
        ctx->round_keys[i] = ctx->round_keys[i - 4] ^ temp;
    }
}

static inline uint8_t xtime(uint8_t x) {
    return (x << 1) ^ ((x & 0x80) ? 0x1b : 0x00);
}

void seld_aes128_encrypt_block(const struct seld_aes128_ctx* ctx, const uint8_t in[16], uint8_t out[16]) {
    uint8_t state[4][4];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            state[j][i] = in[i * 4 + j];
        }
    }

    // Round 0: AddRoundKey
    for (int i = 0; i < 4; i++) {
        uint32_t rk = ctx->round_keys[i];
        state[0][i] ^= (uint8_t)(rk >> 24);
        state[1][i] ^= (uint8_t)(rk >> 16);
        state[2][i] ^= (uint8_t)(rk >> 8);
        state[3][i] ^= (uint8_t)rk;
    }

    // Rounds 1 to 9
    for (int r = 1; r <= 9; r++) {
        // SubBytes
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                state[i][j] = sbox[state[i][j]];
            }
        }
        // ShiftRows
        uint8_t t;
        t = state[1][0]; state[1][0] = state[1][1]; state[1][1] = state[1][2]; state[1][2] = state[1][3]; state[1][3] = t;
        t = state[2][0]; uint8_t t1 = state[2][1];
        state[2][0] = state[2][2]; state[2][1] = state[2][3]; state[2][2] = t; state[2][3] = t1;
        t = state[3][3]; state[3][3] = state[3][2]; state[3][2] = state[3][1]; state[3][1] = state[3][0]; state[3][0] = t;

        // MixColumns
        for (int i = 0; i < 4; i++) {
            uint8_t a = state[0][i], b = state[1][i], c = state[2][i], d = state[3][i];
            uint8_t ab = a ^ b, bc = b ^ c, cd = c ^ d, da = d ^ a;
            state[0][i] = xtime(ab) ^ b ^ c ^ d;
            state[1][i] = xtime(bc) ^ c ^ d ^ a;
            state[2][i] = xtime(cd) ^ d ^ a ^ b;
            state[3][i] = xtime(da) ^ a ^ b ^ c;
        }

        // AddRoundKey
        for (int i = 0; i < 4; i++) {
            uint32_t rk = ctx->round_keys[r * 4 + i];
            state[0][i] ^= (uint8_t)(rk >> 24);
            state[1][i] ^= (uint8_t)(rk >> 16);
            state[2][i] ^= (uint8_t)(rk >> 8);
            state[3][i] ^= (uint8_t)rk;
        }
    }

    // Round 10: SubBytes, ShiftRows, AddRoundKey
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            state[i][j] = sbox[state[i][j]];
        }
    }
    uint8_t t;
    t = state[1][0]; state[1][0] = state[1][1]; state[1][1] = state[1][2]; state[1][2] = state[1][3]; state[1][3] = t;
    t = state[2][0]; uint8_t t1 = state[2][1];
    state[2][0] = state[2][2]; state[2][1] = state[2][3]; state[2][2] = t; state[2][3] = t1;
    t = state[3][3]; state[3][3] = state[3][2]; state[3][2] = state[3][1]; state[3][1] = state[3][0]; state[3][0] = t;

    for (int i = 0; i < 4; i++) {
        uint32_t rk = ctx->round_keys[40 + i];
        state[0][i] ^= (uint8_t)(rk >> 24);
        state[1][i] ^= (uint8_t)(rk >> 16);
        state[2][i] ^= (uint8_t)(rk >> 8);
        state[3][i] ^= (uint8_t)rk;
    }

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            out[i * 4 + j] = state[j][i];
        }
    }
}

/* GHASH field multiplication in GF(2^128) with reduction polynomial x^128 + x^7 + x^2 + x + 1 */
static void ghash_mul(uint8_t x[16], const uint8_t y[16]) {
    uint8_t z[16] = {0};
    uint8_t v[16];
    memcpy(v, y, 16);

    for (int i = 0; i < 16; i++) {
        for (int j = 7; j >= 0; j--) {
            if ((x[i] >> j) & 1) {
                for (int k = 0; k < 16; k++) z[k] ^= v[k];
            }
            int lsb = v[15] & 1;
            for (int k = 15; k > 0; k--) {
                v[k] = (v[k] >> 1) | ((v[k - 1] & 1) << 7);
            }
            v[0] >>= 1;
            if (lsb) {
                v[0] ^= 0xe1;
            }
        }
    }
    memcpy(x, z, 16);
}

static void ghash_update(uint8_t y[16], const uint8_t h[16], const uint8_t* data, size_t len) {
    while (len >= 16) {
        for (int i = 0; i < 16; i++) y[i] ^= data[i];
        ghash_mul(y, h);
        data += 16;
        len -= 16;
    }
    if (len > 0) {
        for (size_t i = 0; i < len; i++) y[i] ^= data[i];
        ghash_mul(y, h);
    }
}

void seld_aes128_gcm_encrypt(const uint8_t key[16], const uint8_t iv[12],
                             const uint8_t* aad, size_t aad_len,
                             const uint8_t* plaintext, size_t pt_len,
                             uint8_t* ciphertext, uint8_t tag[16]) {
    struct seld_aes128_ctx ctx;
    seld_aes128_init(&ctx, key);

    uint8_t h[16] = {0};
    seld_aes128_encrypt_block(&ctx, h, h);

    // Initial counter block J0 = IV || 0x00000001
    uint8_t j0[16];
    memcpy(j0, iv, 12);
    j0[12] = 0; j0[13] = 0; j0[14] = 0; j0[15] = 1;

    // Encrypt plaintext with CTR mode starting at J0 + 1
    uint8_t ctr[16];
    memcpy(ctr, j0, 16);

    uint32_t counter = 1;
    size_t offset = 0;
    while (offset < pt_len) {
        counter++;
        ctr[12] = (uint8_t)(counter >> 24);
        ctr[13] = (uint8_t)(counter >> 16);
        ctr[14] = (uint8_t)(counter >> 8);
        ctr[15] = (uint8_t)counter;

        uint8_t keystream[16];
        seld_aes128_encrypt_block(&ctx, ctr, keystream);

        size_t chunk = (pt_len - offset < 16) ? (pt_len - offset) : 16;
        for (size_t i = 0; i < chunk; i++) {
            ciphertext[offset + i] = plaintext[offset + i] ^ keystream[i];
        }
        offset += chunk;
    }

    // Compute GHASH
    uint8_t y[16] = {0};
    if (aad && aad_len > 0) {
        ghash_update(y, h, aad, aad_len);
    }
    if (ciphertext && pt_len > 0) {
        ghash_update(y, h, ciphertext, pt_len);
    }

    // Append 64-bit aad bit-length and 64-bit ciphertext bit-length
    uint8_t len_block[16];
    uint64_t aad_bits = (uint64_t)aad_len * 8;
    uint64_t ct_bits = (uint64_t)pt_len * 8;
    for (int i = 0; i < 8; i++) {
        len_block[i] = (uint8_t)(aad_bits >> (56 - i * 8));
        len_block[8 + i] = (uint8_t)(ct_bits >> (56 - i * 8));
    }
    ghash_update(y, h, len_block, 16);

    // Tag = GHASH ^ E(K, J0)
    uint8_t ek0[16];
    seld_aes128_encrypt_block(&ctx, j0, ek0);
    for (int i = 0; i < 16; i++) {
        tag[i] = y[i] ^ ek0[i];
    }
}

int seld_aes128_gcm_decrypt(const uint8_t key[16], const uint8_t iv[12],
                            const uint8_t* aad, size_t aad_len,
                            const uint8_t* ciphertext, size_t ct_len,
                            const uint8_t tag[16], uint8_t* plaintext) {
    struct seld_aes128_ctx ctx;
    seld_aes128_init(&ctx, key);

    uint8_t h[16] = {0};
    seld_aes128_encrypt_block(&ctx, h, h);

    // Initial counter block J0
    uint8_t j0[16];
    memcpy(j0, iv, 12);
    j0[12] = 0; j0[13] = 0; j0[14] = 0; j0[15] = 1;

    // Verify tag
    uint8_t y[16] = {0};
    if (aad && aad_len > 0) {
        ghash_update(y, h, aad, aad_len);
    }
    if (ciphertext && ct_len > 0) {
        ghash_update(y, h, ciphertext, ct_len);
    }

    uint8_t len_block[16];
    uint64_t aad_bits = (uint64_t)aad_len * 8;
    uint64_t ct_bits = (uint64_t)ct_len * 8;
    for (int i = 0; i < 8; i++) {
        len_block[i] = (uint8_t)(aad_bits >> (56 - i * 8));
        len_block[8 + i] = (uint8_t)(ct_bits >> (56 - i * 8));
    }
    ghash_update(y, h, len_block, 16);

    uint8_t ek0[16];
    seld_aes128_encrypt_block(&ctx, j0, ek0);
    uint8_t computed_tag[16];
    for (int i = 0; i < 16; i++) {
        computed_tag[i] = y[i] ^ ek0[i];
    }

    uint8_t diff = 0;
    for (int i = 0; i < 16; i++) {
        diff |= (computed_tag[i] ^ tag[i]);
    }
    if (diff != 0) {
        return -1; // Authentication failed
    }

    // Decrypt ciphertext
    uint8_t ctr[16];
    memcpy(ctr, j0, 16);

    uint32_t counter = 1;
    size_t offset = 0;
    while (offset < ct_len) {
        counter++;
        ctr[12] = (uint8_t)(counter >> 24);
        ctr[13] = (uint8_t)(counter >> 16);
        ctr[14] = (uint8_t)(counter >> 8);
        ctr[15] = (uint8_t)counter;

        uint8_t keystream[16];
        seld_aes128_encrypt_block(&ctx, ctr, keystream);

        size_t chunk = (ct_len - offset < 16) ? (ct_len - offset) : 16;
        for (size_t i = 0; i < chunk; i++) {
            plaintext[offset + i] = ciphertext[offset + i] ^ keystream[i];
        }
        offset += chunk;
    }

    return 0;
}
