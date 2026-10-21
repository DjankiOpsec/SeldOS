/*
 * SeldOS - Humboldt Kernel Project
 * SNL Coreutils: echo - Print Arguments to Standard Output
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#include "stdio.h"
#include "string.h"
#include "seld.h"

int main(int argc, char* argv[]) {
    seld_pledge(PLEDGE_STDIO);
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
