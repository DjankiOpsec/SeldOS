/*
 * SeldOS - Humboldt Kernel Project
 * SNL Userland Utility: poweroff (/bin/poweroff)
 * GPLv3 Licensed.
 */

#include <stdio.h>
#include <unistd.h>
#include <seld.h>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    printf("[*] Powering off SeldOS...\n");
    seld_poweroff();
    return 0;
}
