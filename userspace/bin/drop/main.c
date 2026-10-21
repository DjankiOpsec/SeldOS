/*
 * SeldOS - Humboldt Kernel Project
 * /bin/drop - RAM File Interchange & Base64 Transfer Protocol
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#include "seld.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void base64_encode(const uint8_t* in, size_t in_len) {
    size_t i = 0;
    int line_len = 0;
    while (i < in_len) {
        size_t rem = in_len - i;
        uint32_t oct_a = in[i++];
        uint32_t oct_b = (rem > 1) ? in[i++] : 0;
        uint32_t oct_c = (rem > 2) ? in[i++] : 0;

        uint32_t triple = (oct_a << 16) | (oct_b << 8) | oct_c;

        putchar(b64_table[(triple >> 18) & 0x3F]);
        putchar(b64_table[(triple >> 12) & 0x3F]);
        putchar((rem < 2) ? '=' : b64_table[(triple >> 6) & 0x3F]);
        putchar((rem < 3) ? '=' : b64_table[triple & 0x3F]);

        line_len += 4;
        if (line_len >= 64) {
            putchar('\n');
            line_len = 0;
        }
    }
    if (line_len > 0) putchar('\n');
}

static int b64_char_val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static size_t base64_decode(const char* in, size_t in_len, uint8_t* out, size_t max_out) {
    size_t out_len = 0;
    uint32_t buf = 0;
    int bits = 0;

    for (size_t i = 0; i < in_len; i++) {
        char ch = in[i];
        if (ch == '=' || ch == '\r' || ch == '\n' || ch == ' ') continue;
        int val = b64_char_val(ch);
        if (val < 0) continue;

        buf = (buf << 6) | val;
        bits += 6;

        if (bits >= 8) {
            bits -= 8;
            if (out_len < max_out) {
                out[out_len++] = (uint8_t)((buf >> bits) & 0xFF);
            }
        }
    }
    return out_len;
}

static void drop_dump(const char* filename) {
    int fd = open(filename, 0 /* O_RDONLY */);
    if (fd < 0) {
        printf("drop: %s: No such file or directory\n", filename);
        return;
    }

    struct seld_stat st;
    seld_stat(filename, &st);
    printf("-----BEGIN SELD-DROP FILE:%s SIZE:%u-----\n", filename, st.size);

    uint8_t* data = (uint8_t*)malloc(st.size + 1);
    if (!data) {
        close(fd);
        printf("[-] drop: out of RAM\n");
        return;
    }

    size_t r = read(fd, data, st.size);
    base64_encode(data, r);
    free(data);
    close(fd);

    printf("-----END SELD-DROP FILE:%s-----\n", filename);
}

static void drop_load(const char* filename) {
    printf("[*] Ready to receive base64 payload for '%s'. Paste below, terminate with EOF or empty line:\n", filename);

    char* b64_buf = (char*)malloc(128 * 1024);
    if (!b64_buf) {
        printf("[-] drop: out of memory\n");
        return;
    }

    size_t pos = 0;
    char line[512];
    while (fgets(line, sizeof(line), stdin)) {
        if (strncmp(line, "-----END", 8) == 0 || strcmp(line, "\n") == 0 || strcmp(line, "\r\n") == 0) {
            if (pos > 0) break;
            continue;
        }
        if (strncmp(line, "-----BEGIN", 10) == 0) continue;

        size_t llen = strlen(line);
        if (pos + llen < 128 * 1024 - 1) {
            memcpy(b64_buf + pos, line, llen);
            pos += llen;
        }
    }
    b64_buf[pos] = '\0';

    uint8_t* decoded = (uint8_t*)malloc(128 * 1024);
    if (!decoded) {
        free(b64_buf);
        return;
    }

    size_t dec_len = base64_decode(b64_buf, pos, decoded, 128 * 1024);
    free(b64_buf);

    int out_fd = open(filename, 0x0001 | 0x0040 | 0x0200 /* O_WRONLY | O_CREAT | O_TRUNC */);
    if (out_fd < 0) {
        printf("drop: failed to create output file '%s'\n", filename);
        free(decoded);
        return;
    }

    write(out_fd, decoded, dec_len);
    close(out_fd);
    free(decoded);

    printf("[+] Successfully saved '%s' (%u bytes decoded) into SeldFS RAM!\n", filename, (unsigned int)dec_len);
}

int main(int argc, char* argv[]) {
    seld_pledge(PLEDGE_STDIO | PLEDGE_RPATH | PLEDGE_WPATH);
    seld_unveil("/", "rwc");
    seld_unveil(NULL, NULL);

    if (argc < 3) {
        printf("usage: drop <dump <file> | load <target_file>>\n");
        printf("examples:\n");
        printf("  drop dump secret.txt  (outputs Base64 to terminal for copy-paste)\n");
        printf("  drop load imported.txt (paste Base64 into terminal to write file)\n");
        return 1;
    }

    if (strcmp(argv[1], "dump") == 0 || strcmp(argv[1], "send") == 0) {
        drop_dump(argv[2]);
        return 0;
    } else if (strcmp(argv[1], "load") == 0 || strcmp(argv[1], "recv") == 0) {
        drop_load(argv[2]);
        return 0;
    } else {
        printf("drop: unknown command '%s'\n", argv[1]);
        return 1;
    }
}
