/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * RISC-V 64-bit Cooperative Task Switching
 * GPLv3 Licensed.
 */

.section .text
.global task_switch_asm
.type task_switch_asm, @function

# void task_switch_asm(uint64_t* old_rsp, uint64_t new_rsp);
# a0 = old_rsp (pointer to old task's saved SP)
# a1 = new_rsp (new task's saved SP)
task_switch_asm:
    addi sp, sp, -104
    sd ra,  0(sp)
    sd s0,  8(sp)
    sd s1,  16(sp)
    sd s2,  24(sp)
    sd s3,  32(sp)
    sd s4,  40(sp)
    sd s5,  48(sp)
    sd s6,  56(sp)
    sd s7,  64(sp)
    sd s8,  72(sp)
    sd s9,  80(sp)
    sd s10, 88(sp)
    sd s11, 96(sp)

    sd sp, 0(a0)
    mv sp, a1

    ld ra,  0(sp)
    ld s0,  8(sp)
    ld s1,  16(sp)
    ld s2,  24(sp)
    ld s3,  32(sp)
    ld s4,  40(sp)
    ld s5,  48(sp)
    ld s6,  56(sp)
    ld s7,  64(sp)
    ld s8,  72(sp)
    ld s9,  80(sp)
    ld s10, 88(sp)
    ld s11, 96(sp)
    addi sp, sp, 104
    ret

.global task_trampoline_asm
.type task_trampoline_asm, @function
task_trampoline_asm:
    csrsi sstatus, 2
    jalr s1
    call sched_exit
.Ltask_halt:
    wfi
    j .Ltask_halt
