/*
 * SeldOS - Humboldt Kernel Project
 * /bin/less - Terminal & Framebuffer Text Pager
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#include "seld.h"
#include "font.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LESS_LINES 4096
#define MAX_LINE_COLS  128
#define PAGE_LINES     19
#define STATUS_ROW     19
#define COLS_TOTAL     85

static char* s_lines[MAX_LESS_LINES];
static int   s_line_count = 0;
static int   s_top_row = 0;
static char  s_filename[64] = "(stdin)";

static struct seld_fb_info s_fb;
static int s_has_fb = 0;

static void fb_fill_rect(int x, int y, int w, int h, uint32_t col) {
    if (!s_has_fb) return;
    int x2 = x + w;
    int y2 = y + h;
    if (x2 > (int)s_fb.width) x2 = s_fb.width;
    if (y2 > (int)s_fb.height) y2 = s_fb.height;
    uint32_t* fb = (uint32_t*)s_fb.framebuffer;
    uint32_t pitch_p = s_fb.pitch / 4;
    for (int cy = y; cy < y2; cy++) {
        uint32_t* row = fb + cy * pitch_p;
        for (int cx = x; cx < x2; cx++) {
            row[cx] = col;
        }
    }
}

static void fb_draw_char(int col, int row, char c, uint32_t fg, uint32_t bg) {
    if (!s_has_fb) return;
    if (col < 0 || col >= COLS_TOTAL || row < 0 || row >= 20) return;
    uint8_t uc = (uint8_t)c;
    int px_x = col * 8;
    int px_y = row * 16;
    uint32_t* fb = (uint32_t*)s_fb.framebuffer;
    uint32_t pitch_p = s_fb.pitch / 4;

    for (int y = 0; y < 16; y++) {
        uint8_t mask = font8x16[uc][y];
        uint32_t* dst = fb + (px_y + y) * pitch_p + px_x;
        for (int x = 0; x < 8; x++) {
            dst[x] = (mask & (0x80 >> x)) ? fg : bg;
        }
    }
}

static void fb_draw_str(int col, int row, const char* str, uint32_t fg, uint32_t bg) {
    while (*str && col < COLS_TOTAL) {
        fb_draw_char(col++, row, *str++, fg, bg);
    }
}

static void render_screen(void) {
    if (s_has_fb) {
        // Dark background #0E131B
        fb_fill_rect(0, 0, 680, 19 * 16, 0xFF0E131B);

        for (int r = 0; r < PAGE_LINES; r++) {
            int line_idx = s_top_row + r;
            if (line_idx < s_line_count && s_lines[line_idx]) {
                fb_draw_str(1, r, s_lines[line_idx], 0xFFECEFF4, 0xFF0E131B);
            } else {
                fb_draw_char(1, r, '~', 0xFF4A5568, 0xFF0E131B);
            }
        }

        // Status bar on row 19 (Cyan #4ECDC4 on dark #1E2530)
        fb_fill_rect(0, STATUS_ROW * 16, 680, 16, 0xFF1E2530);
        char status[128];
        int pct = (s_line_count > 0) ? ((s_top_row + PAGE_LINES) * 100 / s_line_count) : 100;
        if (pct > 100) pct = 100;
        snprintf(status, sizeof(status), ": %s (line %d-%d of %d, %d%%) [Space: Next, b: Back, q: Quit]",
                 s_filename, s_top_row + 1,
                 (s_top_row + PAGE_LINES > s_line_count) ? s_line_count : (s_top_row + PAGE_LINES),
                 s_line_count, pct);
        fb_draw_str(1, STATUS_ROW, status, 0xFF4ECDC4, 0xFF1E2530);
    } else {
        // Plain stdout fallback
        printf("\033[H\033[J");
        for (int r = 0; r < PAGE_LINES; r++) {
            int line_idx = s_top_row + r;
            if (line_idx < s_line_count && s_lines[line_idx]) {
                printf("%s\n", s_lines[line_idx]);
            }
        }
        printf("--- %s (%d/%d) [q: quit] ---\n", s_filename, s_top_row + 1, s_line_count);
        fflush(stdout);
    }
}

static void load_stream(int fd) {
    char buf[4096];
    char cur[MAX_LINE_COLS];
    size_t pos = 0;
    int r;

    while ((r = read(fd, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < r; i++) {
            char ch = buf[i];
            if (ch == '\r') continue;
            if (ch == '\n') {
                cur[pos] = '\0';
                if (s_line_count < MAX_LESS_LINES) {
                    s_lines[s_line_count] = (char*)malloc(pos + 1);
                    if (s_lines[s_line_count]) {
                        strcpy(s_lines[s_line_count], cur);
                        s_line_count++;
                    }
                }
                pos = 0;
            } else if (ch == '\t') {
                for (int t = 0; t < 4 && pos < MAX_LINE_COLS - 1; t++) {
                    cur[pos++] = ' ';
                }
            } else {
                if (pos < MAX_LINE_COLS - 1) {
                    cur[pos++] = ch;
                }
            }
        }
    }

    if (pos > 0 && s_line_count < MAX_LESS_LINES) {
        cur[pos] = '\0';
        s_lines[s_line_count] = (char*)malloc(pos + 1);
        if (s_lines[s_line_count]) {
            strcpy(s_lines[s_line_count], cur);
            s_line_count++;
        }
    }

    if (s_line_count == 0) {
        s_lines[0] = (char*)malloc(1);
        if (s_lines[0]) s_lines[0][0] = '\0';
        s_line_count = 1;
    }
}

int main(int argc, char* argv[]) {
    seld_pledge(PLEDGE_STDIO | PLEDGE_RPATH);
    seld_unveil("/", "r");
    seld_unveil(NULL, NULL);

    int fd = 0;
    if (argc >= 2 && strcmp(argv[1], "-") != 0) {
        strncpy(s_filename, argv[1], sizeof(s_filename) - 1);
        s_filename[sizeof(s_filename) - 1] = '\0';
        fd = open(s_filename, 0 /* O_RDONLY */);
        if (fd < 0) {
            printf("less: %s: No such file or directory\n", s_filename);
            return 1;
        }
    }

    load_stream(fd);
    if (fd > 0) close(fd);

    if (seld_get_framebuffer(&s_fb) == 0 && s_fb.framebuffer && s_fb.bpp == 32) {
        s_has_fb = 1;
    }

    render_screen();

    // Event Loop
    int running = 1;
    while (running) {
        struct seld_kbd_event kev;
        if (seld_poll_key(&kev) <= 0) {
            seld_sleep(15);
            continue;
        }

        if (!kev.pressed) continue;
        uint8_t sc = kev.scancode;

        if (sc == 0x01 || sc == 0x10) { /* ESC or 'q' */
            running = 0;
            break;
        } else if (sc == 0x39 || sc == 0x21 || sc == 0x51) { /* Space / 'f' / PageDown */
            s_top_row += PAGE_LINES;
            if (s_top_row >= s_line_count) s_top_row = s_line_count - 1;
            if (s_top_row < 0) s_top_row = 0;
        } else if (sc == 0x30 || sc == 0x49) { /* 'b' / PageUp */
            s_top_row -= PAGE_LINES;
            if (s_top_row < 0) s_top_row = 0;
        } else if (sc == 0x24 || sc == 0x50) { /* 'j' / Down Arrow */
            if (s_top_row < s_line_count - 1) s_top_row++;
        } else if (sc == 0x25 || sc == 0x48) { /* 'k' / Up Arrow */
            if (s_top_row > 0) s_top_row--;
        } else if (sc == 0x22 || sc == 0x47) { /* 'g' / Home */
            s_top_row = 0;
        } else if (sc == 0x4F) { /* End */
            s_top_row = s_line_count - PAGE_LINES;
            if (s_top_row < 0) s_top_row = 0;
        }

        render_screen();
    }

    seld_clear();
    return 0;
}
