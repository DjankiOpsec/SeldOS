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
#define SYS_NET_INFO    31
#define SYS_NET_PING    32
#define SYS_NET_ARP     33
#define SYS_NET_DOWNLOAD    34
#define SYS_NET_TCP_CONNECT 35
#define SYS_NET_TCP_SEND    36
#define SYS_NET_TCP_RECV    37
#define SYS_NET_TCP_CLOSE   38
#define SYS_NET_DNS_RESOLVE 39
#define SYS_SELD            42
#define SYS_IMMUNE_PURGE    45
#define SYS_NET_SET_LOCK    46
#define SYS_NET_GET_LOCK    47
#define SYS_PLEDGE          48
#define SYS_NET_SET_DESYNC  49
#define SYS_NET_GET_DESYNC  50
#define SYS_UNVEIL          51
#define SYS_OPSEC_SET_JITTER 52
#define SYS_OPSEC_GET_JITTER 53

/* Seld-Unveil Permission Flags (OpenBSD-style Ring 3 Filesystem Sandboxing) */
#define UNVEIL_READ         0x01  /* Read file: open(O_RDONLY), readfile, stat */
#define UNVEIL_WRITE        0x02  /* Write file: open(O_WRONLY/O_RDWR), writefile */
#define UNVEIL_EXEC         0x04  /* Execute binary: spawn, exec */
#define UNVEIL_CREATE       0x08  /* Create/delete file: open(O_CREAT), unlink */

/* Seld-Pledge Capability Flags (OpenBSD-style Ring 3 Syscall Sandboxing) */
#define PLEDGE_STDIO        (1 << 0)  /* Basic stdio, heap, exit, yield, uptime, screen, input */
#define PLEDGE_RPATH        (1 << 1)  /* Filesystem read: open, read, stat, listdir, readfile */
#define PLEDGE_WPATH        (1 << 2)  /* Filesystem write/delete: writefile, unlink */
#define PLEDGE_EXEC         (1 << 3)  /* Process execution: spawn, exec */
#define PLEDGE_NET          (1 << 4)  /* Networking: tcp_connect, tcp_send, tcp_recv, tcp_close, ping, arp */
#define PLEDGE_DNS          (1 << 5)  /* Domain name resolution: dns_resolve */
#define PLEDGE_AUDIO        (1 << 6)  /* Audio hardware: beep, audio_play */
#define PLEDGE_PURGE        (1 << 7)  /* Immune system purge and air-gap shield controls */
#define PLEDGE_REBOOT       (1 << 8)  /* Hardware reboot and poweroff */

#define MAX_FD 32
#define DEFAULT_USER_HEAP_BASE 0x0000000040000000ULL

#if defined(__riscv)
#define USER_STACK_TOP    0x0000003FFFFFE000ULL
#define USER_STACK_PAGE   0x0000003FFFFFF000ULL
#define USER_STACK_BOTTOM 0x0000003FFFFF0000ULL
#define USER_SPACE_LIMIT  0x0000003FFFFFF000ULL
#define USER_HEAP_MAX     0x0000003000000000ULL
#else
#define USER_STACK_TOP    0x00007FFFFFFFE000ULL
#define USER_STACK_PAGE   0x00007FFFFFFFF000ULL
#define USER_STACK_BOTTOM 0x00007FFFFFFF0000ULL
#define USER_SPACE_LIMIT  0x00007FFFFFFFF000ULL
#define USER_HEAP_MAX     0x0000700000000000ULL
#endif

/* User-mode network structures */
struct seld_net_info {
    uint32_t ip;
    uint32_t netmask;
    uint32_t gateway;
    uint32_t dns;
    uint8_t  mac[6];
    uint8_t  link_up;
    uint64_t rx_frames;
    uint64_t tx_frames;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_dropped;
    uint64_t rx_checksum_errors;
};

struct seld_arp_entry {
    uint32_t ip;
    uint8_t  mac[6];
    uint8_t  valid;
    uint64_t timestamp_ms;
};

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
void fast_sys_reboot(void) __attribute__((noreturn));
void fast_sys_poweroff(void) __attribute__((noreturn));

#endif /* SELD_FAST_SYSCALL_H */
