/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * Supervisor Thread Scheduler (Ring 0 Cooperative Threads) Interface
 * GPLv3 Licensed.
 */

#ifndef SELD_SCHED_H
#define SELD_SCHED_H

#include <stdint.h>
#include <stddef.h>

#define MAX_TASKS 16
#define TASK_STACK_SIZE 8192 // 8 KiB stack per kernel thread

enum task_state {
    TASK_UNUSED = 0,
    TASK_RUNNING,
    TASK_READY,
    TASK_SLEEPING,
    TASK_TERMINATED
};

struct task {
    uint32_t id;
    char     name[24];
    enum task_state state;
    uint64_t rsp;
    void*    stack_base;
    uint64_t sleep_until_ticks;
    uint64_t runtime_ticks;
};

void sched_init(void);
int sched_create_task(const char* name, void (*entry_fn)(void));
void sched_yield(void);
void sched_tick(void);
void sched_sleep(uint64_t ms);
void sched_exit(void);
struct task* sched_get_current(void);
int sched_get_tasks(struct task out_tasks[MAX_TASKS]);
int cpu_driver_disable(void);
int cpu_is_driver_enabled(void);

#endif /* SELD_SCHED_H */
