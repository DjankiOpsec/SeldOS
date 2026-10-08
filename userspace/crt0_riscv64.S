/*
 * Seld not's Linux - Userspace CRT0 Startup (RISC-V 64-bit)
 * Entry point at 0x400000
 * Standard System V / RISC-V ELF ABI startup:
 * Prepares stack, passes argc in a0, argv in a1, calls main(argc, argv),
 * and cleanly calls exit(status) or SYS_EXIT via ecall.
 * GPLv3 Licensed.
 */

.section .text._start, "ax", @progbits
.globl _start
.type _start, @function

_start:
    /* Clear frame pointer to terminate backtrace */
    mv fp, zero

    /* The kernel passes argc in a0, argv in a1.
       If argc is 0 and stack has arguments laid out: */
    bnez a0, .Lhave_args
    ld a0, 0(sp)
    addi a1, sp, 8

.Lhave_args:
    /* Align stack to 16 bytes per RISC-V ABI */
    andi sp, sp, -16

    /* Call main(argc, argv) */
    call main

    /* Call libc exit(status) with return value */
    call exit

    /* Fallback in case exit returns: issue SYS_EXIT (syscall 1) */
    li a7, 1
    ecall

.Lhalt_loop:
    j .Lhalt_loop

.size _start, . - _start
