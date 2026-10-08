/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * SeldShell - Ring 0 Hardened Command Interpreter
 * Implementation of secure system control, diagnostics, and OpSec verification.
 * GPLv3 Licensed.
 */

#include "shell.h"
#include "vga.h"
#include "serial.h"
#include "kbd.h"
#include "string.h"
#include "io.h"
#include "pmm.h"
#include "kmalloc.h"
#include "ata.h"
#include "seldfs.h"
#include "rand.h"
#include "sha256.h"
#include "pit.h"
#include "sched.h"
#include "vmm.h"
#include "fast_syscall.h"
#include "selftest.h"
#include "elf.h"
#include "net.h"
#include "e1000.h"
#include "panic.h"

#define MAX_CMD_LEN 128
#define MAX_ARGS 16

static void print_out(const char* str) {
    vga_puts(str);
    serial_puts(str);
}

static void print_char(char c) {
    vga_putchar(c);
    serial_putchar(c);
}

static void print_hex64(uint64_t val) {
    vga_print_hex(val);
    serial_print_hex(val);
}

static void print_dec64(uint64_t val) {
    vga_print_dec(val);
    serial_print_dec(val);
}

/* System commands */
static void cmd_help(int argc, char* argv[]) {
    (void)argc; (void)argv;
    print_out("SeldShell Internal Utility Set (GPLv3):\n");
    print_out("  help          Display available command definitions\n");
    print_out("  clear         Clear frame buffer\n");
    print_out("  uname         Display kernel identification and security state\n");
    print_out("  license       Print GNU General Public License manifesto\n");
    print_out("  creg          Read privilege control registers (CR0, CR3, CR4)\n");
    print_out("  cpuid         Query processor vendor identity string\n");
    print_out("  meminfo       Output physical paging baseline statistics\n");
    print_out("  free          Display PMM physical RAM and kmalloc heap statistics\n");
    print_out("  alloc <bytes> Allocate dynamic memory in Ring 0 heap\n");
    print_out("  kfree <hex>   Release allocated dynamic heap block\n");
    print_out("  hexdump <addr> [len] Hexadecimal dump of physical/virtual memory\n");
    print_out("  disks         Inspect attached ATA storage devices\n");
    print_out("  mkfs          Format primary drive with SeldFS filesystem\n");
    print_out("  ls            List files on mounted SeldFS volume\n");
    print_out("  cat <file>    Display contents of text file\n");
    print_out("  write <f> <t> Write text string to file on disk\n");
    print_out("  rm <file>     Remove file from SeldFS volume\n");
    print_out("  rand [count]  Generate cryptographically secure random 64-bit integers\n");
    print_out("  entropy       Inspect hardware entropy sources (RDRAND/RDTSC)\n");
    print_out("  sha256 <file> Compute cryptographic SHA-256 digest of file\n");
    print_out("  verify <file> Verify cryptographic SHA-256 integrity of file\n");
    print_out("  uptime        Display system running time and PIT tick count\n");
    print_out("  sleep <ms>    Suspend execution for specified milliseconds\n");
    print_out("  ps            List all active supervisor threads in Ring 0\n");
    print_out("  spawn         Spawn background supervisor test worker thread\n");
    print_out("  selftest      Execute kernel subsystem self-tests (PMM, Heap, SHA, Sched)\n");
    print_out("  run3 [file]   Launch isolated Ring 3 ELF binary or built-in payload\n");
    print_out("  snl <binary>  Execute ELF-64 binary directly from SeldFS (Seld not's Linux)\n");
    print_out("  echo [args..] Output arguments to standard system sinks\n");
    print_out("  ifconfig      Display network interface status and stats (Intel e1000)\n");
    print_out("  ping <ip>     Send ICMP ECHO_REQUEST packets to network host\n");
    print_out("  arp           Display kernel Address Resolution Protocol (ARP) cache\n");
    print_out("  gpu drv off   Brutally disable GPU display driver (triggers kernel panic)\n");
    print_out("  cpu drv off   Brutally disable CPU scheduler driver (triggers kernel panic)\n");
    print_out("  ram drv off   Brutally disable RAM memory driver (triggers kernel panic)\n");
    print_out("  reboot        Perform hard system reset via 8042 keyboard controller\n");
}

static void cmd_clear(int argc, char* argv[]) {
    (void)argc; (void)argv;
    vga_clear();
}

static void cmd_uname(int argc, char* argv[]) {
    (void)argc; (void)argv;
    print_out("SeldOS kernel-humboldt-x86_64 0.1.0-sec-gplv3 #1 SMP Mon Sep 28 2026 x86_64 Seld/GNU\n");
    print_out("Privilege execution tier: Ring 0 (Direct Supervisor Hardened Mode)\n");
}

static void cmd_license(int argc, char* argv[]) {
    (void)argc; (void)argv;
    print_out("SeldOS & SeldShell are part of the Humboldt Free Software Project.\n");
    print_out("Copyright (C) 2026 Free Software Foundation, Inc.\n");
    print_out("Licensed under the GNU General Public License version 3 (GPLv3).\n");
    print_out("Freedom 0: Run the program for any purpose.\n");
    print_out("Freedom 1: Study how the program works and adapt it to your needs.\n");
    print_out("Freedom 2: Redistribute copies so you can help your neighbor.\n");
    print_out("Freedom 3: Improve the program, and release improvements to the public.\n");
}

static void cmd_creg(int argc, char* argv[]) {
    (void)argc; (void)argv;
#if defined(__riscv)
    uint64_t sstatus, satp, stvec, scause;
    __asm__ volatile ("csrr %0, sstatus" : "=r"(sstatus));
    __asm__ volatile ("csrr %0, satp" : "=r"(satp));
    __asm__ volatile ("csrr %0, stvec" : "=r"(stvec));
    __asm__ volatile ("csrr %0, scause" : "=r"(scause));

    print_out("Supervisor CSR Status:\n");
    print_out("  sstatus: "); print_hex64(sstatus); print_out(" [Supervisor Status]\n");
    print_out("  satp:    "); print_hex64(satp);    print_out(" [Sv39 Page Table Root]\n");
    print_out("  stvec:   "); print_hex64(stvec);   print_out(" [Trap Vector Address]\n");
    print_out("  scause:  "); print_hex64(scause);  print_out(" [Last Exception Cause]\n");
#else
    uint64_t cr0, cr2, cr3, cr4;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));

    print_out("Control Registers Status:\n");
    print_out("  CR0: "); print_hex64(cr0); print_out(" [Paging, Protected Mode, WP, MP]\n");
    print_out("  CR2: "); print_hex64(cr2); print_out(" [Last Page Fault Address]\n");
    print_out("  CR3: "); print_hex64(cr3); print_out(" [PML4 Physical Base Address]\n");
    print_out("  CR4: "); print_hex64(cr4); print_out(" [PAE, OSFXSR, OSXMMEXCPT]\n");
#endif
}

static void cmd_cpuid(int argc, char* argv[]) {
    (void)argc; (void)argv;
#if defined(__riscv)
    print_out("CPU Architecture: RISC-V 64-bit (RV64GC)\n");
    print_out("ISA Extensions: RV64IMAFDC (Base, Mul/Div, Atomics, Float, Double, Compressed)\n");
#else
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    __asm__ volatile ("cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0)
    );

    char vendor[13];
    *(uint32_t*)&vendor[0] = ebx;
    *(uint32_t*)&vendor[4] = edx;
    *(uint32_t*)&vendor[8] = ecx;
    vendor[12] = '\0';

    print_out("CPU Vendor Identity: ");
    print_out(vendor);
    print_out(" (Max basic CPUID leaf: ");
    print_dec64(eax);
    print_out(")\n");
#endif
}

static void cmd_meminfo(int argc, char* argv[]) {
    (void)argc; (void)argv;
    print_out("Physical Memory Mapping:\n");
    print_out("  Virtual Baseline: 0x0000000000000000 - 0x0000000040000000 (1 GiB Identity Mapped)\n");
    print_out("  Page Structure  : PML4 -> PDPT -> PD (512 x 2MiB Huge Pages)\n");
    print_out("  Paging Mode     : Long Mode 4-level paging active\n");
}

static void cmd_free(int argc, char* argv[]) {
    (void)argc; (void)argv;
    struct pmm_stats pstats = pmm_get_stats();
    struct heap_stats hstats = kmalloc_get_stats();

    print_out("Memory Utilization Overview (Ring 0 OpSec):\n");
    print_out("  Physical RAM Total : "); print_dec64(pstats.total_memory / 1024); print_out(" KiB (");
    print_dec64(pstats.total_memory / (1024 * 1024)); print_out(" MiB)\n");
    print_out("  Physical RAM Used  : "); print_dec64(pstats.used_memory / 1024); print_out(" KiB (");
    print_dec64(pstats.used_frames); print_out(" frames)\n");
    print_out("  Physical RAM Free  : "); print_dec64(pstats.free_memory / 1024); print_out(" KiB (");
    print_dec64(pstats.total_frames - pstats.used_frames); print_out(" frames)\n");
    print_out("--------------------------------------------------\n");
    print_out("  Kernel Heap Base   : "); print_hex64(hstats.heap_start); print_out("\n");
    print_out("  Kernel Heap Size   : "); print_dec64(hstats.heap_size / 1024); print_out(" KiB\n");
    print_out("  Kernel Heap Used   : "); print_dec64(hstats.used_bytes); print_out(" bytes\n");
    print_out("  Kernel Heap Free   : "); print_dec64(hstats.free_bytes); print_out(" bytes\n");
    print_out("  Active Allocations : "); print_dec64(hstats.allocations_count); print_out("\n");
}

static void cmd_alloc(int argc, char* argv[]) {
    if (argc < 2) {
        print_out("Usage: alloc <bytes>\n");
        return;
    }
    uint64_t bytes = strtoull(argv[1], NULL, 10);
    if (bytes == 0) {
        print_out("[-] Invalid allocation size.\n");
        return;
    }
    void* ptr = kmalloc(bytes);
    if (!ptr) {
        print_out("[-] Allocation failed: out of heap/physical memory.\n");
        return;
    }
    print_out("[+] Allocated ");
    print_dec64(bytes);
    print_out(" bytes at Ring 0 address: ");
    print_hex64((uint64_t)ptr);
    print_out("\n");
}

static void cmd_kfree(int argc, char* argv[]) {
    if (argc < 2) {
        print_out("Usage: kfree <hex_address>\n");
        return;
    }
    uint64_t addr = strtoull(argv[1], NULL, 16);
    if (addr == 0) {
        print_out("[-] Invalid memory address.\n");
        return;
    }
    kfree((void*)addr);
    print_out("[+] Released memory block at: ");
    print_hex64(addr);
    print_out("\n");
}

static void cmd_hexdump(int argc, char* argv[]) {
    if (argc < 2) {
        print_out("Usage: hexdump <hex_address> [length]\n");
        return;
    }
    uint64_t addr = strtoull(argv[1], NULL, 16);
    uint64_t len = 64; // Default 64 bytes
    if (argc >= 3) {
        len = strtoull(argv[2], NULL, 10);
        if (len > 512) len = 512; // Safety cap
    }

    const uint8_t* ptr = (const uint8_t*)addr;
    const char hex_chars[] = "0123456789ABCDEF";

    for (uint64_t i = 0; i < len; i += 16) {
        print_hex64(addr + i);
        print_out(": ");

        for (uint64_t j = 0; j < 16; j++) {
            if (i + j < len) {
                uint8_t b = ptr[i + j];
                print_char(hex_chars[(b >> 4) & 0xF]);
                print_char(hex_chars[b & 0xF]);
                print_char(' ');
            } else {
                print_out("   ");
            }
        }

        print_out(" |");
        for (uint64_t j = 0; j < 16 && (i + j) < len; j++) {
            char c = ptr[i + j];
            if (c >= 32 && c <= 126) {
                print_char(c);
            } else {
                print_char('.');
            }
        }
        print_out("|\n");
    }
}

static void print_file_entry(const char* name, uint32_t size) {
    print_out("  - ");
    print_out(name);
    for (size_t k = strlen(name); k < 24; k++) {
        print_char(' ');
    }
    print_dec64(size);
    print_out(" bytes\n");
}

static void cmd_disks(int argc, char* argv[]) {
    (void)argc; (void)argv;
    struct ata_device_info dev = ata_get_primary_master();
    struct ata_controller_info ctrl = ata_get_controller_info();

    print_out("Attached Block Storage Controllers & Disks:\n");
    if (ctrl.pci_detected) {
        print_out("  PCI Controller: IDE (Vendor: ");
        print_hex64(ctrl.vendor_id);
        print_out(", Device: ");
        print_hex64(ctrl.device_id);
        print_out(", Bus: ");
        print_dec64(ctrl.pci_bus);
        print_out(", Slot: ");
        print_dec64(ctrl.pci_slot);
        print_out(")\n");
        if (ctrl.bus_master_capable) {
            print_out("  Bus Master DMA: Supported (Base I/O Port: ");
            print_hex64(ctrl.bm_base_port);
            print_out(")\n");
        } else {
            print_out("  Bus Master DMA: Not supported (PIO mode active)\n");
        }
    } else {
        print_out("  PCI Controller: Legacy ISA/IDE compatibility mode\n");
    }

    if (!dev.present) {
        print_out("  Primary Master: [NO DEVICE PRESENT]\n");
    } else {
        print_out("  Primary Master: ");
        print_out(dev.model);
        print_out("\n  Total Sectors : ");
        print_dec64(dev.total_sectors);
        print_out(" (");
        print_dec64((dev.total_sectors * 512) / (1024 * 1024));
        print_out(" MiB)\n");
        print_out("  Sector Size   : 512 bytes (ATA 28-bit LBA)\n");
        print_out("  Driver Engine : Resilient PIO with Timeout Recovery & Soft-Reset\n");
    }

    uint32_t total_blks = 0, used_blks = 0, free_blks = 0;
    seldfs_get_bitmap_stats(&total_blks, &used_blks, &free_blks);
    print_out("SeldFS Allocation Bitmap (Sector 1):\n");
    print_out("  Managed Blocks: ");
    print_dec64(total_blks);
    print_out(" (");
    print_dec64((total_blks * 512) / 1024);
    print_out(" KiB capacity)\n");
    print_out("  Used Blocks   : ");
    print_dec64(used_blks);
    print_out("\n  Free Blocks   : ");
    print_dec64(free_blks);
    print_out("\n");
}

static void cmd_mkfs(int argc, char* argv[]) {
    (void)argc; (void)argv;
    print_out("[*] Initializing SeldFS signature on Primary Master...\n");
    if (seldfs_format() == 0) {
        print_out("[+] SeldFS initialized and mounted successfully.\n");
    } else {
        print_out("[-] Formatting failed: no drive detected or I/O error.\n");
    }
}

static void cmd_ls(int argc, char* argv[]) {
    (void)argc; (void)argv;
    print_out("SeldFS Root Directory Index:\n");
    int count = seldfs_list_files(print_file_entry);
    if (count < 0) {
        print_out("[-] File system not mounted. Run 'mkfs' to initialize.\n");
    } else if (count == 0) {
        print_out("  [empty directory]\n");
    } else {
        print_out("Total files: ");
        print_dec64(count);
        print_out("\n");
    }
}

static void cmd_cat(int argc, char* argv[]) {
    if (argc < 2) {
        print_out("Usage: cat <filename>\n");
        return;
    }
    char buf[1024];
    size_t out_len = 0;
    if (seldfs_read_file(argv[1], buf, sizeof(buf) - 1, &out_len) == 0) {
        buf[out_len] = '\0';
        print_out(buf);
        if (out_len > 0 && buf[out_len - 1] != '\n') {
            print_char('\n');
        }
    } else {
        print_out("cat: cannot open file: ");
        print_out(argv[1]);
        print_out(" (file not found or I/O error)\n");
    }
}

static void cmd_write(int argc, char* argv[]) {
    if (argc < 3) {
        print_out("Usage: write <filename> <text...>\n");
        return;
    }
    char content[512];
    content[0] = '\0';
    size_t cur = 0;
    for (int i = 2; i < argc; i++) {
        size_t l = strlen(argv[i]);
        if (cur + l + 2 < sizeof(content)) {
            memcpy(content + cur, argv[i], l);
            cur += l;
            if (i < argc - 1) {
                content[cur++] = ' ';
            }
        }
    }
    content[cur++] = '\n';
    content[cur] = '\0';

    if (seldfs_write_file(argv[1], content, cur) == 0) {
        print_out("[+] File saved: ");
        print_out(argv[1]);
        print_out(" (");
        print_dec64(cur);
        print_out(" bytes written to disk)\n");
    } else {
        print_out("[-] Failed to write file: disk full or filesystem not initialized.\n");
    }
}

static void cmd_rm(int argc, char* argv[]) {
    if (argc < 2) {
        print_out("Usage: rm <filename>\n");
        return;
    }
    if (seldfs_delete_file(argv[1]) == 0) {
        print_out("[+] File removed: ");
        print_out(argv[1]);
        print_out("\n");
    } else {
        print_out("[-] Failed to remove file: ");
        print_out(argv[1]);
        print_out("\n");
    }
}

static void cmd_rand(int argc, char* argv[]) {
    int count = 1;
    if (argc >= 2) {
        count = (int)strtoull(argv[1], NULL, 10);
        if (count < 1) count = 1;
        if (count > 16) count = 16;
    }
    for (int i = 0; i < count; i++) {
        uint64_t r = rng_get_u64();
        print_out("  [");
        print_dec64(i + 1);
        print_out("] ");
        print_hex64(r);
        print_out("\n");
    }
}

static void cmd_entropy(int argc, char* argv[]) {
    (void)argc; (void)argv;
    print_out("Hardware Entropy Architecture State:\n");
    if (rng_has_rdrand()) {
        print_out("  Primary CSPRNG: Intel/AMD Hardware RDRAND (NIST SP 800-90A/B/C compliant)\n");
    } else {
        print_out("  Primary CSPRNG: RDTSC High-Resolution Timestamp Jitter + SplitMix64\n");
    }
    print_out("  Ring 0 Direct Access: Enabled\n");
}

static void print_hash_hex(const uint8_t hash[32]) {
    const char hex_chars[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        print_char(hex_chars[(hash[i] >> 4) & 0xF]);
        print_char(hex_chars[hash[i] & 0xF]);
    }
}

static void cmd_sha256(int argc, char* argv[]) {
    if (argc < 2) {
        print_out("Usage: sha256 <filename>\n");
        return;
    }
    char buf[4096];
    size_t out_len = 0;
    if (seldfs_read_file(argv[1], buf, sizeof(buf), &out_len) != 0) {
        print_out("sha256: cannot open file: ");
        print_out(argv[1]);
        print_out("\n");
        return;
    }

    uint8_t digest[32];
    sha256_hash(buf, out_len, digest);

    print_hash_hex(digest);
    print_out("  ");
    print_out(argv[1]);
    print_out(" (");
    print_dec64(out_len);
    print_out(" bytes)");

    struct seldfs_inode inode;
    if (seldfs_get_file_info(argv[1], &inode) == 0) {
        if (memcmp(digest, inode.sha256, 32) == 0) {
            print_out(" [INTEGRITY: OK]");
        } else {
            print_out(" [INTEGRITY: MISMATCH]");
        }
    }
    print_char('\n');
}

static void cmd_verify(int argc, char* argv[]) {
    if (argc < 2) {
        print_out("Usage: verify <filename>\n");
        return;
    }

    struct seldfs_inode inode;
    if (seldfs_get_file_info(argv[1], &inode) != 0) {
        print_out("verify: file not found: ");
        print_out(argv[1]);
        print_out("\n");
        return;
    }

    print_out("[*] Verifying file integrity: ");
    print_out(argv[1]);
    print_out("\n  Stored Inode Hash: ");
    print_hash_hex(inode.sha256);
    print_out("\n");

    int res = seldfs_verify_file(argv[1]);
    if (res == 0) {
        print_out("[+] File integrity VERIFIED: SHA-256 matches stored inode checksum.\n");
    } else if (res == -2) {
        print_out("[-] File integrity VIOLATION: SHA-256 mismatch! Data corrupted or tampered.\n");
    } else {
        print_out("[-] File verification failed: I/O error.\n");
    }
}

static void cmd_uptime(int argc, char* argv[]) {
    (void)argc; (void)argv;
    uint64_t ticks = pit_get_ticks();
    uint64_t ms = pit_get_uptime_ms();
    uint64_t sec = ms / 1000;
    uint64_t min = sec / 60;
    sec %= 60;

    print_out("System Chronometry (PIT 8254 Channel 0):\n");
    print_out("  Uptime       : ");
    print_dec64(min);
    print_out(" min, ");
    print_dec64(sec);
    print_out(" sec (total ");
    print_dec64(ms);
    print_out(" ms)\n");
    print_out("  Timer Ticks  : ");
    print_dec64(ticks);
    print_out(" ticks @ 100 Hz\n");
}

static void cmd_sleep(int argc, char* argv[]) {
    if (argc < 2) {
        print_out("Usage: sleep <milliseconds>\n");
        return;
    }
    uint64_t ms = strtoull(argv[1], NULL, 10);
    if (ms == 0) return;

    print_out("[*] Sleeping for ");
    print_dec64(ms);
    print_out(" ms...\n");

    pit_sleep_ms(ms);
    print_out("[+] Resumed execution.\n");
}

static void test_worker_thread(void) {
    serial_puts("\n[Worker-1] Supervisor background thread started.\n");
    for (int i = 1; i <= 3; i++) {
        sched_sleep(500); // Sleep 500 ms
        serial_puts("[Worker-1] Background iteration ");
        serial_print_dec(i);
        serial_puts("/3 complete.\n");
    }
    serial_puts("[Worker-1] Supervisor background thread exiting gracefully.\n");
}

static void cmd_spawn(int argc, char* argv[]) {
    (void)argc; (void)argv;
    int pid = sched_create_task("sec_worker", test_worker_thread);
    if (pid < 0) {
        print_out("[-] Failed to spawn task: scheduler table full or out of memory.\n");
    } else {
        print_out("[+] Spawned background supervisor thread with PID: ");
        print_dec64(pid);
        print_out(" (stack 8 KiB)\n");
    }
}

static void cmd_ps(int argc, char* argv[]) {
    (void)argc; (void)argv;
    struct task list[MAX_TASKS];
    int count = sched_get_tasks(list);

    print_out("PID  NAME                    STATE       RUNTIME (TICKS)\n");
    print_out("---  --------------------    ---------   ---------------\n");

    for (int i = 0; i < count; i++) {
        print_dec64(list[i].id);
        print_out("    ");
        print_out(list[i].name);
        for (size_t k = strlen(list[i].name); k < 24; k++) {
            print_char(' ');
        }

        switch (list[i].state) {
            case TASK_RUNNING:    print_out("RUNNING     "); break;
            case TASK_READY:      print_out("READY       "); break;
            case TASK_SLEEPING:   print_out("SLEEPING    "); break;
            case TASK_TERMINATED: print_out("ZOMBIE      "); break;
            default:              print_out("UNKNOWN     "); break;
        }

        print_dec64(list[i].runtime_ticks);
        print_out("\n");
    }
}

static void cmd_echo(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        print_out(argv[i]);
        if (i < argc - 1) {
            print_char(' ');
        }
    }
    print_char('\n');
}

static void cmd_reboot(int argc, char* argv[]) {
    (void)argc; (void)argv;
#if defined(__riscv)
    fast_sys_reboot();
#else
    print_out("[!] Initiating ACPI/PS2 system reset...\n");
    uint8_t temp;
    do {
        temp = inb(0x64);
        if (temp & 1) inb(0x60);
    } while (temp & 2);
    outb(0x64, 0xFE); // Pulse reset line
    while (1) {
        __asm__ volatile ("hlt");
    }
#endif
}

static void cmd_ifconfig(int argc, char* argv[]) {
    (void)argc; (void)argv;
    struct net_config cfg = net_get_config();
    struct net_stats st = net_get_stats();
    char ip_buf[16], mask_buf[16], gw_buf[16], mac_buf[18];

    net_format_ip(cfg.ip, ip_buf, sizeof(ip_buf));
    net_format_ip(cfg.netmask, mask_buf, sizeof(mask_buf));
    net_format_ip(cfg.gateway, gw_buf, sizeof(gw_buf));
    net_format_mac(cfg.mac, mac_buf, sizeof(mac_buf));

    print_out("eth0: flags=UP,BROADCAST,MULTICAST mtu 1500\n");
    print_out("      ether "); print_out(mac_buf); print_out(" (Intel e1000 PCI)\n");
    print_out("      inet "); print_out(ip_buf);
    print_out("  netmask "); print_out(mask_buf);
    print_out("  gateway "); print_out(gw_buf); print_out("\n");
    print_out("      RX packets "); print_dec64(st.rx_frames);
    print_out("  bytes "); print_dec64(st.rx_bytes);
    print_out("  dropped "); print_dec64(st.rx_dropped);
    print_out("  errors "); print_dec64(st.rx_checksum_errors); print_out("\n");
    print_out("      TX packets "); print_dec64(st.tx_frames);
    print_out("  bytes "); print_dec64(st.tx_bytes);
    print_out("  arp "); print_dec64(st.tx_arp);
    print_out("  icmp "); print_dec64(st.tx_icmp); print_out("\n");
}

static void cmd_arp(int argc, char* argv[]) {
    (void)argc; (void)argv;
    struct arp_entry table[ARP_TABLE_SIZE];
    int count = net_get_arp_table(table, ARP_TABLE_SIZE);

    print_out("Address          HWaddress           Iface    State\n");
    print_out("---------------  -----------------   -----    -------\n");

    if (count == 0) {
        print_out("(ARP cache is currently empty)\n");
        return;
    }

    for (int i = 0; i < count; i++) {
        char ip_str[16];
        char mac_str[18];
        net_format_ip(table[i].ip, ip_str, sizeof(ip_str));
        net_format_mac(table[i].mac, mac_str, sizeof(mac_str));

        print_out(ip_str);
        for (size_t s = strlen(ip_str); s < 17; s++) print_char(' ');

        print_out(mac_str);
        print_out("   eth0     RESOLVED\n");
    }
}

static void cmd_ping(int argc, char* argv[]) {
    if (argc < 2) {
        print_out("Usage: ping <target_ipv4_address>\n");
        return;
    }

    uint32_t target_ip = 0;
    if (net_parse_ip(argv[1], &target_ip) != 0) {
        print_out("[-] ping: invalid IPv4 address format: ");
        print_out(argv[1]);
        print_out("\n");
        return;
    }

    char ip_str[16];
    net_format_ip(target_ip, ip_str, sizeof(ip_str));

    print_out("PING ");
    print_out(ip_str);
    print_out(" 56(84) bytes of data.\n");

    int received = 0;
    int transmitted = 4;

    for (int seq = 1; seq <= transmitted; seq++) {
        uint32_t rtt = 0;
        int res = net_ping(target_ip, (uint16_t)seq, &rtt);
        if (res == 0) {
            received++;
            print_out("64 bytes from ");
            print_out(ip_str);
            print_out(": icmp_seq=");
            print_dec64(seq);
            print_out(" ttl=64 time=");
            print_dec64(rtt);
            print_out(" ms\n");
        } else if (res == -2) {
            print_out("[-] ping: send error or route unreachable\n");
            break;
        } else {
            print_out("Request timeout for icmp_seq ");
            print_dec64(seq);
            print_out("\n");
        }

        if (seq < transmitted) {
            pit_sleep_ms(500);
        }
    }

    print_out("--- ");
    print_out(ip_str);
    print_out(" ping statistics ---\n");
    print_dec64(transmitted);
    print_out(" packets transmitted, ");
    print_dec64(received);
    print_out(" received, ");
    int loss = ((transmitted - received) * 100) / transmitted;
    print_dec64(loss);
    print_out("% packet loss\n");
}

struct shell_command {
    const char* name;
    void (*handler)(int argc, char* argv[]);
};

extern const char userspace_blob_start[];
extern const char userspace_blob_end[];
extern const uint64_t userspace_blob_size;

static void cmd_run3(int argc, char* argv[]) {
    // If argument given, load binary from SeldFS
    if (argc >= 2) {
        const char* prog_name = argv[1];
        print_out("[*] Loading Ring 3 ELF binary from SeldFS: ");
        print_out(prog_name);
        print_out("\n");

        int exit_code = elf_load_and_run(prog_name, argc - 1, &argv[1]);
        if (exit_code < 0) {
            print_out("[-] Failed to load or run ELF binary: ");
            print_out(prog_name);
            print_out("\n");
        } else {
            print_out("[+] Ring 3 process finished execution with exit code: ");
            print_dec64(exit_code);
            print_out("\n");
        }
        return;
    }

    // No argument: check if /bin/init is present on SeldFS
    struct seldfs_inode init_inode;
    if (seldfs_find_file("/bin/init", &init_inode) == 0) {
        print_out("[*] Loading default Ring 3 init binary from SeldFS (/bin/init)...\n");
        char* default_argv[] = {"/bin/init", NULL};
        int exit_code = elf_load_and_run("/bin/init", 1, default_argv);
        print_out("[+] Ring 3 process finished execution with exit code: ");
        print_dec64(exit_code);
        print_out("\n");
        return;
    }

    // No argument: check if embedded payload is an ELF binary
    if (userspace_blob_size >= sizeof(Elf64_Ehdr) &&
        memcmp(userspace_blob_start, "\x7f" "ELF", 4) == 0) {
        print_out("[*] Loading embedded ELF-64 userspace payload...\n");

        uint64_t* user_pml4_virt = NULL;
        uint64_t  user_entry = 0;
        uint64_t  user_pml4_phys = 0;
        int load_err = elf_load_binary(userspace_blob_start, (size_t)userspace_blob_size,
                                       &user_pml4_virt, &user_entry, &user_pml4_phys);
        if (load_err == 0) {
            print_out("[+] Embedded ELF-64 payload loaded successfully.\n");
            print_out("[*] Executing IRETQ to Ring 3 (CS=0x23, SS=0x1B, CPL=3)...\n");

            int exit_code = jump_to_userspace((void (*)(void))user_entry,
                                              (void*)(0x00007FFFFFFFF000ULL - 16),
                                              user_pml4_phys);

            vmm_destroy_address_space(user_pml4_virt);

            print_out("[+] Ring 3 process finished execution with exit code: ");
            print_dec64(exit_code);
            print_out("\n");
            return;
        }
    }

    // Fallback: embedded flat binary payload at 0x400000
    print_out("[*] Loading embedded flat binary payload at 0x400000...\n");

    uint64_t* user_pml4_virt = vmm_create_address_space();
    if (!user_pml4_virt) {
        print_out("[-] Failed to create address space for Ring 3 process.\n");
        return;
    }
    uint64_t user_pml4_phys = virt_to_phys(user_pml4_virt);

    void* stack_frame = pmm_alloc_frame();
    if (!stack_frame) {
        vmm_destroy_address_space(user_pml4_virt);
        print_out("[-] Failed to allocate user stack frame.\n");
        return;
    }
    memset(phys_to_virt((uint64_t)stack_frame), 0, PAGE_SIZE);

    uint64_t user_stack_virt = 0x00007FFFFFFFE000ULL;
    vmm_map_page(user_pml4_virt, user_stack_virt, (uint64_t)stack_frame,
                 VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_USER | VMM_FLAG_NO_EXECUTE);

    size_t blob_len = (size_t)userspace_blob_size;
    if (blob_len == 0) {
        vmm_destroy_address_space(user_pml4_virt);
        print_out("[-] Embedded userspace binary blob is empty.\n");
        return;
    }

    size_t num_pages = (blob_len + PAGE_SIZE - 1) / PAGE_SIZE;
    for (size_t p = 0; p < num_pages; p++) {
        void* code_frame = pmm_alloc_frame();
        if (!code_frame) {
            vmm_destroy_address_space(user_pml4_virt);
            print_out("[-] Failed to allocate user code frame.\n");
            return;
        }
        memset(phys_to_virt((uint64_t)code_frame), 0, PAGE_SIZE);

        size_t offset = p * PAGE_SIZE;
        size_t chunk = blob_len - offset;
        if (chunk > PAGE_SIZE) chunk = PAGE_SIZE;

        memcpy(phys_to_virt((uint64_t)code_frame), userspace_blob_start + offset, chunk);

        uint64_t page_va = 0x400000ULL + offset;
        vmm_map_page(user_pml4_virt, page_va, (uint64_t)code_frame,
                     VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_USER);
    }

    print_out("[+] User Code mapped at virtual: 0x0000000000400000 (R/W/X, User)\n");
    print_out("[+] User Stack mapped at virtual: 0x00007FFFFFFFF000 (RW, NX, User)\n");
    print_out("[*] Executing IRETQ to Ring 3 (CS=0x23, SS=0x1B, CPL=3)...\n");

    int exit_code = jump_to_userspace((void (*)(void))0x400000ULL,
                                      (void*)(user_stack_virt + PAGE_SIZE - 16),
                                      user_pml4_phys);

    vmm_destroy_address_space(user_pml4_virt);

    print_out("[+] Ring 3 process finished execution with exit code: ");
    print_dec64(exit_code);
    print_out("\n");
}

static void cmd_selftest(int argc, char* argv[]) {
    (void)argc; (void)argv;
    selftest_run_all();
}

static void cmd_drv_off(int argc, char* argv[]) {
    const char* target = NULL;
    if (argc >= 3 && strcmp(argv[1], "drv") == 0 && strcmp(argv[2], "off") == 0) {
        target = argv[0];
    } else if (argc >= 3 && strcmp(argv[0], "drv") == 0 && strcmp(argv[1], "off") == 0) {
        target = argv[2];
    } else if (argc >= 3 && strcmp(argv[0], "drv") == 0 && strcmp(argv[2], "off") == 0) {
        target = argv[1];
    } else {
        print_out("Usage: <gpu|cpu|ram> drv off\n");
        print_out("       drv off <gpu|cpu|ram>\n");
        print_out("       Brutally shuts down hardware driver (causes kernel panic).\n");
        return;
    }

    if (strcmp(target, "gpu") == 0) {
        print_out("[!] Brutally disabling GPU display driver...\n");
        vga_driver_disable();
        print_out("[!] GPU driver terminated.\n");
    } else if (strcmp(target, "cpu") == 0) {
        print_out("[!] Brutally disabling CPU core scheduler driver...\n");
        cpu_driver_disable();
        print_out("[!] CPU driver terminated.\n");
    } else if (strcmp(target, "ram") == 0) {
        print_out("[!] Brutally disabling RAM memory manager and heap allocator...\n");
        ram_driver_disable();
        print_out("[!] RAM driver terminated.\n");
    } else {
        print_out("seldshell: drv: unknown subsystem: ");
        print_out(target);
        print_out("\n");
    }
}

static const struct shell_command commands[] = {
    {"help",     cmd_help},
    {"clear",    cmd_clear},
    {"uname",    cmd_uname},
    {"license",  cmd_license},
    {"creg",     cmd_creg},
    {"cpuid",    cmd_cpuid},
    {"meminfo",  cmd_meminfo},
    {"free",     cmd_free},
    {"alloc",    cmd_alloc},
    {"kfree",    cmd_kfree},
    {"hexdump",  cmd_hexdump},
    {"disks",    cmd_disks},
    {"mkfs",     cmd_mkfs},
    {"ls",       cmd_ls},
    {"cat",      cmd_cat},
    {"write",    cmd_write},
    {"rm",       cmd_rm},
    {"rand",     cmd_rand},
    {"entropy",  cmd_entropy},
    {"sha256",   cmd_sha256},
    {"verify",   cmd_verify},
    {"uptime",   cmd_uptime},
    {"sleep",    cmd_sleep},
    {"ps",       cmd_ps},
    {"spawn",    cmd_spawn},
    {"selftest", cmd_selftest},
    {"run3",     cmd_run3},
    {"snl",      cmd_run3},
    {"echo",     cmd_echo},
    {"ifconfig", cmd_ifconfig},
    {"ping",     cmd_ping},
    {"arp",      cmd_arp},
    {"gpu",      cmd_drv_off},
    {"cpu",      cmd_drv_off},
    {"ram",      cmd_drv_off},
    {"drv",      cmd_drv_off},
    {"reboot",   cmd_reboot},
    {NULL,       NULL}
};

static void execute_command(char* line) {
    // Trim leading whitespace
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    if (*line == '\0') {
        return;
    }

    char* argv[MAX_ARGS];
    int argc = 0;

    char* p = line;
    while (*p && argc < MAX_ARGS) {
        while (*p == ' ' || *p == '\t') {
            *p++ = '\0';
        }
        if (*p == '\0') {
            break;
        }
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') {
            p++;
        }
    }

    if (argc == 0) {
        return;
    }

    for (int i = 0; commands[i].name != NULL; i++) {
        if (strcmp(argv[0], commands[i].name) == 0) {
            commands[i].handler(argc, argv);
            return;
        }
    }

    print_out("seldshell: command not found: ");
    print_out(argv[0]);
    print_out(". Type 'help' for internal system commands.\n");
}

void seldshell_init(void) {
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    print_out("SeldShell v1.0.0-sec (Ring 0 GPLv3 Environment)\n");
    print_out("Type 'help' for list of commands, 'license' for GNU GPL details.\n\n");
}

void seldshell_run(void) {
    char cmd_buf[MAX_CMD_LEN];
    size_t cmd_idx = 0;

    while (1) {
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        print_out("seldshell# ");
        vga_set_color(VGA_WHITE, VGA_BLACK);

        cmd_idx = 0;
        memset(cmd_buf, 0, sizeof(cmd_buf));

        while (1) {
            while (!kbd_has_char() && !serial_has_char()) {
                sched_yield();
            }
            char c = 0;
            if (serial_has_char()) {
                c = serial_getchar();
            } else {
                c = kbd_getchar();
            }

            if (c == '\n' || c == '\r') {
                print_char('\n');
                cmd_buf[cmd_idx] = '\0';
                break;
            } else if (c == '\b' || c == 127) {
                if (cmd_idx > 0) {
                    cmd_idx--;
                    cmd_buf[cmd_idx] = '\0';
                    print_char('\b');
                    print_char(' ');
                    print_char('\b');
                }
            } else if (c >= 32 && c <= 126) {
                if (cmd_idx < MAX_CMD_LEN - 1) {
                    cmd_buf[cmd_idx++] = c;
                    print_char(c);
                }
            }
        }

        execute_command(cmd_buf);
        if (cmd_idx > 0) {
            char* hist_entry = (char*)kmalloc(cmd_idx + 1);
            if (hist_entry) {
                memcpy(hist_entry, cmd_buf, cmd_idx + 1);
            }
        }
    }
}
