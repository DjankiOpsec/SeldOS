/*
 * SeldOS - Humboldt Kernel Project
 * SNL Coreutils: ps - Process Status Monitor
 * Queries and displays active tasks / PID info from kernel scheduler.
 * GPLv3 Licensed.
 */

#include "stdio.h"
#include "string.h"
#include "unistd.h"
#include "seld.h"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    struct snl_task_info tasks[16];
    int count = gettasks(tasks, 16);

    if (count < 0) {
        printf("ps: failed to query scheduler tasks from kernel\n");
        return 1;
    }

    printf("PID  NAME                    STATE       RUNTIME (TICKS)\n");
    printf("---  --------------------    ---------   ---------------\n");

    for (int i = 0; i < count; i++) {
        printf("%d", tasks[i].id);
        char pbuf[8];
        snprintf(pbuf, sizeof(pbuf), "%d", tasks[i].id);
        for (size_t k = strlen(pbuf); k < 5; k++) {
            putchar(' ');
        }

        printf("%s", tasks[i].name);
        size_t nlen = strlen(tasks[i].name);
        for (size_t k = nlen; k < 24; k++) {
            putchar(' ');
        }

        switch (tasks[i].state) {
            case 1: printf("RUNNING     "); break;
            case 2: printf("READY       "); break;
            case 3: printf("SLEEPING    "); break;
            case 4: printf("ZOMBIE      "); break;
            default: printf("UNKNOWN     "); break;
        }

        printf("%lu\n", tasks[i].runtime_ticks);
    }

    return 0;
}
