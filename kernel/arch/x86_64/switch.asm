; SeldOS - Humboldt Kernel Project
; Copyright (C) 2026 Free Software Foundation, Inc.
;
; Ring 0 Task Switch Assembly Stub
; Saves callee-saved registers of old task, updates RSP, and restores next task.
; GPLv3 Licensed.

bits 64
section .text

global task_switch_asm

; void task_switch_asm(uint64_t* old_rsp, uint64_t new_rsp);
; rdi = pointer to old_rsp
; rsi = new_rsp

task_switch_asm:
    ; Push callee-saved registers onto current stack
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; Save current RSP to *old_rsp
    mov [rdi], rsp

    ; Switch stack to new_rsp
    mov rsp, rsi

    ; Pop callee-saved registers from new stack
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    ; Return to new task's RIP (which was pushed on stack or entry point)
    ret
