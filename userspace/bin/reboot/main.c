/*
 * SeldOS - Humboldt Kernel Project
 * SNL Userland Utility: reboot (/bin/reboot)
 * GPLv3 Licensed.
 */

#include <stdio.h>
#include <unistd.h>
#include <seld.h>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    printf("[*] Rebooting SeldOS...\n");
    seld_reboot();
    return 0;
}
