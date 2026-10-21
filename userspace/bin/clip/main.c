/*
 * SeldOS - Humboldt Kernel Project
 * /bin/clip - Sovereign In-Memory Clipboard Manager
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#include "seld.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLIP_FILE "/tmp/.clipboard"
#define MAX_CLIP_SIZE (64 * 1024)

static void clip_copy_text(const char* text) {
    if (!text) return;
    size_t len = strlen(text);
    int out_fd = open(CLIP_FILE, 0x0001 | 0x0040 | 0x0200 /* O_WRONLY | O_CREAT | O_TRUNC */);
    if (out_fd < 0) {
        printf("[-] clip: failed to open clipboard storage\n");
        return;
    }
    write(out_fd, text, len);
    close(out_fd);
    printf("[+] clip: %u bytes copied to sovereign RAM clipboard\n", (unsigned int)len);
}

static void clip_copy_stream(int in_fd) {
    char* buf = (char*)malloc(MAX_CLIP_SIZE);
    if (!buf) {
        printf("[-] clip: failed to allocate RAM buffer\n");
        return;
    }

    size_t total = 0;
    int r;
    while (total < MAX_CLIP_SIZE && (r = read(in_fd, buf + total, MAX_CLIP_SIZE - total)) > 0) {
        total += r;
    }

    int out_fd = open(CLIP_FILE, 0x0001 | 0x0040 | 0x0200 /* O_WRONLY | O_CREAT | O_TRUNC */);
    if (out_fd < 0) {
        printf("[-] clip: failed to open clipboard storage\n");
        free(buf);
        return;
    }

    write(out_fd, buf, total);
    close(out_fd);
    free(buf);
    printf("[+] clip: %u bytes copied to sovereign RAM clipboard\n", (unsigned int)total);
}

static void clip_paste(void) {
    int in_fd = open(CLIP_FILE, 0 /* O_RDONLY */);
    if (in_fd < 0) {
        printf("[-] clip: clipboard is empty\n");
        return;
    }

    char buf[4096];
    int r;
    char last_ch = '\0';
    while ((r = read(in_fd, buf, sizeof(buf))) > 0) {
        write(1, buf, r);
        last_ch = buf[r - 1];
    }
    if (last_ch != '\n') {
        putchar('\n');
    }
    close(in_fd);
}

static void clip_clear(void) {
    seld_unlink(CLIP_FILE);
    printf("[+] clip: RAM clipboard wiped cleanly\n");
}

int main(int argc, char* argv[]) {
    seld_pledge(PLEDGE_STDIO | PLEDGE_RPATH | PLEDGE_WPATH);
    seld_unveil("/", "rwc");
    seld_unveil(NULL, NULL);

    if (argc < 2 || strcmp(argv[1], "paste") == 0) {
        clip_paste();
        return 0;
    }

    if (strcmp(argv[1], "copy") == 0) {
        if (argc >= 3) {
            struct seld_stat st;
            if (stat(argv[2], &st) == 0) {
                int fd = open(argv[2], 0 /* O_RDONLY */);
                if (fd >= 0) {
                    clip_copy_stream(fd);
                    close(fd);
                    return 0;
                }
            }
            clip_copy_text(argv[2]);
        } else {
            clip_copy_stream(0);
        }
        return 0;
    }

    if (strcmp(argv[1], "clear") == 0 || strcmp(argv[1], "wipe") == 0) {
        clip_clear();
        return 0;
    }

    if (strcmp(argv[1], "status") == 0) {
        struct seld_stat st;
        if (seld_stat(CLIP_FILE, &st) == 0) {
            printf("[*] RAM Clipboard: %u bytes active\n", st.size);
        } else {
            printf("[*] RAM Clipboard: empty\n");
        }
        return 0;
    }

    printf("usage: clip <copy [file] | paste | clear | status>\n");
    printf("examples:\n");
    printf("  echo \"secret key\" | clip copy\n");
    printf("  clip paste\n");
    printf("  clip copy config.txt\n");
    return 1;
}
