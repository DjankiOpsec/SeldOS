/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * RISC-V 64-bit (rv64gc) Early Boot Entry Point
 * Sv39 Paging Setup & Higher-Half Transition.
 * GPLv3 Licensed.
 */

.section .boot.text, "ax"
.global _start
.type _start, @function

_start:
    # Only allow primary hart (hart 0) to execute kernel initialization
    bnez a0, .park_hart

    # Disable supervisor interrupts and clear pending
    csrw sie, zero
    csrci sstatus, 2

    # Save OpenSBI arguments (a0 = hartid, a1 = dtb pointer)
    mv s0, a0
    mv s1, a1

    # Set up initial boot stack in lower physical memory
    lla sp, boot_stack_top

    # Zero early boot page table (4096 bytes)
    lla t0, boot_page_table
    mv t1, t0
    li t2, 512
.Lzero_pt:
    sd zero, 0(t1)
    addi t1, t1, 8
    addi t2, t2, -1
    bnez t2, .Lzero_pt

    # PTE flag bits:
    # V(1) | R(2) | W(4) | X(8) | A(64) | D(128) = 0xCF
    # PTE for 0x00000000 (PA 0x0): (0x0 >> 12 << 10) | 0xCF = 0xCF
    # PTE for 0x80000000 (PA 0x80000000): (0x80000000 >> 12 << 10) | 0xCF = 0x200000CF

    li t3, 0xCF
    li t4, 0x200000CF

    # 1. Identity map MMIO (Gigapage 0: VA 0x0 -> PA 0x0)
    sd t3, 0(t0)

    # 2. Identity map Kernel RAM (Gigapage 2: VA 0x80000000 -> PA 0x80000000)
    sd t4, 16(t0)

    # 3. HHDM MMIO (Gigapage 256: VA 0xFFFFFFC000000000 -> PA 0x0)
    li t1, 256 * 8
    add t2, t0, t1
    sd t3, 0(t2)

    # 4. HHDM RAM (Gigapage 258: VA 0xFFFFFFC080000000 -> PA 0x80000000)
    li t1, 258 * 8
    add t2, t0, t1
    sd t4, 0(t2)

    # 5. Higher-Half Kernel Base (Gigapage 510: VA 0xFFFFFFFF80000000 -> PA 0x80000000)
    li t1, 510 * 8
    add t2, t0, t1
    sd t4, 0(t2)

    # Enable Sv39 Paging: satp = (8ULL << 60) | (boot_page_table >> 12)
    srli t1, t0, 12
    li t2, 8
    slli t2, t2, 60
    or t1, t1, t2
    csrw satp, t1
    sfence.vma

    # Jump to higher-half entry point
    lla t0, higher_half_target
    ld t1, 0(t0)
    jr t1

.park_hart:
    wfi
    j .park_hart

.section .boot.data, "aw"
.align 8
higher_half_target:
    .quad boot_higher_half

.section .boot.bss, "aw", @nobits
.align 12
boot_page_table:
    .space 4096
boot_stack:
    .space 4096
boot_stack_top:

.section .text
.global boot_higher_half
.type boot_higher_half, @function

boot_higher_half:
    # Zero .bss section in higher half
    la t0, bss_start
    la t1, bss_end
.Lzero_bss:
    bgeu t0, t1, .Ldone_bss
    sd zero, 0(t0)
    addi t0, t0, 8
    j .Lzero_bss
.Ldone_bss:

    # Switch to higher-half 64 KiB kernel stack
    la sp, kernel_stack_top

    # Pass boot arguments to kernel_main(uint64_t mb_info_addr, uint64_t mb_magic)
    # a0 = dtb address, a1 = hartid
    mv a0, s1
    mv a1, s0
    call kernel_main

    # If kernel_main terminates, park processor
.Lhalt_loop:
    wfi
    j .Lhalt_loop

.section .bss
.align 12
kernel_stack:
    .space 65536
kernel_stack_top:
