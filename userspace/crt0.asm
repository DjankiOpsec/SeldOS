; Seld not's Linux - Userspace CRT0 Startup
; Ring 3 entry point at 0x400000
; Standard System V AMD64 ABI startup calling main(argc, argv) and exiting cleanly.

bits 64
section .text._start
global _start
extern main
extern exit

_start:
    ; Terminate stack frame pointer chain per System V ABI
    xor rbp, rbp

    ; The kernel passes argc in rdi, argv in rsi.
    ; If the kernel laid out [rsp]=argc, [rsp+8]=argv[0], etc:
    ; We check if rdi is 0 and rsp contains a valid argc:
    cmp rdi, 0
    jne .have_args
    ; Fallback from stack:
    mov rdi, [rsp]       ; argc
    lea rsi, [rsp + 8]   ; argv
.have_args:

    ; Align stack to 16 bytes before calling main
    ; System V ABI requires (rsp + 8) is 16-byte aligned before call instruction
    ; so that inside main (after call pushes return rip), rsp is 16-byte aligned.
    mov rax, rsp
    and rax, 15
    sub rsp, rax

    ; Call main(argc, argv)
    call main

    ; Exit cleanly via libc exit(status) with return value
    mov edi, eax
    call exit

    ; Fallback in case exit returns
    mov edi, eax
    mov eax, 1 ; SYS_EXIT
    syscall

.halt_loop:
    pause
    jmp .halt_loop
