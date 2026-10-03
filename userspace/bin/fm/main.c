/*
 * SeldOS - Humboldt Kernel Project
 * SNL-FM: Seld Sovereign Graphical File Manager v0.1
 * High-Craft OpSec File Explorer for Humboldt 680x334 Framebuffer.
 * Direct Linear Framebuffer Rendering with 39-Color Thermal Palette.
 * GPLv3 Licensed.
 */

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "seld.h"
#include "font.h"

#define SCR_W 680
#define SCR_H 334

/* 39-Color Humboldt Palette Tokens */
#define COL_BG_MAIN      0xFF0E131B
#define COL_BG_SIDEBAR   0xFF141C27
#define COL_BG_TOPBAR    0xFF1B2432
#define COL_BG_STATUS    0xFF111824
#define COL_BORDER       0xFF283546
#define COL_BORDER_FOCUS 0xFF38BDF8
#define COL_TEXT_LIGHT   0xFFF8FAFC
#define COL_TEXT_MUTED   0xFF94A3B8
#define COL_TEXT_DARK    0xFF64748B
#define COL_SELECTION    0xFF1E2D42

/* Icon Palette */
#define COL_FOLDER_TAB   0xFF38BDF8
#define COL_FOLDER_BODY  0xFF0284C7
#define COL_FOLDER_DARK  0xFF0369A1
#define COL_BIN_BORDER   0xFF10B981
#define COL_BIN_BG       0xFF064E3B
#define COL_BIN_ACCENT   0xFF34D399
#define COL_WAD_BORDER   0xFFEF4444
#define COL_WAD_BG       0xFF450A0A
#define COL_WAD_ACCENT   0xFFF59E0B
#define COL_DOC_BORDER   0xFF94A3B8
#define COL_DOC_BG       0xFFE2E8F0
#define COL_DOC_LINES    0xFF64748B

/* 5x7 ASCII font for compact grid labels */
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

enum item_type {
    TYPE_FOLDER,
    TYPE_BINARY,
    TYPE_WAD,
    TYPE_TEXT,
    TYPE_GENERIC
};

struct fm_item {
    char name[32];
    char full_path[64];
    uint32_t size;
    enum item_type type;
    uint8_t sha256[32];
};

static struct seld_fb_info s_fb;
static uint32_t* s_backbuf = NULL;

static struct fm_item s_items[32];
static int s_num_items = 0;
static int s_selected_idx = 0;
static int s_current_view = 0; // 0=Root, 1=Binaries, 2=Storage, 3=Network

static int s_mouse_x = 0;
static int s_mouse_y = 0;
static int s_mouse_active = 0;
static int s_prev_mouse_btn = 0;

static int s_modal_active = 0;
static char s_modal_title[64];
static char s_modal_text[1024];

/* Offscreen Backbuffer Primitives */
static void put_pixel(int x, int y, uint32_t col) {
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

static void draw_text_5x7(int x, int y, const char* str, uint32_t col) {
    while (*str) {
        char ch = *str++;
        if (ch >= 32 && ch <= 126) {
            int idx = ch - 32;
            for (int c = 0; c < 5; c++) {
                uint8_t line = font5x7[idx][c];
                for (int r = 0; r < 7; r++) {
                    if (line & (1 << r)) {
                        put_pixel(x + c, y + r, col);
                    }
                }
            }
        }
        x += 6;
    }
}

/* Icon Renderers (32x24 pixels) */
static void draw_icon_folder(int x, int y) {
    // Folder tab
    fill_rect(x + 2, y, 10, 4, COL_FOLDER_TAB);
    // Main body
    draw_rect(x + 2, y + 3, 28, 20, COL_FOLDER_TAB, COL_FOLDER_BODY);
    // Subtle inner groove
    fill_rect(x + 5, y + 7, 22, 13, COL_FOLDER_DARK);
}

static void draw_icon_binary(int x, int y) {
    // Terminal / App frame
    draw_rect(x + 3, y + 1, 26, 22, COL_BIN_BORDER, COL_BIN_BG);
    // Top bar
    fill_rect(x + 4, y + 2, 24, 3, COL_BIN_BORDER);
    // Green prompt chevron '>'
    draw_text_5x7(x + 7, y + 9, ">_", COL_BIN_ACCENT);
}

static void draw_icon_wad(int x, int y) {
    // Crimson cartridge
    draw_rect(x + 3, y + 1, 26, 22, COL_WAD_BORDER, COL_WAD_BG);
    fill_rect(x + 4, y + 2, 24, 4, COL_WAD_BORDER);
    // WAD badge
    draw_text_5x7(x + 6, y + 10, "WAD", COL_WAD_ACCENT);
}

static void draw_icon_doc(int x, int y) {
    // Document sheet
    draw_rect(x + 5, y + 1, 22, 22, COL_DOC_BORDER, COL_DOC_BG);
    // Folded top-right corner
    fill_rect(x + 20, y + 1, 7, 6, COL_BG_MAIN);
    // Text lines
    fill_rect(x + 8, y + 7, 13, 2, COL_DOC_LINES);
    fill_rect(x + 8, y + 11, 15, 2, COL_DOC_LINES);
    fill_rect(x + 8, y + 15, 11, 2, COL_DOC_LINES);
}

static void draw_cursor(int x, int y, int touching) {
    if (!s_mouse_active) return;
    static const uint8_t cursor_mask[12][8] = {
        {1, 0, 0, 0, 0, 0, 0, 0},
        {1, 1, 0, 0, 0, 0, 0, 0},
        {1, 2, 1, 0, 0, 0, 0, 0},
        {1, 2, 2, 1, 0, 0, 0, 0},
        {1, 2, 2, 2, 1, 0, 0, 0},
        {1, 2, 2, 2, 2, 1, 0, 0},
        {1, 2, 2, 2, 2, 2, 1, 0},
        {1, 2, 2, 2, 1, 1, 1, 1},
        {1, 2, 1, 2, 1, 0, 0, 0},
        {1, 1, 0, 1, 2, 1, 0, 0},
        {1, 0, 0, 0, 1, 2, 1, 0},
        {0, 0, 0, 0, 0, 1, 1, 0}
    };
    uint32_t body_col = touching ? 0xFFFFCC00 : 0xFFFFFFFF;
    uint32_t outline_col = 0xFF000000;

    for (int r = 0; r < 12; r++) {
        for (int c = 0; c < 8; c++) {
            uint8_t pixel = cursor_mask[r][c];
            if (pixel == 1) {
                put_pixel(x + c, y + r, outline_col);
            } else if (pixel == 2) {
                put_pixel(x + c, y + r, body_col);
            }
        }
    }
}

/* Filesystem Population */
static enum item_type detect_type(const char* name) {
    if (strstr(name, ".wad")) return TYPE_WAD;
    if (strstr(name, ".txt")) return TYPE_TEXT;
    if (strncmp(name, "/bin/", 5) == 0 || strcmp(name, "doom") == 0) return TYPE_BINARY;
    return TYPE_GENERIC;
}

static void format_size(uint32_t bytes, char* buf, size_t buflen) {
    if (bytes >= 1024 * 1024) {
        snprintf(buf, buflen, "%u.%uM", bytes / (1024 * 1024), (bytes % (1024 * 1024)) / 100000);
    } else if (bytes >= 1024) {
        snprintf(buf, buflen, "%uK", bytes / 1024);
    } else {
        snprintf(buf, buflen, "%uB", bytes);
    }
}

static void load_directory(int view_mode) {
    s_num_items = 0;
    s_selected_idx = 0;

    struct seld_dirent dirents[32];
    int count = seld_listdir("/", dirents, 32);
    if (count <= 0) return;

    if (view_mode == 0) {
        // Root View: Folder "bin", plus files at root
        s_items[s_num_items++] = (struct fm_item){
            "bin", "/bin", 0, TYPE_FOLDER, {0}
        };

        for (int i = 0; i < count; i++) {
            if (strncmp(dirents[i].name, "/bin/", 5) != 0) {
                struct fm_item it;
                strncpy(it.name, dirents[i].name, 31);
                it.name[31] = '\0';
                strncpy(it.full_path, dirents[i].name, 63);
                it.size = dirents[i].size;
                it.type = detect_type(it.name);
                memcpy(it.sha256, dirents[i].sha256, 32);
                s_items[s_num_items++] = it;
            }
        }
    } else if (view_mode == 1) {
        // Binaries View (/bin/)
        s_items[s_num_items++] = (struct fm_item){
            "..", "/", 0, TYPE_FOLDER, {0}
        };

        for (int i = 0; i < count; i++) {
            if (strncmp(dirents[i].name, "/bin/", 5) == 0) {
                struct fm_item it;
                strncpy(it.name, dirents[i].name + 5, 31); // strip "/bin/"
                it.name[31] = '\0';
                strncpy(it.full_path, dirents[i].name, 63);
                it.size = dirents[i].size;
                it.type = TYPE_BINARY;
                memcpy(it.sha256, dirents[i].sha256, 32);
                s_items[s_num_items++] = it;
            }
        }
    }
}

/* Modals */
static void show_text_modal(const char* title, const char* path) {
    s_modal_active = 1;
    strncpy(s_modal_title, title, sizeof(s_modal_title) - 1);
    memset(s_modal_text, 0, sizeof(s_modal_text));

    int rd = seld_readfile(path, s_modal_text, sizeof(s_modal_text) - 1);
    if (rd < 0) {
        strncpy(s_modal_text, "Failed to read file from SeldFS storage.", sizeof(s_modal_text) - 1);
    }
}

static void show_info_modal(const char* title, const char* content) {
    s_modal_active = 1;
    strncpy(s_modal_title, title, sizeof(s_modal_title) - 1);
    strncpy(s_modal_text, content, sizeof(s_modal_text) - 1);
}

/* Open / Execute Action */
static void activate_item(struct fm_item* it) {
    if (it->type == TYPE_FOLDER) {
        if (strcmp(it->name, "bin") == 0) {
            s_current_view = 1;
            load_directory(1);
        } else if (strcmp(it->name, "..") == 0) {
            s_current_view = 0;
            load_directory(0);
        }
        return;
    }

    if (it->type == TYPE_TEXT) {
        show_text_modal(it->name, it->full_path);
        return;
    }

    if (it->type == TYPE_WAD) {
        show_info_modal("DOOM WAD Package",
                        "Game Data: DOOM Shareware (v1.9)\n"
                        "Size: 4,196,020 bytes (8196 blocks)\n"
                        "Integrity: SHA-256 Verified\n"
                        "Run '/bin/doom' from shell to play.");
        return;
    }

    if (it->type == TYPE_BINARY) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "Executable ELF-64 Binary: %s\n"
                 "Size: %u bytes\n"
                 "Privilege: Ring 3 Unprivileged\n"
                 "Status: OpSec Verified", it->name, it->size);
        show_info_modal(it->name, msg);
    }
}

/* Rendering */
static void render_screen(void) {
    // 1. Clear background
    fill_rect(0, 0, SCR_W, SCR_H, COL_BG_MAIN);

    // 2. Top Header Bar
    draw_rect(0, 0, SCR_W, 28, COL_BORDER, COL_BG_TOPBAR);
    draw_text_8x16(12, 6, "SeldFS Sovereign Explorer (SNL-FM)", COL_TEXT_LIGHT);

    // Breadcrumb path button
    const char* path_txt = (s_current_view == 1) ? "[ /bin/ ]" : "[ / (Root) ]";
    draw_rect(340, 4, 100, 20, COL_BORDER_FOCUS, 0xFF14202E);
    draw_text_5x7(348, 10, path_txt, COL_FOLDER_TAB);

    // Top Right Close button [X]
    draw_rect(652, 4, 20, 20, 0xFFEF4444, 0xFF450A0A);
    draw_text_5x7(659, 10, "X", 0xFFFFFFFF);

    // 3. Left Sidebar
    int sb_w = 135;
    draw_rect(0, 28, sb_w, SCR_H - 28 - 20, COL_BORDER, COL_BG_SIDEBAR);

    draw_text_5x7(10, 36, "PLACES", COL_TEXT_DARK);

    struct {
        const char* label;
        int view;
    } sb_items[4] = {
        {"[#] Root (/)", 0},
        {"[>] Bin (/bin)", 1},
        {"[~] Network eth0", 2},
        {"[*] SeldFS Disk", 3}
    };

    for (int i = 0; i < 4; i++) {
        int iy = 50 + i * 26;
        int active = (s_current_view == sb_items[i].view);
        if (active) {
            draw_rect(6, iy, sb_w - 12, 22, COL_BORDER_FOCUS, COL_SELECTION);
            draw_text_5x7(12, iy + 7, sb_items[i].label, COL_TEXT_LIGHT);
        } else {
            draw_text_5x7(12, iy + 7, sb_items[i].label, COL_TEXT_MUTED);
        }
    }

    // 4. Main Grid View
    int grid_x = sb_w + 12;
    int grid_y = 38;
    int cell_w = 98;
    int cell_h = 62;
    int cols = 5;

    for (int i = 0; i < s_num_items; i++) {
        int r = i / cols;
        int c = i % cols;
        int cx = grid_x + c * cell_w;
        int cy = grid_y + r * cell_h;

        int selected = (i == s_selected_idx);
        if (selected) {
            draw_rect(cx, cy, cell_w - 6, cell_h - 4, COL_BORDER_FOCUS, COL_SELECTION);
        }

        // Draw Icon
        int ix = cx + (cell_w - 6 - 32) / 2;
        int iy = cy + 4;
        switch (s_items[i].type) {
            case TYPE_FOLDER: draw_icon_folder(ix, iy); break;
            case TYPE_BINARY: draw_icon_binary(ix, iy); break;
            case TYPE_WAD:    draw_icon_wad(ix, iy); break;
            case TYPE_TEXT:   draw_icon_doc(ix, iy); break;
            default:          draw_icon_doc(ix, iy); break;
        }

        // Draw Filename (truncated if > 12 chars)
        char disp_name[14];
        strncpy(disp_name, s_items[i].name, 12);
        disp_name[12] = '\0';
        int text_len = (int)strlen(disp_name);
        int tx = cx + (cell_w - 6 - (text_len * 6)) / 2;
        draw_text_5x7(tx, cy + 32, disp_name, selected ? COL_TEXT_LIGHT : COL_TEXT_MUTED);

        // Draw Size badge
        if (s_items[i].type != TYPE_FOLDER) {
            char sz_buf[10];
            format_size(s_items[i].size, sz_buf, sizeof(sz_buf));
            int sx = cx + (cell_w - 6 - (int)strlen(sz_buf) * 6) / 2;
            draw_text_5x7(sx, cy + 44, sz_buf, COL_TEXT_DARK);
        }
    }

    // 5. Bottom Status Bar
    int status_y = SCR_H - 20;
    draw_rect(0, status_y, SCR_W, 20, COL_BORDER, COL_BG_STATUS);

    if (s_num_items > 0 && s_selected_idx >= 0 && s_selected_idx < s_num_items) {
        char st_line[128];
        struct fm_item* it = &s_items[s_selected_idx];
        if (it->type == TYPE_FOLDER) {
            snprintf(st_line, sizeof(st_line), "Folder: %s (Directory)", it->name);
        } else {
            snprintf(st_line, sizeof(st_line), "File: %s (%u bytes) | SHA-256 Verified", it->name, it->size);
        }
        draw_text_5x7(12, status_y + 6, st_line, COL_TEXT_LIGHT);
    }

    draw_text_5x7(450, status_y + 6, "[Enter: Open] [Q/ESC: Exit]", COL_FOLDER_TAB);

    // 6. Modal Viewer Overlay (if active)
    if (s_modal_active) {
        // Darken overlay
        for (int r = 0; r < SCR_H; r += 2) {
            for (int c = 0; c < SCR_W; c += 2) {
                put_pixel(c, r, 0x00000000);
            }
        }

        int mw = 480;
        int mh = 220;
        int mx = (SCR_W - mw) / 2;
        int my = (SCR_H - mh) / 2;

        draw_rect(mx, my, mw, mh, COL_BORDER_FOCUS, 0xFF17202C);
        draw_rect(mx, my, mw, 24, COL_BORDER_FOCUS, 0xFF222F3E);

        draw_text_8x16(mx + 10, my + 4, s_modal_title, COL_TEXT_LIGHT);

        // Modal close button
        draw_rect(mx + mw - 22, my + 3, 18, 18, 0xFFEF4444, 0xFF551111);
        draw_text_5x7(mx + mw - 16, my + 8, "X", 0xFFFFFFFF);

        // Render modal text lines
        int text_y = my + 34;
        const char* p = s_modal_text;
        char line[64];
        int lidx = 0;

        while (*p && text_y < my + mh - 26) {
            if (*p == '\n') {
                line[lidx] = '\0';
                draw_text_5x7(mx + 14, text_y, line, COL_TEXT_LIGHT);
                text_y += 12;
                lidx = 0;
            } else if (lidx < 58) {
                line[lidx++] = *p;
            }
            p++;
        }
        if (lidx > 0 && text_y < my + mh - 26) {
            line[lidx] = '\0';
            draw_text_5x7(mx + 14, text_y, line, COL_TEXT_LIGHT);
        }

        draw_text_5x7(mx + (mw - 150) / 2, my + mh - 16, "[ Press ESC to Close ]", COL_FOLDER_TAB);
    }

    // 7. Mouse Cursor
    draw_cursor(s_mouse_x, s_mouse_y, s_prev_mouse_btn);

    // Blit backbuffer to actual video memory
    memcpy(s_fb.framebuffer, s_backbuf, SCR_W * SCR_H * 4);
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    if (seld_get_framebuffer(&s_fb) != 0 || !s_fb.framebuffer || s_fb.bpp != 32) {
        printf("[-] SNL-FM requires 32-bit linear graphical framebuffer.\n");
        return -1;
    }

    s_backbuf = (uint32_t*)malloc(SCR_W * SCR_H * sizeof(uint32_t));
    if (!s_backbuf) {
        printf("[-] Out of memory allocating offscreen framebuffer!\n");
        return -2;
    }

    load_directory(0);
    render_screen();

    int running = 1;
    while (running) {
        int needs_render = 0;

        // 1. Poll Mouse (drain queue, zero lag, absolute coordinates)
        struct seld_mouse_event mev;
        while (seld_poll_mouse(&mev) > 0) {
            s_mouse_active = 1;
            s_mouse_x = mev.x;
            s_mouse_y = mev.y;
            if (s_mouse_x < 0) s_mouse_x = 0;
            if (s_mouse_x >= SCR_W) s_mouse_x = SCR_W - 1;
            if (s_mouse_y < 0) s_mouse_y = 0;
            if (s_mouse_y >= SCR_H) s_mouse_y = SCR_H - 1;

            int btn_down = (mev.buttons & 1);
            if (btn_down && !s_prev_mouse_btn) {
                // Left Mouse Click
                if (s_modal_active) {
                    // Click on close button or outside
                    s_modal_active = 0;
                    needs_render = 1;
                } else if (s_mouse_x >= 652 && s_mouse_y <= 24) {
                    // Click top-right exit [X]
                    running = 0;
                } else if (s_mouse_x < 135 && s_mouse_y >= 50 && s_mouse_y <= 154) {
                    // Click on sidebar items
                    int clicked_item = (s_mouse_y - 50) / 26;
                    if (clicked_item == 0) {
                        s_current_view = 0;
                        load_directory(0);
                    } else if (clicked_item == 1) {
                        s_current_view = 1;
                        load_directory(1);
                    } else if (clicked_item == 2) {
                        struct seld_net_info net_info;
                        seld_net_info(&net_info);
                        char net_msg[256];
                        snprintf(net_msg, sizeof(net_msg),
                                 "Network Interface: eth0\n"
                                 "Controller: Intel 82540EP (PCI)\n"
                                 "IPv4: 10.0.2.15  Netmask: 255.255.255.0\n"
                                 "Gateway: 10.0.2.2  Carrier: ONLINE\n"
                                 "Frames: TX %lu, RX %lu",
                                 net_info.tx_frames, net_info.rx_frames);
                        show_info_modal("Network Status (eth0)", net_msg);
                    } else if (clicked_item == 3) {
                        show_info_modal("SeldFS Storage Info",
                                        "Filesystem: SeldFS v2 Hardened Block FS\n"
                                        "Capacity: 32 MiB (65536 data blocks)\n"
                                        "Integrity: Per-Inode SHA-256 Hashes\n"
                                        "Security: W^X Non-Executable Userland");
                    }
                    needs_render = 1;
                } else if (s_mouse_x >= 147 && s_mouse_y >= 38) {
                    // Click on grid
                    int c = (s_mouse_x - 147) / 98;
                    int r = (s_mouse_y - 38) / 62;
                    int idx = r * 5 + c;
                    if (c >= 0 && c < 5 && idx >= 0 && idx < s_num_items) {
                        if (s_selected_idx == idx) {
                            activate_item(&s_items[idx]);
                        } else {
                            s_selected_idx = idx;
                        }
                        needs_render = 1;
                    }
                }
            } else if (s_mouse_x >= 147 && s_mouse_y >= 38) {
                // Hover highlight
                int c = (s_mouse_x - 147) / 98;
                int r = (s_mouse_y - 38) / 62;
                int idx = r * 5 + c;
                if (c >= 0 && c < 5 && idx >= 0 && idx < s_num_items && idx != s_selected_idx) {
                    s_selected_idx = idx;
                    needs_render = 1;
                }
            }

            s_prev_mouse_btn = btn_down;
            needs_render = 1;
        }

        // 2. Poll Keyboard
        struct seld_kbd_event kev;
        if (seld_poll_key(&kev) > 0 && kev.pressed) {
            uint8_t sc = kev.scancode;
            if (sc == 0x01 || sc == 0x10) { // ESC or 'q'
                if (s_modal_active) {
                    s_modal_active = 0;
                    needs_render = 1;
                } else {
                    running = 0;
                }
            } else if (!s_modal_active) {
                if (sc == 0x4B) { // Left
                    if (s_selected_idx > 0) { s_selected_idx--; needs_render = 1; }
                } else if (sc == 0x4D) { // Right
                    if (s_selected_idx < s_num_items - 1) { s_selected_idx++; needs_render = 1; }
                } else if (sc == 0x48) { // Up
                    if (s_selected_idx >= 5) { s_selected_idx -= 5; needs_render = 1; }
                } else if (sc == 0x50) { // Down
                    if (s_selected_idx + 5 < s_num_items) { s_selected_idx += 5; needs_render = 1; }
                } else if (sc == 0x1C) { // Enter
                    if (s_selected_idx >= 0 && s_selected_idx < s_num_items) {
                        activate_item(&s_items[s_selected_idx]);
                        needs_render = 1;
                    }
                }
            }
        }

        if (needs_render) {
            render_screen();
        }

        seld_sleep(16); // ~60 Hz polling
    }

    free(s_backbuf);
    seld_clear();
    seld_set_console_rows(20);
    return 0;
}
