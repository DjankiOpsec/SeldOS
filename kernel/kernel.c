/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "vga.h"
#include "serial.h"
#include "idt.h"
#include "kbd.h"
#include "shell.h"

static void print_banner(void) {
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("=======================================================================\n");
    vga_puts("     _.-'''''-._\n");
    vga_puts("   .'  _     _  '.        SELD OS v0.1-sec (Humboldt Kernel)\n");
    vga_puts("  /   (o)   (o)   \\       GNU General Public License v3\n");
    vga_puts(" |                 |      Dedicated Free Software Foundation OpSec\n");
    vga_puts(" |     <--V-->     |      x86_64 Long Mode Architecture\n");
    vga_puts("  \\               /\n");
    vga_puts("   '.  '-----'  .'\n");
    vga_puts("     '-._____.-'\n");
    vga_puts("=======================================================================\n");

    serial_puts("\n=======================================================================\n");
    serial_puts(" SELD OS v0.1-sec (Humboldt Kernel) - GNU GPLv3 Free Software Foundation\n");
    serial_puts(" Hardened x86_64 Long Mode Supervisor Kernel Initialized\n");
    serial_puts("=======================================================================\n");
}

#include "pmm.h"
#include "kmalloc.h"
#include "ata.h"
#include "seldfs.h"
#include "rand.h"
#include "pit.h"
#include "sched.h"
#include "gdt.h"
#include "vmm.h"
#include "fast_syscall.h"
#include "selftest.h"
#include "mouse.h"
#include "audio.h"

void kernel_main(uint64_t mb_info_addr, uint64_t mb_magic) {
    serial_init();
    vga_init();
    vga_init_fb(mb_magic, mb_info_addr);
    kbd_init();

    print_banner();

    vga_puts("[+] Initializing 64-bit GDT, User Segments, and TSS...\n");
    serial_puts("[+] Initializing 64-bit GDT, User Segments, and TSS...\n");
    gdt_init();

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts("[+] Initializing Interrupt Descriptor Table (IDT)...\n");
    serial_puts("[+] Initializing Interrupt Descriptor Table (IDT)...\n");
    idt_init();
    mouse_init();

    vga_puts("[+] IDT active, 8259 PIC remapped, IRQ handlers registered.\n");
    serial_puts("[+] IDT active, 8259 PIC remapped, IRQ handlers registered.\n");

    vga_puts("[+] Initializing Physical Memory Manager (PMM)...\n");
    pmm_init(mb_magic, mb_info_addr);
    struct pmm_stats pstats = pmm_get_stats();
    vga_puts("[+] PMM online: ");
    vga_print_dec(pstats.total_memory / (1024 * 1024));
    vga_puts(" MiB physical RAM detected.\n");

    vga_puts("[+] Initializing Higher-Half VMM & Memory Protection (W^X)...\n");
    vmm_init();
    vga_enable_fb_console();

    vga_puts("[+] Initializing Fast SYSCALL / SYSRET Subsystem (MSR LSTAR)...\n");
    syscall_init_fast();

    vga_puts("[+] Initializing Kernel Dynamic Allocator (kmalloc)...\n");
    kmalloc_init();
    struct heap_stats hstats = kmalloc_get_stats();
    vga_puts("[+] Kernel Heap online: ");
    vga_print_dec(hstats.heap_size / 1024);
    vga_puts(" KiB baseline pool.\n");

    vga_puts("[+] Initializing ATA PIO Storage Controller...\n");
    ata_init();

    vga_puts("[+] Initializing SeldFS Hardened Block Filesystem...\n");
    seldfs_init();

    vga_puts("[+] Initializing Kernel Cryptographic Subsystem (RDRAND/SHA-256)...\n");
    rng_init();

    vga_puts("[+] Initializing Programmable Interval Timer (100 Hz)...\n");
    pit_init(100);

    vga_puts("[+] Initializing Unified Audio Architecture (Speaker / AC'97 / SB16)...\n");
    serial_puts("[+] Initializing Unified Audio Architecture (Speaker / AC'97 / SB16)...\n");
    audio_init();

    vga_puts("[+] Initializing Supervisor Cooperative Scheduler...\n");
    sched_init();

    vga_puts("[+] Executing Kernel Boot-Time Subsystem Validation...\n");
    serial_puts("[+] Executing Kernel Boot-Time Subsystem Validation...\n");
    selftest_run_all();

    struct seldfs_inode init_node;
    if (seldfs_get_file_info("/bin/init", &init_node) == 0 ||
        seldfs_get_file_info("init", &init_node) == 0) {
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_puts("\n[+] SNL Sovereign Userland detected on SeldFS storage.\n");
        vga_puts("[+] Spawning /bin/init (Ring 3 Unprivileged PID 1)...\n\n");
        serial_puts("\n[+] SNL Sovereign Userland detected on SeldFS storage.\n");
        serial_puts("[+] Spawning /bin/init (Ring 3 Unprivileged PID 1)...\n\n");

        char* init_argv[2] = {"/bin/init", NULL};
        int exit_code = elf_load_and_run("/bin/init", 1, init_argv);

        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_puts("[!] /bin/init terminated with code: ");
        vga_print_dec(exit_code);
        vga_puts(". Falling back to Ring 0 SeldShell supervisor.\n\n");
        serial_puts("[!] /bin/init terminated. Falling back to Ring 0 SeldShell.\n\n");
    }

    vga_puts("[+] Initializing Ring 0 SeldShell environment...\n\n");
    serial_puts("[+] Initializing Ring 0 SeldShell environment...\n\n");

    seldshell_init();
    seldshell_run();

    while (1) {
        __asm__ volatile ("hlt");
    }
}
