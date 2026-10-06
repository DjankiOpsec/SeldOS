/*
 * SeldOS - Humboldt Kernel Project
 * SeldTLS - Native Sovereign TLS 1.3 Subsystem
 * Handshake and Secure Record Transport Layer (RFC 8446)
 * GPLv3 Licensed.
 */

#ifndef _SELD_TLS_H_
#define _SELD_TLS_H_

#include <stdint.h>
#include <stddef.h>

struct seld_tls_cert {
    int      valid;                  /* 1 if server certificate parsed */
    char     subject_cn[128];        /* Subject Common Name (e.g. example.com or .onion) */
    char     issuer_cn[128];         /* Issuer Common Name */
    char     issuer_org[128];        /* Issuer Organization (e.g. Let's Encrypt / DigiCert) */
    char     san[256];               /* Subject Alternative Names */
    uint8_t  sha256[32];             /* SHA-256 fingerprint of DER certificate */
    char     sha256_hex[65];         /* SHA-256 hex string */
    int      domain_matched;         /* 1 if requested SNI matches Subject CN or SAN */
    int      ca_verified;            /* 1 if Issuer matches recognized Root PKI CA */
    uint32_t cert_len;               /* Certificate length in bytes */
};

struct seld_tls_conn {
    int sock;
    uint8_t client_app_key[16];
    uint8_t client_app_iv[12];
    uint8_t server_app_key[16];
    uint8_t server_app_iv[12];
    uint64_t client_seq;
    uint64_t server_seq;

    uint8_t rx_record_buf[16384 + 256];
    uint8_t rx_decrypted_buf[16384 + 256];
    size_t rx_decrypted_len;
    size_t rx_decrypted_pos;

    uint8_t tx_buf[16384 + 256];

    char sni_host[128];
    struct seld_tls_cert peer_cert;
};

/* Connect to remote host over TLS 1.3 via existing TCP socket */
int seld_tls_handshake(struct seld_tls_conn* conn, int tcp_sock, const char* sni_hostname);

/* Send application data over established TLS 1.3 connection */
int seld_tls_write(struct seld_tls_conn* conn, const void* data, size_t len);

/* Read decrypted application data */
int seld_tls_read(struct seld_tls_conn* conn, void* buf, size_t max_len, uint32_t timeout_ms);

/* Retrieve peer certificate information */
int seld_tls_get_cert(const struct seld_tls_conn* conn, struct seld_tls_cert* out_cert);

/* Close TLS connection */
void seld_tls_close(struct seld_tls_conn* conn);

#endif /* _SELD_TLS_H_ */

