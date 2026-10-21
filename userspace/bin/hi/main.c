/*
 * SeldOS - Humboldt Kernel Project
 * /bin/hi - Humboldt Interactive Modal Text Editor (Vi Tribute)
 *
 * Dedicated to Spheniscus Humboldti (Humboldt Penguin Mascot)
 * Framebuffer 680x334 Resolution | 85x20 Grid | 39-Color Humboldt Palette
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <seld.h>
#include <font.h>

#define SCR_W 680
#define SCR_H 334
#define CHAR_W 8
#define CHAR_H 16

#define COLS_TOTAL 85
#define ROWS_TOTAL 20

#define GUTTER_WIDTH 5    /* " %3d |" */
#define TEXT_COLS (COLS_TOTAL - GUTTER_WIDTH) /* 80 columns */
#define EDIT_ROWS 19      /* Rows 0..18 for editing */
#define STATUS_ROW 19     /* Row 19 for status bar */

#define MAX_LINES 2048
#define MAX_LINE_LEN 256

/* Humboldt 39-Color Tokens */
#define COL_BG           0xFF18181A  /* Charcoal Main Background */
#define COL_FG           0xFFF2F4F8  /* Clean White Glyph */
#define COL_GUTTER_BG    0xFF141C27  /* Deep Slate Gutter */
#define COL_GUTTER_FG    0xFF64748B  /* Muted Slate Line Numbers */
#define COL_GUTTER_DIV   0xFF38BDF8  /* Cyan Divider Bar */
#define COL_STATUS_BG    0xFF2A2D34  /* Dark Slate Status Bar */
#define COL_STATUS_FG    0xFFF8FAFC  /* White Status Text */
#define COL_STATUS_MUTED 0xFF94A3B8  /* Muted Status Text */
#define COL_MODE_NORMAL  0xFF38BDF8  /* Cyan [NORMAL] */
#define COL_MODE_INSERT  0xFF10B981  /* Green [INSERT] */
#define COL_MODE_CMD     0xFFF59E0B  /* Amber [COMMAND] */
#define COL_CURSOR_NORM  0xFFE0533C  /* Coral Red Block Cursor */
#define COL_CURSOR_INS   0xFF10B981  /* Emerald Insert Cursor */
#define COL_MATCH_BG     0xFFF59E0B  /* Highlight Search Match */
#define COL_MATCH_FG     0xFF18181A  /* Text inside highlight */
#define COL_DIRTY        0xFFEF4444  /* Red Modified Flag */

typedef enum {
    MODE_NORMAL = 0,
    MODE_INSERT,
    MODE_COMMAND,
    MODE_SEARCH
} editor_mode_t;

typedef struct {
    char text[MAX_LINE_LEN];
    int len;
} line_t;

/* Editor State */
static line_t s_lines[MAX_LINES];
static int s_line_count = 1;

static int s_cursor_row = 0;
static int s_cursor_col = 0;
static int s_top_row = 0;
static int s_left_col = 0;

static editor_mode_t s_mode = MODE_NORMAL;
static char s_filename[64] = "untitled.txt";
static int s_modified = 0;

static char s_cmd_buf[64];
static int s_cmd_len = 0;

static char s_search_buf[64];
static int s_search_len = 0;

static char s_status_msg[128] = "";
static char s_last_pending_cmd = 0;

/* Framebuffer Mapping */
static struct seld_fb_info s_fb;
static int s_has_fb = 0;

/* Single-level Undo Buffer */
static line_t s_undo_lines[MAX_LINES];
static int s_undo_line_count = 1;
static int s_has_undo = 0;

/* Scancode to ASCII tables */
static const char s_sc_lower[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static const char s_sc_upper[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static int s_shift_state = 0;
static int s_caps_state = 0;

/* --------------------------------------------------------------------------
 * Rendering Primitives
 * -------------------------------------------------------------------------- */

static void fb_fill_rect(int x, int y, int w, int h, uint32_t col) {
    if (!s_has_fb) return;
    int x2 = x + w;
    int y2 = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x2 > SCR_W) x2 = SCR_W;
    if (y2 > SCR_H) y2 = SCR_H;

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
    if (!s_has_fb || col < 0 || col >= COLS_TOTAL || row < 0 || row >= ROWS_TOTAL) return;
    uint32_t* fb = (uint32_t*)s_fb.framebuffer;
    uint32_t pitch_p = s_fb.pitch / 4;
    int start_x = col * CHAR_W;
    int start_y = row * CHAR_H;

    const uint8_t* glyph = font8x16[(uint8_t)c];
    for (int y = 0; y < 16; y++) {
        uint8_t bits = glyph[y];
        uint32_t* dst = fb + (start_y + y) * pitch_p + start_x;
        for (int x = 0; x < 8; x++) {
            dst[x] = (bits & (0x80 >> x)) ? fg : bg;
        }
    }
}

static void fb_draw_str(int col, int row, const char* str, uint32_t fg, uint32_t bg) {
    while (*str && col < COLS_TOTAL) {
        fb_draw_char(col++, row, *str++, fg, bg);
    }
}

/* --------------------------------------------------------------------------
 * State Management & Undo
 * -------------------------------------------------------------------------- */

static void save_undo_snapshot(void) {
    for (int i = 0; i < s_line_count; i++) {
        s_undo_lines[i] = s_lines[i];
    }
    s_undo_line_count = s_line_count;
    s_has_undo = 1;
}

static void restore_undo_snapshot(void) {
    if (!s_has_undo) {
        snprintf(s_status_msg, sizeof(s_status_msg), "Already at oldest change");
        return;
    }
    for (int i = 0; i < s_undo_line_count; i++) {
        s_lines[i] = s_undo_lines[i];
    }
    s_line_count = s_undo_line_count;
    s_has_undo = 0;
    if (s_cursor_row >= s_line_count) s_cursor_row = s_line_count - 1;
    snprintf(s_status_msg, sizeof(s_status_msg), "Undo applied");
}

/* --------------------------------------------------------------------------
 * File I/O
 * -------------------------------------------------------------------------- */

static int load_file(const char* path) {
    char* buf = (char*)malloc(128 * 1024);
    if (!buf) return -1;

    int bytes = seld_readfile(path, buf, (128 * 1024) - 1);
    if (bytes <= 0) {
        free(buf);
        s_line_count = 1;
        s_lines[0].text[0] = '\0';
        s_lines[0].len = 0;
        s_modified = 0;
        snprintf(s_status_msg, sizeof(s_status_msg), "\"%s\" [New File]", path);
        return 0;
    }

    buf[bytes] = '\0';
    s_line_count = 0;

    int cur_len = 0;
    char cur_line[MAX_LINE_LEN];

    for (int i = 0; i < bytes; i++) {
        char ch = buf[i];
        if (ch == '\r') continue;
        if (ch == '\n') {
            cur_line[cur_len] = '\0';
            if (s_line_count < MAX_LINES) {
                strncpy(s_lines[s_line_count].text, cur_line, MAX_LINE_LEN - 1);
                s_lines[s_line_count].text[MAX_LINE_LEN - 1] = '\0';
                s_lines[s_line_count].len = cur_len;
                s_line_count++;
            }
            cur_len = 0;
        } else {
            if (cur_len < MAX_LINE_LEN - 1) {
                cur_line[cur_len++] = ch;
            }
        }
    }

    if (cur_len > 0 || s_line_count == 0) {
        cur_line[cur_len] = '\0';
        if (s_line_count < MAX_LINES) {
            strncpy(s_lines[s_line_count].text, cur_line, MAX_LINE_LEN - 1);
            s_lines[s_line_count].text[MAX_LINE_LEN - 1] = '\0';
            s_lines[s_line_count].len = cur_len;
            s_line_count++;
        }
    }

    free(buf);
    s_modified = 0;
    snprintf(s_status_msg, sizeof(s_status_msg), "\"%s\" %d lines, %d bytes loaded", path, s_line_count, bytes);
    return 0;
}

static int save_file(const char* path) {
    size_t total_alloc = 0;
    for (int i = 0; i < s_line_count; i++) {
        total_alloc += s_lines[i].len + 1;
    }
    total_alloc += 16;

    char* buf = (char*)malloc(total_alloc);
    if (!buf) {
        snprintf(s_status_msg, sizeof(s_status_msg), "E35: Out of memory allocating save buffer");
        return -1;
    }

    size_t offset = 0;
    for (int i = 0; i < s_line_count; i++) {
        memcpy(buf + offset, s_lines[i].text, s_lines[i].len);
        offset += s_lines[i].len;
        buf[offset++] = '\n';
    }

    int res = (int)seld_syscall(SYS_WRITEFILE, (long)path, (long)buf, (long)offset);
    free(buf);

    if (res < 0) {
        snprintf(s_status_msg, sizeof(s_status_msg), "E212: Can't open file for writing: %s", path);
        return -1;
    }

    s_modified = 0;
    snprintf(s_status_msg, sizeof(s_status_msg), "\"%s\" %d lines, %d bytes written", path, s_line_count, (int)offset);
    return 0;
}

/* --------------------------------------------------------------------------
 * Buffer Editing Operations
 * -------------------------------------------------------------------------- */

static void insert_char(char c) {
    if (s_cursor_row >= s_line_count) return;
    line_t* line = &s_lines[s_cursor_row];
    if (line->len >= MAX_LINE_LEN - 1) return;

    save_undo_snapshot();
    for (int i = line->len; i > s_cursor_col; i--) {
        line->text[i] = line->text[i - 1];
    }
    line->text[s_cursor_col] = c;
    line->len++;
    line->text[line->len] = '\0';
    s_cursor_col++;
    s_modified = 1;
}

static void delete_char(void) {
    if (s_cursor_row >= s_line_count) return;
    line_t* line = &s_lines[s_cursor_row];
    if (s_cursor_col >= line->len) return;

    save_undo_snapshot();
    for (int i = s_cursor_col; i < line->len - 1; i++) {
        line->text[i] = line->text[i + 1];
    }
    line->len--;
    line->text[line->len] = '\0';
    s_modified = 1;
}

static void backspace_char(void) {
    if (s_cursor_col > 0) {
        s_cursor_col--;
        delete_char();
    } else if (s_cursor_row > 0) {
        /* Join current line with previous line */
        save_undo_snapshot();
        line_t* prev = &s_lines[s_cursor_row - 1];
        line_t* cur = &s_lines[s_cursor_row];
        int prev_len = prev->len;

        if (prev_len + cur->len < MAX_LINE_LEN - 1) {
            memcpy(prev->text + prev_len, cur->text, cur->len);
            prev->len += cur->len;
            prev->text[prev->len] = '\0';

            /* Shift subsequent lines up */
            for (int r = s_cursor_row; r < s_line_count - 1; r++) {
                s_lines[r] = s_lines[r + 1];
            }
            s_line_count--;
            s_cursor_row--;
            s_cursor_col = prev_len;
            s_modified = 1;
        }
    }
}

static void split_line(void) {
    if (s_line_count >= MAX_LINES - 1) return;
    save_undo_snapshot();

    line_t* cur = &s_lines[s_cursor_row];
    int split_idx = s_cursor_col;
    int tail_len = cur->len - split_idx;

    /* Shift lines down */
    for (int r = s_line_count; r > s_cursor_row + 1; r--) {
        s_lines[r] = s_lines[r - 1];
    }
    s_line_count++;

    line_t* next = &s_lines[s_cursor_row + 1];
    memcpy(next->text, cur->text + split_idx, tail_len);
    next->len = tail_len;
    next->text[tail_len] = '\0';

    cur->len = split_idx;
    cur->text[split_idx] = '\0';

    s_cursor_row++;
    s_cursor_col = 0;
    s_modified = 1;
}

static void delete_current_line(void) {
    if (s_line_count <= 1) {
        save_undo_snapshot();
        s_lines[0].text[0] = '\0';
        s_lines[0].len = 0;
        s_cursor_col = 0;
        s_modified = 1;
        return;
    }

    save_undo_snapshot();
    for (int r = s_cursor_row; r < s_line_count - 1; r++) {
        s_lines[r] = s_lines[r + 1];
    }
    s_line_count--;
    if (s_cursor_row >= s_line_count) {
        s_cursor_row = s_line_count - 1;
    }
    s_cursor_col = 0;
    s_modified = 1;
}

static void open_line_below(void) {
    if (s_line_count >= MAX_LINES - 1) return;
    save_undo_snapshot();

    for (int r = s_line_count; r > s_cursor_row + 1; r--) {
        s_lines[r] = s_lines[r - 1];
    }
    s_line_count++;
    s_lines[s_cursor_row + 1].text[0] = '\0';
    s_lines[s_cursor_row + 1].len = 0;

    s_cursor_row++;
    s_cursor_col = 0;
    s_mode = MODE_INSERT;
    s_modified = 1;
}

static void open_line_above(void) {
    if (s_line_count >= MAX_LINES - 1) return;
    save_undo_snapshot();

    for (int r = s_line_count; r > s_cursor_row; r--) {
        s_lines[r] = s_lines[r - 1];
    }
    s_line_count++;
    s_lines[s_cursor_row].text[0] = '\0';
    s_lines[s_cursor_row].len = 0;

    s_cursor_col = 0;
    s_mode = MODE_INSERT;
    s_modified = 1;
}

/* --------------------------------------------------------------------------
 * Search Engine
 * -------------------------------------------------------------------------- */

static void search_forward(void) {
    if (s_search_len == 0) return;

    int start_row = s_cursor_row;
    int start_col = s_cursor_col + 1;

    for (int r = 0; r < s_line_count; r++) {
        int check_row = (start_row + r) % s_line_count;
        const char* text = s_lines[check_row].text;
        int line_len = s_lines[check_row].len;

        int c_start = (r == 0) ? start_col : 0;
        if (c_start >= line_len) continue;

        char* match = strstr(text + c_start, s_search_buf);
        if (match) {
            s_cursor_row = check_row;
            s_cursor_col = (int)(match - text);
            snprintf(s_status_msg, sizeof(s_status_msg), "/%s", s_search_buf);
            return;
        }
    }
    snprintf(s_status_msg, sizeof(s_status_msg), "E486: Pattern not found: %s", s_search_buf);
}

/* --------------------------------------------------------------------------
 * Viewport & Cursor Constraints
 * -------------------------------------------------------------------------- */

static void adjust_viewport(void) {
    if (s_cursor_row < 0) s_cursor_row = 0;
    if (s_cursor_row >= s_line_count) s_cursor_row = s_line_count - 1;

    int cur_len = s_lines[s_cursor_row].len;
    if (s_mode == MODE_NORMAL) {
        if (s_cursor_col >= cur_len && cur_len > 0) {
            s_cursor_col = cur_len - 1;
        }
        if (s_cursor_col < 0) s_cursor_col = 0;
    } else {
        if (s_cursor_col > cur_len) s_cursor_col = cur_len;
        if (s_cursor_col < 0) s_cursor_col = 0;
    }

    /* Vertical Scroll */
    if (s_cursor_row < s_top_row) {
        s_top_row = s_cursor_row;
    }
    if (s_cursor_row >= s_top_row + EDIT_ROWS) {
        s_top_row = s_cursor_row - EDIT_ROWS + 1;
    }

    /* Horizontal Scroll */
    if (s_cursor_col < s_left_col) {
        s_left_col = s_cursor_col;
    }
    if (s_cursor_col >= s_left_col + TEXT_COLS) {
        s_left_col = s_cursor_col - TEXT_COLS + 1;
    }
}

/* --------------------------------------------------------------------------
 * Screen Drawing
 * -------------------------------------------------------------------------- */

static void render_screen(void) {
    if (!s_has_fb) return;

    adjust_viewport();

    /* 1. Clear background */
    fb_fill_rect(0, 0, SCR_W, EDIT_ROWS * CHAR_H, COL_BG);

    /* 2. Render Text Lines & Line Numbers */
    char gutter_buf[16];
    for (int r = 0; r < EDIT_ROWS; r++) {
        int file_line_idx = s_top_row + r;
        int screen_y = r;

        if (file_line_idx < s_line_count) {
            /* Gutter: line number */
            snprintf(gutter_buf, sizeof(gutter_buf), "%3d |", file_line_idx + 1);
            fb_fill_rect(0, screen_y * CHAR_H, GUTTER_WIDTH * CHAR_W, CHAR_H, COL_GUTTER_BG);
            fb_draw_str(0, screen_y, gutter_buf, (file_line_idx == s_cursor_row) ? COL_GUTTER_DIV : COL_GUTTER_FG, COL_GUTTER_BG);

            /* Line Text */
            line_t* line = &s_lines[file_line_idx];
            int vis_start = s_left_col;
            if (vis_start < line->len) {
                int draw_len = line->len - vis_start;
                if (draw_len > TEXT_COLS) draw_len = TEXT_COLS;

                for (int c = 0; c < draw_len; c++) {
                    int char_idx = vis_start + c;
                    char ch = line->text[char_idx];
                    int screen_col = GUTTER_WIDTH + c;

                    /* Search highlight */
                    int is_search_match = 0;
                    if (s_search_len > 0) {
                        for (int k = 0; k < s_search_len; k++) {
                            if (char_idx >= k && strncmp(&line->text[char_idx - k], s_search_buf, s_search_len) == 0) {
                                is_search_match = 1;
                                break;
                            }
                        }
                    }

                    if (is_search_match) {
                        fb_draw_char(screen_col, screen_y, ch, COL_MATCH_FG, COL_MATCH_BG);
                    } else {
                        fb_draw_char(screen_col, screen_y, ch, COL_FG, COL_BG);
                    }
                }
            }
        } else {
            /* Tilde empty lines like Vim */
            fb_fill_rect(0, screen_y * CHAR_H, GUTTER_WIDTH * CHAR_W, CHAR_H, COL_GUTTER_BG);
            fb_draw_char(1, screen_y, '~', COL_GUTTER_FG, COL_GUTTER_BG);
            fb_draw_char(4, screen_y, '|', COL_GUTTER_FG, COL_GUTTER_BG);
        }
    }

    /* 3. Render Cursor */
    int cursor_scr_row = s_cursor_row - s_top_row;
    int cursor_scr_col = GUTTER_WIDTH + (s_cursor_col - s_left_col);

    if (cursor_scr_row >= 0 && cursor_scr_row < EDIT_ROWS &&
        cursor_scr_col >= GUTTER_WIDTH && cursor_scr_col < COLS_TOTAL) {

        char cur_char = ' ';
        if (s_cursor_row < s_line_count && s_cursor_col < s_lines[s_cursor_row].len) {
            cur_char = s_lines[s_cursor_row].text[s_cursor_col];
        }

        uint32_t cur_color = (s_mode == MODE_INSERT) ? COL_CURSOR_INS : COL_CURSOR_NORM;
        fb_draw_char(cursor_scr_col, cursor_scr_row, cur_char, COL_BG, cur_color);
    }

    /* 4. Render Status Bar (Row 19) */
    fb_fill_rect(0, STATUS_ROW * CHAR_H, SCR_W, CHAR_H, COL_STATUS_BG);

    /* Left: Mode Badge */
    char badge[32];
    uint32_t badge_col = COL_MODE_NORMAL;
    if (s_mode == MODE_NORMAL) {
        snprintf(badge, sizeof(badge), " -- NORMAL -- ");
        badge_col = COL_MODE_NORMAL;
    } else if (s_mode == MODE_INSERT) {
        snprintf(badge, sizeof(badge), " -- INSERT -- ");
        badge_col = COL_MODE_INSERT;
    } else if (s_mode == MODE_COMMAND) {
        snprintf(badge, sizeof(badge), ":%s", s_cmd_buf);
        badge_col = COL_MODE_CMD;
    } else if (s_mode == MODE_SEARCH) {
        snprintf(badge, sizeof(badge), "/%s", s_search_buf);
        badge_col = COL_MATCH_BG;
    }

    fb_draw_str(1, STATUS_ROW, badge, badge_col, COL_STATUS_BG);

    /* Center: Filename & Modified state */
    char file_info[64];
    snprintf(file_info, sizeof(file_info), "\"%s\" %s", s_filename, s_modified ? "[+]" : "");
    fb_draw_str(22, STATUS_ROW, file_info, s_modified ? COL_DIRTY : COL_STATUS_FG, COL_STATUS_BG);

    /* Status notification message if any */
    if (s_status_msg[0] != '\0' && s_mode == MODE_NORMAL) {
        fb_draw_str(42, STATUS_ROW, s_status_msg, COL_STATUS_MUTED, COL_STATUS_BG);
    }

    /* Right: Position & Penguin Mascot Badge */
    char pos_info[32];
    snprintf(pos_info, sizeof(pos_info), "%d,%d  %s", s_cursor_row + 1, s_cursor_col + 1, "[hi/85x20]");
    int pos_col = COLS_TOTAL - (int)strlen(pos_info) - 1;
    fb_draw_str(pos_col, STATUS_ROW, pos_info, COL_GUTTER_DIV, COL_STATUS_BG);

    /* 5. Clear bottom guard band (320..334px) */
    fb_fill_rect(0, ROWS_TOTAL * CHAR_H, SCR_W, SCR_H - (ROWS_TOTAL * CHAR_H), 0xFF0E131B);
}

/* --------------------------------------------------------------------------
 * Command Mode Dispatcher
 * -------------------------------------------------------------------------- */

static int handle_command(void) {
    if (strcmp(s_cmd_buf, "q") == 0) {
        if (s_modified) {
            snprintf(s_status_msg, sizeof(s_status_msg), "No write since last change (use :q! to override)");
            return 0;
        }
        return 1; /* Exit */
    } else if (strcmp(s_cmd_buf, "q!") == 0) {
        return 1; /* Force Exit */
    } else if (strcmp(s_cmd_buf, "w") == 0) {
        save_file(s_filename);
        return 0;
    } else if (strncmp(s_cmd_buf, "w ", 2) == 0) {
        const char* target = s_cmd_buf + 2;
        while (*target == ' ') target++;
        if (*target) {
            strncpy(s_filename, target, sizeof(s_filename) - 1);
            s_filename[sizeof(s_filename) - 1] = '\0';
            save_file(s_filename);
        }
        return 0;
    } else if (strcmp(s_cmd_buf, "wq") == 0 || strcmp(s_cmd_buf, "x") == 0) {
        if (save_file(s_filename) == 0) {
            return 1; /* Exit on success */
        }
        return 0;
    } else if (strcmp(s_cmd_buf, "h") == 0 || strcmp(s_cmd_buf, "help") == 0) {
        snprintf(s_status_msg, sizeof(s_status_msg), "hi: :w save | :q quit | :wq save&quit | i insert | ESC norm");
        return 0;
    } else {
        snprintf(s_status_msg, sizeof(s_status_msg), "E492: Not an editor command: %s", s_cmd_buf);
        return 0;
    }
}

/* --------------------------------------------------------------------------
 * Main Event Loop
 * -------------------------------------------------------------------------- */

int main(int argc, char* argv[]) {
    // 1. Restrict capabilities with Seld-Pledge
    seld_pledge(PLEDGE_STDIO | PLEDGE_RPATH | PLEDGE_WPATH);
    seld_unveil("/", "rwc");
    seld_unveil(NULL, NULL);

    // 2. Map framebuffer
    if (seld_get_framebuffer(&s_fb) == 0 && s_fb.framebuffer && s_fb.bpp == 32) {
        s_has_fb = 1;
    }

    if (!s_has_fb) {
        printf("[-] hi: requires 32-bit Humboldt graphical framebuffer.\n");
        return -1;
    }

    // 3. Load file if supplied
    if (argc >= 2 && argv[1] && argv[1][0] != '\0') {
        strncpy(s_filename, argv[1], sizeof(s_filename) - 1);
        s_filename[sizeof(s_filename) - 1] = '\0';
    }

    load_file(s_filename);
    render_screen();

    // 4. Input Processing Loop
    int running = 1;
    while (running) {
        struct seld_kbd_event kev;
        if (seld_poll_key(&kev) <= 0) {
            seld_sleep(15);
            continue;
        }

        uint8_t sc = kev.scancode;
        int is_press = kev.pressed;

        /* Left Shift (0x2A): Dead Man's Grip ONLY. Does not affect typing modifier */
        if (sc == 0x2A) {
            continue;
        }

        /* Right Shift (0x36): Typing modifier for Caps & symbols */
        if (sc == 0x36) {
            s_shift_state = is_press;
            continue;
        }

        /* Toggle Caps Lock on press */
        if (sc == 0x3A && is_press) {
            s_caps_state = !s_caps_state;
            continue;
        }

        if (!is_press) {
            continue;
        }

        /* Resolve ASCII char */
        uint8_t clean_sc = sc & 0x7F;
        char ascii_ch = 0;
        if (clean_sc < 128) {
            char base_ch = s_sc_lower[clean_sc];
            int is_alpha = (base_ch >= 'a' && base_ch <= 'z');
            int use_upper = is_alpha ? (s_shift_state ^ s_caps_state) : s_shift_state;
            ascii_ch = use_upper ? s_sc_upper[clean_sc] : base_ch;
        }

        /* ------------------------------------------------------------------
         * Mode: INSERT
         * ------------------------------------------------------------------ */
        if (s_mode == MODE_INSERT) {
            if (clean_sc == 0x01) { /* ESC */
                s_mode = MODE_NORMAL;
                if (s_cursor_col > 0) s_cursor_col--;
                s_status_msg[0] = '\0';
            } else if (clean_sc == 0x0E) { /* Backspace */
                backspace_char();
            } else if (clean_sc == 0x1C) { /* Enter */
                split_line();
            } else if (clean_sc == 0x0F) { /* Tab */
                insert_char(' ');
                insert_char(' ');
                insert_char(' ');
                insert_char(' ');
            } else if (clean_sc == 0x4B) { /* Left Arrow */
                if (s_cursor_col > 0) s_cursor_col--;
            } else if (clean_sc == 0x4D) { /* Right Arrow */
                if (s_cursor_col < s_lines[s_cursor_row].len) s_cursor_col++;
            } else if (clean_sc == 0x48) { /* Up Arrow */
                if (s_cursor_row > 0) s_cursor_row--;
            } else if (clean_sc == 0x50) { /* Down Arrow */
                if (s_cursor_row < s_line_count - 1) s_cursor_row++;
            } else if (ascii_ch >= 32 && ascii_ch <= 126) {
                insert_char(ascii_ch);
            }
            render_screen();
            continue;
        }

        /* ------------------------------------------------------------------
         * Mode: COMMAND (:)
         * ------------------------------------------------------------------ */
        if (s_mode == MODE_COMMAND) {
            if (clean_sc == 0x01) { /* ESC: Cancel command */
                s_mode = MODE_NORMAL;
                s_cmd_buf[0] = '\0';
                s_cmd_len = 0;
                s_status_msg[0] = '\0';
            } else if (clean_sc == 0x0E) { /* Backspace */
                if (s_cmd_len > 0) {
                    s_cmd_buf[--s_cmd_len] = '\0';
                } else {
                    s_mode = MODE_NORMAL;
                }
            } else if (clean_sc == 0x1C) { /* Enter: Execute command */
                s_mode = MODE_NORMAL;
                if (handle_command()) {
                    running = 0;
                    break;
                }
                s_cmd_buf[0] = '\0';
                s_cmd_len = 0;
            } else if (ascii_ch >= 32 && ascii_ch <= 126 && s_cmd_len < (int)sizeof(s_cmd_buf) - 1) {
                s_cmd_buf[s_cmd_len++] = ascii_ch;
                s_cmd_buf[s_cmd_len] = '\0';
            }
            render_screen();
            continue;
        }

        /* ------------------------------------------------------------------
         * Mode: SEARCH (/)
         * ------------------------------------------------------------------ */
        if (s_mode == MODE_SEARCH) {
            if (clean_sc == 0x01) { /* ESC */
                s_mode = MODE_NORMAL;
                s_search_buf[0] = '\0';
                s_search_len = 0;
                s_status_msg[0] = '\0';
            } else if (clean_sc == 0x0E) { /* Backspace */
                if (s_search_len > 0) {
                    s_search_buf[--s_search_len] = '\0';
                } else {
                    s_mode = MODE_NORMAL;
                }
            } else if (clean_sc == 0x1C) { /* Enter: Run search */
                s_mode = MODE_NORMAL;
                search_forward();
            } else if (ascii_ch >= 32 && ascii_ch <= 126 && s_search_len < (int)sizeof(s_search_buf) - 1) {
                s_search_buf[s_search_len++] = ascii_ch;
                s_search_buf[s_search_len] = '\0';
            }
            render_screen();
            continue;
        }

        /* ------------------------------------------------------------------
         * Mode: NORMAL
         * ------------------------------------------------------------------ */
        s_status_msg[0] = '\0';

        /* Arrow Keys */
        if (clean_sc == 0x4B || ascii_ch == 'h') { /* Left */
            if (s_cursor_col > 0) s_cursor_col--;
        } else if (clean_sc == 0x4D || ascii_ch == 'l') { /* Right */
            if (s_cursor_col < s_lines[s_cursor_row].len - 1) s_cursor_col++;
        } else if (clean_sc == 0x48 || ascii_ch == 'k') { /* Up */
            if (s_cursor_row > 0) s_cursor_row--;
        } else if (clean_sc == 0x50 || ascii_ch == 'j') { /* Down */
            if (s_cursor_row < s_line_count - 1) s_cursor_row++;
        }
        /* Mode transitions */
        else if (ascii_ch == 'i') {
            s_mode = MODE_INSERT;
        } else if (ascii_ch == 'a') {
            if (s_cursor_col < s_lines[s_cursor_row].len) s_cursor_col++;
            s_mode = MODE_INSERT;
        } else if (ascii_ch == 'I') {
            s_cursor_col = 0;
            s_mode = MODE_INSERT;
        } else if (ascii_ch == 'A') {
            s_cursor_col = s_lines[s_cursor_row].len;
            s_mode = MODE_INSERT;
        } else if (ascii_ch == 'o') {
            open_line_below();
        } else if (ascii_ch == 'O') {
            open_line_above();
        } else if (ascii_ch == ':') {
            s_mode = MODE_COMMAND;
            s_cmd_buf[0] = '\0';
            s_cmd_len = 0;
        } else if (ascii_ch == '/') {
            s_mode = MODE_SEARCH;
            s_search_buf[0] = '\0';
            s_search_len = 0;
        } else if (ascii_ch == 'n') {
            search_forward();
        }
        /* Navigation */
        else if (ascii_ch == '0' || clean_sc == 0x47) { /* 0 / Home */
            s_cursor_col = 0;
        } else if (ascii_ch == '$' || clean_sc == 0x4F) { /* $ / End */
            s_cursor_col = s_lines[s_cursor_row].len > 0 ? s_lines[s_cursor_row].len - 1 : 0;
        } else if (ascii_ch == 'G') { /* Bottom */
            s_cursor_row = s_line_count - 1;
            s_cursor_col = 0;
        } else if (clean_sc == 0x49) { /* Page Up */
            s_cursor_row -= EDIT_ROWS;
            if (s_cursor_row < 0) s_cursor_row = 0;
        } else if (clean_sc == 0x51) { /* Page Down */
            s_cursor_row += EDIT_ROWS;
            if (s_cursor_row >= s_line_count) s_cursor_row = s_line_count - 1;
        }
        /* Word navigation */
        else if (ascii_ch == 'w') {
            line_t* line = &s_lines[s_cursor_row];
            int c = s_cursor_col;
            while (c < line->len && line->text[c] != ' ') c++;
            while (c < line->len && line->text[c] == ' ') c++;
            if (c < line->len) {
                s_cursor_col = c;
            } else if (s_cursor_row < s_line_count - 1) {
                s_cursor_row++;
                s_cursor_col = 0;
            }
        } else if (ascii_ch == 'b') {
            if (s_cursor_col > 0) {
                line_t* line = &s_lines[s_cursor_row];
                int c = s_cursor_col - 1;
                while (c > 0 && line->text[c] == ' ') c--;
                while (c > 0 && line->text[c - 1] != ' ') c--;
                s_cursor_col = c;
            } else if (s_cursor_row > 0) {
                s_cursor_row--;
                s_cursor_col = s_lines[s_cursor_row].len > 0 ? s_lines[s_cursor_row].len - 1 : 0;
            }
        }
        /* Editing commands */
        else if (ascii_ch == 'x' || clean_sc == 0x53) { /* Delete */
            delete_char();
        } else if (ascii_ch == 'u') {
            restore_undo_snapshot();
        }
        /* Double-key commands (dd, gg, ZZ) */
        else if (ascii_ch == 'd') {
            if (s_last_pending_cmd == 'd') {
                delete_current_line();
                s_last_pending_cmd = 0;
            } else {
                s_last_pending_cmd = 'd';
                continue;
            }
        } else if (ascii_ch == 'g') {
            if (s_last_pending_cmd == 'g') {
                s_cursor_row = 0;
                s_cursor_col = 0;
                s_last_pending_cmd = 0;
            } else {
                s_last_pending_cmd = 'g';
                continue;
            }
        } else if (ascii_ch == 'Z') {
            if (s_last_pending_cmd == 'Z') {
                save_file(s_filename);
                running = 0;
                break;
            } else {
                s_last_pending_cmd = 'Z';
                continue;
            }
        }

        s_last_pending_cmd = 0;
        render_screen();
    }

    /* Clean exit: Clear screen and return to shell */
    seld_clear();
    return 0;
}
