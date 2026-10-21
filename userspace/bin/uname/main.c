/*
 * SeldOS - Humboldt Kernel Project
 * SNL Coreutils: uname - Print System Information
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#include "stdio.h"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

#if defined(__riscv)
    printf("SNL Seld Not Linux 0.1-sec riscv64 Humboldt Hardened\n");
#else
    printf("SNL Seld Not Linux 0.1-sec x86_64 Humboldt Hardened\n");
#endif
    return 0;
}
