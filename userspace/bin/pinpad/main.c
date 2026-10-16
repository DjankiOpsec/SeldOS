/*
 * SeldOS - Humboldt Kernel Project
 * SNL OpSec Coreutils: /bin/pinpad - Scrambled On-Screen Pinpad
 *
 * Countermeasure against Acoustic Keystroke Resonance Evasion:
 * Prevents directional microphones and room eavesdroppers from
 * identifying passwords and PINs through physical keyboard acoustics.
 *
 * GPLv3 Licensed.
 */

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "seld.h"
#include "sha256.h"

#define GRID_ROWS 4
#define GRID_COLS 4
#define PIN_MAX_LEN 64

static const char* default_tokens[GRID_ROWS * GRID_COLS] = {
    "0", "1", "2", "3",
    "4", "5", "6", "7",
    "8", "9", "DEL", "CLR",
    "RND", "HASH", "OK", "EXIT"
};

static char grid[GRID_ROWS][GRID_COLS][8];
static int cursor_row = 1;
static int cursor_col = 1;

static char pin_buf[PIN_MAX_LEN];
static size_t pin_len = 0;

static uint64_t prng_state = 0x853c49e6748fea9bULL;

static uint64_t prng_next(void) {
    prng_state ^= (prng_state << 13);
    prng_state ^= (prng_state >> 7);
    prng_state ^= (prng_state << 17);
    return prng_state;
}

static void shuffle_grid(void) {
    const char* pool[GRID_ROWS * GRID_COLS];
    for (int i = 0; i < GRID_ROWS * GRID_COLS; i++) {
        pool[i] = default_tokens[i];
    }

    // Fisher-Yates Shuffle using hardware uptime & PRNG
    prng_state ^= uptime();
    for (int i = (GRID_ROWS * GRID_COLS) - 1; i > 0; i--) {
        int j = (int)(prng_next() % (uint64_t)(i + 1));
        const char* tmp = pool[i];
        pool[i] = pool[j];
        pool[j] = tmp;
    }

    int idx = 0;
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            strncpy(grid[r][c], pool[idx++], sizeof(grid[r][c]) - 1);
            grid[r][c][sizeof(grid[r][c]) - 1] = '\0';
        }
    }
}

static void render_screen(void) {
    clear();
    printf("+==============================================================+\n");
    printf("|    [ OPSEC ACOUSTIC SHIELD: SCRAMBLED ON-SCREEN PINPAD ]     |\n");
    printf("|    Acoustic Keystroke Analysis Evasion (Dynamic Matrix)      |\n");
    printf("+==============================================================+\n\n");

    printf("Entered PIN: [ ");
    for (size_t i = 0; i < pin_len; i++) {
        printf("* ");
    }
    printf("] (%d symbols entered)\n\n", (int)pin_len);

    for (int r = 0; r < GRID_ROWS; r++) {
        printf("+-------+-------+-------+-------+\n|");
        for (int c = 0; c < GRID_COLS; c++) {
            int is_cursor = (r == cursor_row && c == cursor_col);
            if (is_cursor) {
                printf(" >%4s<|", grid[r][c]);
            } else {
                printf("  %4s |", grid[r][c]);
            }
        }
        printf("\n");
    }
    printf("+-------+-------+-------+-------+\n\n");
    printf("Controls: [W/A/S/D] or [Arrows] Move | [SPACE/ENTER] Select\n");
    printf("          [R] Re-scramble | [C] Clear | [Q] Abort & Exit\n");
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // 1. Restrict capabilities with Seld-Pledge & Unveil
    seld_pledge(PLEDGE_STDIO);
    seld_unveil(NULL, NULL);

    memset(pin_buf, 0, sizeof(pin_buf));
    pin_len = 0;

    prng_state ^= uptime() ^ ((uint64_t)(uintptr_t)argv << 16);
    shuffle_grid();

    render_screen();

    while (1) {
        struct seld_kbd_event kev;
        if (seld_poll_key(&kev) <= 0 || !kev.pressed) {
            seld_sleep(20);
            continue;
        }

        uint8_t sc = kev.scancode & 0x7F;
        char c = 0;
        if (sc == 0x48 || sc == 0x11) c = 'w';       // Up / W
        else if (sc == 0x50 || sc == 0x1F) c = 's';  // Down / S
        else if (sc == 0x4B || sc == 0x1E) c = 'a';  // Left / A
        else if (sc == 0x4D || sc == 0x20) c = 'd';  // Right / D
        else if (sc == 0x13) c = 'r';                // R (Re-scramble)
        else if (sc == 0x2E) c = 'c';                // C (Clear)
        else if (sc == 0x0E) c = '\b';               // Backspace (DEL)
        else if (sc == 0x10 || sc == 0x01) c = 'q';  // Q / ESC (Abort)
        else if (sc == 0x39 || sc == 0x1C) c = ' ';  // Space / Enter (Select)

        if (c == 0) continue;

        if (c == 'w') {
            cursor_row = (cursor_row + GRID_ROWS - 1) % GRID_ROWS;
            render_screen();
        } else if (c == 's') {
            cursor_row = (cursor_row + 1) % GRID_ROWS;
            render_screen();
        } else if (c == 'a') {
            cursor_col = (cursor_col + GRID_COLS - 1) % GRID_COLS;
            render_screen();
        } else if (c == 'd') {
            cursor_col = (cursor_col + 1) % GRID_COLS;
            render_screen();
        } else if (c == 'r') {
            shuffle_grid();
            render_screen();
        } else if (c == 'c') {
            memset(pin_buf, 0, sizeof(pin_buf));
            pin_len = 0;
            render_screen();
        } else if (c == '\b') {
            if (pin_len > 0) {
                pin_buf[--pin_len] = '\0';
            }
            render_screen();
        } else if (c == 'q') {
            clear();
            printf("[*] Pinpad: Aborted by operator. Plaintext wiped.\n");
            fflush(stdout);
            memset(pin_buf, 0, sizeof(pin_buf));
            seld_flush_uarch();
            return 0;
        } else if (c == ' ') {
            const char* token = grid[cursor_row][cursor_col];
            if (strcmp(token, "EXIT") == 0) {
                clear();
                printf("[*] Pinpad: Exited. Plaintext memory zeroed.\n");
                fflush(stdout);
                memset(pin_buf, 0, sizeof(pin_buf));
                seld_flush_uarch();
                return 0;
            } else if (strcmp(token, "CLR") == 0) {
                memset(pin_buf, 0, sizeof(pin_buf));
                pin_len = 0;
                render_screen();
            } else if (strcmp(token, "DEL") == 0) {
                if (pin_len > 0) {
                    pin_buf[--pin_len] = '\0';
                }
                render_screen();
            } else if (strcmp(token, "RND") == 0) {
                shuffle_grid();
                render_screen();
            } else if (strcmp(token, "HASH") == 0 || strcmp(token, "OK") == 0) {
                clear();
                printf("[+] PIN Accepted (%d symbols).\n", (int)pin_len);

                // Compute SHA-256 of entered PIN
                uint8_t hash[32];
                struct sha256_ctx ctx;
                sha256_init(&ctx);
                sha256_update(&ctx, (const uint8_t*)pin_buf, pin_len);
                sha256_final(&ctx, hash);

                printf("[+] SHA-256 PIN Digest: ");
                for (int i = 0; i < 32; i++) {
                    printf("%02x", hash[i]);
                }
                printf("\n");
                fflush(stdout);

                // Anti-Forensics: zero buffer and flush microarchitecture
                memset(pin_buf, 0, sizeof(pin_buf));
                pin_len = 0;
                seld_flush_uarch();
                return 0;
            } else {
                // Digit token
                if (pin_len < PIN_MAX_LEN - 1) {
                    pin_buf[pin_len++] = token[0];
                    pin_buf[pin_len] = '\0';
                }
                // Dynamic Re-scramble: re-shuffle matrix after each entry
                shuffle_grid();
                render_screen();
            }
        }
    }

    memset(pin_buf, 0, sizeof(pin_buf));
    seld_flush_uarch();
    return 0;
}
