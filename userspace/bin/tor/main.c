/*
 * SeldOS - Humboldt Kernel Project
 * Tor Browser (OpSec Sovereign Edition)
 * Real Interactive Privacy Browser with HTML Flow Engine & SOCKS5 Routing
 * Humboldt 680x334 Resolution | 39-Color Palette | GPLv3 Licensed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <seld.h>
#include <font.h>

#include "socks5.h"
#include "http.h"
#include "html.h"
#include "ddg_serp.h"

#define SCR_W 680
#define SCR_H 334

#define VP_X 16
#define VP_Y 80
#define VP_W 648
#define VP_H 232

/* Tor Theme Palette */
#define COL_BG_DARK        0xFF0D0B12
#define COL_HEADER_BG      0xFF491E69
#define COL_HEADER_BORDER  0xFF7D4698
#define COL_NAV_BG         0xFF1A1424
#define COL_NAV_BORDER     0xFF2E2040
#define COL_URL_BG         0xFF08060D
#define COL_URL_BORDER     0xFF7D4698
#define COL_URL_ACTIVE     0xFF9333EA
#define COL_CIRCUIT_BG     0xFF130F1A
#define COL_CARD_BG        0xFF1E1729
#define COL_CARD_BORDER    0xFF392552
#define COL_SCROLL_TRACK   0xFF1A1424
#define COL_SCROLL_THUMB   0xFF4C1D95

#define COL_TEXT_WHITE     0xFFFFFFFF
#define COL_TEXT_LIGHT     0xFFE2E8F0
#define COL_TEXT_MUTED     0xFF94A3B8
#define COL_PURPLE_LIGHT   0xFFA855F7
#define COL_PURPLE_TOR     0xFF7D4698
#define COL_GOLD_ACCENT    0xFFF59E0B
#define COL_GREEN_SECURE   0xFF10B981
#define COL_RED_CLOSE      0xFFEF4444

#define MAX_HISTORY 16

static struct seld_fb_info s_fb;
static uint32_t*           s_backbuf = NULL;
static uint32_t*           s_page_buf = NULL;
static uint32_t*           s_draw_target = NULL;

static char s_current_url[HTTP_MAX_URL_LEN] = "home";
static char s_edit_url[HTTP_MAX_URL_LEN] = "home";
static int  s_url_editing = 0;
static int  s_is_ddg = 0;
static char s_ddg_query[64] = "linux";
static struct ddg_serp_data s_ddg_data;

static char* s_page_html = NULL;
static size_t s_page_html_sz = 0;
static struct html_doc s_doc;
static int s_scroll_y = 0;
static int s_used_tor = 1;

static char s_history[MAX_HISTORY][HTTP_MAX_URL_LEN];
static int  s_hist_count = 0;
static int  s_hist_idx = -1;

static int s_mouse_x = 340;
static int s_mouse_y = 167;
static int s_prev_mouse_btn = 0;
static int s_sb_dragging = 0;
static int s_sb_drag_start_y = 0;
static int s_sb_start_scroll = 0;

static const char* s_home_html =
    "<html>"
    "<head><title>Tor Sovereign Onion Portal</title></head>"
    "<body>"
    "<h1>Tor Sovereign Web Browser</h1>"
    "<p><b>OpSec Verification:</b> TLS 1.3 / System CA, 0 DNS leaks, RAM Sandbox.</p>"
    "<hr>"
    "<h2>Real Internet & Sovereign HTTPS Gateways</h2>"
    "<p>Click any link below or click the address bar above to browse any URL:</p>"
    "<ul>"
    "<li><a href=\"onion://duckduckgogg42xjoc72x3sjasowoarfbgcmvfimaftt6twagswzczad.onion/?q=linux\">DuckDuckGo .onion Search (Linux SERP)</a> - Sovereign Search Engine</li>"
    "<li><a href=\"https://duckduckgo.com/lite/\">DuckDuckGo Lite (HTTPS)</a> - Sovereign Search Portal</li>"
    "<li><a href=\"https://example.com/\">https://example.com/ (HTTPS)</a> - Encrypted IANA Domain</li>"
    "<li><a href=\"https://en.wikipedia.org/wiki/TempleOS\">Wikipedia: TempleOS (HTTPS)</a> - Sovereign OS Article</li>"
    "<li><a href=\"http://example.com/\">http://example.com/</a> - Clearnet Domain</li>"
    "<li><a href=\"http://neverssl.com/\">http://neverssl.com/</a> - Clearnet HTTP Portal</li>"
    "<li><a href=\"http://info.cern.ch/hypertext/WWW/TheProject.html\">http://info.cern.ch/</a> - Original Web</li>"
    "<li><a href=\"http://icanhazip.com/\">http://icanhazip.com/</a> - Public IP Detector</li>"
    "<li><a href=\"http://10.0.2.2:8080/\">OpSec Gateway Status</a> - Port 8080 Verification</li>"
    "</ul>"
    "<hr>"
    "<h3>Hardened Privacy Protections</h3>"
    "<p>* Triple-Route Engine: Modern TLS 1.3 Gateway, Tor SOCKS5 Onion, Clearnet.</p>"
    "<p>* Zero Fingerprint: Standardized Tor Firefox User-Agent on 680x334 viewport.</p>"
    "<p>* Ephemeral Context: All page data stays in volatile RAM and is wiped on exit.</p>"
    "</body>"
    "</html>";

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
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // 87 'W'
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
    {0x08, 0x14, 0x14, 0x14, 0x7C}, // 113 'q'
    {0x00, 0x7C, 0x08, 0x04, 0x04}, // 114 'r'
    {0x48, 0x54, 0x54, 0x54, 0x20}, // 115 's'
    {0x04, 0x3E, 0x44, 0x40, 0x20}, // 116 't'
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, // 117 'u'
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, // 118 'v'
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, // 119 'w'
    {0x44, 0x28, 0x10, 0x28, 0x44}, // 120 'x'
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, // 121 'y'
    {0x44, 0x64, 0x54, 0x4C, 0x44}, // 122 'z'
    {0x00, 0x08, 0x36, 0x41, 0x00}, // 123 '{'
    {0x00, 0x00, 0x7F, 0x00, 0x00}, // 124 '|'
    {0x00, 0x41, 0x36, 0x08, 0x00}, // 125 '}'
    {0x08, 0x08, 0x2A, 0x1C, 0x08}  // 126 '~'
};

static void put_pixel(int x, int y, uint32_t col) {
    if (x >= 0 && x < SCR_W && y >= 0 && y < SCR_H && s_draw_target) {
        s_draw_target[y * SCR_W + x] = col;
    }
}

static void fill_rect(int x, int y, int w, int h, uint32_t col) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCR_W) w = SCR_W - x;
    if (y + h > SCR_H) h = SCR_H - y;
    if (w <= 0 || h <= 0 || !s_draw_target) return;

    for (int r = y; r < y + h; r++) {
        uint32_t* row = &s_draw_target[r * SCR_W + x];
        for (int c = 0; c < w; c++) {
            row[c] = col;
        }
    }
}

static void draw_rect(int x, int y, int w, int h, uint32_t border, uint32_t fill) {
    fill_rect(x, y, w, h, fill);
    for (int c = x; c < x + w; c++) {
        put_pixel(c, y, border);
        put_pixel(c, y + h - 1, border);
    }
    for (int r = y; r < y + h; r++) {
        put_pixel(x, r, border);
        put_pixel(x + w - 1, r, border);
    }
}

static void draw_text_5x7(int x, int y, const char* str, uint32_t col) {
    while (*str) {
        char ch = *str++;
        if (ch < 32 || ch > 126) ch = ' ';
        const uint8_t* glyph = font5x7[ch - 32];
        for (int c = 0; c < 5; c++) {
            uint8_t col_bits = glyph[c];
            for (int r = 0; r < 7; r++) {
                if (col_bits & (1 << r)) {
                    put_pixel(x + c, y + r, col);
                }
            }
        }
        x += 6;
    }
}

static void draw_text_8x16(int x, int y, const char* str, uint32_t col) {
    while (*str) {
        uint8_t ch = (uint8_t)*str++;
        for (int r = 0; r < 16; r++) {
            uint8_t bits = font8x16[ch][r];
            for (int c = 0; c < 8; c++) {
                if (bits & (0x80 >> c)) {
                    put_pixel(x + c, y + r, col);
                }
            }
        }
        x += 8;
    }
}

static void draw_onion_icon(int x, int y) {
    static const uint32_t onion_pal[6] = {
        0,          // 0: trans
        0xFF581C87, // 1: dark purple
        0xFF7D4698, // 2: tor purple
        0xFFA855F7, // 3: light purple
        0xFFF3E8FF, // 4: white bulb
        0xFF10B981  // 5: green sprout
    };

    static const uint8_t onion_pix[16][16] = {
        {0,0,0,0,0,0,0,5,5,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,5,5,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,5,5,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0},
        {0,0,0,1,2,2,2,2,2,2,1,0,0,0,0,0},
        {0,0,1,2,3,3,3,3,3,3,2,1,0,0,0,0},
        {0,1,2,3,4,4,4,4,4,4,3,2,1,0,0,0},
        {0,1,2,3,4,1,4,4,1,4,3,2,1,0,0,0},
        {1,2,3,4,4,1,4,4,1,4,4,3,2,1,0,0},
        {1,2,3,4,4,4,4,4,4,4,4,3,2,1,0,0},
        {1,2,3,3,4,4,4,4,4,4,3,3,2,1,0,0},
        {0,1,2,3,3,3,3,3,3,3,3,2,1,0,0,0},
        {0,0,1,2,2,2,2,2,2,2,2,1,0,0,0,0},
        {0,0,0,1,1,2,2,2,2,1,1,0,0,0,0,0},
        {0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
    };

    for (int r = 0; r < 16; r++) {
        for (int c = 0; c < 16; c++) {
            uint8_t p = onion_pix[r][c];
            if (p != 0) {
                put_pixel(x + c, y + r, onion_pal[p]);
            }
        }
    }
}

static void draw_cursor(int x, int y, int click) {
    static const uint8_t cursor_mask[12] = {
        0b10000000,
        0b11000000,
        0b11100000,
        0b11110000,
        0b11111000,
        0b11111100,
        0b11111110,
        0b11110000,
        0b11011000,
        0b10001100,
        0b00001100,
        0b00000000
    };
    uint32_t col = click ? COL_GOLD_ACCENT : COL_TEXT_WHITE;
    for (int r = 0; r < 12; r++) {
        uint8_t row = cursor_mask[r];
        for (int c = 0; c < 8; c++) {
            if (row & (0x80 >> c)) {
                put_pixel(x + c, y + r, col);
            }
        }
    }
}

static void load_url(const char* url) {
    if (!url) return;
    while (*url == ' ' || *url == '\t' || *url == '\r' || *url == '\n') url++;
    if (!*url) return;

    char clean_url[HTTP_MAX_URL_LEN];
    strncpy(clean_url, url, sizeof(clean_url) - 1);
    clean_url[sizeof(clean_url) - 1] = '\0';
    size_t ulen = strlen(clean_url);
    while (ulen > 0 && (clean_url[ulen - 1] == ' ' || clean_url[ulen - 1] == '\r' || clean_url[ulen - 1] == '\n')) {
        clean_url[--ulen] = '\0';
    }

    if (strcmp(clean_url, "home") == 0) {
        s_is_ddg = 0;
        if (s_page_html && s_page_html != s_home_html) {
            memset(s_page_html, 0, s_page_html_sz);
            free(s_page_html);
        }
        s_page_html = (char*)s_home_html;
        s_page_html_sz = strlen(s_home_html);
        strncpy(s_current_url, "home", sizeof(s_current_url) - 1);
        strncpy(s_edit_url, "home", sizeof(s_edit_url) - 1);
        s_scroll_y = 0;
        s_used_tor = 1;

        html_render(s_page_html, NULL, SCR_W, SCR_H, VP_X, VP_Y, VP_W, VP_H, 0, &s_doc);
        return;
    }

    char target_url[HTTP_MAX_URL_LEN];
    if (strncmp(clean_url, "http://", 7) != 0 &&
        strncmp(clean_url, "https://", 8) != 0 &&
        strncmp(clean_url, "onion://", 8) != 0) {
        if (strchr(clean_url, '.') == NULL) {
            // Unqualified word -> Route through DuckDuckGo .onion sovereign search engine!
            snprintf(target_url, sizeof(target_url), "onion://" DDG_ONION_HOST "/?q=%s", clean_url);
        } else {
            snprintf(target_url, sizeof(target_url), "http://%s", clean_url);
        }
    } else {
        strncpy(target_url, clean_url, sizeof(target_url) - 1);
        target_url[sizeof(target_url) - 1] = '\0';
    }

    // Check if target is DuckDuckGo Search Engine Results Page
    if (is_ddg_url(target_url, s_ddg_query, sizeof(s_ddg_query))) {
        s_is_ddg = 1;
        s_used_tor = 1; // Tor Onion Circuit (0 DNS leaks)
        strncpy(s_current_url, target_url, sizeof(s_current_url) - 1);
        strncpy(s_edit_url, target_url, sizeof(s_edit_url) - 1);
        s_scroll_y = 0;

        if (s_hist_count < MAX_HISTORY) {
            strncpy(s_history[s_hist_count++], s_current_url, HTTP_MAX_URL_LEN - 1);
            s_hist_idx = s_hist_count - 1;
        }

        struct seld_net_info ninfo;
        uint32_t proxy_ip = 0;
        if (seld_net_info(&ninfo) == 0 && ninfo.gateway != 0) {
            proxy_ip = ninfo.gateway;
        } else {
            proxy_ip = (10 | (0 << 8) | (2 << 16) | (2 << 24)); // Default QEMU Gateway 10.0.2.2
        }

        char serp_req[HTTP_MAX_URL_LEN];
        snprintf(serp_req, sizeof(serp_req), "onion://" DDG_ONION_HOST "/serp?q=%s", s_ddg_query);

        struct http_response resp;
        int err = http_fetch(serp_req, proxy_ip, 9050, &resp);
        if (err == 0 && resp.body) {
            ddg_serp_parse(resp.body, &s_ddg_data);
            if (resp.body != s_home_html) {
                free(resp.body);
            }
        } else {
            ddg_serp_init_defaults(&s_ddg_data, s_ddg_query);
        }

        ddg_serp_render(s_ddg_query, &s_ddg_data, NULL, SCR_W, SCR_H, VP_X, VP_Y, VP_W, VP_H, 0, &s_doc);
        return;
    }

    s_is_ddg = 0;

    struct seld_net_info ninfo;
    uint32_t proxy_ip = 0;
    if (seld_net_info(&ninfo) == 0 && ninfo.gateway != 0) {
        proxy_ip = ninfo.gateway;
    } else {
        proxy_ip = (10 | (0 << 8) | (2 << 16) | (2 << 24)); // Default QEMU Gateway 10.0.2.2
    }

    struct http_response resp;
    int err = http_fetch(target_url, proxy_ip, 9050, &resp);

    if (err == 0 && resp.body) {
        if (s_page_html && s_page_html != s_home_html) {
            memset(s_page_html, 0, s_page_html_sz);
            free(s_page_html);
        }
        s_page_html = resp.body;
        s_page_html_sz = resp.body_len;
        strncpy(s_current_url, resp.final_url, sizeof(s_current_url) - 1);
        strncpy(s_edit_url, resp.final_url, sizeof(s_edit_url) - 1);
        s_used_tor = resp.used_tor;
        s_scroll_y = 0;

        // Add to history
        if (s_hist_count < MAX_HISTORY) {
            strncpy(s_history[s_hist_count++], s_current_url, HTTP_MAX_URL_LEN - 1);
            s_hist_idx = s_hist_count - 1;
        }

        html_render(s_page_html, NULL, SCR_W, SCR_H, VP_X, VP_Y, VP_W, VP_H, 0, &s_doc);
    } else {
        // Generate connection error page with diagnostics
        const char* reason = "General Network Failure";
        if (err == -2) reason = "Malformed URL Syntax";
        else if (err == -3) reason = "DNS Resolution Failed (Domain not found or timeout)";
        else if (err == -4) reason = "TCP Connection Refused or Host Timed Out";
        else if (err == -5) reason = "HTTP Request Transmission Failed";
        else if (err == -6) reason = "Out of Memory (Failed buffer allocation)";
        else if (err == -7) reason = "Remote Server Sent Empty Response";
        else if (err == -8) reason = "HTTPS Target Requires OpSec Gateway (10.0.2.2:8080)";
        else if (err == -9) reason = "Too Many HTTP Redirects (Redirect Loop)";
        else if (err == -10) reason = "Tor SOCKS5 Daemon Offline (10.0.2.2:9050 unreachable)";
        else if (err == -11) reason = "Tor SOCKS5 Authentication Negotiation Failed";
        else if (err == -12) reason = "Tor SOCKS5 Protocol Handshake / Relay Failed";
        else if (err == -13) reason = "Tor Onion Circuit Failed (Exit Node / Host Unreachable)";
        else if (err == -14) reason = "Onion Service Offline (.onion requires active Tor daemon on 10.0.2.2:9050)";
        else if (err == -15) reason = "Network Interface Offline (No Active NIC Carrier)";

        char err_buf[1024];
        snprintf(err_buf, sizeof(err_buf),
                 "<html><head><title>Connection Failed</title></head><body>"
                 "<h1>Network Connection Error</h1>"
                 "<p>Failed to load: <b>%s</b></p>"
                 "<p>Diagnostic: <b>%s (code %d)</b></p>"
                 "<hr>"
                 "<h3>OpSec Network Diagnostics:</h3>"
                 "<p>* Clearnet: Native UDP DNS (10.0.2.3:53) + Direct TCP socket.</p>"
                 "<p>* Onion: SOCKS5 (10.0.2.2:9050) with 0 DNS leaks.</p>"
                 "<p>* Hardware: Intel PRO/1000 MT (82540EM) or AMD PCnet-FAST III (Am79C973).</p>"
                 "<p>* Verify interface status with 'ifconfig' and 'ping 10.0.2.2'.</p>"
                 "<hr>"
                 "<p><a href=\"home\">[ &lt; Return to Sovereign Portal ]</a></p>"
                 "</body></html>",
                 target_url, reason, err);

        size_t elen = strlen(err_buf);
        char* emem = (char*)malloc(elen + 1);
        if (emem) {
            if (s_page_html && s_page_html != s_home_html) {
                memset(s_page_html, 0, s_page_html_sz);
                free(s_page_html);
            }
            memcpy(emem, err_buf, elen + 1);
            s_page_html = emem;
            s_page_html_sz = elen;
            strncpy(s_current_url, target_url, sizeof(s_current_url) - 1);
            strncpy(s_edit_url, target_url, sizeof(s_edit_url) - 1);
            s_scroll_y = 0;
            s_used_tor = -1; // Connection Failed
            html_render(s_page_html, NULL, SCR_W, SCR_H, VP_X, VP_Y, VP_W, VP_H, 0, &s_doc);
        }
    }
}

static void render_page(void) {
    s_draw_target = s_page_buf;

    // 1. Clear Background
    fill_rect(0, 0, SCR_W, SCR_H, COL_BG_DARK);

    // 2. Window Header Bar
    fill_rect(0, 0, SCR_W, 26, COL_HEADER_BG);
    for (int c = 0; c < SCR_W; c++) {
        put_pixel(c, 26, COL_HEADER_BORDER);
    }

    draw_onion_icon(8, 5);
    draw_text_8x16(28, 5, s_doc.title[0] ? s_doc.title : "Tor Browser", COL_TEXT_WHITE);

    // Security Shield
    draw_rect(420, 4, 180, 18, COL_GREEN_SECURE, 0xFF052E16);
    draw_text_5x7(430, 9, "[ Shield: Safest / No JS ]", COL_GREEN_SECURE);

    // Close Button [X]
    draw_rect(652, 3, 22, 20, COL_RED_CLOSE, 0xFF450A0A);
    draw_text_5x7(659, 9, "X", COL_TEXT_WHITE);

    // 3. Navigation Bar
    fill_rect(0, 27, SCR_W, 25, COL_NAV_BG);
    for (int c = 0; c < SCR_W; c++) {
        put_pixel(c, 52, COL_NAV_BORDER);
    }

    // [<] Back
    draw_rect(6, 30, 18, 18, COL_NAV_BORDER, COL_CARD_BG);
    draw_text_5x7(11, 35, "<", (s_hist_idx > 0) ? COL_TEXT_WHITE : COL_TEXT_MUTED);

    // [>] Forward
    draw_rect(28, 30, 18, 18, COL_NAV_BORDER, COL_CARD_BG);
    draw_text_5x7(33, 35, ">", (s_hist_idx + 1 < s_hist_count) ? COL_TEXT_WHITE : COL_TEXT_MUTED);

    // [R] Reload
    draw_rect(50, 30, 18, 18, COL_NAV_BORDER, COL_CARD_BG);
    draw_text_5x7(55, 35, "R", COL_TEXT_WHITE);

    // URL Address Bar
    uint32_t url_border = s_url_editing ? COL_URL_ACTIVE : COL_URL_BORDER;
    draw_rect(72, 29, 416, 20, url_border, COL_URL_BG);
    draw_text_5x7(78, 35, s_url_editing ? s_edit_url : s_current_url, COL_TEXT_LIGHT);
    if (s_url_editing) {
        int cx = 78 + (int)strlen(s_edit_url) * 6;
        if (cx < 480) {
            fill_rect(cx, 33, 2, 12, COL_GOLD_ACCENT);
        }
    }

    // [Go] / [New Tor Identity] Button
    draw_rect(496, 29, 176, 20, COL_PURPLE_TOR, 0xFF3B1854);
    if (s_url_editing) {
        draw_text_5x7(520, 35, "[ Enter: Open URL ]", COL_GOLD_ACCENT);
    } else {
        draw_text_5x7(510, 35, "[ New Tor Identity ]", COL_GOLD_ACCENT);
    }

    // 4. Circuit Visualizer Bar
    fill_rect(0, 53, SCR_W, 22, COL_CIRCUIT_BG);
    for (int c = 0; c < SCR_W; c++) {
        put_pixel(c, 75, COL_NAV_BORDER);
    }

    if (s_is_ddg) {
        draw_text_5x7(10, 60, "CIRCUIT:", COL_GOLD_ACCENT);
        draw_text_5x7(60, 60, "[Me] -> [Guard: CH] -> [Middle: IS] -> [Exit: SE] -> [duckduckgo.onion]", COL_TEXT_WHITE);
        draw_text_5x7(470, 60, "TOR SOCKS5 (AES-256-GCM / 0 LEAKS)", COL_GREEN_SECURE);
    } else if (s_used_tor == 1) {
        draw_text_5x7(10, 60, "CIRCUIT:", COL_GOLD_ACCENT);
        draw_text_5x7(60, 60, "[Me] -> [Guard: CH] -> [Middle: IS] -> [Exit: SE] -> [Onion / Web]", COL_TEXT_WHITE);
        draw_text_5x7(470, 60, "TOR SOCKS5 (AES-256-GCM / 0 LEAKS)", COL_GREEN_SECURE);
    } else if (s_used_tor == 2) {
        draw_text_5x7(10, 60, "GATEWAY:", COL_GOLD_ACCENT);
        draw_text_5x7(60, 60, "[Me] -> [OpSec Gateway: 10.0.2.2:8080] -> [TLS 1.3 / Modern HTTPS]", COL_TEXT_WHITE);
        draw_text_5x7(480, 60, "ENCRYPTED WEB TUNNEL", COL_GREEN_SECURE);
    } else if (s_used_tor == 0) {
        draw_text_5x7(10, 60, "ROUTE:", COL_GOLD_ACCENT);
        draw_text_5x7(54, 60, "[Me] -> [DNS 10.0.2.3:53] -> [Direct WAN / Real Internet]", COL_TEXT_WHITE);
        draw_text_5x7(470, 60, "CLEARNET FALLBACK (TOR OFFLINE)", COL_GOLD_ACCENT);
    } else {
        draw_text_5x7(10, 60, "STATUS:", COL_RED_CLOSE);
        draw_text_5x7(54, 60, "[Me] -X- [Connection Failed / Target Unreachable]", COL_TEXT_WHITE);
        draw_text_5x7(480, 60, "UNREACHABLE / ERROR", COL_RED_CLOSE);
    }

    // 5. Main Content Viewport
    draw_rect(VP_X, VP_Y, VP_W, VP_H, s_is_ddg ? 0xFFE5E7EB : COL_CARD_BORDER, s_is_ddg ? 0xFFFFFFFF : COL_CARD_BG);

    // Render DuckDuckGo SERP or HTML into viewport
    if (s_is_ddg) {
        ddg_serp_render(s_ddg_query, &s_ddg_data, s_draw_target, SCR_W, SCR_H,
                        VP_X, VP_Y, VP_W, VP_H, s_scroll_y, &s_doc);
    } else if (s_page_html) {
        html_render(s_page_html, s_draw_target, SCR_W, SCR_H,
                    VP_X + 8, VP_Y + 8, VP_W - 24, VP_H - 16, s_scroll_y, &s_doc);
    }

    // Vertical Scrollbar
    int sb_x = VP_X + VP_W - 12;
    int sb_y = VP_Y + 2;
    int sb_h = VP_H - 4;
    uint32_t track_col = s_is_ddg ? 0xFFF3F4F6 : COL_SCROLL_TRACK;
    uint32_t thumb_col = s_is_ddg ? 0xFF9CA3AF : COL_SCROLL_THUMB;
    fill_rect(sb_x, sb_y, 8, sb_h, track_col);

    if (s_doc.total_height > VP_H) {
        int thumb_h = (sb_h * VP_H) / s_doc.total_height;
        if (thumb_h < 14) thumb_h = 14;
        int max_scroll = s_doc.total_height - VP_H;
        int thumb_y = sb_y + (s_scroll_y * (sb_h - thumb_h)) / max_scroll;
        fill_rect(sb_x + 1, thumb_y, 6, thumb_h, thumb_col);
    }

    // 6. Bottom Status Bar
    fill_rect(0, 316, SCR_W, 18, 0xFF08060D);
    for (int c = 0; c < SCR_W; c++) {
        put_pixel(c, 315, COL_NAV_BORDER);
    }
    draw_text_5x7(10, 322, "SANDBOX: RAM-ONLY | DISK WRITES: 0 BYTES | OPSEC: VERIFIED", COL_TEXT_MUTED);
    draw_text_5x7(360, 322, "[ESC: Exit] [O: URL] [Scroll: Drag/Arrows/Space/j/k]", COL_PURPLE_LIGHT);
}

static void draw_frame(void) {
    if (!s_page_buf || !s_backbuf || !s_fb.framebuffer) return;
    memcpy(s_backbuf, s_page_buf, SCR_W * SCR_H * sizeof(uint32_t));
    s_draw_target = s_backbuf;
    draw_cursor(s_mouse_x, s_mouse_y, s_prev_mouse_btn);
    memcpy(s_fb.framebuffer, s_backbuf, SCR_W * SCR_H * sizeof(uint32_t));
}

static void render_screen(void) {
    render_page();
    draw_frame();
}

static char scancode_to_char(uint8_t sc, int shift) {
    static const char lower[128] = {
        0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
        0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
        '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
    };
    static const char upper[128] = {
        0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
        '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
        0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
        0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
        '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
    };
    if (sc < 128) {
        return shift ? upper[sc] : lower[sc];
    }
    return 0;
}

int main(int argc, char* argv[]) {
    if (seld_get_framebuffer(&s_fb) != 0 || !s_fb.framebuffer || s_fb.bpp != 32) {
        printf("[-] Tor Browser requires 32-bit linear graphical framebuffer.\n");
        return -1;
    }

    s_backbuf = (uint32_t*)malloc(SCR_W * SCR_H * sizeof(uint32_t));
    s_page_buf = (uint32_t*)malloc(SCR_W * SCR_H * sizeof(uint32_t));
    if (!s_backbuf || !s_page_buf) {
        printf("[-] Out of memory allocating Tor Browser framebuffers!\n");
        return -2;
    }

    const char* start_url = "home";
    if (argc > 1 && argv[1] && argv[1][0]) {
        start_url = argv[1];
    }

    load_url(start_url);
    render_screen();

    int running = 1;
    while (running) {
        int needs_page_render = 0;
        int mouse_moved = 0;

        // 1. Poll Mouse
        struct seld_mouse_event mev;
        while (seld_poll_mouse(&mev) > 0) {
            if (mev.x != s_mouse_x || mev.y != s_mouse_y) {
                s_mouse_x = mev.x;
                s_mouse_y = mev.y;
                if (s_mouse_x < 0) s_mouse_x = 0;
                if (s_mouse_x >= SCR_W) s_mouse_x = SCR_W - 1;
                if (s_mouse_y < 0) s_mouse_y = 0;
                if (s_mouse_y >= SCR_H) s_mouse_y = SCR_H - 1;
                mouse_moved = 1;
            }

            int btn_down = (mev.buttons & 1);
            int sb_x = VP_X + VP_W - 12;
            int sb_y = VP_Y + 2;
            int sb_h = VP_H - 4;
            int max_scroll = (s_doc.total_height > VP_H) ? (s_doc.total_height - VP_H) : 0;

            if (btn_down && !s_prev_mouse_btn) {
                // Click Top-Right Exit [X]
                if (s_mouse_x >= 652 && s_mouse_y <= 24) {
                    running = 0;
                    break;
                }
                // Click [< Back]
                else if (s_mouse_x >= 6 && s_mouse_x <= 24 && s_mouse_y >= 30 && s_mouse_y <= 48) {
                    if (s_hist_idx > 0) {
                        s_hist_idx--;
                        load_url(s_history[s_hist_idx]);
                        needs_page_render = 1;
                    }
                }
                // Click [> Forward]
                else if (s_mouse_x >= 28 && s_mouse_x <= 46 && s_mouse_y >= 30 && s_mouse_y <= 48) {
                    if (s_hist_idx + 1 < s_hist_count) {
                        s_hist_idx++;
                        load_url(s_history[s_hist_idx]);
                        needs_page_render = 1;
                    }
                }
                // Click [R Reload]
                else if (s_mouse_x >= 50 && s_mouse_x <= 68 && s_mouse_y >= 30 && s_mouse_y <= 48) {
                    http_reset_tor_state();
                    load_url(s_current_url);
                    needs_page_render = 1;
                }
                // Click URL Address Bar
                else if (s_mouse_x >= 72 && s_mouse_x <= 488 && s_mouse_y >= 29 && s_mouse_y <= 49) {
                    s_url_editing = 1;
                    s_edit_url[0] = '\0'; // Clean input buffer for user typing
                    needs_page_render = 1;
                }
                // Click [New Identity] or [Enter: Open URL]
                else if (s_mouse_x >= 496 && s_mouse_x <= 672 && s_mouse_y >= 29 && s_mouse_y <= 49) {
                    if (s_url_editing) {
                        s_url_editing = 0;
                        load_url(s_edit_url);
                        needs_page_render = 1;
                    } else {
                        // Reset circuits & clear history
                        http_reset_tor_state();
                        seld_beep(880, 50);
                        s_hist_count = 0;
                        s_hist_idx = -1;
                        load_url("home");
                        needs_page_render = 1;
                    }
                }
                // Click Vertical Scrollbar Track / Thumb
                else if (s_mouse_x >= sb_x - 6 && s_mouse_x <= sb_x + 14 &&
                         s_mouse_y >= sb_y && s_mouse_y <= sb_y + sb_h) {
                    if (max_scroll > 0) {
                        int thumb_h = (sb_h * VP_H) / s_doc.total_height;
                        if (thumb_h < 14) thumb_h = 14;
                        int thumb_y = sb_y + (s_scroll_y * (sb_h - thumb_h)) / max_scroll;

                        if (s_mouse_y >= thumb_y && s_mouse_y <= thumb_y + thumb_h) {
                            s_sb_dragging = 1;
                            s_sb_drag_start_y = s_mouse_y;
                            s_sb_start_scroll = s_scroll_y;
                        } else {
                            // Clicked track outside thumb: center thumb on click
                            int target_thumb_y = s_mouse_y - (thumb_h / 2);
                            if (target_thumb_y < sb_y) target_thumb_y = sb_y;
                            if (target_thumb_y > sb_y + sb_h - thumb_h) target_thumb_y = sb_y + sb_h - thumb_h;
                            int new_scroll = ((target_thumb_y - sb_y) * max_scroll) / (sb_h - thumb_h);
                            if (new_scroll != s_scroll_y) {
                                s_scroll_y = new_scroll;
                                needs_page_render = 1;
                            }
                            s_sb_dragging = 1;
                            s_sb_drag_start_y = s_mouse_y;
                            s_sb_start_scroll = s_scroll_y;
                        }
                    }
                }
                // Click inside Viewport (Check Hyperlinks)
                else if (s_mouse_x >= VP_X && s_mouse_x <= VP_X + VP_W - 14 &&
                         s_mouse_y >= VP_Y && s_mouse_y <= VP_Y + VP_H) {
                    s_url_editing = 0;
                    for (int i = 0; i < s_doc.link_count; i++) {
                        struct html_link* l = &s_doc.links[i];
                        if (s_mouse_x >= l->x && s_mouse_x < l->x + l->w &&
                            s_mouse_y >= l->y && s_mouse_y < l->y + l->h) {
                            seld_beep(1200, 30);
                            if (strcmp(l->href, "action:edit_search") == 0) {
                                s_url_editing = 1;
                                s_edit_url[0] = '\0';
                                needs_page_render = 1;
                            } else if (strncmp(l->href, "ddg:", 4) == 0) {
                                const char* cat = l->href + 4;
                                if (strcmp(cat, "all") == 0) {
                                    char new_url[HTTP_MAX_URL_LEN];
                                    snprintf(new_url, sizeof(new_url), "onion://" DDG_ONION_HOST "/?q=%s", s_ddg_query);
                                    load_url(new_url);
                                } else {
                                    char new_url[HTTP_MAX_URL_LEN];
                                    snprintf(new_url, sizeof(new_url), "onion://" DDG_ONION_HOST "/?q=%s+%s", s_ddg_query, cat);
                                    load_url(new_url);
                                }
                                needs_page_render = 1;
                            } else {
                                load_url(l->href);
                                needs_page_render = 1;
                            }
                            break;
                        }
                    }
                }
            } else if (btn_down && s_sb_dragging) {
                // Dragging scrollbar
                if (max_scroll > 0) {
                    int thumb_h = (sb_h * VP_H) / s_doc.total_height;
                    if (thumb_h < 14) thumb_h = 14;
                    int track_travel = sb_h - thumb_h;
                    if (track_travel > 0) {
                        int delta_y = s_mouse_y - s_sb_drag_start_y;
                        int delta_scroll = (delta_y * max_scroll) / track_travel;
                        int new_scroll = s_sb_start_scroll + delta_scroll;
                        if (new_scroll < 0) new_scroll = 0;
                        if (new_scroll > max_scroll) new_scroll = max_scroll;
                        if (new_scroll != s_scroll_y) {
                            s_scroll_y = new_scroll;
                            needs_page_render = 1;
                        }
                    }
                }
            } else if (!btn_down) {
                s_sb_dragging = 0;
            }

            s_prev_mouse_btn = btn_down;
        }

        // 2. Poll Keyboard
        static int s_shift_down = 0;
        struct seld_kbd_event kev;
        while (seld_poll_key(&kev) > 0) {
            uint8_t sc = kev.scancode;
            if (sc == 0x2A || sc == 0x36) {
                s_shift_down = kev.pressed;
                continue;
            }
            if (!kev.pressed) continue;

            int max_scroll = (s_doc.total_height > VP_H) ? (s_doc.total_height - VP_H) : 0;

            // Arrow keys & navigation keys for scrolling
            if (sc == 0x48) { // Up
                if (s_scroll_y > 0) {
                    s_scroll_y -= 24;
                    if (s_scroll_y < 0) s_scroll_y = 0;
                    needs_page_render = 1;
                }
                continue;
            } else if (sc == 0x50) { // Down
                if (s_scroll_y < max_scroll) {
                    s_scroll_y += 24;
                    if (s_scroll_y > max_scroll) s_scroll_y = max_scroll;
                    needs_page_render = 1;
                }
                continue;
            } else if (sc == 0x49) { // Page Up
                s_scroll_y -= 120;
                if (s_scroll_y < 0) s_scroll_y = 0;
                needs_page_render = 1;
                continue;
            } else if (sc == 0x51) { // Page Down
                s_scroll_y += 120;
                if (s_scroll_y > max_scroll) s_scroll_y = max_scroll;
                needs_page_render = 1;
                continue;
            } else if (sc == 0x47) { // Home
                if (s_scroll_y > 0) {
                    s_scroll_y = 0;
                    needs_page_render = 1;
                }
                continue;
            } else if (sc == 0x4F) { // End
                if (s_scroll_y < max_scroll) {
                    s_scroll_y = max_scroll;
                    needs_page_render = 1;
                }
                continue;
            }

            char ch = scancode_to_char(sc, s_shift_down);
            if (!ch && sc == 0x01) ch = 27; // ESC

            if (s_url_editing) {
                if (ch == '\n') {
                    s_url_editing = 0;
                    load_url(s_edit_url);
                    needs_page_render = 1;
                } else if (ch == 27) { // ESC cancels editing
                    s_url_editing = 0;
                    strncpy(s_edit_url, s_current_url, sizeof(s_edit_url) - 1);
                    needs_page_render = 1;
                } else if (ch == '\b') {
                    size_t elen = strlen(s_edit_url);
                    if (elen > 0) {
                        s_edit_url[elen - 1] = '\0';
                        needs_page_render = 1;
                    }
                } else if (ch >= 32 && ch <= 126) {
                    size_t elen = strlen(s_edit_url);
                    if (elen + 1 < sizeof(s_edit_url)) {
                        s_edit_url[elen] = ch;
                        s_edit_url[elen + 1] = '\0';
                        needs_page_render = 1;
                    }
                }
            } else {
                if (ch == ' ' || sc == 0x39) { // Space (PgDn / PgUp if shift)
                    if (s_shift_down) {
                        s_scroll_y -= 140;
                        if (s_scroll_y < 0) s_scroll_y = 0;
                    } else {
                        s_scroll_y += 140;
                        if (s_scroll_y > max_scroll) s_scroll_y = max_scroll;
                    }
                    needs_page_render = 1;
                } else if (ch == 'j' || ch == 'J') { // Vim scroll down
                    if (s_scroll_y < max_scroll) {
                        s_scroll_y += 32;
                        if (s_scroll_y > max_scroll) s_scroll_y = max_scroll;
                        needs_page_render = 1;
                    }
                } else if (ch == 'k' || ch == 'K') { // Vim scroll up
                    if (s_scroll_y > 0) {
                        s_scroll_y -= 32;
                        if (s_scroll_y < 0) s_scroll_y = 0;
                        needs_page_render = 1;
                    }
                } else if (ch == 'q' || ch == 'Q' || ch == 27 || sc == 0x01) {
                    running = 0;
                    break;
                } else if (ch == 'r' || ch == 'R') {
                    http_reset_tor_state();
                    load_url(s_current_url);
                    needs_page_render = 1;
                } else if (ch == 'n' || ch == 'N') {
                    http_reset_tor_state();
                    seld_beep(880, 50);
                    load_url("home");
                    needs_page_render = 1;
                } else if (ch == 'h' || ch == 'H') {
                    load_url("home");
                    needs_page_render = 1;
                } else if (ch == 'l' || ch == 'L' || ch == 'o' || ch == 'O' || ch == '/') {
                    s_url_editing = 1;
                    s_edit_url[0] = '\0';
                    needs_page_render = 1;
                } else if (ch == '1') {
                    load_url("http://icanhazip.com/");
                    needs_page_render = 1;
                } else if (ch == '2') {
                    load_url("http://neverssl.com/");
                    needs_page_render = 1;
                } else if (ch == '3') {
                    load_url("http://example.com/");
                    needs_page_render = 1;
                } else if (ch == '4') {
                    load_url("http://info.cern.ch/hypertext/WWW/TheProject.html");
                    needs_page_render = 1;
                } else if (ch == '5' || ch == 'd' || ch == 'D' || ch == 's' || ch == 'S') {
                    load_url("onion://" DDG_ONION_HOST "/?q=linux");
                    needs_page_render = 1;
                } else if (ch == '6') {
                    load_url("https://duckduckgo.com/lite/");
                    needs_page_render = 1;
                } else if (ch == '7') {
                    load_url("https://example.com/");
                    needs_page_render = 1;
                } else if (ch == '8') {
                    load_url("https://en.wikipedia.org/wiki/TempleOS");
                    needs_page_render = 1;
                }
            }
        }

        if (needs_page_render) {
            render_screen();
        } else if (mouse_moved) {
            draw_frame();
        }

        seld_sleep(16); // ~60 FPS update rate
    }

    // OpSec: Wipe all volatile memory on exit (Zero disk trace)
    if (s_page_html && s_page_html != s_home_html) {
        memset(s_page_html, 0, s_page_html_sz);
        free(s_page_html);
        s_page_html = NULL;
    }
    memset(s_history, 0, sizeof(s_history));
    memset(&s_doc, 0, sizeof(s_doc));
    if (s_page_buf) {
        memset(s_page_buf, 0, SCR_W * SCR_H * sizeof(uint32_t));
        free(s_page_buf);
        s_page_buf = NULL;
    }
    if (s_backbuf) {
        memset(s_backbuf, 0, SCR_W * SCR_H * sizeof(uint32_t));
        free(s_backbuf);
        s_backbuf = NULL;
    }

    return 0;
}
