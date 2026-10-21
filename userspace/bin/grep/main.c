/*
 * SeldOS - Humboldt Kernel Project
 * /bin/grep - Fast Line Pattern Search Utility
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#include "seld.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int s_opt_i = 0; // Case-insensitive
static int s_opt_v = 0; // Invert match
static int s_opt_n = 0; // Line numbers
static int s_opt_c = 0; // Count only
static int s_opt_l = 0; // Filename only

static char to_lower_c(char c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

static const char* stristr_custom(const char* haystack, const char* needle) {
    if (!*needle) return haystack;
    for (; *haystack; haystack++) {
        if (to_lower_c(*haystack) == to_lower_c(*needle)) {
            const char* h = haystack;
            const char* n = needle;
            while (*h && *n && to_lower_c(*h) == to_lower_c(*n)) {
                h++;
                n++;
            }
            if (!*n) return haystack;
        }
    }
    return NULL;
}

static int line_matches(const char* line, const char* pattern) {
    int found = 0;
    if (s_opt_i) {
        found = (stristr_custom(line, pattern) != NULL);
    } else {
        found = (strstr(line, pattern) != NULL);
    }
    return s_opt_v ? !found : found;
}

static int grep_stream(int fd, const char* filename, const char* pattern, int show_fname) {
    char buf[4096];
    char line[2048];
    size_t line_pos = 0;
    int line_num = 1;
    int match_count = 0;

    int r;
    while ((r = read(fd, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < r; i++) {
            char ch = buf[i];
            if (ch == '\r') continue;
            if (ch == '\n') {
                line[line_pos] = '\0';
                if (line_matches(line, pattern)) {
                    match_count++;
                    if (s_opt_l) {
                        printf("%s\n", filename ? filename : "(stdin)");
                        return 1;
                    }
                    if (!s_opt_c) {
                        if (show_fname && filename) {
                            printf("%s:", filename);
                        }
                        if (s_opt_n) {
                            printf("%d:", line_num);
                        }
                        printf("%s\n", line);
                    }
                }
                line_pos = 0;
                line_num++;
            } else {
                if (line_pos < sizeof(line) - 1) {
                    line[line_pos++] = ch;
                }
            }
        }
    }

    // Flush last line if not newline-terminated
    if (line_pos > 0) {
        line[line_pos] = '\0';
        if (line_matches(line, pattern)) {
            match_count++;
            if (s_opt_l) {
                printf("%s\n", filename ? filename : "(stdin)");
                return 1;
            }
            if (!s_opt_c) {
                if (show_fname && filename) {
                    printf("%s:", filename);
                }
                if (s_opt_n) {
                    printf("%d:", line_num);
                }
                printf("%s\n", line);
            }
        }
    }

    if (s_opt_c) {
        if (show_fname && filename) {
            printf("%s:%d\n", filename, match_count);
        } else {
            printf("%d\n", match_count);
        }
    }

    return (match_count > 0) ? 0 : 1;
}

int main(int argc, char* argv[]) {
    seld_pledge(PLEDGE_STDIO | PLEDGE_RPATH);
    seld_unveil("/", "r");
    seld_unveil(NULL, NULL);

    if (argc < 2) {
        printf("usage: grep [-i] [-v] [-n] [-c] [-l] pattern [file...]\n");
        return 2;
    }

    int arg_idx = 1;
    while (arg_idx < argc && argv[arg_idx][0] == '-' && argv[arg_idx][1] != '\0') {
        const char* opt = argv[arg_idx] + 1;
        while (*opt) {
            if (*opt == 'i') s_opt_i = 1;
            else if (*opt == 'v') s_opt_v = 1;
            else if (*opt == 'n') s_opt_n = 1;
            else if (*opt == 'c') s_opt_c = 1;
            else if (*opt == 'l') s_opt_l = 1;
            else {
                printf("grep: unknown option '-%c'\n", *opt);
                return 2;
            }
            opt++;
        }
        arg_idx++;
    }

    if (arg_idx >= argc) {
        printf("grep: missing pattern\n");
        return 2;
    }

    const char* pattern = argv[arg_idx++];
    int file_count = argc - arg_idx;

    if (file_count <= 0) {
        // Read from stdin (0)
        return grep_stream(0, NULL, pattern, 0);
    }

    int total_matched = 0;
    int show_fname = (file_count > 1);

    for (int i = arg_idx; i < argc; i++) {
        const char* fname = argv[i];
        if (strcmp(fname, "-") == 0) {
            if (grep_stream(0, "(stdin)", pattern, show_fname) == 0) total_matched = 1;
            continue;
        }

        int fd = open(fname, 0 /* O_RDONLY */);
        if (fd < 0) {
            printf("grep: %s: No such file or directory\n", fname);
            continue;
        }

        if (grep_stream(fd, fname, pattern, show_fname) == 0) {
            total_matched = 1;
        }
        close(fd);
    }

    return total_matched ? 0 : 1;
}
