/* SeldOS - Higher-Half x86_64 Linker Script */
ENTRY(start)

KERNEL_VIRTUAL_BASE = 0xFFFFFFFF80000000;
KERNEL_PHYSICAL_BASE = 0x100000;

SECTIONS {
    . = KERNEL_PHYSICAL_BASE;

    /* Boot sections in lower identity range (physical 1MB) */
    .boot ALIGN(4096) : {
        *(.multiboot_header)
        *(.boot_text)
        *(.boot_data)
        *(.boot_bss)
    }

    /* Transition to Higher-Half virtual address */
    . += KERNEL_VIRTUAL_BASE;

    kernel_start = .;

    .text ALIGN(4096) : AT(ADDR(.text) - KERNEL_VIRTUAL_BASE) {
        text_start = .;
        *(.text .text.*)
        text_end = .;
    }

    .rodata ALIGN(4096) : AT(ADDR(.rodata) - KERNEL_VIRTUAL_BASE) {
        rodata_start = .;
        *(.rodata .rodata.*)
        rodata_end = .;
    }

    .data ALIGN(4096) : AT(ADDR(.data) - KERNEL_VIRTUAL_BASE) {
        data_start = .;
        *(.data .data.*)
        data_end = .;
    }

    .bss ALIGN(4096) : AT(ADDR(.bss) - KERNEL_VIRTUAL_BASE) {
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
