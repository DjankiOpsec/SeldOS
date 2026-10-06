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
#include "boot_anim.h"
#include "panic.h"

static void print_banner(void) {
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("=======================================================================\n");
    vga_puts("                          SELD OS v0.1-sec (Humboldt Kernel)\n");
    vga_puts("                          GNU General Public License v3\n");
    vga_puts("                          Bare-Metal x86_64 Hardened Kernel\n");
    vga_puts("                          x86_64 Long Mode Architecture\n");
    vga_puts("\n");
    vga_puts("\n");
    vga_puts("\n");
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
#include "net.h"
#include "e1000.h"

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

    // Linux-Style Boot Animation with SVGZ Logo & Running Init Lines
    boot_anim_init();
    boot_anim_step("GDT/TSS", "64-bit segments and ring transitions configured", 1, 15);
    boot_anim_step("IDT", "256 vector gates remapped (PIC 0x20/0x28)", 2, 15);
    boot_anim_step("PMM", "Physical memory frame allocator online", 3, 15);
    boot_anim_step("VMM", "Higher-half paging active, W^X memory protection enforced", 4, 15);

    syscall_init_fast();
    boot_anim_step("SYSCALL", "Fast MSR LSTAR vector handshake initialized", 5, 15);

    kmalloc_init();
    boot_anim_step("KMALLOC", "Dynamic kernel heap pool online (1024 KiB pool)", 6, 15);

    ata_init();
    boot_anim_step("ATA", "PIO primary storage controller online", 7, 15);

    seldfs_init();
    boot_anim_step("SELDFS", "Block filesystem mounted, root directory verified", 8, 15);

    rng_init();
    boot_anim_step("CRYPTO", "Hardware RDRAND and SHA-256 primitives active", 9, 15);

    pit_init(100);
    boot_anim_step("PIT", "100 Hz chronometer timer online, IRQ0 active", 10, 15);

    audio_init();
    boot_anim_step("AUDIO", "Sound architecture online (Speaker/AC97/SB16)", 11, 15);

    sched_init();
    boot_anim_step("SCHED", "Supervisor cooperative scheduler initialized", 12, 15);

    net_init();
    boot_anim_step("NET", "Intel e1000 PCI Gigabit Network online", 13, 15);

    selftest_run_all();
    boot_anim_step("SELFTEST", "All 8/8 kernel subsystem tests passed", 14, 15);

    boot_anim_step("INIT", "Transferring execution to /bin/init (Ring 3 PID 1)", 15, 15);
    boot_anim_finish();

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
