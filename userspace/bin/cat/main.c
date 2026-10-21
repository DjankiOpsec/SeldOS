/*
 * SeldOS - Humboldt Kernel Project
 * SNL Coreutils: cat - Concatenate and Display File Contents
 * Reads and displays contents of specified files from SeldFS storage.
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"

#include "seld.h"

#define CHUNK_SIZE 32768

static char file_buf[CHUNK_SIZE];

static void cat_fd(int fd) {
    char buf[1024];
    int n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        write(1, buf, n);
    }
}

int main(int argc, char* argv[]) {
    seld_pledge(PLEDGE_STDIO | PLEDGE_RPATH);
    seld_unveil("/", "r");
    seld_unveil(NULL, NULL);

    if (argc < 2) {
        cat_fd(0);
        return 0;
    }

    int error = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-") == 0) {
            cat_fd(0);
            continue;
        }

        struct seld_stat st;
        int has_stat = (stat(argv[i], &st) == 0);
        char* buf = file_buf;
        size_t buf_size = sizeof(file_buf);
        int allocated = 0;

        if (has_stat && st.size > buf_size) {
            buf = (char*)malloc(st.size);
            if (buf) {
                buf_size = st.size;
                allocated = 1;
            } else {
                buf = file_buf;
                buf_size = sizeof(file_buf);
            }
        }

        int bytes = readfile(argv[i], buf, buf_size);
        if (bytes < 0) {
            printf("cat: %s: No such file or directory\n", argv[i]);
            if (allocated) free(buf);
            error = 1;
            continue;
        }

        // Output bytes directly to stdout
        if (bytes > 0) {
            write(1, buf, bytes);
        }

        if (allocated) {
            free(buf);
            buf = NULL;
        }
    }

    return error;
}
