// doomgeneric port for SeldOS (Humboldt Kernel)
// Includes Mobile / Touchscreen Virtual Controls HUD & Mouse Driver Integration
// Features full 640x480 offscreen compositing (zero flicker) & direct touch menu navigation
// GPLv3 Licensed.

#include "doomkeys.h"
#include "m_argv.h"
#include "doomgeneric.h"
#include "m_menu.h"
#include "doomstat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "seld.h"

#define COMP_W 680
#define COMP_H 334

static struct seld_fb_info s_fb;
static int s_fb_available = 0;
static int s_show_touch_hud = 1;

static int32_t s_mouse_x = 340;
static int32_t s_mouse_y = 167;
static int s_is_touching = 0;
static int s_prev_touching = 0;
static int s_cur_weapon = 2; // Default shotgun / pistol
static int s_wpn_prev_active = 0;
static int s_wpn_next_active = 0;

/* Offscreen Compositing Buffer for 100% flicker-free rendering */
static uint32_t s_compose_buf[COMP_W * COMP_H];

/* 5x7 ASCII bitmap font (ASCII 32 to 90) */
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

static void draw_char(uint32_t* buf, int x, int y, char c, uint32_t color, int scale) {
    if (c >= 'a' && c <= 'z') c -= ('a' - 'A');
    if (c < 32 || c > 90) return;
    const uint8_t* glyph = font5x7[c - 32];
    for (int col = 0; col < 5; col++) {
        uint8_t line = glyph[col];
        for (int row = 0; row < 7; row++) {
            if (line & (1 << row)) {
                for (int sx = 0; sx < scale; sx++) {
                    for (int sy = 0; sy < scale; sy++) {
                        int px = x + col * scale + sx;
                        int py = y + row * scale + sy;
                        if (px >= 0 && px < COMP_W && py >= 0 && py < COMP_H) {
                            buf[py * COMP_W + px] = color;
                        }
                    }
                }
            }
        }
    }
}

static void draw_string(uint32_t* buf, int x, int y, const char* str, uint32_t color, int scale) {
    while (*str) {
        draw_char(buf, x, y, *str, color, scale);
        x += (5 + 1) * scale;
        str++;
    }
}

static void draw_button(uint32_t* buf, int x, int y, int w, int h, const char* label, int active, uint32_t border_color, uint32_t fill_color) {
    // Fill interior
    if (active) {
        for (int py = y + 1; py < y + h - 1; py++) {
            for (int px = x + 1; px < x + w - 1; px++) {
                if (px >= 0 && px < COMP_W && py >= 0 && py < COMP_H) {
                    buf[py * COMP_W + px] = fill_color;
                }
            }
        }
    } else {
        // High quality semi-transparent tinted background
        for (int py = y + 1; py < y + h - 1; py++) {
            for (int px = x + 1; px < x + w - 1; px++) {
                if (px >= 0 && px < COMP_W && py >= 0 && py < COMP_H) {
                    uint32_t bg = buf[py * COMP_W + px];
                    uint32_t br = (bg >> 16) & 0xFF;
                    uint32_t bg_val = (bg >> 8) & 0xFF;
                    uint32_t bb = bg & 0xFF;

                    uint32_t tr = (fill_color >> 16) & 0xFF;
                    uint32_t tg = (fill_color >> 8) & 0xFF;
                    uint32_t tb = fill_color & 0xFF;

                    uint32_t r = (br * 2 + tr) / 3;
                    uint32_t g = (bg_val * 2 + tg) / 3;
                    uint32_t b = (bb * 2 + tb) / 3;

                    buf[py * COMP_W + px] = (r << 16) | (g << 8) | b;
                }
            }
        }
    }

    // Border (2px)
    for (int t = 0; t < 2; t++) {
        for (int px = x; px < x + w; px++) {
            if (px >= 0 && px < COMP_W) {
                if (y + t >= 0 && y + t < COMP_H) buf[(y + t) * COMP_W + px] = border_color;
                if (y + h - 1 - t >= 0 && y + h - 1 - t < COMP_H) buf[(y + h - 1 - t) * COMP_W + px] = border_color;
            }
        }
        for (int py = y; py < y + h; py++) {
            if (py >= 0 && py < COMP_H) {
                if (x + t >= 0 && x + t < COMP_W) buf[py * COMP_W + (x + t)] = border_color;
                if (x + w - 1 - t >= 0 && x + w - 1 - t < COMP_W) buf[py * COMP_W + (x + w - 1 - t)] = border_color;
            }
        }
    }

    // Centered label text
    if (label && *label) {
        int text_len = 0;
        const char* p = label;
        while (*p++) text_len++;
        int scale = (w >= 75 && h >= 45) ? 2 : 1;
        int text_w = text_len * 6 * scale;
        int text_h = 7 * scale;
        int tx = x + (w - text_w) / 2;
        int ty = y + (h - text_h) / 2;
        uint32_t text_color = active ? 0x00000000 : 0x00FFFFFF;
        draw_string(buf, tx, ty, label, text_color, scale);
    }
}

/* Touch Action IDs */
enum touch_btn_id {
    T_UP = 0,
    T_DOWN,
    T_LEFT,
    T_RIGHT,
    T_STRAFE_L,
    T_STRAFE_R,
    T_FIRE,
    T_USE,
    T_RUN,
    T_ESC,
    T_ENTER,
    T_MAP,
    T_COUNT
};

static const unsigned char s_touch_keys[T_COUNT] = {
    [T_UP]       = KEY_UPARROW,
    [T_DOWN]     = KEY_DOWNARROW,
    [T_LEFT]     = KEY_LEFTARROW,
    [T_RIGHT]    = KEY_RIGHTARROW,
    [T_STRAFE_L] = KEY_STRAFE_L,
    [T_STRAFE_R] = KEY_STRAFE_R,
    [T_FIRE]     = KEY_FIRE,
    [T_USE]      = KEY_USE,
    [T_RUN]      = KEY_RSHIFT,
    [T_ESC]      = KEY_ESCAPE,
    [T_ENTER]    = KEY_ENTER,
    [T_MAP]      = KEY_TAB,
};

static uint32_t s_active_touch_mask = 0;

/* Simulated Input Event Queue */
struct doom_input_event {
    int pressed;
    unsigned char key;
};
#define INPUT_QUEUE_SIZE 128
static struct doom_input_event s_input_q[INPUT_QUEUE_SIZE];
static volatile size_t s_in_head = 0;
static volatile size_t s_in_tail = 0;

static void queue_doom_key(int pressed, unsigned char key) {
    size_t next = (s_in_head + 1) % INPUT_QUEUE_SIZE;
    if (next != s_in_tail) {
        s_input_q[s_in_head].pressed = pressed;
        s_input_q[s_in_head].key = key;
        s_in_head = next;
    }
}

static unsigned char convert_scancode_to_doom(uint8_t scancode) {
    switch (scancode) {
        case 0x1C: return KEY_ENTER;
        case 0x01: return KEY_ESCAPE;
        case 0x4B: return KEY_LEFTARROW;
        case 0x4D: return KEY_RIGHTARROW;
        case 0x48: return KEY_UPARROW;
        case 0x50: return KEY_DOWNARROW;
        case 0x1D: return KEY_FIRE;      // Left Ctrl
        case 0x39: return KEY_USE;       // Space
        case 0x2A: return KEY_RSHIFT;    // Left Shift
        case 0x36: return KEY_RSHIFT;    // Right Shift
        case 0x38: return KEY_LALT;      // Alt (Strafe)
        case 0x0F: return KEY_TAB;       // Tab (Map)
        case 0x0E: return KEY_BACKSPACE;

        case 0x10: return 'q';
        case 0x11: return 'w';
        case 0x12: return 'e';
        case 0x13: return 'r';
        case 0x14: return 't';
        case 0x15: return 'y';
        case 0x16: return 'u';
        case 0x17: return 'i';
        case 0x18: return 'o';
        case 0x19: return 'p';
        case 0x1E: return 'a';
        case 0x1F: return 's';
        case 0x20: return 'd';
        case 0x21: return 'f';
        case 0x22: return 'g';
        case 0x23: return 'h';
        case 0x24: return 'j';
        case 0x25: return 'k';
        case 0x26: return 'l';
        case 0x2C: return 'z';
        case 0x2D: return 'x';
        case 0x2E: return 'c';
        case 0x2F: return 'v';
        case 0x30: return 'b';
        case 0x31: return 'n';
        case 0x32: return 'm';

        case 0x02: return '1';
        case 0x03: return '2';
        case 0x04: return '3';
        case 0x05: return '4';
        case 0x06: return '5';
        case 0x07: return '6';
        case 0x08: return '7';
        case 0x09: return '8';
        case 0x0A: return '9';
        case 0x0B: return '0';
        case 0x0C: return '-';
        case 0x0D: return '=';

        case 0x33: return ',';
        case 0x34: return '.';
        case 0x35: return '/';

        case 0x3B: return KEY_F1;
        case 0x3C: return KEY_F2;
        case 0x3D: return KEY_F3;
        case 0x3E: return KEY_F4;
        case 0x3F: return KEY_F5;
        case 0x40: return KEY_F6;
        case 0x41: return KEY_F7;
        case 0x42: return KEY_F8;
        case 0x43: return KEY_F9;
        case 0x44: return KEY_F10;
        case 0x57: return KEY_F11;
        case 0x58: return KEY_F12;

        default: return 0;
    }
}

void DG_Init(void) {
    memset(s_compose_buf, 0, sizeof(s_compose_buf));

    // Check if PC mode was configured from shell or command line
    struct seld_stat pcmode_st;
    if (seld_stat("pcmode", &pcmode_st) == 0 || M_CheckParm("-pc") || M_CheckParm("-nopad")) {
        s_show_touch_hud = 0;
        printf("[+] DoomGeneric SeldOS: PC Mode active - Mobile touch HUD disabled.\n");
    }

    // Flush any stale keyboard and mouse events accumulated before DOOM launched
    struct seld_kbd_event kev;
    while (seld_poll_key(&kev) > 0);
    struct seld_mouse_event mev;
    while (seld_poll_mouse(&mev) > 0);

    if (seld_get_framebuffer(&s_fb) == 0 && s_fb.framebuffer != NULL) {
        s_fb_available = 1;
        printf("[+] DoomGeneric SeldOS: Framebuffer online (%dx%d, pitch=%d, bpp=%d)\n",
               s_fb.width, s_fb.height, s_fb.pitch, s_fb.bpp);
    } else {
        s_fb_available = 0;
        printf("[-] DoomGeneric SeldOS: Framebuffer not available!\n");
    }
}

static uint64_t s_frame_count = 0;
static uint64_t s_last_fps_time = 0;

void DG_DrawFrame(void) {
    if (!s_fb_available || !s_fb.framebuffer || !DG_ScreenBuffer) {
        return;
    }

    uint32_t* fb32 = (uint32_t*)s_fb.framebuffer;
    uint32_t fb_pitch_pixels = s_fb.pitch / 4;

    // 1. Composite clean background letterboxing bars
    const uint32_t dark_bar_color = 0x00141418;
    for (int y = 0; y < 32; y++) {
        uint32_t* line = s_compose_buf + y * COMP_W;
        for (int x = 0; x < COMP_W; x++) line[x] = dark_bar_color;
    }
    for (int y = 300; y < COMP_H; y++) {
        uint32_t* line = s_compose_buf + y * COMP_W;
        for (int x = 0; x < COMP_W; x++) line[x] = dark_bar_color;
    }
    for (int y = 32; y < 300; y++) {
        uint32_t* line = s_compose_buf + y * COMP_W;
        for (int x = 0; x < 20; x++) line[x] = dark_bar_color;
        for (int x = 660; x < COMP_W; x++) line[x] = dark_bar_color;
    }

    // 2. Composite DOOM 640x400 active playfield into lines 32..299
    for (int y = 0; y < 268; y++) {
        uint32_t* dst = s_compose_buf + (32 + y) * COMP_W + 20;
        int src_y = (y * DOOMGENERIC_RESY) / 268;
        if (src_y >= DOOMGENERIC_RESY) src_y = DOOMGENERIC_RESY - 1;
        uint32_t* src = DG_ScreenBuffer + src_y * DOOMGENERIC_RESX;
        memcpy(dst, src, DOOMGENERIC_RESX * sizeof(uint32_t));
    }

    // 3. Composite Touch Controls & Top Control Bar (Mobile Mode only)
    if (s_show_touch_hud) {
        draw_button(s_compose_buf, 10,  5, 55, 24, "ESC",  (s_active_touch_mask & (1 << T_ESC)),   0x00FF4444, 0x00FF2222);
        draw_button(s_compose_buf, 70,  5, 55, 24, "MAP",  (s_active_touch_mask & (1 << T_MAP)),   0x0044FFFF, 0x0000CCCC);
        draw_button(s_compose_buf, 130, 5, 65, 24, "ENTER",(s_active_touch_mask & (1 << T_ENTER)), 0x0044FF44, 0x0000CC00);
        draw_button(s_compose_buf, 200, 5, 65, 24, "< WPN", s_wpn_prev_active,                     0x00FFCC00, 0x00EEAA00);
        draw_button(s_compose_buf, 270, 5, 65, 24, "WPN >", s_wpn_next_active,                     0x00FFCC00, 0x00EEAA00);
        draw_button(s_compose_buf, 575, 5, 95, 24, "HUD:ON", 0,                                     0x00AAAAAA, 0x00555555);

        // D-Pad
        draw_button(s_compose_buf, 70,  175, 60, 42, "UP",   (s_active_touch_mask & (1 << T_UP)),       0x0000FFCC, 0x0000FFCC);
        draw_button(s_compose_buf, 70,  270, 60, 42, "DOWN", (s_active_touch_mask & (1 << T_DOWN)),     0x0000FFCC, 0x0000FFCC);
        draw_button(s_compose_buf, 10,  222, 55, 42, "LEFT", (s_active_touch_mask & (1 << T_LEFT)),     0x0000FFCC, 0x0000FFCC);
        draw_button(s_compose_buf, 135, 222, 55, 42, "RIGHT",(s_active_touch_mask & (1 << T_RIGHT)),    0x0000FFCC, 0x0000FFCC);
        draw_button(s_compose_buf, 10,  270, 55, 42, "< STR",(s_active_touch_mask & (1 << T_STRAFE_L)), 0x0000AACC, 0x0000AACC);
        draw_button(s_compose_buf, 135, 270, 55, 42, "STR >",(s_active_touch_mask & (1 << T_STRAFE_R)), 0x0000AACC, 0x0000AACC);

        // Action Buttons
        draw_button(s_compose_buf, 560, 218, 110, 62, "FIRE", (s_active_touch_mask & (1 << T_FIRE)), 0x00FF2222, 0x00FF3333);
        draw_button(s_compose_buf, 450, 225, 95,  52, "USE",  (s_active_touch_mask & (1 << T_USE)),  0x0022FF22, 0x0022DD22);
        draw_button(s_compose_buf, 560, 165, 100, 42, "RUN",  (s_active_touch_mask & (1 << T_RUN)),  0x002288FF, 0x000066FF);

        // Pointer / Crosshair Cursor
        if (s_mouse_x >= 0 && s_mouse_x < COMP_W && s_mouse_y >= 0 && s_mouse_y < COMP_H) {
            int mx = s_mouse_x;
            int my = s_mouse_y;
            uint32_t cur_col = s_is_touching ? 0x00FFFF00 : 0x0000FFFF;

            for (int d = -5; d <= 5; d++) {
                if (mx + d >= 0 && mx + d < COMP_W) {
                    s_compose_buf[my * COMP_W + (mx + d)] = cur_col;
                }
                if (my + d >= 0 && my + d < COMP_H) {
                    s_compose_buf[(my + d) * COMP_W + mx] = cur_col;
                }
            }
            s_compose_buf[my * COMP_W + mx] = 0x00FFFFFF;
        }
    }

    // 6. Fast Single-Pass Blit from offscreen compositing buffer to hardware VRAM
    for (int y = 0; y < COMP_H; y++) {
        uint32_t* dst = fb32 + y * fb_pitch_pixels;
        uint32_t* src = s_compose_buf + y * COMP_W;
        memcpy(dst, src, COMP_W * sizeof(uint32_t));
    }

    s_frame_count++;
    uint64_t now = seld_uptime();
    if (now - s_last_fps_time >= 2000) {
        printf("[*] DOOM Engine: %lu frames rendered (uptime %lu ms)\n", s_frame_count, now);
        s_last_fps_time = now;
    }
}

void DG_SleepMs(uint32_t ms) {
    seld_sleep(ms);
}

uint32_t DG_GetTicksMs(void) {
    return (uint32_t)seld_uptime();
}

static void update_touch_controls(void) {
    struct seld_mouse_event mev;
    while (seld_poll_mouse(&mev) > 0) {
        s_mouse_x = mev.x;
        s_mouse_y = mev.y;
        if (s_mouse_x < 0) s_mouse_x = 0;
        if (s_mouse_x >= COMP_W) s_mouse_x = COMP_W - 1;
        if (s_mouse_y < 0) s_mouse_y = 0;
        if (s_mouse_y >= COMP_H) s_mouse_y = COMP_H - 1;

        int is_touching = (mev.buttons & 1);
        int touch_press_edge = (!s_prev_touching && is_touching);

        // 1. Edge-triggered actions (HUD Toggle, Weapon Selection, Menu direct selection)
        if (touch_press_edge) {
            // HUD toggle button (x: 570..675, y: 0..32)
            if (s_mouse_x >= 570 && s_mouse_x <= 675 && s_mouse_y >= 0 && s_mouse_y <= 32) {
                s_show_touch_hud = !s_show_touch_hud;
            }

            // Weapon Cycle Down (< WPN, x: 200..268, y: 0..32)
            if (s_mouse_x >= 200 && s_mouse_x <= 268 && s_mouse_y >= 0 && s_mouse_y <= 32) {
                s_cur_weapon = (s_cur_weapon <= 1) ? 7 : (s_cur_weapon - 1);
                queue_doom_key(1, (unsigned char)('0' + s_cur_weapon));
                queue_doom_key(0, (unsigned char)('0' + s_cur_weapon));
                s_wpn_prev_active = 1;
            }

            // Weapon Cycle Up (WPN >, x: 270..338, y: 0..32)
            if (s_mouse_x >= 270 && s_mouse_x <= 338 && s_mouse_y >= 0 && s_mouse_y <= 32) {
                s_cur_weapon = (s_cur_weapon >= 7) ? 1 : (s_cur_weapon + 1);
                queue_doom_key(1, (unsigned char)('0' + s_cur_weapon));
                queue_doom_key(0, (unsigned char)('0' + s_cur_weapon));
                s_wpn_next_active = 1;
            }

            // Direct Touch Selection of DOOM Menu Items
            if (menuactive) {
                M_TouchSelect(s_mouse_x, s_mouse_y);
            }
        }

        if (!is_touching) {
            s_wpn_prev_active = 0;
            s_wpn_next_active = 0;
        }

        // 2. Level-triggered button hit testing (held down while touching)
        uint32_t new_mask = 0;

        if (is_touching) {
            int x = s_mouse_x;
            int y = s_mouse_y;

            // Top Control Bar
            if (y >= 0 && y <= 32) {
                if (x >= 10  && x <= 68)  new_mask |= (1 << T_ESC);
                if (x >= 70  && x <= 128) new_mask |= (1 << T_MAP);
                if (x >= 130 && x <= 198) new_mask |= (1 << T_ENTER);
            }

            // On-Screen Virtual Buttons
            if (s_show_touch_hud) {
                // D-Pad
                if (x >= 60  && x <= 140 && y >= 165 && y <= 225) new_mask |= (1 << T_UP);
                if (x >= 60  && x <= 140 && y >= 260 && y <= 325) new_mask |= (1 << T_DOWN);
                if (x >= 5   && x <= 70  && y >= 215 && y <= 270) new_mask |= (1 << T_LEFT);
                if (x >= 130 && x <= 195 && y >= 215 && y <= 270) new_mask |= (1 << T_RIGHT);
                if (x >= 5   && x <= 70  && y >= 260 && y <= 325) new_mask |= (1 << T_STRAFE_L);
                if (x >= 130 && x <= 195 && y >= 260 && y <= 325) new_mask |= (1 << T_STRAFE_R);

                // Actions
                if (x >= 550 && x <= 675 && y >= 210 && y <= 290) new_mask |= (1 << T_FIRE);
                if (x >= 440 && x <= 550 && y >= 220 && y <= 285) new_mask |= (1 << T_USE);
                if (x >= 550 && x <= 670 && y >= 155 && y <= 215) new_mask |= (1 << T_RUN);
            }
        }

        // Generate clean key-down and key-up transitions
        for (int i = 0; i < T_COUNT; i++) {
            int was_act = (s_active_touch_mask & (1 << i)) != 0;
            int is_act  = (new_mask & (1 << i)) != 0;
            if (!was_act && is_act) {
                queue_doom_key(1, s_touch_keys[i]);
            } else if (was_act && !is_act) {
                queue_doom_key(0, s_touch_keys[i]);
            }
        }

        s_active_touch_mask = new_mask;
        s_prev_touching = is_touching;
        s_is_touching = is_touching;
    }
}

int DG_GetKey(int* pressed, unsigned char* doomKey) {
    // 1. Process touch/mouse input
    update_touch_controls();

    // 2. Deliver simulated touch keys from queue
    if (s_in_head != s_in_tail) {
        *pressed = s_input_q[s_in_tail].pressed;
        *doomKey = s_input_q[s_in_tail].key;
        s_in_tail = (s_in_tail + 1) % INPUT_QUEUE_SIZE;
        return 1;
    }

    // 3. Process physical keyboard input
    struct seld_kbd_event ev;
    while (seld_poll_key(&ev) > 0) {
        unsigned char k = convert_scancode_to_doom(ev.scancode);
        if (k != 0) {
            *pressed = ev.pressed;
            *doomKey = k;
            return 1;
        }
    }

    return 0;
}

void DG_SetWindowTitle(const char * title) {
    (void)title;
}

int main(int argc, char** argv) {
    printf("[*] Starting DOOM on SeldOS (Humboldt Kernel)...\n");
    printf("[*] Touchscreen & Mouse HUD Virtual Controls Online.\n");
    doomgeneric_Create(argc, argv);

    while (1) {
        doomgeneric_Tick();
    }

    return 0;
}
