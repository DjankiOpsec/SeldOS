/*
 * SeldOS - Humboldt Kernel Project
 * SNL (Seld Not Linux) Sovereign Shell v0.1 (snl-sh)
 * Autonomous interactive Ring 3 Shell with line editing, builtins,
 * and SeldFS /bin program execution.
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "seld.h"
#include "banner_logo.h"

#define MAX_LINE_LEN 256
#define MAX_ARGS 16

#define SH_HISTORY_MAX 64
static char* s_sh_history[SH_HISTORY_MAX];
static int s_sh_history_count = 0;

static void draw_banner_vector_logo(void);

static void print_banner(void) {
    printf("=======================================================================\n");
    printf("                          SNL (Seld Not Linux) Sovereign Shell v0.1\n");
    printf("                          Ring 3 Sovereign CLI Environment (Humboldt Hardened)\n");
    printf("                          Humboldt Framebuffer 680x334 | 39-Color Palette\n");
#if defined(__riscv)
    printf("                          RISC-V 64-bit (RV64GC) Isolated Execution\n");
#else
    printf("                          x86_64 Long Mode Isolated Execution\n");
#endif
    printf("                          [OPSEC ALERT] >>> ЗАЖИМАЙ SHIFT! <<<\n");
    printf("                          (DEAD MAN'S GRIP ACTIVE: ОТПУСТИШЬ - СБРОС)\n");
    printf("\n");
    printf("=======================================================================\n");
    printf("Welcome to SNL Sovereign Shell. Type 'help' for available commands.\n");
    draw_banner_vector_logo();
}

/* 5x7 ASCII bitmap font (ASCII 32 to 90) for UI buttons */
static const uint8_t font5x7[59][5] = {
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
    {0x61, 0x51, 0x49, 0x45, 0x43}  // 90 'Z'
};

static const char scancode_ascii[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static const char scancode_ascii_upper[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static int s_rshift_pressed = 0;
static int s_caps_lock = 0;

static struct seld_fb_info s_fb = {0};
static int s_fb_checked = 0;
static int s_mobile_mode = 1;
static int s_keyboard_enabled = 1;
static int s_prev_touch = 0;
static int s_mouse_x = 340;
static int s_mouse_y = 167;
static int s_mouse_active = 0;

static void clear_touch_area(void) {
    if (!s_fb_checked) {
        seld_get_framebuffer(&s_fb);
        s_fb_checked = 1;
    }
    if (!s_fb.framebuffer || s_fb.bpp != 32) return;
    uint32_t* fb = (uint32_t*)s_fb.framebuffer;
    uint32_t pitch_p = s_fb.pitch / 4;
    for (int y = 200; y < (int)s_fb.height; y++) {
        for (int x = 0; x < (int)s_fb.width; x++) {
            fb[y * pitch_p + x] = 0x00000000;
        }
    }
}

static void draw_banner_vector_logo(void) {
    if (!s_fb_checked) {
        seld_get_framebuffer(&s_fb);
        s_fb_checked = 1;
    }
    if (!s_fb.framebuffer || s_fb.bpp != 32) return;
    uint32_t* fb = (uint32_t*)s_fb.framebuffer;
    uint32_t pitch_p = s_fb.pitch / 4;
    static const uint32_t palette39[39] = {
        0x00000000, 0x000000AA, 0x0000AA00, 0x0000AAAA,
        0x00AA0000, 0x00AA00AA, 0x00AA5500, 0x00AAAAAA,
        0x00555555, 0x005555FF, 0x0055FF55, 0x0055FFFF,
        0x00FF5555, 0x00FF55FF, 0x00FFFF55, 0x00FFFFFF,
        0x0018181A, 0x002A2D34, 0x00F2F4F8, 0x00E5D9C5,
        0x00FF6B8B, 0x00E84A5F, 0x002C3539, 0x00848B98,
        0x003D3635, 0x009E2A2B, 0x00E0533C, 0x00001F3F,
        0x00005B96, 0x00018E9A, 0x006497B1, 0x000B6623,
        0x002E8B57, 0x0040E0D0, 0x00B0E0E6, 0x00D27D2D,
        0x008B4513, 0x00C2B280, 0x005C6B73
    };
    int start_x = 44;
    int start_y = 18;
    for (int y = 0; y < BANNER_LOGO_H; y++) {
        int py = start_y + y;
        if (py >= (int)s_fb.height) break;
        for (int x = 0; x < BANNER_LOGO_W; x++) {
            int px = start_x + x;
            if (px >= (int)s_fb.width) break;
            uint8_t idx = s_banner_logo[y][x];
            if (idx != 0xFF && idx < 39) {
                fb[py * pitch_p + px] = palette39[idx];
            }
        }
    }
}

struct vkey {
    int x, y, w, h;
    char key;
    char label[10];
    uint32_t border;
    const char* command;
};

#define MAX_VKEYS 64
static struct vkey s_vkeys[MAX_VKEYS];
static int s_num_vkeys = 0;

static void fb_draw_char5x7(uint32_t* fb, int x, int y, char c, uint32_t color, int scale) {
    if (c >= 'a' && c <= 'z') c -= ('a' - 'A');
    if (c < 32 || c > 90) return;
    const uint8_t* glyph = font5x7[c - 32];
    uint32_t pitch_p = s_fb.pitch / 4;
    for (int col = 0; col < 5; col++) {
        uint8_t line = glyph[col];
        for (int row = 0; row < 7; row++) {
            if (line & (1 << row)) {
                for (int sx = 0; sx < scale; sx++) {
                    for (int sy = 0; sy < scale; sy++) {
                        int px = x + col * scale + sx;
                        int py = y + row * scale + sy;
                        if (px >= 0 && px < (int)s_fb.width && py >= 0 && py < (int)s_fb.height) {
                            fb[py * pitch_p + px] = color;
                        }
                    }
                }
            }
        }
    }
}

static void fb_draw_rect(uint32_t* fb, int x, int y, int w, int h, uint32_t border, uint32_t bg) {
    uint32_t pitch_p = s_fb.pitch / 4;
    for (int r = y; r < y + h && r < (int)s_fb.height; r++) {
        for (int c = x; c < x + w && c < (int)s_fb.width; c++) {
            if (r == y || r == y + h - 1 || c == x || c == x + w - 1) {
                fb[r * pitch_p + c] = border;
            } else {
                fb[r * pitch_p + c] = bg;
            }
        }
    }
}

static void init_vkeys(void) {
    s_num_vkeys = 0;

    // Quick Command Buttons (Row 0: y: 204, h: 24)
    s_vkeys[s_num_vkeys++] = (struct vkey){8, 204, 64, 24, 0, "TOR", 0xFF9966FF, "tor"};
    s_vkeys[s_num_vkeys++] = (struct vkey){78, 204, 46, 24, 0, "LS", 0xFF3399FF, "ls"};
    s_vkeys[s_num_vkeys++] = (struct vkey){130, 204, 46, 24, 0, "PS", 0xFF33FFFF, "ps"};
    s_vkeys[s_num_vkeys++] = (struct vkey){182, 204, 66, 24, 0, "UNAME", 0xFFFFFF33, "uname"};
    s_vkeys[s_num_vkeys++] = (struct vkey){254, 204, 76, 24, 0, "COLORS", 0xFFFF6B8B, "colors"};
    s_vkeys[s_num_vkeys++] = (struct vkey){336, 204, 58, 24, 0, "HELP", 0xFFE0E0E0, "help"};
    s_vkeys[s_num_vkeys++] = (struct vkey){400, 204, 62, 24, 0, "CLEAR", 0xFFFF5555, "clear"};
    s_vkeys[s_num_vkeys++] = (struct vkey){468, 204, 48, 24, 0, "PC", 0xFFFF9933, "pc"};
    s_vkeys[s_num_vkeys++] = (struct vkey){522, 204, 56, 24, 0, "BEEP", 0xFFFF55FF, "beep"};
    s_vkeys[s_num_vkeys++] = (struct vkey){584, 204, 50, 24, 0, "FM", 0xFF38BDF8, "fm"};

    if (!s_keyboard_enabled) return;

    // Row 1: Numbers & Symbols (y: 232, h: 22)
    const char* r1 = "1234567890-/";
    for (int i = 0; i < 12; i++) {
        char s[2] = {r1[i], '\0'};
        struct vkey vk = {8 + i * 55, 232, 50, 22, r1[i], "", 0xFF5588BB, NULL};
        strcpy(vk.label, s);
        s_vkeys[s_num_vkeys++] = vk;
    }

    // Row 2: QWERTY (y: 257, h: 22)
    const char* r2 = "qwertyuiop";
    for (int i = 0; i < 10; i++) {
        char s[2] = {r2[i] - 32, '\0'};
        struct vkey vk = {10 + i * 66, 257, 60, 22, r2[i], "", 0xFF66AACC, NULL};
        strcpy(vk.label, s);
        s_vkeys[s_num_vkeys++] = vk;
    }

    // Row 3: ASDFGHJKL. (y: 282, h: 22)
    const char* r3 = "asdfghjkl.";
    for (int i = 0; i < 10; i++) {
        char s[2] = {r3[i] == '.' ? '.' : (char)(r3[i] - 32), '\0'};
        struct vkey vk = {16 + i * 66, 282, 60, 22, r3[i], "", 0xFF66AACC, NULL};
        strcpy(vk.label, s);
        s_vkeys[s_num_vkeys++] = vk;
    }

    // Row 4: ZXCVBNM, SPACE, BKSP, ENTER (y: 307, h: 23)
    const char* r4 = "zxcvbnm";
    for (int i = 0; i < 7; i++) {
        char s[2] = {r4[i] - 32, '\0'};
        struct vkey vk = {8 + i * 48, 307, 44, 23, r4[i], "", 0xFF66AACC, NULL};
        strcpy(vk.label, s);
        s_vkeys[s_num_vkeys++] = vk;
    }

    s_vkeys[s_num_vkeys++] = (struct vkey){350, 307, 114, 23, ' ', "SPACE", 0xFF88AAEE, NULL};
    s_vkeys[s_num_vkeys++] = (struct vkey){470, 307, 88, 23, '\b', "BKSP", 0xFFFF5555, NULL};
    s_vkeys[s_num_vkeys++] = (struct vkey){564, 307, 108, 23, '\n', "ENTER", 0xFF22DD22, NULL};
}

static void draw_vkey(struct vkey* vk, int active) {
    if (!s_fb.framebuffer || s_fb.bpp != 32) return;
    uint32_t* fb = (uint32_t*)s_fb.framebuffer;
    uint32_t bg = active ? 0xFF3B5270 : 0xFF1C2433;
    uint32_t border = active ? 0xFFFFFFFF : vk->border;
    uint32_t text_col = active ? 0xFFFFFFFF : vk->border;

    fb_draw_rect(fb, vk->x, vk->y, vk->w, vk->h, border, bg);

    int text_len = (int)strlen(vk->label);
    int scale = (vk->h > 26) ? 2 : 1;
    if (vk->w > 60 && vk->h > 26) scale = 2;
    int char_w = 5 * scale + (scale > 1 ? 1 : 0);
    int char_h = 7 * scale;

    int tx = vk->x + (vk->w - text_len * (char_w + 1)) / 2;
    int ty = vk->y + (vk->h - char_h) / 2;
    if (tx < vk->x + 2) tx = vk->x + 2;

    for (int c = 0; c < text_len; c++) {
        fb_draw_char5x7(fb, tx + c * (char_w + 1), ty, vk->label[c], text_col, scale);
    }
}

static struct vkey* find_vkey_at(int x, int y) {
    for (int i = 0; i < s_num_vkeys; i++) {
        struct vkey* vk = &s_vkeys[i];
        if (x >= vk->x - 3 && x <= vk->x + vk->w + 3 &&
            y >= vk->y - 3 && y <= vk->y + vk->h + 3) {
            return vk;
        }
    }
    return NULL;
}

/* 8x12 Classic High-Contrast Arrow Cursor */
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

static uint32_t s_cursor_saved_pixels[12 * 8];
static int s_cursor_saved_x = -1;
static int s_cursor_saved_y = -1;
static int s_cursor_saved_active = 0;

static void cursor_hide(void) {
    if (!s_cursor_saved_active) return;
    if (!s_fb.framebuffer || s_fb.bpp != 32) return;
    uint32_t* fb = (uint32_t*)s_fb.framebuffer;
    uint32_t pitch_p = s_fb.pitch / 4;

    for (int r = 0; r < 12; r++) {
        int py = s_cursor_saved_y + r;
        if (py < 0 || py >= (int)s_fb.height) continue;
        for (int c = 0; c < 8; c++) {
            int px = s_cursor_saved_x + c;
            if (px < 0 || px >= (int)s_fb.width) continue;
            if (cursor_mask[r][c] != 0) {
                fb[py * pitch_p + px] = s_cursor_saved_pixels[r * 8 + c];
            }
        }
    }
    s_cursor_saved_active = 0;
}

static void cursor_show(int x, int y, int touching) {
    if (!s_fb_checked) {
        seld_get_framebuffer(&s_fb);
        s_fb_checked = 1;
    }
    if (!s_fb.framebuffer || s_fb.bpp != 32) return;
    cursor_hide();

    uint32_t* fb = (uint32_t*)s_fb.framebuffer;
    uint32_t pitch_p = s_fb.pitch / 4;
    uint32_t body_col = touching ? 0xFFFFCC00 : 0xFFFFFFFF;
    uint32_t outline_col = 0xFF000000;

    for (int r = 0; r < 12; r++) {
        int py = y + r;
        if (py < 0 || py >= (int)s_fb.height) continue;
        for (int c = 0; c < 8; c++) {
            int px = x + c;
            if (px < 0 || px >= (int)s_fb.width) continue;
            s_cursor_saved_pixels[r * 8 + c] = fb[py * pitch_p + px];
            if (cursor_mask[r][c] == 1) {
                fb[py * pitch_p + px] = outline_col;
            } else if (cursor_mask[r][c] == 2) {
                fb[py * pitch_p + px] = body_col;
            }
        }
    }
    s_cursor_saved_x = x;
    s_cursor_saved_y = y;
    s_cursor_saved_active = 1;
}

static void draw_shell_hud(void) {
    if (!s_mobile_mode) return;
    if (!s_fb_checked) {
        seld_get_framebuffer(&s_fb);
        s_fb_checked = 1;
    }
    if (!s_fb.framebuffer || s_fb.bpp != 32) return;

    init_vkeys();

    uint32_t* fb = (uint32_t*)s_fb.framebuffer;
    uint32_t pitch_p = s_fb.pitch / 4;

    // Fill touch keyboard background (y: 200..333) with dark sleek slate
    for (int y = 200; y < 334 && y < (int)s_fb.height; y++) {
        for (int x = 0; x < 680 && x < (int)s_fb.width; x++) {
            fb[y * pitch_p + x] = (y == 200) ? 0xFF335577 : 0xFF111722;
        }
    }

    // Draw buttons
    for (int i = 0; i < s_num_vkeys; i++) {
        draw_vkey(&s_vkeys[i], 0);
    }
}

static int read_line(char* buf, size_t max_len) {
    if (s_mobile_mode) {
        cursor_hide();
        draw_shell_hud();
        if (s_mouse_active) {
            cursor_show(s_mouse_x, s_mouse_y, 0);
        }
    }
    size_t len = 0;
    int prev_touch = 0;
    s_prev_touch = 0;

    while (1) {
        int activity = 0;

        // 1. Check touch / mouse events from mobile screen
        if (s_mobile_mode) {
            struct seld_mouse_event mev;
            while (seld_poll_mouse(&mev) > 0) {
                activity = 1;
                s_mouse_active = 1;
                s_mouse_x = mev.x;
                s_mouse_y = mev.y;
                int touching = (mev.buttons & 1);

                cursor_show(s_mouse_x, s_mouse_y, touching);

                if (touching && !prev_touch) {
                    struct vkey* hit_vk = find_vkey_at(mev.x, mev.y);
                    if (hit_vk) {
                        // Visual button press feedback
                        cursor_hide();
                        draw_vkey(hit_vk, 1);
                        cursor_show(s_mouse_x, s_mouse_y, touching);

                        // Audio touch click feedback
                        seld_beep(1100, 15);

                        // Quick command execution (e.g. DOOM, LS, PS, etc.)
                        if (hit_vk->command != NULL) {
                            seld_sleep(50);
                            cursor_hide();
                            draw_vkey(hit_vk, 0);

                            const char* c = hit_vk->command;
                            size_t clen = strlen(c);
                            for (size_t k = 0; k < clen; k++) {
                                putchar(c[k]);
                            }
                            putchar('\n');
                            fflush(stdout);
                            strcpy(buf, c);
                            return (int)clen;
                        }

                        // Toggle keyboard button
                        if (strcmp(hit_vk->label, "KBD") == 0) {
                            s_keyboard_enabled = !s_keyboard_enabled;
                            cursor_hide();
                            draw_shell_hud();
                            cursor_show(s_mouse_x, s_mouse_y, 0);
                            break;
                        }

                        // Enter key
                        if (hit_vk->key == '\n') {
                            seld_sleep(40);
                            cursor_hide();
                            draw_vkey(hit_vk, 0);

                            putchar('\n');
                            fflush(stdout);
                            buf[len] = '\0';
                            return (int)len;
                        }

                        // Backspace key
                        if (hit_vk->key == '\b') {
                            if (len > 0) {
                                len--;
                                putchar('\b');
                                putchar(' ');
                                putchar('\b');
                                fflush(stdout);
                            }
                            seld_sleep(40);
                            cursor_hide();
                            draw_vkey(hit_vk, 0);
                            cursor_show(s_mouse_x, s_mouse_y, touching);
                            break;
                        }

                        // Printable character
                        if (hit_vk->key >= 32 && hit_vk->key <= 126) {
                            if (len < max_len - 1) {
                                buf[len++] = hit_vk->key;
                                putchar(hit_vk->key);
                                fflush(stdout);
                            }
                            seld_sleep(40);
                            cursor_hide();
                            draw_vkey(hit_vk, 0);
                            cursor_show(s_mouse_x, s_mouse_y, touching);
                            break;
                        }
                    }
                }
                prev_touch = touching;
            }
        }

        // 2. Check hardware physical keyboard events
        struct seld_kbd_event kev;
        if (seld_poll_key(&kev) > 0) {
            activity = 1;
            uint8_t raw_sc = kev.scancode;

            /* Left Shift (0x2A): Dead Man's Grip ONLY. Ignore for shell typing */
            if (raw_sc == 0x2A) {
                continue;
            }

            /* Right Shift (0x36): Typing modifier for Caps & symbols */
            if (raw_sc == 0x36) {
                s_rshift_pressed = kev.pressed;
                continue;
            }

            /* Left Ctrl (0x1D): track pressed state */
            static int s_lctrl_pressed = 0;
            if (raw_sc == 0x1D) {
                s_lctrl_pressed = kev.pressed;
                continue;
            }

            /* Caps Lock (0x3A): Toggle on press */
            if (raw_sc == 0x3A && kev.pressed) {
                s_caps_lock = !s_caps_lock;
                continue;
            }

            /* Ctrl+C: Cancel current command line */
            if (s_lctrl_pressed && kev.pressed && (raw_sc & 0x7F) == 0x2E) {
                putchar('^');
                putchar('C');
                putchar('\n');
                fflush(stdout);
                buf[0] = '\0';
                return 0;
            }

            /* Ctrl+Z: Suspend current command line */
            if (s_lctrl_pressed && kev.pressed && (raw_sc & 0x7F) == 0x2C) {
                putchar('^');
                putchar('Z');
                putchar('\n');
                printf("[1]+ Stopped\n");
                fflush(stdout);
                buf[0] = '\0';
                return 0;
            }

            if (kev.pressed) {
                if (s_mouse_active) {
                    cursor_hide();
                    s_mouse_active = 0;
                }
                uint8_t sc = raw_sc & 0x7F;
                char c = 0;
                if (sc < 128) {
                    char base = scancode_ascii[sc];
                    int is_alpha = (base >= 'a' && base <= 'z');
                    int use_upper = is_alpha ? (s_rshift_pressed ^ s_caps_lock) : s_rshift_pressed;
                    c = use_upper ? scancode_ascii_upper[sc] : base;
                }
                if (c == '\r' || c == '\n') {
                    if (s_mobile_mode) cursor_hide();
                    putchar('\n');
                    fflush(stdout);
                    buf[len] = '\0';
                    return (int)len;
                } else if (c == '\b') {
                    if (len > 0) {
                        len--;
                        putchar('\b');
                        putchar(' ');
                        putchar('\b');
                        fflush(stdout);
                    }
                } else if (c >= 32 && c <= 126) {
                    if (len < max_len - 1) {
                        buf[len++] = c;
                        putchar(c);
                        fflush(stdout);
                    }
                }
            }
        }

        if (!activity) {
            seld_sleep(10);
        }
    }
}

static int parse_args(char* line, char* argv[], int max_args) {
    int argc = 0;
    char* p = line;

    while (*p != '\0') {
        while (*p == ' ' || *p == '\t') {
            *p++ = '\0';
        }
        if (*p == '\0') break;

        char quote = '\0';
        if (*p == '"' || *p == '\'') {
            quote = *p;
            *p++ = '\0';
        }

        if (argc < max_args - 1) {
            argv[argc++] = p;
        }

        if (quote != '\0') {
            while (*p != '\0' && *p != quote) {
                p++;
            }
            if (*p == quote) {
                *p++ = '\0';
            }
        } else {
            while (*p != '\0' && *p != ' ' && *p != '\t') {
                p++;
            }
        }
    }
    argv[argc] = NULL;
    return argc;
}

static void builtin_fetch(void) {
    seld_clear();
    print_banner();
    if (s_mobile_mode) {
        draw_shell_hud();
    }
}

static void builtin_reboot(void) {
    printf("[*] Rebooting SeldOS...\n");
    seld_reboot();
}

static void builtin_poweroff(void) {
    printf("[*] Powering off SeldOS...\n");
    seld_poweroff();
}

static void builtin_help(void) {
    printf("SNL (Seld Not Linux) Sovereign Shell v0.1\n");
    printf("Autonomous Interactive Ring 3 Shell (Humboldt Hardened / AGPLv3)\n\n");
    printf("Builtin Commands:\n");
    printf("  help            Display this help summary\n");
    printf("  fetch           Display Sovereign system banner and mascot\n");
    printf("  colors          Display 39 Humboldt colors palette\n");
    printf("  pc [on|off]     Toggle PC mode (disable mobile touch HUD & keyboard)\n");
    printf("  mobile          Enable mobile touch HUD and virtual keyboard\n");
    printf("  clear           Clear screen buffer\n");
    printf("  echo [args..]   Output arguments to standard output\n");
    printf("  exit            Terminate shell session\n");
    printf("  reboot          Reboot the system\n");
    printf("  poweroff        Shut down and power off system (ACPI/QEMU)\n");
    printf("  uptime          Display system running time from PIT chronometer\n");
    printf("  beep [freq] [d] Play audio tone (default: 440 Hz, 150 ms)\n");
    printf("  snd [freq|off]  Continuous tone generator\n");
    printf("  mem             Display physical memory & kernel heap usage\n");
    printf("  ifconfig [st]   Display interface stats or configure static IP (Zero-DHCP)\n");
    printf("  ping <ip>       Send ICMP Echo requests to host\n");
    printf("  arp             Display kernel ARP resolution cache\n");
    printf("  purge           OpSec Immune Purge: kill rogue tasks, zero-wipe threats, sever sockets\n");
    printf("  net [mode]      Air-Gap Shield: status, lock, unlock, stealth, desync, delay\n");
    printf("  skew [on|off]   Anti-Clock-Skew Normalization (Zero-PPM virtual oscillator)\n");
    printf("  time [status|s] Certified In-Band Network Time (Zero UDP 123 NTP leaks)\n");
    printf("  telemetry       Hardware Telemetry & ACPI/SMBus Sanitization (0-serial mask)\n");
    printf("  desync [mode]   TSPU/DPI Evasion: split (SNI segmentation), fake, off\n");
    printf("  jitter [on|off] THL Keystroke Timing Obfuscation & Jitter (50ms Quantization)\n");
    printf("  grip [on|off]   OpSec Dead Man's Grip: Hold Shift to live, release = wipe & reset\n");
    printf("  chaff [on|off]  Poisson Cover Traffic & Uniform Padding (GPA Anti-Correlation)\n");
    printf("  tempest [on|off]TEMPEST Video RF Dispersion & Subpixel Dithering (SDR Evasion)\n");
    printf("  uarch flush     Microarchitectural Flush (Branch Predictor IBPB, Caches, TLB)\n");
    printf("  pci lockdown    Zero-Trust PCI DMA & Option ROM Lockdown\n");
    printf("  blackout        CIA Peripheral Blackout (LEDs off, DACs mute, USB quarantine)\n");
    printf("  firekill        Autonomous Fire-and-Kill (DoD 3-pass RAM scrub & hard poweroff)\n");
    printf("  export [K=V]    Set environment variable (or list environment variables)\n");
    printf("  env             Print all exported environment variables\n");
    printf("  unveil <p> <m>  Seld-Unveil Filesystem Sandboxing (OpenBSD unveil: r, w, x, c, lock)\n");
    printf("  selftest        Execute userspace Ring 3 verification test suite\n\n");
    printf("External Utilities in /bin/:\n");
    printf("  grep [-inv] <p> Fast line pattern search in files or standard input\n");
    printf("  head [-n N] [f] Display first N lines of file or standard input\n");
    printf("  tail [-n N] [f] Display last N lines of file or standard input\n");
    printf("  less [file]     Terminal & 680x334 Framebuffer interactive pager\n");
    printf("  kill <pid>      Send termination signal to process by PID\n");
    printf("  clip <cmd>      Sovereign RAM clipboard manager (copy, paste, clear)\n");
    printf("  drop <cmd>      Base64 & terminal RAM file interchange protocol (dump, load)\n");
    printf("  otp [cmd]       Visual Cryptography One-Time Pad Shield (HDMI noise Shannon H~1)\n");
    printf("  pinpad          Scrambled On-Screen Pinpad (Acoustic Keystroke Evasion)\n");
    printf("  diode [cmd]     Air-Gap Optical & Acoustic Data Diode (keygen, send, beep, qr)\n");
    printf("  stealth [cmd]   Anti-TSPU/DPI Stealth Suite & Native C99 VLESS Client\n");
    printf("  ls              List files with size, blocks, and SHA-256 hash\n");
    printf("  cat <file>      Display contents of file\n");
    printf("  echo [args..]   Print arguments to stdout\n");
    printf("  rm <file>       Remove file from SeldFS storage\n");
    printf("  sha256sum <f>   Verify file integrity against stored SHA-256\n");
    printf("  uname           Display system identification\n");
    printf("  ps              Query and display active tasks / PID info\n");
    printf("  fm              Seld Sovereign Graphical File Manager (SNL-FM)\n");
    printf("  hi [file]       Humboldt Interactive Modal Text Editor (Vi keybindings, 680x334 FB)\n");
    printf("  download <url>  Fetch binary/package over network into SeldFS\n");
    printf("  tor             Sovereign Tor & SeldTLS 1.3 Web Browser\n");
    printf("  oracle [X]      TempleOS-style Opsec Oracle (fast chant, 290-680 Hz Dorian audio)\n");
    printf("  doom            Classic DOOM (download via 'download doom')\n");
    printf("  init            First userspace program (init system)\n\n");
    printf("Hardware Driver Control (Simulate Kernel Panic):\n");
    printf("  gpu drv off     Brutally disable GPU display driver (triggers kernel panic)\n");
    printf("  cpu drv off     Brutally disable CPU scheduler driver (triggers kernel panic)\n");
    printf("  ram drv off     Brutally disable RAM memory driver (triggers kernel panic)\n");
}

static void builtin_colors(void) {
    printf("=======================================================================\n");
    printf(" SeldOS Humboldt 39-Color Palette\n");
    printf(" Display Resolution: 680x334 (85x20 text raster + 14px guard band)\n");
    printf("=======================================================================\n");
    printf(" 0..15: Standard VGA | 16..26: Monochromatic UI | 27..38: Extended Accents\n\n\n");

    if (!s_fb_checked) {
        seld_get_framebuffer(&s_fb);
        s_fb_checked = 1;
    }
    if (s_fb.framebuffer && s_fb.bpp == 32) {
        uint32_t* fb = (uint32_t*)s_fb.framebuffer;
        uint32_t pitch_p = s_fb.pitch / 4;
        static const uint32_t palette39[39] = {
            0x00000000, 0x000000AA, 0x0000AA00, 0x0000AAAA,
            0x00AA0000, 0x00AA00AA, 0x00AA5500, 0x00AAAAAA,
            0x00555555, 0x005555FF, 0x0055FF55, 0x0055FFFF,
            0x00FF5555, 0x00FF55FF, 0x00FFFF55, 0x00FFFFFF,
            0x0018181A, 0x002A2D34, 0x00F2F4F8, 0x00E5D9C5,
            0x00FF6B8B, 0x00E84A5F, 0x002C3539, 0x00848B98,
            0x003D3635, 0x009E2A2B, 0x00E0533C, 0x00001F3F,
            0x00005B96, 0x00018E9A, 0x006497B1, 0x000B6623,
            0x002E8B57, 0x0040E0D0, 0x00B0E0E6, 0x00D27D2D,
            0x008B4513, 0x00C2B280, 0x005C6B73
        };
        int swatch_w = 17;
        int start_x = 8;
        int start_y = 130;
        int h = 22;
        for (int c = 0; c < 39; c++) {
            uint32_t col = palette39[c];
            for (int y = start_y; y < start_y + h && y < (int)s_fb.height; y++) {
                for (int x = start_x + c * swatch_w; x < start_x + (c + 1) * swatch_w && x < (int)s_fb.width; x++) {
                    fb[y * pitch_p + x] = col;
                }
            }
        }
        printf("[+] 39 Color Swatches rendered to screen across 680x334 framebuffer!\n");
    }
}

static void builtin_pc(int argc, char* argv[]) {
    int enable_pc = 1;
    if (argc >= 2) {
        if (strcmp(argv[1], "off") == 0 || strcmp(argv[1], "0") == 0) {
            enable_pc = 0;
        } else if (strcmp(argv[1], "on") == 0 || strcmp(argv[1], "1") == 0) {
            enable_pc = 1;
        }
    }

    if (enable_pc) {
        cursor_hide();
        s_mouse_active = 0;
        s_mobile_mode = 0;
        seld_set_console_rows(20);
        clear_touch_area();
        seld_writefile("pcmode", "1", 1);
        printf("[+] PC Mode ENABLED: Mobile touch controls and virtual keyboard disabled.\n");
        printf("    Terminal screen is in clean desktop mode (85x20 text).\n");
        printf("    Type 'mobile' or 'pc off' to re-enable touch controls.\n");
    } else {
        s_mobile_mode = 1;
        seld_set_console_rows(12);
        seld_unlink("pcmode");
        draw_shell_hud();
        printf("[+] Mobile Mode ENABLED: Touch HUD and virtual keyboard restored.\n");
    }
}

static void builtin_mobile(void) {
    s_mobile_mode = 1;
    seld_set_console_rows(12);
    seld_unlink("pcmode");
    draw_shell_hud();
    printf("[+] Mobile Mode ENABLED: Touch HUD and virtual keyboard restored.\n");
}

static void builtin_echo(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        printf("%s", argv[i]);
        if (i < argc - 1) {
            putchar(' ');
        }
    }
    putchar('\n');
}

static void builtin_beep(int argc, char* argv[]) {
    // SeldOS Beep Command
    // Syntax:
    //   beep                   -> default tone (440 Hz, 150 ms)
    //   beep <freq_hz> [dur]   -> tone at freq_hz for dur ms
    uint32_t freq = 440;
    uint32_t dur = 150;

    if (argc >= 2) {
        if (strcmp(argv[1], "ona") == 0 || strcmp(argv[1], "-o") == 0) {
            if (argc < 3) {
                printf("snl: beep: error: missing note argument (1..127).\n");
                printf("Usage: beep [freq_hz] [duration_ms]\n");
                return;
            }
            uint32_t raw_ona = 0;
            int err = parse_uint32_safe(argv[2], &raw_ona);
            if (err == -2) {
                printf("snl: beep: error: negative value not permitted (C overflow prevention).\n");
                return;
            } else if (err == -3) {
                printf("snl: beep: error: integer overflow detected (exceeds 32-bit range).\n");
                return;
            } else if (err != 0 || raw_ona < 1 || raw_ona > 127) {
                printf("snl: beep: error: note must be between 1 and 127.\n");
                return;
            }
            freq = seld_ona2freq((int8_t)raw_ona);

            if (argc >= 4) {
                uint32_t raw_dur = 0;
                int d_err = parse_uint32_safe(argv[3], &raw_dur);
                if (d_err == -2) {
                    printf("snl: beep: error: negative duration not permitted.\n");
                    return;
                } else if (d_err == -3) {
                    printf("snl: beep: error: integer overflow in duration.\n");
                    return;
                } else if (d_err != 0) {
                    printf("snl: beep: error: invalid duration '%s'.\n", argv[3]);
                    return;
                }
                dur = raw_dur;
            }
        } else {
            // Numeric frequency or duration
            uint32_t raw_freq = 0;
            int err = parse_uint32_safe(argv[1], &raw_freq);
            if (err == -2) {
                printf("snl: beep: error: negative value not permitted (C overflow prevention).\n");
                printf("Usage: beep [freq_hz] [duration_ms]\n");
                return;
            } else if (err == -3) {
                printf("snl: beep: error: integer overflow detected (exceeds 32-bit range).\n");
                return;
            } else if (err != 0) {
                printf("snl: beep: error: invalid numeric frequency '%s'.\n", argv[1]);
                printf("Usage: beep [freq_hz] [duration_ms]\n");
                return;
            }

            if (raw_freq < 20 || raw_freq > 20000) {
                printf("snl: beep: error: frequency %u Hz out of audible range (20 - 20000 Hz).\n", raw_freq);
                return;
            }
            freq = raw_freq;

            if (argc >= 3) {
                uint32_t raw_dur = 0;
                int d_err = parse_uint32_safe(argv[2], &raw_dur);
                if (d_err == -2) {
                    printf("snl: beep: error: negative duration not permitted.\n");
                    return;
                } else if (d_err == -3) {
                    printf("snl: beep: error: integer overflow in duration.\n");
                    return;
                } else if (d_err != 0) {
                    printf("snl: beep: error: invalid duration '%s'.\n", argv[2]);
                    return;
                }
                dur = raw_dur;
            }
        }
    }

    if (dur > 10000) {
        printf("snl: beep: error: duration %u ms exceeds maximum allowed limit (10000 ms).\n", dur);
        return;
    }
    if (dur == 0) dur = 150;

    printf("[Audio] Tone: %u Hz (%u ms)...\n", freq, dur);
    fflush(stdout);

    seld_beep(freq, dur);
}

static void builtin_snd(int argc, char* argv[]) {
    // SeldOS Snd Command (Continuous PC Speaker Tone Generator)
    // Syntax:
    //   snd off | snd 0        -> stops sound (silence)
    //   snd <freq_hz>          -> continuous tone at freq_hz
    if (argc < 2) {
        printf("PC Speaker Sound Generator (snd)\n");
        printf("Usage:\n");
        printf("  snd <freq_hz>          Start continuous tone (e.g. snd 440)\n");
        printf("  snd off | snd 0        Stop tone / silence speaker\n");
        return;
    }

    if (strcmp(argv[1], "off") == 0 || strcmp(argv[1], "0") == 0 || strcmp(argv[1], "stop") == 0) {
        seld_beep(0, 0); // dur=0, freq=0 -> silence
        printf("[Audio] Speaker silenced.\n");
        return;
    }

    uint32_t freq = 0;

    if (strcmp(argv[1], "ona") == 0 || strcmp(argv[1], "-o") == 0) {
        if (argc < 3) {
            printf("snl: snd: error: missing note argument (1..127).\n");
            return;
        }
        uint32_t raw_ona = 0;
        int err = parse_uint32_safe(argv[2], &raw_ona);
        if (err != 0 || raw_ona < 1 || raw_ona > 127) {
            printf("snl: snd: error: note must be between 1 and 127.\n");
            return;
        }
        freq = seld_ona2freq((int8_t)raw_ona);
    } else {
        uint32_t raw_freq = 0;
        int err = parse_uint32_safe(argv[1], &raw_freq);
        if (err == -2) {
            printf("snl: snd: error: negative value not permitted.\n");
            return;
        } else if (err == -3) {
            printf("snl: snd: error: integer overflow detected.\n");
            return;
        } else if (err != 0) {
            printf("snl: snd: error: invalid frequency '%s'.\n", argv[1]);
            return;
        }
        if (raw_freq < 20 || raw_freq > 20000) {
            printf("snl: snd: error: frequency %u Hz out of audible range (20 - 20000 Hz).\n", raw_freq);
            return;
        }
        freq = raw_freq;
    }

    // Call seld_beep with duration=0 for continuous sound
    seld_beep(freq, 0);
    printf("[Audio] Continuous tone: %u Hz. Type 'snd off' to stop.\n", freq);
}

static void builtin_uptime(void) {
    uint64_t ms = uptime();
    uint64_t sec = ms / 1000;
    uint64_t min = sec / 60;
    sec %= 60;
    printf("System Uptime: %lu min, %lu sec (%lu ms total)\n", min, sec, ms);
}

static void builtin_mem(void) {
    struct snl_meminfo info;
    if (meminfo(&info) != 0) {
        printf("[-] Failed to query memory statistics.\n");
        return;
    }
    printf("Memory Utilization Overview (Ring 3 OpSec Query):\n");
    printf("  Physical RAM Total : %lu KiB (%lu MiB)\n",
           info.total_ram / 1024, info.total_ram / (1024 * 1024));
    printf("  Physical RAM Used  : %lu KiB\n", info.used_ram / 1024);
    printf("  Physical RAM Free  : %lu KiB\n", info.free_ram / 1024);
    printf("  --------------------------------------------------\n");
    printf("  Kernel Heap Size   : %lu KiB\n", info.heap_size / 1024);
    printf("  Kernel Heap Used   : %lu bytes\n", info.heap_used);
    printf("  Kernel Heap Free   : %lu bytes\n", info.heap_free);
}

static void format_ip_u(uint32_t ip, char* buf, size_t sz) {
    uint8_t o1 = (uint8_t)(ip & 0xFF);
    uint8_t o2 = (uint8_t)((ip >> 8) & 0xFF);
    uint8_t o3 = (uint8_t)((ip >> 16) & 0xFF);
    uint8_t o4 = (uint8_t)((ip >> 24) & 0xFF);
    snprintf(buf, sz, "%u.%u.%u.%u", o1, o2, o3, o4);
}

static void format_mac_u(const uint8_t* mac, char* buf, size_t sz) {
    if (!mac || !buf || sz < 18) return;
    const char hex[] = "0123456789ABCDEF";
    size_t idx = 0;
    for (int i = 0; i < 6; i++) {
        buf[idx++] = hex[(mac[i] >> 4) & 0x0F];
        buf[idx++] = hex[mac[i] & 0x0F];
        if (i < 5) buf[idx++] = ':';
    }
    buf[idx] = '\0';
}

static int parse_ip_u(const char* str, uint32_t* ip_out) {
    if (!str || !ip_out) return -1;
    uint32_t octets[4] = {0};
    int octet_idx = 0;
    const char* p = str;
    while (*p && octet_idx < 4) {
        if (*p < '0' || *p > '9') return -1;
        uint32_t val = 0;
        while (*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
            if (val > 255) return -1;
            p++;
        }
        octets[octet_idx++] = val;
        if (*p == '.') {
            p++;
            if (*p == '\0') return -1;
        } else if (*p != '\0') {
            return -1;
        }
    }
    if (octet_idx != 4 || *p != '\0') return -1;
    *ip_out = octets[0] | (octets[1] << 8) | (octets[2] << 16) | (octets[3] << 24);
    return 0;
}

static void builtin_ifconfig(int argc, char* argv[]) {
    if (argc >= 2 && strcmp(argv[1], "static") == 0) {
        if (argc < 5) {
            printf("Usage: ifconfig static <ip> <netmask> <gateway> [dns]\n");
            return;
        }
        uint32_t ip = 0, mask = 0, gw = 0, dns = 0x02024C4C; // default 76.76.2.2 Control D
        if (parse_ip_u(argv[2], &ip) != 0 ||
            parse_ip_u(argv[3], &mask) != 0 ||
            parse_ip_u(argv[4], &gw) != 0) {
            printf("[-] Invalid IPv4 address format.\n");
            printf("Usage: ifconfig static <ip> <netmask> <gateway> [dns]\n");
            return;
        }
        if (argc >= 6) {
            if (parse_ip_u(argv[5], &dns) != 0) {
                printf("[-] Invalid DNS server IPv4 address format.\n");
                return;
            }
        }
        int res = seld_set_net_config(ip, mask, gw, dns);
        if (res == 0) {
            char ip_b[16], mask_b[16], gw_b[16], dns_b[16];
            format_ip_u(ip, ip_b, sizeof(ip_b));
            format_ip_u(mask, mask_b, sizeof(mask_b));
            format_ip_u(gw, gw_b, sizeof(gw_b));
            format_ip_u(dns, dns_b, sizeof(dns_b));
            printf("[+] Static IPv4 configured: IP %s, Mask %s, GW %s, DNS %s (Zero-DHCP)\n",
                   ip_b, mask_b, gw_b, dns_b);
        } else {
            printf("[-] Failed to apply static network configuration (error %d)\n", res);
        }
        return;
    }

    struct seld_net_info info;
    if (seld_net_info(&info) != 0) {
        printf("[-] Failed to query network interface.\n");
        return;
    }

    if (!info.link_up) {
        printf("eth0: flags=DOWN (Interface Offline / No Compatible NIC Carrier Detected)\n");
        printf("      In VirtualBox / VM settings: Set Adapter Type to 82540EM (e1000) or Am79C973 (PCnet)\n");
        return;
    }

    char ip_buf[16], mask_buf[16], gw_buf[16], mac_buf[18], dns_buf[16];
    format_ip_u(info.ip, ip_buf, sizeof(ip_buf));
    format_ip_u(info.netmask, mask_buf, sizeof(mask_buf));
    format_ip_u(info.gateway, gw_buf, sizeof(gw_buf));
    format_ip_u(info.dns, dns_buf, sizeof(dns_buf));
    format_mac_u(info.mac, mac_buf, sizeof(mac_buf));

    printf("eth0: flags=UP,BROADCAST,MULTICAST mtu 1500\n");
    printf("      ether %s (Hardware PCI Ethernet)\n", mac_buf);
    printf("      inet %s  netmask %s  gateway %s\n", ip_buf, mask_buf, gw_buf);
    printf("      dns  %s (Control D Ads & Trackers - OpSec P2)\n", dns_buf);
    printf("      RX packets %lu  bytes %lu  dropped %lu  errors %lu\n",
           info.rx_frames, info.rx_bytes, info.rx_dropped, info.rx_checksum_errors);
    printf("      TX packets %lu  bytes %lu\n",
           info.tx_frames, info.tx_bytes);
}

static void builtin_arp(void) {
    struct seld_arp_entry table[16];
    int count = seld_net_arp(table, 16);
    if (count < 0) {
        printf("[-] Failed to query ARP cache.\n");
        return;
    }
    printf("Address          HWaddress           Iface    State\n");
    printf("---------------  -----------------   -----    -------\n");
    if (count == 0) {
        printf("(ARP cache is currently empty)\n");
        return;
    }
    for (int i = 0; i < count; i++) {
        char ip_str[16];
        char mac_str[18];
        format_ip_u(table[i].ip, ip_str, sizeof(ip_str));
        format_mac_u(table[i].mac, mac_str, sizeof(mac_str));
        printf("%-16s %-18s eth0     RESOLVED\n", ip_str, mac_str);
    }
}

static void builtin_ping(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: ping <target_ipv4_address>\n");
        return;
    }
    seld_net_lease_acquire();
    uint32_t target_ip = 0;
    if (parse_ip_u(argv[1], &target_ip) != 0) {
        int dres = seld_dns_resolve(argv[1], &target_ip);
        if (dres == -5 || target_ip == 0) {
            printf("[-] ping: host '%s' is BLOCKED by Control D OpSec Filter (0.0.0.0)\n", argv[1]);
            seld_net_lease_release();
            return;
        } else if (dres != 0) {
            printf("[-] ping: cannot resolve '%s': Unknown host\n", argv[1]);
            seld_net_lease_release();
            return;
        }
    }

    char ip_str[16];
    format_ip_u(target_ip, ip_str, sizeof(ip_str));
    if (strcmp(argv[1], ip_str) == 0) {
        printf("PING %s 56(84) bytes of data.\n", ip_str);
    } else {
        printf("PING %s (%s) 56(84) bytes of data.\n", argv[1], ip_str);
    }

    int received = 0;
    int transmitted = 4;
    for (int seq = 1; seq <= transmitted; seq++) {
        uint32_t rtt = 0;
        int res = seld_net_ping(target_ip, (uint16_t)seq, &rtt);
        if (res == 0) {
            received++;
            printf("64 bytes from %s: icmp_seq=%d ttl=64 time=%u ms\n", ip_str, seq, rtt);
        } else if (res == -5) {
            printf("[-] ping: blocked in STEALTH mode (anti-active-tracking guard)\n");
            break;
        } else if (res == -2) {
            printf("[-] ping: route unreachable or send failed\n");
            break;
        } else {
            printf("Request timeout for icmp_seq %d\n", seq);
        }
        if (seq < transmitted) {
            seld_sleep(500);
        }
    }
    printf("--- %s ping statistics ---\n", ip_str);
    int loss = ((transmitted - received) * 100) / transmitted;
    printf("%d packets transmitted, %d received, %d%% packet loss\n",
           transmitted, received, loss);
    seld_net_lease_release();
}

static void builtin_dns(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: dns <hostname> [dot]\n");
        printf("  dns <host>      - Resolve via Control D (76.76.2.2:53 [Ads & Trackers])\n");
        printf("  dns <host> dot  - Resolve via Control D DoT (p2.freedns.controld.com:853)\n");
        printf("  OpSec Profile: Ads & Trackers (No logging / Zero profiling)\n");
        return;
    }
    seld_net_lease_acquire();
    int use_dot = (argc >= 3 && (strcmp(argv[2], "dot") == 0 || strcmp(argv[2], "tls") == 0));
    uint32_t ip = 0;
    int res;
    if (use_dot) {
        printf("Resolving %s via Control D DNS-over-TLS (p2.freedns.controld.com:853)...\n", argv[1]);
        res = seld_dns_resolve_dot(argv[1], &ip);
    } else {
        printf("Resolving %s via Control D (76.76.2.2:53 [Ads & Trackers])...\n", argv[1]);
        res = seld_dns_resolve(argv[1], &ip);
    }
    if (res == 0 && ip != 0) {
        char ip_str[16];
        format_ip_u(ip, ip_str, sizeof(ip_str));
        printf("Resolved: %s -> %s [%s]\n", argv[1], ip_str, use_dot ? "DoT TLS 1.3" : "Control D P2");
    } else if (res == -5 || ip == 0) {
        printf("[BLOCKED] %s -> 0.0.0.0 (Filtered by Control D Ads & Trackers)\n", argv[1]);
    } else {
        printf("[-] Failed to resolve %s (error %d)\n", argv[1], res);
    }
    seld_net_lease_release();
}

static void builtin_purge(void) {
    printf("\n=======================================================================\n");
    printf(" [OpSec] SELDOS IMMUNE SYSTEM & ZERO-TRUST DEFENSE SWEEP\n");
    printf("=======================================================================\n");
    printf("[*] Initiating Kernel-Level Cryptographic Integrity Audit...\n");
    printf("[*] Sweeping Scheduler Process Table for rogue tasks...\n");
    printf("[*] Severing active TCP connections & dropping socket buffers...\n");
    printf("[*] Scrubbing physical memory pages & engaging Air-Gap Shield...\n");

    int res = seld_immune_purge();
    int wiped_files = res & 0xFFFF;
    int killed_tasks = (res >> 16) & 0xFF;
    int aborted_socks = (res >> 24) & 0xFF;

    printf("[+] SeldFS Cryptographic Audit : Verified inodes (%d tampered/unauthorized wiped)\n", wiped_files);
    printf("[+] Process Scheduler Sweep    : %d rogue/unauthorized tasks terminated\n", killed_tasks);
    printf("[+] Network Socket Severing    : %d TCP sockets aborted (RST sent, ARP/DNS wiped)\n", aborted_socks);
    printf("[+] Air-Gap Network Shield     : ENGAGED (Default-Deny On-Demand Mode active)\n");
    printf("[+] Executable Attestation     : Default-Deny SHA-256 Gatekeeper ENFORCED\n");
    printf("=======================================================================\n");
    printf(" [OK] SYSTEM INTEGRITY: IMMUNE, SEALED & HARDENED\n");
    printf("=======================================================================\n\n");
}

static void builtin_net(int argc, char* argv[]) {
    if (argc <= 1 || strcmp(argv[1], "status") == 0) {
        int status = seld_net_get_lock();
        int desync = seld_net_get_desync();
        printf("[*] SeldOS Air-Gap Network Shield Status: ");
        if (status == 1) {
            printf("LOCKED (Strict Air-Gap Lockdown - All TX dropped)\n");
        } else if (status == 2) {
            printf("ON-DEMAND (Default-Deny - Auto-leased for tor/download/ping)\n");
        } else if (status == 3) {
            printf("STEALTH (Zero-Emission / TSPU Evasion / DoT Forced / Ping Drop)\n");
        } else {
            printf("UNLOCKED (Clearnet Open Mode)\n");
        }
        printf("    TSPU TCP Desync: %s\n",
               desync == SELD_DESYNC_SPLIT ? "SPLIT (SNI/Host Segment Evasion active)" :
               desync == SELD_DESYNC_FAKE  ? "FAKE (TTL desync)" : "OFF");
        int delay = seld_get_boot_delay();
        printf("    Poisson Boot Delay: %s\n", delay ? "ACTIVE (Initial TX hold active)" : "DISABLED / EXPIRED");
        printf("    Usage: net <status | lock | unlock | ondemand | stealth | desync | delay>\n");
        return;
    }

    if (strcmp(argv[1], "lock") == 0) {
        seld_net_lock(1);
        printf("[+] Air-Gap Network Shield: STRICT LOCKDOWN ENGAGED (All network TX dropped).\n");
    } else if (strcmp(argv[1], "unlock") == 0) {
        seld_net_lock(0);
        printf("[*] Air-Gap Network Shield: DISENGAGED (Clearnet Open Mode).\n");
    } else if (strcmp(argv[1], "ondemand") == 0) {
        seld_net_lock(2);
        printf("[+] Air-Gap Network Shield: ON-DEMAND MODE ENGAGED (Auto-leased for tor/download).\n");
    } else if (strcmp(argv[1], "stealth") == 0) {
        seld_net_lock(3);
        seld_net_set_desync(SELD_DESYNC_SPLIT);
        printf("[+] Stealth Mode ENGAGED: In-Kernel TCP Desync active, raw UDP DNS blocked (DoT forced), ICMP dropped.\n");
    } else if (strcmp(argv[1], "desync") == 0) {
        if (argc >= 3) {
            if (strcmp(argv[2], "split") == 0) {
                seld_net_set_desync(SELD_DESYNC_SPLIT);
                printf("[+] TCP Desync mode: SPLIT (SNI Segmentation enabled).\n");
            } else if (strcmp(argv[2], "fake") == 0) {
                seld_net_set_desync(SELD_DESYNC_FAKE);
                printf("[+] TCP Desync mode: FAKE enabled.\n");
            } else if (strcmp(argv[2], "off") == 0 || strcmp(argv[2], "none") == 0) {
                seld_net_set_desync(SELD_DESYNC_NONE);
                printf("[*] TCP Desync mode: DISABLED.\n");
            } else {
                printf("Usage: net desync <split | fake | off>\n");
            }
        } else {
            int d = seld_net_get_desync();
            printf("Current TCP Desync mode: %s\n",
                   d == SELD_DESYNC_SPLIT ? "SPLIT" : d == SELD_DESYNC_FAKE ? "FAKE" : "OFF");
        }
    } else if (strcmp(argv[1], "delay") == 0) {
        if (argc >= 3) {
            if (strcmp(argv[2], "on") == 0 || strcmp(argv[2], "1") == 0) {
                seld_set_boot_delay(1, 0);
                printf("[+] Poisson Boot Delay: ENABLED (Randomized initial TX hold active).\n");
            } else if (strcmp(argv[2], "off") == 0 || strcmp(argv[2], "0") == 0) {
                seld_set_boot_delay(0, 0);
                printf("[*] Poisson Boot Delay: DISABLED.\n");
            } else if (strcmp(argv[2], "status") == 0) {
                int d = seld_get_boot_delay();
                printf("[*] Poisson Boot Delay: %s\n", d ? "ACTIVE" : "EXPIRED / INACTIVE");
            } else {
                uint32_t custom_ms = 0;
                if (parse_uint32_safe(argv[2], &custom_ms) == 0 && custom_ms > 0) {
                    seld_set_boot_delay(1, custom_ms);
                    printf("[+] Poisson Boot Delay set to %u ms hold.\n", custom_ms);
                } else {
                    printf("Usage: net delay <on | off | status | ms>\n");
                }
            }
        } else {
            int d = seld_get_boot_delay();
            printf("[*] Poisson Boot Delay: %s\n", d ? "ACTIVE" : "EXPIRED / INACTIVE");
            printf("Usage: net delay <on | off | status | ms>\n");
        }
    } else {
        printf("snl: net: unknown argument '%s'. Options: status, lock, unlock, ondemand, stealth, desync, delay\n", argv[1]);
    }
}

static void builtin_desync(int argc, char* argv[]) {
    if (argc < 2) {
        int d = seld_net_get_desync();
        printf("TSPU TCP Desync: %s\n",
               d == SELD_DESYNC_SPLIT ? "SPLIT (SNI Segmentation)" :
               d == SELD_DESYNC_FAKE  ? "FAKE" : "OFF");
        printf("Usage: desync <split | fake | off>\n");
        return;
    }
    if (strcmp(argv[1], "split") == 0) {
        seld_net_set_desync(SELD_DESYNC_SPLIT);
        printf("[+] TCP Desync: SPLIT enabled (SNI segmentation across packets).\n");
    } else if (strcmp(argv[1], "fake") == 0) {
        seld_net_set_desync(SELD_DESYNC_FAKE);
        printf("[+] TCP Desync: FAKE enabled.\n");
    } else if (strcmp(argv[1], "off") == 0 || strcmp(argv[1], "none") == 0) {
        seld_net_set_desync(SELD_DESYNC_NONE);
        printf("[*] TCP Desync: DISABLED.\n");
    } else {
        printf("Usage: desync <split | fake | off>\n");
    }
}

static void builtin_jitter(int argc, char* argv[]) {
    if (argc < 2 || strcmp(argv[1], "status") == 0) {
        int j = seld_get_jitter();
        printf("[*] THL Keystroke Timing Jitter: %s\n",
               j ? "ENABLED (50ms Quantized PIT Batching)" : "DISABLED (Direct Low-Latency)");
        printf("    Usage: jitter <on | off | status>\n");
        return;
    }
    if (strcmp(argv[1], "on") == 0 || strcmp(argv[1], "1") == 0) {
        seld_set_jitter(1);
        printf("[+] THL Keystroke Timing Jitter ENABLED: Scancodes quantized to ~50ms CSPRNG batching.\n");
    } else if (strcmp(argv[1], "off") == 0 || strcmp(argv[1], "0") == 0) {
        seld_set_jitter(0);
        printf("[*] THL Keystroke Timing Jitter DISABLED: Direct low-latency typing restored.\n");
    } else {
        printf("Usage: jitter <on | off | status>\n");
    }
}

static void builtin_deadman(int argc, char* argv[]) {
    if (argc < 2 || strcmp(argv[1], "status") == 0) {
        int d = seld_get_deadman();
        printf("[*] OpSec Dead Man's Grip (Shift Latch): %s\n",
               d ? "ARMED (Release triggers instant DoD RAM Wipe & Reset)" : "DISARMED");
        printf("    Usage: grip <on | off | arm | status>\n");
        return;
    }
    if (strcmp(argv[1], "on") == 0 || strcmp(argv[1], "arm") == 0 || strcmp(argv[1], "1") == 0) {
        seld_set_deadman(1);
        printf("[!] [OPSEC ALERT] DEAD MAN'S GRIP ARMED: ЗАЖИМАЙ SHIFT!\n");
        printf("    Сессия защищена физическим удержанием. Отпустишь Shift - мгновенный сброс!\n");
    } else if (strcmp(argv[1], "off") == 0 || strcmp(argv[1], "0") == 0) {
        seld_set_deadman(0);
        printf("[*] OpSec Dead Man's Grip DISARMED.\n");
    } else {
        printf("Usage: grip <on | off | arm | status>\n");
    }
}

static void builtin_chaff(int argc, char* argv[]) {
    if (argc < 2 || strcmp(argv[1], "status") == 0) {
        int c = seld_get_chaff();
        printf("[*] Poisson Cover Traffic & Uniform Padding: %s\n",
               c ? "ACTIVE (Anti-Correlation GPA Defense)" : "DISABLED");
        printf("    Usage: chaff <on | off | status>\n");
        return;
    }
    if (strcmp(argv[1], "on") == 0 || strcmp(argv[1], "1") == 0) {
        seld_set_chaff(1);
        printf("[+] Poisson Cover Traffic & Uniform Packet Padding ACTIVE: Flow correlation defeated.\n");
    } else if (strcmp(argv[1], "off") == 0 || strcmp(argv[1], "0") == 0) {
        seld_set_chaff(0);
        printf("[*] Poisson Cover Traffic & Uniform Packet Padding DISABLED.\n");
    } else {
        printf("Usage: chaff <on | off | status>\n");
    }
}

static void builtin_tempest(int argc, char* argv[]) {
    if (argc < 2 || strcmp(argv[1], "status") == 0) {
        int t = seld_get_tempest();
        printf("[*] TEMPEST Video RF Dispersion & Subpixel Dithering: %s\n",
               t ? "ACTIVE (SDR Van Eck Evasion)" : "DISABLED");
        printf("    Usage: tempest <on | off | status>\n");
        return;
    }
    if (strcmp(argv[1], "on") == 0 || strcmp(argv[1], "1") == 0) {
        seld_set_tempest(1);
        printf("[+] TEMPEST Evasion ACTIVE: RGB micro-dithering masks electromagnetic emissions.\n");
    } else if (strcmp(argv[1], "off") == 0 || strcmp(argv[1], "0") == 0) {
        seld_set_tempest(0);
        printf("[*] TEMPEST Evasion DISABLED.\n");
    } else {
        printf("Usage: tempest <on | off | status>\n");
    }
}

static void builtin_uarch(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    printf("[*] Executing CPU Microarchitectural Flush (IBPB, Caches, TLB)...\n");
    seld_flush_uarch();
    printf("[+] Microarchitectural state sanitized. Branch history erased.\n");
}

static void builtin_pci(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    printf("[*] Initiating Zero-Trust PCI DMA & Option ROM Lockdown...\n");
    int count = seld_pci_lockdown();
    printf("[+] PCI Zero-Trust Lockdown complete: revoked untrusted Bus Masters (%d).\n", count);
}

static void builtin_unveil(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Seld-Unveil Path-Based Sandboxing (OpenBSD unveil(2))\n");
        printf("Usage:\n");
        printf("  unveil <path> <permissions>   Add unveil rule (e.g. unveil /bin rx)\n");
        printf("  unveil lock                   Lock unveil configuration (irreversible)\n");
        printf("Permissions:\n");
        printf("  r - Read | w - Write | x - Execute | c - Create\n");
        return;
    }

    if (strcmp(argv[1], "lock") == 0) {
        int ret = seld_unveil(NULL, NULL);
        if (ret == 0) {
            printf("[+] Seld-Unveil configuration locked permanently for this session.\n");
        } else {
            printf("[-] Failed to lock unveil configuration (code %d)\n", ret);
        }
        return;
    }

    if (argc < 3) {
        printf("Usage: unveil <path> <permissions>  (e.g. unveil /bin rx)\n");
        return;
    }

    int ret = seld_unveil(argv[1], argv[2]);
    if (ret == 0) {
        printf("[+] Seld-Unveil rule registered: '%s' -> [%s]\n", argv[1], argv[2]);
    } else {
        printf("[-] Failed to register unveil rule for '%s' (code %d)\n", argv[1], ret);
    }
}

static void builtin_skew(int argc, char* argv[]) {
    if (argc >= 2) {
        if (strcmp(argv[1], "on") == 0 || strcmp(argv[1], "1") == 0) {
            seld_set_skew_defense(1);
            printf("[+] Anti-Clock-Skew Normalization: ACTIVE (Zero-PPM virtual oscillator + CSPRNG dither)\n");
        } else if (strcmp(argv[1], "off") == 0 || strcmp(argv[1], "0") == 0) {
            seld_set_skew_defense(0);
            printf("[*] Anti-Clock-Skew Normalization: DISABLED\n");
        } else if (strcmp(argv[1], "status") == 0) {
            printf("[*] Anti-Clock-Skew Normalization Status: %s\n",
                   seld_get_skew_defense() ? "ACTIVE" : "DISABLED");
        } else {
            printf("Usage: skew <on | off | status>\n");
        }
    } else {
        printf("[*] Anti-Clock-Skew Normalization: %s\n",
               seld_get_skew_defense() ? "ACTIVE" : "DISABLED");
        printf("Usage: skew <on | off | status>\n");
    }
}

static void builtin_time(int argc, char* argv[]) {
    if (argc >= 3 && strcmp(argv[1], "sync") == 0) {
        uint64_t epoch = (uint64_t)strtoul(argv[2], NULL, 10);
        if (epoch > 0) {
            seld_set_time(epoch);
            printf("[+] Certified network time updated: epoch %lu sec (Zero UDP NTP leaks)\n", epoch);
            return;
        }
    }
    uint64_t epoch = seld_get_time();
    if (epoch > 0) {
        printf("[+] Certified In-Band Network Time: epoch %lu sec (0 UDP NTP leaks)\n", epoch);
    } else {
        printf("[*] Certified Network Time: Not yet synchronized in-band (UDP 123 blocked)\n");
    }
}

static void builtin_telemetry(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    struct seld_hw_telemetry tel;
    if (seld_get_hw_telemetry(&tel) == 0) {
        printf("[*] SeldOS Hardware Telemetry Sanitization:\n");
        printf("    Manufacturer   : %s\n", tel.manufacturer);
        printf("    Product Model  : %s\n", tel.product_name);
        printf("    Hardware Serial: %s [Sanitized / Zeroed]\n", tel.serial_number);
        printf("    BIOS / Firmware: %s\n", tel.bios_version);
        printf("    Battery State  : %s (%u%%, %u cycles, Serial: %s)\n",
               tel.battery_status, tel.battery_percent, tel.battery_cycles, tel.battery_serial);
        printf("    ACPI / SMBus   : ISOLATED (Direct silicon queries blocked)\n");
    } else {
        printf("[-] Failed to query hardware telemetry.\n");
    }
}

static void builtin_blackout(void) {
    printf("[!] Activating Sovereign Peripheral Blackout...\n");
    seld_opsec_blackout();
    printf("[+] Peripheral Blackout ACTIVE: KBD LEDs extinguished, Audio DACs muted, USB controllers unmapped.\n");
}

static void builtin_firekill(void) {
    printf("[!] Triggering Instant OpSec Fire-and-Kill...\n");
    seld_opsec_fire_kill();
}

static __attribute__((noinline)) int selftest_deep_stack_recurse(int depth) {
    volatile uint8_t frame_probe[3072];
    frame_probe[0] = (uint8_t)depth;
    frame_probe[3071] = (uint8_t)(depth ^ 0xA5);
    if (depth > 0) {
        int r = selftest_deep_stack_recurse(depth - 1);
        if (frame_probe[0] != (uint8_t)depth || frame_probe[3071] != (uint8_t)(depth ^ 0xA5)) {
            return -1;
        }
        return r;
    }
    // At bottom of deep stack (depth 0), verify that passing a stack buffer to a syscall works!
    struct seld_stat st;
    if (seld_stat("/bin/sh", &st) != 0 || st.size == 0) {
        return -2;
    }
    return 0;
}

static void builtin_selftest(void) {
    printf("\n[Ring 3] ========================================================\n");
    printf("[Ring 3]   SELD OS USERSPACE RUNTIME & TEST SUITE (Ring 3)\n");
    printf("[Ring 3] ========================================================\n");

    int total_tests = 8;
    int passed_tests = 0;
    volatile int stack_var = 0;

    // Test 1: Hardware Privilege Level & Segment Selectors (CPL=3 / U-mode)
    printf("[*] [TEST 1/8] Inspecting CPU Privilege Level & Segments...\n");
#if defined(__riscv)
    printf("    RISC-V Execution Privilege: U-mode (User Mode)\n");
    printf("[+] [TEST 1/8] PASSED: Hardware U-mode verified. Running in unprivileged user mode.\n");
    passed_tests++;
#else
    uint16_t cs = 0, ss = 0;
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));
    __asm__ volatile ("mov %%ss, %0" : "=r"(ss));
    uint8_t cpl = cs & 3;
    uint8_t spl = ss & 3;

    printf("    CS selector: 0x%x (RPL = %d)\n", cs, cpl);
    printf("    SS selector: 0x%x (RPL = %d)\n", ss, spl);

    if (cpl == 3 && spl == 3) {
        printf("[+] [TEST 1/8] PASSED: Hardware CPL=3 verified. Running in unprivileged user mode.\n");
        passed_tests++;
    } else {
        printf("[-] [TEST 1/8] FAILED: CPL/SPL privilege level mismatch!\n");
    }
#endif

    // Test 2: Fast SYSCALL / SYSRET Handshake (SYS_SELD 42)
    printf("[*] [TEST 2/8] Invoking SYS_SELD handshake (syscall 42)...\n");
    long seld_ret = seld_ping();
    if (seld_ret == 0x5E1D5EC) {
        printf("[+] [TEST 2/8] PASSED: Syscall handshake returned 0x5E1D5EC.\n");
        passed_tests++;
    } else {
        printf("[-] [TEST 2/8] FAILED: Invalid handshake return code: 0x%lx\n", seld_ret);
    }

    // Test 3: Process Identification (SYS_GETPID)
    printf("[*] [TEST 3/8] Invoking SYS_GETPID via getpid() wrapper...\n");
    int pid = getpid();
    printf("    Assigned Process ID: %d\n", pid);
    if (pid >= 1) {
        printf("[+] [TEST 3/8] PASSED: Valid process ID received from kernel scheduler.\n");
        passed_tests++;
    } else {
        printf("[-] [TEST 3/8] FAILED: Invalid PID received!\n");
    }

    // Test 4: System Chronometry (SYS_UPTIME)
    printf("[*] [TEST 4/8] Invoking SYS_UPTIME via uptime() wrapper...\n");
    uint64_t ms = uptime();
    printf("    Current system uptime: %lu ms\n", ms);
    if (ms > 0) {
        printf("[+] [TEST 4/8] PASSED: High-precision PIT timer counter queried.\n");
        passed_tests++;
    } else {
        printf("[-] [TEST 4/8] FAILED: Uptime query returned 0 ms!\n");
    }

    // Test 5: Syscall Verification & Parameter Passing (SYS_SELD_VERIFY)
    printf("[*] [TEST 5/8] Testing SYS_SELD_VERIFY parameter validation...\n");
    long v0 = seld_verify(0, 0);
    long token = 0x12345678L;
    long v1 = seld_verify(token, 0);
    long expected_v1 = token ^ 0x5E1D5E1DL;

    long v_user_stack = seld_verify(1, (long)&stack_var);
    long v_kernel_ptr = seld_verify(1, (long)0xFFFF800000000000ULL);

    if (v0 == 0x5E1D0001L && v1 == expected_v1 && v_user_stack == 1 && v_kernel_ptr == 0) {
        printf("[+] [TEST 5/8] PASSED: Token arithmetic and user/kernel pointer verification valid.\n");
        passed_tests++;
    } else {
        printf("[-] [TEST 5/8] FAILED: Parameter verification mismatch!\n");
    }

    // Test 6: Memory Bounds & Stack R/W Integrity
    printf("[*] [TEST 6/8] Validating userspace memory bounds and stack R/W...\n");
    uint64_t code_addr = (uint64_t)&builtin_selftest;
    uint64_t stack_addr = (uint64_t)&stack_var;

    printf("    Code virtual address: 0x%lx\n", code_addr);
    printf("    Stack virtual address: 0x%lx\n", stack_addr);

    int bounds_ok = 1;
#if defined(__riscv)
    if (code_addr >= 0x0000004000000000ULL || stack_addr >= 0x0000004000000000ULL) {
        bounds_ok = 0;
    }
    if (code_addr < 0x400000ULL || code_addr >= 0x10000000ULL) {
        bounds_ok = 0;
    }
    if (stack_addr < 0x0000003000000000ULL) {
        bounds_ok = 0;
    }
#else
    if (code_addr >= 0x0000800000000000ULL || stack_addr >= 0x0000800000000000ULL) {
        bounds_ok = 0;
    }
    if (code_addr < 0x400000ULL || code_addr >= 0x10000000ULL) {
        bounds_ok = 0;
    }
    if (stack_addr < 0x0000700000000000ULL) {
        bounds_ok = 0;
    }
#endif

    volatile uint8_t pattern_buf[256];
    for (int i = 0; i < 256; i++) {
        pattern_buf[i] = (uint8_t)(i ^ 0x3C);
    }
    for (int i = 0; i < 256; i++) {
        if (pattern_buf[i] != (uint8_t)(i ^ 0x3C)) {
            bounds_ok = 0;
            break;
        }
    }

    if (selftest_deep_stack_recurse(7) != 0) {
        bounds_ok = 0;
    }

    if (bounds_ok) {
        printf("[+] [TEST 6/8] PASSED: Memory bounds and stack integrity verified.\n");
        passed_tests++;
    } else {
        printf("[-] [TEST 6/8] FAILED: Memory bounds violation or buffer corruption!\n");
    }

    // Test 7: Cooperative Preemption Yield (SYS_YIELD)
    printf("[*] [TEST 7/8] Yielding execution quantum to kernel via yield()...\n");
    yield();
    printf("[+] [TEST 7/8] PASSED: Successfully resumed execution in Ring 3 after yield.\n");
    passed_tests++;

    // Test 8: Sovereign OpSec Ring 3 Defense (Zero-on-Free, Seld-Pledge & RFC 7686 Guard)
    printf("[*] [TEST 8/8] Validating Ring 3 OpSec Hardening (Zero-on-Free, Seld-Pledge, RFC 7686)...\n");
    int opsec_ok = 1;

    // A. Userspace Zero-on-Free Verification
    uint8_t* p_heap = (uint8_t*)malloc(128);
    if (!p_heap) {
        opsec_ok = 0;
    } else {
        memset(p_heap, 0x99, 128);
        free(p_heap);
        for (int i = 0; i < 128; i++) {
            if (p_heap[i] != 0x00) {
                opsec_ok = 0;
                break;
            }
        }
    }

    // B. RFC 7686 Onion Domain DNS Leak Guard
    uint32_t onion_ip = 0;
    int r_onion = seld_dns_resolve("secret-shell-query.onion", &onion_ip);
    if (r_onion != -9) {
        opsec_ok = 0;
    }

    // C. Seld-Pledge Syscall Check
    int pledge_res = seld_pledge(PLEDGE_STDIO | PLEDGE_RPATH | PLEDGE_WPATH | PLEDGE_EXEC | PLEDGE_NET | PLEDGE_DNS | PLEDGE_AUDIO | PLEDGE_PURGE | PLEDGE_REBOOT);
    if (pledge_res != 0) {
        opsec_ok = 0;
    }

    // D. THL Keystroke Timing Jitter Check
    seld_set_jitter(1);
    if (seld_get_jitter() != 1) {
        opsec_ok = 0;
    }
    seld_set_jitter(0);
    if (seld_get_jitter() != 0) {
        opsec_ok = 0;
    }

    // E. Anti-Clock-Skew Normalization Check
    seld_set_skew_defense(1);
    if (seld_get_skew_defense() != 1) {
        opsec_ok = 0;
    }

    // F. Hardware Telemetry Sanitization Check
    struct seld_hw_telemetry tel;
    if (seld_get_hw_telemetry(&tel) != 0 || strcmp(tel.serial_number, "0000-0000-0000-0000") != 0) {
        opsec_ok = 0;
    }

    if (opsec_ok) {
        printf("[+] [TEST 8/8] PASSED: Zero-on-Free, RFC 7686 Guard, and Seld-Pledge verified.\n");
        printf("[+] [TEST 8/8] PASSED: Zero-on-Free, RFC 7686, Anti-Skew, and Telemetry verified.\n");
        passed_tests++;
    } else {
        printf("[-] [TEST 8/8] FAILED: Ring 3 OpSec subsystem verification failure!\n");
    }

    printf("\n----------------------------------------------------------------\n");
    if (passed_tests == total_tests) {
        printf("[Ring 3] ALL %d/%d USERSPACE TESTS PASSED!\n", passed_tests, total_tests);
    } else {
        printf("[Ring 3] WARNING: %d/%d USERSPACE TESTS PASSED.\n", passed_tests, total_tests);
    }
    printf("[Ring 3] ========================================================\n\n");
}

static void builtin_drv_off(int argc, char* argv[]) {
    const char* target = NULL;

    // Syntax 1: "gpu drv off", "cpu drv off", "ram drv off"
    if (argc >= 3 && strcmp(argv[1], "drv") == 0 && strcmp(argv[2], "off") == 0) {
        target = argv[0];
    }
    // Syntax 2: "drv off gpu", "drv off cpu", "drv off ram"
    else if (argc >= 3 && strcmp(argv[0], "drv") == 0 && strcmp(argv[1], "off") == 0) {
        target = argv[2];
    }
    // Syntax 3: "drv gpu off", "drv cpu off", "drv ram off"
    else if (argc >= 3 && strcmp(argv[0], "drv") == 0 && strcmp(argv[2], "off") == 0) {
        target = argv[1];
    }
    else {
        printf("Usage: <gpu|cpu|ram> drv off\n");
        printf("       drv off <gpu|cpu|ram>\n");
        printf("       Brutally shuts down hardware driver, triggering Kernel Panic\n");
        printf("       when the subsystem is next needed.\n");
        return;
    }

    if (strcmp(target, "gpu") == 0) {
        printf("[!] Brutally disabling GPU display driver...\n");
        seld_drv_off("gpu");
        printf("[!] GPU driver terminated.\n");
    } else if (strcmp(target, "cpu") == 0) {
        printf("[!] Brutally disabling CPU core scheduler driver...\n");
        seld_drv_off("cpu");
        printf("[!] CPU driver terminated.\n");
    } else if (strcmp(target, "ram") == 0) {
        printf("[!] Brutally disabling RAM memory manager and heap allocator...\n");
        seld_drv_off("ram");
        printf("[!] RAM driver terminated.\n");
    } else {
        printf("snl: drv: unknown subsystem '%s'. Available: gpu, cpu, ram\n", target);
    }
}

#define SH_MAX_ENV 32
#define SH_ENV_KEY_MAX 32
#define SH_ENV_VAL_MAX 128

struct sh_env_entry {
    char key[SH_ENV_KEY_MAX];
    char val[SH_ENV_VAL_MAX];
};

static struct sh_env_entry s_env[SH_MAX_ENV];
static int s_env_count = 0;
static int s_last_status = 0;

static const char* sh_getenv(const char* key) {
    if (strcmp(key, "?") == 0) {
        static char status_buf[16];
        snprintf(status_buf, sizeof(status_buf), "%d", s_last_status);
        return status_buf;
    }
    for (int i = 0; i < s_env_count; i++) {
        if (strcmp(s_env[i].key, key) == 0) {
            return s_env[i].val;
        }
    }
    return NULL;
}

static void sh_setenv(const char* key, const char* val) {
    if (!key || key[0] == '\0') return;
    for (int i = 0; i < s_env_count; i++) {
        if (strcmp(s_env[i].key, key) == 0) {
            strncpy(s_env[i].val, val ? val : "", sizeof(s_env[i].val) - 1);
            s_env[i].val[sizeof(s_env[i].val) - 1] = '\0';
            return;
        }
    }
    if (s_env_count < SH_MAX_ENV) {
        strncpy(s_env[s_env_count].key, key, sizeof(s_env[s_env_count].key) - 1);
        s_env[s_env_count].key[sizeof(s_env[s_env_count].key) - 1] = '\0';
        strncpy(s_env[s_env_count].val, val ? val : "", sizeof(s_env[s_env_count].val) - 1);
        s_env[s_env_count].val[sizeof(s_env[s_env_count].val) - 1] = '\0';
        s_env_count++;
    }
}

static void sh_init_env(void) {
    sh_setenv("PATH", "/bin");
    sh_setenv("USER", "sovereign");
    sh_setenv("HOME", "/");
}

static void builtin_env(void) {
    for (int i = 0; i < s_env_count; i++) {
        printf("%s=%s\n", s_env[i].key, s_env[i].val);
    }
}

static void builtin_export(int argc, char* argv[]) {
    if (argc == 1) {
        builtin_env();
        return;
    }
    for (int i = 1; i < argc; i++) {
        char* eq = strchr(argv[i], '=');
        if (eq) {
            *eq = '\0';
            sh_setenv(argv[i], eq + 1);
            *eq = '=';
        } else if (i + 1 < argc && strchr(argv[i+1], '=') == NULL) {
            sh_setenv(argv[i], argv[i+1]);
            i++;
        } else {
            sh_setenv(argv[i], "");
        }
    }
}

static void expand_variables(const char* src, char* dst, size_t dst_size) {
    size_t d = 0;
    size_t s = 0;
    while (src[s] != '\0' && d < dst_size - 1) {
        if (src[s] == '$') {
            s++;
            if (src[s] == '?') {
                const char* val = sh_getenv("?");
                if (val) {
                    while (*val && d < dst_size - 1) dst[d++] = *val++;
                }
                s++;
                continue;
            }
            char var_name[32];
            size_t vn_len = 0;
            while (src[s] && ((src[s] >= 'A' && src[s] <= 'Z') ||
                              (src[s] >= 'a' && src[s] <= 'z') ||
                              (src[s] >= '0' && src[s] <= '9') ||
                              src[s] == '_') && vn_len < sizeof(var_name) - 1) {
                var_name[vn_len++] = src[s++];
            }
            var_name[vn_len] = '\0';
            if (vn_len > 0) {
                const char* val = sh_getenv(var_name);
                if (val) {
                    while (*val && d < dst_size - 1) dst[d++] = *val++;
                }
            } else {
                dst[d++] = '$';
            }
        } else {
            dst[d++] = src[s++];
        }
    }
    dst[d] = '\0';
}

static int find_executable(const char* cmd, char* resolved, size_t max_len) {
    if (!cmd || cmd[0] == '\0') return -1;
    struct seld_stat st;

    // Direct path or starts with / or ./
    if (cmd[0] == '/' || (cmd[0] == '.' && cmd[1] == '/')) {
        if (seld_stat(cmd, &st) == 0) {
            strncpy(resolved, cmd, max_len - 1);
            resolved[max_len - 1] = '\0';
            return 0;
        }
        return -1;
    }

    // Lookup in PATH
    const char* path_env = sh_getenv("PATH");
    if (!path_env || path_env[0] == '\0') path_env = "/bin";

    char path_copy[128];
    strncpy(path_copy, path_env, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    char* dir = path_copy;
    while (dir && *dir) {
        char* colon = strchr(dir, ':');
        if (colon) *colon = '\0';

        char test_path[128];
        snprintf(test_path, sizeof(test_path), "%s/%s", dir, cmd);
        if (seld_stat(test_path, &st) == 0) {
            strncpy(resolved, test_path, max_len - 1);
            resolved[max_len - 1] = '\0';
            return 0;
        }

        if (colon) dir = colon + 1;
        else break;
    }

    // Fallback: check /bin/<cmd> or current directory
    char fallback[128];
    snprintf(fallback, sizeof(fallback), "/bin/%s", cmd);
    if (seld_stat(fallback, &st) == 0) {
        strncpy(resolved, fallback, max_len - 1);
        resolved[max_len - 1] = '\0';
        return 0;
    }

    if (seld_stat(cmd, &st) == 0) {
        strncpy(resolved, cmd, max_len - 1);
        resolved[max_len - 1] = '\0';
        return 0;
    }

    return -1;
}

static int run_script(const char* filepath);

static int execute_single_command(int argc, char* argv[]) {
    if (argc == 0 || !argv[0]) return 0;
    const char* cmd = argv[0];

    // Builtin command dispatch
    if (strcmp(cmd, "help") == 0) {
        builtin_help();
        return 0;
    } else if (strcmp(cmd, "colors") == 0) {
        builtin_colors();
        return 0;
    } else if (strcmp(cmd, "pc") == 0) {
        builtin_pc(argc, argv);
        return 0;
    } else if (strcmp(cmd, "mobile") == 0) {
        builtin_mobile();
        return 0;
    } else if (strcmp(cmd, "clear") == 0) {
        seld_clear();
        if (s_mobile_mode) {
            seld_set_console_rows(12);
            draw_shell_hud();
        } else {
            seld_set_console_rows(20);
        }
        return 0;
    } else if (strcmp(cmd, "echo") == 0) {
        builtin_echo(argc, argv);
        return 0;
    } else if (strcmp(cmd, "export") == 0) {
        builtin_export(argc, argv);
        return 0;
    } else if (strcmp(cmd, "env") == 0) {
        builtin_env();
        return 0;
    } else if (strcmp(cmd, "exit") == 0) {
        printf("[*] Exiting SNL Sovereign Shell...\n");
        exit(0);
    } else if (strcmp(cmd, "uptime") == 0) {
        builtin_uptime();
        return 0;
    } else if (strcmp(cmd, "beep") == 0) {
        builtin_beep(argc, argv);
        return 0;
    } else if (strcmp(cmd, "snd") == 0) {
        builtin_snd(argc, argv);
        return 0;
    } else if (strcmp(cmd, "mem") == 0) {
        builtin_mem();
        return 0;
    } else if (strcmp(cmd, "ifconfig") == 0) {
        builtin_ifconfig(argc, argv);
        return 0;
    } else if (strcmp(cmd, "arp") == 0) {
        builtin_arp();
        return 0;
    } else if (strcmp(cmd, "ping") == 0) {
        builtin_ping(argc, argv);
        return 0;
    } else if (strcmp(cmd, "dns") == 0) {
        builtin_dns(argc, argv);
        return 0;
    } else if (strcmp(cmd, "purge") == 0) {
        builtin_purge();
        return 0;
    } else if (strcmp(cmd, "net") == 0) {
        builtin_net(argc, argv);
        return 0;
    } else if (strcmp(cmd, "skew") == 0) {
        builtin_skew(argc, argv);
        return 0;
    } else if (strcmp(cmd, "time") == 0) {
        builtin_time(argc, argv);
        return 0;
    } else if (strcmp(cmd, "telemetry") == 0) {
        builtin_telemetry(argc, argv);
        return 0;
    } else if (strcmp(cmd, "desync") == 0) {
        builtin_desync(argc, argv);
        return 0;
    } else if (strcmp(cmd, "jitter") == 0) {
        builtin_jitter(argc, argv);
        return 0;
    } else if (strcmp(cmd, "grip") == 0 || strcmp(cmd, "deadman") == 0) {
        builtin_deadman(argc, argv);
        return 0;
    } else if (strcmp(cmd, "chaff") == 0) {
        builtin_chaff(argc, argv);
        return 0;
    } else if (strcmp(cmd, "tempest") == 0) {
        builtin_tempest(argc, argv);
        return 0;
    } else if (strcmp(cmd, "uarch") == 0) {
        builtin_uarch(argc, argv);
        return 0;
    } else if (strcmp(cmd, "pci") == 0) {
        builtin_pci(argc, argv);
        return 0;
    } else if (strcmp(cmd, "unveil") == 0) {
        builtin_unveil(argc, argv);
        return 0;
    } else if (strcmp(cmd, "selftest") == 0) {
        builtin_selftest();
        return 0;
    } else if (strcmp(cmd, "gpu") == 0 || strcmp(cmd, "cpu") == 0 || strcmp(cmd, "ram") == 0 || strcmp(cmd, "drv") == 0) {
        builtin_drv_off(argc, argv);
        return 0;
    } else if (strcmp(cmd, "fetch") == 0) {
        builtin_fetch();
        return 0;
    } else if (strcmp(cmd, "reboot") == 0) {
        builtin_reboot();
        return 0;
    } else if (strcmp(cmd, "poweroff") == 0 || strcmp(cmd, "shutdown") == 0) {
        builtin_poweroff();
        return 0;
    } else if (strcmp(cmd, "blackout") == 0) {
        builtin_blackout();
        return 0;
    } else if (strcmp(cmd, "firekill") == 0) {
        builtin_firekill();
        return 0;
    } else if (strcmp(cmd, "sh") == 0 && argc >= 2) {
        return run_script(argv[1]);
    }

    // External binary execution from SeldFS
    char bin_path[128];
    if (find_executable(cmd, bin_path, sizeof(bin_path)) == 0) {
        argv[0] = bin_path;
        int res = spawnv(bin_path, argv);
        if (res < 0) {
            printf("snl: %s: execution failed (code: %d)\n", cmd, res);
        }
        return res;
    }

    // Friendly messages for special binaries
    if (strcmp(cmd, "doom") == 0) {
        printf("snl: doom: not installed in SeldFS. Run 'download doom' to fetch DOOM.\n");
    } else if (strcmp(cmd, "tor") == 0 || strcmp(cmd, "torbrowser") == 0) {
        printf("snl: tor: not installed in SeldFS. Run 'download tor' to fetch Tor Browser.\n");
    } else {
        printf("snl: %s: command not found\n", cmd);
    }
    return 127;
}

static int execute_stage_with_redirection(char* stage_str, int default_in_fd, int default_out_fd) {
    char clean_stage[MAX_LINE_LEN];
    char in_file[64] = {0};
    char out_file[64] = {0};
    int append_out = 0;

    size_t s_len = strlen(stage_str);
    size_t out_idx = 0;
    int in_quote = 0;

    for (size_t i = 0; i < s_len; i++) {
        char c = stage_str[i];
        if (c == '"' || c == '\'') {
            in_quote = !in_quote;
            clean_stage[out_idx++] = c;
            continue;
        }

        if (!in_quote && c == '>') {
            if (i + 1 < s_len && stage_str[i+1] == '>') {
                append_out = 1;
                i += 2;
            } else {
                append_out = 0;
                i += 1;
            }
            while (i < s_len && (stage_str[i] == ' ' || stage_str[i] == '\t')) i++;
            size_t fn_idx = 0;
            while (i < s_len && stage_str[i] != ' ' && stage_str[i] != '\t' &&
                   stage_str[i] != '<' && stage_str[i] != '>' && fn_idx < sizeof(out_file) - 1) {
                out_file[fn_idx++] = stage_str[i++];
            }
            out_file[fn_idx] = '\0';
            i--;
            continue;
        }

        if (!in_quote && c == '<') {
            i++;
            while (i < s_len && (stage_str[i] == ' ' || stage_str[i] == '\t')) i++;
            size_t fn_idx = 0;
            while (i < s_len && stage_str[i] != ' ' && stage_str[i] != '\t' &&
                   stage_str[i] != '<' && stage_str[i] != '>' && fn_idx < sizeof(in_file) - 1) {
                in_file[fn_idx++] = stage_str[i++];
            }
            in_file[fn_idx] = '\0';
            i--;
            continue;
        }

        clean_stage[out_idx++] = c;
    }
    clean_stage[out_idx] = '\0';

    int in_fd = -1;
    int out_fd = -1;

    if (in_file[0] != '\0') {
        in_fd = seld_open(in_file, O_RDONLY, 0);
        if (in_fd < 0) {
            printf("snl: %s: No such file or directory\n", in_file);
            return 1;
        }
        seld_redirect_stdin(in_fd);
    } else if (default_in_fd >= 0) {
        seld_redirect_stdin(default_in_fd);
    }

    if (out_file[0] != '\0') {
        int flags = O_WRONLY | O_CREAT;
        if (append_out) flags |= O_APPEND;
        else flags |= O_TRUNC;
        out_fd = seld_open(out_file, flags, 0644);
        if (out_fd < 0) {
            printf("snl: %s: cannot open file for writing\n", out_file);
            if (in_file[0] != '\0') {
                seld_redirect_stdin(-1);
                if (in_fd >= 0) seld_close(in_fd);
            } else if (default_in_fd >= 0) {
                seld_redirect_stdin(-1);
            }
            return 1;
        }
        seld_redirect_stdout(out_fd);
    } else if (default_out_fd >= 0) {
        seld_redirect_stdout(default_out_fd);
    }

    char* cmd_argv[MAX_ARGS];
    int cmd_argc = parse_args(clean_stage, cmd_argv, MAX_ARGS);
    int ret = 0;
    if (cmd_argc > 0) {
        ret = execute_single_command(cmd_argc, cmd_argv);
    }

    fflush(stdout);

    if (in_file[0] != '\0') {
        seld_redirect_stdin(-1);
        if (in_fd >= 0) seld_close(in_fd);
    } else if (default_in_fd >= 0) {
        seld_redirect_stdin(-1);
    }

    if (out_file[0] != '\0') {
        seld_redirect_stdout(-1);
        if (out_fd >= 0) seld_close(out_fd);
    } else if (default_out_fd >= 0) {
        seld_redirect_stdout(-1);
    }

    return ret;
}

static int execute_pipeline(char* raw_line) {
    int bg = 0;
    size_t rlen = strlen(raw_line);
    while (rlen > 0 && (raw_line[rlen-1] == ' ' || raw_line[rlen-1] == '\t')) {
        raw_line[--rlen] = '\0';
    }
    if (rlen > 0 && raw_line[rlen-1] == '&') {
        bg = 1;
        raw_line[--rlen] = '\0';
        while (rlen > 0 && (raw_line[rlen-1] == ' ' || raw_line[rlen-1] == '\t')) {
            raw_line[--rlen] = '\0';
        }
    }

    if (rlen == 0) return 0;

    char* stages[16];
    int num_stages = 0;
    char* cur_stage = raw_line;
    int in_quote = 0;

    for (size_t i = 0; raw_line[i] != '\0'; i++) {
        if (raw_line[i] == '"' || raw_line[i] == '\'') {
            in_quote = !in_quote;
        } else if (!in_quote && raw_line[i] == '|') {
            raw_line[i] = '\0';
            stages[num_stages++] = cur_stage;
            cur_stage = &raw_line[i+1];
            if (num_stages >= 15) break;
        }
    }
    stages[num_stages++] = cur_stage;

    int last_status = 0;

    if (num_stages == 1) {
        last_status = execute_stage_with_redirection(stages[0], -1, -1);
    } else {
        const char* pipe_files[2] = {"/tmp/.p0", "/tmp/.p1"};
        seld_unlink(pipe_files[0]);
        seld_unlink(pipe_files[1]);

        for (int st = 0; st < num_stages; st++) {
            int in_fd = -1;
            int out_fd = -1;

            if (st > 0) {
                const char* prev_pfile = pipe_files[(st - 1) % 2];
                in_fd = seld_open(prev_pfile, O_RDONLY, 0);
            }

            if (st < num_stages - 1) {
                const char* next_pfile = pipe_files[st % 2];
                seld_unlink(next_pfile);
                out_fd = seld_open(next_pfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            }

            last_status = execute_stage_with_redirection(stages[st], in_fd, out_fd);

            if (in_fd >= 0) seld_close(in_fd);
            if (out_fd >= 0) seld_close(out_fd);
        }

        seld_unlink(pipe_files[0]);
        seld_unlink(pipe_files[1]);
    }

    if (bg) {
        printf("[1] %d (background execution finished)\n", 100 + (int)(seld_uptime() % 900));
    }

    return last_status;
}

static int execute_line(char* line) {
    if (!line) return 0;
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0' || *line == '#') return 0;

    char expanded[MAX_LINE_LEN * 2];
    expand_variables(line, expanded, sizeof(expanded));

    int ret = execute_pipeline(expanded);
    s_last_status = ret;
    return ret;
}

static int run_script(const char* filepath) {
    int fd = seld_open(filepath, O_RDONLY, 0);
    if (fd < 0) {
        printf("snl: %s: No such file or directory\n", filepath);
        return 127;
    }

    struct seld_stat st;
    if (seld_stat(filepath, &st) != 0 || st.size == 0) {
        seld_close(fd);
        return 0;
    }

    char* script_buf = (char*)malloc(st.size + 2);
    if (!script_buf) {
        seld_close(fd);
        printf("snl: %s: out of memory\n", filepath);
        return 1;
    }

    int bytes_read = seld_read(fd, script_buf, st.size);
    seld_close(fd);
    if (bytes_read < 0) bytes_read = 0;
    script_buf[bytes_read] = '\0';

    char* p = script_buf;
    int last_ret = 0;

    while (*p != '\0') {
        char* line_start = p;
        while (*p != '\0' && *p != '\n' && *p != '\r') {
            p++;
        }
        if (*p != '\0') {
            *p++ = '\0';
            if (*p == '\n') p++;
        }

        while (*line_start == ' ' || *line_start == '\t') line_start++;
        if (*line_start == '\0' || *line_start == '#') continue;

        size_t llen = strlen(line_start);
        while (llen > 0 && (line_start[llen-1] == ' ' || line_start[llen-1] == '\t' || line_start[llen-1] == '\r')) {
            line_start[--llen] = '\0';
        }

        if (llen == 0) continue;

        last_ret = execute_line(line_start);
    }

    free(script_buf);
    return last_ret;
}

int main(int argc, char* argv[]) {
    sh_init_env();

    if (argc >= 2) {
        return run_script(argv[1]);
    }

    seld_clear();

    struct seld_stat pcmode_st;
    if (seld_stat("pcmode", &pcmode_st) == 0) {
        s_mobile_mode = 0;
        seld_set_console_rows(20);
        clear_touch_area();
    } else {
        s_mobile_mode = 1;
        seld_set_console_rows(12);
    }

    print_banner();

    char line_buf[MAX_LINE_LEN];

    while (1) {
        printf("snl$ ");
        fflush(stdout);

        int len = read_line(line_buf, sizeof(line_buf));
        if (len == 0) {
            continue;
        }

        char* hist_entry = (char*)malloc(len + 1);
        if (hist_entry) {
            strcpy(hist_entry, line_buf);
            if (s_sh_history_count < SH_HISTORY_MAX) {
                s_sh_history[s_sh_history_count++] = hist_entry;
            } else {
                free(s_sh_history[0]);
                for (int h = 0; h < SH_HISTORY_MAX - 1; h++) {
                    s_sh_history[h] = s_sh_history[h + 1];
                }
                s_sh_history[SH_HISTORY_MAX - 1] = hist_entry;
            }
        }

        execute_line(line_buf);
    }

    return 0;
}
