/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * RISC-V 64-bit Userspace Ring Transition & Process Exit Primitives
 * Handles entering U-mode and returning control back to the kernel.
 * GPLv3 Licensed.
 */

.section .data
.align 8
.global user_depth
user_depth:
    .quad 0

.global user_stack_offset
user_stack_offset:
    .quad 0

# Saved supervisor register frames for nested userland processes (16 levels)
# 14 registers * 8 bytes = 112 bytes per level
kernel_saved_contexts:
    .space 16 * 112

.section .text
.global jump_to_userspace_asm
.type jump_to_userspace_asm, @function

# int jump_to_userspace_asm(void (*user_func)(void), void* user_stack_top, uint64_t user_cr3, uint64_t argc, void* argv);
# a0 = user_func (entry point)
# a1 = user_stack_top
# a2 = user_cr3 (physical page table root)
# a3 = argc
# a4 = argv
jump_to_userspace_asm:
    # 1. Save kernel callee-saved registers
    la t0, user_depth
    ld t1, 0(t0)
    li t2, 16
    bgeu t1, t2, .Ldepth_cap

    li t2, 112
    mul t3, t1, t2
    la t4, kernel_saved_contexts
    add t4, t4, t3

    sd sp,  0(t4)
    sd ra,  8(t4)
    sd s0,  16(t4)
    sd s1,  24(t4)
    sd s2,  32(t4)
    sd s3,  40(t4)
    sd s4,  48(t4)
    sd s5,  56(t4)
    sd s6,  64(t4)
    sd s7,  72(t4)
    sd s8,  80(t4)
    sd s9,  88(t4)
    sd s10, 96(t4)
    sd s11, 104(t4)

    addi t1, t1, 1
    sd t1, 0(t0)
    slli t2, t1, 13
    la t3, user_stack_offset
    sd t2, 0(t3)

.Ldepth_cap:
    # 2. Switch page table (satp) if user_cr3 != 0
    beqz a2, .Lskip_cr3
    srli a2, a2, 12
    li t2, 8
    slli t2, t2, 60
    or a2, a2, t2
    csrw satp, a2
    sfence.vma
.Lskip_cr3:

    # 3. Setup sscratch for user trap handling
    la t0, kernel_syscall_stack_top
    ld t1, 0(t0)
    la t2, user_stack_offset
    ld t3, 0(t2)
    sub t1, t1, t3
    li t2, 8192
    add t1, t1, t2
    csrw sscratch, t1

    # 4. Program CSRs for U-mode transition
    csrw sepc, a0

    # sstatus: SPP = 0 (U-mode), SPIE = 1 (enable IRQ on sret), SUM = 1 (allow access to user memory)
    csrr t0, sstatus
    li t1, ~(1 << 8)
    and t0, t0, t1
    li t1, (1 << 5) | (1 << 18)
    or t0, t0, t1
    csrw sstatus, t0

    # 5. Set user stack pointer
    mv sp, a1

    # 6. Pass argc, argv in a0, a1
    mv a0, a3
    mv a1, a4

    # 7. Zero other registers for clean unprivileged environment
    li ra, 0
    li gp, 0
    li tp, 0
    li t0, 0
    li t1, 0
    li t2, 0
    li t3, 0
    li t4, 0
    li t5, 0
    li t6, 0
    li s0, 0
    li s1, 0
    li s2, 0
    li s3, 0
    li s4, 0
    li s5, 0
    li s6, 0
    li s7, 0
    li s8, 0
    li s9, 0
    li s10, 0
    li s11, 0
    li a2, 0
    li a3, 0
    li a4, 0
    li a5, 0
    li a6, 0
    li a7, 0

    sret

.global user_exit_to_kernel
.type user_exit_to_kernel, @function

# void user_exit_to_kernel(uint64_t exit_code);
# a0 = exit_code
user_exit_to_kernel:
    # 1. Restore kernel page table
    la t0, kernel_pml4_phys
    ld t1, 0(t0)
    beqz t1, .Lskip_kcr3

    # Ensure supervisor higher-half mappings in kernel PML4 before switching satp
    li t2, 0xFFFFFFC000000000
    add t2, t2, t1
    # 256: MMIO (0x000000C7)
    li t3, 256 * 8
    add t3, t3, t2
    li t4, 0x000000C7
    sd t4, 0(t3)
    # 258: RAM (0x200000C7)
    li t3, 258 * 8
    add t3, t3, t2
    li t4, 0x200000C7
    sd t4, 0(t3)
    # 510: Higher-half kernel base (0x200000CF)
    li t3, 510 * 8
    add t3, t3, t2
    li t4, 0x200000CF
    sd t4, 0(t3)

    srli t1, t1, 12
    li t2, 8
    slli t2, t2, 60
    or t1, t1, t2
    csrw satp, t1
    sfence.vma
.Lskip_kcr3:

    # 2. Reset sscratch to 0
    csrw sscratch, zero

    # 3. Decrement user_depth
    la t0, user_depth
    ld t1, 0(t0)
    beqz t1, .Ldepth_zero
    addi t1, t1, -1
    sd t1, 0(t0)

.Ldepth_zero:
    slli t2, t1, 13
    la t3, user_stack_offset
    sd t2, 0(t3)

    # 4. Restore kernel registers
    li t2, 112
    mul t3, t1, t2
    la t4, kernel_saved_contexts
    add t4, t4, t3

    ld sp,  0(t4)
    ld ra,  8(t4)
    ld s0,  16(t4)
    ld s1,  24(t4)
    ld s2,  32(t4)
    ld s3,  40(t4)
    ld s4,  48(t4)
    ld s5,  56(t4)
    ld s6,  64(t4)
    ld s7,  72(t4)
    ld s8,  80(t4)
    ld s9,  88(t4)
    ld s10, 96(t4)
    ld s11, 104(t4)

    # Return exit_code in a0
    ret

.section .rodata
.Lmsg_before_sret:
    .string "[+] jump_to_userspace_asm: about to sret\n"

