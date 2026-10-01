; SeldOS Higher-Half 64-bit Trampoline
; Boots in 32-bit protected mode via Multiboot 1/2.
; Sets up initial paging:
;   - PML4[0]: Identity map first 1GB (P3_identity -> P2_table)
;   - PML4[511]: Higher-Half Kernel map at 0xFFFFFFFF80000000 (P3_higher -> P2_table)
;   - PML4[256]: HHDM Direct Physical Map at 0xFFFF800000000000 (P3_hhdm -> P2_table)
; Jumps into 64-bit Long Mode in the Higher Half!

global start
extern kernel_main
extern gdt64_ptr

section .boot_bss nobits
align 4096
boot_p4:
    resb 4096
boot_p3_ident:
    resb 4096
boot_p3_higher:
    resb 4096
boot_p3_hhdm:
    resb 4096
boot_p2:
    resb 4096
boot_stack_bottom:
    resb 4096 * 4
boot_stack_top:

section .boot_data
align 16
boot_gdt64:
    dq 0 ; 0x00: Null
.code: equ $ - boot_gdt64
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53) ; 0x08: Kernel Code (DPL 0, 64-bit)
.data: equ $ - boot_gdt64
    dq (1<<44) | (1<<47) | (1<<41)            ; 0x10: Kernel Data (DPL 0, writable)
.pointer:
    dw $ - boot_gdt64 - 1
    dq boot_gdt64

section .boot_text
bits 32
start:
    mov esp, boot_stack_top
    mov edi, ebx ; Multiboot info pointer
    mov esi, eax ; Multiboot magic

    ; Check multiboot magic (Multiboot 1: 0x2badb002, Multiboot 2: 0x36d76289)
    cmp eax, 0x36d76289
    je .mb_ok
    cmp eax, 0x2badb002
    je .mb_ok
    jmp .error

.mb_ok:
    call setup_boot_paging
    call enable_paging_and_sse

    lgdt [boot_gdt64.pointer]

    ; Jump to 64-bit lower trampoline
    jmp boot_gdt64.code:long_mode_entry

.error:
    mov dword [0xb8000], 0x4f524f45 ; 'ER' red
    hlt

setup_boot_paging:
    ; Map 512 entries of 2MiB huge pages into boot_p2 (First 1GiB of physical memory)
    mov ecx, 0
.map_p2:
    mov eax, 0x200000 ; 2MiB
    mul ecx           ; EAX = start phys addr
    or eax, 0b10000011 ; Present, Writable, 2MiB Huge Page
    mov [boot_p2 + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .map_p2

    ; boot_p3_ident[0] -> boot_p2
    mov eax, boot_p2
    or eax, 0b11 ; Present, Writable
    mov [boot_p3_ident], eax

    ; boot_p3_higher[510] -> boot_p2 (0xFFFFFFFF80000000 is index 510 in PDPT 511!)
    ; 0xFFFFFFFF80000000: PML4 index = 511, PDPT index = 510, PD index = 0
    mov [boot_p3_higher + 510 * 8], eax

    ; boot_p3_hhdm[0] -> boot_p2 (0xFFFF800000000000: PML4 index = 256, PDPT index = 0)
    mov [boot_p3_hhdm], eax

    ; Link into boot_p4:
    ; PML4[0] = boot_p3_ident (identity map)
    mov eax, boot_p3_ident
    or eax, 0b11
    mov [boot_p4 + 0 * 8], eax

    ; PML4[511] = boot_p3_higher (higher half kernel base)
    mov eax, boot_p3_higher
    or eax, 0b11
    mov [boot_p4 + 511 * 8], eax

    ; PML4[256] = boot_p3_hhdm (higher half direct physical map)
    mov eax, boot_p3_hhdm
    or eax, 0b11
    mov [boot_p4 + 256 * 8], eax

    ret

enable_paging_and_sse:
    ; Load CR3
    mov eax, boot_p4
    mov cr3, eax

    ; Enable PAE (bit 5) in CR4
    mov eax, cr4
    or eax, (1 << 5)
    mov cr4, eax

    ; Enable Long Mode (LME), System Call Extensions (SCE), and No-Execute (NXE) in EFER MSR (0xC0000080)
    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 11) | (1 << 8) | (1 << 0) ; NXE (bit 11) | LME (bit 8) | SCE (bit 0)
    wrmsr

    ; Enable Paging (bit 31), WP (bit 16), MP (bit 1), clear EM (bit 2) in CR0
    mov eax, cr0
    or eax, (1 << 31) | (1 << 16) | (1 << 1)
    and eax, ~(1 << 2)
    mov cr0, eax

    ; Enable OSFXSR (bit 9) and OSXMMEXCPT (bit 10) in CR4
    mov eax, cr4
    or eax, (1 << 9) | (1 << 10)
    mov cr4, eax

    ret

bits 64
long_mode_entry:
    ; Now in 64-bit mode (running at physical address ~0x100xxx)
    mov ax, boot_gdt64.data
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Far-jump / indirect jump to Higher-Half address:
    mov rax, higher_half_entry
    jmp rax

section .text
higher_half_entry:
    ; Running at virtual address 0xFFFFFFFF80xxxxxx !
    ; Unmap identity mapping (PML4[0] = 0) to enforce Higher-Half isolation
    mov rax, 0xFFFF800000000000 ; Use HHDM to access boot_p4
    add rax, boot_p4
    mov qword [rax], 0
    ; Reload CR3 to flush TLB
    mov rax, cr3
    mov cr3, rax

    ; Set RSP to a proper Higher-Half stack in .bss
    mov rsp, kernel_stack_top

    ; Pass multiboot parameters (rdi = info, rsi = magic)
    ; Convert multiboot info pointer in rdi to HHDM pointer (if non-zero)
    test rdi, rdi
    jz .call_kernel
    mov rax, 0xFFFF800000000000
    add rdi, rax

.call_kernel:
    call kernel_main

.halt:
    cli
    hlt
    jmp .halt

section .bss
align 16
global kernel_stack_bottom
global kernel_stack_top
kernel_stack_bottom:
    resb 32768
kernel_stack_top:
