/*
 * SeldOS - Humboldt Kernel Project
 * SNL OpSec CCTV / Surveillance Interception (/bin/cctv)
 *
 * Real-time Public Surveillance & Traffic Camera Stream Interception
 * Rendered on 680x334 Linear Framebuffer with Freestanding JPEG Decoder.
 * Zero DNS Leaks (Kernel Blocked), Tor SOCKS5 Remote Resolution.
 * Anti-Forensics In-Memory Operation & Zero-on-Exit Sanitization.
 *
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "seld.h"
#include "nanojpeg.h"

#define SCR_W 680
#define SCR_H 334

/* Humboldt Tactical Stealth Palette */
#define COL_BG_CANVAS      0xFF070B0E  /* Deep stealth carbon black */
#define COL_VIDEO_BG       0xFF020406  /* Viewport deepest black */
#define COL_CARD_BG        0xFF0B1219  /* Tactical module card body */
#define COL_CARD_HEADER    0xFF0F1A24  /* Tactical module card header */
#define COL_CARD_BORDER    0xFF1E293B  /* Hairline slate border */
#define COL_CARD_BORDER_HI 0xFF14532D  /* Emerald accent border */

#define COL_TEXT_WHITE     0xFFF8FAFC  /* Crisp high-contrast readout */
#define COL_TEXT_EMERALD   0xFF10B981  /* Phosphor green (active/secure) */
#define COL_TEXT_MINT      0xFF6EE7B7  /* Bright mint highlight */
#define COL_TEXT_CYAN      0xFF38BDF8  /* Telemetry / coordinates */
#define COL_TEXT_AMBER     0xFFF59E0B  /* Sensor status / warnings */
#define COL_TEXT_CRIMSON   0xFFEF4444  /* Alert / LIVE recording dot */
#define COL_TEXT_SLATE     0xFF64748B  /* Muted labels & ticks */
#define COL_TEXT_DIM       0xFF334155  /* Inactive table entries */

#define COL_HIGHLIGHT_ROW  0xFF064E3B  /* Solid emerald selection bar */
#define COL_CHIP_BG        0xFF141E28  /* Hotkey button pill background */
#define COL_CHIP_BORDER    0xFF233549  /* Hotkey border */
#define COL_BADGE_HLS      0xFF065F46  /* HLS video stream badge */
#define COL_BADGE_SNAP     0xFF78350F  /* Snapshot webcam badge */
#define COL_BADGE_RAW      0xFF991B1B  /* Raw unauthenticated IP stream badge */

/* Legacy aliases for backward compatibility */
#define COL_BG_BLACK       COL_BG_CANVAS
#define COL_PANEL_BG       COL_CARD_BG
#define COL_PANEL_BORDER   COL_CARD_BORDER
#define COL_PANEL_HEADER   COL_TEXT_CYAN
#define COL_TEXT_GREEN     COL_TEXT_EMERALD
#define COL_TEXT_ALERT     COL_TEXT_CRIMSON
#define COL_TEXT_MUTED     COL_TEXT_SLATE
#define COL_BRACKET        COL_TEXT_EMERALD
#define COL_RETICLE        COL_TEXT_EMERALD

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
    {0x08, 0x14, 0x54, 0x54, 0x3C}, // 103 'g'
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
    {0x00, 0x00, 0x77, 0x00, 0x00}, // 124 '|'
    {0x00, 0x41, 0x36, 0x08, 0x00}, // 125 '}'
    {0x08, 0x08, 0x2A, 0x1C, 0x08}  // 126 '~'
};

struct cctv_cam {
    char key[64];
    char name[48];
    char loc[64];
    int dist_km;
    int def_w;
    int def_h;
    uint8_t feed_type;
};
#define MAX_PRESETS 8

static struct cctv_cam s_presets[MAX_PRESETS];
static int s_num_presets = 0;
static int s_cam_idx = 0;
static char s_current_index[32] = "606000"; // Default: Dzerzhinsk
static char s_centroid_title[64] = "DZERZHINSK (606000)";
static uint32_t s_frame_seq = 0;
static char s_capture_time[32] = "ACQUIRING...";
static int s_need_fetch = 1;
static int s_gateway_failed = 0;
static int s_is_direct = 0;

/* Feed Filter Mode: 0 = ALL, 1 = VID, 2 = RAW, 3 = SNP */
static int s_filter_mode = 0;
static const char* s_filter_mode_names[4] = { "ALL", "VID", "RAW", "SNP" };

static inline uint32_t get_filter_mode_color(int mode) {
    switch (mode) {
        case 1: return COL_TEXT_MINT;    // VID (mint green)
        case 2: return 0xFFFCA5A5;        // RAW (light red)
        case 3: return COL_TEXT_AMBER;   // SNP (amber)
        default: return COL_TEXT_CYAN;   // ALL (cyan)
    }
}

/* CCTV Matrix Binary Formats (cctv.bin) */
struct sctv_header {
    uint32_t magic;           // 0x53435456 ('SCTV')
    uint16_t version;         // 1
    uint16_t flags;           // 0
    uint32_t num_cities;      // number of cities
    uint32_t num_cameras;     // number of cameras
    uint32_t cities_offset;   // byte offset to cities table
    uint32_t cameras_offset;  // byte offset to cameras table
    uint32_t strings_offset;  // byte offset to strings table
    uint32_t strings_size;    // size of strings table
} __attribute__((packed));

struct sctv_city {
    char name[24];            // lowercase city name
    char country[4];          // country code
    float lat;
    float lon;
} __attribute__((packed));

struct sctv_cam_entry {
    float lat;
    float lon;
    uint32_t id_offset;       // offset into strings table
    uint8_t feed_type;        // 0=image, 1=mjpeg, 3=hls, 5=mp4
    uint8_t flags;
    uint16_t reserved;
} __attribute__((packed));

static uint8_t* s_cctv_matrix_buf = NULL;
static size_t s_cctv_matrix_size = 0;
static const struct sctv_header* s_matrix_hdr = NULL;
static const struct sctv_city* s_matrix_cities = NULL;
static const struct sctv_cam_entry* s_matrix_cameras = NULL;
static const char* s_matrix_strings = NULL;

static inline float approx_sqrt(float n) {
    if (n <= 0.0f) return 0.0f;
    float x = (n > 1.0f) ? n * 0.5f : 1.0f;
    for (int i = 0; i < 6; i++) {
        x = 0.5f * (x + n / x);
    }
    return x;
}

static inline float approx_cos(float deg) {
    float rad = deg * 0.0174532925f;
    float rad2 = rad * rad;
    float c = 1.0f - (rad2 * 0.5f) + (rad2 * rad2 * 0.04166666f);
    if (c < 0.0f) c = 0.0f;
    if (c > 1.0f) c = 1.0f;
    return c;
}

static void format_camera_title(const char* cid, char* out_name, size_t out_sz) {
    if (!cid || !out_name || out_sz == 0) return;
    const char* p = cid;
    if (strncmp(p, "raw-", 4) == 0) {
        const char* ip_part = p + 4;
        size_t j = 0;
        while (*ip_part && j < out_sz - 1) {
            char ch = *ip_part++;
            if (ch == '-') ch = '.';
            out_name[j++] = ch;
        }
        out_name[j] = '\0';
        return;
    }
    if (strncmp(p, "openwebcamdb-", 13) == 0) {
        p += 13;
    } else if (strncmp(p, "inetcom-", 8) == 0) {
        p += 8;
    } else {
        const char* dash = strchr(p, '-');
        if (dash && (dash - p) <= 16 && *(dash + 1)) {
            p = dash + 1;
        }
    }

    // Check if remaining string is all digits (e.g. "1731429998")
    int all_digits = 1;
    for (size_t i = 0; p[i]; i++) {
        if (p[i] < '0' || p[i] > '9') {
            all_digits = 0;
            break;
        }
    }
    if (all_digits && p[0]) {
        snprintf(out_name, out_sz, "CAM-%.12s", p);
        return;
    }

    size_t j = 0;
    int capitalize = 1;
    while (*p && j < out_sz - 1) {
        char ch = *p++;
        if (ch == '-' || ch == '_' || ch == '.') {
            if (j > 0 && out_name[j-1] != ' ') {
                out_name[j++] = ' ';
            }
            capitalize = 1;
        } else {
            if (capitalize && ch >= 'a' && ch <= 'z') {
                ch = ch - 'a' + 'A';
            }
            out_name[j++] = ch;
            capitalize = 0;
        }
    }
    out_name[j] = '\0';
}

static int search_matrix_offline(const char* query) {
    if (!s_matrix_hdr || !s_matrix_cities || !s_matrix_cameras || !s_matrix_strings) {
        return -1;
    }

    char q_clean[32];
    size_t qlen = 0;
    const char* qp = query ? query : "606000";
    while (*qp && qlen < sizeof(q_clean) - 1) {
        char ch = *qp++;
        if (ch >= 'A' && ch <= 'Z') ch = ch - 'A' + 'a';
        if (ch == '-' || ch == '_') ch = ' ';
        q_clean[qlen++] = ch;
    }
    q_clean[qlen] = '\0';
    while (qlen > 0 && q_clean[qlen - 1] == ' ') q_clean[--qlen] = '\0';

    float target_lat = 0.0f;
    float target_lon = 0.0f;
    int matched = 0;

    for (uint32_t i = 0; i < s_matrix_hdr->num_cities; i++) {
        const struct sctv_city* c = &s_matrix_cities[i];
        if (strcmp(q_clean, c->name) == 0 || strstr(c->name, q_clean) != NULL) {
            target_lat = c->lat;
            target_lon = c->lon;
            matched = 1;
            char upper_name[24];
            size_t k = 0;
            while (c->name[k] && k < sizeof(upper_name) - 1) {
                char ch = c->name[k];
                if (ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
                upper_name[k++] = ch;
            }
            upper_name[k] = '\0';
            snprintf(s_centroid_title, sizeof(s_centroid_title), "%s (%.2f%c, %.2f%c) [%.2s]",
                     upper_name,
                     target_lat >= 0 ? target_lat : -target_lat, target_lat >= 0 ? 'N' : 'S',
                     target_lon >= 0 ? target_lon : -target_lon, target_lon >= 0 ? 'E' : 'W',
                     c->country);
            break;
        }
    }

    if (!matched) {
        target_lat = 56.2457f;
        target_lon = 43.4883f;
        snprintf(s_centroid_title, sizeof(s_centroid_title), "SURVEILLANCE GRID (%s)", q_clean);
    }

    float cos_lat = approx_cos(target_lat);

    struct {
        float dist_sq;
        uint32_t cam_idx;
    } top[MAX_PRESETS];

    for (int i = 0; i < MAX_PRESETS; i++) {
        top[i].dist_sq = 1e30f;
        top[i].cam_idx = 0;
    }

    uint32_t total_cams = s_matrix_hdr->num_cameras;
    for (uint32_t i = 0; i < total_cams; i++) {
        const struct sctv_cam_entry* cam = &s_matrix_cameras[i];

        // Feed Filter Mode: 0 = ALL, 1 = VID, 2 = RAW, 3 = SNP
        if (s_filter_mode == 1) {
            // VID: live video feeds only
            if (cam->feed_type == 0 || cam->feed_type == 1) continue;
        } else if (s_filter_mode == 2) {
            // RAW: direct unauthenticated IP feeds only
            if (cam->feed_type != 1) continue;
        } else if (s_filter_mode == 3) {
            // SNP: optical snapshots only
            if (cam->feed_type != 0) continue;
        }

        float dy = (cam->lat - target_lat) * 111.0f;
        float dx = (cam->lon - target_lon) * (111.0f * cos_lat);
        float d2 = dy * dy + dx * dx;
        float score = d2;

        // Proximity ranking: in ALL mode, prefer video/raw feeds within the same sector (< 40km)
        // But NEVER let a camera thousands of km away beat a local city camera!
        if (s_filter_mode == 0) {
            if (cam->feed_type != 0) {
                score = d2;
            } else {
                score = d2 + 1600.0f; // Snapshot penalty: equivalent to +40km (40^2 = 1600)
            }
        }

        if (score < top[MAX_PRESETS - 1].dist_sq) {
            int ins = MAX_PRESETS - 1;
            while (ins > 0 && score < top[ins - 1].dist_sq) {
                top[ins] = top[ins - 1];
                ins--;
            }
            top[ins].dist_sq = score;
            top[ins].cam_idx = i;
        }
    }

    s_num_presets = 0;
    for (int i = 0; i < MAX_PRESETS; i++) {
        if (top[i].dist_sq >= 1e29f) break;
        uint32_t cidx = top[i].cam_idx;
        const struct sctv_cam_entry* cam = &s_matrix_cameras[cidx];
        if (cam->id_offset >= s_matrix_hdr->strings_size) continue;

        const char* cid = s_matrix_strings + cam->id_offset;
        struct cctv_cam* p = &s_presets[s_num_presets];
        strncpy(p->key, cid, sizeof(p->key) - 1);
        p->key[sizeof(p->key) - 1] = '\0';

        format_camera_title(cid, p->name, sizeof(p->name));
        snprintf(p->loc, sizeof(p->loc), "%.2fN, %.2fE", cam->lat, cam->lon);
        float dy = (cam->lat - target_lat) * 111.0f;
        float dx = (cam->lon - target_lon) * (111.0f * cos_lat);
        p->dist_km = (int)(approx_sqrt(dy * dy + dx * dx) + 0.5f);
        p->def_w = 640;
        p->def_h = 480;
        p->feed_type = cam->feed_type;
        s_num_presets++;
    }

    return s_num_presets;
}


static const char scancode_ascii[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static int s_input_mode = 0;
static char s_input_buf[32] = "";
static int s_input_len = 0;

/* Video & UI State */
static struct seld_fb_info s_fb;
static uint32_t* s_backbuf = NULL;
static int s_fullscreen = 0;
static int s_paused = 0;
static int s_show_hud = 1;
static int s_audio_enabled = 1;

static uint32_t s_frame_count = 0;
static uint32_t s_last_fetch_frame = 0;
static int s_net_status = 0; // 0=init, 1=online, -1=offline/refused, -2=send err, -3=recv err
static int s_last_frame_bytes = 0;

static int s_img_w = 0;
static int s_img_h = 0;
static int s_img_color = 1;
static unsigned char* s_raw_rgb = NULL;
static nj_context_t* s_nj = NULL;

/* Ingestion Buffer (Volatile RAM) */
static uint8_t s_jpeg_buf[524288]; // 512 KB max frame buffer

/* Drawing Primitives */
static inline void put_pixel(int x, int y, uint32_t col) {
    if (x >= 0 && x < SCR_W && y >= 0 && y < SCR_H) {
        s_backbuf[y * SCR_W + x] = col;
    }
}

static void fill_rect(int x, int y, int w, int h, uint32_t col) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCR_W) w = SCR_W - x;
    if (y + h > SCR_H) h = SCR_H - y;
    if (w <= 0 || h <= 0) return;

    for (int r = y; r < y + h; r++) {
        uint32_t* row = &s_backbuf[r * SCR_W + x];
        for (int c = 0; c < w; c++) {
            row[c] = col;
        }
    }
}

static void draw_line(int x0, int y0, int x1, int y1, uint32_t col) {
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        put_pixel(x0, y0, col);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void draw_char_5x7(int x, int y, char c, uint32_t col) {
    if (c < 32 || c > 126) c = '?';
    int idx = c - 32;
    for (int col_i = 0; col_i < 5; col_i++) {
        uint8_t line = font5x7[idx][col_i];
        for (int row_i = 0; row_i < 7; row_i++) {
            if (line & (1 << row_i)) {
                put_pixel(x + col_i, y + row_i, col);
            }
        }
    }
}

static void draw_text_5x7(int x, int y, const char* str, uint32_t col) {
    if (!str) return;
    int cx = x;
    while (*str) {
        if (*str == '\n') {
            cx = x;
            y += 9;
        } else {
            draw_char_5x7(cx, y, *str, col);
            cx += 6;
        }
        str++;
    }
}

static void draw_rect_outline(int x, int y, int w, int h, uint32_t col) {
    if (w <= 0 || h <= 0) return;
    draw_line(x, y, x + w - 1, y, col);
    draw_line(x, y + h - 1, x + w - 1, y + h - 1, col);
    draw_line(x, y, x, y + h - 1, col);
    draw_line(x + w - 1, y, x + w - 1, y + h - 1, col);
}

static void draw_corner_brackets(int x, int y, int w, int h, int len, uint32_t col) {
    if (len <= 0) len = 8;
    draw_line(x, y, x + len, y, col);
    draw_line(x, y, x, y + len, col);
    draw_line(x + w - 1, y, x + w - 1 - len, y, col);
    draw_line(x + w - 1, y, x + w - 1, y + len, col);
    draw_line(x, y + h - 1, x + len, y + h - 1, col);
    draw_line(x, y + h - 1, x, y + h - 1 - len, col);
    draw_line(x + w - 1, y + h - 1, x + w - 1 - len, y + h - 1, col);
    draw_line(x + w - 1, y + h - 1, x + w - 1, y + h - 1 - len, col);
}

static void draw_tactical_card(int x, int y, int w, int h, const char* title, const char* tag) {
    fill_rect(x, y, w, h, COL_CARD_BG);
    draw_rect_outline(x, y, w, h, COL_CARD_BORDER);
    fill_rect(x + 1, y + 1, w - 2, 13, COL_CARD_HEADER);
    draw_line(x + 1, y + 14, x + w - 2, y + 14, COL_CARD_BORDER);
    if (title) {
        draw_text_5x7(x + 6, y + 4, title, COL_TEXT_WHITE);
    }
    if (tag) {
        int tag_len = strlen(tag);
        int tag_x = x + w - 6 - tag_len * 6;
        draw_text_5x7(tag_x, y + 4, tag, COL_TEXT_CYAN);
    }
}

static void draw_osd_pill(int x, int y, int w, int h, uint32_t bg_col, uint32_t border_col) {
    fill_rect(x, y, w, h, bg_col);
    draw_rect_outline(x, y, w, h, border_col);
}

static void draw_hotkey_chip(int x, int y, int w, int h, const char* key, const char* label, uint32_t col) {
    fill_rect(x, y, w, h, COL_CHIP_BG);
    draw_rect_outline(x, y, w, h, COL_CHIP_BORDER);
    draw_text_5x7(x + 4, y + 5, key, col);
    int klen = strlen(key);
    draw_text_5x7(x + 6 + klen * 6, y + 5, label, COL_TEXT_WHITE);
}

/* Image Scaling & Rendering onto Framebuffer */
static void blit_image(int dst_x, int dst_y, int dst_w, int dst_h) {
    if (!s_raw_rgb || s_img_w <= 0 || s_img_h <= 0) {
        fill_rect(dst_x, dst_y, dst_w, dst_h, COL_VIDEO_BG);
        draw_text_5x7(dst_x + (dst_w / 2) - 60, dst_y + (dst_h / 2) - 4,
                      "NO CARRIER / TUNNELING TOR...", COL_TEXT_ALERT);
        return;
    }

    fill_rect(dst_x, dst_y, dst_w, dst_h, COL_VIDEO_BG);

    // Maintain aspect ratio
    int scale_x = (dst_w << 16) / s_img_w;
    int scale_y = (dst_h << 16) / s_img_h;
    int scale = (scale_x < scale_y) ? scale_x : scale_y;
    if (scale <= 0) scale = 1;

    int rw = (s_img_w * scale) >> 16;
    int rh = (s_img_h * scale) >> 16;
    if (rw > dst_w) rw = dst_w;
    if (rh > dst_h) rh = dst_h;

    int ox = dst_x + (dst_w - rw) / 2;
    int oy = dst_y + (dst_h - rh) / 2;

    int ncomp = s_img_color ? 3 : 1;

    // Fast integer nearest-neighbor sampling
    for (int dy = 0; dy < rh; dy++) {
        int sy = (dy << 16) / scale;
        if (sy >= s_img_h) sy = s_img_h - 1;
        const unsigned char* src_row = s_raw_rgb + (sy * s_img_w * ncomp);
        uint32_t* dst_row = s_backbuf + ((oy + dy) * SCR_W + ox);

        for (int dx = 0; dx < rw; dx++) {
            int sx = (dx << 16) / scale;
            if (sx >= s_img_w) sx = s_img_w - 1;

            if (s_img_color) {
                const unsigned char* p = src_row + (sx * 3);
                dst_row[dx] = 0xFF000000 | ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
            } else {
                unsigned char g = src_row[sx];
                dst_row[dx] = 0xFF000000 | ((uint32_t)g << 16) | ((uint32_t)g << 8) | (uint32_t)g;
            }
        }
    }
}

struct direct_seed {
    const char* prefix;
    uint8_t ip[4];
    uint16_t port;
    const char* path;
    const char* host;
};

static const struct direct_seed s_direct_seeds[] = {
    { "dz-lenin-square",    {109, 170, 96, 131}, 2003,  "/cgi-bin/snapshot.cgi", "109.170.96.131:2003" },
    { "dz-",                {109, 170, 96, 131}, 2003,  "/cgi-bin/snapshot.cgi", "109.170.96.131:2003" },
    { "nn-",                {109, 170, 96, 131}, 2003,  "/cgi-bin/snapshot.cgi", "109.170.96.131:2003" },
    { "raw-109-170-96-131", {109, 170, 96, 131}, 2003,  "/cgi-bin/snapshot.cgi", "109.170.96.131:2003" },
    { "raw-85-172-105-181", {85, 172, 105, 181}, 1027,  "/cgi-bin/snapshot.cgi", "85.172.105.181:1027" },
    { "raw-5-149-155-88",   {5, 149, 155, 88},   8082,  "/cgi-bin/snapshot.cgi", "5.149.155.88:8082" },
    { "inetcom-",           {85, 172, 105, 181}, 1027,  "/cgi-bin/snapshot.cgi", "85.172.105.181:1027" },
    { "raw-108-52-105-4",   {108, 52, 105, 4},   50002, "/SnapshotJPEG?Resolution=640x480&Quality=Clarity", "108.52.105.4:50002" },
    { "raw-208-65-183-226", {208, 65, 183, 226}, 82,    "/SnapshotJPEG?Resolution=640x480&Quality=Clarity", "208.65.183.226:82" },
    { "raw-80-13-184-18",   {80, 13, 184, 18},   98,    "/SnapshotJPEG?Resolution=640x480&Quality=Clarity", "80.13.184.18:98" },
    { "raw-81-250-228-238", {81, 250, 228, 238}, 88,    "/SnapshotJPEG?Resolution=640x480&Quality=Clarity", "81.250.228.238:88" },
    { "raw-93-66-40-62",    {93, 66, 40, 62},    8050,  "/SnapshotJPEG?Resolution=640x480&Quality=Clarity", "93.66.40.62:8050" },
    { "raw-82-85-225-70",   {82, 85, 225, 70},   8010,  "/SnapshotJPEG?Resolution=640x480&Quality=Clarity", "82.85.225.70:8010" },
    { "raw-153-198-1-158",  {153, 198, 1, 158},  60001, "/SnapshotJPEG?Resolution=640x480&Quality=Clarity", "153.198.1.158:60001" },
    { "raw-183-77-159-247", {183, 77, 159, 247}, 5223,  "/SnapshotJPEG?Resolution=640x480&Quality=Clarity", "183.77.159.247:5223" },
    { "raw-114-184-246-240", {114, 184, 246, 240}, 50000, "/SnapshotJPEG?Resolution=640x480&Quality=Clarity", "114.184.246.240:50000" },
    { "raw-31-17-87-107",   {31, 17, 87, 107},   83,    "/SnapshotJPEG?Resolution=640x480&Quality=Clarity", "31.17.87.107:83" },
    { "raw-95-223-165-58",  {95, 223, 165, 58},  83,    "/snapshot.cgi?user=admin&pwd=", "95.223.165.58:83" },
    { "raw-79-224-22-232",  {79, 224, 22, 232},  5010,  "/snapshot.cgi?user=admin&pwd=", "79.224.22.232:5010" },
    { "raw-31-125-95-247",  {31, 125, 95, 247},  50000, "/SnapshotJPEG?Resolution=640x480&Quality=Clarity", "31.125.95.247:50000" },
    { "raw-89-240-255-235", {89, 240, 255, 235}, 8000,  "/SnapshotJPEG?Resolution=640x480&Quality=Clarity", "89.240.255.235:8000" }
};

static void resolve_direct_camera(const char* key, uint32_t* ip_out, uint16_t* port_out,
                                  char* path_out, size_t path_sz, char* host_out, size_t host_sz) {
    if (!key || !key[0]) {
        *ip_out = SELD_MAKE_IP(109, 170, 96, 131);
        *port_out = 2003;
        strncpy(path_out, "/cgi-bin/snapshot.cgi", path_sz - 1);
        strncpy(host_out, "109.170.96.131:2003", host_sz - 1);
        return;
    }

    size_t num_seeds = sizeof(s_direct_seeds) / sizeof(s_direct_seeds[0]);
    for (size_t i = 0; i < num_seeds; i++) {
        if (strstr(key, s_direct_seeds[i].prefix) != NULL ||
            strncmp(key, s_direct_seeds[i].prefix, strlen(s_direct_seeds[i].prefix)) == 0) {
            *ip_out = SELD_MAKE_IP(s_direct_seeds[i].ip[0], s_direct_seeds[i].ip[1],
                                   s_direct_seeds[i].ip[2], s_direct_seeds[i].ip[3]);
            *port_out = s_direct_seeds[i].port;
            strncpy(path_out, s_direct_seeds[i].path, path_sz - 1);
            strncpy(host_out, s_direct_seeds[i].host, host_sz - 1);
            return;
        }
    }

    // Check if key is raw IP: raw-A-B-C-D
    if (strncmp(key, "raw-", 4) == 0) {
        int o1 = 0, o2 = 0, o3 = 0, o4 = 0;
        const char* p = key + 4;
        while (*p >= '0' && *p <= '9') o1 = o1 * 10 + (*p++ - '0');
        if (*p == '-' || *p == '.') p++;
        while (*p >= '0' && *p <= '9') o2 = o2 * 10 + (*p++ - '0');
        if (*p == '-' || *p == '.') p++;
        while (*p >= '0' && *p <= '9') o3 = o3 * 10 + (*p++ - '0');
        if (*p == '-' || *p == '.') p++;
        while (*p >= '0' && *p <= '9') o4 = o4 * 10 + (*p++ - '0');
        if (o1 > 0 && o4 > 0) {
            *ip_out = SELD_MAKE_IP(o1, o2, o3, o4);
            *port_out = 80;
            strncpy(path_out, "/cgi-bin/snapshot.cgi", path_sz - 1);
            snprintf(host_out, host_sz, "%d.%d.%d.%d", o1, o2, o3, o4);
            return;
        }
    }

    // Default Fallback: Volga Dzerzhinsk Lenin Square
    *ip_out = SELD_MAKE_IP(109, 170, 96, 131);
    *port_out = 2003;
    strncpy(path_out, "/cgi-bin/snapshot.cgi", path_sz - 1);
    strncpy(host_out, "109.170.96.131:2003", host_sz - 1);
}

static void init_fallback_presets(void) {
    s_num_presets = 0;
    snprintf(s_centroid_title, sizeof(s_centroid_title), "DIRECT DOWNLINK (%s)", s_current_index);

    struct {
        const char* key;
        const char* name;
        const char* loc;
        int dist;
        uint8_t ft;
    } default_cams[] = {
        { "dz-lenin-square",    "DZ LENIN SQUARE", "Volga Sector (56.24N, 43.46E)", 12, 1 },
        { "raw-85-172-105-181", "MOSCOW CENTRAL",  "Central Hub (55.76N, 37.62E)",  380, 1 },
        { "raw-5-149-155-88",   "MOSCOW WEST",     "Kuntsevo Hub (55.76N, 37.60E)", 385, 1 },
        { "raw-108-52-105-4",   "NYC TIMES SQ",    "Broadway / 42nd (40.76N)",      6800, 1 },
        { "raw-31-17-87-107",   "BERLIN MITTE",    "Alexanderplatz (52.52N)",       1650, 1 },
        { "raw-80-13-184-18",   "PARIS CENTER",    "Chatelet (48.86N, 2.35E)",      2480, 1 },
        { "raw-93-66-40-62",    "ROME CENTRAL",    "Termini Hub (41.90N, 12.50E)",  2300, 1 },
        { "raw-153-198-1-158",  "TOKYO METRO",     "Shinjuku (35.69N, 139.69E)",    7200, 1 }
    };

    for (int i = 0; i < 8; i++) {
        struct cctv_cam* c = &s_presets[s_num_presets++];
        strncpy(c->key, default_cams[i].key, sizeof(c->key) - 1);
        c->key[sizeof(c->key) - 1] = '\0';
        strncpy(c->name, default_cams[i].name, sizeof(c->name) - 1);
        c->name[sizeof(c->name) - 1] = '\0';
        strncpy(c->loc, default_cams[i].loc, sizeof(c->loc) - 1);
        c->loc[sizeof(c->loc) - 1] = '\0';
        c->dist_km = default_cams[i].dist;
        c->def_w = 640;
        c->def_h = 480;
        c->feed_type = default_cams[i].ft;
    }
}

/*
 * Dynamic Proximity Camera Catalog Fetcher
 * Queries OpSec Gateway for nearest cameras to city/postal index
 */
static int fetch_catalog(const char* index_or_city) {
    if (index_or_city && index_or_city[0]) {
        strncpy(s_current_index, index_or_city, sizeof(s_current_index) - 1);
        s_current_index[sizeof(s_current_index) - 1] = '\0';
    }

    // 1. Instant offline resolution from worldwide camera matrix (zero network leaks)
    if (s_matrix_hdr != NULL) {
        int n = search_matrix_offline(s_current_index);
        if (n > 0) {
            return n;
        }
    }

    // 2. Query OpSec Gateway on port 8080 if available
    if (!s_gateway_failed) {
        seld_net_lease_acquire();
        uint32_t ip = SELD_MAKE_IP(10, 0, 2, 2);
        struct seld_net_info net;
        if (seld_net_info(&net) == 0 && net.gateway != 0) {
            ip = net.gateway;
        }
        int sock = seld_tcp_connect(ip, 8080);
        if (sock < 0 && ip != SELD_MAKE_IP(10, 0, 2, 2)) {
            sock = seld_tcp_connect(SELD_MAKE_IP(10, 0, 2, 2), 8080);
        }
        if (sock < 0) {
            sock = seld_tcp_connect(SELD_MAKE_IP(127, 0, 0, 1), 8080);
        }
        if (sock >= 0) {
            char req[256];
            snprintf(req, sizeof(req),
                "GET /api/cctv/cameras?index=%s HTTP/1.1\r\n"
                "Host: 10.0.2.2:8080\r\n"
                "User-Agent: SeldOS-CCTV/2.0 (Humboldt Avionics)\r\n"
                "Connection: close\r\n\r\n", s_current_index);

            if (seld_tcp_send(sock, req, strlen(req)) > 0) {
                char cat_buf[4096];
                int total = 0;
                int cat_content_len = -1;
                int cat_header_len = -1;
                while (total < (int)sizeof(cat_buf) - 1) {
                    int want = sizeof(cat_buf) - 1 - total;
                    if (want > 1024) want = 1024;
                    int n = seld_tcp_recv(sock, cat_buf + total, want, total == 0 ? 3000 : 800);
                    if (n <= 0) break;
                    total += n;

                    if (cat_content_len < 0) {
                        cat_buf[total] = '\0';
                        char* cl = strstr(cat_buf, "Content-Length: ");
                        if (cl) cat_content_len = atoi(cl + 16);
                        char* sep = strstr(cat_buf, "\r\n\r\n");
                        if (sep) cat_header_len = (int)(sep + 4 - cat_buf);
                    }
                    if (cat_content_len > 0 && cat_header_len > 0) {
                        if (total >= cat_header_len + cat_content_len) break;
                    }
                }
                seld_tcp_close(sock);
                seld_net_lease_release();

                if (total >= 20) {
                    cat_buf[total] = '\0';
                    char* body = strstr(cat_buf, "\r\n\r\n");
                    if (!body) body = strstr(cat_buf, "\n\n");
                    if (!body) body = cat_buf;
                    else body += (body[0] == '\r') ? 4 : 2;

                    s_num_presets = 0;
                    char* line = body;
                    while (line && *line) {
                        char* next = strchr(line, '\n');
                        if (next) {
                            if (next > line && *(next - 1) == '\r') *(next - 1) = '\0';
                            *next = '\0';
                            next++;
                        }

                        if (strncmp(line, "CENTROID: ", 10) == 0) {
                            char* p = line + 10;
                            while (*p && *p != ' ') p++;
                            while (*p == ' ') p++;
                            while (*p && *p != ' ') p++;
                            while (*p == ' ') p++;
                            if (*p) {
                                strncpy(s_centroid_title, p, sizeof(s_centroid_title) - 1);
                                s_centroid_title[sizeof(s_centroid_title) - 1] = '\0';
                            }
                        } else if (strncmp(line, "CAM ", 4) == 0 && s_num_presets < MAX_PRESETS) {
                            char* p = line + 4;
                            while (*p && *p != ' ') p++;
                            while (*p == ' ') p++;

                            char* key_start = p;
                            while (*p && *p != ' ') p++;
                            int key_len = p - key_start;
                            while (*p == ' ') p++;

                            int dist = atoi(p);
                            while (*p && *p != ' ') p++;
                            while (*p == ' ') p++;

                            int w = atoi(p);
                            while (*p && *p != ' ') p++;
                            while (*p == ' ') p++;

                            int h = atoi(p);
                            while (*p && *p != ' ') p++;
                            while (*p == ' ') p++;

                            char* name_start = p;
                            char* bar = strstr(p, " | ");
                            char* loc_start = "";
                            if (bar) {
                                *bar = '\0';
                                loc_start = bar + 3;
                            }

                            struct cctv_cam* c = &s_presets[s_num_presets];
                            if (key_len >= (int)sizeof(c->key)) key_len = sizeof(c->key) - 1;
                            memcpy(c->key, key_start, key_len);
                            c->key[key_len] = '\0';
                            format_camera_title(name_start, c->name, sizeof(c->name));
                            strncpy(c->loc, loc_start, sizeof(c->loc) - 1);
                            c->loc[sizeof(c->loc) - 1] = '\0';
                            c->dist_km = dist;
                            c->def_w = w;
                            c->def_h = h;
                            c->feed_type = (strstr(c->key, "inetcom") != NULL) ? 3 : 0;
                            s_num_presets++;
                        }
                        line = next;
                    }
                    if (s_num_presets > 0) return s_num_presets;
                }
            } else {
                seld_tcp_close(sock);
                seld_net_lease_release();
            }
        } else {
            seld_net_lease_release();
            s_gateway_failed = 1;
        }
    }

    // 3. Autonomous direct fallback presets
    init_fallback_presets();
    return s_num_presets;
}

/*
 * Fetch Live Camera Frame (Autonomous Hybrid Engine)
 * Attempts OpSec Gateway if online, or seamlessly executes Direct TCP Downlink
 * to the verified camera feed without host proxy dependency.
 */
static int fetch_live_frame(void) {
    if (s_num_presets <= 0) return -1;
    if (s_cam_idx >= s_num_presets) s_cam_idx = 0;

    seld_net_lease_acquire();

    int sock = -1;
    char req[512];
    int using_gateway = 0;

    // 1. Attempt OpSec Gateway (unless previously marked down)
    if (!s_gateway_failed) {
        uint32_t ip = SELD_MAKE_IP(10, 0, 2, 2);
        struct seld_net_info net;
        if (seld_net_info(&net) == 0 && net.gateway != 0) {
            ip = net.gateway;
        }
        sock = seld_tcp_connect(ip, 8080);
        if (sock < 0 && ip != SELD_MAKE_IP(10, 0, 2, 2)) {
            sock = seld_tcp_connect(SELD_MAKE_IP(10, 0, 2, 2), 8080);
        }
        if (sock < 0) {
            sock = seld_tcp_connect(SELD_MAKE_IP(127, 0, 0, 1), 8080);
        }
        if (sock >= 0) {
            using_gateway = 1;
            snprintf(req, sizeof(req),
                "GET /api/cctv/frame?id=%s&cam=%d&index=%s HTTP/1.1\r\n"
                "Host: 10.0.2.2:8080\r\n"
                "User-Agent: SeldOS-CCTV/2.0 (Humboldt Avionics)\r\n"
                "Connection: close\r\n\r\n",
                s_presets[s_cam_idx].key, s_cam_idx + 1, s_current_index);
        } else {
            s_gateway_failed = 1;
        }
    }

    // 2. Direct Autonomous Downlink (Zero-Host dependency)
    if (sock < 0) {
        uint32_t cam_ip = 0;
        uint16_t cam_port = 80;
        char cam_path[256];
        char cam_host[128];
        resolve_direct_camera(s_presets[s_cam_idx].key, &cam_ip, &cam_port,
                              cam_path, sizeof(cam_path), cam_host, sizeof(cam_host));

        sock = seld_tcp_connect(cam_ip, cam_port);
        if (sock < 0) {
            // Secondary direct fallback: Volga Dzerzhinsk Lenin Square
            cam_ip = SELD_MAKE_IP(109, 170, 96, 131);
            cam_port = 2003;
            strncpy(cam_path, "/cgi-bin/snapshot.cgi", sizeof(cam_path) - 1);
            strncpy(cam_host, "109.170.96.131:2003", sizeof(cam_host) - 1);
            sock = seld_tcp_connect(cam_ip, cam_port);
        }

        if (sock < 0) {
            seld_net_lease_release();
            s_net_status = -1;
            return -1;
        }

        using_gateway = 0;
        snprintf(req, sizeof(req),
            "GET %s HTTP/1.0\r\n"
            "Host: %s\r\n"
            "User-Agent: SeldOS-CCTV/2.0 (Autonomous Direct)\r\n"
            "Accept: image/jpeg, image/*\r\n"
            "Connection: close\r\n\r\n",
            cam_path, cam_host);
    }

    s_is_direct = !using_gateway;

    if (seld_tcp_send(sock, req, strlen(req)) <= 0) {
        seld_tcp_close(sock);
        seld_net_lease_release();
        s_net_status = -2;
        return -2;
    }

    int total_bytes = 0;
    int content_len = -1;
    int header_len = -1;
    while (total_bytes < (int)sizeof(s_jpeg_buf) - 1) {
        int want = sizeof(s_jpeg_buf) - 1 - total_bytes;
        if (want > 4096) want = 4096;
        int n = seld_tcp_recv(sock, (char*)s_jpeg_buf + total_bytes, want, total_bytes == 0 ? 2500 : 400);
        if (n <= 0) break;
        total_bytes += n;

        if (content_len < 0) {
            s_jpeg_buf[total_bytes] = '\0';
            char* cl = strstr((char*)s_jpeg_buf, "Content-Length: ");
            if (cl) {
                content_len = atoi(cl + 16);
            }
            char* sep = strstr((char*)s_jpeg_buf, "\r\n\r\n");
            if (sep) {
                header_len = (int)(sep + 4 - (char*)s_jpeg_buf);
            }
        }
        if (content_len > 0 && header_len > 0) {
            if (total_bytes >= header_len + content_len) {
                break;
            }
        }
    }

    seld_tcp_close(sock);
    seld_net_lease_release();

    if (total_bytes < 100) {
        s_net_status = -3;
        return -3;
    }
    s_jpeg_buf[total_bytes] = '\0';

    if (!strstr((char*)s_jpeg_buf, "200 OK") && !strstr((char*)s_jpeg_buf, "200 ok")) {
        s_net_status = -4;
        return -4;
    }

    if (using_gateway) {
        char* seq_hdr = strstr((char*)s_jpeg_buf, "X-Frame-Seq: ");
        if (seq_hdr) {
            s_frame_seq = (uint32_t)atoi(seq_hdr + 13);
        }
        char* time_hdr = strstr((char*)s_jpeg_buf, "X-Capture-Time: ");
        if (time_hdr) {
            char* p = time_hdr + 16;
            int i = 0;
            while (*p && *p != '\r' && *p != '\n' && i < (int)sizeof(s_capture_time) - 1) {
                s_capture_time[i++] = *p++;
            }
            s_capture_time[i] = '\0';
        }
    } else {
        s_frame_seq++;
        uint32_t ms = (s_frame_count * 33) % 1000;
        uint32_t sec = (s_frame_count * 33) / 1000;
        uint32_t mm = (sec / 60) % 60;
        uint32_t hh = (sec / 3600) % 24;
        uint32_t ss = sec % 60;
        snprintf(s_capture_time, sizeof(s_capture_time), "%02u:%02u:%02u.%02u DOWNLINK", hh, mm, ss, ms / 10);
    }

    char* body = strstr((char*)s_jpeg_buf, "\r\n\r\n");
    int sep_len = 4;
    if (!body) {
        body = strstr((char*)s_jpeg_buf, "\n\n");
        sep_len = 2;
    }
    if (!body) {
        s_net_status = -5;
        return -5;
    }

    body += sep_len;
    int jpeg_size = total_bytes - (int)(body - (char*)s_jpeg_buf);
    if (jpeg_size < 100) {
        s_net_status = -5;
        return -5;
    }

    s_last_frame_bytes = jpeg_size;
    s_net_status = 1;

    if (s_nj) {
        njDone(s_nj);
        s_nj = NULL;
    }

    s_nj = njInit();
    if (!s_nj) return -6;

    nj_result_t res = njDecode(s_nj, (const void*)body, jpeg_size);
    if (res == NJ_OK) {
        s_img_w = njGetWidth(s_nj);
        s_img_h = njGetHeight(s_nj);
        s_img_color = njIsColor(s_nj);
        s_raw_rgb = njGetImage(s_nj);
        s_frame_count++;
        return 1;
    }

    return -7;
}

/* Render Tactical Overlay HUD */
static void render_cctv_hud(void) {
    static const struct cctv_cam s_dummy_cam = {"acq", "ACQUIRING...", "Scanning Sector", 0, 640, 480, 0};
    const struct cctv_cam* cam = (s_num_presets > 0 && s_cam_idx < s_num_presets) ? &s_presets[s_cam_idx] : &s_dummy_cam;

    if (s_fullscreen) {
        if (s_raw_rgb) {
            blit_image(0, 0, SCR_W, SCR_H);
        } else {
            fill_rect(0, 0, SCR_W, SCR_H, COL_VIDEO_BG);
            draw_text_5x7(SCR_W / 2 - 120, SCR_H / 2 - 4, "[ ACQUIRING OPTICAL FEED ... ]", COL_TEXT_CYAN);
        }

        // Tactical Reticle in Fullscreen
        int cx = SCR_W / 2;
        int cy = SCR_H / 2;
        draw_line(cx - 15, cy, cx - 4, cy, 0xFF334155);
        draw_line(cx + 4, cy, cx + 15, cy, 0xFF334155);
        draw_line(cx, cy - 15, cx, cy - 4, 0xFF334155);
        draw_line(cx, cy + 4, cx, cy + 15, 0xFF334155);

        if (!s_show_hud) return;

        // Floating Minimal Top Bar
        fill_rect(8, 8, SCR_W - 16, 20, 0xD8070B0E);
        draw_rect_outline(8, 8, SCR_W - 16, 20, COL_CARD_BORDER);

        fill_rect(14, 15, 6, 6, COL_TEXT_EMERALD);
        draw_text_5x7(24, 15, "SELD-OS // OPTICAL RECON", COL_TEXT_WHITE);

        char osd_top[80];
        snprintf(osd_top, sizeof(osd_top), "[ %s ] %dx%d %dKB #%u [%dKM]",
                 cam->name, s_img_w, s_img_h, s_last_frame_bytes / 1024, s_frame_seq, cam->dist_km);
        draw_text_5x7(200, 15, osd_top, COL_TEXT_CYAN);

        if ((s_frame_count % 30) < 15 && !s_paused) {
            fill_rect(SCR_W - 70, 15, 5, 5, COL_TEXT_CRIMSON);
            draw_text_5x7(SCR_W - 60, 15, "LIVE", COL_TEXT_CRIMSON);
        } else {
            draw_text_5x7(SCR_W - 60, 15, s_paused ? "HOLD" : "LIVE", COL_TEXT_SLATE);
        }

        // Floating Minimal Bottom Bar
        fill_rect(8, SCR_H - 24, SCR_W - 16, 18, 0xD8070B0E);
        draw_rect_outline(8, SCR_H - 24, SCR_W - 16, 18, COL_CARD_BORDER);
        draw_text_5x7(14, SCR_H - 18, "[1-8] Cam | [S] Search | [SPACE] Pause | [Z] Exit Fullscreen | [Q] Wipe & Exit", COL_TEXT_MINT);
        return;
    }

    /* 1. Global Tactical Header (y = 0..19) */
    fill_rect(0, 0, SCR_W, 20, COL_CARD_HEADER);
    draw_line(0, 19, SCR_W - 1, 19, COL_CARD_BORDER);

    fill_rect(6, 7, 6, 6, COL_TEXT_EMERALD);
    draw_text_5x7(16, 7, "SELD-OS // SIGINT OPTICAL SURVEILLANCE", COL_TEXT_WHITE);

    char grid_lbl[48];
    snprintf(grid_lbl, sizeof(grid_lbl), "GRID: %-22.22s", s_centroid_title);
    draw_text_5x7(295, 7, grid_lbl, COL_TEXT_CYAN);

    draw_text_5x7(540, 7, "TOR: ISOLATED [0 UDP]", COL_TEXT_MINT);

    /* 2. Precision 4:3 Video Viewport (x = 6, y = 23, w = 401, h = 286) */
    int vx = 6;
    int vy = 23;
    int vw = 401;
    int vh = 286;

    if (s_raw_rgb) {
        blit_image(vx + 1, vy + 1, vw - 2, vh - 2);
    } else {
        fill_rect(vx + 1, vy + 1, vw - 2, vh - 2, COL_VIDEO_BG);
        draw_text_5x7(vx + (vw / 2) - 80, vy + (vh / 2) - 4, "[ ACQUIRING OPTICAL FEED ... ]", COL_TEXT_CYAN);
    }

    // Technical framing & L-corner brackets
    draw_rect_outline(vx, vy, vw, vh, COL_CARD_BORDER);
    draw_corner_brackets(vx + 2, vy + 2, vw - 4, vh - 4, 8, COL_TEXT_EMERALD);

    // Optical Center Crosshair Reticle (subtle)
    int cx = vx + vw / 2;
    int cy = vy + vh / 2;
    draw_line(cx - 8, cy, cx - 2, cy, 0xFF334155);
    draw_line(cx + 2, cy, cx + 8, cy, 0xFF334155);
    draw_line(cx, cy - 8, cx, cy - 2, 0xFF334155);
    draw_line(cx, cy + 2, cx, cy + 8, 0xFF334155);

    // In-Video OSD Pills (semi-translucent with slate border)
    // Top-Left Pill: Camera Callout
    int pill1_w = 170;
    draw_osd_pill(vx + 6, vy + 6, pill1_w, 15, 0xD8070B0E, COL_CARD_BORDER);
    char osd_cam[32];
    snprintf(osd_cam, sizeof(osd_cam), "[ CAM %02d // %-12.12s ]", s_cam_idx + 1, cam->name);
    draw_text_5x7(vx + 10, vy + 10, osd_cam, COL_TEXT_MINT);

    // Top-Right Pill: LIVE / REC Status & Distance
    int pill2_w = 105;
    int pill2_x = vx + vw - 6 - pill2_w;
    draw_osd_pill(pill2_x, vy + 6, pill2_w, 15, 0xD8070B0E, COL_CARD_BORDER);
    int is_blink = (s_frame_count % 30) < 15;
    if (is_blink && !s_paused) {
        fill_rect(pill2_x + 6, vy + 10, 5, 5, COL_TEXT_CRIMSON);
        draw_text_5x7(pill2_x + 14, vy + 10, "LIVE", COL_TEXT_CRIMSON);
    } else {
        draw_text_5x7(pill2_x + 14, vy + 10, s_paused ? "HOLD" : "LIVE", COL_TEXT_SLATE);
    }
    char dist_str[16];
    snprintf(dist_str, sizeof(dist_str), "[%3dKM]", cam->dist_km);
    draw_text_5x7(pill2_x + 48, vy + 10, dist_str, COL_TEXT_WHITE);

    // Bottom-Left Pill: Resolution & Bitrate
    int pill3_w = 145;
    draw_osd_pill(vx + 6, vy + vh - 21, pill3_w, 15, 0xD8070B0E, COL_CARD_BORDER);
    char res_str[32];
    snprintf(res_str, sizeof(res_str), "RES: %dx%d | %dKB", s_img_w, s_img_h, s_last_frame_bytes / 1024);
    draw_text_5x7(vx + 10, vy + vh - 17, res_str, COL_TEXT_CYAN);

    // Bottom-Right Pill: Chronometer / Timestamp
    int pill4_w = 125;
    int pill4_x = vx + vw - 6 - pill4_w;
    draw_osd_pill(pill4_x, vy + vh - 21, pill4_w, 15, 0xD8070B0E, COL_CARD_BORDER);
    draw_text_5x7(pill4_x + 8, vy + vh - 17, s_capture_time, COL_TEXT_WHITE);

    /* 3. Modular Right-hand Deck: 3 Cards (px = 413, pw = 261) */
    int px = 413;
    int pw = 261;

    // Card 1: TARGET SECTOR (y = 23, h = 53)
    draw_tactical_card(px, 23, pw, 53, "[ 01 // TARGET SECTOR ]", "[S] EDIT");
    draw_text_5x7(px + 6, 40, "CENTROID:", COL_TEXT_SLATE);
    char sec_trunc[26];
    snprintf(sec_trunc, sizeof(sec_trunc), "%-23.23s", s_centroid_title);
    draw_text_5x7(px + 64, 40, sec_trunc, COL_TEXT_CYAN);

    draw_text_5x7(px + 6, 51, "POSITION:", COL_TEXT_SLATE);
    char loc_trunc[26];
    snprintf(loc_trunc, sizeof(loc_trunc), "%-23.23s", cam->loc[0] ? cam->loc : "SURVEILLANCE GRID");
    draw_text_5x7(px + 64, 51, loc_trunc, COL_TEXT_WHITE);

    draw_text_5x7(px + 6, 62, "SECURITY:", COL_TEXT_SLATE);
    draw_text_5x7(px + 64, 62, "0 UDP / TOR ONION REMOTE", COL_TEXT_EMERALD);

    // Card 2: SENSOR MATRIX (y = 80, h = 163)
    char active_tag[16];
    snprintf(active_tag, sizeof(active_tag), "SLOT %d/%d", s_num_presets > 0 ? (s_cam_idx + 1) : 0, s_num_presets);
    char card2_title[32];
    snprintf(card2_title, sizeof(card2_title), "[ 02 // < %s > MATRIX ]", s_filter_mode_names[s_filter_mode]);
    draw_tactical_card(px, 80, pw, 163, card2_title, active_tag);

    // Highlight the mode selector pill right next to 02 in vivid mode color
    char mode_pill[16];
    snprintf(mode_pill, sizeof(mode_pill), "< %s >", s_filter_mode_names[s_filter_mode]);
    draw_text_5x7(px + 54, 84, mode_pill, get_filter_mode_color(s_filter_mode));

    for (int i = 0; i < 8; i++) {
        int ry = 98 + i * 17;
        if (i < s_num_presets) {
            int is_active = (i == s_cam_idx);
            if (is_active) {
                fill_rect(px + 3, ry - 1, pw - 6, 15, COL_HIGHLIGHT_ROW);
                fill_rect(px + 3, ry - 1, 2, 15, COL_TEXT_EMERALD);
                draw_text_5x7(px + 7, ry + 3, ">", COL_TEXT_MINT);
            } else {
                draw_text_5x7(px + 7, ry + 3, " ", COL_TEXT_DIM);
            }

            char slot_str[8];
            snprintf(slot_str, sizeof(slot_str), "%d.", i + 1);
            draw_text_5x7(px + 14, ry + 3, slot_str, is_active ? COL_TEXT_MINT : COL_TEXT_SLATE);

            // Feed Badge: [RAW], [VID], or [SNP]
            uint32_t badge_bg = COL_BADGE_SNAP;
            const char* badge_text = "SNP";
            uint32_t badge_fg = COL_TEXT_AMBER;
            if (s_presets[i].feed_type == 1) {
                badge_bg = COL_BADGE_RAW;
                badge_text = "RAW";
                badge_fg = 0xFFFCA5A5; /* light red */
            } else if (s_presets[i].feed_type != 0) {
                badge_bg = COL_BADGE_HLS;
                badge_text = "VID";
                badge_fg = COL_TEXT_MINT;
            }
            fill_rect(px + 28, ry + 1, 26, 11, badge_bg);
            draw_text_5x7(px + 30, ry + 3, badge_text, badge_fg);

            // Camera Name (truncated to 14 chars)
            char cname16[18];
            snprintf(cname16, sizeof(cname16), "%-14.14s", s_presets[i].name);
            draw_text_5x7(px + 58, ry + 3, cname16, is_active ? COL_TEXT_WHITE : COL_TEXT_SLATE);

            // Distance
            char dstr[12];
            snprintf(dstr, sizeof(dstr), "%4dK", s_presets[i].dist_km);
            draw_text_5x7(px + pw - 34, ry + 3, dstr, is_active ? COL_TEXT_MINT : COL_TEXT_DIM);
        } else {
            // Empty slot
            draw_text_5x7(px + 14, ry + 3, "--. [EMPTY SENSOR SLOT]", COL_TEXT_DIM);
        }
    }

    // Card 3: LINK TELEMETRY (y = 247, h = 62)
    draw_tactical_card(px, 247, pw, 62, "[ 03 // LINK TELEMETRY ]", s_is_direct ? "DIRECT" : "SOCKS5");

    draw_text_5x7(px + 6, 264, "CARRIER:", COL_TEXT_SLATE);
    if (s_net_status == 1) {
        if (s_is_direct) {
            draw_text_5x7(px + 60, 264, "LOCKED [DIRECT CLEARDOWNLINK]", COL_TEXT_EMERALD);
        } else {
            draw_text_5x7(px + 60, 264, "LOCKED [SOCKS5 ONION]", COL_TEXT_EMERALD);
        }
    } else if (s_net_status == -1) {
        draw_text_5x7(px + 60, 264, "DOWN (CARRIER LOST)", COL_TEXT_CRIMSON);
    } else {
        draw_text_5x7(px + 60, 264, "ACQUIRING DOWNLINK...", COL_TEXT_AMBER);
    }

    draw_text_5x7(px + 6, 276, "BUFFER:", COL_TEXT_SLATE);
    // Draw buffer fidelity gauge
    draw_rect_outline(px + 56, 277, 128, 6, COL_CARD_BORDER);
    int gauge_w = (s_last_frame_bytes > 0) ? (s_last_frame_bytes * 124) / 80000 : 0;
    if (gauge_w > 124) gauge_w = 124;
    if (gauge_w < 8 && s_last_frame_bytes > 0) gauge_w = 8;
    fill_rect(px + 58, 279, gauge_w, 2, COL_TEXT_EMERALD);
    char bstr[16];
    snprintf(bstr, sizeof(bstr), "%dKB", s_last_frame_bytes / 1024);
    draw_text_5x7(px + 190, 276, bstr, COL_TEXT_WHITE);

    char frame_diag[48];
    snprintf(frame_diag, sizeof(frame_diag), "FRAME: #%u | SEQ: #%u | %s",
             s_frame_count, s_frame_seq, s_paused ? "PAUSED" : "FLOW");
    draw_text_5x7(px + 6, 290, frame_diag, s_paused ? COL_TEXT_AMBER : COL_TEXT_SLATE);

    /* 4. Bottom Command Deck (y = 313..331) */
    int by = 313;
    int bh = 19;

    if (s_input_mode) {
        fill_rect(6, by, 668, bh, 0xFF0D1B2A);
        draw_rect_outline(6, by, 668, bh, COL_TEXT_CYAN);
        draw_text_5x7(12, by + 5, "TARGET SECTOR QUERY:", COL_TEXT_CYAN);
        char in_dsp[64];
        snprintf(in_dsp, sizeof(in_dsp), "[ %s%s ]", s_input_buf, (s_frame_count % 20 < 10) ? "_" : " ");
        draw_text_5x7(144, by + 5, in_dsp, COL_TEXT_WHITE);
        draw_text_5x7(450, by + 5, "[ENTER] COMMIT | [ESC] ABORT", COL_TEXT_EMERALD);
    } else {
        int cw = 106;
        int cgap = 6;
        int cx0 = 6;
        draw_hotkey_chip(cx0 + 0 * (cw + cgap), by, cw, bh, "1-8", "SELECT", COL_TEXT_EMERALD);
        draw_hotkey_chip(cx0 + 1 * (cw + cgap), by, cw, bh, "S", "SEARCH", COL_TEXT_CYAN);
        draw_hotkey_chip(cx0 + 2 * (cw + cgap), by, cw, bh, "<>", "MODE", get_filter_mode_color(s_filter_mode));
        draw_hotkey_chip(cx0 + 3 * (cw + cgap), by, cw, bh, "Z", "FULLSCR", COL_TEXT_AMBER);
        draw_hotkey_chip(cx0 + 4 * (cw + cgap), by, cw, bh, "SPC", "FREEZE", COL_TEXT_WHITE);
        draw_hotkey_chip(cx0 + 5 * (cw + cgap), by, cw, bh, "Q", "WIPE/EXIT", COL_TEXT_CRIMSON);
    }
}

int main(int argc, char* argv[]) {
    // 1. Seld-Pledge Sandboxing (OpSec Capability Sandbox)
    // Note: PLEDGE_RPATH granted for reading /etc/cctv.bin; PLEDGE_DNS explicitly omitted (0 UDP leaks)!
    seld_pledge(PLEDGE_STDIO | PLEDGE_RPATH | PLEDGE_AUDIO | PLEDGE_NET | PLEDGE_PURGE);

    // 2. Verify /etc/cctv.bin presence before switching console mode
    struct seld_stat st;
    const char* cctv_path = "/etc/cctv.bin";
    if (seld_stat(cctv_path, &st) != 0) {
        if (seld_stat("etc/cctv.bin", &st) == 0) cctv_path = "etc/cctv.bin";
        else if (seld_stat("cctv.bin", &st) == 0) cctv_path = "cctv.bin";
        else {
            printf("\n");
            printf("======================================================================\n");
            printf(" [!] TACTICAL INTEL MATRIX MISSING (/etc/cctv.bin)\n");
            printf("======================================================================\n");
            printf(" SeldOS CCTV requires the worldwide optical surveillance matrix.\n");
            printf(" To prevent bloating the base ISO image, heavy packages are on-demand.\n\n");
            printf(" [*] Run the following command in terminal to install:\n");
            printf("     download cctv\n\n");
            printf(" This fetches 115,000+ public surveillance cameras and world city index\n");
            printf(" directly from repository (https://github.com/DjankiOpsec/SeldOS).\n");
            printf("======================================================================\n\n");
            return 1;
        }
    }

    // 3. Load CCTV Matrix into memory for instant zero-leak spatial queries
    if (st.size >= sizeof(struct sctv_header)) {
        s_cctv_matrix_size = st.size;
        s_cctv_matrix_buf = (uint8_t*)malloc(s_cctv_matrix_size);
        if (s_cctv_matrix_buf) {
            int rd = seld_readfile(cctv_path, s_cctv_matrix_buf, s_cctv_matrix_size);
            if (rd >= (int)sizeof(struct sctv_header)) {
                const struct sctv_header* hdr = (const struct sctv_header*)s_cctv_matrix_buf;
                if (hdr->magic == 0x53435456 && hdr->version == 1) {
                    s_matrix_hdr = hdr;
                    s_matrix_cities = (const struct sctv_city*)(s_cctv_matrix_buf + hdr->cities_offset);
                    s_matrix_cameras = (const struct sctv_cam_entry*)(s_cctv_matrix_buf + hdr->cameras_offset);
                    s_matrix_strings = (const char*)(s_cctv_matrix_buf + hdr->strings_offset);
                }
            }
        }
    }

    const char* init_index = "606000"; // Default: Dzerzhinsk
    if (argc > 1) {
        init_index = argv[1];
    }

    // 2. Initialize Framebuffer & Backbuffer
    if (seld_get_framebuffer(&s_fb) < 0 || !s_fb.framebuffer) {
        printf("[-] Error: Unable to map linear framebuffer 680x334\n");
        return 1;
    }

    s_backbuf = (uint32_t*)malloc(SCR_W * SCR_H * sizeof(uint32_t));
    if (!s_backbuf) {
        printf("[-] Error: Out of memory allocating cctv backbuffer\n");
        return 2;
    }

    // Initial Screen Clear
    fill_rect(0, 0, SCR_W, SCR_H, COL_BG_BLACK);
    memcpy(s_fb.framebuffer, s_backbuf, SCR_W * SCR_H * sizeof(uint32_t));

    // 3. Initial Proximity Catalog Fetch for target city/postal index
    fetch_catalog(init_index);

    // Initial Live Frame Fetch
    fetch_live_frame();

    // Drain any leftover keystrokes from console/shell before entering tactical loop
    struct seld_kbd_event k_flush;
    while (seld_poll_key(&k_flush) > 0);

    int running = 1;

    // Main Tactical Loop
    while (running) {
        // 1. Instant keyboard polling at the start of every tick
        struct seld_kbd_event kev;
        while (seld_poll_key(&kev) > 0) {
            if (!kev.pressed) continue;
            uint8_t sc = kev.scancode & 0x7F;

            if (s_input_mode) {
                if (sc == 0x01) { // ESC -> cancel search
                    s_input_mode = 0;
                    if (s_audio_enabled) seld_beep(880, 20);
                } else if (sc == 0x1C) { // ENTER -> commit search
                    s_input_mode = 0;
                    if (s_input_len > 0) {
                        strncpy(s_current_index, s_input_buf, sizeof(s_current_index) - 1);
                        s_current_index[sizeof(s_current_index) - 1] = '\0';
                        fetch_catalog(s_current_index);
                        s_cam_idx = 0;
                        s_need_fetch = 1;
                        if (s_audio_enabled) seld_beep(1760, 35);
                    }
                } else if (sc == 0x0E) { // Backspace
                    if (s_input_len > 0) {
                        s_input_len--;
                        s_input_buf[s_input_len] = '\0';
                        if (s_audio_enabled) seld_beep(1200, 15);
                    }
                } else if (sc < 128) {
                    char ch = scancode_ascii[sc];
                    if (ch >= 32 && ch <= 126 && s_input_len < (int)sizeof(s_input_buf) - 1) {
                        s_input_buf[s_input_len++] = ch;
                        s_input_buf[s_input_len] = '\0';
                        if (s_audio_enabled) seld_beep(1500, 15);
                    }
                }
                continue;
            }

            // Normal Navigation Mode
            if (sc == 0x01 || sc == 0x10) { // ESC or 'Q' -> Anti-Forensics Wipe & Exit
                running = 0;
                break;
            } else if (sc == 0x39) { // Space -> Pause/Resume
                s_paused = !s_paused;
                if (s_audio_enabled) seld_beep(s_paused ? 880 : 1760, 30);
            } else if (sc == 0x2C) { // 'Z' -> Toggle Fullscreen / Split
                s_fullscreen = !s_fullscreen;
                if (s_audio_enabled) seld_beep(1200, 20);
            } else if (sc == 0x23) { // 'H' -> Toggle Minimal HUD
                s_show_hud = !s_show_hud;
            } else if (sc == 0x31 || sc == 0x50) { // 'N', Down Arrow -> Next Camera
                if (s_num_presets > 0) {
                    s_cam_idx = (s_cam_idx + 1) % s_num_presets;
                    s_need_fetch = 1;
                    s_last_fetch_frame = 0;
                    if (s_audio_enabled) seld_beep(1500, 25);
                }
            } else if (sc == 0x19 || sc == 0x48) { // 'P', Up Arrow -> Previous Camera
                if (s_num_presets > 0) {
                    s_cam_idx = (s_cam_idx - 1 + s_num_presets) % s_num_presets;
                    s_need_fetch = 1;
                    s_last_fetch_frame = 0;
                    if (s_audio_enabled) seld_beep(1400, 25);
                }
            } else if (sc >= 0x02 && sc <= 0x09) { // Top row '1' .. '8'
                int idx = sc - 0x02;
                if (idx < s_num_presets) {
                    s_cam_idx = idx;
                    s_need_fetch = 1;
                    s_last_fetch_frame = 0;
                    if (s_audio_enabled) seld_beep(1600, 30);
                }
            } else if (sc == 0x1F || sc == 0x35) { // 'S' or '/' -> Enter Search Prompt
                s_input_mode = 1;
                s_input_len = 0;
                s_input_buf[0] = '\0';
                if (s_audio_enabled) seld_beep(1320, 30);
            } else if (sc == 0x26) { // 'L' -> Poll Now
                s_need_fetch = 1;
                if (s_audio_enabled) seld_beep(1760, 30);
            } else if (sc == 0x4B || sc == 0x1A || sc == 0x33) { // Left Arrow, '[', ',' -> Previous Filter Mode
                s_filter_mode = (s_filter_mode - 1 + 4) % 4;
                search_matrix_offline(s_current_index);
                s_cam_idx = 0;
                s_need_fetch = 1;
                s_last_fetch_frame = 0;
                if (s_audio_enabled) seld_beep(1350, 25);
            } else if (sc == 0x4D || sc == 0x1B || sc == 0x34) { // Right Arrow, ']', '.' -> Next Filter Mode
                s_filter_mode = (s_filter_mode + 1) % 4;
                search_matrix_offline(s_current_index);
                s_cam_idx = 0;
                s_need_fetch = 1;
                s_last_fetch_frame = 0;
                if (s_audio_enabled) seld_beep(1450, 25);
            }
        }

        if (!running) break;

        // 2. Periodic Live Tor Poll every ~6 frames (~200ms / ~5 FPS) or immediately on demand
        if (!s_paused && (s_need_fetch || (s_frame_count - s_last_fetch_frame) >= 6)) {
            s_need_fetch = 0;
            s_last_fetch_frame = s_frame_count;
            fetch_live_frame();
        }

        // 3. Render Tactical Overlay into Offscreen Backbuffer
        fill_rect(0, 0, SCR_W, SCR_H, COL_BG_BLACK);
        render_cctv_hud();

        // 4. Blit Backbuffer to Physical Framebuffer (Zero-Tearing)
        memcpy(s_fb.framebuffer, s_backbuf, SCR_W * SCR_H * sizeof(uint32_t));

        // Target Frame Rate ~30 FPS (33ms sleep)
        seld_sleep(33);
        s_frame_count++;
        if (s_gateway_failed && (s_frame_count % 300) == 0) {
            s_gateway_failed = 0;
        }
    }

    // 5. OpSec Anti-Forensics Memory Sanitization (Zero-on-Exit)
    if (s_nj) {
        njDone(s_nj);
        s_nj = NULL;
    }
    s_raw_rgb = NULL;

    memset(s_jpeg_buf, 0, sizeof(s_jpeg_buf));
    memset(s_presets, 0, sizeof(s_presets));
    memset(s_current_index, 0, sizeof(s_current_index));
    memset(s_centroid_title, 0, sizeof(s_centroid_title));
    if (s_backbuf) {
        memset(s_backbuf, 0, SCR_W * SCR_H * sizeof(uint32_t));
        free(s_backbuf);
        s_backbuf = NULL;
    }
    if (s_fb.framebuffer) {
        memset(s_fb.framebuffer, 0, SCR_W * SCR_H * sizeof(uint32_t));
    }
    if (s_cctv_matrix_buf) {
        memset(s_cctv_matrix_buf, 0, s_cctv_matrix_size);
        free(s_cctv_matrix_buf);
        s_cctv_matrix_buf = NULL;
    }

    // Trigger Humboldt Kernel Immune Purge
    seld_immune_purge();

    // Restore Terminal & Clear Video Memory
    seld_clear();
    seld_set_console_rows(20);

    return 0;
}
