/*
 * SeldOS - Humboldt Kernel Project
 * SNL Coreutils: uname - Print System Information
 * GPLv3 Licensed.
 */

#include "stdio.h"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("SNL Seld Not Linux 0.1-sec x86_64 Humboldt GPLv3\n");
    return 0;
}
