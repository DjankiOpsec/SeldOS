/*
 * SeldOS - Humboldt Kernel Project
 * Tor Browser (OpSec Sovereign Edition)
 * DuckDuckGo .onion Sovereign Search Engine & Rich SERP Layout Engine
 * GPLv3 Licensed.
 */

#include "ddg_serp.h"
#include "ddg_assets.h"
#include "font_cyrillic.h"
#include <font.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define COL_BG_WHITE       0xFFFFFFFF
#define COL_TEXT_MAIN      0xFF111827
#define COL_TEXT_MUTED     0xFF5F6368
#define COL_TEXT_SNIPPET   0xFF4D5156
#define COL_LINK_BLUE      0xFF1A0DAB
#define COL_ACTIVE_BLUE    0xFF2563EB
#define COL_BORDER_GRAY    0xFFDFE1E5
#define COL_CARD_BORDER    0xFFE5E7EB
#define COL_BADGE_BG_GREEN 0xFFDCFCE7
#define COL_BADGE_TXT_GREEN 0xFF16A34A

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

static void put_pixel_clip(uint32_t* buf, int sw, int sh, int x, int y, uint32_t col,
                           int cx1, int cy1, int cx2, int cy2) {
    if (x < cx1 || x >= cx2 || y < cy1 || y >= cy2) return;
    if (x < 0 || x >= sw || y < 0 || y >= sh || !buf) return;
    buf[y * sw + x] = col;
}

static void fill_rect_clip(uint32_t* buf, int sw, int sh, int x, int y, int w, int h, uint32_t col,
                           int cx1, int cy1, int cx2, int cy2) {
    for (int r = 0; r < h; r++) {
        for (int c = 0; c < w; c++) {
            put_pixel_clip(buf, sw, sh, x + c, y + r, col, cx1, cy1, cx2, cy2);
        }
    }
}

static void draw_rect_clip(uint32_t* buf, int sw, int sh, int x, int y, int w, int h,
                           uint32_t border, uint32_t fill, int cx1, int cy1, int cx2, int cy2) {
    fill_rect_clip(buf, sw, sh, x, y, w, h, fill, cx1, cy1, cx2, cy2);
    for (int c = 0; c < w; c++) {
        put_pixel_clip(buf, sw, sh, x + c, y, border, cx1, cy1, cx2, cy2);
        put_pixel_clip(buf, sw, sh, x + c, y + h - 1, border, cx1, cy1, cx2, cy2);
    }
    for (int r = 0; r < h; r++) {
        put_pixel_clip(buf, sw, sh, x, y + r, border, cx1, cy1, cx2, cy2);
        put_pixel_clip(buf, sw, sh, x + w - 1, y + r, border, cx1, cy1, cx2, cy2);
    }
}

static void draw_bitmap_clip(uint32_t* buf, int sw, int sh, int x, int y, int w, int h,
                             const uint32_t* pixels, int cx1, int cy1, int cx2, int cy2) {
    if (!pixels) return;
    for (int r = 0; r < h; r++) {
        for (int c = 0; c < w; c++) {
            uint32_t p = pixels[r * w + c];
            uint8_t a = (uint8_t)(p >> 24);
            if (a > 32) {
                put_pixel_clip(buf, sw, sh, x + c, y + r, p | 0xFF000000, cx1, cy1, cx2, cy2);
            }
        }
    }
}

static void draw_char_5x7_clip(uint32_t* buf, int sw, int sh, int x, int y, char ch, uint32_t col,
                               int cx1, int cy1, int cx2, int cy2) {
    if (ch < 32 || ch > 126) ch = ' ';
    const uint8_t* glyph = font5x7[ch - 32];
    for (int c = 0; c < 5; c++) {
        uint8_t col_bits = glyph[c];
        for (int r = 0; r < 7; r++) {
            if (col_bits & (1 << r)) {
                put_pixel_clip(buf, sw, sh, x + c, y + r, col, cx1, cy1, cx2, cy2);
            }
        }
    }
}

static void draw_char_8x16_clip(uint32_t* buf, int sw, int sh, int x, int y, char ch, uint32_t col,
                                int cx1, int cy1, int cx2, int cy2) {
    uint8_t u = (uint8_t)ch;
    for (int r = 0; r < 16; r++) {
        uint8_t bits = font8x16[u][r];
        for (int c = 0; c < 8; c++) {
            if (bits & (0x80 >> c)) {
                put_pixel_clip(buf, sw, sh, x + c, y + r, col, cx1, cy1, cx2, cy2);
            }
        }
    }
}

static int draw_utf8_char_clip(uint32_t* buf, int sw, int sh, int x, int y, uint32_t cp, uint32_t col,
                               int cx1, int cy1, int cx2, int cy2) {
    if (cp >= 32 && cp <= 126) {
        draw_char_5x7_clip(buf, sw, sh, x, y, (char)cp, col, cx1, cy1, cx2, cy2);
        return 6;
    } else if (cp >= 0x0410 && cp <= 0x042F) { // Russian Capital
        int idx = cp - 0x0410;
        const uint8_t* glyph = font_cyr_upper[idx];
        for (int c = 0; c < 5; c++) {
            uint8_t bits = glyph[c];
            for (int r = 0; r < 7; r++) {
                if (bits & (1 << r)) put_pixel_clip(buf, sw, sh, x + c, y + r, col, cx1, cy1, cx2, cy2);
            }
        }
        return 6;
    } else if (cp >= 0x0430 && cp <= 0x044F) { // Russian Lowercase
        int idx = cp - 0x0430;
        const uint8_t* glyph = font_cyr_lower[idx];
        for (int c = 0; c < 5; c++) {
            uint8_t bits = glyph[c];
            for (int r = 0; r < 7; r++) {
                if (bits & (1 << r)) put_pixel_clip(buf, sw, sh, x + c, y + r, col, cx1, cy1, cx2, cy2);
            }
        }
        return 6;
    } else if (cp == 0x0401) { // Ё
        draw_utf8_char_clip(buf, sw, sh, x, y, 0x0415, col, cx1, cy1, cx2, cy2);
        put_pixel_clip(buf, sw, sh, x + 1, y - 1, col, cx1, cy1, cx2, cy2);
        put_pixel_clip(buf, sw, sh, x + 3, y - 1, col, cx1, cy1, cx2, cy2);
        return 6;
    } else if (cp == 0x0451) { // ё
        draw_utf8_char_clip(buf, sw, sh, x, y, 0x0435, col, cx1, cy1, cx2, cy2);
        put_pixel_clip(buf, sw, sh, x + 1, y - 1, col, cx1, cy1, cx2, cy2);
        put_pixel_clip(buf, sw, sh, x + 3, y - 1, col, cx1, cy1, cx2, cy2);
        return 6;
    } else if (cp == 0x2014) { // Em-dash —
        for (int c = 0; c < 6; c++) put_pixel_clip(buf, sw, sh, x + c, y + 3, col, cx1, cy1, cx2, cy2);
        return 7;
    } else if (cp == 0x203A) { // Single right-pointing angle quotation mark ›
        put_pixel_clip(buf, sw, sh, x + 1, y + 1, col, cx1, cy1, cx2, cy2);
        put_pixel_clip(buf, sw, sh, x + 2, y + 2, col, cx1, cy1, cx2, cy2);
        put_pixel_clip(buf, sw, sh, x + 3, y + 3, col, cx1, cy1, cx2, cy2);
        put_pixel_clip(buf, sw, sh, x + 2, y + 4, col, cx1, cy1, cx2, cy2);
        put_pixel_clip(buf, sw, sh, x + 1, y + 5, col, cx1, cy1, cx2, cy2);
        return 5;
    } else if (cp == 0x25BE || cp == 0x25BC) { // Down arrowhead ▾
        put_pixel_clip(buf, sw, sh, x, y + 2, col, cx1, cy1, cx2, cy2);
        put_pixel_clip(buf, sw, sh, x + 1, y + 2, col, cx1, cy1, cx2, cy2);
        put_pixel_clip(buf, sw, sh, x + 2, y + 2, col, cx1, cy1, cx2, cy2);
        put_pixel_clip(buf, sw, sh, x + 3, y + 2, col, cx1, cy1, cx2, cy2);
        put_pixel_clip(buf, sw, sh, x + 4, y + 2, col, cx1, cy1, cx2, cy2);
        put_pixel_clip(buf, sw, sh, x + 1, y + 3, col, cx1, cy1, cx2, cy2);
        put_pixel_clip(buf, sw, sh, x + 2, y + 3, col, cx1, cy1, cx2, cy2);
        put_pixel_clip(buf, sw, sh, x + 3, y + 3, col, cx1, cy1, cx2, cy2);
        put_pixel_clip(buf, sw, sh, x + 2, y + 4, col, cx1, cy1, cx2, cy2);
        return 6;
    } else if (cp == 0x2713 || cp == 0x2714) { // Checkmark ✓
        put_pixel_clip(buf, sw, sh, x, y + 3, col, cx1, cy1, cx2, cy2);
        put_pixel_clip(buf, sw, sh, x + 1, y + 4, col, cx1, cy1, cx2, cy2);
        put_pixel_clip(buf, sw, sh, x + 2, y + 5, col, cx1, cy1, cx2, cy2);
        put_pixel_clip(buf, sw, sh, x + 3, y + 3, col, cx1, cy1, cx2, cy2);
        put_pixel_clip(buf, sw, sh, x + 4, y + 1, col, cx1, cy1, cx2, cy2);
        return 6;
    }
    draw_char_5x7_clip(buf, sw, sh, x, y, ' ', col, cx1, cy1, cx2, cy2);
    return 6;
}

static int draw_utf8_string_clip(uint32_t* buf, int sw, int sh, int x, int y,
                                 const char* str, uint32_t col, int cx1, int cy1, int cx2, int cy2) {
    if (!str) return 0;
    int cur_x = x;
    const char* p = str;
    while (*p) {
        uint32_t cp = 0;
        p = utf8_next_codepoint(p, &cp);
        if (cp == 0) break;
        cur_x += draw_utf8_char_clip(buf, sw, sh, cur_x, y, cp, col, cx1, cy1, cx2, cy2);
    }
    return cur_x - x;
}

static void add_link(struct html_doc* doc, int x, int y, int w, int h, const char* href) {
    if (!doc || doc->link_count >= MAX_PAGE_LINKS || !href) return;
    struct html_link* l = &doc->links[doc->link_count++];
    l->x = x;
    l->y = y;
    l->w = w;
    l->h = h;
    strncpy(l->href, href, sizeof(l->href) - 1);
    l->href[sizeof(l->href) - 1] = '\0';
}

int is_ddg_url(const char* url, char* query_out, size_t query_sz) {
    if (!url) return 0;
    if (query_out && query_sz > 0) {
        strncpy(query_out, DDG_DEFAULT_QUERY, query_sz - 1);
        query_out[query_sz - 1] = '\0';
    }

    if (strstr(url, DDG_ONION_HOST) != NULL ||
        strstr(url, "duckduckgo.com") != NULL ||
        strncmp(url, "ddg:", 4) == 0 ||
        strcmp(url, "ddg") == 0 ||
        strcmp(url, "linux") == 0 ||
        strncmp(url, "search:", 7) == 0) {
        
        const char* q = strstr(url, "q=");
        if (!q) q = strstr(url, "query=");
        if (q) {
            q += (q[1] == '=' ? 2 : 6);
            if (query_out && query_sz > 0) {
                size_t qi = 0;
                while (*q && *q != '&' && *q != ' ' && qi < query_sz - 1) {
                    query_out[qi++] = *q++;
                }
                query_out[qi] = '\0';
            }
        }
        return 1;
    }
    return 0;
}

static int draw_wrapped_text(uint32_t* buf, int sw, int sh, int x, int y, int max_chars,
                             const char* text, int max_lines, int line_h, uint32_t col,
                             int cx1, int cy1, int cx2, int cy2) {
    if (!text || !text[0]) return 0;
    const char* p = text;
    int cur_y = y;
    int line_count = 0;

    while (*p && line_count < max_lines) {
        while (*p == ' ') p++;
        if (!*p) break;

        char line_buf[128];
        size_t len = 0;
        const char* end = p;
        const char* last_space = NULL;

        while (*end && len < (size_t)max_chars) {
            if (*end == ' ') last_space = end;
            len++;
            end++;
        }

        if (*end != '\0' && last_space && last_space > p) {
            end = last_space;
        }

        size_t line_len = (size_t)(end - p);
        if (line_len >= sizeof(line_buf)) line_len = sizeof(line_buf) - 1;
        memcpy(line_buf, p, line_len);
        line_buf[line_len] = '\0';

        draw_utf8_string_clip(buf, sw, sh, x, cur_y, line_buf, col, cx1, cy1, cx2, cy2);
        cur_y += line_h;
        line_count++;
        p = end;
    }
    return line_count;
}

int ddg_serp_parse(const char* packet, struct ddg_serp_data* data) {
    if (!packet || !data) return -1;
    memset(data, 0, sizeof(*data));

    const char* p = packet;
    while (*p) {
        const char* end = strchr(p, '\n');
        size_t len = end ? (size_t)(end - p) : strlen(p);
        if (len > 0 && p[len - 1] == '\r') len--;

        char line[512];
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, p, len);
        line[len] = '\0';

        if (strncmp(line, "QUERY:", 6) == 0) {
            const char* val = line + 6;
            while (*val == ' ') val++;
            strncpy(data->query, val, sizeof(data->query) - 1);
        } else if (strncmp(line, "RESULT|", 7) == 0) {
            if (data->count < DDG_MAX_RESULTS) {
                struct ddg_search_result* r = &data->results[data->count];
                char* t = line + 7;
                char* u = strchr(t, '|');
                if (u) {
                    *u++ = '\0';
                    char* d = strchr(u, '|');
                    if (d) {
                        *d++ = '\0';
                        char* s = strchr(d, '|');
                        if (s) {
                            *s++ = '\0';
                            strncpy(r->snippet, s, sizeof(r->snippet) - 1);
                        }
                        strncpy(r->domain, d, sizeof(r->domain) - 1);
                    }
                    strncpy(r->url, u, sizeof(r->url) - 1);
                }
                strncpy(r->title, t, sizeof(r->title) - 1);
                data->count++;
            }
        } else if (strncmp(line, "CARD|", 5) == 0) {
            char* t = line + 5;
            char* x = strchr(t, '|');
            if (x) {
                *x++ = '\0';
                char* src = strchr(x, '|');
                if (src) {
                    *src++ = '\0';
                    char* u = strchr(src, '|');
                    if (u) {
                        *u++ = '\0';
                        strncpy(data->card_url, u, sizeof(data->card_url) - 1);
                    }
                    strncpy(data->card_source, src, sizeof(data->card_source) - 1);
                }
                strncpy(data->card_text, x, sizeof(data->card_text) - 1);
            }
            strncpy(data->card_title, t, sizeof(data->card_title) - 1);
        }

        if (!end) break;
        p = end + 1;
    }

    return (data->count > 0) ? 0 : -1;
}

void ddg_serp_init_defaults(struct ddg_serp_data* data, const char* query) {
    if (!data) return;
    memset(data, 0, sizeof(*data));
    const char* q = (query && query[0]) ? query : DDG_DEFAULT_QUERY;
    strncpy(data->query, q, sizeof(data->query) - 1);

    if (strstr(q, "tux") != NULL || strstr(q, "Tux") != NULL) {
        data->count = 4;
        strncpy(data->results[0].title, "Download Tux Paint", sizeof(data->results[0].title) - 1);
        strncpy(data->results[0].url, "https://tuxpaint.org/download/", sizeof(data->results[0].url) - 1);
        strncpy(data->results[0].domain, "tuxpaint.org", sizeof(data->results[0].domain) - 1);
        strncpy(data->results[0].snippet, "Tux Paint is a fun and easy-to-use painting program that runs on various platforms and devices. Download the latest version, view the gallery, or learn more about its features and history.", sizeof(data->results[0].snippet) - 1);

        strncpy(data->results[1].title, "Tux (mascot) - Wikipedia", sizeof(data->results[1].title) - 1);
        strncpy(data->results[1].url, "https://en.wikipedia.org/wiki/Tux_(mascot)", sizeof(data->results[1].url) - 1);
        strncpy(data->results[1].domain, "en.wikipedia.org", sizeof(data->results[1].domain) - 1);
        strncpy(data->results[1].snippet, "Tux is a penguin character and the official mascot of the Linux kernel, created by Linus Torvalds and Larry Ewing. Learn about the history, uses and reception of Tux, as well as its variations.", sizeof(data->results[1].snippet) - 1);

        strncpy(data->results[2].title, "Tux Paint - Free art software for kids of all ages", sizeof(data->results[2].title) - 1);
        strncpy(data->results[2].url, "https://tuxpaint.org/", sizeof(data->results[2].url) - 1);
        strncpy(data->results[2].domain, "tuxpaint.org", sizeof(data->results[2].domain) - 1);
        strncpy(data->results[2].snippet, "Tux Paint is a free, award-winning drawing program for children ages 3 to 12. Tux Paint is used in schools around the world as a computer literacy drawing activity.", sizeof(data->results[2].snippet) - 1);

        strncpy(data->results[3].title, "Tux Paint - Wikipedia", sizeof(data->results[3].title) - 1);
        strncpy(data->results[3].url, "https://en.wikipedia.org/wiki/Tux_Paint", sizeof(data->results[3].url) - 1);
        strncpy(data->results[3].domain, "en.wikipedia.org", sizeof(data->results[3].domain) - 1);
        strncpy(data->results[3].snippet, "Tux Paint is a free and open source raster graphics editor geared towards young children. The project was started in 2002 by Bill Kendrick who continues to maintain it.", sizeof(data->results[3].snippet) - 1);

        strncpy(data->card_title, "Tux", sizeof(data->card_title) - 1);
        strncpy(data->card_text, "Tux is a penguin character and the official brand mascot of the Linux kernel, created by Linus Torvalds and Larry Ewing in 1996.", sizeof(data->card_text) - 1);
        strncpy(data->card_source, "Wikipedia", sizeof(data->card_source) - 1);
        strncpy(data->card_url, "https://en.wikipedia.org/wiki/Tux_(mascot)", sizeof(data->card_url) - 1);
    } else {
        data->count = 4;
        strncpy(data->results[0].title, "Download Linux | Linux.org", sizeof(data->results[0].title) - 1);
        strncpy(data->results[0].url, "https://www.linux.org/pages/download/", sizeof(data->results[0].url) - 1);
        strncpy(data->results[0].domain, "Linux.org", sizeof(data->results[0].domain) - 1);
        strncpy(data->results[0].snippet, "Find links to popular Linux distributions and download pages on Linux.org Forums. Explore different Linux options.", sizeof(data->results[0].snippet) - 1);

        strncpy(data->results[1].title, "Linux.org", sizeof(data->results[1].title) - 1);
        strncpy(data->results[1].url, "https://www.linux.org/", sizeof(data->results[1].url) - 1);
        strncpy(data->results[1].domain, "Linux.org", sizeof(data->results[1].domain) - 1);
        strncpy(data->results[1].snippet, "Of course, many companies may need an OS other than Linux, such as Windows. The setup is straightforward like Linux.", sizeof(data->results[1].snippet) - 1);

        strncpy(data->results[2].title, "Linux \xE2\x80\x94 \xD0\x92\xD0\xB8\xD0\xBA\xD0\xB8\xD0\xBF\xD0\xB5\xD0\xB4\xD0\xB8\xD1\x8F", sizeof(data->results[2].title) - 1);
        strncpy(data->results[2].url, "https://ru.wikipedia.org/wiki/Linux", sizeof(data->results[2].url) - 1);
        strncpy(data->results[2].domain, "\xD0\x92\xD0\xB8\xD0\xBA\xD0\xB8\xD0\xBF\xD0\xB5\xD0\xB4\xD0\xB8\xD1\x8F", sizeof(data->results[2].domain) - 1);
        strncpy(data->results[2].snippet, "Linux-\xD1\x81\xD0\xB8\xD1\x81\xD1\x82\xD0\xB5\xD0\xBC\xD1\x8B \xD1\x80\xD0\xB5\xD0\xB0\xD0\xBB\xD0\xB8\xD0\xB7\xD1\x83\xD1\x8E\xD1\x82\xD1\x81\xD1\x8F \xD0\xBD\xD0\xB0 \xD0\xBC\xD0\xBE\xD0\xB4\xD1\x83\xD0\xBB\xD1\x8C\xD0\xBD\xD1\x8B\xD1\x85 \xD0\xBF\xD1\x80\xD0\xB8\xD0\xBD\xD1\x86\xD0\xB8\xD0\xBF\xD0\xB0\xD1\x85, \xD1\x81\xD1\x82\xD0\xB0\xD0\xBD\xD0\xB4\xD0\xB0\xD1\x80\xD1\x82\xD0\xB0\xD1\x85 \xD0\xB8 \xD1\x81\xD0\xBE\xD0\xB3\xD0\xBB\xD0\xB0\xD1\x88\xD0\xB5\xD0\xBD\xD0\xB8\xD1\x8F\xD1\x85.", sizeof(data->results[2].snippet) - 1);

        strncpy(data->results[3].title, "Linux - Wikipedia", sizeof(data->results[3].title) - 1);
        strncpy(data->results[3].url, "https://en.wikipedia.org/wiki/Linux", sizeof(data->results[3].url) - 1);
        strncpy(data->results[3].domain, "Wikipedia", sizeof(data->results[3].domain) - 1);
        strncpy(data->results[3].snippet, "Linux is a family of free and open-source software Unix-like operating systems based on Linux kernel.", sizeof(data->results[3].snippet) - 1);

        strncpy(data->card_title, "Linux", sizeof(data->card_title) - 1);
        strncpy(data->card_text, "Linux \xE2\x80\x94 \xD1\x81\xD0\xB5\xD0\xBC\xD0\xB5\xD0\xB9\xD1\x81\xD1\x82\xD0\xB2\xD0\xBE Unix-\xD0\xBF\xD0\xBE\xD0\xB4\xD0\xBE\xD0\xB1\xD0\xBD\xD1\x8B\xD1\x85 \xD0\xBE\xD0\xBF\xD0\xB5\xD1\x80\xD0\xB0\xD1\x86\xD0\xB8\xD0\xBE\xD0\xBD\xD0\xBD\xD1\x8B\xD1\x85 \xD1\x81\xD0\xB8\xD1\x81\xD1\x82\xD0\xB5\xD0\xBC \xD0\xBD\xD0\xB0 \xD0\xB1\xD0\xB0\xD0\xB7\xD0\xB5 \xD1\x8F\xD0\xB4\xD1\x80\xD0\xB0 Linux, \xD0\xB2\xD0\xBA\xD0\xBB\xD1\x8E\xD1\x87\xD0\xB0\xD1\x8E\xD1\x89\xD0\xB8\xD1\x85 \xD1\x82\xD0\xBE\xD1\x82 \xD0\xB8\xD0\xBB\xD0\xB8 \xD0\xB8\xD0\xBD\xD0\xBE\xD0\xB9 \xD0\xBD\xD0\xB0\xD0\xB1\xD0\xBE\xD1\x80 \xD1\x83\xD1\x82\xD0\xB8\xD0\xBB\xD0\xB8\xD1\x82 \xD0\xB8 \xD0\xBF\xD1\x80\xD0\xBE\xD0\xB3\xD1\x80\xD0\xB0\xD0\xBC\xD0\xBC GNU.", sizeof(data->card_text) - 1);
        strncpy(data->card_source, "Wikipedia (RU)", sizeof(data->card_source) - 1);
        strncpy(data->card_url, "https://ru.wikipedia.org/wiki/Linux", sizeof(data->card_url) - 1);
    }
}

void ddg_serp_render(const char* query, const struct ddg_serp_data* data,
                     uint32_t* backbuf, int screen_w, int screen_h,
                     int vp_x, int vp_y, int vp_w, int vp_h, int scroll_y,
                     struct html_doc* doc_out) {
    if (!doc_out) return;
    memset(doc_out, 0, sizeof(*doc_out));
    const char* qtext = (query && query[0]) ? query : DDG_DEFAULT_QUERY;
    snprintf(doc_out->title, sizeof(doc_out->title), "%s at DuckDuckGo", qtext);

    int cx1 = vp_x;
    int cy1 = vp_y;
    int cx2 = vp_x + vp_w;
    int cy2 = vp_y + vp_h;

    // 1. Clear Viewport Background to Clean White
    fill_rect_clip(backbuf, screen_w, screen_h, vp_x, vp_y, vp_w, vp_h, COL_BG_WHITE, cx1, cy1, cx2, cy2);

    int doc_y = 6;

    // -------------------------------------------------------------
    // TOP HEADER: Dax Penguin Logo & Pill Search Bar
    // -------------------------------------------------------------
    int header_y = vp_y + doc_y - scroll_y;
    // DuckDuckGo Logo (Dax in penguin suit)
    draw_bitmap_clip(backbuf, screen_w, screen_h, vp_x + 12, header_y,
                     DDG_LOGO_W, DDG_LOGO_H, ddg_logo_pixels, cx1, cy1, cx2, cy2);
    add_link(doc_out, vp_x + 12, header_y, DDG_LOGO_W, DDG_LOGO_H, "home");

    // Pill Search Input Box
    int sbox_x = vp_x + 42;
    int sbox_w = 400;
    int sbox_h = 24;
    draw_rect_clip(backbuf, screen_w, screen_h, sbox_x, header_y, sbox_w, sbox_h,
                   COL_BORDER_GRAY, COL_BG_WHITE, cx1, cy1, cx2, cy2);
    add_link(doc_out, sbox_x, header_y, sbox_w, sbox_h, "action:edit_search");

    // Inside text in 8x16 font
    int qx = sbox_x + 10;
    for (size_t i = 0; i < strlen(qtext) && i < 35; i++) {
        draw_char_8x16_clip(backbuf, screen_w, screen_h, qx + (int)i * 8, header_y + 4, qtext[i], COL_TEXT_MAIN, cx1, cy1, cx2, cy2);
    }

    // Magnifying glass icon on right of search pill
    int mg_x = sbox_x + sbox_w - 20;
    int mg_y = header_y + 7;
    for (int dy = 0; dy <= 5; dy++) {
        for (int dx = 0; dx <= 5; dx++) {
            if ((dx == 0 || dx == 5) && dy >= 1 && dy <= 4)
                put_pixel_clip(backbuf, screen_w, screen_h, mg_x + dx, mg_y + dy, COL_TEXT_MUTED, cx1, cy1, cx2, cy2);
            else if ((dy == 0 || dy == 5) && dx >= 1 && dx <= 4)
                put_pixel_clip(backbuf, screen_w, screen_h, mg_x + dx, mg_y + dy, COL_TEXT_MUTED, cx1, cy1, cx2, cy2);
        }
    }
    put_pixel_clip(backbuf, screen_w, screen_h, mg_x + 6, mg_y + 6, COL_TEXT_MUTED, cx1, cy1, cx2, cy2);
    put_pixel_clip(backbuf, screen_w, screen_h, mg_x + 7, mg_y + 7, COL_TEXT_MUTED, cx1, cy1, cx2, cy2);
    put_pixel_clip(backbuf, screen_w, screen_h, mg_x + 8, mg_y + 8, COL_TEXT_MUTED, cx1, cy1, cx2, cy2);

    doc_y += 30;

    // -------------------------------------------------------------
    // NAVIGATION TABS ROW
    // -------------------------------------------------------------
    int tabs_y = vp_y + doc_y - scroll_y;
    int tab_x = vp_x + 42;

    // [Q All] (Active Tab with Blue Underline)
    draw_utf8_string_clip(backbuf, screen_w, screen_h, tab_x, tabs_y, "Q All", COL_ACTIVE_BLUE, cx1, cy1, cx2, cy2);
    for (int bx = tab_x; bx < tab_x + 36; bx++) {
        put_pixel_clip(backbuf, screen_w, screen_h, bx, tabs_y + 11, COL_ACTIVE_BLUE, cx1, cy1, cx2, cy2);
        put_pixel_clip(backbuf, screen_w, screen_h, bx, tabs_y + 12, COL_ACTIVE_BLUE, cx1, cy1, cx2, cy2);
    }
    add_link(doc_out, tab_x, tabs_y, 40, 16, "ddg:all");
    tab_x += 48;

    draw_utf8_string_clip(backbuf, screen_w, screen_h, tab_x, tabs_y, "Images", COL_TEXT_MUTED, cx1, cy1, cx2, cy2);
    add_link(doc_out, tab_x, tabs_y, 44, 16, "ddg:images");
    tab_x += 46;
    draw_utf8_string_clip(backbuf, screen_w, screen_h, tab_x, tabs_y, "Videos", COL_TEXT_MUTED, cx1, cy1, cx2, cy2);
    add_link(doc_out, tab_x, tabs_y, 44, 16, "ddg:videos");
    tab_x += 46;
    draw_utf8_string_clip(backbuf, screen_w, screen_h, tab_x, tabs_y, "News", COL_TEXT_MUTED, cx1, cy1, cx2, cy2);
    add_link(doc_out, tab_x, tabs_y, 36, 16, "ddg:news");
    tab_x += 38;
    draw_utf8_string_clip(backbuf, screen_w, screen_h, tab_x, tabs_y, "More \xE2\x96\xBE", COL_TEXT_MUTED, cx1, cy1, cx2, cy2);

    int gear_x = vp_x + vp_w - 24;
    draw_char_5x7_clip(backbuf, screen_w, screen_h, gear_x, tabs_y, '*', COL_TEXT_MUTED, cx1, cy1, cx2, cy2);

    doc_y += 18;

    // -------------------------------------------------------------
    // FILTER BADGES ROW
    // -------------------------------------------------------------
    int filt_y = vp_y + doc_y - scroll_y;
    int fx = vp_x + 42;

    int pbadge_w = 64;
    draw_rect_clip(backbuf, screen_w, screen_h, fx, filt_y - 2, pbadge_w, 15,
                   0xFF86EFAC, COL_BADGE_BG_GREEN, cx1, cy1, cx2, cy2);
    draw_utf8_string_clip(backbuf, screen_w, screen_h, fx + 4, filt_y + 2, "\xE2\x9C\x93 Private \xE2\x96\xBE", COL_BADGE_TXT_GREEN, cx1, cy1, cx2, cy2);
    fx += pbadge_w + 10;

    draw_rect_clip(backbuf, screen_w, screen_h, fx, filt_y - 1, 18, 12, COL_ACTIVE_BLUE, COL_ACTIVE_BLUE, cx1, cy1, cx2, cy2);
    fill_rect_clip(backbuf, screen_w, screen_h, fx + 1, filt_y, 8, 10, COL_BG_WHITE, cx1, cy1, cx2, cy2);
    draw_utf8_string_clip(backbuf, screen_w, screen_h, fx + 22, filt_y + 2, "Russia \xE2\x96\xBE", COL_TEXT_MAIN, cx1, cy1, cx2, cy2);
    fx += 78;

    draw_utf8_string_clip(backbuf, screen_w, screen_h, fx, filt_y + 2, "Safe search: moderate \xE2\x96\xBE", COL_TEXT_MUTED, cx1, cy1, cx2, cy2);
    fx += 134;

    draw_utf8_string_clip(backbuf, screen_w, screen_h, fx, filt_y + 2, "Any time \xE2\x96\xBE", COL_TEXT_MUTED, cx1, cy1, cx2, cy2);

    doc_y += 18;

    // Thin separator line
    int sep_y = vp_y + doc_y - scroll_y;
    for (int x = vp_x + 10; x < vp_x + vp_w - 10; x++) {
        put_pixel_clip(backbuf, screen_w, screen_h, x, sep_y, COL_CARD_BORDER, cx1, cy1, cx2, cy2);
    }
    doc_y += 8;

    // -------------------------------------------------------------
    // MAIN CONTENT AREA: TWO-COLUMN SPLIT LAYOUT
    // -------------------------------------------------------------
    int left_col_x = vp_x + 42;
    int right_col_x = vp_x + 400;
    int right_col_w = 236;

    int start_content_doc_y = doc_y;

    // =============================================================
    // RIGHT COLUMN: KNOWLEDGE GRAPH CARD
    // =============================================================
    int card_top_doc_y = doc_y;
    int card_screen_y = vp_y + card_top_doc_y - scroll_y;
    int card_h = 196;

    if (data && data->card_title[0]) {
        draw_rect_clip(backbuf, screen_w, screen_h, right_col_x, card_screen_y, right_col_w, card_h,
                       COL_CARD_BORDER, COL_BG_WHITE, cx1, cy1, cx2, cy2);

        // Card Title
        int c_head_y = card_screen_y + 10;
        for (size_t i = 0; i < strlen(data->card_title) && i < 18; i++) {
            draw_char_8x16_clip(backbuf, screen_w, screen_h, right_col_x + 10 + (int)i * 8, c_head_y, data->card_title[i], COL_TEXT_MAIN, cx1, cy1, cx2, cy2);
        }

        // Thumbnail
        int thumb_x = right_col_x + right_col_w - DDG_THUMB_W - 10;
        int thumb_y = card_screen_y + 8;
        if (strstr(data->card_title, "Tux") != NULL || strstr(qtext, "tux") != NULL) {
            draw_bitmap_clip(backbuf, screen_w, screen_h, thumb_x + 20, thumb_y, DDG_LOGO_W, DDG_LOGO_H, ddg_logo_pixels, cx1, cy1, cx2, cy2);
        } else {
            draw_bitmap_clip(backbuf, screen_w, screen_h, thumb_x, thumb_y, DDG_THUMB_W, DDG_THUMB_H, ddg_thumb_pixels, cx1, cy1, cx2, cy2);
        }
        add_link(doc_out, right_col_x, card_screen_y, right_col_w, 60, data->card_url[0] ? data->card_url : "https://duckduckgo.com");

        // Summary Text
        int c_text_y = card_screen_y + 60;
        draw_wrapped_text(backbuf, screen_w, screen_h, right_col_x + 10, c_text_y, 35, data->card_text, 6, 12, COL_TEXT_SNIPPET, cx1, cy1, cx2, cy2);

        // Link: Continued in ...
        int c_link_y = c_text_y + 76;
        char cont_buf[64];
        snprintf(cont_buf, sizeof(cont_buf), "Continued in %s", data->card_source[0] ? data->card_source : "Wikipedia");
        draw_utf8_string_clip(backbuf, screen_w, screen_h, right_col_x + 10, c_link_y, cont_buf, COL_LINK_BLUE, cx1, cy1, cx2, cy2);
        add_link(doc_out, right_col_x + 10, c_link_y - 2, 210, 16, data->card_url[0] ? data->card_url : "https://duckduckgo.com");

        // Footer divider
        int c_div_y = card_screen_y + card_h - 22;
        for (int x = right_col_x; x < right_col_x + right_col_w; x++) {
            put_pixel_clip(backbuf, screen_w, screen_h, x, c_div_y, COL_CARD_BORDER, cx1, cy1, cx2, cy2);
        }
        draw_utf8_string_clip(backbuf, screen_w, screen_h, right_col_x + (right_col_w / 2) - 4, c_div_y + 6, "\xE2\x96\xBE", COL_TEXT_MUTED, cx1, cy1, cx2, cy2);

        // Source & Feedback
        int c_foot_y = card_screen_y + card_h + 8;
        draw_utf8_string_clip(backbuf, screen_w, screen_h, right_col_x + 10, c_foot_y, "Source: ", COL_TEXT_MUTED, cx1, cy1, cx2, cy2);
        draw_utf8_string_clip(backbuf, screen_w, screen_h, right_col_x + 52, c_foot_y, data->card_source, COL_LINK_BLUE, cx1, cy1, cx2, cy2);
        add_link(doc_out, right_col_x + 10, c_foot_y - 2, 210, 16, data->card_url);
        draw_utf8_string_clip(backbuf, screen_w, screen_h, right_col_x + 52 + (int)strlen(data->card_source) * 6 + 6, c_foot_y, "\xC2\xB7 Was this helpful?", COL_TEXT_MUTED, cx1, cy1, cx2, cy2);
    }

    // =============================================================
    // LEFT COLUMN: WEB SEARCH RESULTS
    // =============================================================
    doc_y = start_content_doc_y;
    int res_count = (data && data->count > 0) ? data->count : 0;

    for (int i = 0; i < res_count; i++) {
        const struct ddg_search_result* r = &data->results[i];
        int r_y = vp_y + doc_y - scroll_y;

        const uint32_t* fav = (strstr(r->domain, "wiki") != NULL) ? ddg_fav_wiki : ddg_fav_linux;
        draw_bitmap_clip(backbuf, screen_w, screen_h, left_col_x, r_y, 12, 12, fav, cx1, cy1, cx2, cy2);
        draw_utf8_string_clip(backbuf, screen_w, screen_h, left_col_x + 16, r_y + 2, r->domain, COL_TEXT_MAIN, cx1, cy1, cx2, cy2);
        doc_y += 14;

        int r_url_y = vp_y + doc_y - scroll_y;
        char crumb_buf[140];
        snprintf(crumb_buf, sizeof(crumb_buf), "%s   \xC2\xB7\xC2\xB7\xC2\xB7", r->url);
        draw_utf8_string_clip(backbuf, screen_w, screen_h, left_col_x, r_url_y, crumb_buf, COL_TEXT_MUTED, cx1, cy1, cx2, cy2);
        doc_y += 12;

        int r_title_y = vp_y + doc_y - scroll_y;
        draw_utf8_string_clip(backbuf, screen_w, screen_h, left_col_x, r_title_y, r->title, COL_LINK_BLUE, cx1, cy1, cx2, cy2);
        doc_y += 14;

        int r_snip_y = vp_y + doc_y - scroll_y;
        int snip_lines = draw_wrapped_text(backbuf, screen_w, screen_h, left_col_x, r_snip_y, 56, r->snippet, 2, 11, COL_TEXT_SNIPPET, cx1, cy1, cx2, cy2);
        int item_h = (r_snip_y + (snip_lines > 0 ? snip_lines * 11 : 11)) - r_y + 4;
        add_link(doc_out, left_col_x, r_y, 350, item_h, r->url);
        doc_y += (snip_lines > 0 ? snip_lines * 11 : 11) + 16;

        // If result 0 is Linux.org and query is linux, show sitelinks grid matching photo
        if (i == 0 && strcmp(qtext, "linux") == 0) {
            int sl_c1_x = left_col_x + 6;
            int sl_c2_x = left_col_x + 175;

            // Row 1
            int sl_r1_y = vp_y + doc_y - scroll_y;
            draw_utf8_string_clip(backbuf, screen_w, screen_h, sl_c1_x, sl_r1_y, "Advanced Tutorials", COL_LINK_BLUE, cx1, cy1, cx2, cy2);
            add_link(doc_out, sl_c1_x, sl_r1_y, 110, 9, "https://www.linux.org/forums/#tutorials");
            draw_utf8_string_clip(backbuf, screen_w, screen_h, sl_c1_x, sl_r1_y + 9, "Advanced Tutorials - Down...", COL_TEXT_MUTED, cx1, cy1, cx2, cy2);

            draw_utf8_string_clip(backbuf, screen_w, screen_h, sl_c2_x, sl_r1_y, "Search", COL_LINK_BLUE, cx1, cy1, cx2, cy2);
            add_link(doc_out, sl_c2_x, sl_r1_y, 40, 9, "https://www.linux.org/search/");
            draw_utf8_string_clip(backbuf, screen_w, screen_h, sl_c2_x, sl_r1_y + 9, "\xD0\x9C\xD1\x8B \xD1\x85\xD0\xBE\xD1\x82\xD0\xB5\xD0\xBB\xD0\xB8 \xD0\xB1\xD1\x8B \xD0\xBF\xD0\xBE\xD0\xBA\xD0\xB0\xD0\xB7\xD0\xB0\xD1\x82\xD1\x8C...", COL_TEXT_MUTED, cx1, cy1, cx2, cy2);
            doc_y += 22;

            // Row 2
            int sl_r2_y = vp_y + doc_y - scroll_y;
            draw_utf8_string_clip(backbuf, screen_w, screen_h, sl_c1_x, sl_r2_y, "Help", COL_LINK_BLUE, cx1, cy1, cx2, cy2);
            add_link(doc_out, sl_c1_x, sl_r2_y, 30, 9, "https://www.linux.org/help/");
            draw_utf8_string_clip(backbuf, screen_w, screen_h, sl_c1_x, sl_r2_y + 9, "Help - Linux.org Forums...", COL_TEXT_MUTED, cx1, cy1, cx2, cy2);

            draw_utf8_string_clip(backbuf, screen_w, screen_h, sl_c2_x, sl_r2_y, "Privacy Policy", COL_LINK_BLUE, cx1, cy1, cx2, cy2);
            add_link(doc_out, sl_c2_x, sl_r2_y, 80, 9, "https://www.linux.org/help/privacy-policy/");
            draw_utf8_string_clip(backbuf, screen_w, screen_h, sl_c2_x, sl_r2_y + 9, "We are Linux.org (commit...)", COL_TEXT_MUTED, cx1, cy1, cx2, cy2);
            doc_y += 22;

            // Row 3
            int sl_r3_y = vp_y + doc_y - scroll_y;
            draw_utf8_string_clip(backbuf, screen_w, screen_h, sl_c1_x, sl_r3_y, "Linux.Org, What's New", COL_LINK_BLUE, cx1, cy1, cx2, cy2);
            add_link(doc_out, sl_c1_x, sl_r3_y, 120, 9, "https://www.linux.org/whats-new/");
            draw_utf8_string_clip(backbuf, screen_w, screen_h, sl_c1_x, sl_r3_y + 9, "What's new - Linux.org...", COL_TEXT_MUTED, cx1, cy1, cx2, cy2);

            draw_utf8_string_clip(backbuf, screen_w, screen_h, sl_c2_x, sl_r3_y, "Forums", COL_LINK_BLUE, cx1, cy1, cx2, cy2);
            add_link(doc_out, sl_c2_x, sl_r3_y, 45, 9, "https://www.linux.org/forums/");
            draw_utf8_string_clip(backbuf, screen_w, screen_h, sl_c2_x, sl_r3_y + 9, "Server Linux section...", COL_TEXT_MUTED, cx1, cy1, cx2, cy2);
            doc_y += 26;
        }
    }

    int total_h = (doc_y > (card_top_doc_y + card_h + 30)) ? doc_y : (card_top_doc_y + card_h + 30);
    doc_out->total_height = total_h + 40;
}
