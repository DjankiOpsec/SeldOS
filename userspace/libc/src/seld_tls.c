/*
 * SeldOS - Humboldt Kernel Project
 * SeldTLS - Native Sovereign TLS 1.3 Subsystem
 * Handshake and Record Layer Implementation (RFC 8446)
 * Full X.509 Certificate Chain, Root CA Trust Store, & CertificateVerify Validation.
 * Pure C Freestanding Engine.
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#include "seld_tls.h"
#include "seld_x25519.h"
#include "seld_aes128_gcm.h"
#include "seld_hkdf.h"
#include "seld_crypto_verify.h"
#include "sha256.h"
#include <seld.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define SELD_TLS_MAX_CHAIN 6
#define SELD_TLS_REASM_CAP 32768

static int seld_get_random_bytes(uint8_t* out, size_t len) {
    if (len == 0) return 0;
    if (!out) return -1;
    int ret = seld_getrandom(out, len);
    if (ret != (int)len) {
        seld_explicit_bzero(out, len);
        return -1; // Fail closed: never fall back to predictable userspace entropy
    }
    return 0;
}

static void make_nonce(const uint8_t iv[12], uint64_t seq, uint8_t nonce[12]) {
    memcpy(nonce, iv, 12);
    for (int i = 0; i < 8; i++) {
        nonce[11 - i] ^= (uint8_t)(seq >> (i * 8));
    }
}

static int tcp_recv_exact(int sock, uint8_t* buf, size_t len, uint32_t timeout_ms) {
    size_t received = 0;
    while (received < len) {
        int n = seld_tcp_recv(sock, buf + received, len - received, timeout_ms);
        if (n <= 0) return -1;
        received += (size_t)n;
    }
    return 0;
}

static int tcp_send_exact(int sock, const uint8_t* buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        int n = seld_tcp_send(sock, buf + sent, len - sent);
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

static int read_tls_record(int sock, uint8_t* out_hdr, uint8_t* out_body, size_t max_body, uint32_t timeout_ms) {
    if (tcp_recv_exact(sock, out_hdr, 5, timeout_ms) != 0) return -1;
    uint16_t rlen = (uint16_t)(((uint16_t)out_hdr[3] << 8) | (uint16_t)out_hdr[4]);
    if (rlen > max_body) return -2;
    if (rlen > 0 && tcp_recv_exact(sock, out_body, rlen, timeout_ms) != 0) return -3;
    return (int)rlen;
}

static int domain_matches(const char* host, const char* pattern) {
    if (!host || !pattern || !*host || !*pattern) return 0;
    if (strcasecmp(host, pattern) == 0) return 1;

    /* RFC 6125 Section 6.4.3 single-label leftmost wildcard: *.example.com */
    if (pattern[0] == '*' && pattern[1] == '.') {
        const char* p_suffix = pattern + 1; /* ".example.com" */
        /* Reject overly broad wildcards like "*.com" without an interior dot */
        if (strchr(p_suffix + 1, '.') == NULL) return 0;
        /* Reject patterns with additional wildcards */
        if (strchr(p_suffix + 1, '*') != NULL) return 0;

        size_t hlen = strlen(host);
        size_t slen = strlen(p_suffix);
        if (hlen <= slen) return 0;

        size_t prefix_len = hlen - slen;
        if (strcasecmp(host + prefix_len, p_suffix) != 0) return 0;

        /* Leftmost label must not contain '.' (wildcard matches exactly one label) */
        for (size_t i = 0; i < prefix_len; i++) {
            if (host[i] == '.') return 0;
        }
        return 1;
    }
    return 0;
}

static int parse_server_certificate(struct seld_tls_conn* conn,
                                    const uint8_t* msg_body, uint32_t msg_len,
                                    struct seld_pub_key* leaf_pk_out) {
    if (!conn || !msg_body || !leaf_pk_out || msg_len < 4) return -1;
    memset(leaf_pk_out, 0, sizeof(*leaf_pk_out));

    uint8_t ctx_len = msg_body[0];
    if ((uint32_t)(1 + ctx_len + 3) > msg_len) return -1;

    const uint8_t* p = msg_body + 1 + ctx_len;
    uint32_t cert_list_len = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
    p += 3;
    if (cert_list_len < 5 || (size_t)(p - msg_body) + cert_list_len != msg_len) return -1;

    const uint8_t* certs[SELD_TLS_MAX_CHAIN];
    size_t cert_lens[SELD_TLS_MAX_CHAIN];
    size_t num_certs = 0;

    const uint8_t* list_end = p + cert_list_len;
    while (p < list_end && num_certs < SELD_TLS_MAX_CHAIN) {
        if (p + 3 > list_end) return -1;
        uint32_t clen = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
        p += 3;
        if (clen == 0 || p + clen + 2 > list_end) return -1;

        certs[num_certs] = p;
        cert_lens[num_certs] = clen;
        num_certs++;
        p += clen;

        uint16_t ext_len = (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
        p += 2;
        if (p + ext_len > list_end) return -1;
        p += ext_len;
    }

    if (num_certs == 0) return -1;

    /* 1. Compute SHA-256 fingerprint of leaf certificate */
    sha256_hash(certs[0], cert_lens[0], conn->peer_cert.sha256);
    static const char hex_chars[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        conn->peer_cert.sha256_hex[i * 2]     = hex_chars[(conn->peer_cert.sha256[i] >> 4) & 0x0F];
        conn->peer_cert.sha256_hex[i * 2 + 1] = hex_chars[conn->peer_cert.sha256[i] & 0x0F];
    }
    conn->peer_cert.sha256_hex[64] = '\0';
    conn->peer_cert.cert_len = (uint32_t)cert_lens[0];

    /* 2. Strictly parse leaf X.509 DER structure */
    struct seld_x509_cert leaf_x509;
    if (seld_x509_parse(certs[0], cert_lens[0], &leaf_x509) != 0) {
        return -2;
    }

    *leaf_pk_out = leaf_x509.pub_key;
    conn->peer_cert.valid = 1;
    strncpy(conn->peer_cert.subject_cn, leaf_x509.subject_cn, sizeof(conn->peer_cert.subject_cn) - 1);
    strncpy(conn->peer_cert.issuer_cn, leaf_x509.issuer_cn, sizeof(conn->peer_cert.issuer_cn) - 1);
    strncpy(conn->peer_cert.issuer_org, leaf_x509.issuer_org, sizeof(conn->peer_cert.issuer_org) - 1);
    strncpy(conn->peer_cert.san, leaf_x509.san, sizeof(conn->peer_cert.san) - 1);

    /* 3. RFC 6125 Hostname Verification:
     * If SAN dNSName is present, match strictly against SAN; only fall back to Subject CN if SAN is absent. */
    if (leaf_x509.has_san && conn->peer_cert.san[0]) {
        const char* sp = conn->peer_cert.san;
        while (*sp) {
            while (*sp == ' ' || *sp == ',') sp++;
            if (!*sp) break;
            char token[128];
            size_t ti = 0;
            while (*sp && *sp != ',' && *sp != ' ' && ti < sizeof(token) - 1) {
                token[ti++] = *sp++;
            }
            token[ti] = '\0';
            if (domain_matches(conn->sni_host, token)) {
                conn->peer_cert.domain_matched = 1;
                break;
            }
        }
    } else if (domain_matches(conn->sni_host, conn->peer_cert.subject_cn)) {
        conn->peer_cert.domain_matched = 1;
    }

    /* 4. Full Cryptographic X.509 Chain & Root CA Verification */
    int is_onion = (strstr(conn->sni_host, ".onion") != NULL);
    if (!is_onion) {
        uint64_t now_epoch = seld_get_time();
        if (seld_x509_verify_chain(certs, cert_lens, num_certs, now_epoch, NULL, NULL, 0) == 0) {
            conn->peer_cert.ca_verified = 1;
        }
    }

    return 0;
}

int seld_tls_handshake(struct seld_tls_conn* conn, int tcp_sock, const char* sni_hostname) {
    if (!conn || tcp_sock < 0 || !sni_hostname) return -1;
    size_t sni_len = strlen(sni_hostname);
    if (sni_len == 0 || sni_len >= sizeof(conn->sni_host)) return -1;

    memset(conn, 0, sizeof(*conn));
    conn->sock = tcp_sock;
    memcpy(conn->sni_host, sni_hostname, sni_len + 1);

    /* 1. Generate ephemeral Client X25519 Keypair & Randoms (Fail-Closed CSPRNG) */
    uint8_t priv_key[32];
    uint8_t pub_key[32];
    uint8_t client_random[32];
    uint8_t session_id[32];

    if (seld_get_random_bytes(priv_key, 32) != 0 ||
        seld_get_random_bytes(client_random, 32) != 0 ||
        seld_get_random_bytes(session_id, 32) != 0) {
        seld_explicit_bzero(priv_key, sizeof(priv_key));
        return -17; /* CSPRNG failure */
    }
    seld_x25519_base(pub_key, priv_key);

    /* 2. Build ClientHello message with Standard Browser JA3/JA4 Fingerprint */
    uint8_t ch_buf[2048];
    static const uint16_t s_std_ciphers[] = {
        0x1301, // TLS_AES_128_GCM_SHA256 (Supported TLS 1.3 negotiated suite)
        0xc02b, // TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
        0xc02f, // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
        0xc02c, // TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384
        0xc030, // TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384
        0xcca9, // TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256
        0xcca8, // TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256
        0xc013, // TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA
        0xc014, // TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA
        0x009c, // TLS_RSA_WITH_AES_128_GCM_SHA256
        0x009d, // TLS_RSA_WITH_AES_256_GCM_SHA384
        0x002f, // TLS_RSA_WITH_AES_128_CBC_SHA
        0x0035  // TLS_RSA_WITH_AES_256_CBC_SHA
    };

    uint8_t exts[1024];
    size_t ep = 0;

    /* 1. Extension: Server Name Indication (0x0000) */
    exts[ep++] = 0x00; exts[ep++] = 0x00;
    uint16_t sni_ext_len = (uint16_t)(sni_len + 5);
    exts[ep++] = (uint8_t)(sni_ext_len >> 8); exts[ep++] = (uint8_t)sni_ext_len;
    uint16_t sni_list_len = (uint16_t)(sni_len + 3);
    exts[ep++] = (uint8_t)(sni_list_len >> 8); exts[ep++] = (uint8_t)sni_list_len;
    exts[ep++] = 0x00;
    exts[ep++] = (uint8_t)(sni_len >> 8); exts[ep++] = (uint8_t)sni_len;
    memcpy(exts + ep, sni_hostname, sni_len);
    ep += sni_len;

    /* 2. Extended Master Secret (0x0017) */
    exts[ep++] = 0x00; exts[ep++] = 0x17;
    exts[ep++] = 0x00; exts[ep++] = 0x00;

    /* 3. Renegotiation Info (0xff01) */
    exts[ep++] = 0xff; exts[ep++] = 0x01;
    exts[ep++] = 0x00; exts[ep++] = 0x01;
    exts[ep++] = 0x00;

    /* 4. Supported Groups (0x000a) -> X25519 (0x001d), P-256 (0x0017), P-384 (0x0018) */
    exts[ep++] = 0x00; exts[ep++] = 0x0a;
    exts[ep++] = 0x00; exts[ep++] = 0x08;
    exts[ep++] = 0x00; exts[ep++] = 0x06;
    exts[ep++] = 0x00; exts[ep++] = 0x1d;
    exts[ep++] = 0x00; exts[ep++] = 0x17;
    exts[ep++] = 0x00; exts[ep++] = 0x18;

    /* 5. EC Point Formats (0x000b) */
    exts[ep++] = 0x00; exts[ep++] = 0x0b;
    exts[ep++] = 0x00; exts[ep++] = 0x02;
    exts[ep++] = 0x01; exts[ep++] = 0x00;

    /* 6. Session Ticket (0x0023) */
    exts[ep++] = 0x00; exts[ep++] = 0x23;
    exts[ep++] = 0x00; exts[ep++] = 0x00;

    /* 7. ALPN (0x0010) -> http/1.1 */
    exts[ep++] = 0x00; exts[ep++] = 0x10;
    exts[ep++] = 0x00; exts[ep++] = 0x0b;
    exts[ep++] = 0x00; exts[ep++] = 0x09;
    exts[ep++] = 0x08; exts[ep++] = 'h'; exts[ep++] = 't'; exts[ep++] = 't'; exts[ep++] = 'p';
    exts[ep++] = '/'; exts[ep++] = '1'; exts[ep++] = '.'; exts[ep++] = '1';

    /* 8. Status Request / OCSP (0x0005) */
    exts[ep++] = 0x00; exts[ep++] = 0x05;
    exts[ep++] = 0x00; exts[ep++] = 0x05;
    exts[ep++] = 0x01;
    exts[ep++] = 0x00; exts[ep++] = 0x00;
    exts[ep++] = 0x00; exts[ep++] = 0x00;

    /* 9. Key Share (0x0033) -> X25519 Public Key */
    exts[ep++] = 0x00; exts[ep++] = 0x33;
    exts[ep++] = 0x00; exts[ep++] = 0x26;
    exts[ep++] = 0x00; exts[ep++] = 0x24;
    exts[ep++] = 0x00; exts[ep++] = 0x1d;
    exts[ep++] = 0x00; exts[ep++] = 0x20;
    memcpy(exts + ep, pub_key, 32);
    ep += 32;

    /* 10. Supported Versions (0x002b) -> TLS 1.3 (0x0304), TLS 1.2 (0x0303) */
    exts[ep++] = 0x00; exts[ep++] = 0x2b;
    exts[ep++] = 0x00; exts[ep++] = 0x05;
    exts[ep++] = 0x04;
    exts[ep++] = 0x03; exts[ep++] = 0x04;
    exts[ep++] = 0x03; exts[ep++] = 0x03;

    /* 11. Signature Algorithms (0x000d) — strictly algorithms we cryptographically verify */
    exts[ep++] = 0x00; exts[ep++] = 0x0d;
    exts[ep++] = 0x00; exts[ep++] = 0x12; /* ext len 18 */
    exts[ep++] = 0x00; exts[ep++] = 0x10; /* list len 16 (8 algorithms) */
    exts[ep++] = 0x04; exts[ep++] = 0x03; // ecdsa_secp256r1_sha256
    exts[ep++] = 0x08; exts[ep++] = 0x04; // rsa_pss_rsae_sha256
    exts[ep++] = 0x04; exts[ep++] = 0x01; // rsa_pkcs1_sha256 (for CA certs)
    exts[ep++] = 0x05; exts[ep++] = 0x03; // ecdsa_secp384r1_sha384
    exts[ep++] = 0x08; exts[ep++] = 0x05; // rsa_pss_rsae_sha384
    exts[ep++] = 0x05; exts[ep++] = 0x01; // rsa_pkcs1_sha384
    exts[ep++] = 0x08; exts[ep++] = 0x06; // rsa_pss_rsae_sha512
    exts[ep++] = 0x06; exts[ep++] = 0x01; // rsa_pkcs1_sha512

    /* 12. PSK Key Exchange Modes (0x002d) */
    exts[ep++] = 0x00; exts[ep++] = 0x2d;
    exts[ep++] = 0x00; exts[ep++] = 0x02;
    exts[ep++] = 0x01; exts[ep++] = 0x01;

    /* Build Handshake body */
    size_t chp = 0;
    ch_buf[chp++] = 0x03; ch_buf[chp++] = 0x03;
    memcpy(ch_buf + chp, client_random, 32); chp += 32;
    ch_buf[chp++] = 32;
    memcpy(ch_buf + chp, session_id, 32); chp += 32;
    ch_buf[chp++] = (uint8_t)(sizeof(s_std_ciphers) >> 8);
    ch_buf[chp++] = (uint8_t)(sizeof(s_std_ciphers) & 0xff);
    for (size_t ci = 0; ci < sizeof(s_std_ciphers) / sizeof(s_std_ciphers[0]); ci++) {
        ch_buf[chp++] = (uint8_t)(s_std_ciphers[ci] >> 8);
        ch_buf[chp++] = (uint8_t)(s_std_ciphers[ci] & 0xff);
    }
    ch_buf[chp++] = 0x01; ch_buf[chp++] = 0x00;
    ch_buf[chp++] = (uint8_t)(ep >> 8); ch_buf[chp++] = (uint8_t)ep;
    memcpy(ch_buf + chp, exts, ep); chp += ep;

    uint8_t ch_msg[2048];
    ch_msg[0] = 0x01;
    ch_msg[1] = (uint8_t)(chp >> 16);
    ch_msg[2] = (uint8_t)(chp >> 8);
    ch_msg[3] = (uint8_t)chp;
    memcpy(ch_msg + 4, ch_buf, chp);
    size_t ch_msg_len = 4 + chp;

    uint8_t ch_rec[2054];
    ch_rec[0] = 0x16; ch_rec[1] = 0x03; ch_rec[2] = 0x01;
    ch_rec[3] = (uint8_t)(ch_msg_len >> 8); ch_rec[4] = (uint8_t)ch_msg_len;
    memcpy(ch_rec + 5, ch_msg, ch_msg_len);
    size_t ch_rec_len = 5 + ch_msg_len;

    struct sha256_ctx tr_ctx;
    sha256_init(&tr_ctx);
    sha256_update(&tr_ctx, ch_msg, ch_msg_len);

    if (tcp_send_exact(tcp_sock, ch_rec, ch_rec_len) != 0) {
        seld_explicit_bzero(priv_key, sizeof(priv_key));
        return -2;
    }

    /* 3. Receive ServerHello record */
    uint8_t hdr[5];
    int rlen = read_tls_record(tcp_sock, hdr, conn->rx_record_buf, sizeof(conn->rx_record_buf), 15000);
    if (rlen <= 4 || hdr[0] != 0x16 || conn->rx_record_buf[0] != 0x02) {
        seld_explicit_bzero(priv_key, sizeof(priv_key));
        return -3;
    }

    uint32_t sh_msg_len = ((uint32_t)conn->rx_record_buf[1] << 16) |
                          ((uint32_t)conn->rx_record_buf[2] << 8)  |
                          (uint32_t)conn->rx_record_buf[3];
    if (sh_msg_len < 38 || 4 + sh_msg_len > (size_t)rlen) {
        seld_explicit_bzero(priv_key, sizeof(priv_key));
        return -4;
    }

    /* Reject HelloRetryRequest magic random (RFC 8446 Section 4.1.3) */
    static const uint8_t hrr_random[32] = {
        0xCF, 0x21, 0xAD, 0x74, 0xE5, 0x9A, 0x61, 0x11,
        0xBE, 0x1D, 0x8C, 0x02, 0x1E, 0x65, 0xB8, 0x91,
        0xC2, 0xA2, 0x11, 0x16, 0x7A, 0xBB, 0x8C, 0x5E,
        0x07, 0x9E, 0x09, 0xE2, 0xC8, 0xA8, 0x33, 0x9C
    };
    uint8_t* sh_body = conn->rx_record_buf + 4;
    if (memcmp(sh_body + 2, hrr_random, 32) == 0) {
        seld_explicit_bzero(priv_key, sizeof(priv_key));
        return -5;
    }

    sha256_update(&tr_ctx, conn->rx_record_buf, 4 + sh_msg_len);

    uint8_t sh_sess_len = sh_body[34];
    if (sh_sess_len > 32 || (uint32_t)(35 + sh_sess_len + 3 + 2) > sh_msg_len) {
        seld_explicit_bzero(priv_key, sizeof(priv_key));
        return -5;
    }
    uint16_t chosen_cipher = (uint16_t)(((uint16_t)sh_body[35 + sh_sess_len] << 8) | sh_body[36 + sh_sess_len]);
    if (chosen_cipher != 0x1301) {
        seld_explicit_bzero(priv_key, sizeof(priv_key));
        return -5;
    }
    size_t sh_pos = 35 + (size_t)sh_sess_len + 3;
    uint16_t sh_ext_len = (uint16_t)(((uint16_t)sh_body[sh_pos] << 8) | (uint16_t)sh_body[sh_pos + 1]);
    sh_pos += 2;
    if (sh_pos + sh_ext_len > sh_msg_len) {
        seld_explicit_bzero(priv_key, sizeof(priv_key));
        return -5;
    }

    uint8_t server_pub[32];
    int found_keyshare = 0;
    int tls13_version_ok = 0;
    size_t cur_ext = sh_pos;
    size_t sh_ext_end = sh_pos + sh_ext_len;
    while (cur_ext + 4 <= sh_ext_end) {
        uint16_t etype = (uint16_t)(((uint16_t)sh_body[cur_ext] << 8) | (uint16_t)sh_body[cur_ext + 1]);
        uint16_t elen = (uint16_t)(((uint16_t)sh_body[cur_ext + 2] << 8) | (uint16_t)sh_body[cur_ext + 3]);
        if (cur_ext + 4 + elen > sh_ext_end) {
            seld_explicit_bzero(priv_key, sizeof(priv_key));
            return -5;
        }
        if (etype == 0x0033 && elen == 36) {
            uint16_t grp = (uint16_t)(((uint16_t)sh_body[cur_ext + 4] << 8) | sh_body[cur_ext + 5]);
            uint16_t klen = (uint16_t)(((uint16_t)sh_body[cur_ext + 6] << 8) | sh_body[cur_ext + 7]);
            if (grp == 0x001d && klen == 32) {
                memcpy(server_pub, sh_body + cur_ext + 8, 32);
                found_keyshare = 1;
            }
        } else if (etype == 0x002b && elen == 2) {
            uint16_t ver = (uint16_t)(((uint16_t)sh_body[cur_ext + 4] << 8) | sh_body[cur_ext + 5]);
            if (ver == 0x0304) tls13_version_ok = 1;
        }
        cur_ext += 4 + elen;
    }
    if (!found_keyshare || !tls13_version_ok) {
        seld_explicit_bzero(priv_key, sizeof(priv_key));
        return -6;
    }

    /* 4. Derive Shared Secret & Handshake Traffic Keys */
    uint8_t shared_secret[32];
    seld_x25519(shared_secret, priv_key, server_pub);

    /* RFC 7748 Section 6.1: Reject all-zero shared secret (low-order curve points) */
    uint8_t ss_or = 0;
    for (int i = 0; i < 32; i++) ss_or |= shared_secret[i];
    if (ss_or == 0) {
        seld_explicit_bzero(priv_key, sizeof(priv_key));
        seld_explicit_bzero(shared_secret, sizeof(shared_secret));
        return -6;
    }

    uint8_t tr_hash_hello[32];
    struct sha256_ctx tr_copy;
    memcpy(&tr_copy, &tr_ctx, sizeof(tr_ctx));
    sha256_final(&tr_copy, tr_hash_hello);

    uint8_t zero32[32] = {0};
    uint8_t early_secret[32];
    seld_hkdf_extract(NULL, 0, zero32, 32, early_secret);

    uint8_t empty_hash[32];
    sha256_hash(NULL, 0, empty_hash);

    uint8_t derived_early[32];
    seld_tls13_derive_secret(early_secret, "derived", empty_hash, derived_early);

    uint8_t handshake_secret[32];
    seld_hkdf_extract(derived_early, 32, shared_secret, 32, handshake_secret);

    uint8_t client_hs_secret[32];
    uint8_t server_hs_secret[32];
    seld_tls13_derive_secret(handshake_secret, "c hs traffic", tr_hash_hello, client_hs_secret);
    seld_tls13_derive_secret(handshake_secret, "s hs traffic", tr_hash_hello, server_hs_secret);

    uint8_t s_hs_key[16], s_hs_iv[12];
    uint8_t c_hs_key[16], c_hs_iv[12];
    seld_hkdf_expand_label(server_hs_secret, "key", NULL, 0, 16, s_hs_key);
    seld_hkdf_expand_label(server_hs_secret, "iv", NULL, 0, 12, s_hs_iv);
    seld_hkdf_expand_label(client_hs_secret, "key", NULL, 0, 16, c_hs_key);
    seld_hkdf_expand_label(client_hs_secret, "iv", NULL, 0, 12, c_hs_iv);

    /* 5. Receive Server Encrypted Handshake Records */
    uint64_t s_hs_seq = 0;
    int server_finished_done = 0;
    int cert_received = 0;
    int cert_verify_received = 0;
    struct seld_pub_key leaf_pk;
    memset(&leaf_pk, 0, sizeof(leaf_pk));

    uint8_t* reasm_buf = (uint8_t*)malloc(SELD_TLS_REASM_CAP);
    if (!reasm_buf) {
        seld_explicit_bzero(priv_key, sizeof(priv_key));
        seld_explicit_bzero(shared_secret, sizeof(shared_secret));
        return -11;
    }
    size_t reasm_len = 0;
    int hs_err = 0;

    while (!server_finished_done) {
        rlen = read_tls_record(tcp_sock, hdr, conn->rx_record_buf, sizeof(conn->rx_record_buf), 15000);
        if (rlen <= 0) { hs_err = -7; break; }
        if (hdr[0] == 0x14) continue; /* Ignore middlebox ChangeCipherSpec */
        if (hdr[0] != 0x17) { hs_err = -8; break; }

        uint8_t nonce[12];
        make_nonce(s_hs_iv, s_hs_seq++, nonce);

        if (rlen < 16) { hs_err = -9; break; }
        size_t ct_len = (size_t)rlen - 16;
        uint8_t tag[16];
        memcpy(tag, conn->rx_record_buf + ct_len, 16);

        if (seld_aes128_gcm_decrypt(s_hs_key, nonce, hdr, 5, conn->rx_record_buf, ct_len, tag, conn->rx_decrypted_buf) != 0) {
            hs_err = -10;
            break;
        }

        /* Unpad inner content type with underflow guard */
        size_t pad = 0;
        while (pad < ct_len && conn->rx_decrypted_buf[ct_len - 1 - pad] == 0) pad++;
        if (pad >= ct_len) { hs_err = -10; break; }
        uint8_t inner_type = conn->rx_decrypted_buf[ct_len - 1 - pad];
        if (inner_type != 0x16) { hs_err = -8; break; }
        size_t inner_len = ct_len - 1 - pad;

        if (reasm_len + inner_len > SELD_TLS_REASM_CAP) { hs_err = -11; break; }
        memcpy(reasm_buf + reasm_len, conn->rx_decrypted_buf, inner_len);
        reasm_len += inner_len;

        size_t pos = 0;
        while (pos + 4 <= reasm_len) {
            uint8_t mtype = reasm_buf[pos];
            uint32_t mlen = ((uint32_t)reasm_buf[pos + 1] << 16) |
                            ((uint32_t)reasm_buf[pos + 2] << 8)  |
                            (uint32_t)reasm_buf[pos + 3];
            if (pos + 4 + mlen > reasm_len) {
                break; /* Incomplete handshake message, await next record */
            }

            if (mtype == 11) { /* Certificate (0x0B) */
                if (cert_received) { hs_err = -15; break; }
                if (parse_server_certificate(conn, reasm_buf + pos + 4, mlen, &leaf_pk) != 0) {
                    hs_err = -14;
                    break;
                }
                cert_received = 1;
            } else if (mtype == 15) { /* CertificateVerify (0x0F) */
                if (!cert_received || cert_verify_received || leaf_pk.type == SELD_PK_NONE) {
                    hs_err = -15;
                    break;
                }
                if (mlen < 4) { hs_err = -16; break; }
                const uint8_t* cv_body = reasm_buf + pos + 4;
                uint16_t sig_scheme = (uint16_t)(((uint16_t)cv_body[0] << 8) | cv_body[1]);
                uint16_t sig_len = (uint16_t)(((uint16_t)cv_body[2] << 8) | cv_body[3]);
                if (4U + (uint32_t)sig_len != mlen) {
                    hs_err = -16;
                    break;
                }

                /* RFC 8446 Section 4.4.3: Signature over 64*0x20 || context || 0x00 || Transcript-Hash */
                struct sha256_ctx tr_pre_cv;
                memcpy(&tr_pre_cv, &tr_ctx, sizeof(tr_ctx));
                uint8_t tr_hash_pre_cv[32];
                sha256_final(&tr_pre_cv, tr_hash_pre_cv);

                uint8_t cv_content[64 + 34 + 32];
                memset(cv_content, 0x20, 64);
                static const char cv_ctx_str[] = "TLS 1.3, server CertificateVerify";
                memcpy(cv_content + 64, cv_ctx_str, 33);
                cv_content[64 + 33] = 0x00;
                memcpy(cv_content + 64 + 34, tr_hash_pre_cv, 32);

                if (seld_crypto_verify_tls13_cv(&leaf_pk, sig_scheme, cv_content, sizeof(cv_content),
                                                cv_body + 4, sig_len) != 0) {
                    hs_err = -16; /* Cryptographic CertificateVerify signature verification failed! */
                    break;
                }
                cert_verify_received = 1;
            } else if (mtype == 20) { /* Finished (0x14) */
                if (!cert_received || !cert_verify_received) {
                    hs_err = -13;
                    break;
                }

                int is_onion = (strstr(conn->sni_host, ".onion") != NULL);
                if (!conn->peer_cert.valid || !conn->peer_cert.domain_matched || (!conn->peer_cert.ca_verified && !is_onion)) {
                    hs_err = -14; /* TLS_ERR_CERT_UNTRUSTED_OR_MITM */
                    break;
                }

                struct sha256_ctx tr_pre_fin;
                memcpy(&tr_pre_fin, &tr_ctx, sizeof(tr_ctx));
                uint8_t tr_hash_pre_fin[32];
                sha256_final(&tr_pre_fin, tr_hash_pre_fin);

                uint8_t s_fin_key[32];
                seld_hkdf_expand_label(server_hs_secret, "finished", NULL, 0, 32, s_fin_key);

                uint8_t s_fin_expected[32];
                seld_hmac_sha256(s_fin_key, 32, tr_hash_pre_fin, 32, s_fin_expected);
                seld_explicit_bzero(s_fin_key, sizeof(s_fin_key));

                if (mlen != 32 || memcmp(reasm_buf + pos + 4, s_fin_expected, 32) != 0) {
                    hs_err = -12;
                    break;
                }

                sha256_update(&tr_ctx, reasm_buf + pos, 4 + mlen);
                server_finished_done = 1;
                pos += 4 + mlen;
                break;
            }

            sha256_update(&tr_ctx, reasm_buf + pos, 4 + mlen);
            pos += 4 + mlen;
        }

        if (hs_err != 0) break;

        if (pos > 0) {
            if (pos < reasm_len) {
                memmove(reasm_buf, reasm_buf + pos, reasm_len - pos);
                reasm_len -= pos;
            } else {
                reasm_len = 0;
            }
        }
    }

    seld_explicit_bzero(reasm_buf, SELD_TLS_REASM_CAP);
    free(reasm_buf);
    seld_explicit_bzero(&leaf_pk, sizeof(leaf_pk));

    if (hs_err != 0) {
        seld_explicit_bzero(priv_key, sizeof(priv_key));
        seld_explicit_bzero(shared_secret, sizeof(shared_secret));
        seld_explicit_bzero(handshake_secret, sizeof(handshake_secret));
        seld_explicit_bzero(client_hs_secret, sizeof(client_hs_secret));
        seld_explicit_bzero(server_hs_secret, sizeof(server_hs_secret));
        seld_explicit_bzero(c_hs_key, sizeof(c_hs_key));
        seld_explicit_bzero(s_hs_key, sizeof(s_hs_key));
        return hs_err;
    }

    /* 6. Compute Client Finished */
    uint8_t tr_hash_server_finished[32];
    memcpy(&tr_copy, &tr_ctx, sizeof(tr_ctx));
    sha256_final(&tr_copy, tr_hash_server_finished);

    uint8_t c_fin_key[32];
    seld_hkdf_expand_label(client_hs_secret, "finished", NULL, 0, 32, c_fin_key);

    uint8_t c_fin_verify[32];
    seld_hmac_sha256(c_fin_key, 32, tr_hash_server_finished, 32, c_fin_verify);

    uint8_t c_fin_msg[36];
    c_fin_msg[0] = 0x14;
    c_fin_msg[1] = 0x00; c_fin_msg[2] = 0x00; c_fin_msg[3] = 0x20;
    memcpy(c_fin_msg + 4, c_fin_verify, 32);

    uint8_t c_fin_plain[37];
    memcpy(c_fin_plain, c_fin_msg, 36);
    c_fin_plain[36] = 0x16;

    uint8_t c_fin_hdr[5] = {0x17, 0x03, 0x03, 0x00, (uint8_t)(37 + 16)};
    uint8_t c_fin_nonce[12];
    make_nonce(c_hs_iv, 0, c_fin_nonce);

    uint8_t c_fin_enc[37 + 16];
    seld_aes128_gcm_encrypt(c_hs_key, c_fin_nonce, c_fin_hdr, 5, c_fin_plain, 37, c_fin_enc, c_fin_enc + 37);

    uint8_t flight[70];
    static const uint8_t ccs[6] = {0x14, 0x03, 0x03, 0x00, 0x01, 0x01};
    memcpy(flight, ccs, 6);
    memcpy(flight + 6, c_fin_hdr, 5);
    memcpy(flight + 6 + 5, c_fin_enc, 37 + 16);
    if (tcp_send_exact(tcp_sock, flight, 6 + 5 + 37 + 16) != 0) {
        seld_explicit_bzero(priv_key, sizeof(priv_key));
        return -2;
    }

    /* 7. Derive Application Traffic Keys */
    uint8_t derived_hs[32];
    seld_tls13_derive_secret(handshake_secret, "derived", empty_hash, derived_hs);

    uint8_t master_secret[32];
    seld_hkdf_extract(derived_hs, 32, zero32, 32, master_secret);

    uint8_t c_app_secret[32];
    uint8_t s_app_secret[32];
    seld_tls13_derive_secret(master_secret, "c ap traffic", tr_hash_server_finished, c_app_secret);
    seld_tls13_derive_secret(master_secret, "s ap traffic", tr_hash_server_finished, s_app_secret);

    seld_hkdf_expand_label(c_app_secret, "key", NULL, 0, 16, conn->client_app_key);
    seld_hkdf_expand_label(c_app_secret, "iv", NULL, 0, 12, conn->client_app_iv);
    seld_hkdf_expand_label(s_app_secret, "key", NULL, 0, 16, conn->server_app_key);
    seld_hkdf_expand_label(s_app_secret, "iv", NULL, 0, 12, conn->server_app_iv);

    conn->client_seq = 0;
    conn->server_seq = 0;

    /* OpSec Zeroization: Volatile wipe of all ephemeral handshake secrets */
    seld_explicit_bzero(priv_key, sizeof(priv_key));
    seld_explicit_bzero(pub_key, sizeof(pub_key));
    seld_explicit_bzero(shared_secret, sizeof(shared_secret));
    seld_explicit_bzero(client_random, sizeof(client_random));
    seld_explicit_bzero(session_id, sizeof(session_id));
    seld_explicit_bzero(early_secret, sizeof(early_secret));
    seld_explicit_bzero(derived_early, sizeof(derived_early));
    seld_explicit_bzero(handshake_secret, sizeof(handshake_secret));
    seld_explicit_bzero(server_hs_secret, sizeof(server_hs_secret));
    seld_explicit_bzero(client_hs_secret, sizeof(client_hs_secret));
    seld_explicit_bzero(derived_hs, sizeof(derived_hs));
    seld_explicit_bzero(master_secret, sizeof(master_secret));
    seld_explicit_bzero(c_app_secret, sizeof(c_app_secret));
    seld_explicit_bzero(s_app_secret, sizeof(s_app_secret));
    seld_explicit_bzero(c_hs_key, sizeof(c_hs_key));
    seld_explicit_bzero(s_hs_key, sizeof(s_hs_key));
    seld_explicit_bzero(c_hs_iv, sizeof(c_hs_iv));
    seld_explicit_bzero(s_hs_iv, sizeof(s_hs_iv));
    seld_explicit_bzero(c_fin_key, sizeof(c_fin_key));
    seld_explicit_bzero(c_fin_verify, sizeof(c_fin_verify));
    seld_explicit_bzero(c_fin_msg, sizeof(c_fin_msg));
    seld_explicit_bzero(c_fin_plain, sizeof(c_fin_plain));

    return 0;
}

int seld_tls_write(struct seld_tls_conn* conn, const void* data, size_t len) {
    if (!conn || conn->sock < 0 || !data || len == 0) return -1;

    size_t offset = 0;
    while (offset < len) {
        if (conn->client_seq == UINT64_MAX) return -3;
        size_t chunk = (len - offset < 16384) ? (len - offset) : 16384;
        memcpy(conn->rx_record_buf, (const uint8_t*)data + offset, chunk);
        conn->rx_record_buf[chunk] = 0x17;

        size_t ct_len = chunk + 1;
        uint16_t rec_len = (uint16_t)(ct_len + 16);
        uint8_t hdr[5] = {0x17, 0x03, 0x03, (uint8_t)(rec_len >> 8), (uint8_t)rec_len};

        uint8_t nonce[12];
        make_nonce(conn->client_app_iv, conn->client_seq++, nonce);

        memcpy(conn->tx_buf, hdr, 5);
        seld_aes128_gcm_encrypt(conn->client_app_key, nonce, hdr, 5,
                                conn->rx_record_buf, ct_len,
                                conn->tx_buf + 5, conn->tx_buf + 5 + ct_len);

        if (tcp_send_exact(conn->sock, conn->tx_buf, 5 + rec_len) != 0) return -2;
        offset += chunk;
    }
    return (int)len;
}

int seld_tls_read(struct seld_tls_conn* conn, void* buf, size_t max_len, uint32_t timeout_ms) {
    if (!conn || conn->sock < 0 || !buf || max_len == 0) return -1;

    if (conn->rx_decrypted_pos < conn->rx_decrypted_len) {
        size_t avail = conn->rx_decrypted_len - conn->rx_decrypted_pos;
        size_t to_copy = (avail < max_len) ? avail : max_len;
        memcpy(buf, conn->rx_decrypted_buf + conn->rx_decrypted_pos, to_copy);
        conn->rx_decrypted_pos += to_copy;
        return (int)to_copy;
    }

    conn->rx_decrypted_pos = 0;
    conn->rx_decrypted_len = 0;

    for (int ctrl_records = 0; ctrl_records < 32; ctrl_records++) {
        uint8_t hdr[5];
        int rlen = read_tls_record(conn->sock, hdr, conn->rx_record_buf, sizeof(conn->rx_record_buf), timeout_ms);
        if (rlen <= 0) return rlen;

        if (hdr[0] != 0x17 || rlen < 16) return -2;
        if (conn->server_seq == UINT64_MAX) return -3;

        size_t ct_len = (size_t)rlen - 16;
        uint8_t tag[16];
        memcpy(tag, conn->rx_record_buf + ct_len, 16);

        uint8_t nonce[12];
        make_nonce(conn->server_app_iv, conn->server_seq++, nonce);

        if (seld_aes128_gcm_decrypt(conn->server_app_key, nonce, hdr, 5,
                                    conn->rx_record_buf, ct_len, tag,
                                    conn->rx_decrypted_buf) != 0) {
            return -3;
        }

        size_t pad = 0;
        while (pad < ct_len && conn->rx_decrypted_buf[ct_len - 1 - pad] == 0) pad++;
        if (pad >= ct_len) return -4;

        uint8_t inner_type = conn->rx_decrypted_buf[ct_len - 1 - pad];
        size_t inner_len = ct_len - 1 - pad;

        if (inner_type == 0x15) { /* TLS Alert (close_notify) */
            return 0;
        }
        if (inner_type == 0x16) { /* Post-handshake message (e.g. NewSessionTicket) */
            continue;
        }
        if (inner_type != 0x17) {
            return -4;
        }
        if (inner_len == 0) {
            continue;
        }

        size_t to_copy = (inner_len < max_len) ? inner_len : max_len;
        memcpy(buf, conn->rx_decrypted_buf, to_copy);

        if (inner_len > to_copy) {
            conn->rx_decrypted_len = inner_len;
            conn->rx_decrypted_pos = to_copy;
        }

        return (int)to_copy;
    }

    return -5; /* Too many consecutive non-application records */
}

int seld_tls_get_cert(const struct seld_tls_conn* conn, struct seld_tls_cert* out_cert) {
    if (!conn || !out_cert) return -1;
    memcpy(out_cert, &conn->peer_cert, sizeof(struct seld_tls_cert));
    return conn->peer_cert.valid ? 0 : -2;
}

void seld_tls_close(struct seld_tls_conn* conn) {
    if (!conn) return;
    if (conn->sock >= 0) {
        seld_tcp_close(conn->sock);
        conn->sock = -1;
    }
    seld_explicit_bzero(conn->client_app_key, sizeof(conn->client_app_key));
    seld_explicit_bzero(conn->server_app_key, sizeof(conn->server_app_key));
    seld_explicit_bzero(conn->client_app_iv, sizeof(conn->client_app_iv));
    seld_explicit_bzero(conn->server_app_iv, sizeof(conn->server_app_iv));
    seld_explicit_bzero(conn->rx_record_buf, sizeof(conn->rx_record_buf));
    seld_explicit_bzero(conn->rx_decrypted_buf, sizeof(conn->rx_decrypted_buf));
    seld_explicit_bzero(conn->tx_buf, sizeof(conn->tx_buf));
}
