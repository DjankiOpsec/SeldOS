; Multiboot 1 & Multiboot 2 headers for SeldOS (Humboldt Edition)
section .multiboot_header
align 8

; --- Multiboot 1 Header (supported by QEMU -kernel directly) ---
MB1_MAGIC    equ 0x1BADB002
MB1_FLAGS    equ 0x00000007 ; ALIGN (1) + MEMINFO (2) + VIDEO (4)
MB1_CHECKSUM equ -(MB1_MAGIC + MB1_FLAGS)

align 4
    dd MB1_MAGIC
    dd MB1_FLAGS
    dd MB1_CHECKSUM
    dd 0, 0, 0, 0, 0 ; unused header fields
    dd 0             ; mode_type (0 = linear graphics)
    dd 680           ; width  (Humboldt Penguin anatomical height 680 mm)
    dd 334           ; height (Algarrobo Reserve latitude 33.4° S / 334)
    dd 32            ; depth (32-bit RGB)

; --- Multiboot 2 Header (supported by GRUB2) ---
align 8
mb2_start:
    dd 0xe85250d6                ; Multiboot2 magic number
    dd 0                         ; Architecture 0 (i386 32-bit entry)
    dd mb2_end - mb2_start       ; Header length
    dd 0x100000000 - (0xe85250d6 + 0 + (mb2_end - mb2_start)) ; Checksum

    ; Framebuffer tag (type 5)
    align 8
    dw 5                         ; type = 5 (FRAMEBUFFER)
    dw 1                         ; flags = 1 (optional, don't fail boot if not supported)
    dd 20                        ; size = 20
    dd 680                       ; width  (Humboldt Penguin anatomical height 680 mm)
    dd 334                       ; height (Algarrobo Reserve latitude 33.4° S / 334)
    dd 32                        ; depth (32-bit RGB)

    ; End tag
    align 8
    dw 0    ; type = 0
    dw 0    ; flags = 0
    dd 8    ; size = 8
mb2_end:
