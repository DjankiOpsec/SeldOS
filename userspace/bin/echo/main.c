/*
 * SeldOS - Humboldt Kernel Project
 * SNL Coreutils: echo - Print Arguments to Standard Output
 * GPLv3 Licensed.
 */

#include "stdio.h"
#include "string.h"

int main(int argc, char* argv[]) {
    int start = 1;
    int newline = 1;

    if (argc > 1 && strcmp(argv[1], "-n") == 0) {
        newline = 0;
        start = 2;
    }

    for (int i = start; i < argc; i++) {
        printf("%s", argv[i]);
        if (i < argc - 1) {
            putchar(' ');
        }
    }

    if (newline) {
        putchar('\n');
    }

    return 0;
}
