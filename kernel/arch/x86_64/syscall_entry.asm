; SeldOS Fast SYSCALL / SYSRET Assembly Entry
; Saves user RSP, switches to TSS.RSP0 stack, saves registers, calls C handler,
; and returns to Ring 3 via SYSRETQ. Supports nested Ring 3 executions (SYS_SPAWN).
; GPLv3 Licensed.

bits 64
section .text

global syscall_entry_asm
global jump_to_userspace_asm
global user_exit_to_kernel

extern fast_syscall_dispatcher
extern kernel_syscall_stack_top
extern kernel_pml4_phys

%define MAX_USER_DEPTH 8

section .data
align 8

; Storage for user RSP when entering kernel via SYSCALL
global user_rsp_storage
user_rsp_storage: dq 0

; Nesting depth and per-depth kernel stack offset
user_depth: dq 0
user_stack_offset: dq 0

; Storage for kernel registers when entering Ring 3 (indexed by user_depth)
kernel_saved_rsp: times MAX_USER_DEPTH dq 0
kernel_saved_rbp: times MAX_USER_DEPTH dq 0
kernel_saved_rbx: times MAX_USER_DEPTH dq 0
kernel_saved_r12: times MAX_USER_DEPTH dq 0
kernel_saved_r13: times MAX_USER_DEPTH dq 0
kernel_saved_r14: times MAX_USER_DEPTH dq 0
kernel_saved_r15: times MAX_USER_DEPTH dq 0

section .text

syscall_entry_asm:
    ; CPU saves RIP -> RCX, RFLAGS -> R11
    ; CPU sets CS = STAR[47:32] (0x08), SS = STAR[47:32] + 8 (0x10)
    ; Interrupts are disabled by FMASK (bit 9 cleared)

    ; Save user RSP
    mov [rel user_rsp_storage], rsp

    ; Switch to kernel syscall stack (with per-nesting-depth offset)
    mov rsp, [rel kernel_syscall_stack_top]
    sub rsp, [rel user_stack_offset]

    ; Push registers to form user context frame
    push qword [rel user_rsp_storage] ; Original user RSP
    push r11                          ; Original user RFLAGS
    push rcx                          ; Original user RIP
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    push r8
    push r9
    push r10
    push rdx
    push rsi
    push rdi

    ; Syscall arguments in x86_64 ABI:
    ; RAX = syscall number
    ; RDI = arg1
    ; RSI = arg2
    ; RDX = arg3
    ; R10 = arg4
    mov r8, r10
    mov rcx, rdx
    mov rdx, rsi
    mov rsi, rdi
    mov rdi, rax

    sti
    call fast_syscall_dispatcher
    cli
    ; Return value from syscall is in RAX

    ; Restore registers
    pop rdi
    pop rsi
    pop rdx
    pop r10
    pop r9
    pop r8
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    pop rcx ; Restore user RIP
    pop r11 ; Restore user RFLAGS
    pop rsp ; Restore user RSP

    ; Return to Ring 3 (SYSRETQ sets CS = STAR[63:48]+16 = 0x23, SS = STAR[63:48]+8 = 0x1B)
    o64 sysret

; int jump_to_userspace_asm(void (*user_func)(void), void* user_stack_top, uint64_t user_cr3, uint64_t argc, void* argv);
; rdi = user_func
; rsi = user_stack_top
; rdx = user_cr3
; rcx = argc
; r8  = argv
jump_to_userspace_asm:
    ; Save supervisor callee-saved registers into slot for current user_depth
    mov rax, [rel user_depth]
    cmp rax, MAX_USER_DEPTH
    jae .depth_cap
    mov [rel kernel_saved_rsp + rax*8], rsp
    mov [rel kernel_saved_rbp + rax*8], rbp
    mov [rel kernel_saved_rbx + rax*8], rbx
    mov [rel kernel_saved_r12 + rax*8], r12
    mov [rel kernel_saved_r13 + rax*8], r13
    mov [rel kernel_saved_r14 + rax*8], r14
    mov [rel kernel_saved_r15 + rax*8], r15
    inc qword [rel user_depth]
    mov rax, [rel user_depth]
    shl rax, 13 ; 8192 bytes per depth level
    mov [rel user_stack_offset], rax
.depth_cap:

    ; Switch to user address space (CR3) if provided
    test rdx, rdx
    jz .skip_cr3
    mov cr3, rdx
.skip_cr3:

    ; Prepare IRETQ stack frame for Ring 3 transition:
    ; SS     (0x18 | 3 = 0x1B)
    ; RSP    (rsi)
    ; RFLAGS (0x202 = Interrupts enabled, bit 1 set)
    ; CS     (0x20 | 3 = 0x23)
    ; RIP    (rdi)

    push qword 0x1B          ; User Data Segment (RPL 3)
    push rsi                 ; User Stack Pointer
    push qword 0x202         ; RFLAGS (IF = 1)
    push qword 0x23          ; User Code Segment (RPL 3)
    push rdi                 ; User Entry Point

    ; Clear segment registers with user data selector (0x1B)
    mov ax, 0x1B
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Pass argc in rdi, argv in rsi to userspace
    mov rdi, rcx
    mov rsi, r8

    ; Clear other general purpose registers for OpSec cleanliness
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor r8, r8
    xor r9, r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15
    xor rbp, rbp

    ; Drop privileges to Ring 3!
    iretq

; void user_exit_to_kernel(uint64_t exit_code);
; rdi = exit_code
user_exit_to_kernel:
    ; Switch back to kernel PML4
    mov rax, [rel kernel_pml4_phys]
    test rax, rax
    jz .skip_kcr3
    mov cr3, rax
.skip_kcr3:

    ; Restore kernel segment selectors
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Decrement nesting depth
    mov rax, [rel user_depth]
    test rax, rax
    jz .depth_zero
    dec rax
    mov [rel user_depth], rax
.depth_zero:
    mov rdx, rax
    shl rdx, 13 ; 8192 bytes per depth level
    mov [rel user_stack_offset], rdx

    ; Restore supervisor registers from active depth
    mov rsp, [rel kernel_saved_rsp + rax*8]
    mov rbp, [rel kernel_saved_rbp + rax*8]
    mov rbx, [rel kernel_saved_rbx + rax*8]
    mov r12, [rel kernel_saved_r12 + rax*8]
    mov r13, [rel kernel_saved_r13 + rax*8]
    mov r14, [rel kernel_saved_r14 + rax*8]
    mov r15, [rel kernel_saved_r15 + rax*8]

    ; Return exit_code in RAX to caller of jump_to_userspace_asm
    mov rax, rdi
    ret
