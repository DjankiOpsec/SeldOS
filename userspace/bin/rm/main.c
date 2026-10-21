/*
 * SeldOS - Humboldt Kernel Project
 * SNL Coreutils: rm - Remove Files from SeldFS Storage
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#include "stdio.h"
#include "unistd.h"
#include "seld.h"

int main(int argc, char* argv[]) {
    seld_pledge(PLEDGE_STDIO | PLEDGE_WPATH);
    seld_unveil("/", "rwc");
    seld_unveil(NULL, NULL);

    if (argc < 2) {
        printf("Usage: rm <file...>\n");
        return 1;
    }

    int error = 0;

    for (int i = 1; i < argc; i++) {
        int res = unlink(argv[i]);
        if (res == 0) {
            printf("rm: removed '%s'\n", argv[i]);
        } else {
            printf("rm: cannot remove '%s': No such file or directory\n", argv[i]);
            error = 1;
        }
    }

    return error;
}
