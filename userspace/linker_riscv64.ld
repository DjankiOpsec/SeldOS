/*
 * Seld not's Linux Userspace Linker Script (RISC-V 64-bit)
 * Target: 64-bit ELF executable linked at 0x400000
 */

ENTRY(_start)

SECTIONS
{
    . = 0x400000;

    .text : ALIGN(16) {
        *(.text._start)
        *(.text .text.*)
    }

    . = ALIGN(0x1000);

    .rodata : ALIGN(16) {
        *(.rodata .rodata.*)
        *(.srodata .srodata.*)
    }

    . = ALIGN(0x1000);

    .data : ALIGN(16) {
        *(.data .data.*)
        *(.sdata .sdata.*)
    }

    .bss : ALIGN(16) {
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(COMMON)
    }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
