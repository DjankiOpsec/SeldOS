/*
 * SeldOS - Humboldt Kernel Project
 * Sovereign Air-Gap Optical & Acoustic Cryptographic Subsystem
 * 40-Line Key Sheet Derivation, AES-128-GCM AEAD, Base64 & CRC32
 * GPLv3 Licensed.
 */

#include "diode_crypto.h"
#include "sha256.h"
#include "seld_hkdf.h"
#include "seld_aes128_gcm.h"
#include "seld.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static inline uint64_t diode_rdtsc(void) {
#if defined(__riscv)
    uint64_t val;
    __asm__ volatile ("rdtime %0" : "=r"(val));
    return val;
#else
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
#endif
}

void diode_secure_zero(void* ptr, size_t len) {
    if (!ptr || len == 0) return;
    volatile uint8_t* p = (volatile uint8_t*)ptr;
    while (len--) {
        *p++ = 0;
    }
}

void diode_random_bytes(uint8_t* out, size_t len) {
    static uint64_t seed = 0x5E1DCAFE5E1D1337ULL;
    static uint8_t pool[32] = {0};
    static size_t pool_idx = 32;

    for (size_t i = 0; i < len; i++) {
        if (pool_idx >= 32) {
            uint64_t t = diode_rdtsc();
            seed = seed * 6364136223846793005ULL + 1442695040888963407ULL + t;
            
            struct sha256_ctx ctx;
            sha256_init(&ctx);
            sha256_update(&ctx, &seed, sizeof(seed));
            sha256_update(&ctx, &t, sizeof(t));
            uint64_t uptime = seld_uptime();
            sha256_update(&ctx, &uptime, sizeof(uptime));
            int pid = seld_getpid();
            sha256_update(&ctx, &pid, sizeof(pid));
            sha256_update(&ctx, pool, sizeof(pool));
            sha256_final(&ctx, pool);
            pool_idx = 0;
        }
        out[i] = pool[pool_idx++];
    }
}

static const char HEX_CHARS[] = "0123456789ABCDEF";

int diode_generate_keysheet(char* out_buf, size_t max_len) {
    if (!out_buf || max_len < 1200) return -1;

    size_t offset = 0;
    offset += snprintf(out_buf + offset, max_len - offset,
                       "# SELD-AIRGAP SOVEREIGN ONE-TIME KEY SHEET (40 LINES)\n"
                       "# OPSEC: ZERO-TRUST ACOUSTIC/OPTICAL TRANSMISSION BINDING\n");

    for (int line = 1; line <= DIODE_KEY_LINES; line++) {
        uint8_t rand_bytes[8];
        diode_random_bytes(rand_bytes, sizeof(rand_bytes));

        char hex[17];
        for (int b = 0; b < 8; b++) {
            hex[b * 2]     = HEX_CHARS[(rand_bytes[b] >> 4) & 0x0F];
            hex[b * 2 + 1] = HEX_CHARS[rand_bytes[b] & 0x0F];
        }
        hex[16] = '\0';

        /* Formatted as 4 groups of 4: L01: A7F2-9C1B-D4E8-3F10 */
        offset += snprintf(out_buf + offset, max_len - offset,
                           "L%02d: %.4s-%.4s-%.4s-%.4s\n",
                           line, hex, hex + 4, hex + 8, hex + 12);
        if (offset >= max_len - 64) {
            return -1; /* Overflow guard */
        }
    }

    return (int)offset;
}

int diode_derive_master_key(const char* keysheet_str, uint8_t master_key[DIODE_AES_KEY_SIZE], uint32_t* key_id_out) {
    if (!keysheet_str || !master_key) return -1;

    /* Extract clean key characters (ignoring comments '#' and label prefixes 'L%d:') */
    char clean_tokens[DIODE_KEY_LINES * DIODE_CHARS_PER_LINE + 16];
    size_t clean_len = 0;

    const char* p = keysheet_str;
    while (*p) {
        // Skip leading whitespace
        while (*p == ' ' || *p == '\t' || *p == '\r') p++;

        // Skip comment lines
        if (*p == '#') {
            while (*p && *p != '\n') p++;
            if (*p == '\n') p++;
            continue;
        }

        // Skip line prefix up to ':' if present on this line (e.g. 'L01:', 'LINE 01:', '01:')
        const char* colon = strchr(p, ':');
        const char* newline = strchr(p, '\n');
        if (colon && (!newline || colon < newline) && (colon - p) < 20) {
            p = colon + 1;
        }

        // Collect hex chars until end of line
        while (*p && *p != '\n') {
            char c = *p++;
            if ((c >= '0' && c <= '9') ||
                (c >= 'A' && c <= 'F') ||
                (c >= 'a' && c <= 'f')) {
                if (clean_len < sizeof(clean_tokens) - 1) {
                    if (c >= 'a' && c <= 'f') c = (char)(c - 'a' + 'A');
                    clean_tokens[clean_len++] = c;
                }
            }
        }
        if (*p == '\n') p++;
    }
    clean_tokens[clean_len] = '\0';

    if (clean_len < 32) {
        return -2; /* Insufficient key material */
    }

    /* Compute SHA-256 of extracted key material */
    uint8_t sheet_hash[32];
    sha256_hash(clean_tokens, clean_len, sheet_hash);

    if (key_id_out) {
        *key_id_out = ((uint32_t)sheet_hash[0] << 24) |
                      ((uint32_t)sheet_hash[1] << 16) |
                      ((uint32_t)sheet_hash[2] << 8)  |
                      ((uint32_t)sheet_hash[3]);
    }

    /* HKDF-Extract with sovereign salt */
    static const char HKDF_SALT[] = "SeldOS-AirGap-KeySheet-v1";
    uint8_t prk[32];
    seld_hkdf_extract((const uint8_t*)HKDF_SALT, strlen(HKDF_SALT),
                      (const uint8_t*)clean_tokens, clean_len, prk);

    /* HKDF-Expand for AES-128 key (16 bytes):
     * T(1) = HMAC-SHA256(PRK, info || 0x01)
     */
    static const char HKDF_INFO[] = "AES-128-GCM-Payload-Key";
    uint8_t info_buf[64];
    size_t info_len = strlen(HKDF_INFO);
    memcpy(info_buf, HKDF_INFO, info_len);
    info_buf[info_len] = 0x01; /* Counter */

    uint8_t okm[32];
    seld_hmac_sha256(prk, 32, info_buf, info_len + 1, okm);

    memcpy(master_key, okm, DIODE_AES_KEY_SIZE);

    /* Zero sensitive intermediate buffers */
    diode_secure_zero(clean_tokens, sizeof(clean_tokens));
    diode_secure_zero(prk, sizeof(prk));
    diode_secure_zero(okm, sizeof(okm));

    return 0;
}

int diode_encrypt_payload(const uint8_t key[DIODE_AES_KEY_SIZE],
                          const char* filename,
                          const uint8_t* plaintext, size_t pt_len,
                          uint32_t session_id,
                          uint8_t** out_envelope, size_t* out_env_len) {
    if (!key || !plaintext || !out_envelope || !out_env_len) return -1;

    size_t env_size = sizeof(struct diode_envelope_header) + pt_len;
    uint8_t* env = (uint8_t*)malloc(env_size);
    if (!env) return -2;

    struct diode_envelope_header* hdr = (struct diode_envelope_header*)env;
    hdr->magic = DIODE_MAGIC;
    hdr->version = 1;
    hdr->session_id = session_id;
    memset(hdr->filename, 0, sizeof(hdr->filename));
    if (filename) {
        const char* base_fn = strrchr(filename, '/');
        if (base_fn) base_fn++;
        else base_fn = filename;
        strncpy(hdr->filename, base_fn, sizeof(hdr->filename) - 1);
    }
    hdr->plaintext_len = (uint32_t)pt_len;

    /* Random 12-byte IV */
    diode_random_bytes(hdr->iv, DIODE_IV_SIZE);

    /* Plaintext SHA-256 */
    sha256_hash(plaintext, pt_len, hdr->plaintext_sha256);

    /* AAD: everything from magic up to plaintext_len */
    size_t aad_len = offsetof(struct diode_envelope_header, iv);
    const uint8_t* aad = env;

    uint8_t* ciphertext = env + sizeof(struct diode_envelope_header);

    /* Encrypt with AES-128-GCM */
    seld_aes128_gcm_encrypt(key, hdr->iv, aad, aad_len, plaintext, pt_len, ciphertext, hdr->tag);

    *out_envelope = env;
    *out_env_len = env_size;
    return 0;
}

int diode_decrypt_payload(const uint8_t key[DIODE_AES_KEY_SIZE],
                          const uint8_t* envelope, size_t env_len,
                          char* out_filename,
                          uint8_t** out_plaintext, size_t* out_pt_len) {
    if (!key || !envelope || env_len < sizeof(struct diode_envelope_header) || !out_plaintext || !out_pt_len) {
        return -1;
    }

    const struct diode_envelope_header* hdr = (const struct diode_envelope_header*)envelope;
    if (hdr->magic != DIODE_MAGIC || hdr->version != 1) {
        return -2; /* Invalid header / magic */
    }

    size_t expected_size = sizeof(struct diode_envelope_header) + hdr->plaintext_len;
    if (env_len != expected_size) {
        return -3; /* Envelope length mismatch */
    }

    uint8_t* pt = (uint8_t*)malloc(hdr->plaintext_len + 1);
    if (!pt) return -4;

    size_t aad_len = offsetof(struct diode_envelope_header, iv);
    const uint8_t* aad = envelope;
    const uint8_t* ct = envelope + sizeof(struct diode_envelope_header);

    int res = seld_aes128_gcm_decrypt(key, hdr->iv, aad, aad_len, ct, hdr->plaintext_len, hdr->tag, pt);
    if (res != 0) {
        /* AEAD Tag mismatch! Fail-closed wipe */
        diode_secure_zero(pt, hdr->plaintext_len + 1);
        free(pt);
        return -5;
    }

    /* Verify plaintext SHA-256 */
    uint8_t computed_sha[32];
    sha256_hash(pt, hdr->plaintext_len, computed_sha);
    if (memcmp(computed_sha, hdr->plaintext_sha256, 32) != 0) {
        diode_secure_zero(pt, hdr->plaintext_len + 1);
        free(pt);
        return -6; /* Integrity violation */
    }

    pt[hdr->plaintext_len] = '\0';
    if (out_filename) {
        strncpy(out_filename, hdr->filename, DIODE_MAX_FILENAME - 1);
        out_filename[DIODE_MAX_FILENAME - 1] = '\0';
    }

    *out_plaintext = pt;
    *out_pt_len = hdr->plaintext_len;
    return 0;
}

/* Standard CRC-32 (polynomial 0xEDB88320) */
uint32_t diode_crc32(const void* data, size_t len) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(int)(crc & 1)));
        }
    }
    return ~crc;
}

static const char B64_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int diode_base64_encode(const uint8_t* in, size_t in_len, char* out, size_t out_max) {
    size_t out_len = 4 * ((in_len + 2) / 3);
    if (out_max <= out_len) return -1;

    size_t i = 0, j = 0;
    while (i < in_len) {
        uint32_t octet_a = (i < in_len) ? in[i++] : 0;
        uint32_t octet_b = (i < in_len) ? in[i++] : 0;
        uint32_t octet_c = (i < in_len) ? in[i++] : 0;

        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        out[j++] = B64_CHARS[(triple >> 18) & 0x3F];
        out[j++] = B64_CHARS[(triple >> 12) & 0x3F];
        out[j++] = (i > in_len + 1) ? '=' : B64_CHARS[(triple >> 6) & 0x3F];
        out[j++] = (i > in_len)     ? '=' : B64_CHARS[triple & 0x3F];
    }
    out[j] = '\0';
    return (int)j;
}

static int b64_char_val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

int diode_base64_decode(const char* in, size_t in_len, uint8_t* out, size_t out_max) {
    if (!in || !out) return -1;
    size_t i = 0, j = 0;

    while (i < in_len) {
        while (i < in_len && (in[i] == ' ' || in[i] == '\r' || in[i] == '\n' || in[i] == '\t')) i++;
        if (i >= in_len) break;

        char c1 = in[i++];
        char c2 = (i < in_len) ? in[i++] : '=';
        char c3 = (i < in_len) ? in[i++] : '=';
        char c4 = (i < in_len) ? in[i++] : '=';

        int v1 = b64_char_val(c1);
        int v2 = b64_char_val(c2);
        int v3 = (c3 == '=') ? 0 : b64_char_val(c3);
        int v4 = (c4 == '=') ? 0 : b64_char_val(c4);

        if (v1 < 0 || v2 < 0 || (c3 != '=' && v3 < 0) || (c4 != '=' && v4 < 0)) {
            return -2; /* Invalid Base64 character */
        }

        uint32_t triple = ((uint32_t)v1 << 18) | ((uint32_t)v2 << 12) | ((uint32_t)v3 << 6) | (uint32_t)v4;

        if (j < out_max) out[j++] = (uint8_t)((triple >> 16) & 0xFF);
        if (c3 != '=' && j < out_max) out[j++] = (uint8_t)((triple >> 8) & 0xFF);
        if (c4 != '=' && j < out_max) out[j++] = (uint8_t)(triple & 0xFF);
    }

    return (int)j;
}
