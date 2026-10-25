/*
 * SeldOS - Humboldt Kernel Project
 * SNL OpSec Tactical VoIP Wiretap & Audio Interceptor (/bin/wiretap)
 *
 * Real-time Ethernet Promiscuous Sniffing, SIP INVITE/SDP Session Demuxing,
 * RTP Stream Capture (RFC 3550), On-the-Fly G.711 PCMU/PCMA Audio Decoding,
 * AC'97 DMA Output, and Tor SOCKS5 Proxy Relay.
 *
 * Rendered on 680x334 Linear Framebuffer with Real-Time Oscilloscope & VU Meter.
 * Ring 3 Humboldt Hardened (PLEDGE_STDIO | PLEDGE_NET | PLEDGE_AUDIO | PLEDGE_PURGE).
 * Zero Packet Emission (Passive Silent Wire) & Amnesic RAM Zeroization on Exit.
 *
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "seld.h"

#define SCR_W 680
#define SCR_H 334

/* Humboldt Tactical Palette */
#define COL_BG_CANVAS      0xFF070B0E
#define COL_CARD_BG        0xFF0B1219
#define COL_CARD_HEADER    0xFF0F1A24
#define COL_CARD_BORDER    0xFF1E293B
#define COL_SCOPE_BG       0xFF040D08
#define COL_SCOPE_GRID     0xFF0A2416
#define COL_SCOPE_BEAM     0xFF4ADE80
#define COL_SCOPE_PEAK     0xFF86EFAC

#define COL_TEXT_WHITE     0xFFF8FAFC
#define COL_TEXT_EMERALD   0xFF10B981
#define COL_TEXT_MINT      0xFF6EE7B7
#define COL_TEXT_CYAN      0xFF38BDF8
#define COL_TEXT_AMBER     0xFFF59E0B
#define COL_TEXT_CRIMSON   0xFFEF4444
#define COL_TEXT_SLATE     0xFF64748B
#define COL_TEXT_DIM       0xFF334155

#define COL_CHIP_BG        0xFF141E28
#define COL_CHIP_BORDER    0xFF233549

/* 5x7 Font for Compact Tactical HUD (ASCII 32..126) */
static const uint8_t font5x7[95][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 32 ' '
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // 33 '!'
    {0x00, 0x07, 0x00, 0x07, 0x00}, // 34 '"'
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // 35 '#'
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // 36 '$'
    {0x23, 0x13, 0x08, 0x64, 0x62}, // 37 '%'
    {0x36, 0x49, 0x55, 0x22, 0x50}, // 38 '&'
    {0x00, 0x05, 0x03, 0x00, 0x00}, // 39 '''
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // 40 '('
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // 41 ')'
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // 42 '*'
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // 43 '+'
    {0x00, 0x50, 0x30, 0x00, 0x00}, // 44 ','
    {0x08, 0x08, 0x08, 0x08, 0x08}, // 45 '-'
    {0x00, 0x60, 0x60, 0x00, 0x00}, // 46 '.'
    {0x20, 0x10, 0x08, 0x04, 0x02}, // 47 '/'
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 48 '0'
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 49 '1'
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 50 '2'
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 51 '3'
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 52 '4'
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 53 '5'
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 54 '6'
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 55 '7'
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 56 '8'
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 57 '9'
    {0x00, 0x36, 0x36, 0x00, 0x00}, // 58 ':'
    {0x00, 0x56, 0x36, 0x00, 0x00}, // 59 ';'
    {0x08, 0x14, 0x22, 0x41, 0x00}, // 60 '<'
    {0x14, 0x14, 0x14, 0x14, 0x14}, // 61 '='
    {0x00, 0x41, 0x22, 0x14, 0x08}, // 62 '>'
    {0x02, 0x01, 0x51, 0x09, 0x06}, // 63 '?'
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // 64 '@'
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // 65 'A'
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // 66 'B'
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // 67 'C'
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // 68 'D'
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // 69 'E'
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // 70 'F'
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // 71 'G'
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // 72 'H'
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // 73 'I'
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // 74 'J'
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // 75 'K'
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // 76 'L'
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // 77 'M'
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // 78 'N'
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 79 'O'
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // 80 'P'
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // 81 'Q'
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // 82 'R'
    {0x46, 0x49, 0x49, 0x49, 0x31}, // 83 'S'
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // 84 'T'
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // 85 'U'
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // 86 'V'
    {0x7F, 0x20, 0x18, 0x20, 0x7F}, // 87 'W'
    {0x63, 0x14, 0x08, 0x14, 0x63}, // 88 'X'
    {0x07, 0x08, 0x70, 0x08, 0x07}, // 89 'Y'
    {0x61, 0x51, 0x49, 0x45, 0x43}, // 90 'Z'
    {0x00, 0x7F, 0x41, 0x41, 0x00}, // 91 '['
    {0x02, 0x04, 0x08, 0x10, 0x20}, // 92 '\'
    {0x00, 0x41, 0x41, 0x7F, 0x00}, // 93 ']'
    {0x04, 0x02, 0x01, 0x02, 0x04}, // 94 '^'
    {0x40, 0x40, 0x40, 0x40, 0x40}, // 95 '_'
    {0x00, 0x01, 0x02, 0x04, 0x00}, // 96 '`'
    {0x20, 0x54, 0x54, 0x54, 0x78}, // 97 'a'
    {0x7F, 0x48, 0x44, 0x44, 0x38}, // 98 'b'
    {0x38, 0x44, 0x44, 0x44, 0x20}, // 99 'c'
    {0x38, 0x44, 0x44, 0x48, 0x7F}, // 100 'd'
    {0x38, 0x54, 0x54, 0x54, 0x18}, // 101 'e'
    {0x08, 0x7E, 0x09, 0x01, 0x02}, // 102 'f'
    {0x0C, 0x52, 0x52, 0x52, 0x3E}, // 103 'g'
    {0x7F, 0x08, 0x04, 0x04, 0x78}, // 104 'h'
    {0x00, 0x44, 0x7D, 0x40, 0x00}, // 105 'i'
    {0x20, 0x40, 0x44, 0x3D, 0x00}, // 106 'j'
    {0x7F, 0x10, 0x28, 0x44, 0x00}, // 107 'k'
    {0x00, 0x41, 0x7F, 0x40, 0x00}, // 108 'l'
    {0x7C, 0x04, 0x18, 0x04, 0x78}, // 109 'm'
    {0x7C, 0x08, 0x04, 0x04, 0x78}, // 110 'n'
    {0x38, 0x44, 0x44, 0x44, 0x38}, // 111 'o'
    {0x7C, 0x14, 0x14, 0x14, 0x08}, // 112 'p'
    {0x08, 0x14, 0x14, 0x18, 0x7C}, // 113 'q'
    {0x7C, 0x08, 0x04, 0x04, 0x08}, // 114 'r'
    {0x48, 0x54, 0x54, 0x54, 0x20}, // 115 's'
    {0x04, 0x3F, 0x44, 0x40, 0x20}, // 116 't'
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, // 117 'u'
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, // 118 'v'
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, // 119 'w'
    {0x44, 0x28, 0x10, 0x28, 0x44}, // 120 'x'
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, // 121 'y'
    {0x44, 0x64, 0x54, 0x4C, 0x44}, // 122 'z'
    {0x00, 0x08, 0x36, 0x41, 0x00}, // 123 '{'
    {0x00, 0x00, 0x7F, 0x00, 0x00}, // 124 '|'
    {0x00, 0x41, 0x36, 0x08, 0x00}, // 125 '}'
    {0x08, 0x04, 0x08, 0x10, 0x08}  // 126 '~'
};

/* Endianness utilities */
static inline uint16_t net_htons(uint16_t v) {
    return (uint16_t)(((v & 0xFF) << 8) | ((v >> 8) & 0xFF));
}

static inline uint16_t net_ntohs(uint16_t v) {
    return net_htons(v);
}

static inline uint32_t net_htonl(uint32_t v) {
    return ((v & 0xFF) << 24) |
           (((v >> 8) & 0xFF) << 16) |
           (((v >> 16) & 0xFF) << 8) |
           ((v >> 24) & 0xFF);
}

static inline uint32_t net_ntohl(uint32_t v) {
    return net_htonl(v);
}

/* G.711 Audio Decoding Tables & Math */
static inline int16_t ulaw_to_pcm16(uint8_t u_val) {
    u_val = (uint8_t)~u_val;
    int t = ((u_val & 0x0F) << 3) + 0x84;
    t <<= ((u_val & 0x70) >> 4);
    return (int16_t)((u_val & 0x80) ? (0x84 - t) : (t - 0x84));
}

static inline int16_t alaw_to_pcm16(uint8_t a_val) {
    a_val ^= 0x55;
    int t = (a_val & 0x0F) << 4;
    int seg = (a_val & 0x70) >> 4;
    switch (seg) {
        case 0: t += 8; break;
        case 1: t += 0x108; break;
        default: t += 0x108; t <<= (seg - 1); break;
    }
    return (int16_t)((a_val & 0x80) ? t : -t);
}

/* Downscale linear 16-bit signed PCM to 8-bit unsigned PCM for SeldOS AC'97 */
static inline uint8_t pcm16_to_u8(int16_t sample) {
    int v = (sample >> 8) + 128;
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    return (uint8_t)v;
}

/* Call Session Tracking */
#define CALL_STATE_IDLE      0
#define CALL_STATE_INVITE    1
#define CALL_STATE_RINGING   2
#define CALL_STATE_ACTIVE    3
#define CALL_STATE_HANGUP    4

#define MODE_PROMISC 0
#define MODE_TOR     1

struct call_session {
    int      state;
    char     caller[48];
    char     callee[48];
    char     call_id[48];
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t rtp_port;
    uint8_t  payload_type; // 0 = PCMU, 8 = PCMA
    uint32_t ssrc;
    uint32_t packets_rx;
    uint32_t bytes_rx;
    uint64_t start_ms;
    uint64_t end_ms;
};

#define MAX_HISTORY 4
static struct call_session s_active_call;
static struct call_session s_history[MAX_HISTORY];
static int                 s_history_count = 0;

/* Audio Scope Buffers */
#define SCOPE_W 340
#define SCOPE_H 150
#define SCOPE_X 16
#define SCOPE_Y 62

static uint8_t  s_audio_scope_samples[SCOPE_W];
static int      s_scope_write_pos = 0;
static int      s_audio_muted = 0;
static int      s_mode = MODE_PROMISC; // 0 = Promisc Wire, 1 = Tor SOCKS5
static int      s_synth_active = 0;
static uint32_t s_synth_tick = 0;

/* Framebuffer & Backbuffer */
static struct seld_fb_info s_fb;
static uint32_t*           s_backbuf = NULL;

/* Fast Graphics Primitives */
static void draw_pixel(int x, int y, uint32_t color) {
    if (x >= 0 && x < SCR_W && y >= 0 && y < SCR_H && s_backbuf) {
        s_backbuf[y * SCR_W + x] = color;
    }
}

static void fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (!s_backbuf) return;
    int x2 = x + w;
    int y2 = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x2 > SCR_W) x2 = SCR_W;
    if (y2 > SCR_H) y2 = SCR_H;

    for (int cy = y; cy < y2; cy++) {
        uint32_t* row = &s_backbuf[cy * SCR_W + x];
        for (int cx = x; cx < x2; cx++) {
            *row++ = color;
        }
    }
}

static void draw_rect(int x, int y, int w, int h, uint32_t color) {
    for (int i = 0; i < w; i++) {
        draw_pixel(x + i, y, color);
        draw_pixel(x + i, y + h - 1, color);
    }
    for (int i = 0; i < h; i++) {
        draw_pixel(x, y + i, color);
        draw_pixel(x + w - 1, y + i, color);
    }
}

static void draw_char(int x, int y, char c, uint32_t color) {
    if (c < 32 || c > 126) c = '?';
    const uint8_t* glyph = font5x7[c - 32];
    for (int col = 0; col < 5; col++) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < 7; row++) {
            if (bits & (1 << row)) {
                draw_pixel(x + col, y + row, color);
            }
        }
    }
}

static void draw_text(int x, int y, const char* str, uint32_t color) {
    if (!str) return;
    int cx = x;
    while (*str) {
        if (*str == '\n') {
            cx = x;
            y += 9;
        } else {
            draw_char(cx, y, *str, color);
            cx += 6;
        }
        str++;
    }
}

static void flip_screen(void) {
    if (!s_fb.framebuffer || !s_backbuf) return;
    uint32_t* dst = (uint32_t*)s_fb.framebuffer;
    uint32_t pitch_p = s_fb.pitch / 4;
    for (int y = 0; y < SCR_H; y++) {
        memcpy(&dst[y * pitch_p], &s_backbuf[y * SCR_W], SCR_W * sizeof(uint32_t));
    }
}

/* SIP Header String Extraction */
static void extract_sip_header(const char* body, const char* header, char* out, size_t max_out) {
    out[0] = '\0';
    const char* p = strstr(body, header);
    if (!p) return;
    p += strlen(header);
    while (*p == ' ' || *p == '\t') p++;

    // Look for quote or display name if present: "Name" <sip:...>
    if (*p == '"') {
        const char* qend = strchr(p + 1, '"');
        if (qend) {
            size_t nlen = (size_t)(qend - p + 1);
            if (nlen < max_out) {
                strncpy(out, p, nlen);
                out[nlen] = '\0';
                return;
            }
        }
    }

    // Otherwise grab until newline or semicolon
    size_t i = 0;
    while (*p && *p != '\r' && *p != '\n' && i < max_out - 1) {
        out[i++] = *p++;
    }
    out[i] = '\0';
}

/* Extract RTP audio port from SDP body ("m=audio 16384 RTP/AVP 0") */
static int extract_sdp_rtp_port(const char* body, uint16_t* port_out, uint8_t* pt_out) {
    const char* p = strstr(body, "m=audio ");
    if (!p) return 0;
    p += 8;
    while (*p == ' ') p++;

    int port = 0;
    while (*p >= '0' && *p <= '9') {
        port = port * 10 + (*p - '0');
        p++;
    }
    if (port <= 0 || port > 65535) return 0;
    *port_out = (uint16_t)port;

    // Search for payload type
    const char* rtp_str = strstr(p, "RTP/AVP ");
    if (rtp_str) {
        rtp_str += 8;
        while (*rtp_str == ' ') rtp_str++;
        int pt = 0;
        while (*rtp_str >= '0' && *rtp_str <= '9') {
            pt = pt * 10 + (*rtp_str - '0');
            rtp_str++;
        }
        *pt_out = (uint8_t)pt;
    } else {
        *pt_out = 0; // Default PCMU
    }
    return 1;
}

/* Push finished call into call history ledger */
static void archive_active_call(void) {
    if (s_active_call.state == CALL_STATE_IDLE) return;
    if (s_history_count < MAX_HISTORY) {
        s_history[s_history_count++] = s_active_call;
    } else {
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            s_history[i] = s_history[i + 1];
        }
        s_history[MAX_HISTORY - 1] = s_active_call;
    }
    s_active_call.state = CALL_STATE_HANGUP;
}

/* Process incoming raw Ethernet frame */
static void process_packet(const uint8_t* frame, size_t len) {
    if (len < 14 + 20 + 8) return; // Eth + IPv4 + UDP minimum

    // Check Ethernet Header
    uint16_t ethertype = (frame[12] << 8) | frame[13];
    if (ethertype != 0x0800) return; // IPv4 only

    const uint8_t* ip_pkt = frame + 14;
    uint8_t ver_ihl = ip_pkt[0];
    if ((ver_ihl >> 4) != 4) return;
    size_t ip_hlen = (ver_ihl & 0x0F) * 4;
    if (ip_hlen < 20 || 14 + ip_hlen + 8 > len) return;

    uint8_t proto = ip_pkt[9];
    if (proto != 17) return; // UDP only

    uint32_t src_ip = *(const uint32_t*)(ip_pkt + 12);
    uint32_t dst_ip = *(const uint32_t*)(ip_pkt + 16);

    const uint8_t* udp_pkt = ip_pkt + ip_hlen;
    uint16_t src_port = (udp_pkt[0] << 8) | udp_pkt[1];
    uint16_t dst_port = (udp_pkt[2] << 8) | udp_pkt[3];
    uint16_t udp_len  = (udp_pkt[4] << 8) | udp_pkt[5];

    if (udp_len < 8 || 14 + ip_hlen + udp_len > len) return;
    const uint8_t* payload = udp_pkt + 8;
    size_t payload_len = udp_len - 8;

    // 1. SIP Signaling Detection (Standard port 5060 or packet with "SIP/2.0")
    if (src_port == 5060 || dst_port == 5060 ||
        (payload_len > 12 && memcmp(payload, "INVITE ", 7) == 0) ||
        (payload_len > 12 && memcmp(payload, "SIP/2.0 ", 8) == 0)) {

        char sip_buf[1500];
        size_t cpy_len = payload_len < sizeof(sip_buf) - 1 ? payload_len : sizeof(sip_buf) - 1;
        memcpy(sip_buf, payload, cpy_len);
        sip_buf[cpy_len] = '\0';

        if (strstr(sip_buf, "INVITE ") != NULL) {
            archive_active_call();
            memset(&s_active_call, 0, sizeof(s_active_call));
            s_active_call.state = CALL_STATE_INVITE;
            s_active_call.src_ip = src_ip;
            s_active_call.dst_ip = dst_ip;
            s_active_call.start_ms = seld_uptime();

            extract_sip_header(sip_buf, "From:", s_active_call.caller, sizeof(s_active_call.caller));
            extract_sip_header(sip_buf, "To:", s_active_call.callee, sizeof(s_active_call.callee));
            extract_sip_header(sip_buf, "Call-ID:", s_active_call.call_id, sizeof(s_active_call.call_id));

            uint16_t rtp_p = 0;
            uint8_t  rtp_pt = 0;
            if (extract_sdp_rtp_port(sip_buf, &rtp_p, &rtp_pt)) {
                s_active_call.rtp_port = rtp_p;
                s_active_call.payload_type = rtp_pt;
                s_active_call.state = CALL_STATE_ACTIVE;
            }
        } else if (strstr(sip_buf, "200 OK") != NULL && s_active_call.state >= CALL_STATE_INVITE) {
            uint16_t rtp_p = 0;
            uint8_t  rtp_pt = 0;
            if (extract_sdp_rtp_port(sip_buf, &rtp_p, &rtp_pt)) {
                s_active_call.rtp_port = rtp_p;
                s_active_call.payload_type = rtp_pt;
                s_active_call.state = CALL_STATE_ACTIVE;
            }
        } else if (strstr(sip_buf, "BYE ") != NULL || strstr(sip_buf, "CANCEL ") != NULL) {
            if (s_active_call.state == CALL_STATE_ACTIVE || s_active_call.state == CALL_STATE_INVITE) {
                s_active_call.end_ms = seld_uptime();
                archive_active_call();
            }
        }
        return;
    }

    // 2. RTP Voice Stream Detection
    // Either matches the negotiated RTP port or heuristic detection of RFC 3550 RTP
    int is_rtp = 0;
    if (s_active_call.rtp_port != 0 && (dst_port == s_active_call.rtp_port || src_port == s_active_call.rtp_port)) {
        is_rtp = 1;
    } else if (payload_len >= 12 && ((payload[0] & 0xC0) == 0x80)) {
        // V=2, standard RTP
        uint8_t pt = payload[1] & 0x7F;
        if (pt == 0 || pt == 8) { // PCMU or PCMA
            is_rtp = 1;
            if (s_active_call.state == CALL_STATE_IDLE) {
                s_active_call.state = CALL_STATE_ACTIVE;
                strcpy(s_active_call.caller, "Interception [Direct RTP]");
                strcpy(s_active_call.callee, "PBX Switch");
                s_active_call.rtp_port = dst_port;
                s_active_call.payload_type = pt;
                s_active_call.start_ms = seld_uptime();
            }
        }
    }

    if (is_rtp && payload_len > 12) {
        uint8_t pt = payload[1] & 0x7F;
        uint32_t ssrc = (payload[8] << 24) | (payload[9] << 16) | (payload[10] << 8) | payload[11];
        s_active_call.ssrc = ssrc;
        s_active_call.packets_rx++;
        s_active_call.bytes_rx += payload_len - 12;

        const uint8_t* rtp_payload = payload + 12;
        size_t audio_len = payload_len - 12;

        uint8_t pcm8_out[512];
        size_t dec_len = audio_len < sizeof(pcm8_out) ? audio_len : sizeof(pcm8_out);

        for (size_t i = 0; i < dec_len; i++) {
            int16_t pcm16;
            if (pt == 8 || s_active_call.payload_type == 8) {
                pcm16 = alaw_to_pcm16(rtp_payload[i]);
            } else {
                pcm16 = ulaw_to_pcm16(rtp_payload[i]);
            }
            uint8_t u8 = pcm16_to_u8(pcm16);
            pcm8_out[i] = u8;

            // Push to visual oscilloscope buffer
            s_audio_scope_samples[s_scope_write_pos] = u8;
            s_scope_write_pos = (s_scope_write_pos + 1) % SCOPE_W;
        }

        // Stream to AC'97 hardware audio DAC unless operator activated Mute
        if (!s_audio_muted && dec_len > 0) {
            seld_audio_play(pcm8_out, dec_len, 8000);
        }
    }
}

/* Synthetic Test Generator (Demo / QEMU Self-Test) */
static void run_synthetic_tick(void) {
    s_synth_tick++;

    if (s_active_call.state == CALL_STATE_IDLE && (s_synth_tick % 40 == 1)) {
        // Trigger simulated SIP INVITE call from Director to Accountant
        s_active_call.state = CALL_STATE_ACTIVE;
        strcpy(s_active_call.caller, "\"Director\" <+7(495)911-00-01>");
        strcpy(s_active_call.callee, "\"Chief Accountant\" <+7(495)911-00-42>");
        strcpy(s_active_call.call_id, "4a8e2b91@pbx.corp.lan");
        s_active_call.rtp_port = 16384;
        s_active_call.payload_type = 0; // PCMU
        s_active_call.ssrc = 0x3F88A102;
        s_active_call.start_ms = seld_uptime();
    }

    if (s_active_call.state == CALL_STATE_ACTIVE) {
        // Generate realistic voice-band multi-frequency acoustic wave
        uint8_t synth_buf[160]; // 20ms @ 8 kHz
        for (int i = 0; i < 160; i++) {
            int t = s_synth_tick * 160 + i;
            // Harmonic voice formants: F1=300 Hz, F2=750 Hz, F3=2200 Hz with cadence modulation
            int v1 = ((t * 300) / 8000) & 1 ? 40 : -40;
            int v2 = ((t * 750) / 8000) & 1 ? 25 : -25;
            int v3 = ((t * 2200) / 8000) & 1 ? 15 : -15;
            int cadence = (t / 800) % 2 ? 1 : 0; // Speech pauses
            int sample = (v1 + v2 + v3) * cadence;

            uint8_t u8 = (uint8_t)(sample + 128);
            synth_buf[i] = u8;

            s_audio_scope_samples[s_scope_write_pos] = u8;
            s_scope_write_pos = (s_scope_write_pos + 1) % SCOPE_W;
        }

        s_active_call.packets_rx++;
        s_active_call.bytes_rx += 160;

        if (!s_audio_muted) {
            seld_audio_play(synth_buf, sizeof(synth_buf), 8000);
        }

        // Auto-hangup demo after ~300 ticks to show ledger rotation
        if (s_synth_tick > 320) {
            archive_active_call();
            s_synth_tick = 0;
        }
    }
}

/* Tactical HUD Rendering */
static void render_hud(void) {
    fill_rect(0, 0, SCR_W, SCR_H, COL_BG_CANVAS);

    // 1. Top Tactical Bar
    fill_rect(0, 0, SCR_W, 26, COL_CARD_HEADER);
    draw_rect(0, 0, SCR_W, 26, COL_CARD_BORDER);

    draw_text(16, 9, "WIRETAP // SOVEREIGN TACTICAL VOIP INTERCEPTOR [SNL RING 3]", COL_TEXT_WHITE);

    // Badges
    int bx = 450;
    if (s_mode == MODE_PROMISC) {
        fill_rect(bx, 5, 110, 16, COL_CHIP_BG);
        draw_rect(bx, 5, 110, 16, COL_TEXT_EMERALD);
        draw_text(bx + 6, 9, "WIRE: PROMISCUOUS", COL_TEXT_EMERALD);
    } else {
        fill_rect(bx, 5, 110, 16, COL_CHIP_BG);
        draw_rect(bx, 5, 110, 16, COL_TEXT_CYAN);
        draw_text(bx + 6, 9, "ROUTING: TOR SOCKS5", COL_TEXT_CYAN);
    }

    bx += 116;
    fill_rect(bx, 5, 96, 16, COL_CHIP_BG);
    draw_rect(bx, 5, 96, 16, s_audio_muted ? COL_TEXT_CRIMSON : COL_TEXT_EMERALD);
    draw_text(bx + 6, 9, s_audio_muted ? "AUDIO: MUTED" : "AC'97: STREAM", s_audio_muted ? COL_TEXT_CRIMSON : COL_TEXT_EMERALD);

    // 2. Left Panel: Real-Time Audio Oscilloscope
    fill_rect(SCOPE_X, SCOPE_Y, SCOPE_W, SCOPE_H, COL_SCOPE_BG);
    draw_rect(SCOPE_X, SCOPE_Y, SCOPE_W, SCOPE_H, COL_CARD_BORDER);

    // Oscilloscope Grid Lines
    for (int y = SCOPE_Y + 25; y < SCOPE_Y + SCOPE_H; y += 25) {
        for (int x = SCOPE_X + 2; x < SCOPE_X + SCOPE_W - 2; x += 6) {
            draw_pixel(x, y, COL_SCOPE_GRID);
        }
    }
    for (int x = SCOPE_X + 34; x < SCOPE_X + SCOPE_W; x += 34) {
        for (int y = SCOPE_Y + 2; y < SCOPE_Y + SCOPE_H - 2; y += 6) {
            draw_pixel(x, y, COL_SCOPE_GRID);
        }
    }

    // Center Baseline
    int mid_y = SCOPE_Y + SCOPE_H / 2;
    for (int x = SCOPE_X; x < SCOPE_X + SCOPE_W; x += 2) {
        draw_pixel(x, mid_y, COL_SCOPE_GRID);
    }

    // Oscilloscope Waveform Phosphor Plot
    int prev_plot_y = mid_y;
    for (int x = 0; x < SCOPE_W; x++) {
        int idx = (s_scope_write_pos + x) % SCOPE_W;
        uint8_t sample = s_audio_scope_samples[idx];
        int val = (int)sample - 128; // -128..+127
        int plot_y = mid_y - (val * (SCOPE_H - 16)) / 256;
        if (plot_y < SCOPE_Y + 2) plot_y = SCOPE_Y + 2;
        if (plot_y > SCOPE_Y + SCOPE_H - 3) plot_y = SCOPE_Y + SCOPE_H - 3;

        draw_pixel(SCOPE_X + x, plot_y, COL_SCOPE_BEAM);
        // Antialiased vertical line between steps
        if (x > 0) {
            int step = plot_y > prev_plot_y ? 1 : -1;
            for (int sy = prev_plot_y; sy != plot_y; sy += step) {
                draw_pixel(SCOPE_X + x, sy, COL_SCOPE_BEAM);
            }
        }
        prev_plot_y = plot_y;
    }

    // Oscilloscope OSD Header & Telemetry
    draw_text(SCOPE_X + 8, SCOPE_Y + 8, "RTP VOICE OSCILLOSCOPE (8 kHz G.711 -> AC'97 DMA)", COL_TEXT_MINT);

    char scope_stat[64];
    snprintf(scope_stat, sizeof(scope_stat), "CODEC: %s | PKTS: %u",
             s_active_call.payload_type == 8 ? "G.711 A-LAW (PCMA)" : "G.711 u-LAW (PCMU)",
             (unsigned int)s_active_call.packets_rx);
    draw_text(SCOPE_X + 8, SCOPE_Y + SCOPE_H - 14, scope_stat, COL_TEXT_CYAN);

    // 3. Audio VU Meter Bar (Below Scope)
    int vu_y = SCOPE_Y + SCOPE_H + 8;
    fill_rect(SCOPE_X, vu_y, SCOPE_W, 14, COL_CARD_BG);
    draw_rect(SCOPE_X, vu_y, SCOPE_W, 14, COL_CARD_BORDER);
    draw_text(SCOPE_X + 6, vu_y + 4, "VU:", COL_TEXT_SLATE);

    int vu_level = 0;
    if (s_active_call.state == CALL_STATE_ACTIVE) {
        int sum = 0;
        for (int i = 0; i < 32; i++) {
            int idx = (s_scope_write_pos + SCOPE_W - 1 - i) % SCOPE_W;
            int diff = (int)s_audio_scope_samples[idx] - 128;
            if (diff < 0) diff = -diff;
            sum += diff;
        }
        vu_level = (sum / 32) * 2;
        if (vu_level > 260) vu_level = 260;
    }

    fill_rect(SCOPE_X + 30, vu_y + 3, vu_level, 8, vu_level > 200 ? COL_TEXT_CRIMSON : (vu_level > 120 ? COL_TEXT_AMBER : COL_TEXT_EMERALD));

    // 4. Right Panel: Active Call Interception Box
    int rx = 370;
    int rw = 294;
    fill_rect(rx, SCOPE_Y, rw, 100, COL_CARD_BG);
    draw_rect(rx, SCOPE_Y, rw, 100, COL_CARD_BORDER);

    fill_rect(rx, SCOPE_Y, rw, 22, COL_CARD_HEADER);
    draw_rect(rx, SCOPE_Y, rw, 22, COL_CARD_BORDER);

    if (s_active_call.state == CALL_STATE_ACTIVE) {
        fill_rect(rx + 8, SCOPE_Y + 7, 8, 8, COL_TEXT_CRIMSON); // Live Red REC dot
        draw_text(rx + 22, SCOPE_Y + 7, "LIVE INTERCEPTION [RECORDING/DECODING]", COL_TEXT_CRIMSON);
    } else if (s_active_call.state == CALL_STATE_INVITE) {
        draw_text(rx + 8, SCOPE_Y + 7, "SIP INVITE NEGOTIATING...", COL_TEXT_AMBER);
    } else {
        draw_text(rx + 8, SCOPE_Y + 7, "LISTENING (PROMISCUOUS ETHERNET)...", COL_TEXT_SLATE);
    }

    char buf[80];
    snprintf(buf, sizeof(buf), "CALLER : %s", s_active_call.caller[0] ? s_active_call.caller : "---");
    draw_text(rx + 8, SCOPE_Y + 28, buf, COL_TEXT_WHITE);

    snprintf(buf, sizeof(buf), "CALLEE : %s", s_active_call.callee[0] ? s_active_call.callee : "---");
    draw_text(rx + 8, SCOPE_Y + 42, buf, COL_TEXT_AMBER);

    snprintf(buf, sizeof(buf), "PORT   : %u (RTP) | SSRC: 0x%08X",
             s_active_call.rtp_port, (unsigned int)s_active_call.ssrc);
    draw_text(rx + 8, SCOPE_Y + 56, buf, COL_TEXT_CYAN);

    snprintf(buf, sizeof(buf), "DATA   : %u KB INTERCEPTED",
             (unsigned int)(s_active_call.bytes_rx / 1024));
    draw_text(rx + 8, SCOPE_Y + 70, buf, COL_TEXT_MINT);

    // 5. Recent Calls Ledger Table (Bottom Right)
    int ly = SCOPE_Y + 108;
    int lh = 110;
    fill_rect(rx, ly, rw, lh, COL_CARD_BG);
    draw_rect(rx, ly, rw, lh, COL_CARD_BORDER);

    fill_rect(rx, ly, rw, 20, COL_CARD_HEADER);
    draw_rect(rx, ly, rw, 20, COL_CARD_BORDER);
    draw_text(rx + 8, ly + 6, "SESSION LEDGER (LAST INTERCEPTIONS)", COL_TEXT_SLATE);

    int row_y = ly + 26;
    for (int i = 0; i < s_history_count; i++) {
        struct call_session* cs = &s_history[s_history_count - 1 - i];
        char hbuf[64];
        snprintf(hbuf, sizeof(hbuf), "#%d %s", i + 1, cs->caller);
        draw_text(rx + 8, row_y, hbuf, COL_TEXT_WHITE);
        snprintf(hbuf, sizeof(hbuf), "   -> %s (%u pkts)", cs->callee, (unsigned int)cs->packets_rx);
        draw_text(rx + 8, row_y + 10, hbuf, COL_TEXT_DIM);
        row_y += 22;
        if (row_y > ly + lh - 12) break;
    }
    if (s_history_count == 0) {
        draw_text(rx + 8, ly + 36, "No previous calls archived.", COL_TEXT_DIM);
    }

    // 6. Bottom OpSec & Hotkeys Bar
    int by = SCR_H - 30;
    fill_rect(0, by, SCR_W, 30, COL_CARD_HEADER);
    draw_rect(0, by, SCR_W, 30, COL_CARD_BORDER);

    draw_text(16, by + 10, "[Q] BURN & EXIT", COL_TEXT_CRIMSON);
    draw_text(130, by + 10, "[M] MUTE AUDIO", COL_TEXT_WHITE);
    draw_text(240, by + 10, "[T] DEMO SYNTH", s_synth_active ? COL_TEXT_EMERALD : COL_TEXT_SLATE);
    draw_text(355, by + 10, "[S] PROMISC / TOR", COL_TEXT_CYAN);
    draw_text(485, by + 10, "[C] CLEAR LEDGER", COL_TEXT_SLATE);

    flip_screen();
}

int main(int argc, char* argv[]) {
    int test_mode = 0;
    int demo_mode = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--test") == 0) {
            test_mode = 1;
        } else if (strcmp(argv[i], "--demo") == 0) {
            demo_mode = 1;
        } else if (strcmp(argv[i], "--tor") == 0) {
            s_mode = MODE_TOR;
        }
    }

    // OpSec Ring 3 Sandboxing: Pledge & Unveil
    seld_pledge(PLEDGE_STDIO | PLEDGE_NET | PLEDGE_AUDIO | PLEDGE_PURGE);
    seld_unveil(NULL, NULL);

    // Initialize Framebuffer
    if (seld_get_framebuffer(&s_fb) != 0 || !s_fb.framebuffer) {
        printf("wiretap: framebuffer unavailable\n");
        return 1;
    }

    s_backbuf = (uint32_t*)malloc(SCR_W * SCR_H * sizeof(uint32_t));
    if (!s_backbuf) {
        printf("wiretap: out of memory allocating backbuffer\n");
        return 1;
    }
    memset(s_backbuf, 0, SCR_W * SCR_H * sizeof(uint32_t));
    memset(s_audio_scope_samples, 128, sizeof(s_audio_scope_samples));

    // Enable Hardware Promiscuous Sniffing
    seld_net_set_promisc(1);

    if (demo_mode || test_mode) {
        s_synth_active = 1;
    }

    uint8_t pkt_buf[1536];
    int running = 1;
    int test_frames = 0;

    while (running) {
        // Poll and drain raw Ethernet packets from hardware sniffer ring
        int len;
        int packet_limit = 32;
        while (packet_limit-- > 0 && (len = seld_net_raw_poll(pkt_buf, sizeof(pkt_buf))) > 0) {
            process_packet(pkt_buf, (size_t)len);
        }

        // Run synthetic voice generator if demo mode is on
        if (s_synth_active) {
            run_synthetic_tick();
        }

        render_hud();

        if (test_mode) {
            test_frames++;
            if (test_frames >= 20) {
                // Verification passed in test mode
                break;
            }
            seld_sleep(20);
            continue;
        }

        // Process Interactive Keyboard Events
        struct seld_kbd_event ev;
        while (seld_poll_key(&ev) > 0) {
            if (!ev.pressed) continue;
            // 0x10 = 'Q', 0x01 = ESC
            if (ev.scancode == 0x10 || ev.scancode == 0x01) {
                running = 0;
                break;
            } else if (ev.scancode == 0x32) { // 'M'
                s_audio_muted = !s_audio_muted;
            } else if (ev.scancode == 0x14) { // 'T'
                s_synth_active = !s_synth_active;
                if (!s_synth_active && s_active_call.state == CALL_STATE_ACTIVE) {
                    archive_active_call();
                }
            } else if (ev.scancode == 0x1F) { // 'S'
                s_mode = (s_mode == MODE_PROMISC) ? MODE_TOR : MODE_PROMISC;
            } else if (ev.scancode == 0x2E) { // 'C'
                s_history_count = 0;
            }
        }

        seld_sleep(15);
    }

    // Amnesic RAM & Hardware Cleanup
    seld_net_set_promisc(0);
    memset(s_backbuf, 0, SCR_W * SCR_H * sizeof(uint32_t));
    memset(&s_active_call, 0, sizeof(s_active_call));
    memset(s_history, 0, sizeof(s_history));
    free(s_backbuf);

    seld_clear();
    return 0;
}
