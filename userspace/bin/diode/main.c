/*
 * SeldOS - Humboldt Kernel Project
 * /bin/diode: Sovereign Optical & Acoustic Air-Gap Data Diode Controller
 *
 * Implements:
 * 1. 40-Line Cryptographic Key Sheet Generator (/airgap.key)
 * 2. Unidirectional Optical Data Diode (Chunked Animated QR Codes on Screen)
 * 3. Acoustic Air-Gap Channel (PC Speaker 1200/2200 Hz FSK Synchronous Transmission)
 * 4. AES-128-GCM AEAD Fail-Closed Encryption & Memory Hygiene
 * 5. Ring 3 Seld-Pledge Sandboxing (PLEDGE_STDIO | PLEDGE_RPATH | PLEDGE_AUDIO)
 *
 * GPLv3 Licensed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <seld.h>
#include "diode_crypto.h"
#include "fsk.h"
#include "qrcodegen.h"

#define SCR_W 680
#define SCR_H 334

#define CHUNK_PAYLOAD_SIZE 72 /* 72 bytes binary -> ~96 bytes Base64 */

/* 5x7 ASCII bitmap font (ASCII 32 to 126) for HUD text */
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

static struct seld_fb_info s_fb;
static uint32_t* s_fb_buf = NULL;
static int s_has_fb = 0;

static void fb_fill_rect(int x, int y, int w, int h, uint32_t col) {
    if (!s_has_fb || !s_fb_buf) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCR_W) w = SCR_W - x;
    if (y + h > SCR_H) h = SCR_H - y;
    if (w <= 0 || h <= 0) return;

    for (int r = y; r < y + h; r++) {
        uint32_t* row = &s_fb_buf[r * SCR_W + x];
        for (int c = 0; c < w; c++) {
            row[c] = col;
        }
    }
}

static void fb_draw_char(int x, int y, char c, uint32_t fg) {
    if (!s_has_fb || !s_fb_buf) return;
    if (c < 32 || c > 126) c = '?';
    const uint8_t* glyph = font5x7[c - 32];

    for (int col = 0; col < 5; col++) {
        uint8_t line = glyph[col];
        for (int row = 0; row < 7; row++) {
            if ((line >> row) & 1) {
                int px = x + col;
                int py = y + row;
                if (px >= 0 && px < SCR_W && py >= 0 && py < SCR_H) {
                    s_fb_buf[py * SCR_W + px] = fg;
                }
            }
        }
    }
}

static void fb_draw_string(int x, int y, const char* str, uint32_t fg) {
    while (*str) {
        fb_draw_char(x, y, *str++, fg);
        x += 6;
    }
}

static void print_usage(void) {
    printf("=======================================================================\n");
    printf(" SeldOS /bin/diode: Sovereign Optical & Acoustic Air-Gap Data Diode\n");
    printf("=======================================================================\n");
    printf(" Commands:\n");
    printf("   diode keygen [filepath]        Generate 40-line OTP key sheet (def: /airgap.key)\n");
    printf("   diode send <file> [keyfile]    Encrypt & transmit via screen QR & PC speaker\n");
    printf("   diode beep [keyfile]           Acoustic transmission of master key only\n");
    printf("   diode qr <file> [keyfile]      Optical QR display of payload only (silent)\n");
    printf("   diode help                     Show this sovereign OpSec manual\n\n");
    printf(" Security Model:\n");
    printf("   - Optical Diode: Unidirectional screen-to-camera animated QR stream\n");
    printf("   - Acoustic Key:  Unidirectional speaker-to-mic 1200/2200 Hz Bell 202 FSK\n");
    printf("   - Cryptography:  AES-128-GCM AEAD, HKDF-SHA256, Zero-on-Exit Hygiene\n");
    printf("   - Sandboxing:    Ring 3 OpenBSD-style Seld-Pledge (stdio, rpath, audio)\n");
    printf("=======================================================================\n");
}

static int cmd_keygen(int argc, char* argv[]) {
    seld_pledge(PLEDGE_STDIO | PLEDGE_RPATH | PLEDGE_WPATH);

    const char* key_path = (argc >= 3) ? argv[2] : "/airgap.key";

    char key_buf[DIODE_KEY_BUF_SIZE];
    int len = diode_generate_keysheet(key_buf, sizeof(key_buf));
    if (len < 0) {
        printf("[-] Error: Failed to generate key sheet.\n");
        return 1;
    }

    /* Derive master key to print security attestation */
    uint8_t master_key[DIODE_AES_KEY_SIZE];
    uint32_t key_id = 0;
    if (diode_derive_master_key(key_buf, master_key, &key_id) != 0) {
        printf("[-] Error: Key derivation failed.\n");
        diode_secure_zero(key_buf, sizeof(key_buf));
        return 1;
    }

    /* Write to SeldFS */
    int res = seld_writefile(key_path, key_buf, (size_t)len);
    if (res < 0) {
        printf("[-] Error: Failed to save key sheet to %s (code: %d)\n", key_path, res);
        diode_secure_zero(key_buf, sizeof(key_buf));
        diode_secure_zero(master_key, sizeof(master_key));
        return 1;
    }

    printf("\n%s\n", key_buf);
    printf("=======================================================================\n");
    printf("[+] Sovereign 40-Line Key Sheet saved to: %s (%d bytes)\n", key_path, len);
    printf("[+] Master Key ID: 0x%08X\n", key_id);
    printf("[+] Master Key Hex: ");
    for (int i = 0; i < DIODE_AES_KEY_SIZE; i++) printf("%02X", master_key[i]);
    printf("\n");
    printf("[+] OpSec Directive: Transfer key to Host Receiver via acoustic beep or print.\n");
    printf("=======================================================================\n");

    diode_secure_zero(key_buf, sizeof(key_buf));
    diode_secure_zero(master_key, sizeof(master_key));
    return 0;
}

static int cmd_beep(int argc, char* argv[]) {
    seld_pledge(PLEDGE_STDIO | PLEDGE_RPATH | PLEDGE_AUDIO);

    const char* key_path = (argc >= 3) ? argv[2] : "/airgap.key";

    char key_buf[DIODE_KEY_BUF_SIZE];
    int n = seld_readfile(key_path, key_buf, sizeof(key_buf) - 1);
    if (n <= 0) {
        printf("[-] Error: Key sheet %s not found. Run 'diode keygen' first.\n", key_path);
        return 1;
    }
    key_buf[n] = '\0';

    uint8_t master_key[DIODE_AES_KEY_SIZE];
    uint32_t key_id = 0;
    if (diode_derive_master_key(key_buf, master_key, &key_id) != 0) {
        printf("[-] Error: Malformed key sheet in %s.\n", key_path);
        diode_secure_zero(key_buf, sizeof(key_buf));
        return 1;
    }

    uint32_t session_id;
    diode_random_bytes((uint8_t*)&session_id, sizeof(session_id));

    struct fsk_key_packet pkt;
    fsk_build_key_packet(&pkt, session_id, key_id, master_key);

    printf("[*] Acoustic Air-Gap: Broadcasting Master Key over PC Speaker...\n");
    printf("    Session ID : 0x%08X\n", session_id);
    printf("    Key ID     : 0x%08X\n", key_id);
    printf("    CRC-32     : 0x%08X\n", pkt.crc32);
    printf("    Baud Rate  : 33.3 Baud (30ms/bit, Mark 2200Hz, Space 1200Hz)\n");

    fsk_transmit_key_packet(&pkt, FSK_DEFAULT_SYMBOL_MS);

    printf("[+] Acoustic transmission complete.\n");

    diode_secure_zero(key_buf, sizeof(key_buf));
    diode_secure_zero(master_key, sizeof(master_key));
    diode_secure_zero(&pkt, sizeof(pkt));
    return 0;
}

struct qr_frame {
    char frame_str[256];
    uint8_t qrcode[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
    int qr_size;
};

static int run_optical_diode(const char* filename,
                             const uint8_t* envelope, size_t env_len,
                             uint32_t session_id) {
    /* Split envelope into chunks of CHUNK_PAYLOAD_SIZE */
    size_t total_chunks = (env_len + CHUNK_PAYLOAD_SIZE - 1) / CHUNK_PAYLOAD_SIZE;
    if (total_chunks == 0) total_chunks = 1;

    struct qr_frame* frames = (struct qr_frame*)malloc(total_chunks * sizeof(struct qr_frame));
    if (!frames) {
        printf("[-] Error: Out of memory for QR frames.\n");
        return -1;
    }

    uint8_t tempBuffer[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];

    /* Pre-encode all frames */
    for (size_t i = 0; i < total_chunks; i++) {
        size_t offset = i * CHUNK_PAYLOAD_SIZE;
        size_t c_len = (offset + CHUNK_PAYLOAD_SIZE <= env_len) ? CHUNK_PAYLOAD_SIZE : (env_len - offset);

        char b64[160];
        diode_base64_encode(envelope + offset, c_len, b64, sizeof(b64));

        const char* base_fn = strrchr(filename, '/');
        if (base_fn) base_fn++;
        else base_fn = filename;

        uint32_t chunk_crc = diode_crc32(envelope + offset, c_len);

        snprintf(frames[i].frame_str, sizeof(frames[i].frame_str),
                 "SELD1:%u/%u:%08X:%s:%s:%08X",
                 (unsigned)(i + 1), (unsigned)total_chunks,
                 session_id, base_fn, b64, chunk_crc);

        bool ok = qrcodegen_encodeText(frames[i].frame_str, tempBuffer, frames[i].qrcode,
                                      qrcodegen_Ecc_LOW, 1, 10, qrcodegen_Mask_AUTO, true);
        if (!ok) {
            printf("[-] Error: Failed to encode QR frame %u/%u\n", (unsigned)(i+1), (unsigned)total_chunks);
            free(frames);
            return -2;
        }
        frames[i].qr_size = qrcodegen_getSize(frames[i].qrcode);
    }

    printf("[+] Optical Data Diode ready: %u frames generated (%u bytes total).\n",
           (unsigned)total_chunks, (unsigned)env_len);

    /* Try initializing Framebuffer for graphical QR rendering */
    s_has_fb = (seld_get_framebuffer(&s_fb) == 0 && s_fb.framebuffer != NULL);
    if (s_has_fb) {
        s_fb_buf = (uint32_t*)s_fb.framebuffer;
    }

    if (!s_has_fb) {
        /* Headless / Text Console fallback: print all frames */
        printf("[*] Console Text Mode: Cycling frames...\n");
        for (size_t i = 0; i < total_chunks; i++) {
            printf("\n--- [FRAME %u/%u] ---\n%s\n", (unsigned)(i+1), (unsigned)total_chunks, frames[i].frame_str);
        }
        free(frames);
        return 0;
    }

    /* Graphical Framebuffer loop */
    size_t cur_frame = 0;
    int paused = 0;
    int running = 1;

    while (running) {
        const struct qr_frame* qrf = &frames[cur_frame];
        int qr_dim = qrf->qr_size;

        /* Calculate pixel scale: fit inside 250x250 area */
        int module_px = 240 / (qr_dim + 8);
        if (module_px < 3) module_px = 3;
        if (module_px > 7) module_px = 7;

        int total_box_px = (qr_dim + 8) * module_px;
        int box_x = (SCR_W - total_box_px) / 2;
        int box_y = (SCR_H - total_box_px) / 2;

        /* Clear Framebuffer with dark obsidian theme */
        fb_fill_rect(0, 0, SCR_W, SCR_H, 0xFF141A24);

        /* Top Header Bar */
        fb_fill_rect(0, 0, SCR_W, 26, 0xFF1E2838);
        fb_draw_string(16, 9, "SELD-AIRGAP OPTICAL DATA DIODE (UNIDIRECTIONAL SCREEN->CAM)", 0xFF38BDF8);

        const char* base_fn = strrchr(filename, '/');
        if (base_fn) base_fn++;
        else base_fn = filename;

        char sub_hdr[80];
        snprintf(sub_hdr, sizeof(sub_hdr), "SESSION: 0x%08X | FILE: %s", session_id, base_fn);
        fb_draw_string(SCR_W - 270, 9, sub_hdr, 0xFF94A3B8);

        /* Draw White QR Code Container */
        fb_fill_rect(box_x, box_y, total_box_px, total_box_px, 0xFFFFFFFF);

        /* Render QR Code modules (Black modules on White quiet zone) */
        int quiet_zone = 4;
        for (int y = 0; y < qr_dim; y++) {
            for (int x = 0; x < qr_dim; x++) {
                if (qrcodegen_getModule(qrf->qrcode, x, y)) {
                    int mx = box_x + (x + quiet_zone) * module_px;
                    int my = box_y + (y + quiet_zone) * module_px;
                    fb_fill_rect(mx, my, module_px, module_px, 0xFF000000);
                }
            }
        }

        /* Bottom Status HUD */
        fb_fill_rect(0, SCR_H - 28, SCR_W, 28, 0xFF1E2838);

        char hud_status[80];
        snprintf(hud_status, sizeof(hud_status), "FRAME: %u/%u %s | SIZE: %u B",
                 (unsigned)(cur_frame + 1), (unsigned)total_chunks,
                 paused ? "[PAUSED]" : "[CYCLING]", (unsigned)env_len);
        fb_draw_string(16, SCR_H - 19, hud_status, 0xFF34D399);

        fb_draw_string(SCR_W - 280, SCR_H - 19, "[SPACE] Pause | [<- ->] Step | [Q] Exit", 0xFFE2E8F0);

        /* Poll Keyboard non-blocking for 800ms (100ms * 8) */
        for (int tick = 0; tick < 8; tick++) {
            struct seld_kbd_event ev;
            while (seld_poll_key(&ev)) {
                if (ev.pressed) {
                    if (ev.scancode == 0x10 || ev.scancode == 0x01) { // 'Q' or ESC
                        running = 0;
                        break;
                    } else if (ev.scancode == 0x39) { // Space
                        paused = !paused;
                    } else if (ev.scancode == 0x4D || ev.scancode == 0x31) { // Right / 'N'
                        cur_frame = (cur_frame + 1) % total_chunks;
                        tick = 8;
                        break;
                    } else if (ev.scancode == 0x4B || ev.scancode == 0x19) { // Left / 'P'
                        cur_frame = (cur_frame == 0) ? (total_chunks - 1) : (cur_frame - 1);
                        tick = 8;
                        break;
                    }
                }
            }
            if (!running) break;
            seld_sleep(100);
        }

        if (!paused && running) {
            cur_frame = (cur_frame + 1) % total_chunks;
        }
    }

    /* Clear Framebuffer and cleanly restore text console */
    fb_fill_rect(0, 0, SCR_W, SCR_H, 0xFF141A24);
    seld_clear();

    free(frames);
    return 0;
}

static int cmd_send(int argc, char* argv[], int silent_mode) {
    seld_pledge(PLEDGE_STDIO | PLEDGE_RPATH | PLEDGE_AUDIO);

    if (argc < 3) {
        printf("[-] Usage: diode %s <filename> [keyfile]\n", silent_mode ? "qr" : "send");
        return 1;
    }

    const char* filename = argv[2];
    const char* key_path = (argc >= 4) ? argv[3] : "/airgap.key";

    /* 1. Read 40-line key sheet */
    char key_buf[DIODE_KEY_BUF_SIZE];
    int kn = seld_readfile(key_path, key_buf, sizeof(key_buf) - 1);
    if (kn <= 0) {
        printf("[-] Error: Key sheet %s not found. Generate one first using 'diode keygen'.\n", key_path);
        return 1;
    }
    key_buf[kn] = '\0';

    uint8_t master_key[DIODE_AES_KEY_SIZE];
    uint32_t key_id = 0;
    if (diode_derive_master_key(key_buf, master_key, &key_id) != 0) {
        printf("[-] Error: Failed to derive master key from %s.\n", key_path);
        diode_secure_zero(key_buf, sizeof(key_buf));
        return 1;
    }

    /* 2. Read payload file */
    struct seld_stat st;
    if (seld_stat(filename, &st) != 0) {
        printf("[-] Error: File %s not found in SeldFS.\n", filename);
        diode_secure_zero(key_buf, sizeof(key_buf));
        diode_secure_zero(master_key, sizeof(master_key));
        return 1;
    }

    size_t file_size = st.size;
    if (file_size == 0) {
        printf("[-] Error: File %s is empty.\n", filename);
        diode_secure_zero(key_buf, sizeof(key_buf));
        diode_secure_zero(master_key, sizeof(master_key));
        return 1;
    }

    uint8_t* file_buf = (uint8_t*)malloc(file_size);
    if (!file_buf) {
        printf("[-] Error: Failed to allocate memory for %s.\n", filename);
        diode_secure_zero(key_buf, sizeof(key_buf));
        diode_secure_zero(master_key, sizeof(master_key));
        return 1;
    }

    int rn = seld_readfile(filename, file_buf, file_size);
    if (rn != (int)file_size) {
        printf("[-] Error: Failed to read complete file %s.\n", filename);
        free(file_buf);
        diode_secure_zero(key_buf, sizeof(key_buf));
        diode_secure_zero(master_key, sizeof(master_key));
        return 1;
    }

    /* 3. Encrypt payload */
    uint32_t session_id;
    diode_random_bytes((uint8_t*)&session_id, sizeof(session_id));

    uint8_t* envelope = NULL;
    size_t env_len = 0;
    if (diode_encrypt_payload(master_key, filename, file_buf, file_size, session_id, &envelope, &env_len) != 0) {
        printf("[-] Error: AES-128-GCM encryption failed.\n");
        free(file_buf);
        diode_secure_zero(key_buf, sizeof(key_buf));
        diode_secure_zero(master_key, sizeof(master_key));
        return 1;
    }

    /* Zero plaintext memory immediately */
    diode_secure_zero(file_buf, file_size);
    free(file_buf);

    printf("=======================================================================\n");
    printf(" SELD-AIRGAP OPTICAL & ACOUSTIC TRANSMISSION\n");
    printf("=======================================================================\n");
    printf(" [*] Payload File  : %s (%u bytes)\n", filename, (unsigned)file_size);
    printf(" [*] Envelope Size : %u bytes (AES-128-GCM AEAD Authenticated)\n", (unsigned)env_len);
    printf(" [*] Session ID    : 0x%08X\n", session_id);
    printf(" [*] Key ID        : 0x%08X\n", key_id);

    /* 4. Acoustic transmission (if not in silent/qr-only mode) */
    if (!silent_mode) {
        struct fsk_key_packet pkt;
        fsk_build_key_packet(&pkt, session_id, key_id, master_key);

        printf("\n[*] Transmitting Master Key via Acoustic Air-Gap (PC Speaker)...\n");
        printf("    Beep duration: ~9.8s (Bell 202 FSK). Point host microphone now!\n");

        fsk_transmit_key_packet(&pkt, FSK_DEFAULT_SYMBOL_MS);

        printf("[+] Acoustic Key Broadcast completed successfully.\n");
        diode_secure_zero(&pkt, sizeof(pkt));
    }

    /* 5. Optical Data Diode (Animated QR code display) */
    printf("\n[*] Launching Optical Data Diode Display. Aim host webcam at monitor.\n");
    seld_sleep(500);

    run_optical_diode(filename, envelope, env_len, session_id);

    /* 6. Secure cleanup */
    diode_secure_zero(key_buf, sizeof(key_buf));
    diode_secure_zero(master_key, sizeof(master_key));
    if (envelope) {
        diode_secure_zero(envelope, env_len);
        free(envelope);
    }

    printf("\n[+] SELD-AIRGAP Data Diode session ended cleanly.\n");
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 0;
    }

    const char* subcmd = argv[1];

    if (strcmp(subcmd, "keygen") == 0) {
        return cmd_keygen(argc, argv);
    } else if (strcmp(subcmd, "beep") == 0 || strcmp(subcmd, "key-audio") == 0) {
        return cmd_beep(argc, argv);
    } else if (strcmp(subcmd, "send") == 0 || strcmp(subcmd, "transmit") == 0 || strcmp(subcmd, "tx") == 0) {
        return cmd_send(argc, argv, 0);
    } else if (strcmp(subcmd, "qr") == 0) {
        return cmd_send(argc, argv, 1);
    } else if (strcmp(subcmd, "help") == 0 || strcmp(subcmd, "--help") == 0) {
        print_usage();
        return 0;
    } else {
        printf("[-] Unknown command '%s'. Run 'diode help' for usage.\n", subcmd);
        return 1;
    }
}
