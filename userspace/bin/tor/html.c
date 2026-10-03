/*
 * SeldOS - Humboldt Kernel Project
 * Tor Browser (OpSec Sovereign Edition)
 * Fast Sovereign HTML Layout & Flow Engine
 * GPLv3 Licensed.
 */

#include "html.h"
#include <font.h>
#include <string.h>
#include <stdlib.h>

#define COL_TEXT_LIGHT   0xFFE2E8F0
#define COL_TEXT_WHITE   0xFFFFFFFF
#define COL_TEXT_MUTED   0xFF94A3B8
#define COL_GOLD_H1      0xFFF59E0B
#define COL_PURPLE_H2    0xFFA855F7
#define COL_LINK_BLUE    0xFF60A5FA
#define COL_LINK_UNDER   0xFF3B82F6
#define COL_CODE_GREEN   0xFF10B981
#define COL_HR_BORDER    0xFF392552
#define COL_BLOCKQUOTE   0xFF7D4698

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
    if (x < 0 || x >= sw || y < 0 || y >= sh) return;
    buf[y * sw + x] = col;
}

static void draw_char_5x7(uint32_t* buf, int sw, int sh, int x, int y, char ch, uint32_t col,
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

static void draw_char_8x16(uint32_t* buf, int sw, int sh, int x, int y, char ch, uint32_t col,
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

static void extract_attr(const char* tag_str, const char* attr_name, char* out, size_t out_sz) {
    out[0] = '\0';
    const char* p = strstr(tag_str, attr_name);
    if (!p) return;
    p += strlen(attr_name);
    while (*p == ' ' || *p == '=') p++;
    char quote = 0;
    if (*p == '"' || *p == '\'') {
        quote = *p++;
    }
    size_t idx = 0;
    while (*p && idx < out_sz - 1) {
        if (quote && *p == quote) break;
        if (!quote && (*p == ' ' || *p == '>')) break;
        out[idx++] = *p++;
    }
    out[idx] = '\0';
}

void html_render(const char* html, uint32_t* backbuf, int screen_w, int screen_h,
                 int vp_x, int vp_y, int vp_w, int vp_h, int scroll_y,
                 struct html_doc* doc_out) {
    if (!html || !doc_out) return;
    memset(doc_out, 0, sizeof(*doc_out));
    strncpy(doc_out->title, "Tor Sovereign Browser", sizeof(doc_out->title) - 1);

    int cx = vp_x;
    int doc_y = 10;
    int base_margin_left = vp_x;
    int margin_left = base_margin_left;
    int max_x = vp_x + vp_w - 12;

    int heading_level = 0;
    int in_link = 0;
    int in_pre = 0;
    int in_title = 0;
    int in_style = 0;
    int in_script = 0;

    char cur_href[128] = {0};
    uint32_t cur_col = COL_TEXT_LIGHT;
    const char* p = html;

    while (*p) {
        // 0. Skip HTML comments <!-- ... -->
        if (strncmp(p, "<!--", 4) == 0) {
            const char* c_end = strstr(p + 4, "-->");
            if (c_end) {
                p = c_end + 3;
                continue;
            } else {
                break;
            }
        }

        // 1. Skip <script ...> ... </script> blocks completely
        if (strncasecmp(p, "<script", 7) == 0) {
            const char* s_end = p + 7;
            while (*s_end) {
                if (*s_end == '<' && strncasecmp(s_end, "</script>", 9) == 0) {
                    p = s_end + 9;
                    break;
                }
                s_end++;
            }
            if (!*s_end) break;
            continue;
        }

        // 2. Skip <style ...> ... </style> blocks completely
        if (strncasecmp(p, "<style", 6) == 0) {
            const char* s_end = p + 6;
            while (*s_end) {
                if (*s_end == '<' && strncasecmp(s_end, "</style>", 8) == 0) {
                    p = s_end + 8;
                    break;
                }
                s_end++;
            }
            if (!*s_end) break;
            continue;
        }

        // 3. Tag parsing
        if (*p == '<') {
            const char* tag_start = p + 1;
            const char* tag_end = strchr(tag_start, '>');
            if (!tag_end) break;

            char tag_buf[256];
            size_t tlen = (size_t)(tag_end - tag_start);
            if (tlen >= sizeof(tag_buf)) tlen = sizeof(tag_buf) - 1;
            memcpy(tag_buf, tag_start, tlen);
            tag_buf[tlen] = '\0';

            p = tag_end + 1;

            // Strip leading spaces in tag name
            char* t = tag_buf;
            while (*t == ' ') t++;

            if (strncasecmp(t, "title", 5) == 0) {
                in_title = 1;
            } else if (strncasecmp(t, "/title", 6) == 0) {
                in_title = 0;
            } else if (strncasecmp(t, "style", 5) == 0) {
                in_style = 1;
            } else if (strncasecmp(t, "/style", 6) == 0) {
                in_style = 0;
            } else if (strncasecmp(t, "script", 6) == 0) {
                in_script = 1;
            } else if (strncasecmp(t, "/script", 7) == 0) {
                in_script = 0;
            } else if (strncasecmp(t, "h1", 2) == 0) {
                heading_level = 1;
                cx = margin_left;
                doc_y += 18;
                cur_col = COL_GOLD_H1;
            } else if (strncasecmp(t, "/h1", 3) == 0) {
                heading_level = 0;
                cx = margin_left;
                doc_y += 20;
                cur_col = COL_TEXT_LIGHT;
            } else if (strncasecmp(t, "h2", 2) == 0) {
                heading_level = 2;
                cx = margin_left;
                doc_y += 14;
                cur_col = COL_PURPLE_H2;
            } else if (strncasecmp(t, "/h2", 3) == 0) {
                heading_level = 0;
                cx = margin_left;
                doc_y += 16;
                cur_col = COL_TEXT_LIGHT;
            } else if (strncasecmp(t, "h3", 2) == 0 || strncasecmp(t, "h4", 2) == 0) {
                heading_level = 3;
                cx = margin_left;
                doc_y += 10;
                cur_col = COL_TEXT_WHITE;
            } else if (strncasecmp(t, "/h3", 3) == 0 || strncasecmp(t, "/h4", 3) == 0) {
                heading_level = 0;
                cx = margin_left;
                doc_y += 12;
                cur_col = COL_TEXT_LIGHT;
            } else if (strncasecmp(t, "p", 1) == 0 && (t[1] == ' ' || t[1] == '\0')) {
                cx = margin_left;
                doc_y += 10;
            } else if (strncasecmp(t, "/p", 2) == 0) {
                cx = margin_left;
                doc_y += 10;
            } else if (strncasecmp(t, "br", 2) == 0) {
                cx = margin_left;
                doc_y += (heading_level ? 16 : 12);
            } else if (strncasecmp(t, "hr", 2) == 0) {
                cx = margin_left;
                doc_y += 8;
                int screen_y = vp_y + doc_y - scroll_y;
                if (screen_y >= vp_y && screen_y < vp_y + vp_h && backbuf) {
                    for (int x = vp_x; x < vp_x + vp_w - 16; x++) {
                        put_pixel_clip(backbuf, screen_w, screen_h, x, screen_y, COL_HR_BORDER,
                                       vp_x, vp_y, vp_x + vp_w, vp_y + vp_h);
                    }
                }
                doc_y += 8;
            } else if (strncasecmp(t, "ul", 2) == 0 || strncasecmp(t, "ol", 2) == 0) {
                margin_left = base_margin_left + 16;
                cx = margin_left;
                doc_y += 6;
            } else if (strncasecmp(t, "/ul", 3) == 0 || strncasecmp(t, "/ol", 3) == 0) {
                margin_left = base_margin_left;
                cx = margin_left;
                doc_y += 6;
            } else if (strncasecmp(t, "li", 2) == 0) {
                cx = margin_left;
                doc_y += 12;
                // Render bullet marker
                int screen_y = vp_y + doc_y - scroll_y;
                if (screen_y >= vp_y && screen_y < vp_y + vp_h && backbuf) {
                    draw_char_5x7(backbuf, screen_w, screen_h, cx - 10, screen_y + 1, '*', COL_PURPLE_H2,
                                  vp_x, vp_y, vp_x + vp_w, vp_y + vp_h);
                }
            } else if (strncasecmp(t, "blockquote", 10) == 0) {
                margin_left = base_margin_left + 16;
                cx = margin_left;
                doc_y += 8;
            } else if (strncasecmp(t, "/blockquote", 11) == 0) {
                margin_left = base_margin_left;
                cx = margin_left;
                doc_y += 8;
            } else if (strncasecmp(t, "code", 4) == 0 || strncasecmp(t, "pre", 3) == 0) {
                in_pre = 1;
                cur_col = COL_CODE_GREEN;
            } else if (strncasecmp(t, "/code", 5) == 0 || strncasecmp(t, "/pre", 4) == 0) {
                in_pre = 0;
                cur_col = COL_TEXT_LIGHT;
            } else if (strncasecmp(t, "a", 1) == 0 && (t[1] == ' ' || t[1] == '\0')) {
                in_link = 1;
                extract_attr(t, "href", cur_href, sizeof(cur_href));
                cur_col = COL_LINK_BLUE;
            } else if (strncasecmp(t, "/a", 2) == 0) {
                in_link = 0;
                cur_href[0] = '\0';
                cur_col = heading_level ? COL_GOLD_H1 : COL_TEXT_LIGHT;
            }

            continue;
        }

        // Title text accumulation
        if (in_title) {
            size_t tidx = 0;
            while (*p && *p != '<' && tidx < sizeof(doc_out->title) - 1) {
                doc_out->title[tidx++] = *p++;
            }
            doc_out->title[tidx] = '\0';
            continue;
        }

        // Skip script and style blocks
        if (in_style || in_script) {
            p++;
            continue;
        }

        // Whitespace handling
        if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            if (*p == '\n' && in_pre) {
                cx = margin_left;
                doc_y += 12;
            } else if (cx > margin_left) {
                cx += (heading_level ? 8 : 6);
            }
            p++;
            continue;
        }

        // Word parsing (consecutive non-whitespace characters)
        char word[128];
        size_t widx = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && *p != '<' && widx < sizeof(word) - 1) {
            // HTML Entity Decoding
            if (*p == '&') {
                if (strncmp(p, "&amp;", 5) == 0) { word[widx++] = '&'; p += 5; }
                else if (strncmp(p, "&lt;", 4) == 0) { word[widx++] = '<'; p += 4; }
                else if (strncmp(p, "&gt;", 4) == 0) { word[widx++] = '>'; p += 4; }
                else if (strncmp(p, "&quot;", 6) == 0) { word[widx++] = '"'; p += 6; }
                else if (strncmp(p, "&#39;", 5) == 0) { word[widx++] = '\''; p += 5; }
                else if (strncmp(p, "&nbsp;", 6) == 0) { word[widx++] = ' '; p += 6; }
                else { word[widx++] = *p++; }
            } else {
                word[widx++] = *p++;
            }
        }
        word[widx] = '\0';

        if (widx == 0) continue;

        int char_w = (heading_level ? 8 : 6);
        int line_h = (heading_level ? 16 : 12);
        int word_w = (int)widx * char_w;

        // Word wrap
        if (cx + word_w > max_x) {
            cx = margin_left;
            doc_y += line_h;
        }

        int screen_x = cx;
        int screen_y = vp_y + doc_y - scroll_y;

        // Register link hitbox if in <a>
        if (in_link && cur_href[0] != '\0') {
            if (doc_out->link_count < MAX_PAGE_LINKS) {
                struct html_link* l = &doc_out->links[doc_out->link_count++];
                l->x = screen_x;
                l->y = screen_y;
                l->w = word_w;
                l->h = line_h;
                strncpy(l->href, cur_href, sizeof(l->href) - 1);
                l->href[sizeof(l->href) - 1] = '\0';
            }
        }

        // Render word glyphs if in visible viewport
        if (screen_y + line_h >= vp_y && screen_y < vp_y + vp_h && backbuf) {
            for (size_t i = 0; i < widx; i++) {
                int px = screen_x + (int)i * char_w;
                if (heading_level) {
                    draw_char_8x16(backbuf, screen_w, screen_h, px, screen_y, word[i], cur_col,
                                   vp_x, vp_y, vp_x + vp_w, vp_y + vp_h);
                } else {
                    draw_char_5x7(backbuf, screen_w, screen_h, px, screen_y + 2, word[i], cur_col,
                                  vp_x, vp_y, vp_x + vp_w, vp_y + vp_h);
                }
            }

            // Draw link underline
            if (in_link) {
                int uy = screen_y + line_h - 1;
                for (int ux = screen_x; ux < screen_x + word_w; ux++) {
                    put_pixel_clip(backbuf, screen_w, screen_h, ux, uy, COL_LINK_UNDER,
                                   vp_x, vp_y, vp_x + vp_w, vp_y + vp_h);
                }
            }
        }

        cx += word_w;
    }

    doc_out->total_height = doc_y + 24;
}
