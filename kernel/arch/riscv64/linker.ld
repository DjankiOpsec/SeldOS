/* SeldOS - Higher-Half RISC-V 64-bit Linker Script */
ENTRY(_start)

KERNEL_VIRTUAL_OFFSET = 0xFFFFFFFF00000000;
KERNEL_PHYSICAL_BASE = 0x80200000;

SECTIONS {
    . = KERNEL_PHYSICAL_BASE;

    /* Boot sections in lower identity range (physical 0x80200000) */
    .boot ALIGN(4096) : {
        KEEP(*(.boot.text))
        *(.boot.data)
        *(.boot.bss)
    }

    /* Transition to Higher-Half virtual address */
    . += KERNEL_VIRTUAL_OFFSET;

    kernel_start = .;

    .text ALIGN(4096) : AT(ADDR(.text) - KERNEL_VIRTUAL_OFFSET) {
        text_start = .;
        *(.text .text.*)
        text_end = .;
    }

    .rodata ALIGN(4096) : AT(ADDR(.rodata) - KERNEL_VIRTUAL_OFFSET) {
        rodata_start = .;
        *(.rodata .rodata.*)
        rodata_end = .;
    }

    .data ALIGN(4096) : AT(ADDR(.data) - KERNEL_VIRTUAL_OFFSET) {
        data_start = .;
        *(.data .data.*)
        data_end = .;
    }

    .bss ALIGN(4096) : AT(ADDR(.bss) - KERNEL_VIRTUAL_OFFSET) {
        bss_start = .;
        *(COMMON)
        *(.bss .bss.*)
        bss_end = .;
    }

    . = ALIGN(4096);
    kernel_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
    }
}
