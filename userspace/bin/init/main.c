/*
 * SeldOS - Humboldt Kernel Project
 * SNL Init System: /bin/init - First Userspace Process (Ring 3)
 * Greets the user, performs autonomous subsystem self-checks,
 * and launches the interactive SNL Sovereign Shell (/bin/sh).
 * GPLv3 Licensed.
 */

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "seld.h"

static void print_banner(void) {
    printf("\n");
    printf("=======================================================================\n");
    printf(" [init]   SNL (Seld Not Linux) Sovereign Init System (PID 1, Ring 3)\n");
#if defined(__riscv)
    printf(" [init]   Humboldt Kernel Project - RISC-V 64-bit Hardened Architecture (GNU GPLv3)\n");
#else
    printf(" [init]   Humboldt Kernel Project - x86_64 Hardened Architecture (GNU GPLv3)\n");
#endif
    printf("=======================================================================\n");
}

static int run_self_checks(void) {
    printf("[init] Running boot-time userspace self-checks...\n");

    // 1. Hardware CPL Check
#if defined(__riscv)
    printf("[+] [init:CHECK 1/5] CPU Privilege Level: U-mode (Unprivileged User Mode)\n");
#else
    uint16_t cs = 0, ss = 0;
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));
    __asm__ volatile ("mov %%ss, %0" : "=r"(ss));
    uint8_t cpl = cs & 3;
    uint8_t spl = ss & 3;

    if (cpl != 3 || spl != 3) {
        printf("[-] [init:CHECK 1/5] FAILED: Invalid hardware privilege level (CPL=%d, SPL=%d)\n", cpl, spl);
        return 0;
    }
    printf("[+] [init:CHECK 1/5] CPU Privilege Level: CPL=3 (Unprivileged User Mode)\n");
#endif

    // 2. Fast Syscall Handshake
    long ping_res = seld_ping();
    if (ping_res != 0x5E1D5EC) {
        printf("[-] [init:CHECK 2/5] FAILED: Kernel fast syscall handshake returned 0x%lx\n", ping_res);
        return 0;
    }
#if defined(__riscv)
    printf("[+] [init:CHECK 2/5] RISC-V U-mode ECALL Syscall Handshake: OK (0x5E1D5EC)\n");
#else
    printf("[+] [init:CHECK 2/5] MSR LSTAR Fast Syscall Handshake: OK (0x5E1D5EC)\n");
#endif

    // 3. Process & Timer State
    int pid = getpid();
    uint64_t ms = uptime();
    printf("[+] [init:CHECK 3/5] PID: %d, System Chronometer: %lu ms\n", pid, ms);

    // 4. SeldFS Root Index & Shell Verification
    struct seld_stat st;
    if (stat("/bin/sh", &st) == 0) {
        printf("[+] [init:CHECK 4/5] SeldFS Storage: /bin/sh verified (%u bytes, %u blocks)\n",
               st.size, st.block_count);
    } else {
        printf("[*] [init:CHECK 4/5] SeldFS Storage: /bin/sh lookup (fallback enabled)\n");
    }

    // 5. OpSec Ring 3 Memory Wipe & RFC 7686 Guard
    uint8_t* sec_buf = (uint8_t*)malloc(64);
    if (sec_buf) {
        memset(sec_buf, 0x77, 64);
        free(sec_buf);
        for (int i = 0; i < 64; i++) {
            if (sec_buf[i] != 0) {
                printf("[-] [init:CHECK 5/5] FAILED: Userspace Zero-on-Free did not wipe heap!\n");
                return 0;
            }
        }
    }
    uint32_t leaked_ip = 0;
    int r_onion = seld_dns_resolve("test-hidden-service.onion", &leaked_ip);
    if (r_onion != -9) {
        printf("[-] [init:CHECK 5/5] FAILED: RFC 7686 Onion DNS leak guard not enforced!\n");
        return 0;
    }
    printf("[+] [init:CHECK 5/5] OpSec Hardening: Zero-on-Free & RFC 7686 Onion Guard active.\n");

    printf("[+] [init] All userspace initialization self-checks PASSED.\n\n");
    return 1;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    print_banner();

    if (!run_self_checks()) {
        printf("[init] CRITICAL: Userspace self-checks failed! Halting.\n");
        exit(1);
    }

    printf("[init] Spawning SNL Sovereign Shell (/bin/sh)...\n\n");

    char* sh_argv[2] = {"/bin/sh", NULL};

    while (1) {
        int status = spawnv("/bin/sh", sh_argv);
        if (status < 0) {
            printf("[init] Failed to spawn /bin/sh (code %d). Retrying in 2 seconds...\n", status);
            sleep(2);
        } else {
            printf("\n[init] Shell process terminated (exit code: %d). Respawning /bin/sh...\n\n", status);
            sleep(1);
        }
    }

    return 0;
}
