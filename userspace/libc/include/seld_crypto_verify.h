/*
 * SeldOS - Humboldt Kernel Project
 * SeldTLS - Cryptographic Verification & X.509 DER Parser Interface
 * RSA (PKCS#1 v1.5 / PSS), ECDSA (P-256 / P-384), X.509 Chain & Trust Anchor
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#ifndef _SELD_CRYPTO_VERIFY_H_
#define _SELD_CRYPTO_VERIFY_H_

#include <stdint.h>
#include <stddef.h>

#define SELD_MAX_RSA_BYTES 512  /* Up to 4096-bit RSA */
#define SELD_MAX_EC_BYTES  97   /* Uncompressed P-384: 1 + 48 + 48 */

enum seld_pk_type {
    SELD_PK_NONE = 0,
    SELD_PK_RSA = 1,
    SELD_PK_EC_P256 = 2,
    SELD_PK_EC_P384 = 3
};

enum seld_sig_alg {
    SELD_SIG_UNKNOWN = 0,
    SELD_SIG_RSA_PKCS1_SHA256 = 1,
    SELD_SIG_RSA_PKCS1_SHA384 = 2,
    SELD_SIG_RSA_PKCS1_SHA512 = 3,
    SELD_SIG_RSA_PSS_SHA256 = 4,
    SELD_SIG_RSA_PSS_SHA384 = 5,
    SELD_SIG_RSA_PSS_SHA512 = 6,
    SELD_SIG_ECDSA_P256_SHA256 = 7,
    SELD_SIG_ECDSA_P384_SHA384 = 8
};

struct seld_pub_key {
    enum seld_pk_type type;
    uint8_t  rsa_n[SELD_MAX_RSA_BYTES];
    size_t   rsa_n_len;
    uint8_t  rsa_e[16];
    size_t   rsa_e_len;
    uint8_t  ec_point[SELD_MAX_EC_BYTES];
    size_t   ec_len;
};

struct seld_x509_cert {
    const uint8_t* tbs_ptr;
    size_t         tbs_len;
    const uint8_t* issuer_der;
    size_t         issuer_len;
    const uint8_t* subject_der;
    size_t         subject_len;
    const uint8_t* spki_der;
    size_t         spki_len;
    enum seld_sig_alg sig_alg;
    const uint8_t* sig_ptr;
    size_t         sig_len;
    int            pss_salt_len;

    struct seld_pub_key pub_key;

    uint64_t not_before;
    uint64_t not_after;
    int      validity_ok;

    int      is_ca;
    int      has_pathlen;
    uint32_t pathlen;
    int      has_key_usage;
    int      key_cert_sign;
    int      has_san;

    char     subject_cn[128];
    char     issuer_cn[128];
    char     issuer_org[128];
    char     san[256];
};

void seld_explicit_bzero(void* ptr, size_t len);

int seld_x509_parse_spki(const uint8_t* spki_der, size_t spki_len, struct seld_pub_key* out);
int seld_x509_parse(const uint8_t* der, size_t der_len, struct seld_x509_cert* out);

int seld_crypto_verify_sig(const struct seld_pub_key* pk,
                           enum seld_sig_alg alg,
                           int pss_salt_len,
                           const uint8_t* msg, size_t msg_len,
                           const uint8_t* sig, size_t sig_len);

int seld_crypto_verify_tls13_cv(const struct seld_pub_key* pk,
                                uint16_t sig_scheme,
                                const uint8_t* content, size_t content_len,
                                const uint8_t* sig, size_t sig_len);

int seld_x509_verify_chain(const uint8_t* const* certs_der,
                           const size_t* certs_len,
                           size_t num_certs,
                           uint64_t now_epoch,
                           struct seld_x509_cert* leaf_out,
                           char* root_name_out, size_t root_name_sz);

#endif /* _SELD_CRYPTO_VERIFY_H_ */
