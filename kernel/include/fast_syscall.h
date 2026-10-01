/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * Fast System Call (SYSCALL / SYSRET) Interface
 * MSR Initialization, Register Conventions, and Entry Point.
 * GPLv3 Licensed.
 */

#ifndef SELD_FAST_SYSCALL_H
#define SELD_FAST_SYSCALL_H

#include <stdint.h>
#include <stddef.h>

#define MSR_EFER  0xC0000080
#define MSR_STAR  0xC0000081
#define MSR_LSTAR 0xC0000082
#define MSR_FMASK 0xC0000084

#define SYS_EXIT        1
#define SYS_WRITE       2
#define SYS_READ        3
#define SYS_YIELD       4
#define SYS_UPTIME      5
#define SYS_GETPID      6
#define SYS_SELD_VERIFY 7

#define SYS_OPEN        10
#define SYS_CLOSE       11
#define SYS_STAT        12
#define SYS_LISTDIR     13
#define SYS_BRK         14
#define SYS_SPAWN       15
#define SYS_EXEC        16

#define SYS_SLEEP       17
#define SYS_UNLINK      18
#define SYS_GETTASKS    19
#define SYS_MEMINFO     20
#define SYS_CLEAR       21
#define SYS_READFILE    22
#define SYS_WRITEFILE   23
#define SYS_LSEEK       24
#define SYS_FRAMEBUFFER 25
#define SYS_POLLKEY     26
#define SYS_POLLMOUSE   27
#define SYS_BEEP        28
#define SYS_AUDIO_PLAY  29
#define SYS_SET_CONSOLE_ROWS 30
#define SYS_SELD        42

#define MAX_FD 32
#define DEFAULT_USER_HEAP_BASE 0x0000000040000000ULL

/* User-mode stat structure */
struct seld_stat {
    char     name[32];
    uint32_t size;
    uint32_t block_count;
    uint32_t flags;
    uint8_t  sha256[32];
};

/* User-mode directory entry structure */
struct seld_dirent {
    char     name[32];
    uint32_t size;
    uint32_t block_count;
    uint8_t  sha256[32];
};

/* User-mode task info structure */
struct snl_task_info {
    uint32_t id;
    char     name[24];
    uint32_t state;
    uint64_t rsp;
    void*    stack_base;
    uint64_t sleep_until_ticks;
    uint64_t runtime_ticks;
};

/* User-mode memory info structure */
struct snl_meminfo {
    uint64_t total_ram;
    uint64_t used_ram;
    uint64_t free_ram;
    uint64_t heap_size;
    uint64_t heap_used;
    uint64_t heap_free;
};

struct user_process_state {
    uint64_t* pml4_virt;
    uint64_t  pml4_phys;
    uint64_t  heap_start;
    uint64_t  heap_brk;
    uint64_t  mapped_brk;
};

extern uint64_t kernel_syscall_stack_top;
extern struct user_process_state current_user_proc;

int validate_user_buffer(const void* user_ptr, size_t size, int write);
void syscall_init_fast(void);
int jump_to_userspace(void (*user_func)(void), void* user_stack_top, uint64_t user_cr3);
void user_exit_to_kernel(uint64_t exit_code);
int elf_load_and_run(const char* path, int argc, char* argv[]);

#endif /* SELD_FAST_SYSCALL_H */
