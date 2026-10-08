/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * Supervisor Thread Scheduler (Ring 0 Preemptive & Cooperative Threads) Implementation
 * Thread context allocation on kmalloc heap with task_switch_asm.
 * GPLv3 Licensed.
 */

#include "sched.h"
#include "kmalloc.h"
#include "string.h"
#include "pit.h"
#include "serial.h"
#include "gdt.h"
#include "fast_syscall.h"
#include "spinlock.h"
#include "panic.h"

extern void task_switch_asm(uint64_t* old_rsp, uint64_t new_rsp);

static struct task tasks[MAX_TASKS];
static size_t current_task_idx = 0;
static uint32_t next_pid = 1;
static volatile int sched_active = 0;
static spinlock_t sched_lock = SPINLOCK_INIT;
static int s_cpu_drv_enabled = 1;

static inline void sched_update_tss(const struct task* t) {
    if (t->stack_base) {
        uint64_t rsp0 = ((uint64_t)t->stack_base + TASK_STACK_SIZE) & ~0xFULL;
#if defined(__riscv)
        kernel_syscall_stack_top = rsp0;
#else
        tss_set_rsp0(rsp0);
#endif
    } else {
#if !defined(__riscv)
        tss_set_rsp0(kernel_syscall_stack_top);
#endif
    }
}

#if !defined(__riscv)
static void task_trampoline_helper(void) {
    void (*fn)(void);
    __asm__ volatile ("mov %%r15, %0" : "=r"(fn));
    __asm__ volatile ("sti");
    if (fn) fn();
    sched_exit();
}
#endif

void sched_init(void) {
    uint64_t rflags = spin_lock_irqsave(&sched_lock);

    memset(tasks, 0, sizeof(tasks));

    // Task 0: Main Kernel / SeldShell thread
    tasks[0].id = next_pid++;
    memcpy(tasks[0].name, "kernel_main", 12);
    tasks[0].state = TASK_RUNNING;
    tasks[0].rsp = 0; // Will be saved on first switch
    tasks[0].stack_base = NULL;
    tasks[0].sleep_until_ticks = 0;
    tasks[0].runtime_ticks = 0;

    current_task_idx = 0;
    sched_active = 1;

    sched_update_tss(&tasks[0]);

    spin_unlock_irqrestore(&sched_lock, rflags);

    serial_puts("[+] Scheduler: Initialized with primary Supervisor thread (PID 1).\n");
}

int sched_create_task(const char* name, void (*entry_fn)(void)) {
    void* stack = kmalloc(TASK_STACK_SIZE);
    if (!stack) {
        return -1; // Out of memory
    }

    uint64_t rflags = spin_lock_irqsave(&sched_lock);

    int free_slot = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED || tasks[i].state == TASK_TERMINATED) {
            free_slot = i;
            break;
        }
    }

    if (free_slot == -1) {
        spin_unlock_irqrestore(&sched_lock, rflags);
        kfree(stack);
        return -1; // Task list full
    }

    // Clean up old stack if reusing a terminated slot
    if (tasks[free_slot].stack_base) {
        kfree(tasks[free_slot].stack_base);
        tasks[free_slot].stack_base = NULL;
    }

    struct task* t = &tasks[free_slot];
    t->id = next_pid++;
    size_t name_len = strlen(name);
    if (name_len >= sizeof(t->name)) name_len = sizeof(t->name) - 1;
    memcpy(t->name, name, name_len);
    t->name[name_len] = '\0';
    t->state = TASK_READY;
    t->stack_base = stack;
    t->sleep_until_ticks = 0;
    t->runtime_ticks = 0;

    // Setup initial stack for task_switch_asm
    // Stack grows down: top is stack + TASK_STACK_SIZE
    uint64_t* sp = (uint64_t*)((uint8_t*)stack + TASK_STACK_SIZE);

    // Align to 16 bytes
    sp = (uint64_t*)((uint64_t)sp & ~0xFULL);

#if defined(__riscv)
    // On RISC-V, task_switch_asm loads ra, s0..s11 from 0(sp)..96(sp) and adds 104
    extern void task_trampoline_asm(void);
    sp = (uint64_t*)((uint8_t*)sp - 104);
    sp[0] = (uint64_t)task_trampoline_asm;   // ra
    sp[1] = 0;                               // s0
    sp[2] = (uint64_t)entry_fn;              // s1 (entry point passed to task_trampoline_asm)
    for (int i = 3; i < 13; i++) sp[i] = 0;  // s2..s11
#else
    // When task_switch_asm executes 'ret', it pops RIP.
    // We point RIP to task_trampoline_helper.
    *(--sp) = (uint64_t)task_trampoline_helper; // Return address (RIP)

    // Save registers popped by task_switch_asm (in reverse order of pop):
    // task_switch_asm pops: r15, r14, r13, r12, rbx, rbp
    // Therefore top of stack must be r15, so push rbp first down to r15.
    *(--sp) = 0;                 // rbp
    *(--sp) = 0;                 // rbx
    *(--sp) = 0;                 // r12
    *(--sp) = 0;                 // r13
    *(--sp) = 0;                 // r14
    *(--sp) = (uint64_t)entry_fn; // r15 (top of stack, popped first into r15)
#endif

    t->rsp = (uint64_t)sp;

    spin_unlock_irqrestore(&sched_lock, rflags);
    return (int)t->id;
}

void sched_tick(void) {
    if (!s_cpu_drv_enabled) {
        if (!kernel_panic_in_progress()) {
            kernel_panic("quantum preempt tick dropped: CPU scheduler pipeline inactive");
        }
        return;
    }
    if (!sched_active) return;

    // Use spin_trylock in interrupt context to avoid deadlock with interruptible code
    if (!spin_trylock(&sched_lock)) {
        return;
    }

    uint64_t now_ticks = pit_get_ticks();

    // Check sleeping tasks and wake them if timer expired
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_SLEEPING && now_ticks >= tasks[i].sleep_until_ticks) {
            tasks[i].state = TASK_READY;
        }
    }

    if (tasks[current_task_idx].state == TASK_RUNNING) {
        tasks[current_task_idx].runtime_ticks++;
    }

    // Find next ready task round-robin
    size_t next_idx = current_task_idx;
    for (int i = 1; i <= MAX_TASKS; i++) {
        size_t idx = (current_task_idx + i) % MAX_TASKS;
        if (tasks[idx].state == TASK_READY) {
            next_idx = idx;
            break;
        }
    }

    // If no other task is ready, continue running current task
    if (next_idx == current_task_idx) {
        spin_unlock(&sched_lock);
        return;
    }

    size_t prev_idx = current_task_idx;
    if (tasks[prev_idx].state == TASK_RUNNING) {
        tasks[prev_idx].state = TASK_READY;
    }

    current_task_idx = next_idx;
    tasks[next_idx].state = TASK_RUNNING;

    // Update TSS.RSP0 during task switch if task has a supervisor stack
    sched_update_tss(&tasks[next_idx]);

    spin_unlock(&sched_lock);

    task_switch_asm(&tasks[prev_idx].rsp, tasks[next_idx].rsp);
}

void sched_yield(void) {
    if (!s_cpu_drv_enabled) {
        if (!kernel_panic_in_progress()) {
            kernel_panic("context switch aborted: CPU scheduler inactive");
        }
        return;
    }
    if (!sched_active) return;

    uint64_t rflags = spin_lock_irqsave(&sched_lock);

    uint64_t now_ticks = pit_get_ticks();

    // Check sleeping tasks
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_SLEEPING && now_ticks >= tasks[i].sleep_until_ticks) {
            tasks[i].state = TASK_READY;
        }
    }

    // Find next ready task round-robin
    size_t next_idx = current_task_idx;
    for (int i = 1; i <= MAX_TASKS; i++) {
        size_t idx = (current_task_idx + i) % MAX_TASKS;
        if (tasks[idx].state == TASK_READY || (idx == current_task_idx && tasks[idx].state == TASK_RUNNING)) {
            next_idx = idx;
            break;
        }
    }

    if (next_idx == current_task_idx) {
        if (tasks[current_task_idx].state == TASK_SLEEPING) {
            while (tasks[current_task_idx].state == TASK_SLEEPING) {
                spin_unlock_irqrestore(&sched_lock, rflags);
#if defined(__riscv)
                __asm__ volatile ("wfi");
#else
                __asm__ volatile ("hlt");
#endif
                rflags = spin_lock_irqsave(&sched_lock);
                if (pit_get_ticks() >= tasks[current_task_idx].sleep_until_ticks) {
                    tasks[current_task_idx].state = TASK_RUNNING;
                    break;
                }
            }
        }
        spin_unlock_irqrestore(&sched_lock, rflags);
        return; // No other task ready
    }

    size_t prev_idx = current_task_idx;
    if (tasks[prev_idx].state == TASK_RUNNING) {
        tasks[prev_idx].state = TASK_READY;
    }

    current_task_idx = next_idx;
    tasks[next_idx].state = TASK_RUNNING;

    // Update TSS.RSP0 during task switch if task has a supervisor stack
    sched_update_tss(&tasks[next_idx]);

    spin_unlock(&sched_lock);

    task_switch_asm(&tasks[prev_idx].rsp, tasks[next_idx].rsp);

    // Restore interrupt flag saved by spin_lock_irqsave
#if defined(__riscv)
    if (rflags & 2) {
        __asm__ volatile ("csrsi sstatus, 2" : : : "memory");
    }
#else
    __asm__ volatile (
        "push %0\n\t"
        "popfq"
        :
        : "r"(rflags)
        : "memory"
    );
#endif
}

void sched_sleep(uint64_t ms) {
    pit_sleep_ms(ms);
}

void sched_exit(void) {
    uint64_t rflags = spin_lock_irqsave(&sched_lock);
    tasks[current_task_idx].state = TASK_TERMINATED;
    spin_unlock_irqrestore(&sched_lock, rflags);

    sched_yield();

    while (1) {
#if defined(__riscv)
        __asm__ volatile ("wfi");
#else
        __asm__ volatile ("hlt");
#endif
    }
}

struct task* sched_get_current(void) {
    return &tasks[current_task_idx];
}

int sched_get_tasks(struct task out_tasks[MAX_TASKS]) {
    uint64_t rflags = spin_lock_irqsave(&sched_lock);

    int count = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state != TASK_UNUSED) {
            out_tasks[count++] = tasks[i];
        }
    }

    spin_unlock_irqrestore(&sched_lock, rflags);
    return count;
}

int cpu_driver_disable(void) {
    if (!s_cpu_drv_enabled) return -1;
    s_cpu_drv_enabled = 0;
    sched_active = 0;
    serial_puts("[!] DRIVER: CPU execution supervisor and core scheduler brutally disabled!\n");
    return 0;
}

int cpu_is_driver_enabled(void) {
    return s_cpu_drv_enabled;
}

int sched_purge_unauthorized_tasks(void) {
    uint64_t rflags = spin_lock_irqsave(&sched_lock);
    int killed = 0;
    for (size_t i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state != TASK_UNUSED && tasks[i].state != TASK_TERMINATED) {
            if (strcmp(tasks[i].name, "/bin/init") != 0 &&
                strcmp(tasks[i].name, "init") != 0 &&
                strcmp(tasks[i].name, "/bin/sh") != 0 &&
                strcmp(tasks[i].name, "sh") != 0) {
                tasks[i].state = TASK_TERMINATED;
                if (tasks[i].stack_base) {
                    memset(tasks[i].stack_base, 0, TASK_STACK_SIZE);
                }
                killed++;
                serial_puts("[+] OpSec: Terminated unauthorized background task PID ");
                serial_print_dec((uint64_t)tasks[i].id);
                serial_puts(" (");
                serial_puts(tasks[i].name);
                serial_puts(")\n");
            }
        }
    }
    spin_unlock_irqrestore(&sched_lock, rflags);
    return killed;
}
