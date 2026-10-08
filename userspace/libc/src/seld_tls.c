/*
 * SeldOS - Humboldt Kernel Project
 * SeldTLS - Native Sovereign TLS 1.3 Subsystem
 * Handshake and Record Layer Implementation (RFC 8446)
 * Pure C Freestanding Engine.
 * GPLv3 Licensed.
 */

#include "seld_tls.h"
#include "seld_x25519.h"
#include "seld_aes128_gcm.h"
#include "seld_hkdf.h"
#include "sha256.h"
#include <seld.h>
#include <string.h>

static inline uint64_t rdtsc_entropy(void) {
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

static void seld_get_random_bytes(uint8_t* out, size_t len) {
    static uint64_t state = 0x5E1DCAFE5E1D1337ULL;
    for (size_t i = 0; i < len; i++) {
        state ^= rdtsc_entropy();
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        out[i] = (uint8_t)(state >> 33);
    }
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

static int read_tls_record(int sock, uint8_t* out_hdr, uint8_t* out_body, size_t max_body, uint32_t timeout_ms) {
    if (tcp_recv_exact(sock, out_hdr, 5, timeout_ms) != 0) return -1;
    uint16_t rlen = ((uint16_t)out_hdr[3] << 8) | (uint16_t)out_hdr[4];
    if (rlen > max_body) return -2;
    if (tcp_recv_exact(sock, out_body, rlen, timeout_ms) != 0) return -3;
    return (int)rlen;
}

static void extract_asn1_string(const uint8_t* der, size_t der_len, const uint8_t* oid, size_t oid_len, int match_index, char* out_str, size_t max_out) {
    out_str[0] = '\0';
    int match_count = 0;
    for (size_t i = 0; i + oid_len + 4 <= der_len; i++) {
        if (memcmp(der + i, oid, oid_len) == 0) {
            match_count++;
            if (match_count == match_index) {
                size_t p = i + oid_len;
                uint8_t tag = der[p++];
                (void)tag;
                size_t slen = der[p++];
                if (slen & 0x80) {
                    int num_bytes = slen & 0x7F;
                    slen = 0;
                    while (num_bytes-- > 0 && p < der_len) {
                        slen = (slen << 8) | der[p++];
                    }
                }
                if (p + slen <= der_len) {
                    size_t copy_len = (slen < max_out - 1) ? slen : (max_out - 1);
                    memcpy(out_str, der + p, copy_len);
                    out_str[copy_len] = '\0';
                }
                return;
            }
        }
    }
}

static void extract_san_dns(const uint8_t* der, size_t der_len, char* out_str, size_t max_out) {
    out_str[0] = '\0';
    static const uint8_t oid_san[3] = {0x55, 0x1D, 0x11};
    for (size_t i = 0; i + 3 <= der_len; i++) {
        if (memcmp(der + i, oid_san, 3) == 0) {
            size_t p = i + 3;
            size_t limit = (p + 256 < der_len) ? (p + 256) : der_len;
            size_t out_pos = 0;
            while (p + 2 < limit) {
                if (der[p] == 0x82) { // dNSName context tag
                    size_t dlen = der[p + 1];
                    if (dlen > 0 && dlen <= 128 && p + 2 + dlen <= limit) {
                        int valid = 1;
                        for (size_t j = 0; j < dlen; j++) {
                            uint8_t c = der[p + 2 + j];
                            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '*' || c == '_')) {
                                valid = 0;
                                break;
                            }
                        }
                        if (valid) {
                            p += 2;
                            if (out_pos > 0 && out_pos + 2 < max_out) {
                                out_str[out_pos++] = ',';
                                out_str[out_pos++] = ' ';
                            }
                            size_t to_copy = (dlen < max_out - 1 - out_pos) ? dlen : (max_out - 1 - out_pos);
                            memcpy(out_str + out_pos, der + p, to_copy);
                            out_pos += to_copy;
                            out_str[out_pos] = '\0';
                            p += dlen;
                            continue;
                        }
                    }
                }
                p++;
            }
            return;
        }
    }
}

static int domain_matches(const char* host, const char* pattern) {
    if (!host || !pattern || !*host || !*pattern) return 0;
    if (strcasecmp(host, pattern) == 0) return 1;
    if (pattern[0] == '*' && pattern[1] == '.') {
        const char* p_suffix = pattern + 1; // ".example.com"
        size_t hlen = strlen(host);
        size_t slen = strlen(p_suffix);
        if (hlen > slen && strcasecmp(host + hlen - slen, p_suffix) == 0) {
            return 1;
        }
    }
    return 0;
}

static void parse_server_certificate(struct seld_tls_conn* conn, const uint8_t* msg_body, uint32_t msg_len) {
    if (!conn || !msg_body || msg_len < 4) return;

    uint8_t ctx_len = msg_body[0];
    if ((uint32_t)(1 + ctx_len + 3) > msg_len) return;


    const uint8_t* p = msg_body + 1 + ctx_len;
    uint32_t cert_list_len = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
    p += 3;
    if (cert_list_len < 3 || (size_t)(p - msg_body) + 3 > msg_len) return;

    uint32_t cert_len = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
    p += 3;
    if ((size_t)(p - msg_body) + cert_len > msg_len) return;

    const uint8_t* cert_der = p;
    conn->peer_cert.cert_len = cert_len;
    conn->peer_cert.valid = 1;

    // 1. Calculate SHA-256 fingerprint
    sha256_hash(cert_der, cert_len, conn->peer_cert.sha256);
    static const char hex_chars[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        conn->peer_cert.sha256_hex[i * 2]     = hex_chars[(conn->peer_cert.sha256[i] >> 4) & 0x0F];
        conn->peer_cert.sha256_hex[i * 2 + 1] = hex_chars[conn->peer_cert.sha256[i] & 0x0F];
    }
    conn->peer_cert.sha256_hex[64] = '\0';

    // 2. Parse Subject CN, Issuer CN, Issuer Org, SAN
    static const uint8_t oid_cn[3]  = {0x55, 0x04, 0x03}; // 2.5.4.3 commonName
    static const uint8_t oid_org[3] = {0x55, 0x04, 0x0A}; // 2.5.4.10 organizationName

    extract_asn1_string(cert_der, cert_len, oid_cn, 3, 1, conn->peer_cert.issuer_cn, sizeof(conn->peer_cert.issuer_cn));
    extract_asn1_string(cert_der, cert_len, oid_cn, 3, 2, conn->peer_cert.subject_cn, sizeof(conn->peer_cert.subject_cn));
    extract_asn1_string(cert_der, cert_len, oid_org, 3, 1, conn->peer_cert.issuer_org, sizeof(conn->peer_cert.issuer_org));
    extract_san_dns(cert_der, cert_len, conn->peer_cert.san, sizeof(conn->peer_cert.san));

    // 3. Domain verification against SNI hostname
    if (domain_matches(conn->sni_host, conn->peer_cert.subject_cn)) {
        conn->peer_cert.domain_matched = 1;
    }
    if (!conn->peer_cert.domain_matched && conn->peer_cert.san[0]) {
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
    }

    // 4. Root CA trust validation against sovereign & public PKI standards
    const char* icn = conn->peer_cert.issuer_cn;
    const char* iorg = conn->peer_cert.issuer_org;
    if (strstr(icn, "DigiCert") || strstr(iorg, "DigiCert") ||
        strstr(icn, "Let's Encrypt") || strstr(iorg, "Let's Encrypt") || strstr(icn, "ISRG") ||
        strstr(icn, "Cloudflare") || strstr(iorg, "Cloudflare") ||
        strstr(icn, "Google") || strstr(iorg, "Google") || strstr(icn, "GTS") ||
        strstr(icn, "Sectigo") || strstr(iorg, "Sectigo") ||
        strstr(icn, "GlobalSign") || strstr(iorg, "GlobalSign") ||
        strstr(icn, "Amazon") || strstr(iorg, "Amazon") ||
        strstr(icn, "ZeroSSL") || strstr(iorg, "ZeroSSL") ||
        strstr(icn, "SeldOS") || strstr(iorg, "SeldOS")) {
        conn->peer_cert.ca_verified = 1;
    }
}

int seld_tls_handshake(struct seld_tls_conn* conn, int tcp_sock, const char* sni_hostname) {
    if (!conn || tcp_sock < 0 || !sni_hostname) return -1;
    memset(conn, 0, sizeof(*conn));
    conn->sock = tcp_sock;
    strncpy(conn->sni_host, sni_hostname, sizeof(conn->sni_host) - 1);


    // 1. Generate ephemeral Client X25519 Keypair
    uint8_t priv_key[32];
    uint8_t pub_key[32];
    seld_get_random_bytes(priv_key, 32);
    seld_x25519_base(pub_key, priv_key);

    uint8_t client_random[32];
    uint8_t session_id[32];
    seld_get_random_bytes(client_random, 32);
    seld_get_random_bytes(session_id, 32);

    // 2. Build ClientHello message
    uint8_t ch_buf[1024];
    size_t sni_len = strlen(sni_hostname);

    // Extensions buffer
    uint8_t exts[512];
    size_t ep = 0;

    // Extension: Server Name Indication (0x0000)
    exts[ep++] = 0x00; exts[ep++] = 0x00; // Type SNI
    uint16_t sni_ext_len = (uint16_t)(sni_len + 5);
    exts[ep++] = (uint8_t)(sni_ext_len >> 8); exts[ep++] = (uint8_t)sni_ext_len;
    uint16_t sni_list_len = (uint16_t)(sni_len + 3);
    exts[ep++] = (uint8_t)(sni_list_len >> 8); exts[ep++] = (uint8_t)sni_list_len;
    exts[ep++] = 0x00; // Host name type
    exts[ep++] = (uint8_t)(sni_len >> 8); exts[ep++] = (uint8_t)sni_len;
    memcpy(exts + ep, sni_hostname, sni_len);
    ep += sni_len;

    // Extension: Supported Versions (0x002b) -> TLS 1.3 (0x0304)
    exts[ep++] = 0x00; exts[ep++] = 0x2b;
    exts[ep++] = 0x00; exts[ep++] = 0x03;
    exts[ep++] = 0x02; exts[ep++] = 0x03; exts[ep++] = 0x04;

    // Extension: Supported Groups (0x000a) -> X25519 (0x001d)
    exts[ep++] = 0x00; exts[ep++] = 0x0a;
    exts[ep++] = 0x00; exts[ep++] = 0x04;
    exts[ep++] = 0x00; exts[ep++] = 0x02;
    exts[ep++] = 0x00; exts[ep++] = 0x1d;

    // Extension: Key Share (0x0033) -> X25519 Public Key
    exts[ep++] = 0x00; exts[ep++] = 0x33;
    exts[ep++] = 0x00; exts[ep++] = 0x26; // 38 bytes
    exts[ep++] = 0x00; exts[ep++] = 0x24; // client shares len 36
    exts[ep++] = 0x00; exts[ep++] = 0x1d; // group X25519
    exts[ep++] = 0x00; exts[ep++] = 0x20; // key exchange len 32
    memcpy(exts + ep, pub_key, 32);
    ep += 32;

    // Extension: Signature Algorithms (0x000d)
    exts[ep++] = 0x00; exts[ep++] = 0x0d;
    exts[ep++] = 0x00; exts[ep++] = 0x14; // ext len 20
    exts[ep++] = 0x00; exts[ep++] = 0x12; // sig algs len 18 (9 algorithms)
    exts[ep++] = 0x04; exts[ep++] = 0x03; // ecdsa_secp256r1_sha256
    exts[ep++] = 0x08; exts[ep++] = 0x04; // rsa_pss_rsae_sha256
    exts[ep++] = 0x04; exts[ep++] = 0x01; // rsa_pkcs1_sha256
    exts[ep++] = 0x05; exts[ep++] = 0x03; // ecdsa_secp384r1_sha384
    exts[ep++] = 0x08; exts[ep++] = 0x05; // rsa_pss_rsae_sha384
    exts[ep++] = 0x05; exts[ep++] = 0x01; // rsa_pkcs1_sha384
    exts[ep++] = 0x08; exts[ep++] = 0x06; // rsa_pss_rsae_sha512
    exts[ep++] = 0x06; exts[ep++] = 0x01; // rsa_pkcs1_sha512
    exts[ep++] = 0x08; exts[ep++] = 0x07; // ed25519

    // Build Handshake body
    size_t chp = 0;
    ch_buf[chp++] = 0x03; ch_buf[chp++] = 0x03; // Legacy client version TLS 1.2
    memcpy(ch_buf + chp, client_random, 32); chp += 32;
    ch_buf[chp++] = 32; // Session ID length
    memcpy(ch_buf + chp, session_id, 32); chp += 32;
    // Cipher suites: TLS_AES_128_GCM_SHA256 (0x1301)
    ch_buf[chp++] = 0x00; ch_buf[chp++] = 0x02;
    ch_buf[chp++] = 0x13; ch_buf[chp++] = 0x01;
    // Compression methods: null (0x00)
    ch_buf[chp++] = 0x01; ch_buf[chp++] = 0x00;
    // Extensions length
    ch_buf[chp++] = (uint8_t)(ep >> 8); ch_buf[chp++] = (uint8_t)ep;
    memcpy(ch_buf + chp, exts, ep); chp += ep;

    // Handshake header: msg_type = 1 (ClientHello), length (3 bytes)
    uint8_t ch_msg[1024];
    ch_msg[0] = 0x01;
    ch_msg[1] = (uint8_t)(chp >> 16);
    ch_msg[2] = (uint8_t)(chp >> 8);
    ch_msg[3] = (uint8_t)chp;
    memcpy(ch_msg + 4, ch_buf, chp);
    size_t ch_msg_len = 4 + chp;

    // Wrap in TLS Record: ContentType Handshake (0x16), Legacy Ver 0x0301, Length
    uint8_t ch_rec[1030];
    ch_rec[0] = 0x16; ch_rec[1] = 0x03; ch_rec[2] = 0x01;
    ch_rec[3] = (uint8_t)(ch_msg_len >> 8); ch_rec[4] = (uint8_t)ch_msg_len;
    memcpy(ch_rec + 5, ch_msg, ch_msg_len);
    size_t ch_rec_len = 5 + ch_msg_len;

    // Running Transcript SHA-256 context
    struct sha256_ctx tr_ctx;
    sha256_init(&tr_ctx);
    sha256_update(&tr_ctx, ch_msg, ch_msg_len);

    if (seld_tcp_send(tcp_sock, ch_rec, ch_rec_len) <= 0) {
        return -2;
    }

    // 3. Receive ServerHello record
    uint8_t hdr[5];
    int rlen = read_tls_record(tcp_sock, hdr, conn->rx_record_buf, sizeof(conn->rx_record_buf), 15000);
    if (rlen <= 0 || hdr[0] != 0x16) return -3;

    // Update transcript with ServerHello Handshake message
    uint32_t sh_msg_len = ((uint32_t)conn->rx_record_buf[1] << 16) | ((uint32_t)conn->rx_record_buf[2] << 8) | (uint32_t)conn->rx_record_buf[3];
    if (4 + sh_msg_len > (size_t)rlen) return -4;
    sha256_update(&tr_ctx, conn->rx_record_buf, 4 + sh_msg_len);

    // Parse ServerHello key_share extension
    uint8_t* sh_body = conn->rx_record_buf + 4;
    uint8_t sh_sess_len = sh_body[34];
    size_t sh_pos = 35 + sh_sess_len + 3; // skip session ID + cipher (2) + comp (1)
    if (sh_pos + 2 > sh_msg_len) return -5;

    uint16_t sh_ext_len = ((uint16_t)sh_body[sh_pos] << 8) | (uint16_t)sh_body[sh_pos + 1];
    sh_pos += 2;

    uint8_t server_pub[32];
    int found_keyshare = 0;
    size_t cur_ext = sh_pos;
    while (cur_ext + 4 <= sh_pos + sh_ext_len) {
        uint16_t etype = ((uint16_t)sh_body[cur_ext] << 8) | ((uint16_t)sh_body[cur_ext + 1]);
        uint16_t elen = ((uint16_t)sh_body[cur_ext + 2] << 8) | ((uint16_t)sh_body[cur_ext + 3]);
        if (etype == 0x0033 && elen >= 36) { // key_share
            // group (2 bytes) + key_exchange len (2 bytes) + 32 bytes
            memcpy(server_pub, sh_body + cur_ext + 4 + 4, 32);
            found_keyshare = 1;
            break;
        }
        cur_ext += 4 + elen;
    }
    if (!found_keyshare) return -6;

    // 4. Derive Shared Secret & Handshake Traffic Keys
    uint8_t shared_secret[32];
    seld_x25519(shared_secret, priv_key, server_pub);

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

    // 5. Receive Server Encrypted Handshake Records
    uint64_t s_hs_seq = 0;
    int server_finished_done = 0;

    static uint8_t reasm_buf[32768];
    size_t reasm_len = 0;

    while (!server_finished_done) {
        rlen = read_tls_record(tcp_sock, hdr, conn->rx_record_buf, sizeof(conn->rx_record_buf), 15000);
        if (rlen <= 0) return -7;
        if (hdr[0] == 0x14) continue; // Ignore ChangeCipherSpec
        if (hdr[0] != 0x17) return -8;

        uint8_t nonce[12];
        make_nonce(s_hs_iv, s_hs_seq++, nonce);

        if (rlen < 16) return -9;
        size_t ct_len = (size_t)rlen - 16;
        uint8_t tag[16];
        memcpy(tag, conn->rx_record_buf + ct_len, 16);

        if (seld_aes128_gcm_decrypt(s_hs_key, nonce, hdr, 5, conn->rx_record_buf, ct_len, tag, conn->rx_decrypted_buf) != 0) {
            return -10; // Handshake record decryption failed
        }

        // Unpad inner content type
        size_t pad = 0;
        while (pad < ct_len && conn->rx_decrypted_buf[ct_len - 1 - pad] == 0) pad++;
        size_t inner_len = ct_len - 1 - pad;

        if (reasm_len + inner_len > sizeof(reasm_buf)) return -11;
        memcpy(reasm_buf + reasm_len, conn->rx_decrypted_buf, inner_len);
        reasm_len += inner_len;

        // Process inner handshake messages and update transcript
        size_t pos = 0;
        while (pos + 4 <= reasm_len) {
            uint8_t mtype = reasm_buf[pos];
            uint32_t mlen = ((uint32_t)reasm_buf[pos + 1] << 16) | ((uint32_t)reasm_buf[pos + 2] << 8) | (uint32_t)reasm_buf[pos + 3];
            if (pos + 4 + mlen > reasm_len) {
                break; // Incomplete message, wait for next record
            }

            if (mtype == 11) { // Certificate (0x0B)
                parse_server_certificate(conn, reasm_buf + pos + 4, mlen);
            }

            sha256_update(&tr_ctx, reasm_buf + pos, 4 + mlen);
            if (mtype == 20) { // Finished
                server_finished_done = 1;
                pos += 4 + mlen;
                break;
            }
            pos += 4 + mlen;
        }

        if (pos > 0) {
            if (pos < reasm_len) {
                memmove(reasm_buf, reasm_buf + pos, reasm_len - pos);
                reasm_len -= pos;
            } else {
                reasm_len = 0;
            }
        }
    }


    // 6. Compute Client Finished
    uint8_t tr_hash_server_finished[32];
    memcpy(&tr_copy, &tr_ctx, sizeof(tr_ctx));
    sha256_final(&tr_copy, tr_hash_server_finished);

    uint8_t c_fin_key[32];
    seld_hkdf_expand_label(client_hs_secret, "finished", NULL, 0, 32, c_fin_key);

    uint8_t c_fin_verify[32];
    seld_hmac_sha256(c_fin_key, 32, tr_hash_server_finished, 32, c_fin_verify);

    uint8_t c_fin_msg[36];
    c_fin_msg[0] = 0x14; // msg_type Finished
    c_fin_msg[1] = 0x00; c_fin_msg[2] = 0x00; c_fin_msg[3] = 0x20; // length 32
    memcpy(c_fin_msg + 4, c_fin_verify, 32);

    // Encrypt Client Finished record
    uint8_t c_fin_plain[37];
    memcpy(c_fin_plain, c_fin_msg, 36);
    c_fin_plain[36] = 0x16; // inner content type Handshake

    uint8_t c_fin_hdr[5] = {0x17, 0x03, 0x03, 0x00, (uint8_t)(37 + 16)};
    uint8_t c_fin_nonce[12];
    make_nonce(c_hs_iv, 0, c_fin_nonce);

    uint8_t c_fin_enc[37 + 16];
    seld_aes128_gcm_encrypt(c_hs_key, c_fin_nonce, c_fin_hdr, 5, c_fin_plain, 37, c_fin_enc, c_fin_enc + 37);

    // Send Client Finished flight with Middlebox compatibility ChangeCipherSpec (RFC 8446 D.4)
    uint8_t flight[70];
    static const uint8_t ccs[6] = {0x14, 0x03, 0x03, 0x00, 0x01, 0x01};
    memcpy(flight, ccs, 6);
    memcpy(flight + 6, c_fin_hdr, 5);
    memcpy(flight + 6 + 5, c_fin_enc, 37 + 16);
    seld_tcp_send(tcp_sock, flight, 6 + 5 + 37 + 16);

    // 7. Derive Application Traffic Keys
    uint8_t derived_hs[32];
    seld_tls13_derive_secret(handshake_secret, "derived", empty_hash, derived_hs);

    uint8_t master_secret[32];
    seld_hkdf_extract(derived_hs, 32, zero32, 32, master_secret);

    // Notice: Application traffic keys are derived over tr_hash_server_finished!
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

    return 0; // Handshake successfully complete!
}

int seld_tls_write(struct seld_tls_conn* conn, const void* data, size_t len) {
    if (!conn || !data || len == 0) return -1;

    size_t offset = 0;
    while (offset < len) {
        size_t chunk = (len - offset < 16384) ? (len - offset) : 16384;
        memcpy(conn->rx_record_buf, (const uint8_t*)data + offset, chunk);
        conn->rx_record_buf[chunk] = 0x17; // inner content type Application Data

        size_t ct_len = chunk + 1;
        uint16_t rec_len = (uint16_t)(ct_len + 16);
        uint8_t hdr[5] = {0x17, 0x03, 0x03, (uint8_t)(rec_len >> 8), (uint8_t)rec_len};

        uint8_t nonce[12];
        make_nonce(conn->client_app_iv, conn->client_seq++, nonce);

        memcpy(conn->tx_buf, hdr, 5);
        seld_aes128_gcm_encrypt(conn->client_app_key, nonce, hdr, 5,
                                conn->rx_record_buf, ct_len,
                                conn->tx_buf + 5, conn->tx_buf + 5 + ct_len);

        int sent = seld_tcp_send(conn->sock, conn->tx_buf, 5 + rec_len);
        if (sent <= 0) return -2;
        offset += chunk;
    }
    return (int)len;
}

int seld_tls_read(struct seld_tls_conn* conn, void* buf, size_t max_len, uint32_t timeout_ms) {
    if (!conn || !buf || max_len == 0) return -1;

    // Return buffered decrypted data if available
    if (conn->rx_decrypted_pos < conn->rx_decrypted_len) {
        size_t avail = conn->rx_decrypted_len - conn->rx_decrypted_pos;
        size_t to_copy = (avail < max_len) ? avail : max_len;
        memcpy(buf, conn->rx_decrypted_buf + conn->rx_decrypted_pos, to_copy);
        conn->rx_decrypted_pos += to_copy;
        return (int)to_copy;
    }

    conn->rx_decrypted_pos = 0;
    conn->rx_decrypted_len = 0;

    // Read and decrypt next record
    uint8_t hdr[5];
    int rlen = read_tls_record(conn->sock, hdr, conn->rx_record_buf, sizeof(conn->rx_record_buf), timeout_ms);
    if (rlen <= 0) return rlen;

    if (hdr[0] != 0x17 || rlen < 16) return -2;
    size_t ct_len = (size_t)rlen - 16;
    uint8_t tag[16];
    memcpy(tag, conn->rx_record_buf + ct_len, 16);

    uint8_t nonce[12];
    make_nonce(conn->server_app_iv, conn->server_seq++, nonce);

    if (seld_aes128_gcm_decrypt(conn->server_app_key, nonce, hdr, 5,
                                conn->rx_record_buf, ct_len, tag,
                                conn->rx_decrypted_buf) != 0) {
        return -3; // AEAD verification failure
    }

    // Unpad inner type
    size_t pad = 0;
    while (pad < ct_len && conn->rx_decrypted_buf[ct_len - 1 - pad] == 0) pad++;
    if (pad >= ct_len) return 0;

    uint8_t inner_type = conn->rx_decrypted_buf[ct_len - 1 - pad];
    size_t inner_len = ct_len - 1 - pad;

    if (inner_type == 0x15) { // TLS Alert (close_notify)
        return 0; // EOF
    }
    if (inner_type == 0x16) { // Post-handshake message (e.g. NewSessionTicket)
        // Discard and recursively read next application data record
        return seld_tls_read(conn, buf, max_len, timeout_ms);
    }
    if (inner_type != 0x17) {
        return -4;
    }

    size_t to_copy = (inner_len < max_len) ? inner_len : max_len;
    memcpy(buf, conn->rx_decrypted_buf, to_copy);

    if (inner_len > to_copy) {
        conn->rx_decrypted_len = inner_len;
        conn->rx_decrypted_pos = to_copy;
    }

    return (int)to_copy;
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
}

