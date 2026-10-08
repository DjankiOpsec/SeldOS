/*
 * SeldOS - Humboldt Kernel Project
 * Sovereign Air-Gap Optical & Acoustic Cryptographic Subsystem
 * 40-Line Key Sheet Derivation, AES-128-GCM AEAD, Base64 & CRC32
 * GPLv3 Licensed.
 */

#ifndef _DIODE_CRYPTO_H_
#define _DIODE_CRYPTO_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define DIODE_KEY_LINES         40
#define DIODE_CHARS_PER_LINE    16
#define DIODE_KEY_BUF_SIZE      2048
#define DIODE_AES_KEY_SIZE      16  /* AES-128 */
#define DIODE_IV_SIZE           12  /* GCM IV */
#define DIODE_TAG_SIZE          16  /* GCM AEAD Tag */
#define DIODE_HASH_SIZE         32  /* SHA-256 */
#define DIODE_MAX_FILENAME      32
#define DIODE_MAGIC             0x53454C44 /* "SELD" */

#pragma pack(push, 1)
struct diode_envelope_header {
    uint32_t magic;                      /* "SELD" (0x53454C44) */
    uint8_t  version;                    /* Protocol version 1 */
    uint32_t session_id;                 /* Random 32-bit session ID */
    char     filename[DIODE_MAX_FILENAME]; /* Original file name (null-padded) */
    uint32_t plaintext_len;              /* Plaintext byte length */
    uint8_t  iv[DIODE_IV_SIZE];          /* AES-GCM IV (12 bytes) */
    uint8_t  tag[DIODE_TAG_SIZE];        /* AES-GCM AEAD Tag (16 bytes) */
    uint8_t  plaintext_sha256[DIODE_HASH_SIZE]; /* SHA-256 of plaintext */
};
#pragma pack(pop)

/* Generate cryptographically random bytes */
void diode_random_bytes(uint8_t* out, size_t len);

/* Generate 40-line key sheet */
int diode_generate_keysheet(char* out_buf, size_t max_len);

/* Derive AES-128 master key and key_id from 40-line key sheet string */
int diode_derive_master_key(const char* keysheet_str, uint8_t master_key[DIODE_AES_KEY_SIZE], uint32_t* key_id_out);

/* AES-128-GCM encrypt file payload into envelope */
int diode_encrypt_payload(const uint8_t key[DIODE_AES_KEY_SIZE],
                          const char* filename,
                          const uint8_t* plaintext, size_t pt_len,
                          uint32_t session_id,
                          uint8_t** out_envelope, size_t* out_env_len);

/* AES-128-GCM decrypt envelope into plaintext */
int diode_decrypt_payload(const uint8_t key[DIODE_AES_KEY_SIZE],
                          const uint8_t* envelope, size_t env_len,
                          char* out_filename,
                          uint8_t** out_plaintext, size_t* out_pt_len);

/* Standard CRC32 (polynomial 0xEDB88320) */
uint32_t diode_crc32(const void* data, size_t len);

/* Base64 encode and decode */
int diode_base64_encode(const uint8_t* in, size_t in_len, char* out, size_t out_max);
int diode_base64_decode(const char* in, size_t in_len, uint8_t* out, size_t out_max);

/* Secure memory wipe (volatile to prevent compiler optimization) */
void diode_secure_zero(void* ptr, size_t len);

#endif /* _DIODE_CRYPTO_H_ */
