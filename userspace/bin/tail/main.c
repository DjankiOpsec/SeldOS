/*
 * SeldOS - Humboldt Kernel Project
 * /bin/tail - Output the Last Part of Files or Standard Input
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#include "seld.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TAIL_LINES 256
#define MAX_LINE_LEN   1024

static void tail_stream(int fd, int n_lines) {
    if (n_lines <= 0) return;
    if (n_lines > MAX_TAIL_LINES) n_lines = MAX_TAIL_LINES;

    char* line_ring[MAX_TAIL_LINES];
    for (int i = 0; i < n_lines; i++) {
        line_ring[i] = (char*)malloc(MAX_LINE_LEN);
        if (line_ring[i]) line_ring[i][0] = '\0';
    }

    int ring_head = 0;
    int total_lines = 0;

    char buf[4096];
    char cur_line[MAX_LINE_LEN];
    size_t cur_pos = 0;

    int r;
    while ((r = read(fd, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < r; i++) {
            char ch = buf[i];
            if (ch == '\r') continue;
            if (ch == '\n') {
                cur_line[cur_pos] = '\0';
                if (line_ring[ring_head]) {
                    strncpy(line_ring[ring_head], cur_line, MAX_LINE_LEN - 1);
                    line_ring[ring_head][MAX_LINE_LEN - 1] = '\0';
                }
                ring_head = (ring_head + 1) % n_lines;
                total_lines++;
                cur_pos = 0;
            } else {
                if (cur_pos < MAX_LINE_LEN - 1) {
                    cur_line[cur_pos++] = ch;
                }
            }
        }
    }

    if (cur_pos > 0) {
        cur_line[cur_pos] = '\0';
        if (line_ring[ring_head]) {
            strncpy(line_ring[ring_head], cur_line, MAX_LINE_LEN - 1);
            line_ring[ring_head][MAX_LINE_LEN - 1] = '\0';
        }
        ring_head = (ring_head + 1) % n_lines;
        total_lines++;
    }

    int count = (total_lines < n_lines) ? total_lines : n_lines;
    int start_idx = (total_lines < n_lines) ? 0 : ring_head;

    for (int i = 0; i < count; i++) {
        int idx = (start_idx + i) % n_lines;
        if (line_ring[idx]) {
            printf("%s\n", line_ring[idx]);
        }
    }
    fflush(stdout);

    for (int i = 0; i < n_lines; i++) {
        if (line_ring[i]) free(line_ring[i]);
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
            printf("tail: option requires an argument -- 'n'\n");
            return 1;
        }
    } else if (argc >= 2 && argv[1][0] == '-' && argv[1][1] >= '0' && argv[1][1] <= '9') {
        max_lines = atoi(argv[1] + 1);
        if (max_lines <= 0) max_lines = 10;
        file_start = 2;
    }

    if (file_start >= argc) {
        tail_stream(0, max_lines);
        return 0;
    }

    int multiple = (argc - file_start > 1);
    for (int i = file_start; i < argc; i++) {
        const char* fname = argv[i];
        if (strcmp(fname, "-") == 0) {
            if (multiple) printf("==> (stdin) <==\n");
            tail_stream(0, max_lines);
            continue;
        }

        int fd = open(fname, 0 /* O_RDONLY */);
        if (fd < 0) {
            printf("tail: cannot open '%s' for reading\n", fname);
            continue;
        }

        if (multiple) printf("==> %s <==\n", fname);
        tail_stream(fd, max_lines);
        close(fd);
    }

    return 0;
}
