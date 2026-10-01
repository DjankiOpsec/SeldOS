; SeldOS ISR Assembly Stubs
bits 64
section .text

extern isr_handler

%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push qword 0          ; dummy error code
    push qword %1         ; interrupt number
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push qword %1         ; interrupt number (error code already on stack)
    jmp isr_common_stub
%endmacro

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_ERRCODE   21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_ERRCODE   29
ISR_ERRCODE   30
ISR_NOERRCODE 31

; IRQs (vectors 32 to 47)
ISR_NOERRCODE 32 ; IRQ0: Timer
ISR_NOERRCODE 33 ; IRQ1: Keyboard
ISR_NOERRCODE 34 ; IRQ2: Cascade
ISR_NOERRCODE 35 ; IRQ3: COM2
ISR_NOERRCODE 36 ; IRQ4: COM1
ISR_NOERRCODE 37 ; IRQ5: LPT2
ISR_NOERRCODE 38 ; IRQ6: Floppy
ISR_NOERRCODE 39 ; IRQ7: Spurious Master
ISR_NOERRCODE 40 ; IRQ8: RTC
ISR_NOERRCODE 41 ; IRQ9: ACPI
ISR_NOERRCODE 42 ; IRQ10
ISR_NOERRCODE 43 ; IRQ11
ISR_NOERRCODE 44 ; IRQ12: PS/2 Mouse
ISR_NOERRCODE 45 ; IRQ13: FPU
ISR_NOERRCODE 46 ; IRQ14: Primary ATA
ISR_NOERRCODE 47 ; IRQ15: Secondary ATA / Spurious Slave
ISR_NOERRCODE 128 ; Syscall 0x80

isr_common_stub:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp          ; Pass interrupt_frame pointer as first arg to isr_handler
    call isr_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16           ; Pop int_no and err_code
    iretq
