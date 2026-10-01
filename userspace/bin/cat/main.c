/*
 * SeldOS - Humboldt Kernel Project
 * SNL Coreutils: cat - Concatenate and Display File Contents
 * Reads and displays contents of specified files from SeldFS storage.
 * GPLv3 Licensed.
 */

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"

#define CHUNK_SIZE 32768

static char file_buf[CHUNK_SIZE];

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: cat <file...>\n");
        return 1;
    }

    int error = 0;

    for (int i = 1; i < argc; i++) {
        int bytes = readfile(argv[i], file_buf, sizeof(file_buf));
        if (bytes < 0) {
            printf("cat: %s: No such file or directory\n", argv[i]);
            error = 1;
            continue;
        }

        // Output bytes directly to stdout
        for (int b = 0; b < bytes; b++) {
            putchar(file_buf[b]);
        }
        if (bytes > 0 && file_buf[bytes - 1] != '\n') {
            putchar('\n');
        }
    }

    return error;
}
