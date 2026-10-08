/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * RISC-V 64-bit Low-Level Supervisor Trap Vector Entry & Return
 * GPLv3 Licensed.
 */

.section .text
.global trap_entry_asm
.type trap_entry_asm, @function
.align 4

trap_entry_asm:
    # Atomically swap sp with sscratch to determine privilege mode
    csrrw sp, sscratch, sp
    bnez sp, .Lfrom_umode

    # Trap occurred in S-mode: restore sp from sscratch
    csrrw sp, sscratch, sp
    addi sp, sp, -288
    # Save original S-mode sp
    addi t0, sp, 288
    sd t0, 8(sp)
    j .Lsave_gprs

.Lfrom_umode:
    # Trap occurred in U-mode: sp is now kernel stack top, sscratch contains user sp
    addi sp, sp, -288
    csrr t0, sscratch
    sd t0, 8(sp)        # Save user sp
    csrw sscratch, zero  # Indicate kernel execution

.Lsave_gprs:
    sd ra,  0(sp)
    sd gp,  16(sp)
    sd tp,  24(sp)
    sd t0,  32(sp)
    sd t1,  40(sp)
    sd t2,  48(sp)
    sd s0,  56(sp)
    sd s1,  64(sp)
    sd a0,  72(sp)
    sd a1,  80(sp)
    sd a2,  88(sp)
    sd a3,  96(sp)
    sd a4,  104(sp)
    sd a5,  112(sp)
    sd a6,  120(sp)
    sd a7,  128(sp)
    sd s2,  136(sp)
    sd s3,  144(sp)
    sd s4,  152(sp)
    sd s5,  160(sp)
    sd s6,  168(sp)
    sd s7,  176(sp)
    sd s8,  184(sp)
    sd s9,  192(sp)
    sd s10, 200(sp)
    sd s11, 208(sp)
    sd t3,  216(sp)
    sd t4,  224(sp)
    sd t5,  232(sp)
    sd t6,  240(sp)

    csrr t0, sstatus
    sd t0,  248(sp)
    csrr t1, sepc
    sd t1,  256(sp)
    csrr t2, scause
    sd t2,  264(sp)
    csrr t3, stval
    sd t3,  272(sp)

    # Invoke C-level trap handler with trap_frame pointer
    mv a0, sp
    call riscv_trap_handler

    # Restore CSRs
    ld t0, 248(sp)
    csrw sstatus, t0
    ld t1, 256(sp)
    csrw sepc, t1

    # Check if returning to U-mode or S-mode (SPP is bit 8 of sstatus)
    andi t0, t0, (1 << 8)
    bnez t0, .Lreturn_smode

    # Returning to U-mode:
    # 1. Load user sp into sscratch
    ld t0, 8(sp)
    csrw sscratch, t0

    # 2. Restore all 31 registers
    ld ra,  0(sp)
    ld gp,  16(sp)
    ld tp,  24(sp)
    ld t0,  32(sp)
    ld t1,  40(sp)
    ld t2,  48(sp)
    ld s0,  56(sp)
    ld s1,  64(sp)
    ld a0,  72(sp)
    ld a1,  80(sp)
    ld a2,  88(sp)
    ld a3,  96(sp)
    ld a4,  104(sp)
    ld a5,  112(sp)
    ld a6,  120(sp)
    ld a7,  128(sp)
    ld s2,  136(sp)
    ld s3,  144(sp)
    ld s4,  152(sp)
    ld s5,  160(sp)
    ld s6,  168(sp)
    ld s7,  176(sp)
    ld s8,  184(sp)
    ld s9,  192(sp)
    ld s10, 200(sp)
    ld s11, 208(sp)
    ld t3,  216(sp)
    ld t4,  224(sp)
    ld t5,  232(sp)
    ld t6,  240(sp)

    # 3. sp becomes kernel stack top, swap with sscratch (user sp)
    addi sp, sp, 288
    csrrw sp, sscratch, sp
    sret

.Lreturn_smode:
    # Returning to S-mode:
    ld ra,  0(sp)
    ld gp,  16(sp)
    ld tp,  24(sp)
    ld t0,  32(sp)
    ld t1,  40(sp)
    ld t2,  48(sp)
    ld s0,  56(sp)
    ld s1,  64(sp)
    ld a0,  72(sp)
    ld a1,  80(sp)
    ld a2,  88(sp)
    ld a3,  96(sp)
    ld a4,  104(sp)
    ld a5,  112(sp)
    ld a6,  120(sp)
    ld a7,  128(sp)
    ld s2,  136(sp)
    ld s3,  144(sp)
    ld s4,  152(sp)
    ld s5,  160(sp)
    ld s6,  168(sp)
    ld s7,  176(sp)
    ld s8,  184(sp)
    ld s9,  192(sp)
    ld s10, 200(sp)
    ld s11, 208(sp)
    ld t3,  216(sp)
    ld t4,  224(sp)
    ld t5,  232(sp)
    ld t6,  240(sp)
    addi sp, sp, 288
    sret
