/*
 * Seld not's Linux Userspace Linker Script
 * Target: 64-bit flat binary linked at 0x400000
 */

ENTRY(_start)

SECTIONS
{
    . = 0x400000;

    .text : ALIGN(16) {
        *(.text._start)
        *(.text .text.*)
    }

    .rodata : ALIGN(16) {
        *(.rodata .rodata.*)
    }

    . = ALIGN(0x1000);

    .data : ALIGN(16) {
        *(.data .data.*)
    }

    .bss : ALIGN(16) {
        *(.bss .bss.*)
        *(COMMON)
    }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
