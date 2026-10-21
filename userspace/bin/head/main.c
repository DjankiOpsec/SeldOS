/*
 * SeldOS - Humboldt Kernel Project
 * /bin/head - Display First Lines of Files or Standard Input
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#include "seld.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void head_stream(int fd, int max_lines) {
    char buf[4096];
    int lines_printed = 0;
    int r;

    while (lines_printed < max_lines && (r = read(fd, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < r; i++) {
            putchar(buf[i]);
            if (buf[i] == '\n') {
                lines_printed++;
                if (lines_printed >= max_lines) break;
            }
        }
        fflush(stdout);
    }
}

int main(int argc, char* argv[]) {
    seld_pledge(PLEDGE_STDIO | PLEDGE_RPATH);
    seld_unveil("/", "r");
    seld_unveil(NULL, NULL);

    int max_lines = 10;
    int file_start = 1;

    if (argc >= 2 && strcmp(argv[1], "-n") == 0) {
        if (argc >= 3) {
            max_lines = atoi(argv[2]);
            if (max_lines <= 0) max_lines = 10;
            file_start = 3;
        } else {
            printf("head: option requires an argument -- 'n'\n");
            return 1;
        }
    } else if (argc >= 2 && argv[1][0] == '-' && argv[1][1] >= '0' && argv[1][1] <= '9') {
        max_lines = atoi(argv[1] + 1);
        if (max_lines <= 0) max_lines = 10;
        file_start = 2;
    }

    if (file_start >= argc) {
        head_stream(0, max_lines);
        return 0;
    }

    int multiple = (argc - file_start > 1);
    for (int i = file_start; i < argc; i++) {
        const char* fname = argv[i];
        if (strcmp(fname, "-") == 0) {
            if (multiple) printf("==> (stdin) <==\n");
            head_stream(0, max_lines);
            continue;
        }

        int fd = open(fname, 0 /* O_RDONLY */);
        if (fd < 0) {
            printf("head: cannot open '%s' for reading\n", fname);
            continue;
        }

        if (multiple) printf("==> %s <==\n", fname);
        head_stream(fd, max_lines);
        close(fd);
    }

    return 0;
}
