/*
 * SeldOS - Humboldt Kernel Project
 * SNL (Seld Not Linux) Sovereign Shell v0.1 (snl-sh)
 * Autonomous interactive Ring 3 Shell with line editing, builtins,
 * and SeldFS /bin program execution.
 * GPLv3 Licensed.
 */

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "seld.h"

#define MAX_LINE_LEN 256
#define MAX_ARGS 16

static void print_banner(void) {
    printf("=======================================================================\n");
    printf("     _.-'''''-._\n");
    printf("   .'  _     _  '.        SNL (Seld Not Linux) Sovereign Shell v0.1\n");
    printf("  /   (o)   (o)   \\       Ring 3 Sovereign CLI Environment (GPLv3)\n");
    printf(" |                 |      Humboldt Framebuffer 680x334 | 39-Color Palette\n");
    printf(" |     <--V-->     |      x86_64 Long Mode Isolated Execution\n");
    printf("  \\               /\n");
    printf("   '.  '-----'  .'\n");
    printf("     '-._____.-'\n");
    printf("=======================================================================\n");
    printf("Welcome to SNL Sovereign Shell. Type 'help' for available commands.\n");
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
    s_vkeys[s_num_vkeys++] = (struct vkey){8, 204, 64, 24, 0, "DOOM", 0xFF22DD22, "doom"};
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
            if (kev.pressed) {
                if (s_mouse_active) {
                    cursor_hide();
                    s_mouse_active = 0;
                }
                uint8_t sc = kev.scancode & 0x7F;
                char c = scancode_ascii[sc];
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

        if (argc < max_args - 1) {
            argv[argc++] = p;
        }

        while (*p != '\0' && *p != ' ' && *p != '\t') {
            p++;
        }
    }
    argv[argc] = NULL;
    return argc;
}

static void builtin_help(void) {
    printf("SNL (Seld Not Linux) Sovereign Shell v0.1\n");
    printf("Autonomous Interactive Ring 3 Shell (GPLv3)\n\n");
    printf("Builtin Commands:\n");
    printf("  help            Display this help summary\n");
    printf("  colors          Display 39 Humboldt colors palette\n");
    printf("  pc [on|off]     Toggle PC mode (disable mobile touch HUD & keyboard)\n");
    printf("  mobile          Enable mobile touch HUD and virtual keyboard\n");
    printf("  clear           Clear screen buffer\n");
    printf("  echo [args..]   Output arguments to standard output\n");
    printf("  exit            Terminate shell session\n");
    printf("  uptime          Display system running time from PIT chronometer\n");
    printf("  beep [freq] [d] Test audio output with tone frequency and duration\n");
    printf("  mem             Display physical memory & kernel heap usage\n");
    printf("  ifconfig        Display network interface details & statistics (e1000)\n");
    printf("  ping <ip>       Send ICMP Echo requests to host\n");
    printf("  arp             Display kernel ARP resolution cache\n");
    printf("  selftest        Execute userspace Ring 3 verification test suite\n\n");
    printf("External Utilities in /bin/:\n");
    printf("  ls              List files with size, blocks, and SHA-256 hash\n");
    printf("  cat <file>      Display contents of file\n");
    printf("  echo [args..]   Print arguments to stdout\n");
    printf("  rm <file>       Remove file from SeldFS storage\n");
    printf("  sha256sum <f>   Verify file integrity against stored SHA-256\n");
    printf("  uname           Display system identification\n");
    printf("  ps              Query and display active tasks / PID info\n");
    printf("  doom            Classic DOOM (doomgeneric with linear framebuffer)\n");
    printf("  fm              Seld Sovereign Graphical File Manager (SNL-FM)\n");
    printf("  download <url>  Fetch binary/package over network into SeldFS\n");
    printf("  tor             Tor Browser (download via 'download tor')\n");
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
    uint32_t freq = 880;
    uint32_t dur = 150;
    if (argc >= 2) freq = (uint32_t)atoi(argv[1]);
    if (argc >= 3) dur = (uint32_t)atoi(argv[2]);
    printf("[Audio] Playing beep tone: %u Hz (%u ms)...\n", freq, dur);
    seld_beep(freq, dur);
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

static void builtin_ifconfig(void) {
    struct seld_net_info info;
    if (seld_net_info(&info) != 0) {
        printf("[-] Failed to query network interface.\n");
        return;
    }

    if (!info.link_up) {
        printf("eth0: flags=DOWN (Interface Offline / No Compatible NIC Carrier Detected)\n");
        printf("      In VirtualBox / VM settings: Set Adapter Type to Intel PRO/1000 MT (82540EM) or AMD PCnet-FAST III (Am79C973)\n");
        return;
    }

    char ip_buf[16], mask_buf[16], gw_buf[16], mac_buf[18];
    format_ip_u(info.ip, ip_buf, sizeof(ip_buf));
    format_ip_u(info.netmask, mask_buf, sizeof(mask_buf));
    format_ip_u(info.gateway, gw_buf, sizeof(gw_buf));
    format_mac_u(info.mac, mac_buf, sizeof(mac_buf));

    printf("eth0: flags=UP,BROADCAST,MULTICAST mtu 1500\n");
    printf("      ether %s (Hardware PCI Ethernet)\n", mac_buf);
    printf("      inet %s  netmask %s  gateway %s\n", ip_buf, mask_buf, gw_buf);
    printf("      RX packets %lu  bytes %lu  dropped %lu  errors %lu\n",
           info.rx_frames, info.rx_bytes, info.rx_dropped, info.rx_checksum_errors);
    printf("      TX packets %lu  bytes %lu\n",
           info.tx_frames, info.tx_bytes);
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
    uint32_t target_ip = 0;
    if (parse_ip_u(argv[1], &target_ip) != 0) {
        if (seld_dns_resolve(argv[1], &target_ip) != 0) {
            printf("[-] ping: cannot resolve '%s': Unknown host\n", argv[1]);
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
}

static void builtin_dns(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: dns <hostname>\n");
        return;
    }
    printf("Resolving %s via DNS (UDP 10.0.2.3:53)...\n", argv[1]);
    uint32_t ip = 0;
    if (seld_dns_resolve(argv[1], &ip) == 0) {
        char ip_str[16];
        format_ip_u(ip, ip_str, sizeof(ip_str));
        printf("Resolved: %s -> %s\n", argv[1], ip_str);
    } else {
        printf("[-] Failed to resolve %s\n", argv[1]);
    }
}

static void builtin_selftest(void) {
    printf("\n[Ring 3] ========================================================\n");
    printf("[Ring 3]   SELD OS USERSPACE RUNTIME & TEST SUITE (Ring 3)\n");
    printf("[Ring 3] ========================================================\n");

    int total_tests = 7;
    int passed_tests = 0;

    // Test 1: Hardware Privilege Level & Segment Selectors (CPL=3)
    printf("[*] [TEST 1/7] Inspecting CPU Privilege Level & Segments...\n");
    uint16_t cs = 0, ss = 0;
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));
    __asm__ volatile ("mov %%ss, %0" : "=r"(ss));
    uint8_t cpl = cs & 3;
    uint8_t spl = ss & 3;

    printf("    CS selector: 0x%x (RPL = %d)\n", cs, cpl);
    printf("    SS selector: 0x%x (RPL = %d)\n", ss, spl);

    if (cpl == 3 && spl == 3) {
        printf("[+] [TEST 1/7] PASSED: Hardware CPL=3 verified. Running in unprivileged user mode.\n");
        passed_tests++;
    } else {
        printf("[-] [TEST 1/7] FAILED: CPL/SPL privilege level mismatch!\n");
    }

    // Test 2: Fast SYSCALL / SYSRET Handshake (SYS_SELD 42)
    printf("[*] [TEST 2/7] Invoking SYS_SELD handshake (syscall 42)...\n");
    long seld_ret = seld_ping();
    if (seld_ret == 0x5E1D5EC) {
        printf("[+] [TEST 2/7] PASSED: Syscall handshake returned 0x5E1D5EC.\n");
        passed_tests++;
    } else {
        printf("[-] [TEST 2/7] FAILED: Invalid handshake return code: 0x%lx\n", seld_ret);
    }

    // Test 3: Process Identification (SYS_GETPID)
    printf("[*] [TEST 3/7] Invoking SYS_GETPID via getpid() wrapper...\n");
    int pid = getpid();
    printf("    Assigned Process ID: %d\n", pid);
    if (pid >= 1) {
        printf("[+] [TEST 3/7] PASSED: Valid process ID received from kernel scheduler.\n");
        passed_tests++;
    } else {
        printf("[-] [TEST 3/7] FAILED: Invalid PID received!\n");
    }

    // Test 4: System Chronometry (SYS_UPTIME)
    printf("[*] [TEST 4/7] Invoking SYS_UPTIME via uptime() wrapper...\n");
    uint64_t ms = uptime();
    printf("    Current system uptime: %lu ms\n", ms);
    if (ms > 0) {
        printf("[+] [TEST 4/7] PASSED: High-precision PIT timer counter queried.\n");
        passed_tests++;
    } else {
        printf("[-] [TEST 4/7] FAILED: Uptime query returned 0 ms!\n");
    }

    // Test 5: Syscall Verification & Parameter Passing (SYS_SELD_VERIFY)
    printf("[*] [TEST 5/7] Testing SYS_SELD_VERIFY parameter validation...\n");
    long v0 = seld_verify(0, 0);
    long token = 0x12345678L;
    long v1 = seld_verify(token, 0);
    long expected_v1 = token ^ 0x5E1D5E1DL;

    long v_user_stack = seld_verify(1, (long)&cpl);
    long v_kernel_ptr = seld_verify(1, (long)0xFFFF800000000000ULL);

    if (v0 == 0x5E1D0001L && v1 == expected_v1 && v_user_stack == 1 && v_kernel_ptr == 0) {
        printf("[+] [TEST 5/7] PASSED: Token arithmetic and user/kernel pointer verification valid.\n");
        passed_tests++;
    } else {
        printf("[-] [TEST 5/7] FAILED: Parameter verification mismatch!\n");
    }

    // Test 6: Memory Bounds & Stack R/W Integrity
    printf("[*] [TEST 6/7] Validating userspace memory bounds and stack R/W...\n");
    uint64_t code_addr = (uint64_t)&builtin_selftest;
    uint64_t stack_addr = (uint64_t)&cpl;

    printf("    Code virtual address: 0x%lx\n", code_addr);
    printf("    Stack virtual address: 0x%lx\n", stack_addr);

    int bounds_ok = 1;
    if (code_addr >= 0x0000800000000000ULL || stack_addr >= 0x0000800000000000ULL) {
        bounds_ok = 0;
    }
    if (code_addr < 0x400000ULL || code_addr >= 0x10000000ULL) {
        bounds_ok = 0;
    }
    if (stack_addr < 0x0000700000000000ULL) {
        bounds_ok = 0;
    }

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

    if (bounds_ok) {
        printf("[+] [TEST 6/7] PASSED: Memory bounds and stack integrity verified.\n");
        passed_tests++;
    } else {
        printf("[-] [TEST 6/7] FAILED: Memory bounds violation or buffer corruption!\n");
    }

    // Test 7: Cooperative Preemption Yield (SYS_YIELD)
    printf("[*] [TEST 7/7] Yielding execution quantum to kernel via yield()...\n");
    yield();
    printf("[+] [TEST 7/7] PASSED: Successfully resumed execution in Ring 3 after yield.\n");
    passed_tests++;

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

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

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
    char* cmd_argv[MAX_ARGS];

    while (1) {
        printf("snl$ ");
        fflush(stdout);

        int len = read_line(line_buf, sizeof(line_buf));
        if (len == 0) {
            continue;
        }

        int cmd_argc = parse_args(line_buf, cmd_argv, MAX_ARGS);
        if (cmd_argc == 0) {
            continue;
        }

        const char* cmd = cmd_argv[0];

        // Builtin command dispatch
        if (strcmp(cmd, "help") == 0) {
            builtin_help();
        } else if (strcmp(cmd, "colors") == 0) {
            builtin_colors();
        } else if (strcmp(cmd, "pc") == 0) {
            builtin_pc(cmd_argc, cmd_argv);
        } else if (strcmp(cmd, "mobile") == 0) {
            builtin_mobile();
        } else if (strcmp(cmd, "clear") == 0) {
            seld_clear();
            if (s_mobile_mode) {
                seld_set_console_rows(12);
                draw_shell_hud();
            } else {
                seld_set_console_rows(20);
            }
        } else if (strcmp(cmd, "echo") == 0) {
            builtin_echo(cmd_argc, cmd_argv);
        } else if (strcmp(cmd, "exit") == 0) {
            printf("[*] Exiting SNL Sovereign Shell...\n");
            exit(0);
        } else if (strcmp(cmd, "uptime") == 0) {
            builtin_uptime();
        } else if (strcmp(cmd, "beep") == 0) {
            builtin_beep(cmd_argc, cmd_argv);
        } else if (strcmp(cmd, "mem") == 0) {
            builtin_mem();
        } else if (strcmp(cmd, "ifconfig") == 0) {
            builtin_ifconfig();
        } else if (strcmp(cmd, "arp") == 0) {
            builtin_arp();
        } else if (strcmp(cmd, "ping") == 0) {
            builtin_ping(cmd_argc, cmd_argv);
        } else if (strcmp(cmd, "dns") == 0) {
            builtin_dns(cmd_argc, cmd_argv);
        } else if (strcmp(cmd, "selftest") == 0) {
            builtin_selftest();
        } else if (strcmp(cmd, "gpu") == 0 || strcmp(cmd, "cpu") == 0 || strcmp(cmd, "ram") == 0 || strcmp(cmd, "drv") == 0) {
            builtin_drv_off(cmd_argc, cmd_argv);
        } else {
            // External command execution from SeldFS (/bin/<cmd>)
            int res = spawnv(cmd, cmd_argv);
            if (res < 0) {
                if (strcmp(cmd, "tor") == 0 || strcmp(cmd, "torbrowser") == 0) {
                    printf("snl: tor: not installed in SeldFS.\n");
                    printf("     Run 'download tor' to fetch Tor Browser over the network.\n");
                } else {
                    printf("snl: %s: command not found\n", cmd);
                }
            }
        }

        if (len > 0) {
            char* hist_entry = (char*)malloc(len + 1);
            if (hist_entry) {
                strcpy(hist_entry, line_buf);
            }
        }
    }

    return 0;
}
