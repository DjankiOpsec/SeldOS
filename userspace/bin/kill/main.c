/*
 * SeldOS - Humboldt Kernel Project
 * /bin/kill - Send Termination Signal to Process by PID
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#include "seld.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]) {
    seld_pledge(PLEDGE_STDIO | PLEDGE_EXEC);

    if (argc < 2) {
        printf("usage: kill [-9] <pid>\n");
        return 1;
    }

    int sig = 9;
    int pid_arg_idx = 1;

    if (argv[1][0] == '-') {
        sig = atoi(argv[1] + 1);
        if (sig <= 0) sig = 9;
        pid_arg_idx = 2;
    }

    if (pid_arg_idx >= argc) {
        printf("kill: missing pid\n");
        return 1;
    }

    uint32_t pid = (uint32_t)atoi(argv[pid_arg_idx]);
    if (pid == 0) {
        printf("kill: invalid pid '%s'\n", argv[pid_arg_idx]);
        return 1;
    }

    if (pid == 1) {
        printf("kill: Operation not permitted: PID 1 is supervisor init\n");
        return 1;
    }

    int res = seld_kill(pid, sig);
    if (res != 0) {
        printf("kill: (%u) - No such process\n", pid);
        return 1;
    }

    printf("[+] Terminated process PID %u (signal %d)\n", pid, sig);
    return 0;
}
