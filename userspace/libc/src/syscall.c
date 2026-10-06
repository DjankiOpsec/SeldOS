/*
 * SeldOS - Humboldt Kernel Project
 * SNL C Standard Library (libsnl)
 * Fast SYSCALL / SYSRET Assembly Wrappers
 * GPLv3 Licensed.
 */

#include "seld.h"
#include <string.h>

long seld_syscall(long num, long arg1, long arg2, long arg3) {
    long ret;
    register long r10 __asm__("r10") = 0;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10)
        : "rcx", "r11", "memory"
    );
    return ret;
}

long seld_syscall4(long num, long arg1, long arg2, long arg3, long arg4) {
    long ret;
    register long r10 __asm__("r10") = arg4;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10)
        : "rcx", "r11", "memory"
    );
    return ret;
}

void seld_exit(int status) {
    seld_syscall(SYS_EXIT, (long)status, 0, 0);
    while (1) {
        __asm__ volatile ("pause");
    }
}

long seld_write(int fd, const void* buf, size_t count) {
    return seld_syscall(SYS_WRITE, (long)fd, (long)buf, (long)count);
}

long seld_read(int fd, void* buf, size_t count) {
    return seld_syscall(SYS_READ, (long)fd, (long)buf, (long)count);
}

void seld_yield(void) {
    seld_syscall(SYS_YIELD, 0, 0, 0);
}

uint64_t seld_uptime(void) {
    return (uint64_t)seld_syscall(SYS_UPTIME, 0, 0, 0);
}

int seld_getpid(void) {
    return (int)seld_syscall(SYS_GETPID, 0, 0, 0);
}

long seld_verify(long code, long arg) {
    return seld_syscall(SYS_SELD_VERIFY, code, arg, 0);
}

int seld_open(const char* path, int flags, int mode) {
    return (int)seld_syscall(SYS_OPEN, (long)path, (long)flags, (long)mode);
}

int seld_close(int fd) {
    return (int)seld_syscall(SYS_CLOSE, (long)fd, 0, 0);
}

int seld_stat(const char* path, struct seld_stat* st) {
    return (int)seld_syscall(SYS_STAT, (long)path, (long)st, 0);
}

int seld_listdir(const char* path, struct seld_dirent* dirents, size_t max_entries) {
    return (int)seld_syscall(SYS_LISTDIR, (long)path, (long)dirents, (long)max_entries);
}

void* seld_brk(void* addr) {
    return (void*)seld_syscall(SYS_BRK, (long)addr, 0, 0);
}

int seld_spawn(const char* path) {
    char* argv[2] = {(char*)path, NULL};
    return seld_spawnv(path, argv);
}

int seld_spawnv(const char* path, char* const argv[]) {
    return (int)seld_syscall(SYS_SPAWN, (long)path, (long)argv, 0);
}

void seld_sleep(uint64_t ms) {
    seld_syscall(SYS_SLEEP, (long)ms, 0, 0);
}

int seld_unlink(const char* path) {
    return (int)seld_syscall(SYS_UNLINK, (long)path, 0, 0);
}

int seld_gettasks(struct snl_task_info* tasks, size_t max_tasks) {
    return (int)seld_syscall(SYS_GETTASKS, (long)tasks, (long)max_tasks, 0);
}

int seld_meminfo(struct snl_meminfo* info) {
    return (int)seld_syscall(SYS_MEMINFO, (long)info, 0, 0);
}

int seld_clear(void) {
    return (int)seld_syscall(SYS_CLEAR, 0, 0, 0);
}

int seld_set_console_rows(int rows) {
    return (int)seld_syscall(SYS_SET_CONSOLE_ROWS, (long)rows, 0, 0);
}

int seld_readfile(const char* path, void* buf, size_t max_len) {
    return (int)seld_syscall(SYS_READFILE, (long)path, (long)buf, (long)max_len);
}

int seld_writefile(const char* path, const void* buf, size_t len) {
    return (int)seld_syscall(SYS_WRITEFILE, (long)path, (long)buf, (long)len);
}

long seld_lseek(int fd, long offset, int whence) {
    return seld_syscall(SYS_LSEEK, (long)fd, offset, (long)whence);
}

int seld_get_framebuffer(struct seld_fb_info* fb) {
    return (int)seld_syscall(SYS_FRAMEBUFFER, (long)fb, 0, 0);
}

int seld_poll_key(struct seld_kbd_event* ev) {
    return (int)seld_syscall(SYS_POLLKEY, (long)ev, 0, 0);
}

int seld_poll_mouse(struct seld_mouse_event* ev) {
    return (int)seld_syscall(SYS_POLLMOUSE, (long)ev, 0, 0);
}

static const uint16_t s_libc_ona_to_freq[128] = {
        0,    15,    15,    16,    17,    18,    19,    21,
       22,    23,    24,    26,    28,    29,    31,    33,
       35,    37,    39,    41,    44,    46,    49,    52,
       55,    58,    62,    65,    69,    73,    78,    82,
       87,    92,    98,   104,   110,   117,   123,   131,
      139,   147,   156,   165,   175,   185,   196,   208,
      220,   233,   247,   262,   277,   294,   311,   330,
      349,   370,   392,   415,   440,   466,   494,   523,
      554,   587,   622,   659,   698,   740,   784,   831,
      880,   932,   988,  1047,  1109,  1175,  1245,  1319,
     1397,  1480,  1568,  1661,  1760,  1865,  1976,  2093,
     2217,  2349,  2489,  2637,  2794,  2960,  3136,  3322,
     3520,  3729,  3951,  4186,  4435,  4699,  4978,  5274,
     5588,  5920,  6272,  6645,  7040,  7459,  7902,  8372,
     8870,  9397,  9956, 10548, 11175, 11840, 12544, 13290,
    14080, 14917, 15804, 16744, 17740, 18795, 19912, 21096
};

uint32_t seld_ona2freq(int8_t ona) {
    if (ona <= 0) return 0;
    return (uint32_t)s_libc_ona_to_freq[(uint8_t)ona];
}

int8_t seld_freq2ona(uint32_t freq_hz) {
    if (freq_hz == 0) return 0;
    if (freq_hz <= s_libc_ona_to_freq[1]) return 1;
    if (freq_hz >= s_libc_ona_to_freq[127]) return 127;

    int low = 1, high = 127;
    int best_ona = 1;
    uint32_t best_diff = 0xFFFFFFFF;

    while (low <= high) {
        int mid = (low + high) / 2;
        uint32_t mid_f = s_libc_ona_to_freq[mid];
        uint32_t diff = (freq_hz > mid_f) ? (freq_hz - mid_f) : (mid_f - freq_hz);
        if (diff < best_diff) {
            best_diff = diff;
            best_ona = mid;
        }

        if (mid_f == freq_hz) {
            return (int8_t)mid;
        } else if (mid_f < freq_hz) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return (int8_t)best_ona;
}

int seld_beep(uint32_t freq_hz, uint32_t duration_ms) {
    return (int)seld_syscall(SYS_BEEP, (long)freq_hz, (long)duration_ms, 0);
}

int seld_snd(int8_t ona) {
    if (ona <= 0) {
        return (int)seld_syscall(SYS_BEEP, 0, 0, 0);
    }
    uint32_t freq = seld_ona2freq(ona);
    return (int)seld_syscall(SYS_BEEP, (long)freq, 0, 0);
}

int seld_audio_play(const void* samples, size_t len, uint32_t sample_rate) {
    return (int)seld_syscall(SYS_AUDIO_PLAY, (long)samples, (long)len, (long)sample_rate);
}

long seld_ping(void) {
    return seld_syscall(SYS_SELD, 0, 0, 0);
}

int seld_net_info(struct seld_net_info* info) {
    return (int)seld_syscall(SYS_NET_INFO, (long)info, 0, 0);
}

int seld_net_ping(uint32_t ip, uint16_t seq, uint32_t* rtt_ms) {
    return (int)seld_syscall(SYS_NET_PING, (long)ip, (long)seq, (long)rtt_ms);
}

int seld_net_arp(struct seld_arp_entry* entries, size_t max_entries) {
    return (int)seld_syscall(SYS_NET_ARP, (long)entries, (long)max_entries, 0);
}

int seld_net_download(uint32_t ip, uint16_t port, const char* url_path, const char* local_path) {
    return (int)seld_syscall4(SYS_NET_DOWNLOAD, (long)ip, (long)port, (long)url_path, (long)local_path);
}

#include "seld_tls.h"
#include <stdio.h>
#include <stdlib.h>

int seld_https_download(const char* host, const char* path, const char* local_path) {
    if (!host || !path || !local_path) return -1;

    uint32_t ip = 0;
    if (seld_dns_resolve(host, &ip) != 0 || ip == 0) {
        return -2; // DNS resolution failed
    }

    printf("[*] DNS: %s -> %u.%u.%u.%u\n",
           host,
           (unsigned int)(ip & 0xFF),
           (unsigned int)((ip >> 8) & 0xFF),
           (unsigned int)((ip >> 16) & 0xFF),
           (unsigned int)((ip >> 24) & 0xFF));

    int sock = seld_tcp_connect(ip, 443);
    if (sock < 0) return -3; // TCP connection failed

    printf("[*] TCP connected to port 443. Starting sovereign SeldTLS 1.3 handshake...\n");

    struct seld_tls_conn* tls = (struct seld_tls_conn*)malloc(sizeof(struct seld_tls_conn));
    if (!tls) {
        seld_tcp_close(sock);
        return -4;
    }

    int hs = seld_tls_handshake(tls, sock, host);
    if (hs != 0) {
        printf("[-] SeldTLS handshake failed (code=%d)\n", hs);
        seld_tls_close(tls);
        free(tls);
        return -4; // TLS 1.3 handshake failed
    }

    printf("[+] TLS 1.3 handshake negotiated! Cipher: TLS_AES_128_GCM_SHA256\n");
    printf("[*] Fetching %s via encrypted tunnel...\n", path);

    char req[512];
    snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: SeldOS-Native-SeldTLS/1.0\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n\r\n",
        path, host
    );

    if (seld_tls_write(tls, req, strlen(req)) <= 0) {
        seld_tls_close(tls);
        free(tls);
        return -5;
    }

    // Read response body into memory buffer (up to 5 MiB for binaries & WAD assets)
    size_t cap = 5 * 1024 * 1024;
    uint8_t* resp_buf = (uint8_t*)malloc(cap);
    if (!resp_buf) {
        seld_tls_close(tls);
        free(tls);
        return -6;
    }

    size_t total_read = 0;
    while (total_read < cap) {
        int n = seld_tls_read(tls, resp_buf + total_read, cap - total_read, 8000);
        if (n > 0) {
            total_read += (size_t)n;
        } else if (n == 0) {
            break; // EOF or close_notify
        } else {
            // Read error or TLS verification failure
            seld_tls_close(tls);
            free(tls);
            free(resp_buf);
            return -7;
        }
    }
    seld_tls_close(tls);
    free(tls);

    if (total_read == 0) {
        free(resp_buf);
        return -7;
    }

    // Separate HTTP header from body
    size_t hdr_len = 0;
    for (size_t i = 0; i + 3 < total_read; i++) {
        if (resp_buf[i] == '\r' && resp_buf[i+1] == '\n' && resp_buf[i+2] == '\r' && resp_buf[i+3] == '\n') {
            hdr_len = i + 4;
            break;
        }
    }

    if (hdr_len == 0 || hdr_len >= total_read) {
        free(resp_buf);
        return -8;
    }

    // Check 200 OK
    if (strncmp((char*)resp_buf, "HTTP/1.1 200", 12) != 0 &&
        strncmp((char*)resp_buf, "HTTP/1.0 200", 12) != 0) {
        free(resp_buf);
        return -9; // HTTP status not 200
    }

    uint8_t* body = resp_buf + hdr_len;
    size_t body_len = total_read - hdr_len;

    // Check Content-Length if present in header
    char* cl_ptr = strstr((char*)resp_buf, "content-length:");
    if (!cl_ptr) cl_ptr = strstr((char*)resp_buf, "Content-Length:");
    if (cl_ptr && (size_t)(cl_ptr - (char*)resp_buf) < hdr_len) {
        cl_ptr += 15;
        while (*cl_ptr == ' ' || *cl_ptr == '\t') cl_ptr++;
        size_t expected_len = (size_t)atoi(cl_ptr);
        if (expected_len > 0 && body_len < expected_len) {
            free(resp_buf);
            return -8; // Truncated transfer
        }
    }

    int wres = seld_writefile(local_path, body, body_len);
    free(resp_buf);

    return (wres == 0) ? 0 : -10;
}

int seld_download_url(const char* url, const char* local_path) {
    if (!url || !local_path) return -1;

    // Tor Browser package: Direct GitHub repo fetch over SeldTLS 1.3
    if (strcmp(url, "tor") == 0 || strcmp(url, "torbrowser") == 0) {
        return seld_https_download("raw.githubusercontent.com",
                                   "/DjankiOpsec/SeldOS/main/build/bin/tor",
                                   local_path);
    }

    // DOOM package: Direct GitHub repo fetch over SeldTLS 1.3
    if (strcmp(url, "doom") == 0) {
        return seld_https_download("raw.githubusercontent.com",
                                   "/DjankiOpsec/SeldOS/main/build/bin/doom",
                                   local_path);
    }

    // DOOM WAD game assets: Direct GitHub repo fetch over SeldTLS 1.3
    if (strcmp(url, "wad") == 0 || strcmp(url, "doom1.wad") == 0) {
        return seld_https_download("raw.githubusercontent.com",
                                   "/DjankiOpsec/SeldOS/main/doom1.wad",
                                   local_path);
    }

    const char* p = url;
    int is_https = 0;
    if (strncmp(p, "https://", 8) == 0) {
        is_https = 1;
        p += 8;
    } else if (strncmp(p, "http://", 7) == 0) {
        p += 7;
    }

    char host_str[64];
    size_t hidx = 0;
    while (*p && *p != ':' && *p != '/' && hidx < sizeof(host_str) - 1) {
        host_str[hidx++] = *p++;
    }
    host_str[hidx] = '\0';

    uint16_t port = is_https ? 443 : 80;
    if (*p == ':') {
        p++;
        port = 0;
        while (*p >= '0' && *p <= '9') {
            port = (uint16_t)(port * 10 + (*p++ - '0'));
        }
    }

    const char* path = (*p == '/') ? p : "/";

    if (is_https) {
        return seld_https_download(host_str, path, local_path);
    }

    uint32_t ip = 0;
    if (strcmp(host_str, "localhost") == 0 || strcmp(host_str, "127.0.0.1") == 0 || strcmp(host_str, "gateway") == 0) {
        ip = 0;
    } else {
        unsigned int o1 = 0, o2 = 0, o3 = 0, o4 = 0;
        const char* hp = host_str;
        while (*hp >= '0' && *hp <= '9') o1 = o1 * 10 + (*hp++ - '0');
        if (*hp == '.') hp++;
        while (*hp >= '0' && *hp <= '9') o2 = o2 * 10 + (*hp++ - '0');
        if (*hp == '.') hp++;
        while (*hp >= '0' && *hp <= '9') o3 = o3 * 10 + (*hp++ - '0');
        if (*hp == '.') hp++;
        while (*hp >= '0' && *hp <= '9') o4 = o4 * 10 + (*hp++ - '0');

        if (o1 <= 255 && o2 <= 255 && o3 <= 255 && o4 <= 255 && hp > host_str) {
            ip = (uint32_t)(o1 | (o2 << 8) | (o3 << 16) | (o4 << 24));
        } else {
            seld_dns_resolve(host_str, &ip);
        }
    }

    return seld_net_download(ip, port, path, local_path);
}

int seld_tcp_connect(uint32_t ip, uint16_t port) {
    return (int)seld_syscall(SYS_NET_TCP_CONNECT, (long)ip, (long)port, 0);
}

int seld_tcp_send(int sock, const void* data, size_t len) {
    return (int)seld_syscall(SYS_NET_TCP_SEND, (long)sock, (long)data, (long)len);
}

int seld_tcp_recv(int sock, void* buf, size_t max_len, uint32_t timeout_ms) {
    return (int)seld_syscall4(SYS_NET_TCP_RECV, (long)sock, (long)buf, (long)max_len, (long)timeout_ms);
}

int seld_tcp_close(int sock) {
    return (int)seld_syscall(SYS_NET_TCP_CLOSE, (long)sock, 0, 0);
}

static const uint8_t* dns_dot_skip_name(const uint8_t* p, const uint8_t* end) {
    while (p < end) {
        uint8_t len = *p;
        if (len == 0) return p + 1;
        if ((len & 0xC0) == 0xC0) return p + 2; // Compression pointer
        p += 1 + len;
    }
    return end;
}

int seld_dns_resolve_dot(const char* hostname, uint32_t* ip_out) {
    if (!hostname || !ip_out) return -1;
    *ip_out = 0;

    // 0. Sanitize hostname
    const char* p = hostname;
    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    else if (strncmp(p, "https://", 8) == 0) p += 8;

    char clean_host[128];
    size_t clen = 0;
    while (*p && *p != '/' && *p != ':' && *p != ' ' && clen < sizeof(clean_host) - 1) {
        char ch = *p++;
        if (ch >= 'A' && ch <= 'Z') ch += 32;
        clean_host[clen++] = ch;
    }
    clean_host[clen] = '\0';
    if (clen == 0) return -1;

    // RFC 7686 Guard: Onion domains must never be resolved via DNS
    if ((clen >= 6 && strcmp(clean_host + clen - 6, ".onion") == 0) || strcmp(clean_host, "onion") == 0) {
        return -9;
    }

    // Fast bootstrap for Control D endpoints
    if (strcmp(clean_host, "p2.freedns.controld.com") == 0 ||
        strcmp(clean_host, "freedns.controld.com") == 0) {
        *ip_out = SELD_CONTROLD_DOT_IP;
        return 0;
    }
    if (strcmp(clean_host, "controld.com") == 0) {
        *ip_out = SELD_CONTROLD_DNS_P2_PRIMARY;
        return 0;
    }

    // Connect to Control D DoT Anycast (76.76.2.11) on port 853 (RFC 7858)
    int sock = seld_tcp_connect(SELD_CONTROLD_DOT_IP, 853);
    if (sock < 0) return -2;

    struct seld_tls_conn* tls = (struct seld_tls_conn*)malloc(sizeof(struct seld_tls_conn));
    if (!tls) {
        seld_tcp_close(sock);
        return -3;
    }

    if (seld_tls_handshake(tls, sock, SELD_CONTROLD_DOT_HOST) != 0) {
        free(tls);
        seld_tcp_close(sock);
        return -4;
    }

    // Build DNS query with 2-byte length prefix (RFC 7858)
    uint8_t qbuf[512];
    uint8_t* dst = qbuf + 2; // Leave 2 bytes for length prefix

    // DNS Header: ID=0x5E1D, Flags=0x0100 (Standard Query, RD=1), QDCOUNT=1
    dst[0] = 0x5E; dst[1] = 0x1D; // ID
    dst[2] = 0x01; dst[3] = 0x00; // Flags: RD
    dst[4] = 0x00; dst[5] = 0x01; // QDCOUNT: 1
    dst[6] = 0x00; dst[7] = 0x00; // ANCOUNT: 0
    dst[8] = 0x00; dst[9] = 0x00; // NSCOUNT: 0
    dst[10] = 0x00; dst[11] = 0x00; // ARCOUNT: 0
    dst += 12;

    // Question: QNAME
    const char* src = clean_host;
    while (*src) {
        const char* dot = strchr(src, '.');
        size_t label_len = dot ? (size_t)(dot - src) : strlen(src);
        if (label_len == 0 || label_len > 63 || (size_t)(dst - qbuf) + label_len + 6 >= sizeof(qbuf)) {
            seld_tls_close(tls);
            free(tls);
            seld_tcp_close(sock);
            return -5;
        }
        *dst++ = (uint8_t)label_len;
        memcpy(dst, src, label_len);
        dst += label_len;
        if (!dot) break;
        src = dot + 1;
    }
    *dst++ = 0x00; // Root label
    *dst++ = 0x00; *dst++ = 0x01; // QTYPE: A (1)
    *dst++ = 0x00; *dst++ = 0x01; // QCLASS: IN (1)

    uint16_t wire_len = (uint16_t)(dst - (qbuf + 2));
    qbuf[0] = (uint8_t)(wire_len >> 8);
    qbuf[1] = (uint8_t)(wire_len & 0xFF);

    if (seld_tls_write(tls, qbuf, wire_len + 2) <= 0) {
        seld_tls_close(tls);
        free(tls);
        seld_tcp_close(sock);
        return -6;
    }

    // Read 2-byte response length
    uint8_t len_bytes[2];
    int r = seld_tls_read(tls, len_bytes, 2, 3000);
    if (r != 2) {
        seld_tls_close(tls);
        free(tls);
        seld_tcp_close(sock);
        return -7;
    }
    uint16_t resp_len = ((uint16_t)len_bytes[0] << 8) | len_bytes[1];
    if (resp_len < 12 || resp_len > 4096) {
        seld_tls_close(tls);
        free(tls);
        seld_tcp_close(sock);
        return -8;
    }

    uint8_t resp_buf[4096];
    size_t total_read = 0;
    while (total_read < resp_len) {
        int nr = seld_tls_read(tls, resp_buf + total_read, resp_len - total_read, 2000);
        if (nr <= 0) break;
        total_read += (size_t)nr;
    }

    seld_tls_close(tls);
    free(tls);
    seld_tcp_close(sock);

    if (total_read < resp_len) return -9;

    // Parse DNS response
    uint16_t flags = ((uint16_t)resp_buf[2] << 8) | resp_buf[3];
    uint8_t rcode = (uint8_t)(flags & 0x0F);
    if (rcode != 0) return -10; // NXDOMAIN or error

    uint16_t qdcount = ((uint16_t)resp_buf[4] << 8) | resp_buf[5];
    uint16_t ancount = ((uint16_t)resp_buf[6] << 8) | resp_buf[7];
    if (ancount == 0) return -11;

    const uint8_t* ptr = resp_buf + 12;
    const uint8_t* end = resp_buf + resp_len;

    // Skip question section
    for (int q = 0; q < qdcount && ptr < end; q++) {
        ptr = dns_dot_skip_name(ptr, end);
        if (ptr + 4 > end) return -12;
        ptr += 4; // QTYPE + QCLASS
    }

    // Parse answers
    for (int a = 0; a < ancount && ptr < end; a++) {
        ptr = dns_dot_skip_name(ptr, end);
        if (ptr + 10 > end) return -13;
        uint16_t rtype = ((uint16_t)ptr[0] << 8) | ptr[1];
        uint16_t rclass = ((uint16_t)ptr[2] << 8) | ptr[3];
        uint16_t rdlength = ((uint16_t)ptr[8] << 8) | ptr[9];
        ptr += 10;
        if (ptr + rdlength > end) return -14;

        if (rtype == 1 && rclass == 1 && rdlength == 4) { // Type A, Class IN, 4 bytes
            uint32_t ip = (uint32_t)(ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24));
            *ip_out = ip;
            if (ip == 0) return -5; // Blocked by OpSec Filter (0.0.0.0)
            return 0;
        }
        ptr += rdlength;
    }

    return -15; // No A record found
}

int seld_dns_resolve(const char* hostname, uint32_t* ip_out) {
    if (!hostname || !ip_out) return -1;
    // 1. Fast kernel resolution via Control D Ads & Trackers (76.76.2.2:53 / 76.76.10.2:53)
    int res = (int)seld_syscall(SYS_NET_DNS_RESOLVE, (long)hostname, (long)ip_out, 0);
    if (res == 0) {
        if (*ip_out == 0) return -5; // Blocked by OpSec Filter
        return 0;
    }
    if (res == -5) {
        *ip_out = 0;
        return -5; // Blocked by OpSec Filter
    }
    if (res == -9) {
        *ip_out = 0;
        return -9; // Blocked by RFC 7686 Guard (.onion domain leak prevention)
    }
    if (res == -10) {
        // Stealth mode: UDP DNS blocked, immediately fall back to DoT via TLS 1.3
        return seld_dns_resolve_dot(hostname, ip_out);
    }

    // 2. Encrypted fallback: DNS-over-TLS (p2.freedns.controld.com:853)
    return seld_dns_resolve_dot(hostname, ip_out);
}

int seld_drv_off(const char* driver_name) {
    if (!driver_name) return -1;
    return (int)seld_syscall(SYS_DRV_OFF, (long)driver_name, 0, 0);
}

int seld_reboot(void) {
    return (int)seld_syscall(SYS_REBOOT, 0, 0, 0);
}

int seld_poweroff(void) {
    return (int)seld_syscall(SYS_POWEROFF, 0, 0, 0);
}

int seld_immune_purge(void) {
    return (int)seld_syscall(SYS_IMMUNE_PURGE, 0, 0, 0);
}

int seld_net_lock(int mode) {
    long k_mode = (long)mode;
    if (mode == SELD_AIRGAP_STEALTH) {
        k_mode = 5;
    }
    return (int)seld_syscall(SYS_NET_SET_LOCK, k_mode, 0, 0);
}

int seld_net_get_lock(void) {
    return (int)seld_syscall(SYS_NET_GET_LOCK, 0, 0, 0);
}

int seld_net_lease_acquire(void) {
    return (int)seld_syscall(SYS_NET_SET_LOCK, NET_LEASE_ACQUIRE, 0, 0);
}

int seld_net_lease_release(void) {
    return (int)seld_syscall(SYS_NET_SET_LOCK, NET_LEASE_RELEASE, 0, 0);
}

int seld_net_set_desync(int mode) {
    return (int)seld_syscall(SYS_NET_SET_DESYNC, (long)mode, 0, 0);
}

int seld_net_get_desync(void) {
    return (int)seld_syscall(SYS_NET_GET_DESYNC, 0, 0, 0);
}

void reboot(void) {
    seld_reboot();
}

void poweroff(void) {
    seld_poweroff();
}

int seld_pledge(uint32_t flags) {
    return (int)seld_syscall(SYS_PLEDGE, (long)flags, 0, 0);
}

int seld_unveil(const char* path, const char* permissions) {
    return (int)seld_syscall(SYS_UNVEIL, (long)path, (long)permissions, 0);
}

int unveil(const char* path, const char* permissions) {
    return seld_unveil(path, permissions);
}

int seld_set_jitter(int enable) {
    return (int)seld_syscall(SYS_OPSEC_SET_JITTER, (long)enable, 0, 0);
}

int seld_get_jitter(void) {
    return (int)seld_syscall(SYS_OPSEC_GET_JITTER, 0, 0, 0);
}

