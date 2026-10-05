/*
 * SeldOS - Humboldt Kernel Project
 * SeldTLS - Native Sovereign TLS 1.3 Subsystem
 * HMAC-SHA256 and HKDF (RFC 5869 / RFC 8446) Implementation
 * GPLv3 Licensed.
 */

#include "seld_hkdf.h"
#include "sha256.h"
#include <string.h>

void seld_hmac_sha256(const uint8_t* key, size_t key_len,
                      const uint8_t* data, size_t data_len,
                      uint8_t out[32]) {
    uint8_t k[64] = {0};
    if (key_len > 64) {
        sha256_hash(key, key_len, k);
    } else {
        memcpy(k, key, key_len);
    }

    uint8_t ipad[64];
    uint8_t opad[64];
    for (int i = 0; i < 64; i++) {
        ipad[i] = k[i] ^ 0x36;
        opad[i] = k[i] ^ 0x5c;
    }

    struct sha256_ctx ctx;
    uint8_t inner_hash[32];

    sha256_init(&ctx);
    sha256_update(&ctx, ipad, 64);
    if (data && data_len > 0) {
        sha256_update(&ctx, data, data_len);
    }
    sha256_final(&ctx, inner_hash);

    sha256_init(&ctx);
    sha256_update(&ctx, opad, 64);
    sha256_update(&ctx, inner_hash, 32);
    sha256_final(&ctx, out);
}

void seld_hkdf_extract(const uint8_t* salt, size_t salt_len,
                       const uint8_t* ikm, size_t ikm_len,
                       uint8_t prk[32]) {
    static const uint8_t zero_salt[32] = {0};
    if (!salt || salt_len == 0) {
        salt = zero_salt;
        salt_len = 32;
    }
    seld_hmac_sha256(salt, salt_len, ikm, ikm_len, prk);
}

void seld_hkdf_expand_label(const uint8_t prk[32],
                            const char* label,
                            const uint8_t* context, size_t context_len,
                            uint16_t length,
                            uint8_t* out) {
    /* RFC 8446 Section 7.1:
     * struct {
     *     uint16 length = Length;
     *     opaque label<7..255> = "tls13 " + Label;
     *     opaque context<0..255> = Context;
     * } HkdfLabel;
     */
    uint8_t hkdf_label[256];
    size_t label_len = strlen(label);
    size_t full_label_len = 6 + label_len; // "tls13 " + label

    hkdf_label[0] = (uint8_t)(length >> 8);
    hkdf_label[1] = (uint8_t)length;
    hkdf_label[2] = (uint8_t)full_label_len;
    memcpy(hkdf_label + 3, "tls13 ", 6);
    memcpy(hkdf_label + 9, label, label_len);

    size_t pos = 9 + label_len;
    hkdf_label[pos++] = (uint8_t)context_len;
    if (context && context_len > 0) {
        memcpy(hkdf_label + pos, context, context_len);
        pos += context_len;
    }

    // In TLS 1.3, length is at most 32 bytes (1 AES key, 1 IV, or 1 Hash) -> 1 block T(1)
    uint8_t info[257];
    memcpy(info, hkdf_label, pos);
    info[pos] = 0x01; // Block 1 counter

    uint8_t t[32];
    seld_hmac_sha256(prk, 32, info, pos + 1, t);
    memcpy(out, t, length);
}

void seld_tls13_derive_secret(const uint8_t secret[32],
                              const char* label,
                              const uint8_t* transcript_hash,
                              uint8_t out[32]) {
    seld_hkdf_expand_label(secret, label, transcript_hash, 32, 32, out);
}
