/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * Fast SYSCALL / SYSRET Dispatcher and User Mode Transition
 * Handles requests from isolated Ring 3 applications.
 * Implements process virtual memory (brk), ELF process execution (spawn/exec),
 * and universal file I/O over SeldFS.
 * GPLv3 Licensed.
 */

#include "fast_syscall.h"
#include "elf.h"
#include "gdt.h"
#include "vga.h"
#include "serial.h"
#include "pit.h"
#include "sched.h"
#include "kbd.h"
#include "mouse.h"
#include "seldfs.h"
#include "pmm.h"
#include "vmm.h"
#include "kmalloc.h"
#include "string.h"
#include "audio.h"
#include "net.h"
#include "e1000.h"
#include "panic.h"
#include "io.h"
#include "syscall.h"

extern void syscall_entry_asm(void);
extern int jump_to_userspace_asm(void (*user_func)(void), void* user_stack_top, uint64_t user_cr3, uint64_t argc, void* argv);
extern void user_exit_to_kernel(uint64_t exit_code);

static uint8_t syscall_stack[65536] __attribute__((aligned(16)));
uint64_t kernel_syscall_stack_top = (uint64_t)&syscall_stack[sizeof(syscall_stack)];

/* Process user heap break address */
static uint64_t user_current_brk = 0;

/* Seld-Pledge Process Sandboxing State */
static uint32_t current_pledge_mask = 0xFFFFFFFF;
static uint8_t  current_pledge_enforced = 0;

/* Seld-Unveil Process Filesystem Sandboxing State (OpenBSD unveil(2)) */
#define MAX_UNVEIL_RULES 16
#define UNVEIL_PATH_MAX  SELDFS_MAX_FILENAME

struct unveil_rule {
    char    path[UNVEIL_PATH_MAX];
    uint8_t perms;
    uint8_t is_dir;
};

static struct unveil_rule current_unveil_rules[MAX_UNVEIL_RULES];
static size_t current_unveil_count = 0;
static uint8_t current_unveil_enforced = 0;
static uint8_t current_unveil_locked = 0;

/* User-mode file descriptor table (0=stdin, 1=stdout, 2=stderr, 3..31=SeldFS files) */
#define MAX_USER_FDS 32
struct user_fd_entry {
    int      used;
    char     filename[SELDFS_MAX_FILENAME];
    uint32_t offset;
    uint32_t flags;
    uint32_t size;
};
static struct user_fd_entry user_fds[MAX_USER_FDS];

static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t lo = (uint32_t)val;
    uint32_t hi = (uint32_t)(val >> 32);
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

void syscall_init_fast(void) {
    // 1. Enable SCE (System Call Extensions) and NXE (No-Execute) in EFER
    uint64_t efer = rdmsr(MSR_EFER);
    wrmsr(MSR_EFER, efer | (1ULL << 0) | (1ULL << 11));

    // 2. Setup STAR MSR:
    // Bits [47:32] = 0x08 (Kernel CS for SYSCALL; Kernel SS is 0x10)
    // Bits [63:48] = 0x10 (Base for SYSRET: User CS is 0x10+16=0x20|3=0x23, User SS is 0x10+8=0x18|3=0x1B)
    uint64_t star = ((uint64_t)0x08 << 32) | ((uint64_t)0x10 << 48);
    wrmsr(MSR_STAR, star);

    // 3. Setup LSTAR MSR (handler address)
    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry_asm);

    // 4. Setup FMASK MSR: mask IF (0x200) to disable interrupts upon syscall entry
    wrmsr(MSR_FMASK, 0x200);

    // 5. Update TSS.RSP0
    tss_set_rsp0(kernel_syscall_stack_top);

    // 6. Initialize FD table (0, 1, 2 reserved)
    memset(user_fds, 0, sizeof(user_fds));
    user_fds[0].used = 1; // stdin
    user_fds[1].used = 1; // stdout
    user_fds[2].used = 1; // stderr

    serial_puts("[+] SYSCALL: Fast system call extension enabled (MSR LSTAR configured).\n");
}

/* SeldFS path resolution helper (supports symmetric lookup: 'sh' <-> '/bin/sh') */
static int resolve_seldfs_path(const char* path, char* out_resolved) {
    if (!path || !out_resolved) return 0;

    // Strip leading "./" if present
    if (path[0] == '.' && path[1] == '/') {
        path += 2;
    }

    // 1. Direct match
    struct seldfs_inode inode;
    if (seldfs_get_file_info(path, &inode) == 0) {
        strncpy(out_resolved, path, SELDFS_MAX_FILENAME - 1);
        out_resolved[SELDFS_MAX_FILENAME - 1] = '\0';
        return 1;
    }

    // 2. Try prefixing /bin/ if not starting with '/'
    if (path[0] != '/') {
        char test_path[SELDFS_MAX_FILENAME];
        test_path[0] = '/';
        test_path[1] = 'b';
        test_path[2] = 'i';
        test_path[3] = 'n';
        test_path[4] = '/';
        size_t idx = 5;
        for (size_t i = 0; path[i] && idx < SELDFS_MAX_FILENAME - 1; i++) {
            test_path[idx++] = path[i];
        }
        test_path[idx] = '\0';
        if (seldfs_get_file_info(test_path, &inode) == 0) {
            strncpy(out_resolved, test_path, SELDFS_MAX_FILENAME - 1);
            out_resolved[SELDFS_MAX_FILENAME - 1] = '\0';
            return 1;
        }
    }

    // 3. Try stripping /bin/ if starts with /bin/
    if (path[0] == '/' && path[1] == 'b' && path[2] == 'i' && path[3] == 'n' && path[4] == '/') {
        const char* stripped = path + 5;
        if (seldfs_get_file_info(stripped, &inode) == 0) {
            strncpy(out_resolved, stripped, SELDFS_MAX_FILENAME - 1);
            out_resolved[SELDFS_MAX_FILENAME - 1] = '\0';
            return 1;
        }
    }

    // 4. Try stripping leading '/' if present
    if (path[0] == '/') {
        const char* stripped = path + 1;
        if (seldfs_get_file_info(stripped, &inode) == 0) {
            strncpy(out_resolved, stripped, SELDFS_MAX_FILENAME - 1);
            out_resolved[SELDFS_MAX_FILENAME - 1] = '\0';
            return 1;
        }
    }

    // Default to copying path
    strncpy(out_resolved, path, SELDFS_MAX_FILENAME - 1);
    out_resolved[SELDFS_MAX_FILENAME - 1] = '\0';
    return 0;
}

/* Seld-Unveil Path Canonicalization Helper */
static void normalize_unveil_path(const char* in, char* out, size_t max_len, int* out_is_dir) {
    if (!in || !out || max_len == 0) return;
    if (out_is_dir) *out_is_dir = 0;

    if (in[0] == '.' && in[1] == '/') {
        in += 2;
    }

    size_t in_len = strlen(in);
    size_t o = 0;

    if (in[0] != '/') {
        if (o < max_len - 1) out[o++] = '/';
    }

    for (size_t i = 0; i < in_len && o < max_len - 1; i++) {
        out[o++] = in[i];
    }
    out[o] = '\0';

    if (o > 1 && out[o - 1] == '/') {
        out[o - 1] = '\0';
        o--;
        if (out_is_dir) *out_is_dir = 1;
    }

    if (strcmp(out, "/bin") == 0 || strcmp(out, "/") == 0) {
        if (out_is_dir) *out_is_dir = 1;
    }
}

/* Seld-Unveil Registration Handler (SYS_UNVEIL, vector 51) */
static int sys_unveil_handler(const char* user_path, const char* user_perms) {
    if (!user_path && !user_perms) {
        current_unveil_locked = 1;
        serial_puts("[+] OpSec: Seld-Unveil configuration LOCKED (irreversible)\n");
        return 0;
    }

    if (current_unveil_locked) {
        serial_puts("[-] OpSec Violation: Attempt to modify unveil rules after unveil(NULL, NULL) lock!\n");
        return -37; // EPERM
    }

    if (!user_path || !user_perms) {
        return -1;
    }

    if (!validate_user_buffer(user_path, 1, 0) || !validate_user_buffer(user_perms, 1, 0)) {
        return -1;
    }

    uint8_t perms = 0;
    for (size_t i = 0; i < 16; i++) {
        if (!validate_user_buffer(user_perms + i, 1, 0)) break;
        char c = user_perms[i];
        if (c == '\0') break;
        if (c == 'r') perms |= UNVEIL_READ;
        else if (c == 'w') perms |= UNVEIL_WRITE;
        else if (c == 'x') perms |= UNVEIL_EXEC;
        else if (c == 'c') perms |= UNVEIL_CREATE;
    }

    if (perms == 0) {
        return -1;
    }

    char k_user_path[UNVEIL_PATH_MAX];
    size_t ulen = 0;
    while (ulen < UNVEIL_PATH_MAX - 1) {
        if (!validate_user_buffer(user_path + ulen, 1, 0)) break;
        char c = user_path[ulen];
        if (c == '\0') break;
        k_user_path[ulen++] = c;
    }
    k_user_path[ulen] = '\0';
    if (ulen == 0) return -1;

    char canon[UNVEIL_PATH_MAX];
    int is_dir = 0;
    normalize_unveil_path(k_user_path, canon, sizeof(canon), &is_dir);

    for (size_t i = 0; i < current_unveil_count; i++) {
        if (strcmp(current_unveil_rules[i].path, canon) == 0) {
            current_unveil_rules[i].perms = perms;
            current_unveil_rules[i].is_dir = is_dir;
            current_unveil_enforced = 1;
            serial_puts("[+] OpSec: Updated unveil rule '");
            serial_puts(canon);
            serial_puts("' perms=0x");
            serial_print_hex((uint64_t)perms);
            serial_puts("\n");
            return 0;
        }
    }

    if (current_unveil_count >= MAX_UNVEIL_RULES) {
        serial_puts("[-] OpSec: Maximum unveil rules exceeded!\n");
        return -1;
    }

    strncpy(current_unveil_rules[current_unveil_count].path, canon, UNVEIL_PATH_MAX - 1);
    current_unveil_rules[current_unveil_count].perms = perms;
    current_unveil_rules[current_unveil_count].is_dir = is_dir;
    current_unveil_count++;
    current_unveil_enforced = 1;

    serial_puts("[+] OpSec: Unveiled '");
    serial_puts(canon);
    serial_puts("' (perms=0x");
    serial_print_hex((uint64_t)perms);
    serial_puts(")\n");
    return 0;
}

/* Seld-Unveil Verification Gatekeeper */
static int check_unveil(const char* target_path, uint8_t req_perm) {
    if (!current_unveil_enforced) {
        return 1;
    }
    if (!target_path) return 0;

    char canon_target[UNVEIL_PATH_MAX];
    int target_is_dir = 0;
    normalize_unveil_path(target_path, canon_target, sizeof(canon_target), &target_is_dir);

    // Also prepare alternate path with /bin prefix if not already starting with /bin
    char canon_bin[UNVEIL_PATH_MAX];
    canon_bin[0] = '\0';
    if (strncmp(canon_target, "/bin/", 5) != 0 && strcmp(canon_target, "/bin") != 0) {
        canon_bin[0] = '/';
        canon_bin[1] = 'b';
        canon_bin[2] = 'i';
        canon_bin[3] = 'n';
        strncpy(canon_bin + 4, canon_target, sizeof(canon_bin) - 5);
        canon_bin[sizeof(canon_bin) - 1] = '\0';
    }

    int best_idx = -1;
    size_t best_len = 0;

    for (size_t i = 0; i < current_unveil_count; i++) {
        struct unveil_rule* r = &current_unveil_rules[i];
        if (strcmp(r->path, canon_target) == 0 || (canon_bin[0] && strcmp(r->path, canon_bin) == 0)) {
            size_t l = strlen(r->path);
            if (l >= best_len) {
                best_len = l;
                best_idx = (int)i;
            }
        } else if (strcmp(r->path, "/") == 0) {
            if (1 > best_len) {
                best_len = 1;
                best_idx = (int)i;
            }
        } else if (r->is_dir) {
            size_t rlen = strlen(r->path);
            if ((strncmp(canon_target, r->path, rlen) == 0 && canon_target[rlen] == '/') ||
                (canon_bin[0] && strncmp(canon_bin, r->path, rlen) == 0 && canon_bin[rlen] == '/')) {
                if (rlen > best_len) {
                    best_len = rlen;
                    best_idx = (int)i;
                }
            }
        }
    }

    if (best_idx >= 0) {
        struct unveil_rule* r = &current_unveil_rules[best_idx];
        if ((r->perms & req_perm) == req_perm) {
            return 1;
        } else {
            serial_puts("[-] OpSec Unveil Block: Access denied (missing perms 0x");
            serial_print_hex((uint64_t)req_perm);
            serial_puts(") for '");
            serial_puts(target_path);
            serial_puts("'\n");
            return 0;
        }
    }

    serial_puts("[-] OpSec Unveil Block: Path not unveiled: '");
    serial_puts(target_path);
    serial_puts("'\n");
    return 0;
}

#define MAX_ARG_COUNT 16
#define MAX_ARG_LEN   128

/* Full ELF-64 Loader & Executable Runner */
int elf_load_and_run(const char* path, int argc, char* argv[]) {
    if (!path) return -1;

    char resolved[SELDFS_MAX_FILENAME];
    resolve_seldfs_path(path, resolved);

    // 0. Binary Gatekeeper: Unveil Execution check
    if (!check_unveil(resolved, UNVEIL_EXEC)) {
        serial_puts("[-] OpSec Unveil Gatekeeper: Execution denied by unveil contract for: '");
        serial_puts(resolved);
        serial_puts("'\n");
        return -1;
    }

    // 1. Binary Gatekeeper: Authorization check
    if (!seldfs_is_authorized_file(resolved)) {
        serial_puts("[-] OpSec Binary Gatekeeper: Blocked unauthorized binary: '");
        serial_puts(resolved);
        serial_puts("'\n");
        return -1;
    }

    // 2. Binary Gatekeeper: Cryptographic SHA-256 Attestation
    if (seldfs_verify_file(resolved) != 0) {
        serial_puts("[-] OpSec Binary Gatekeeper: SHA-256 verification FAILED for '");
        serial_puts(resolved);
        serial_puts("'. Tampered binary rejected!\n");
        return -1;
    }

    struct seldfs_inode inode;
    if (seldfs_get_file_info(resolved, &inode) != 0 || inode.size < sizeof(Elf64_Ehdr)) {
        serial_puts("[-] elf_load_and_run: File not found or too small: ");
        serial_puts(resolved);
        serial_puts("\n");
        return -1;
    }

    void* elf_buf = kmalloc(inode.size);
    if (!elf_buf) return -1;

    size_t bytes_read = 0;
    if (seldfs_read_file(resolved, elf_buf, inode.size, &bytes_read) != 0 || bytes_read != inode.size) {
        kfree(elf_buf);
        return -1;
    }

    uint64_t* proc_pml4_virt = NULL;
    uint64_t  proc_entry = 0;
    uint64_t  proc_pml4_phys = 0;
    int load_res = elf_load_binary(elf_buf, inode.size, &proc_pml4_virt, &proc_entry, &proc_pml4_phys);
    kfree(elf_buf);

    if (load_res != 0) {
        serial_puts("[-] elf_load_and_run: ELF parse/load error\n");
        return -1;
    }

    // Buffer argc / argv strings in kernel space before switching address spaces
    char k_args[MAX_ARG_COUNT][MAX_ARG_LEN];
    int k_argc = 0;

    if (argc > 0 && argv) {
        for (int i = 0; i < argc && k_argc < MAX_ARG_COUNT; i++) {
            if (argv[i]) {
                strncpy(k_args[k_argc], argv[i], MAX_ARG_LEN - 1);
                k_args[k_argc][MAX_ARG_LEN - 1] = '\0';
                k_argc++;
            }
        }
    }
    if (k_argc == 0) {
        strncpy(k_args[0], resolved, MAX_ARG_LEN - 1);
        k_args[0][MAX_ARG_LEN - 1] = '\0';
        k_argc = 1;
    }

    // Map stack arguments into top of user stack page (virtual 0x00007FFFFFFFE000)
    uint64_t stack_phys = vmm_get_mapping(proc_pml4_virt, 0x00007FFFFFFFE000ULL) & 0x000FFFFFFFFFF000ULL;
    if (!stack_phys) {
        vmm_destroy_address_space(proc_pml4_virt);
        return -1;
    }
    uint8_t* k_stack_page = (uint8_t*)phys_to_virt(stack_phys);

    // Place argument strings starting at offset 3500
    uint64_t str_offset = 3500;
    uint64_t user_str_vas[MAX_ARG_COUNT];

    for (int i = 0; i < k_argc; i++) {
        size_t slen = strlen(k_args[i]) + 1;
        if (str_offset + slen > 4080) break;
        memcpy(k_stack_page + str_offset, k_args[i], slen);
        user_str_vas[i] = 0x00007FFFFFFFE000ULL + str_offset;
        str_offset += slen;
    }

    // Place user argv pointers array at offset 3200 (8-byte aligned)
    uint64_t* user_argv_page = (uint64_t*)(k_stack_page + 3200);
    for (int i = 0; i < k_argc; i++) {
        user_argv_page[i] = user_str_vas[i];
    }
    user_argv_page[k_argc] = 0; // NULL terminator

    uint64_t child_argv_va = 0x00007FFFFFFFE000ULL + 3200;

    // Stack pointer for entry (16-byte aligned): offset 3104 (0xC20)
    uint64_t user_rsp = 0x00007FFFFFFFE000ULL + 3104;

    // Provide argc and argv on stack for ABI fallback
    uint64_t* stack_header = (uint64_t*)(k_stack_page + 3104);
    stack_header[0] = (uint64_t)k_argc;
    stack_header[1] = child_argv_va;

    // Save parent state
    uint64_t saved_brk = user_current_brk;
    user_current_brk = DEFAULT_USER_HEAP_BASE;

    uint32_t saved_pledge_mask = current_pledge_mask;
    uint8_t  saved_pledge_enforced = current_pledge_enforced;
    current_pledge_mask = 0xFFFFFFFF;
    current_pledge_enforced = 0;

    struct unveil_rule saved_unveil_rules[MAX_UNVEIL_RULES];
    memcpy(saved_unveil_rules, current_unveil_rules, sizeof(current_unveil_rules));
    size_t saved_unveil_count = current_unveil_count;
    uint8_t saved_unveil_enforced = current_unveil_enforced;
    uint8_t saved_unveil_locked = current_unveil_locked;
    if (!saved_unveil_enforced) {
        memset(current_unveil_rules, 0, sizeof(current_unveil_rules));
        current_unveil_count = 0;
        current_unveil_enforced = 0;
        current_unveil_locked = 0;
    }

    uint64_t parent_cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(parent_cr3));

    const char* base_bin = resolved;
    if (strncmp(base_bin, "/bin/", 5) == 0) base_bin += 5;
    else if (base_bin[0] == '/') base_bin += 1;

    int is_net_tool = (strcmp(base_bin, "tor") == 0 ||
                       strcmp(base_bin, "torbrowser") == 0 ||
                       strcmp(base_bin, "download") == 0 ||
                       strcmp(base_bin, "stealth") == 0);
    if (is_net_tool) {
        net_lease_acquire();
    }

    int exit_code = jump_to_userspace_asm((void (*)(void))proc_entry,
                                          (void*)user_rsp,
                                          proc_pml4_phys,
                                          (uint64_t)k_argc,
                                          (void*)child_argv_va);

    if (is_net_tool) {
        net_lease_release();
    }

    // Clean up child address space
    vmm_destroy_address_space(proc_pml4_virt);

    // Restore parent state
    user_current_brk = saved_brk;
    current_pledge_mask = saved_pledge_mask;
    current_pledge_enforced = saved_pledge_enforced;
    memcpy(current_unveil_rules, saved_unveil_rules, sizeof(current_unveil_rules));
    current_unveil_count = saved_unveil_count;
    current_unveil_enforced = saved_unveil_enforced;
    current_unveil_locked = saved_unveil_locked;

    if (parent_cr3 && parent_cr3 != vmm_get_kernel_pml4()) {
        vmm_switch_pml4(parent_cr3);
    }

    return exit_code;
}

static void system_reboot(void) __attribute__((noreturn));
static void system_reboot(void) {
    serial_puts("[!] System reboot requested. Resetting CPU...\n");
    vga_puts("\n[!] Rebooting system...\n");

    // OpSec: Sever network connections and perform Cold-Boot memory wipe
    net_abort_all_connections();
    pmm_secure_wipe_all_free();

    // 1. 8042 keyboard controller reset pulse
    for (int t = 0; t < 1000; t++) {
        uint8_t temp = inb(0x64);
        if (temp & 1) inb(0x60);
        if (!(temp & 2)) break;
    }
    outb(0x64, 0xFE);

    // 2. ACPI/PCI hard reset port 0xCF9
    outb(0xCF9, 0x02);
    outb(0xCF9, 0x06);
    outb(0xCF9, 0x0E);

    // 3. Fast reset port 0x92
    outb(0x92, (uint8_t)(inb(0x92) | 1));

    // 4. Triple fault via corrupted IDT
    struct { uint16_t limit; uint64_t base; } __attribute__((packed)) null_idt = {0, 0};
    __asm__ volatile ("lidt %0; int $3" : : "m"(null_idt));

    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}

static void system_poweroff(void) __attribute__((noreturn));
static void system_poweroff(void) {
    serial_puts("[!] System poweroff requested. Halting all processors...\n");
    vga_puts("\n[!] Powering off system...\n");

    // OpSec: Sever network connections, scrub framebuffer, and wipe RAM
    net_abort_all_connections();
    vga_clear();
    pmm_secure_wipe_all_free();

    // 1. QEMU / Bochs ACPI shutdown (port 0x604, value 0x2000)
    outw(0x604, 0x2000);

    // 2. Older QEMU pc-piix4 (port 0xB004, value 0x2000)
    outw(0xB004, 0x2000);

    // 3. VirtualBox (port 0x4004, value 0x3400)
    outw(0x4004, 0x3400);

    // 4. QEMU debugcon exit (port 0x501, 0x31)
    outb(0x501, 0x31);

    // 5. APM shutdown via port 0xB2 (APM_CNT)
    outb(0xB2, 0x00);

    // Fallback: Disable interrupts and halt
    serial_puts("[+] System halted. Safe to power off.\n");
    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}

/* Seld-Pledge Capability Verification Engine */
static int check_pledge(uint64_t sys_num, uint64_t a1) {
    if (!current_pledge_enforced) return 0;

    uint32_t req = 0;
    switch (sys_num) {
        case SYS_EXIT:
        case SYS_YIELD:
        case SYS_UPTIME:
        case SYS_GETPID:
        case SYS_SLEEP:
        case SYS_BRK:
        case SYS_CLEAR:
        case SYS_POLLKEY:
        case SYS_POLLMOUSE:
        case SYS_FRAMEBUFFER:
        case SYS_SET_CONSOLE_ROWS:
        case SYS_MEMINFO:
        case SYS_GETTASKS:
        case SYS_PLEDGE:
        case SYS_UNVEIL:
        case SYS_OPSEC_SET_JITTER:
        case SYS_OPSEC_GET_JITTER:
        case SYS_SELD:
            req = PLEDGE_STDIO;
            break;

        case SYS_WRITE:
            req = (a1 == 1 || a1 == 2) ? PLEDGE_STDIO : PLEDGE_WPATH;
            break;

        case SYS_READ:
            req = (a1 == 0) ? PLEDGE_STDIO : PLEDGE_RPATH;
            break;

        case SYS_SELD_VERIFY:
        case SYS_OPEN:
        case SYS_CLOSE:
        case 9:
        case SYS_STAT:
        case SYS_LISTDIR:
        case SYS_READFILE:
        case SYS_LSEEK:
            req = PLEDGE_RPATH;
            break;

        case SYS_WRITEFILE:
        case SYS_UNLINK:
            req = PLEDGE_WPATH;
            break;

        case SYS_SPAWN:
        case SYS_EXEC:
            req = PLEDGE_EXEC;
            break;

        case SYS_NET_INFO:
        case SYS_NET_PING:
        case SYS_NET_ARP:
        case SYS_NET_DOWNLOAD:
        case SYS_NET_TCP_CONNECT:
        case SYS_NET_TCP_SEND:
        case SYS_NET_TCP_RECV:
        case SYS_NET_TCP_CLOSE:
        case SYS_NET_SET_DESYNC:
        case SYS_NET_GET_DESYNC:
            req = PLEDGE_NET;
            break;

        case SYS_NET_DNS_RESOLVE:
            req = PLEDGE_DNS;
            break;

        case SYS_BEEP:
        case SYS_AUDIO_PLAY:
            req = PLEDGE_AUDIO;
            break;

        case SYS_IMMUNE_PURGE:
        case SYS_NET_SET_LOCK:
        case SYS_NET_GET_LOCK:
            req = PLEDGE_PURGE;
            break;

        case SYS_REBOOT:
        case SYS_POWEROFF:
            req = PLEDGE_REBOOT;
            break;

        default:
            req = 0;
            break;
    }

    if (req && (current_pledge_mask & req) == 0) {
        serial_puts("[-] OpSec PLEDGE VIOLATION: Syscall ");
        serial_print_dec(sys_num);
        serial_puts(" denied by active pledge mask 0x");
        serial_print_hex((uint64_t)current_pledge_mask);
        serial_puts(" (required 0x");
        serial_print_hex((uint64_t)req);
        serial_puts(")\n");
        return -1;
    }
    return 0;
}

uint64_t fast_syscall_dispatcher(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)a4;

    if (check_pledge(num, a1) != 0) {
        serial_puts("[-] OpSec: Terminating rogue/compromised process breaching pledge contract.\n");
        user_exit_to_kernel(134);
        return (uint64_t)-1;
    }

    switch (num) {
        case SYS_WRITE: {
            // a1 = fd, a2 = buffer, a3 = length
            uint64_t fd = a1;
            const char* buf = (const char*)a2;
            size_t count = (size_t)a3;
            if (count == 0) return 0;
            if (!validate_user_buffer(buf, count, 0)) {
                return (uint64_t)-1;
            }

            if (fd == 1 || fd == 2) {
                // Console stdout/stderr
                for (size_t i = 0; i < count; i++) {
                    vga_putchar(buf[i]);
                    serial_putchar(buf[i]);
                }
                return count;
            } else if (fd >= 3 && fd < MAX_USER_FDS && user_fds[fd].used) {
                // Universal write to SeldFS file
                size_t max_file_sz = SELDFS_DATA_BLOCKS_PER_FILE * SELDFS_BLOCK_SIZE;
                if (user_fds[fd].offset + count > max_file_sz) {
                    return (uint64_t)-1;
                }

                uint8_t* fbuf = (uint8_t*)kmalloc(max_file_sz);
                if (!fbuf) return (uint64_t)-1;
                memset(fbuf, 0, max_file_sz);

                size_t cur_len = 0;
                seldfs_read_file(user_fds[fd].filename, fbuf, max_file_sz, &cur_len);

                memcpy(fbuf + user_fds[fd].offset, buf, count);
                size_t new_len = user_fds[fd].offset + count;
                if (new_len < cur_len) {
                    new_len = cur_len;
                }

                if (seldfs_write_file(user_fds[fd].filename, fbuf, new_len) != 0) {
                    kfree(fbuf);
                    return (uint64_t)-1;
                }

                user_fds[fd].offset += (uint32_t)count;
                user_fds[fd].size = (uint32_t)new_len;
                kfree(fbuf);
                return count;
            }
            return (uint64_t)-1;
        }

        case SYS_READ: {
            // a1 = fd, a2 = buffer, a3 = length (blocking keyboard read or SeldFS file read)
            uint64_t fd = a1;
            char* buf = (char*)a2;
            size_t count = (size_t)a3;
            if (count == 0) return 0;
            if (!validate_user_buffer(buf, count, 1)) {
                return (uint64_t)-1;
            }

            if (fd == 0) {
                // Stdin console keyboard or serial read
                for (size_t i = 0; i < count; i++) {
                    if (serial_has_char()) {
                        buf[i] = serial_getchar();
                    } else {
                        buf[i] = kbd_getchar();
                    }
                }
                return count;
            } else if (fd >= 3 && fd < MAX_USER_FDS && user_fds[fd].used) {
                // Read directly from SeldFS file at current offset
                size_t bytes_read = 0;
                int res = seldfs_read_file_offset(user_fds[fd].filename, user_fds[fd].offset, buf, count, &bytes_read);
                if (res != 0) {
                    return (uint64_t)-1;
                }
                user_fds[fd].offset += (uint32_t)bytes_read;
                return bytes_read;
            }
            return (uint64_t)-1;
        }

        case SYS_OPEN:
        case 8: { // Support backwards compatibility for vector 8
            // a1 = path, a2 = flags
            const char* path = (const char*)a1;
            int flags = (int)a2;
            if (!validate_user_buffer(path, 1, 0)) {
                return (uint64_t)-1;
            }

            char filename[SELDFS_MAX_FILENAME];
            resolve_seldfs_path(path, filename);

            uint8_t req_perm = UNVEIL_READ;
            if (flags & (0x0001 /* O_WRONLY */ | 0x0002 /* O_RDWR */)) {
                req_perm |= UNVEIL_WRITE;
            }
            if (flags & 0x0040 /* O_CREAT */) {
                req_perm |= UNVEIL_CREATE;
            }
            if (!check_unveil(filename, req_perm)) {
                return (uint64_t)-1;
            }

            int target_fd = -1;
            for (int i = 3; i < MAX_USER_FDS; i++) {
                if (!user_fds[i].used) {
                    target_fd = i;
                    break;
                }
            }
            if (target_fd == -1) return (uint64_t)-1;

            struct seldfs_inode inode;
            int found = (seldfs_get_file_info(filename, &inode) == 0);

            if (!found) {
                if ((flags & 0x0040 /* O_CREAT */) || (flags & 0x0001 /* O_WRONLY */) || (flags & 0x0002 /* O_RDWR */)) {
                    if (seldfs_write_file(filename, "", 0) != 0 || seldfs_get_file_info(filename, &inode) != 0) {
                        return (uint64_t)-1;
                    }
                    found = 1;
                } else {
                    return (uint64_t)-1;
                }
            }

            user_fds[target_fd].used = 1;
            memset(user_fds[target_fd].filename, 0, sizeof(user_fds[target_fd].filename));
            strncpy(user_fds[target_fd].filename, filename, sizeof(user_fds[target_fd].filename) - 1);
            user_fds[target_fd].offset = 0;
            user_fds[target_fd].flags = (uint32_t)flags;
            user_fds[target_fd].size = found ? inode.size : 0;

            return (uint64_t)target_fd;
        }

        case SYS_CLOSE:
        case 9: { // Backwards compatibility vector 9
            // a1 = fd
            int fd = (int)a1;
            if (fd >= 3 && fd < MAX_USER_FDS && user_fds[fd].used) {
                user_fds[fd].used = 0;
                return 0;
            }
            return (uint64_t)-1;
        }

        case SYS_STAT: {
            // a1 = path, a2 = struct seld_stat*
            const char* path = (const char*)a1;
            struct seld_stat* st = (struct seld_stat*)a2;
            if (!validate_user_buffer(path, 1, 0) || !validate_user_buffer(st, sizeof(struct seld_stat), 1)) {
                return (uint64_t)-1;
            }

            char filename[SELDFS_MAX_FILENAME];
            resolve_seldfs_path(path, filename);

            if (!check_unveil(filename, UNVEIL_READ)) {
                return (uint64_t)-1;
            }

            struct seldfs_inode inode;
            if (seldfs_get_file_info(filename, &inode) != 0) {
                return (uint64_t)-1;
            }

            memset(st, 0, sizeof(struct seld_stat));
            strncpy(st->name, inode.filename, sizeof(st->name) - 1);
            st->size = inode.size;
            st->block_count = inode.block_count;
            st->flags = inode.used;
            memcpy(st->sha256, inode.sha256, 32);
            return 0;
        }

        case SYS_LISTDIR: {
            struct seld_dirent* dirents = NULL;
            size_t max_entries = 0;

            if (a3 > 0) {
                dirents = (struct seld_dirent*)a2;
                max_entries = (size_t)a3;
            } else {
                dirents = (struct seld_dirent*)a1;
                max_entries = (size_t)a2;
            }

            if (!dirents || max_entries == 0) {
                return (uint64_t)seldfs_get_file_count();
            }

            if (!validate_user_buffer(dirents, max_entries * sizeof(struct seld_dirent), 1)) {
                return (uint64_t)-1;
            }

            size_t written = 0;
            for (size_t i = 0; i < SELDFS_MAX_FILES && written < max_entries; i++) {
                struct seldfs_inode inode;
                if (seldfs_get_inode_by_index(i, &inode) == 1) {
                    memset(&dirents[written], 0, sizeof(struct seld_dirent));
                    strncpy(dirents[written].name, inode.filename, sizeof(dirents[written].name) - 1);
                    dirents[written].size = inode.size;
                    dirents[written].block_count = inode.block_count;
                    memcpy(dirents[written].sha256, inode.sha256, 32);
                    written++;
                }
            }
            return (uint64_t)written;
        }

        case SYS_BRK: {
            // a1 = new_brk
            // User heap break begins at DEFAULT_USER_HEAP_BASE (0x0000000040000000ULL)
            if (user_current_brk == 0) {
                user_current_brk = DEFAULT_USER_HEAP_BASE;
            }

            if (a1 == 0) {
                return user_current_brk;
            }

            if (a1 < DEFAULT_USER_HEAP_BASE || a1 >= 0x0000700000000000ULL) {
                return (uint64_t)-1;
            }

            if (a1 > user_current_brk) {
                if (!ram_is_driver_enabled()) {
                    kernel_panic("sys_brk failed: physical memory manager exhausted (out of memory)");
                }
                uint64_t cr3;
                __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
                uint64_t* user_pml4 = (uint64_t*)phys_to_virt(cr3);

                uint64_t start_va = (user_current_brk + PAGE_SIZE - 1) & ~0xFFFULL;
                uint64_t end_va   = (a1 + PAGE_SIZE - 1) & ~0xFFFULL;

                for (uint64_t va = start_va; va < end_va; va += PAGE_SIZE) {
                    uint64_t existing = vmm_get_mapping(user_pml4, va);
                    if (!existing) {
                        void* frame = pmm_alloc_frame();
                        if (!frame) {
                            return (uint64_t)-1; // Out of memory
                        }
                        memset(phys_to_virt((uint64_t)frame), 0, PAGE_SIZE);
                        vmm_map_page(user_pml4, va, (uint64_t)frame,
                                     VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_USER | VMM_FLAG_NO_EXECUTE);
                    }
                }
            }

            user_current_brk = a1;
            return user_current_brk;
        }

        case SYS_SPAWN:
        case SYS_EXEC: {
            // a1 = path, a2 = argv (optional char* const argv[])
            const char* path = (const char*)a1;
            char* const* u_argv = (char* const*)a2;

            if (!validate_user_buffer(path, 1, 0)) {
                return (uint64_t)-1;
            }

            char k_path[SELDFS_MAX_FILENAME];
            size_t idx = 0;
            while (idx < SELDFS_MAX_FILENAME - 1) {
                if (!validate_user_buffer(path + idx, 1, 0)) return (uint64_t)-1;
                char c = path[idx];
                if (c == '\0') break;
                k_path[idx++] = c;
            }
            k_path[idx] = '\0';
            if (idx == 0) return (uint64_t)-1;

            char parsed_args[MAX_ARG_COUNT][MAX_ARG_LEN];
            char* parsed_ptrs[MAX_ARG_COUNT];
            int p_argc = 0;

            if (u_argv && validate_user_buffer(u_argv, sizeof(char*), 0)) {
                for (int i = 0; i < MAX_ARG_COUNT; i++) {
                    if (!validate_user_buffer(&u_argv[i], sizeof(char*), 0)) break;
                    char* u_str = u_argv[i];
                    if (!u_str) break;
                    if (!validate_user_buffer(u_str, 1, 0)) break;

                    size_t sidx = 0;
                    while (sidx < MAX_ARG_LEN - 1) {
                        if (!validate_user_buffer(u_str + sidx, 1, 0)) break;
                        char ch = u_str[sidx];
                        if (ch == '\0') break;
                        parsed_args[p_argc][sidx++] = ch;
                    }
                    parsed_args[p_argc][sidx] = '\0';
                    parsed_ptrs[p_argc] = parsed_args[p_argc];
                    p_argc++;
                }
            }

            if (p_argc == 0) {
                strncpy(parsed_args[0], k_path, MAX_ARG_LEN - 1);
                parsed_args[0][MAX_ARG_LEN - 1] = '\0';
                parsed_ptrs[0] = parsed_args[0];
                p_argc = 1;
            }

            int exit_code = elf_load_and_run(k_path, p_argc, parsed_ptrs);
            return (uint64_t)exit_code;
        }

        case SYS_YIELD: {
            sched_yield();
            return 0;
        }

        case SYS_UPTIME: {
            return pit_get_uptime_ms();
        }

        case SYS_GETPID: {
            struct task* cur = sched_get_current();
            return cur ? cur->id : 1;
        }

        case SYS_SELD_VERIFY: {
            if (a1 == 0) {
                return 0x5E1D0001ULL;
            } else if (a1 == 1) {
                return (a2 < 0x0000800000000000ULL && a2 >= 0x1000ULL) ? 1 : 0;
            }
            return a1 ^ 0x5E1D5E1DULL;
        }

        case SYS_SLEEP: {
            pit_sleep_ms(a1);
            return 0;
        }

        case SYS_UNLINK: {
            const char* path = (const char*)a1;
            if (!validate_user_buffer(path, 1, 0)) return (uint64_t)-1;
            char resolved[SELDFS_MAX_FILENAME];
            resolve_seldfs_path(path, resolved);
            if (!check_unveil(resolved, UNVEIL_CREATE)) return (uint64_t)-1;
            return (uint64_t)seldfs_delete_file(resolved);
        }

        case SYS_GETTASKS: {
            struct snl_task_info* user_tasks = (struct snl_task_info*)a1;
            size_t max_t = (size_t)a2;
            if (max_t == 0 || !validate_user_buffer(user_tasks, max_t * sizeof(struct snl_task_info), 1)) {
                return (uint64_t)-1;
            }

            struct task k_tasks[MAX_TASKS];
            int count = sched_get_tasks(k_tasks);
            size_t copied = 0;
            for (int i = 0; i < count && copied < max_t; i++) {
                user_tasks[copied].id = k_tasks[i].id;
                memcpy(user_tasks[copied].name, k_tasks[i].name, sizeof(user_tasks[copied].name));
                user_tasks[copied].state = (uint32_t)k_tasks[i].state;
                user_tasks[copied].rsp = k_tasks[i].rsp;
                user_tasks[copied].stack_base = k_tasks[i].stack_base;
                user_tasks[copied].sleep_until_ticks = k_tasks[i].sleep_until_ticks;
                user_tasks[copied].runtime_ticks = k_tasks[i].runtime_ticks;
                copied++;
            }
            return (uint64_t)copied;
        }

        case SYS_MEMINFO: {
            struct snl_meminfo* user_info = (struct snl_meminfo*)a1;
            if (!validate_user_buffer(user_info, sizeof(struct snl_meminfo), 1)) {
                return (uint64_t)-1;
            }

            struct pmm_stats pstats = pmm_get_stats();
            struct heap_stats hstats = kmalloc_get_stats();
            user_info->total_ram = pstats.total_memory;
            user_info->used_ram = pstats.used_memory;
            user_info->free_ram = pstats.free_memory;
            user_info->heap_size = hstats.heap_size;
            user_info->heap_used = hstats.used_bytes;
            user_info->heap_free = hstats.free_bytes;
            return 0;
        }

        case SYS_CLEAR: {
            vga_clear();
            serial_puts("\033[2J\033[H");
            return 0;
        }

        case SYS_READFILE: {
            const char* path = (const char*)a1;
            void* buf = (void*)a2;
            size_t max_len = (size_t)a3;
            if (!validate_user_buffer(path, 1, 0) || !validate_user_buffer(buf, max_len, 1)) {
                return (uint64_t)-1;
            }

            char resolved[SELDFS_MAX_FILENAME];
            resolve_seldfs_path(path, resolved);

            if (!check_unveil(resolved, UNVEIL_READ)) {
                return (uint64_t)-1;
            }

            size_t out_len = 0;
            if (seldfs_read_file(resolved, buf, max_len, &out_len) != 0) {
                return (uint64_t)-1;
            }
            return (uint64_t)out_len;
        }

        case SYS_WRITEFILE: {
            const char* path = (const char*)a1;
            const void* buf = (const void*)a2;
            size_t len = (size_t)a3;
            if (!validate_user_buffer(path, 1, 0) || !validate_user_buffer(buf, len, 0)) {
                return (uint64_t)-1;
            }

            char resolved[SELDFS_MAX_FILENAME];
            resolve_seldfs_path(path, resolved);

            if (!check_unveil(resolved, UNVEIL_WRITE)) {
                return (uint64_t)-1;
            }

            if (seldfs_write_file(resolved, buf, len) != 0) {
                return (uint64_t)-1;
            }
            return 0;
        }

        case SYS_LSEEK: {
            // a1 = fd, a2 = offset, a3 = whence (0=SEEK_SET, 1=SEEK_CUR, 2=SEEK_END)
            int fd = (int)a1;
            long offset = (long)a2;
            int whence = (int)a3;

            if (fd >= 3 && fd < MAX_USER_FDS && user_fds[fd].used) {
                struct seldfs_inode inode;
                if (seldfs_get_file_info(user_fds[fd].filename, &inode) != 0) {
                    return (uint64_t)-1;
                }
                uint32_t fsize = inode.size;
                long new_offset = 0;

                if (whence == 0 /* SEEK_SET */) {
                    new_offset = offset;
                } else if (whence == 1 /* SEEK_CUR */) {
                    new_offset = (long)user_fds[fd].offset + offset;
                } else if (whence == 2 /* SEEK_END */) {
                    new_offset = (long)fsize + offset;
                } else {
                    return (uint64_t)-1;
                }

                if (new_offset < 0) {
                    return (uint64_t)-1;
                }

                user_fds[fd].offset = (uint32_t)new_offset;
                user_fds[fd].size = fsize;
                return (uint64_t)new_offset;
            }
            return (uint64_t)-1;
        }

        case SYS_FRAMEBUFFER: {
            if (!vga_is_gpu_enabled()) {
                kernel_panic("sys_framebuffer failed: video controller unmapped (vga_is_gpu_enabled() == 0)");
            }
            // a1 = struct seld_fb_info* user_fb
            struct fb_info* kfb = vga_get_fb_info();
            if (!kfb || kfb->phys_addr == 0) {
                return (uint64_t)-1;
            }

            struct user_fb_struct {
                void*    framebuffer;
                uint32_t width;
                uint32_t height;
                uint32_t pitch;
                uint32_t bpp;
            }* ufb = (struct user_fb_struct*)a1;

            if (!validate_user_buffer(ufb, sizeof(*ufb), 1)) {
                return (uint64_t)-1;
            }

            // Map physical framebuffer into userspace at fixed virtual address 0x00000000A0000000ULL
            uint64_t user_fb_va = 0x00000000A0000000ULL;
            uint64_t fb_size = (uint64_t)kfb->pitch * kfb->height;
            if (fb_size == 0) fb_size = (uint64_t)kfb->width * kfb->height * (kfb->bpp / 8);
            if (fb_size < 680 * 334 * 4) fb_size = 680 * 334 * 4;

            uint64_t cr3;
            __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
            uint64_t* user_pml4 = (uint64_t*)phys_to_virt(cr3);

            for (uint64_t off = 0; off < fb_size; off += PAGE_SIZE) {
                uint64_t va = user_fb_va + off;
                uint64_t pa = kfb->phys_addr + off;
                vmm_map_page(user_pml4, va, pa,
                             VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_USER | VMM_FLAG_NO_EXECUTE);
            }

            ufb->framebuffer = (void*)user_fb_va;
            ufb->width = kfb->width;
            ufb->height = kfb->height;
            ufb->pitch = kfb->pitch;
            ufb->bpp = kfb->bpp;

            return 0;
        }

        case SYS_POLLKEY: {
            // a1 = struct kbd_event* ev
            struct kbd_event* uev = (struct kbd_event*)a1;
            if (!validate_user_buffer(uev, sizeof(*uev), 1)) {
                return (uint64_t)-1;
            }

            struct kbd_event kev;
            int has_ev = kbd_poll_event(&kev);
            if (has_ev) {
                uev->scancode = kev.scancode;
                uev->pressed = kev.pressed;
                return 1;
            }
            return 0;
        }

        case SYS_POLLMOUSE: {
            // a1 = struct mouse_event* ev
            struct mouse_event* uev = (struct mouse_event*)a1;
            if (!validate_user_buffer(uev, sizeof(*uev), 1)) {
                return (uint64_t)-1;
            }

            struct mouse_event mev;
            int has_ev = mouse_poll_event(&mev);
            if (has_ev) {
                *uev = mev;
                return 1;
            }
            return 0;
        }

        case SYS_BEEP: {
            // a1 = freq_hz, a2 = duration_ms
            uint32_t freq = (uint32_t)a1;
            uint32_t dur = (uint32_t)a2;

            if (dur == 0) {
                // Continuous tone control (Snd primitive)
                if (freq == 0) {
                    audio_stop_tone();
                } else {
                    if (freq > 20000) freq = 20000;
                    audio_play_tone(freq);
                }
            } else {
                // Timed beep with overflow protection
                if (freq > 20000) freq = 20000;
                if (dur > 10000) dur = 10000;
                audio_beep(freq, dur);
            }
            return 0;
        }

        case SYS_AUDIO_PLAY: {
            // a1 = const uint8_t* pcm_data, a2 = size_t len, a3 = uint32_t sample_rate
            const void* buf = (const void*)a1;
            size_t len = (size_t)a2;
            uint32_t rate = (uint32_t)a3;
            if (len == 0) return 0;
            if (!validate_user_buffer(buf, len, 0)) {
                return (uint64_t)-1;
            }
            return (uint64_t)audio_play_pcm((const uint8_t*)buf, len, rate);
        }

        case SYS_SET_CONSOLE_ROWS: {
            size_t rows = (size_t)a1;
            vga_set_console_rows(rows);
            return 0;
        }

        case SYS_NET_INFO: {
            // a1 = struct seld_net_info*
            struct seld_net_info* uinfo = (struct seld_net_info*)a1;
            if (!validate_user_buffer(uinfo, sizeof(*uinfo), 1)) {
                return (uint64_t)-1;
            }

            struct net_config cfg = net_get_config();
            struct net_stats st = net_get_stats();

            struct seld_net_info kinfo;
            memset(&kinfo, 0, sizeof(kinfo));
            kinfo.ip = cfg.ip;
            kinfo.netmask = cfg.netmask;
            kinfo.gateway = cfg.gateway;
            kinfo.dns = cfg.dns;
            memcpy(kinfo.mac, cfg.mac, 6);
            kinfo.link_up = cfg.link_up;
            kinfo.rx_frames = st.rx_frames;
            kinfo.tx_frames = st.tx_frames;
            kinfo.rx_bytes = st.rx_bytes;
            kinfo.tx_bytes = st.tx_bytes;
            kinfo.rx_dropped = st.rx_dropped;
            kinfo.rx_checksum_errors = st.rx_checksum_errors;

            memcpy(uinfo, &kinfo, sizeof(kinfo));
            return 0;
        }

        case SYS_NET_PING: {
            // a1 = target_ip, a2 = seq, a3 = uint32_t* rtt_ms
            uint32_t target_ip = (uint32_t)a1;
            uint16_t seq = (uint16_t)a2;
            uint32_t* urtt = (uint32_t*)a3;

            if (urtt && !validate_user_buffer(urtt, sizeof(uint32_t), 1)) {
                return (uint64_t)-1;
            }

            uint32_t rtt = 0;
            int res = net_ping(target_ip, seq, &rtt);
            if (res == 0 && urtt) {
                *urtt = rtt;
            }
            return (uint64_t)res;
        }

        case SYS_NET_ARP: {
            // a1 = struct seld_arp_entry* entries, a2 = max_entries
            struct seld_arp_entry* uentries = (struct seld_arp_entry*)a1;
            size_t max_e = (size_t)a2;

            if (!uentries || max_e == 0 ||
                !validate_user_buffer(uentries, max_e * sizeof(struct seld_arp_entry), 1)) {
                return (uint64_t)-1;
            }

            struct arp_entry ktable[ARP_TABLE_SIZE];
            int count = net_get_arp_table(ktable, ARP_TABLE_SIZE);
            if (count > (int)max_e) count = (int)max_e;

            for (int i = 0; i < count; i++) {
                uentries[i].ip = ktable[i].ip;
                memcpy(uentries[i].mac, ktable[i].mac, 6);
                uentries[i].valid = ktable[i].valid;
                uentries[i].timestamp_ms = ktable[i].timestamp_ms;
            }

            return (uint64_t)count;
        }

        case SYS_NET_DOWNLOAD: {
            uint32_t server_ip = (uint32_t)a1;
            uint16_t port = (uint16_t)a2;
            const char* url_path = (const char*)a3;
            const char* local_path = (const char*)a4;

            if (!url_path || !local_path ||
                !validate_user_buffer(url_path, 1, 0) ||
                !validate_user_buffer(local_path, 1, 0)) {
                return (uint64_t)-1;
            }

            if (server_ip == 0) {
                struct net_config cfg = net_get_config();
                server_ip = cfg.gateway;
            }
            if (port == 0) {
                port = 8080;
            }

            char resolved_path[SELDFS_MAX_FILENAME];
            resolve_seldfs_path(local_path, resolved_path);

            int res = net_download_to_fs(server_ip, port, url_path, resolved_path);
            return (uint64_t)res;
        }

        case SYS_NET_TCP_CONNECT: {
            uint32_t server_ip = (uint32_t)a1;
            uint16_t port = (uint16_t)a2;
            if (server_ip == 0) {
                struct net_config cfg = net_get_config();
                server_ip = cfg.gateway;
            }
            int sock = net_tcp_socket_connect(server_ip, port);
            return (uint64_t)sock;
        }

        case SYS_NET_TCP_SEND: {
            int sock_id = (int)a1;
            const void* ubuf = (const void*)a2;
            size_t len = (size_t)a3;
            if (!ubuf || len == 0 || !validate_user_buffer(ubuf, len, 0)) {
                return (uint64_t)-1;
            }
            int sent = net_tcp_socket_send(sock_id, ubuf, len);
            return (uint64_t)sent;
        }

        case SYS_NET_TCP_RECV: {
            int sock_id = (int)a1;
            void* ubuf = (void*)a2;
            size_t max_len = (size_t)a3;
            uint32_t timeout_ms = (uint32_t)a4;
            if (!ubuf || max_len == 0 || !validate_user_buffer(ubuf, max_len, 1)) {
                return (uint64_t)-1;
            }
            int recvd = net_tcp_socket_recv(sock_id, ubuf, max_len, timeout_ms);
            return (uint64_t)recvd;
        }

        case SYS_NET_TCP_CLOSE: {
            int sock_id = (int)a1;
            int res = net_tcp_socket_close(sock_id);
            return (uint64_t)res;
        }

        case SYS_NET_DNS_RESOLVE: {
            // a1 = const char* hostname, a2 = uint32_t* ip_out
            const char* uhost = (const char*)a1;
            uint32_t* uip = (uint32_t*)a2;
            if (!uhost || !uip || !validate_user_buffer(uhost, 1, 0) || !validate_user_buffer(uip, sizeof(uint32_t), 1)) {
                return (uint64_t)-1;
            }
            uint32_t resolved_ip = 0;
            int res = net_dns_resolve(uhost, &resolved_ip);
            if (res == 0) {
                *uip = resolved_ip;
                return 0;
            }
            return (uint64_t)res;
        }

        case 40: /* SYS_DRV_OFF */ {
            const char* drv = (const char*)a1;
            if (!drv || !validate_user_buffer(drv, 1, 0)) return (uint64_t)-1;
            char k_drv[16];
            size_t di = 0;
            while (di < sizeof(k_drv) - 1) {
                if (!validate_user_buffer(drv + di, 1, 0)) return (uint64_t)-1;
                char ch = drv[di];
                k_drv[di] = ch;
                if (ch == '\0') break;
                di++;
            }
            k_drv[sizeof(k_drv) - 1] = '\0';

            if (strcmp(k_drv, "gpu") == 0) {
                return (uint64_t)vga_driver_disable();
            } else if (strcmp(k_drv, "cpu") == 0) {
                return (uint64_t)cpu_driver_disable();
            } else if (strcmp(k_drv, "ram") == 0) {
                return (uint64_t)ram_driver_disable();
            }
            return (uint64_t)-2;
        }

        case SYS_SELD: {
            return 0x5E1D5EC;
        }

        case SYS_IMMUNE_PURGE: {
            int wiped_files = seldfs_purge_untrusted();
            int killed_tasks = sched_purge_unauthorized_tasks();
            int aborted_socks = net_abort_all_connections();
            net_set_airgap(NET_AIRGAP_ONDEMAND);

            serial_puts("[+] SYS_IMMUNE_PURGE: Filesystem, tasks and sockets sanitized.\n");
            uint64_t summary = (uint64_t)(wiped_files & 0xFFFF) |
                               ((uint64_t)(killed_tasks & 0xFF) << 16) |
                               ((uint64_t)(aborted_socks & 0xFF) << 24);
            return summary;
        }

        case SYS_NET_SET_LOCK: {
            int mode = (int)a1;
            if (mode == 0) {
                net_set_airgap(NET_AIRGAP_UNLOCKED);
            } else if (mode == 1) {
                net_set_airgap(NET_AIRGAP_LOCKED);
                net_abort_all_connections();
            } else if (mode == 2) {
                net_set_airgap(NET_AIRGAP_ONDEMAND);
            } else if (mode == 3) {
                net_lease_acquire();
            } else if (mode == 4) {
                net_lease_release();
            } else if (mode == 5) {
                net_set_airgap(NET_AIRGAP_STEALTH);
            }
            return 0;
        }

        case SYS_NET_GET_LOCK: {
            return (uint64_t)net_get_airgap();
        }

        case SYS_NET_SET_DESYNC: {
            int mode = (int)a1;
            net_set_desync(mode);
            return 0;
        }

        case SYS_NET_GET_DESYNC: {
            return (uint64_t)net_get_desync();
        }

        case SYS_REBOOT: {
            system_reboot();
            return 0;
        }

        case SYS_POWEROFF: {
            system_poweroff();
            return 0;
        }

        case SYS_PLEDGE: {
            uint32_t requested = (uint32_t)a1;
            if (current_pledge_enforced) {
                // Monotonic restriction: privileges can only be dropped, never escalated
                current_pledge_mask &= requested;
            } else {
                current_pledge_mask = requested;
                current_pledge_enforced = 1;
            }
            serial_puts("[+] OpSec: Active process pledge mask set to 0x");
            serial_print_hex((uint64_t)current_pledge_mask);
            serial_puts("\n");
            return 0;
        }

        case SYS_UNVEIL: {
            const char* path = (const char*)a1;
            const char* perms = (const char*)a2;
            return (uint64_t)sys_unveil_handler(path, perms);
        }

        case SYS_OPSEC_SET_JITTER: {
            kbd_set_jitter((int)a1);
            serial_puts("[+] OpSec: Keyboard THL Timing Jitter ");
            serial_puts((int)a1 ? "ENABLED (50ms Quantized PIT Batching)\n" : "DISABLED (Direct Low-Latency)\n");
            return 0;
        }

        case SYS_OPSEC_GET_JITTER: {
            return (uint64_t)kbd_get_jitter();
        }

        case SYS_EXIT: {
            user_exit_to_kernel(a1);
            return 0;
        }

        default:
            return (uint64_t)-1;
    }
}

int jump_to_userspace(void (*user_func)(void), void* user_stack_top, uint64_t user_cr3) {
    if (user_current_brk == 0) {
        user_current_brk = DEFAULT_USER_HEAP_BASE;
    }
    return jump_to_userspace_asm(user_func, user_stack_top, user_cr3, 0, NULL);
}
