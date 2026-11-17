/*
 * SeldOS - Humboldt Kernel Project
 * SeldTLS - Cryptographic Verification & X.509 DER Parser Engine
 * Implements:
 *   - Strict ASN.1 DER TLV parser (definite lengths, canonical bounds)
 *   - Multi-precision BigInt modular arithmetic (up to 4096-bit Montgomery)
 *   - RSA PKCS#1 v1.5 & RSA-PSS (SHA-256 / SHA-384 / SHA-512)
 *   - ECDSA secp256r1 (P-256) & secp384r1 (P-384)
 *   - X.509 v1/v3 chain validation against embedded Root CA Trust Store
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#include "seld_crypto_verify.h"
#include "seld_root_cas.h"
#include "sha256.h"
#include "seld_sha512.h"
#include <string.h>
#include <stdlib.h>

void seld_explicit_bzero(void* ptr, size_t len) {
    if (!ptr || len == 0) return;
    volatile uint8_t* p = (volatile uint8_t*)ptr;
    while (len--) {
        *p++ = 0;
    }
}

/* ========================================================================= */
/* Multi-Precision BigInt (up to 4096-bit, 128 x 32-bit limbs + headroom)    */
/* ========================================================================= */

#define BN_MAX_LIMBS 130

typedef struct {
    uint32_t d[BN_MAX_LIMBS];
    size_t   n; /* number of active limbs */
} bn_t;

static void bn_zero(bn_t* a) {
    memset(a->d, 0, sizeof(a->d));
    a->n = 0;
}

static void bn_norm(bn_t* a) {
    while (a->n > 0 && a->d[a->n - 1] == 0) {
        a->n--;
    }
}

static int bn_is_zero(const bn_t* a) {
    for (size_t i = 0; i < a->n; i++) {
        if (a->d[i] != 0) return 0;
    }
    return 1;
}

static int bn_cmp(const bn_t* a, const bn_t* b) {
    size_t na = a->n, nb = b->n;
    while (na > 0 && a->d[na - 1] == 0) na--;
    while (nb > 0 && b->d[nb - 1] == 0) nb--;
    if (na != nb) return (na > nb) ? 1 : -1;
    for (size_t i = na; i > 0; i--) {
        if (a->d[i - 1] != b->d[i - 1]) {
            return (a->d[i - 1] > b->d[i - 1]) ? 1 : -1;
        }
    }
    return 0;
}

static int bn_from_be(bn_t* a, const uint8_t* bytes, size_t len) {
    if (len > (BN_MAX_LIMBS - 2) * 4) return -1;
    bn_zero(a);
    size_t limb_idx = 0;
    uint32_t acc = 0;
    int shift = 0;
    for (size_t i = len; i > 0; i--) {
        acc |= ((uint32_t)bytes[i - 1]) << shift;
        shift += 8;
        if (shift == 32) {
            a->d[limb_idx++] = acc;
            acc = 0;
            shift = 0;
        }
    }
    if (shift > 0) {
        a->d[limb_idx++] = acc;
    }
    a->n = limb_idx;
    bn_norm(a);
    return 0;
}

static void bn_to_be(const bn_t* a, uint8_t* out, size_t len) {
    memset(out, 0, len);
    for (size_t i = 0; i < len; i++) {
        size_t pos = len - 1 - i;
        size_t limb_idx = i / 4;
        size_t bit_shift = (i % 4) * 8;
        if (limb_idx < a->n) {
            out[pos] = (uint8_t)((a->d[limb_idx] >> bit_shift) & 0xFF);
        }
    }
}

static uint32_t bn_add_raw(uint32_t* r, const uint32_t* a, const uint32_t* b, size_t k) {
    uint64_t carry = 0;
    for (size_t i = 0; i < k; i++) {
        uint64_t sum = (uint64_t)a[i] + b[i] + carry;
        r[i] = (uint32_t)sum;
        carry = sum >> 32;
    }
    return (uint32_t)carry;
}

static uint32_t bn_sub_raw(uint32_t* r, const uint32_t* a, const uint32_t* b, size_t k) {
    uint64_t borrow = 0;
    for (size_t i = 0; i < k; i++) {
        uint64_t diff = (uint64_t)a[i] - b[i] - borrow;
        r[i] = (uint32_t)diff;
        borrow = (diff >> 63) & 1;
    }
    return (uint32_t)borrow;
}

static int bn_cmp_raw(const uint32_t* a, const uint32_t* b, size_t k) {
    for (size_t i = k; i > 0; i--) {
        if (a[i - 1] != b[i - 1]) {
            return (a[i - 1] > b[i - 1]) ? 1 : -1;
        }
    }
    return 0;
}

static void bn_mod_add(bn_t* r, const bn_t* a, const bn_t* b, const bn_t* mod, size_t k) {
    uint32_t carry = bn_add_raw(r->d, a->d, b->d, k);
    if (carry || bn_cmp_raw(r->d, mod->d, k) >= 0) {
        bn_sub_raw(r->d, r->d, mod->d, k);
    }
    r->n = k;
    bn_norm(r);
}

static void bn_mod_sub(bn_t* r, const bn_t* a, const bn_t* b, const bn_t* mod, size_t k) {
    if (bn_sub_raw(r->d, a->d, b->d, k)) {
        bn_add_raw(r->d, r->d, mod->d, k);
    }
    r->n = k;
    bn_norm(r);
}

static uint32_t mont_n0_inv(uint32_t n0) {
    uint32_t inv = 1;
    for (int i = 0; i < 5; i++) {
        inv = inv * (2U - n0 * inv);
    }
    return 0U - inv;
}

/* r = a * b * R^(-1) mod mod, where R = 2^(32*k) */
static void bn_mont_mul(bn_t* r, const bn_t* a, const bn_t* b, const bn_t* mod, size_t k, uint32_t n0_inv) {
    uint32_t t[BN_MAX_LIMBS + 2];
    memset(t, 0, sizeof(uint32_t) * (k + 2));

    for (size_t i = 0; i < k; i++) {
        uint64_t carry = 0;
        uint32_t bi = (i < b->n) ? b->d[i] : 0;
        for (size_t j = 0; j < k; j++) {
            uint32_t aj = (j < a->n) ? a->d[j] : 0;
            uint64_t prod = (uint64_t)aj * bi + t[j] + carry;
            t[j] = (uint32_t)prod;
            carry = prod >> 32;
        }
        uint64_t sum = (uint64_t)t[k] + carry;
        t[k] = (uint32_t)sum;
        t[k + 1] = (uint32_t)(sum >> 32);

        uint32_t m = t[0] * n0_inv;
        carry = 0;
        uint64_t p0 = (uint64_t)m * mod->d[0] + t[0];
        carry = p0 >> 32;
        for (size_t j = 1; j < k; j++) {
            uint64_t prod = (uint64_t)m * mod->d[j] + t[j] + carry;
            t[j - 1] = (uint32_t)prod;
            carry = prod >> 32;
        }
        sum = (uint64_t)t[k] + carry;
        t[k - 1] = (uint32_t)sum;
        t[k] = t[k + 1] + (uint32_t)(sum >> 32);
    }

    if (t[k] != 0 || bn_cmp_raw(t, mod->d, k) >= 0) {
        bn_sub_raw(r->d, t, mod->d, k);
    } else {
        memcpy(r->d, t, k * sizeof(uint32_t));
    }
    for (size_t i = k; i < BN_MAX_LIMBS; i++) r->d[i] = 0;
    r->n = k;
    bn_norm(r);
}

/* Compute R^2 mod mod where R = 2^(32*k) */
static void bn_mont_r2(bn_t* r2, const bn_t* mod, size_t k) {
    bn_zero(r2);
    r2->d[0] = 1;
    r2->n = 1;
    size_t bits = 2 * 32 * k;
    for (size_t i = 0; i < bits; i++) {
        uint32_t carry = 0;
        for (size_t j = 0; j < k; j++) {
            uint32_t next_c = r2->d[j] >> 31;
            r2->d[j] = (r2->d[j] << 1) | carry;
            carry = next_c;
        }
        if (carry || bn_cmp_raw(r2->d, mod->d, k) >= 0) {
            bn_sub_raw(r2->d, r2->d, mod->d, k);
        }
    }
    r2->n = k;
    bn_norm(r2);
}

/* out = base^exp mod mod (mod must be odd and >= 3, base < mod) */
static int bn_modexp(bn_t* out, const bn_t* base, const bn_t* exp, const bn_t* mod) {
    if (mod->n == 0 || (mod->d[0] & 1) == 0) return -1;
    if (mod->n == 1 && mod->d[0] < 3) return -1;
    if (bn_cmp(base, mod) >= 0) return -1;

    size_t k = mod->n;
    uint32_t n0_inv = mont_n0_inv(mod->d[0]);

    bn_t r2, one, acc, base_m;
    bn_mont_r2(&r2, mod, k);
    bn_zero(&one);
    one.d[0] = 1;
    one.n = 1;

    bn_mont_mul(&acc, &one, &r2, mod, k, n0_inv);
    bn_mont_mul(&base_m, base, &r2, mod, k, n0_inv);

    /* Find highest set bit of exp */
    size_t total_bits = exp->n * 32;
    while (total_bits > 0) {
        size_t b = total_bits - 1;
        if ((exp->d[b / 32] >> (b % 32)) & 1U) break;
        total_bits--;
    }

    for (size_t b = total_bits; b > 0; b--) {
        bn_t tmp;
        bn_mont_mul(&tmp, &acc, &acc, mod, k, n0_inv);
        acc = tmp;
        size_t bit_idx = b - 1;
        if ((exp->d[bit_idx / 32] >> (bit_idx % 32)) & 1U) {
            bn_mont_mul(&tmp, &acc, &base_m, mod, k, n0_inv);
            acc = tmp;
        }
    }

    bn_mont_mul(out, &acc, &one, mod, k, n0_inv);
    return 0;
}

static size_t bn_bitlen(const bn_t* a) {
    if (a->n == 0) return 0;
    size_t top = a->n - 1;
    uint32_t v = a->d[top];
    size_t bits = top * 32;
    while (v) {
        bits++;
        v >>= 1;
    }
    return bits;
}

/* ========================================================================= */
/* RSA PKCS#1 v1.5 and RSA-PSS Verification                                  */
/* ========================================================================= */

static int hash_by_alg(enum seld_sig_alg alg, const uint8_t* msg, size_t msg_len, uint8_t* out, size_t* out_len) {
    if (alg == SELD_SIG_RSA_PKCS1_SHA256 || alg == SELD_SIG_RSA_PSS_SHA256 || alg == SELD_SIG_ECDSA_P256_SHA256) {
        sha256_hash(msg, msg_len, out);
        *out_len = 32;
        return 0;
    }
    if (alg == SELD_SIG_RSA_PKCS1_SHA384 || alg == SELD_SIG_RSA_PSS_SHA384 || alg == SELD_SIG_ECDSA_P384_SHA384) {
        seld_sha384_hash(msg, msg_len, out);
        *out_len = 48;
        return 0;
    }
    if (alg == SELD_SIG_RSA_PKCS1_SHA512 || alg == SELD_SIG_RSA_PSS_SHA512) {
        seld_sha512_hash(msg, msg_len, out);
        *out_len = 64;
        return 0;
    }
    return -1;
}

static int mgf1(enum seld_sig_alg alg, const uint8_t* seed, size_t seed_len, uint8_t* mask, size_t mask_len) {
    size_t hlen = 32;
    if (alg == SELD_SIG_RSA_PSS_SHA384) hlen = 48;
    else if (alg == SELD_SIG_RSA_PSS_SHA512) hlen = 64;

    uint32_t counter = 0;
    size_t off = 0;
    uint8_t digest[64];
    uint8_t cnt[4];

    while (off < mask_len) {
        cnt[0] = (uint8_t)(counter >> 24);
        cnt[1] = (uint8_t)(counter >> 16);
        cnt[2] = (uint8_t)(counter >> 8);
        cnt[3] = (uint8_t)counter;

        if (hlen == 32) {
            struct sha256_ctx ctx;
            sha256_init(&ctx);
            sha256_update(&ctx, seed, seed_len);
            sha256_update(&ctx, cnt, 4);
            sha256_final(&ctx, digest);
        } else if (hlen == 48) {
            struct seld_sha512_ctx ctx;
            seld_sha384_init(&ctx);
            seld_sha512_update(&ctx, seed, seed_len);
            seld_sha512_update(&ctx, cnt, 4);
            seld_sha512_final(&ctx, digest);
        } else {
            struct seld_sha512_ctx ctx;
            seld_sha512_init(&ctx);
            seld_sha512_update(&ctx, seed, seed_len);
            seld_sha512_update(&ctx, cnt, 4);
            seld_sha512_final(&ctx, digest);
        }

        size_t chunk = (mask_len - off < hlen) ? (mask_len - off) : hlen;
        memcpy(mask + off, digest, chunk);
        off += chunk;
        counter++;
    }
    return 0;
}

static int verify_rsa(const struct seld_pub_key* pk, enum seld_sig_alg alg, int expected_salt_len,
                      const uint8_t* msg, size_t msg_len, const uint8_t* sig, size_t sig_len) {
    if (!pk || pk->type != SELD_PK_RSA) return -1;
    if (pk->rsa_n_len < 128 || pk->rsa_n_len > SELD_MAX_RSA_BYTES) return -1;
    if (sig_len != pk->rsa_n_len) return -1;

    bn_t n, e, s, m;
    if (bn_from_be(&n, pk->rsa_n, pk->rsa_n_len) != 0) return -1;
    if (bn_from_be(&e, pk->rsa_e, pk->rsa_e_len) != 0) return -1;
    if (bn_from_be(&s, sig, sig_len) != 0) return -1;

    if (bn_is_zero(&e) || (e.n == 1 && e.d[0] < 3) || (e.d[0] & 1) == 0) return -1;
    if (bn_cmp(&s, &n) >= 0) return -1;

    if (bn_modexp(&m, &s, &e, &n) != 0) return -1;

    uint8_t em[SELD_MAX_RSA_BYTES];
    size_t k = pk->rsa_n_len;
    bn_to_be(&m, em, k);

    uint8_t mhash[64];
    size_t hlen = 0;
    if (hash_by_alg(alg, msg, msg_len, mhash, &hlen) != 0) return -1;

    if (alg == SELD_SIG_RSA_PKCS1_SHA256 || alg == SELD_SIG_RSA_PKCS1_SHA384 || alg == SELD_SIG_RSA_PKCS1_SHA512) {
        static const uint8_t prefix_256[19] = {0x30,0x31,0x30,0x0d,0x06,0x09,0x60,0x86,0x48,0x01,0x65,0x03,0x04,0x02,0x01,0x05,0x00,0x04,0x20};
        static const uint8_t prefix_384[19] = {0x30,0x41,0x30,0x0d,0x06,0x09,0x60,0x86,0x48,0x01,0x65,0x03,0x04,0x02,0x02,0x05,0x00,0x04,0x30};
        static const uint8_t prefix_512[19] = {0x30,0x51,0x30,0x0d,0x06,0x09,0x60,0x86,0x48,0x01,0x65,0x03,0x04,0x02,0x03,0x05,0x00,0x04,0x40};
        const uint8_t* prefix = (hlen == 32) ? prefix_256 : ((hlen == 48) ? prefix_384 : prefix_512);
        size_t tlen = 19 + hlen;
        if (k < tlen + 11) return -1;
        if (em[0] != 0x00 || em[1] != 0x01) return -1;
        size_t ps_end = k - tlen - 1;
        for (size_t i = 2; i < ps_end; i++) {
            if (em[i] != 0xFF) return -1;
        }
        if (em[ps_end] != 0x00) return -1;
        if (memcmp(em + ps_end + 1, prefix, 19) != 0) return -1;
        if (memcmp(em + ps_end + 1 + 19, mhash, hlen) != 0) return -1;
        return 0;
    }

    /* RSA-PSS (RFC 8017 Section 9.1.2) */
    size_t mod_bits = bn_bitlen(&n);
    if (mod_bits < 1024) return -1;
    size_t em_bits = mod_bits - 1;
    size_t em_len = (em_bits + 7) / 8;
    const uint8_t* pss_em = em;
    if (em_len < k) {
        if (em[0] != 0x00) return -1;
        pss_em = em + (k - em_len);
    }
    if (em_len < hlen + 2) return -1;
    if (pss_em[em_len - 1] != 0xBC) return -1;

    uint8_t top_mask = (uint8_t)(0xFFU >> (8 - (em_bits & 7)));
    if ((em_bits & 7) != 0 && (pss_em[0] & (uint8_t)(~top_mask)) != 0) {
        return -1;
    }

    size_t db_len = em_len - hlen - 1;
    const uint8_t* h_ptr = pss_em + db_len;
    uint8_t db[SELD_MAX_RSA_BYTES];
    mgf1(alg, h_ptr, hlen, db, db_len);
    for (size_t i = 0; i < db_len; i++) {
        db[i] ^= pss_em[i];
    }
    if ((em_bits & 7) != 0) {
        db[0] &= top_mask;
    }

    /* Find 0x01 separator after leading zeroes */
    size_t idx = 0;
    while (idx < db_len && db[idx] == 0x00) idx++;
    if (idx >= db_len || db[idx] != 0x01) return -1;
    size_t salt_len = db_len - idx - 1;
    if (expected_salt_len >= 0 && (int)salt_len != expected_salt_len) return -1;
    const uint8_t* salt = db + idx + 1;

    /* Compute H' = Hash(0x00*8 || mHash || salt) */
    uint8_t m_prime_zeros[8] = {0};
    uint8_t h_prime[64];
    if (hlen == 32) {
        struct sha256_ctx ctx;
        sha256_init(&ctx);
        sha256_update(&ctx, m_prime_zeros, 8);
        sha256_update(&ctx, mhash, 32);
        if (salt_len > 0) sha256_update(&ctx, salt, salt_len);
        sha256_final(&ctx, h_prime);
    } else if (hlen == 48) {
        struct seld_sha512_ctx ctx;
        seld_sha384_init(&ctx);
        seld_sha512_update(&ctx, m_prime_zeros, 8);
        seld_sha512_update(&ctx, mhash, 48);
        if (salt_len > 0) seld_sha512_update(&ctx, salt, salt_len);
        seld_sha512_final(&ctx, h_prime);
    } else {
        struct seld_sha512_ctx ctx;
        seld_sha512_init(&ctx);
        seld_sha512_update(&ctx, m_prime_zeros, 8);
        seld_sha512_update(&ctx, mhash, 64);
        if (salt_len > 0) seld_sha512_update(&ctx, salt, salt_len);
        seld_sha512_final(&ctx, h_prime);
    }

    return (memcmp(h_prime, h_ptr, hlen) == 0) ? 0 : -1;
}

/* ========================================================================= */
/* ECDSA P-256 (secp256r1) and P-384 (secp384r1) Verification                */
/* ========================================================================= */

struct ec_curve_params {
    size_t k; /* limbs: 8 for P-256, 12 for P-384 */
    bn_t p, b, n, gx, gy;
};

static void ec_load_p256(struct ec_curve_params* c) {
    static const uint8_t p_be[32]  = {0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
    static const uint8_t b_be[32]  = {0x5a,0xc6,0x35,0xd8,0xaa,0x3a,0x93,0xe7,0xb3,0xeb,0xbd,0x55,0x76,0x98,0x86,0xbc,0x65,0x1d,0x06,0xb0,0xcc,0x53,0xb0,0xf6,0x3b,0xce,0x3c,0x3e,0x27,0xd2,0x60,0x4b};
    static const uint8_t n_be[32]  = {0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xbc,0xe6,0xfa,0xad,0xa7,0x17,0x9e,0x84,0xf3,0xb9,0xca,0xc2,0xfc,0x63,0x25,0x51};
    static const uint8_t gx_be[32] = {0x6b,0x17,0xd1,0xf2,0xe1,0x2c,0x42,0x47,0xf8,0xbc,0xe6,0xe5,0x63,0xa4,0x40,0xf2,0x77,0x03,0x7d,0x81,0x2d,0xeb,0x33,0xa0,0xf4,0xa1,0x39,0x45,0xd8,0x98,0xc2,0x96};
    static const uint8_t gy_be[32] = {0x4f,0xe3,0x42,0xe2,0xfe,0x1a,0x7f,0x9b,0x8e,0xe7,0xeb,0x4a,0x7c,0x0f,0x9e,0x16,0x2b,0xce,0x33,0x57,0x6b,0x31,0x5e,0xce,0xcb,0xb6,0x40,0x68,0x37,0xbf,0x51,0xf5};
    c->k = 8;
    bn_from_be(&c->p, p_be, 32);
    bn_from_be(&c->b, b_be, 32);
    bn_from_be(&c->n, n_be, 32);
    bn_from_be(&c->gx, gx_be, 32);
    bn_from_be(&c->gy, gy_be, 32);
}

static void ec_load_p384(struct ec_curve_params* c) {
    static const uint8_t p_be[48]  = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff};
    static const uint8_t b_be[48]  = {0xb3,0x31,0x2f,0xa7,0xe2,0x3e,0xe7,0xe4,0x98,0x8e,0x05,0x6b,0xe3,0xf8,0x2d,0x19,0x18,0x1d,0x9c,0x6e,0xfe,0x81,0x41,0x12,0x03,0x14,0x08,0x8f,0x50,0x13,0x87,0x5a,0xc6,0x56,0x39,0x8d,0x8a,0x2e,0xd1,0x9d,0x2a,0x85,0xc8,0xed,0xd3,0xec,0x2a,0xef};
    static const uint8_t n_be[48]  = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xc7,0x63,0x4d,0x81,0xf4,0x37,0x2d,0xdf,0x58,0x1a,0x0d,0xb2,0x48,0xb0,0xa7,0x7a,0xec,0xec,0x19,0x6a,0xcc,0xc5,0x29,0x73};
    static const uint8_t gx_be[48] = {0xaa,0x87,0xca,0x22,0xbe,0x8b,0x05,0x37,0x8e,0xb1,0xc7,0x1e,0xf3,0x20,0xad,0x74,0x6e,0x1d,0x3b,0x62,0x8b,0xa7,0x9b,0x98,0x59,0xf7,0x41,0xe0,0x82,0x54,0x2a,0x38,0x55,0x02,0xf2,0x5d,0xbf,0x55,0x29,0x6c,0x3a,0x54,0x5e,0x38,0x72,0x76,0x0a,0xb7};
    static const uint8_t gy_be[48] = {0x36,0x17,0xde,0x4a,0x96,0x26,0x2c,0x6f,0x5d,0x9e,0x98,0xbf,0x92,0x92,0xdc,0x29,0xf8,0xf4,0x1d,0xbd,0x28,0x9a,0x14,0x7c,0xe9,0xda,0x31,0x13,0xb5,0xf0,0xb8,0xc0,0x0a,0x60,0xb1,0xce,0x1d,0x7e,0x81,0x9d,0x7a,0x43,0x1d,0x7c,0x90,0xea,0x0e,0x5f};
    c->k = 12;
    bn_from_be(&c->p, p_be, 48);
    bn_from_be(&c->b, b_be, 48);
    bn_from_be(&c->n, n_be, 48);
    bn_from_be(&c->gx, gx_be, 48);
    bn_from_be(&c->gy, gy_be, 48);
}

struct ec_jac {
    bn_t x, y, z;
    int  inf;
};

/* Montgomery field context for fast curve arithmetic */
struct ec_mont {
    const bn_t* p;
    size_t k;
    uint32_t n0_inv;
    bn_t r2;
    bn_t one;
};

static void ec_mont_init(struct ec_mont* m, const bn_t* p, size_t k) {
    m->p = p;
    m->k = k;
    m->n0_inv = mont_n0_inv(p->d[0]);
    bn_mont_r2(&m->r2, p, k);
    bn_zero(&m->one);
    m->one.d[0] = 1;
    m->one.n = 1;
}

static void ec_to_mont(const struct ec_mont* m, bn_t* r, const bn_t* a) {
    bn_mont_mul(r, a, &m->r2, m->p, m->k, m->n0_inv);
}

static void ec_from_mont(const struct ec_mont* m, bn_t* r, const bn_t* a) {
    bn_mont_mul(r, a, &m->one, m->p, m->k, m->n0_inv);
}

static void ec_mul_m(const struct ec_mont* m, bn_t* r, const bn_t* a, const bn_t* b) {
    bn_mont_mul(r, a, b, m->p, m->k, m->n0_inv);
}

static void ec_sqr_m(const struct ec_mont* m, bn_t* r, const bn_t* a) {
    bn_mont_mul(r, a, a, m->p, m->k, m->n0_inv);
}

static void ec_add_m(const struct ec_mont* m, bn_t* r, const bn_t* a, const bn_t* b) {
    bn_mod_add(r, a, b, m->p, m->k);
}

static void ec_sub_m(const struct ec_mont* m, bn_t* r, const bn_t* a, const bn_t* b) {
    bn_mod_sub(r, a, b, m->p, m->k);
}

/* Jacobian point doubling for a = -3 (works for both P-256 and P-384) */
static void ec_jac_double(const struct ec_mont* m, struct ec_jac* r, const struct ec_jac* p) {
    if (p->inf || bn_is_zero(&p->y)) {
        r->inf = 1;
        return;
    }
    bn_t delta, gamma, beta, alpha, t1, t2, x3, y3, z3;
    ec_sqr_m(m, &delta, &p->z);               /* delta = Z^2 */
    ec_sqr_m(m, &gamma, &p->y);               /* gamma = Y^2 */
    ec_mul_m(m, &beta, &p->x, &gamma);        /* beta = X * gamma */

    ec_sub_m(m, &t1, &p->x, &delta);
    ec_add_m(m, &t2, &p->x, &delta);
    ec_mul_m(m, &alpha, &t1, &t2);            /* alpha = X^2 - Z^4 */
    ec_add_m(m, &t1, &alpha, &alpha);
    ec_add_m(m, &alpha, &t1, &alpha);         /* alpha = 3 * (X^2 - Z^4) */

    ec_sqr_m(m, &x3, &alpha);
    ec_add_m(m, &t1, &beta, &beta);           /* 2*beta */
    ec_add_m(m, &t1, &t1, &t1);               /* 4*beta */
    ec_add_m(m, &t2, &t1, &t1);               /* 8*beta */
    ec_sub_m(m, &x3, &x3, &t2);               /* X3 = alpha^2 - 8*beta */

    ec_add_m(m, &z3, &p->y, &p->z);
    ec_sqr_m(m, &z3, &z3);
    ec_sub_m(m, &z3, &z3, &gamma);
    ec_sub_m(m, &z3, &z3, &delta);            /* Z3 = (Y+Z)^2 - gamma - delta */

    ec_sub_m(m, &t1, &t1, &x3);               /* 4*beta - X3 */
    ec_mul_m(m, &t1, &alpha, &t1);
    ec_sqr_m(m, &gamma, &gamma);              /* gamma^2 */
    ec_add_m(m, &t2, &gamma, &gamma);
    ec_add_m(m, &t2, &t2, &t2);
    ec_add_m(m, &t2, &t2, &t2);               /* 8 * gamma^2 */
    ec_sub_m(m, &y3, &t1, &t2);               /* Y3 = alpha*(4*beta - X3) - 8*gamma^2 */

    r->x = x3;
    r->y = y3;
    r->z = z3;
    r->inf = 0;
}

static void ec_jac_add(const struct ec_mont* m, struct ec_jac* r, const struct ec_jac* p, const struct ec_jac* q) {
    if (p->inf) { *r = *q; return; }
    if (q->inf) { *r = *p; return; }

    bn_t z1z1, z2z2, u1, u2, s1, s2, h, i, j, r_val, v, t1, x3, y3, z3;
    ec_sqr_m(m, &z1z1, &p->z);
    ec_sqr_m(m, &z2z2, &q->z);
    ec_mul_m(m, &u1, &p->x, &z2z2);
    ec_mul_m(m, &u2, &q->x, &z1z1);
    ec_mul_m(m, &t1, &q->z, &z2z2);
    ec_mul_m(m, &s1, &p->y, &t1);
    ec_mul_m(m, &t1, &p->z, &z1z1);
    ec_mul_m(m, &s2, &q->y, &t1);

    if (bn_cmp(&u1, &u2) == 0) {
        if (bn_cmp(&s1, &s2) == 0) {
            ec_jac_double(m, r, p);
            return;
        }
        r->inf = 1;
        return;
    }

    ec_sub_m(m, &h, &u2, &u1);
    ec_add_m(m, &i, &h, &h);
    ec_sqr_m(m, &i, &i);                      /* I = (2*H)^2 */
    ec_mul_m(m, &j, &h, &i);                  /* J = H * I */
    ec_sub_m(m, &r_val, &s2, &s1);
    ec_add_m(m, &r_val, &r_val, &r_val);      /* r = 2*(S2 - S1) */
    ec_mul_m(m, &v, &u1, &i);                 /* V = U1 * I */

    ec_sqr_m(m, &x3, &r_val);
    ec_sub_m(m, &x3, &x3, &j);
    ec_add_m(m, &t1, &v, &v);
    ec_sub_m(m, &x3, &x3, &t1);               /* X3 = r^2 - J - 2*V */

    ec_sub_m(m, &t1, &v, &x3);
    ec_mul_m(m, &y3, &r_val, &t1);
    ec_mul_m(m, &t1, &s1, &j);
    ec_add_m(m, &t1, &t1, &t1);
    ec_sub_m(m, &y3, &y3, &t1);               /* Y3 = r*(V - X3) - 2*S1*J */

    ec_add_m(m, &z3, &p->z, &q->z);
    ec_sqr_m(m, &z3, &z3);
    ec_sub_m(m, &z3, &z3, &z1z1);
    ec_sub_m(m, &z3, &z3, &z2z2);
    ec_mul_m(m, &z3, &z3, &h);                /* Z3 = ((Z1+Z2)^2 - Z1Z1 - Z2Z2)*H */

    r->x = x3;
    r->y = y3;
    r->z = z3;
    r->inf = 0;
}

/* Strict ASN.1 DER TLV Reader */
static int der_read_tlv(const uint8_t** p, const uint8_t* end, uint8_t* tag_out, const uint8_t** val_out, size_t* len_out) {
    if (!p || !*p || *p >= end) return -1;
    const uint8_t* cur = *p;
    uint8_t tag = *cur++;
    if ((tag & 0x1F) == 0x1F) return -1; /* High-tag-number form not expected in X.509/ECDSA */
    if (cur >= end) return -1;

    uint8_t lb = *cur++;
    size_t len = 0;
    if ((lb & 0x80) == 0) {
        len = lb;
    } else {
        int nbytes = lb & 0x7F;
        if (nbytes == 0 || nbytes > 4) return -1; /* Indefinite or >4GB rejected */
        if (cur + nbytes > end) return -1;
        if (cur[0] == 0x00) return -1; /* Non-minimal DER length */
        for (int i = 0; i < nbytes; i++) {
            len = (len << 8) | cur[i];
        }
        cur += nbytes;
        if (len < 0x80) return -1; /* Non-minimal DER length */
    }

    if (len > (size_t)(end - cur)) return -1;
    *tag_out = tag;
    *val_out = cur;
    *len_out = len;
    *p = cur + len;
    return 0;
}

static int der_parse_uint_bytes(const uint8_t* val, size_t len, const uint8_t** out_bytes, size_t* out_len) {
    if (len == 0) return -1;
    if (val[0] & 0x80) return -1; /* Negative INTEGER rejected */
    if (len > 1 && val[0] == 0x00 && !(val[1] & 0x80)) return -1; /* Non-minimal DER integer */
    while (len > 1 && val[0] == 0x00) {
        val++;
        len--;
    }
    *out_bytes = val;
    *out_len = len;
    return 0;
}

static int verify_ecdsa(const struct seld_pub_key* pk, enum seld_sig_alg alg,
                        const uint8_t* msg, size_t msg_len, const uint8_t* sig, size_t sig_len) {
    struct ec_curve_params curve;
    size_t coord_bytes = 0;
    if (pk->type == SELD_PK_EC_P256 && alg == SELD_SIG_ECDSA_P256_SHA256) {
        ec_load_p256(&curve);
        coord_bytes = 32;
    } else if (pk->type == SELD_PK_EC_P384 && alg == SELD_SIG_ECDSA_P384_SHA384) {
        ec_load_p384(&curve);
        coord_bytes = 48;
    } else {
        return -1;
    }

    if (pk->ec_len != 1 + 2 * coord_bytes || pk->ec_point[0] != 0x04) return -1;

    bn_t qx, qy;
    if (bn_from_be(&qx, pk->ec_point + 1, coord_bytes) != 0) return -1;
    if (bn_from_be(&qy, pk->ec_point + 1 + coord_bytes, coord_bytes) != 0) return -1;
    if (bn_cmp(&qx, &curve.p) >= 0 || bn_cmp(&qy, &curve.p) >= 0) return -1;

    /* Validate public point is on curve: Y^2 == X^3 - 3X + b (mod p) */
    struct ec_mont mp;
    ec_mont_init(&mp, &curve.p, curve.k);
    bn_t qx_m, qy_m, lhs, rhs, three_x, b_m;
    ec_to_mont(&mp, &qx_m, &qx);
    ec_to_mont(&mp, &qy_m, &qy);
    ec_to_mont(&mp, &b_m, &curve.b);
    ec_sqr_m(&mp, &lhs, &qy_m);
    ec_sqr_m(&mp, &rhs, &qx_m);
    ec_mul_m(&mp, &rhs, &rhs, &qx_m);
    ec_add_m(&mp, &three_x, &qx_m, &qx_m);
    ec_add_m(&mp, &three_x, &three_x, &qx_m);
    ec_sub_m(&mp, &rhs, &rhs, &three_x);
    ec_add_m(&mp, &rhs, &rhs, &b_m);
    if (bn_cmp(&lhs, &rhs) != 0) return -1;

    /* Parse DER Ecdsa-Sig-Value */
    const uint8_t* p = sig;
    const uint8_t* end = sig + sig_len;
    uint8_t tag;
    const uint8_t* seq_v;
    size_t seq_l;
    if (der_read_tlv(&p, end, &tag, &seq_v, &seq_l) != 0 || tag != 0x30 || p != end) return -1;

    const uint8_t* sp = seq_v;
    const uint8_t* send = seq_v + seq_l;
    const uint8_t *rv, *sv, *rb, *sb;
    size_t rl, sl, rbl, sbl;
    if (der_read_tlv(&sp, send, &tag, &rv, &rl) != 0 || tag != 0x02) return -1;
    if (der_read_tlv(&sp, send, &tag, &sv, &sl) != 0 || tag != 0x02 || sp != send) return -1;
    if (der_parse_uint_bytes(rv, rl, &rb, &rbl) != 0) return -1;
    if (der_parse_uint_bytes(sv, sl, &sb, &sbl) != 0) return -1;

    bn_t r_bn, s_bn;
    if (bn_from_be(&r_bn, rb, rbl) != 0 || bn_from_be(&s_bn, sb, sbl) != 0) return -1;
    if (bn_is_zero(&r_bn) || bn_cmp(&r_bn, &curve.n) >= 0) return -1;
    if (bn_is_zero(&s_bn) || bn_cmp(&s_bn, &curve.n) >= 0) return -1;

    /* Hash message */
    uint8_t digest[64];
    size_t hlen = 0;
    if (hash_by_alg(alg, msg, msg_len, digest, &hlen) != 0) return -1;
    bn_t z;
    if (bn_from_be(&z, digest, coord_bytes) != 0) return -1;
    if (bn_cmp(&z, &curve.n) >= 0) {
        bn_sub_raw(z.d, z.d, curve.n.d, curve.k);
        bn_norm(&z);
    }

    /* Compute w = s^(-1) mod n = s^(n-2) mod n */
    bn_t n_minus_2 = curve.n;
    bn_t two; bn_zero(&two); two.d[0] = 2; two.n = 1;
    bn_sub_raw(n_minus_2.d, n_minus_2.d, two.d, curve.k);
    bn_norm(&n_minus_2);

    bn_t w;
    if (bn_modexp(&w, &s_bn, &n_minus_2, &curve.n) != 0) return -1;

    struct ec_mont mn;
    ec_mont_init(&mn, &curve.n, curve.k);
    bn_t z_m, r_m, w_m, u1, u2;
    ec_to_mont(&mn, &z_m, &z);
    ec_to_mont(&mn, &r_m, &r_bn);
    ec_to_mont(&mn, &w_m, &w);
    ec_mul_m(&mn, &u1, &z_m, &w_m);
    ec_from_mont(&mn, &u1, &u1);
    ec_mul_m(&mn, &u2, &r_m, &w_m);
    ec_from_mont(&mn, &u2, &u2);

    /* Compute R = u1*G + u2*Q using Shamir's trick */
    struct ec_jac G, Q, GQ, R;
    ec_to_mont(&mp, &G.x, &curve.gx);
    ec_to_mont(&mp, &G.y, &curve.gy);
    ec_to_mont(&mp, &G.z, &mp.one);
    G.inf = 0;

    Q.x = qx_m;
    Q.y = qy_m;
    Q.z = G.z;
    Q.inf = 0;

    ec_jac_add(&mp, &GQ, &G, &Q);
    R.inf = 1;

    size_t max_bits = coord_bytes * 8;
    for (size_t b = max_bits; b > 0; b--) {
        size_t idx = b - 1;
        ec_jac_double(&mp, &R, &R);
        uint32_t b1 = (idx < u1.n * 32) ? ((u1.d[idx / 32] >> (idx % 32)) & 1U) : 0;
        uint32_t b2 = (idx < u2.n * 32) ? ((u2.d[idx / 32] >> (idx % 32)) & 1U) : 0;
        if (b1 && b2) ec_jac_add(&mp, &R, &R, &GQ);
        else if (b1)  ec_jac_add(&mp, &R, &R, &G);
        else if (b2)  ec_jac_add(&mp, &R, &R, &Q);
    }

    if (R.inf || bn_is_zero(&R.z)) return -1;

    /* Convert R.x to affine: x = R.x * R.z^(-2) mod p */
    bn_t z_aff, z2, z2_inv, p_minus_2, x_aff;
    ec_from_mont(&mp, &z_aff, &R.z);
    ec_to_mont(&mp, &z2, &z_aff);
    ec_sqr_m(&mp, &z2, &z2);
    ec_from_mont(&mp, &z2, &z2);

    p_minus_2 = curve.p;
    bn_sub_raw(p_minus_2.d, p_minus_2.d, two.d, curve.k);
    bn_norm(&p_minus_2);
    if (bn_modexp(&z2_inv, &z2, &p_minus_2, &curve.p) != 0) return -1;

    ec_to_mont(&mp, &z2_inv, &z2_inv);
    ec_mul_m(&mp, &x_aff, &R.x, &z2_inv);
    ec_from_mont(&mp, &x_aff, &x_aff);

    if (bn_cmp(&x_aff, &curve.n) >= 0) {
        bn_sub_raw(x_aff.d, x_aff.d, curve.n.d, curve.k);
        bn_norm(&x_aff);
    }

    return (bn_cmp(&x_aff, &r_bn) == 0) ? 0 : -1;
}

int seld_crypto_verify_sig(const struct seld_pub_key* pk,
                           enum seld_sig_alg alg,
                           int pss_salt_len,
                           const uint8_t* msg, size_t msg_len,
                           const uint8_t* sig, size_t sig_len) {
    if (!pk || !msg || !sig) return -1;
    if (alg == SELD_SIG_RSA_PKCS1_SHA256 || alg == SELD_SIG_RSA_PKCS1_SHA384 || alg == SELD_SIG_RSA_PKCS1_SHA512 ||
        alg == SELD_SIG_RSA_PSS_SHA256 || alg == SELD_SIG_RSA_PSS_SHA384 || alg == SELD_SIG_RSA_PSS_SHA512) {
        return verify_rsa(pk, alg, pss_salt_len, msg, msg_len, sig, sig_len);
    }
    if (alg == SELD_SIG_ECDSA_P256_SHA256 || alg == SELD_SIG_ECDSA_P384_SHA384) {
        return verify_ecdsa(pk, alg, msg, msg_len, sig, sig_len);
    }
    return -1;
}

int seld_crypto_verify_tls13_cv(const struct seld_pub_key* pk,
                                uint16_t sig_scheme,
                                const uint8_t* content, size_t content_len,
                                const uint8_t* sig, size_t sig_len) {
    if (!pk || !content || !sig) return -1;
    enum seld_sig_alg alg = SELD_SIG_UNKNOWN;
    int expected_salt = -1;
    switch (sig_scheme) {
        case 0x0403: alg = SELD_SIG_ECDSA_P256_SHA256; break;
        case 0x0503: alg = SELD_SIG_ECDSA_P384_SHA384; break;
        case 0x0804: alg = SELD_SIG_RSA_PSS_SHA256; expected_salt = 32; break;
        case 0x0805: alg = SELD_SIG_RSA_PSS_SHA384; expected_salt = 48; break;
        case 0x0806: alg = SELD_SIG_RSA_PSS_SHA512; expected_salt = 64; break;
        default: return -1;
    }
    return seld_crypto_verify_sig(pk, alg, expected_salt, content, content_len, sig, sig_len);
}

/* ========================================================================= */
/* X.509 ASN.1 DER Certificate Parser                                        */
/* ========================================================================= */

static int parse_hash_oid(const uint8_t* oid, size_t len) {
    /* 2.16.840.1.101.3.4.2.{1=SHA256, 2=SHA384, 3=SHA512} */
    static const uint8_t sha_base[8] = {0x60,0x86,0x48,0x01,0x65,0x03,0x04,0x02};
    if (len == 9 && memcmp(oid, sha_base, 8) == 0) {
        if (oid[8] == 1) return 256;
        if (oid[8] == 2) return 384;
        if (oid[8] == 3) return 512;
    }
    return 0;
}

static enum seld_sig_alg parse_alg_id(const uint8_t* der, size_t len, int* pss_salt_out) {
    if (pss_salt_out) *pss_salt_out = -1;
    const uint8_t* p = der;
    const uint8_t* end = der + len;
    uint8_t tag;
    const uint8_t* oid;
    size_t oid_len;
    if (der_read_tlv(&p, end, &tag, &oid, &oid_len) != 0 || tag != 0x06) return SELD_SIG_UNKNOWN;

    static const uint8_t oid_rsa_256[9] = {0x2A,0x86,0x48,0x86,0xF7,0x0D,0x01,0x01,0x0B};
    static const uint8_t oid_rsa_384[9] = {0x2A,0x86,0x48,0x86,0xF7,0x0D,0x01,0x01,0x0C};
    static const uint8_t oid_rsa_512[9] = {0x2A,0x86,0x48,0x86,0xF7,0x0D,0x01,0x01,0x0D};
    static const uint8_t oid_rsa_pss[9] = {0x2A,0x86,0x48,0x86,0xF7,0x0D,0x01,0x01,0x0A};
    static const uint8_t oid_ec_256[8]  = {0x2A,0x86,0x48,0xCE,0x3D,0x04,0x03,0x02};
    static const uint8_t oid_ec_384[8]  = {0x2A,0x86,0x48,0xCE,0x3D,0x04,0x03,0x03};

    if (oid_len == 9 && memcmp(oid, oid_rsa_256, 9) == 0) return SELD_SIG_RSA_PKCS1_SHA256;
    if (oid_len == 9 && memcmp(oid, oid_rsa_384, 9) == 0) return SELD_SIG_RSA_PKCS1_SHA384;
    if (oid_len == 9 && memcmp(oid, oid_rsa_512, 9) == 0) return SELD_SIG_RSA_PKCS1_SHA512;
    if (oid_len == 8 && memcmp(oid, oid_ec_256, 8) == 0)  return SELD_SIG_ECDSA_P256_SHA256;
    if (oid_len == 8 && memcmp(oid, oid_ec_384, 8) == 0)  return SELD_SIG_ECDSA_P384_SHA384;

    if (oid_len == 9 && memcmp(oid, oid_rsa_pss, 9) == 0) {
        const uint8_t* pv;
        size_t pl;
        if (der_read_tlv(&p, end, &tag, &pv, &pl) != 0 || tag != 0x30) return SELD_SIG_UNKNOWN;
        const uint8_t* pp = pv;
        const uint8_t* pend = pv + pl;
        int hash_bits = 0;
        int salt_len = 20;
        while (pp < pend) {
            uint8_t ctag; const uint8_t* cv; size_t cl;
            if (der_read_tlv(&pp, pend, &ctag, &cv, &cl) != 0) return SELD_SIG_UNKNOWN;
            if (ctag == 0xA0) {
                const uint8_t* hp = cv;
                uint8_t htag; const uint8_t* hv; size_t hl;
                if (der_read_tlv(&hp, cv + cl, &htag, &hv, &hl) != 0 || htag != 0x30) return SELD_SIG_UNKNOWN;
                const uint8_t* op = hv;
                uint8_t otag; const uint8_t* ov; size_t ol;
                if (der_read_tlv(&op, hv + hl, &otag, &ov, &ol) != 0 || otag != 0x06) return SELD_SIG_UNKNOWN;
                hash_bits = parse_hash_oid(ov, ol);
            } else if (ctag == 0xA2) {
                const uint8_t* sp = cv;
                uint8_t stag; const uint8_t* sv; size_t sl;
                if (der_read_tlv(&sp, cv + cl, &stag, &sv, &sl) != 0 || stag != 0x02 || sl == 0 || sl > 2) return SELD_SIG_UNKNOWN;
                salt_len = 0;
                for (size_t i = 0; i < sl; i++) salt_len = (salt_len << 8) | sv[i];
            }
        }
        if (pss_salt_out) *pss_salt_out = salt_len;
        if (hash_bits == 256) return SELD_SIG_RSA_PSS_SHA256;
        if (hash_bits == 384) return SELD_SIG_RSA_PSS_SHA384;
        if (hash_bits == 512) return SELD_SIG_RSA_PSS_SHA512;
    }
    return SELD_SIG_UNKNOWN;
}

int seld_x509_parse_spki(const uint8_t* spki_der, size_t spki_len, struct seld_pub_key* out) {
    if (!spki_der || !out) return -1;
    memset(out, 0, sizeof(*out));
    const uint8_t* p = spki_der;
    const uint8_t* end = spki_der + spki_len;
    uint8_t tag;
    const uint8_t* seq_v;
    size_t seq_l;
    if (der_read_tlv(&p, end, &tag, &seq_v, &seq_l) != 0 || tag != 0x30 || p != end) return -1;

    const uint8_t* sp = seq_v;
    const uint8_t* send = seq_v + seq_l;
    const uint8_t* alg_v; size_t alg_l;
    const uint8_t* bit_v; size_t bit_l;
    if (der_read_tlv(&sp, send, &tag, &alg_v, &alg_l) != 0 || tag != 0x30) return -1;
    if (der_read_tlv(&sp, send, &tag, &bit_v, &bit_l) != 0 || tag != 0x03 || sp != send) return -1;
    if (bit_l < 2 || bit_v[0] != 0x00) return -1; /* unused bits must be 0 */

    const uint8_t* ap = alg_v;
    const uint8_t* aend = alg_v + alg_l;
    const uint8_t* oid; size_t oid_l;
    if (der_read_tlv(&ap, aend, &tag, &oid, &oid_l) != 0 || tag != 0x06) return -1;

    static const uint8_t oid_rsa_enc[9] = {0x2A,0x86,0x48,0x86,0xF7,0x0D,0x01,0x01,0x01};
    static const uint8_t oid_ec_pub[7]  = {0x2A,0x86,0x48,0xCE,0x3D,0x02,0x01};
    static const uint8_t oid_p256[8]    = {0x2A,0x86,0x48,0xCE,0x3D,0x03,0x01,0x07};
    static const uint8_t oid_p384[5]    = {0x2B,0x81,0x04,0x00,0x22};

    const uint8_t* key_bytes = bit_v + 1;
    size_t key_len = bit_l - 1;

    if (oid_l == 9 && memcmp(oid, oid_rsa_enc, 9) == 0) {
        const uint8_t* kp = key_bytes;
        const uint8_t* kend = key_bytes + key_len;
        const uint8_t* kseq; size_t kseq_l;
        if (der_read_tlv(&kp, kend, &tag, &kseq, &kseq_l) != 0 || tag != 0x30 || kp != kend) return -1;
        const uint8_t* rp = kseq;
        const uint8_t* rend = kseq + kseq_l;
        const uint8_t *nv, *ev, *nb, *eb;
        size_t nl, el, nbl, ebl;
        if (der_read_tlv(&rp, rend, &tag, &nv, &nl) != 0 || tag != 0x02) return -1;
        if (der_read_tlv(&rp, rend, &tag, &ev, &el) != 0 || tag != 0x02 || rp != rend) return -1;
        if (der_parse_uint_bytes(nv, nl, &nb, &nbl) != 0) return -1;
        if (der_parse_uint_bytes(ev, el, &eb, &ebl) != 0) return -1;
        if (nbl < 128 || nbl > SELD_MAX_RSA_BYTES || ebl == 0 || ebl > sizeof(out->rsa_e)) return -1;
        out->type = SELD_PK_RSA;
        memcpy(out->rsa_n, nb, nbl);
        out->rsa_n_len = nbl;
        memcpy(out->rsa_e, eb, ebl);
        out->rsa_e_len = ebl;
        return 0;
    }

    if (oid_l == 7 && memcmp(oid, oid_ec_pub, 7) == 0) {
        const uint8_t* curve_oid; size_t curve_l;
        if (der_read_tlv(&ap, aend, &tag, &curve_oid, &curve_l) != 0 || tag != 0x06) return -1;
        if (curve_l == 8 && memcmp(curve_oid, oid_p256, 8) == 0) {
            if (key_len != 65 || key_bytes[0] != 0x04) return -1;
            out->type = SELD_PK_EC_P256;
            memcpy(out->ec_point, key_bytes, 65);
            out->ec_len = 65;
            return 0;
        }
        if (curve_l == 5 && memcmp(curve_oid, oid_p384, 5) == 0) {
            if (key_len != 97 || key_bytes[0] != 0x04) return -1;
            out->type = SELD_PK_EC_P384;
            memcpy(out->ec_point, key_bytes, 97);
            out->ec_len = 97;
            return 0;
        }
    }

    return -1;
}

static void parse_x509_name(const uint8_t* name_val, size_t name_len, char* cn_out, size_t cn_sz, char* org_out, size_t org_sz) {
    if (cn_out && cn_sz > 0) cn_out[0] = '\0';
    if (org_out && org_sz > 0) org_out[0] = '\0';

    const uint8_t* p = name_val;
    const uint8_t* end = name_val + name_len;
    while (p < end) {
        uint8_t tag; const uint8_t* set_v; size_t set_l;
        if (der_read_tlv(&p, end, &tag, &set_v, &set_l) != 0 || tag != 0x31) break;
        const uint8_t* sp = set_v;
        const uint8_t* send = set_v + set_l;
        while (sp < send) {
            const uint8_t* seq_v; size_t seq_l;
            if (der_read_tlv(&sp, send, &tag, &seq_v, &seq_l) != 0 || tag != 0x30) break;
            const uint8_t* ap = seq_v;
            const uint8_t* aend = seq_v + seq_l;
            const uint8_t* oid; size_t oid_l;
            const uint8_t* str; size_t str_l;
            if (der_read_tlv(&ap, aend, &tag, &oid, &oid_l) != 0 || tag != 0x06) continue;
            if (der_read_tlv(&ap, aend, &tag, &str, &str_l) != 0) continue;
            if (oid_l == 3 && oid[0] == 0x55 && oid[1] == 0x04) {
                if (oid[2] == 0x03 && cn_out && cn_sz > 1 && cn_out[0] == '\0') {
                    size_t c = (str_l < cn_sz - 1) ? str_l : (cn_sz - 1);
                    memcpy(cn_out, str, c);
                    cn_out[c] = '\0';
                } else if (oid[2] == 0x0A && org_out && org_sz > 1 && org_out[0] == '\0') {
                    size_t c = (str_l < org_sz - 1) ? str_l : (org_sz - 1);
                    memcpy(org_out, str, c);
                    org_out[c] = '\0';
                }
            }
        }
    }
}

static int parse_x509_time(uint8_t tag, const uint8_t* val, size_t len, uint64_t* epoch_out) {
    int year = 0, mon = 0, day = 0, hour = 0, min = 0, sec = 0;
    size_t idx = 0;
    if (tag == 0x17) { /* UTCTime: YYMMDDHHMMSSZ */
        if (len != 13 || val[12] != 'Z') return -1;
        for (size_t i = 0; i < 12; i++) if (val[i] < '0' || val[i] > '9') return -1;
        int yy = (val[0] - '0') * 10 + (val[1] - '0');
        year = (yy >= 50) ? (1900 + yy) : (2000 + yy);
        idx = 2;
    } else if (tag == 0x18) { /* GeneralizedTime: YYYYMMDDHHMMSSZ */
        if (len != 15 || val[14] != 'Z') return -1;
        for (size_t i = 0; i < 14; i++) if (val[i] < '0' || val[i] > '9') return -1;
        year = (val[0] - '0') * 1000 + (val[1] - '0') * 100 + (val[2] - '0') * 10 + (val[3] - '0');
        idx = 4;
    } else {
        return -1;
    }

    mon  = (val[idx] - '0') * 10 + (val[idx + 1] - '0');
    day  = (val[idx + 2] - '0') * 10 + (val[idx + 3] - '0');
    hour = (val[idx + 4] - '0') * 10 + (val[idx + 5] - '0');
    min  = (val[idx + 6] - '0') * 10 + (val[idx + 7] - '0');
    sec  = (val[idx + 8] - '0') * 10 + (val[idx + 9] - '0');

    if (year < 1970 || mon < 1 || mon > 12 || day < 1 || day > 31 || hour > 23 || min > 59 || sec > 60) {
        return -1;
    }

    static const int days_before_month[13] = {0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    uint64_t days = 0;
    for (int y = 1970; y < year; y++) {
        int leap = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
        days += leap ? 366 : 365;
    }
    int cur_leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    days += (uint64_t)days_before_month[mon] + (uint64_t)(day - 1);
    if (mon > 2 && cur_leap) days++;

    *epoch_out = days * 86400ULL + (uint64_t)hour * 3600ULL + (uint64_t)min * 60ULL + (uint64_t)sec;
    return 0;
}

static void parse_x509_extensions(const uint8_t* exts_seq, size_t exts_len, struct seld_x509_cert* out) {
    const uint8_t* p = exts_seq;
    const uint8_t* end = exts_seq + exts_len;
    while (p < end) {
        uint8_t tag; const uint8_t* ev; size_t el;
        if (der_read_tlv(&p, end, &tag, &ev, &el) != 0 || tag != 0x30) break;
        const uint8_t* ep = ev;
        const uint8_t* eend = ev + el;
        const uint8_t* oid; size_t oid_l;
        if (der_read_tlv(&ep, eend, &tag, &oid, &oid_l) != 0 || tag != 0x06) continue;
        if (ep < eend && *ep == 0x01) { /* optional critical BOOLEAN */
            const uint8_t* bv; size_t bl;
            if (der_read_tlv(&ep, eend, &tag, &bv, &bl) != 0) continue;
        }
        const uint8_t* oct_v; size_t oct_l;
        if (der_read_tlv(&ep, eend, &tag, &oct_v, &oct_l) != 0 || tag != 0x04) continue;

        if (oid_l == 3 && oid[0] == 0x55 && oid[1] == 0x1D) {
            if (oid[2] == 0x13) { /* BasicConstraints 2.5.29.19 */
                const uint8_t* bp = oct_v;
                const uint8_t* bend = oct_v + oct_l;
                const uint8_t* bseq; size_t bseq_l;
                if (der_read_tlv(&bp, bend, &tag, &bseq, &bseq_l) == 0 && tag == 0x30) {
                    const uint8_t* ip = bseq;
                    const uint8_t* iend = bseq + bseq_l;
                    if (ip < iend && *ip == 0x01) {
                        const uint8_t* bv; size_t bl;
                        if (der_read_tlv(&ip, iend, &tag, &bv, &bl) == 0 && bl == 1) {
                            out->is_ca = (bv[0] != 0) ? 1 : 0;
                        }
                    }
                    if (ip < iend && *ip == 0x02) {
                        const uint8_t* pv; size_t pl;
                        if (der_read_tlv(&ip, iend, &tag, &pv, &pl) == 0 && pl >= 1 && pl <= 4) {
                            uint32_t plval = 0;
                            for (size_t i = 0; i < pl; i++) plval = (plval << 8) | pv[i];
                            out->has_pathlen = 1;
                            out->pathlen = plval;
                        }
                    }
                }
            } else if (oid[2] == 0x0F) { /* KeyUsage 2.5.29.15 */
                const uint8_t* kp = oct_v;
                const uint8_t* kend = oct_v + oct_l;
                const uint8_t* bv; size_t bl;
                if (der_read_tlv(&kp, kend, &tag, &bv, &bl) == 0 && tag == 0x03 && bl >= 2) {
                    out->has_key_usage = 1;
                    /* keyCertSign is bit 5 -> bit (7-5)=2 of byte 0 (0x04) */
                    size_t total_bits = (bl - 1) * 8 - bv[0];
                    if (total_bits > 5 && (bv[1] & 0x04)) {
                        out->key_cert_sign = 1;
                    }
                }
            } else if (oid[2] == 0x11) { /* SubjectAltName 2.5.29.17 */
                const uint8_t* sp = oct_v;
                const uint8_t* send = oct_v + oct_l;
                const uint8_t* sseq; size_t sseq_l;
                if (der_read_tlv(&sp, send, &tag, &sseq, &sseq_l) == 0 && tag == 0x30) {
                    const uint8_t* gp = sseq;
                    const uint8_t* gend = sseq + sseq_l;
                    size_t out_pos = 0;
                    while (gp < gend) {
                        uint8_t gtag; const uint8_t* gv; size_t gl;
                        if (der_read_tlv(&gp, gend, &gtag, &gv, &gl) != 0) break;
                        if (gtag == 0x82 && gl > 0 && gl <= 128) {
                            int valid = 1;
                            for (size_t j = 0; j < gl; j++) {
                                uint8_t c = gv[j];
                                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                      (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '*' || c == '_')) {
                                    valid = 0;
                                    break;
                                }
                            }
                            if (valid) {
                                out->has_san = 1;
                                if (out_pos > 0 && out_pos + 2 < sizeof(out->san)) {
                                    out->san[out_pos++] = ',';
                                    out->san[out_pos++] = ' ';
                                }
                                size_t to_copy = (gl < sizeof(out->san) - 1 - out_pos) ? gl : (sizeof(out->san) - 1 - out_pos);
                                memcpy(out->san + out_pos, gv, to_copy);
                                out_pos += to_copy;
                                out->san[out_pos] = '\0';
                            }
                        }
                    }
                }
            }
        }
    }
}

int seld_x509_parse(const uint8_t* der, size_t der_len, struct seld_x509_cert* out) {
    if (!der || !out) return -1;
    memset(out, 0, sizeof(*out));

    const uint8_t* p = der;
    const uint8_t* end = der + der_len;
    uint8_t tag;
    const uint8_t* cert_v; size_t cert_l;
    if (der_read_tlv(&p, end, &tag, &cert_v, &cert_l) != 0 || tag != 0x30 || p != end) return -1;

    const uint8_t* cp = cert_v;
    const uint8_t* cend = cert_v + cert_l;

    /* 1. tbsCertificate (capture raw outer TLV for signature verification) */
    const uint8_t* tbs_start = cp;
    const uint8_t* tbs_v; size_t tbs_l;
    if (der_read_tlv(&cp, cend, &tag, &tbs_v, &tbs_l) != 0 || tag != 0x30) return -1;
    out->tbs_ptr = tbs_start;
    out->tbs_len = (size_t)(cp - tbs_start);

    /* 2. signatureAlgorithm */
    const uint8_t* salg_v; size_t salg_l;
    if (der_read_tlv(&cp, cend, &tag, &salg_v, &salg_l) != 0 || tag != 0x30) return -1;
    out->sig_alg = parse_alg_id(salg_v, salg_l, &out->pss_salt_len);

    /* 3. signatureValue */
    const uint8_t* sig_v; size_t sig_l;
    if (der_read_tlv(&cp, cend, &tag, &sig_v, &sig_l) != 0 || tag != 0x03 || cp != cend) return -1;
    if (sig_l < 2 || sig_v[0] != 0x00) return -1;
    out->sig_ptr = sig_v + 1;
    out->sig_len = sig_l - 1;

    /* Parse inside tbsCertificate */
    const uint8_t* tp = tbs_v;
    const uint8_t* tend = tbs_v + tbs_l;

    /* Optional [0] EXPLICIT Version */
    if (tp < tend && *tp == 0xA0) {
        const uint8_t* vv; size_t vl;
        if (der_read_tlv(&tp, tend, &tag, &vv, &vl) != 0) return -1;
    }

    /* serialNumber INTEGER */
    const uint8_t* ser_v; size_t ser_l;
    if (der_read_tlv(&tp, tend, &tag, &ser_v, &ser_l) != 0 || tag != 0x02) return -1;

    /* inner signature AlgorithmIdentifier (must match outer signatureAlgorithm) */
    const uint8_t* isig_v; size_t isig_l;
    if (der_read_tlv(&tp, tend, &tag, &isig_v, &isig_l) != 0 || tag != 0x30) return -1;
    int inner_salt = -1;
    if (parse_alg_id(isig_v, isig_l, &inner_salt) != out->sig_alg) return -1;

    /* issuer Name (capture full TLV) */
    const uint8_t* iss_start = tp;
    const uint8_t* iss_v; size_t iss_l;
    if (der_read_tlv(&tp, tend, &tag, &iss_v, &iss_l) != 0 || tag != 0x30) return -1;
    out->issuer_der = iss_start;
    out->issuer_len = (size_t)(tp - iss_start);
    parse_x509_name(iss_v, iss_l, out->issuer_cn, sizeof(out->issuer_cn), out->issuer_org, sizeof(out->issuer_org));

    /* validity Validity */
    const uint8_t* val_v; size_t val_l;
    if (der_read_tlv(&tp, tend, &tag, &val_v, &val_l) != 0 || tag != 0x30) return -1;
    {
        const uint8_t* vp = val_v;
        const uint8_t* vend = val_v + val_l;
        uint8_t t1, t2; const uint8_t *v1, *v2; size_t l1, l2;
        if (der_read_tlv(&vp, vend, &t1, &v1, &l1) != 0) return -1;
        if (der_read_tlv(&vp, vend, &t2, &v2, &l2) != 0 || vp != vend) return -1;
        if (parse_x509_time(t1, v1, l1, &out->not_before) == 0 &&
            parse_x509_time(t2, v2, l2, &out->not_after) == 0 &&
            out->not_before <= out->not_after) {
            out->validity_ok = 1;
        }
    }

    /* subject Name (capture full TLV) */
    const uint8_t* subj_start = tp;
    const uint8_t* subj_v; size_t subj_l;
    if (der_read_tlv(&tp, tend, &tag, &subj_v, &subj_l) != 0 || tag != 0x30) return -1;
    out->subject_der = subj_start;
    out->subject_len = (size_t)(tp - subj_start);
    parse_x509_name(subj_v, subj_l, out->subject_cn, sizeof(out->subject_cn), NULL, 0);

    /* subjectPublicKeyInfo */
    const uint8_t* spki_start = tp;
    const uint8_t* spki_v; size_t spki_l;
    if (der_read_tlv(&tp, tend, &tag, &spki_v, &spki_l) != 0 || tag != 0x30) return -1;
    out->spki_der = spki_start;
    out->spki_len = (size_t)(tp - spki_start);
    if (seld_x509_parse_spki(out->spki_der, out->spki_len, &out->pub_key) != 0) {
        return -1;
    }

    /* Optional [1], [2], and [3] EXPLICIT extensions */
    while (tp < tend) {
        uint8_t etag; const uint8_t* ev; size_t el;
        if (der_read_tlv(&tp, tend, &etag, &ev, &el) != 0) return -1;
        if (etag == 0xA3) {
            const uint8_t* ep = ev;
            const uint8_t* eend = ev + el;
            uint8_t stag; const uint8_t* sv; size_t sl;
            if (der_read_tlv(&ep, eend, &stag, &sv, &sl) != 0 || stag != 0x30 || ep != eend) return -1;
            parse_x509_extensions(sv, sl, out);
        }
    }

    return 0;
}

/* ========================================================================= */
/* X.509 Certificate Chain & Embedded Trust Anchor Verification              */
/* ========================================================================= */

static const struct seld_root_ca_entry* find_root_by_subject_and_spki(const struct seld_x509_cert* cert) {
    for (size_t i = 0; i < SELD_ROOT_CA_COUNT; i++) {
        if (cert->subject_len == s_seld_root_cas[i].subject_len &&
            cert->spki_len == s_seld_root_cas[i].spki_len &&
            memcmp(cert->subject_der, s_seld_root_cas[i].subject_der, cert->subject_len) == 0 &&
            memcmp(cert->spki_der, s_seld_root_cas[i].spki_der, cert->spki_len) == 0) {
            return &s_seld_root_cas[i];
        }
    }
    return NULL;
}

int seld_x509_verify_chain(const uint8_t* const* certs_der,
                           const size_t* certs_len,
                           size_t num_certs,
                           uint64_t now_epoch,
                           struct seld_x509_cert* leaf_out,
                           char* root_name_out, size_t root_name_sz) {
    if (!certs_der || !certs_len || num_certs == 0 || num_certs > 6) return -1;
    if (root_name_out && root_name_sz > 0) root_name_out[0] = '\0';

    struct seld_x509_cert* chain = (struct seld_x509_cert*)malloc(sizeof(struct seld_x509_cert) * num_certs);
    if (!chain) return -1;

    for (size_t i = 0; i < num_certs; i++) {
        if (seld_x509_parse(certs_der[i], certs_len[i], &chain[i]) != 0) {
            free(chain);
            return -2;
        }
        if (!chain[i].validity_ok) {
            free(chain);
            return -3;
        }
        if (now_epoch > 0) {
            if (now_epoch < chain[i].not_before || now_epoch > chain[i].not_after) {
                free(chain);
                return -4; /* Expired or not yet valid */
            }
        }
    }

    if (leaf_out) {
        *leaf_out = chain[0];
    }

    /* Leaf certificate must not be a CA */
    if (chain[0].is_ca) {
        free(chain);
        return -5;
    }

    /* Check if leaf itself directly matches a pinned root? No, leaf cannot be a CA.
     * Walk intermediates: verify cert[i] is signed by cert[i+1] OR directly by a trusted Root CA. */
    uint32_t ca_depth = 0;
    for (size_t i = 0; i < num_certs; i++) {
        if (i > 0) {
            if (!chain[i].is_ca) {
                free(chain);
                return -7; /* Non-CA certificate in intermediate/root position */
            }
            if (chain[i].has_key_usage && !chain[i].key_cert_sign) {
                free(chain);
                return -8; /* Intermediate/root lacks keyCertSign */
            }
            const struct seld_root_ca_entry* matched_root = find_root_by_subject_and_spki(&chain[i]);
            if (matched_root) {
                if (root_name_out && root_name_sz > 0) {
                    strncpy(root_name_out, matched_root->name, root_name_sz - 1);
                    root_name_out[root_name_sz - 1] = '\0';
                }
                free(chain);
                return 0;
            }
        }

        /* Check if cert[i] is directly signed by any embedded Root CA matching cert[i].issuer */
        for (size_t r = 0; r < SELD_ROOT_CA_COUNT; r++) {
            if (chain[i].issuer_len == s_seld_root_cas[r].subject_len &&
                memcmp(chain[i].issuer_der, s_seld_root_cas[r].subject_der, chain[i].issuer_len) == 0) {
                struct seld_pub_key root_pk;
                if (seld_x509_parse_spki(s_seld_root_cas[r].spki_der, s_seld_root_cas[r].spki_len, &root_pk) == 0) {
                    if (seld_crypto_verify_sig(&root_pk, chain[i].sig_alg, chain[i].pss_salt_len,
                                               chain[i].tbs_ptr, chain[i].tbs_len,
                                               chain[i].sig_ptr, chain[i].sig_len) == 0) {
                        if (root_name_out && root_name_sz > 0) {
                            strncpy(root_name_out, s_seld_root_cas[r].name, root_name_sz - 1);
                            root_name_out[root_name_sz - 1] = '\0';
                        }
                        free(chain);
                        return 0;
                    }
                }
            }
        }

        /* Otherwise, verify cert[i] against cert[i+1] in the server-supplied chain */
        if (i + 1 >= num_certs) {
            free(chain);
            return -6; /* No trusted root anchor found */
        }

        const struct seld_x509_cert* issuer = &chain[i + 1];
        if (!issuer->is_ca) {
            free(chain);
            return -7; /* Intermediate is not marked BasicConstraints cA=TRUE */
        }
        if (issuer->has_key_usage && !issuer->key_cert_sign) {
            free(chain);
            return -8; /* Intermediate lacks keyCertSign */
        }
        if (issuer->has_pathlen && ca_depth > issuer->pathlen) {
            free(chain);
            return -9; /* BasicConstraints pathLenConstraint exceeded */
        }
        if (chain[i].issuer_len != issuer->subject_len ||
            memcmp(chain[i].issuer_der, issuer->subject_der, chain[i].issuer_len) != 0) {
            free(chain);
            return -10; /* Issuer/Subject DN mismatch */
        }
        if (seld_crypto_verify_sig(&issuer->pub_key, chain[i].sig_alg, chain[i].pss_salt_len,
                                   chain[i].tbs_ptr, chain[i].tbs_len,
                                   chain[i].sig_ptr, chain[i].sig_len) != 0) {
            free(chain);
            return -11; /* Signature verification failed */
        }
        ca_depth++;
    }

    free(chain);
    return -12;
}
