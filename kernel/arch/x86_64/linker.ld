/* SeldOS - Higher-Half x86_64 Linker Script */
ENTRY(start)

KERNEL_VIRTUAL_BASE = 0xFFFFFFFF80000000;
KERNEL_PHYSICAL_BASE = 0x100000;

PHDRS {
    boot_text PT_LOAD FLAGS(5); /* R-X */
    boot_data PT_LOAD FLAGS(6); /* RW- */
    text      PT_LOAD FLAGS(5); /* R-X */
    rodata    PT_LOAD FLAGS(4); /* R-- */
    data      PT_LOAD FLAGS(6); /* RW- */
}

SECTIONS {
    . = KERNEL_PHYSICAL_BASE;

    /* Boot sections in lower identity range (physical 1MB) */
    .boot_text ALIGN(4096) : {
        *(.multiboot_header)
        *(.boot_text)
    } :boot_text

    .boot_data ALIGN(4096) : {
        *(.boot_data)
        *(.boot_bss)
    } :boot_data

    /* Transition to Higher-Half virtual address */
    . += KERNEL_VIRTUAL_BASE;

    kernel_start = .;

    .text ALIGN(4096) : AT(ADDR(.text) - KERNEL_VIRTUAL_BASE) {
        text_start = .;
        *(.text .text.*)
        text_end = .;
    } :text

    .rodata ALIGN(4096) : AT(ADDR(.rodata) - KERNEL_VIRTUAL_BASE) {
        rodata_start = .;
        *(.rodata .rodata.*)
        rodata_end = .;
    } :rodata

    .data ALIGN(4096) : AT(ADDR(.data) - KERNEL_VIRTUAL_BASE) {
        data_start = .;
        *(.data .data.*)
        data_end = .;
    } :data

    .bss ALIGN(4096) : AT(ADDR(.bss) - KERNEL_VIRTUAL_BASE) {
        bss_start = .;
        *(COMMON)
        *(.bss .bss.*)
        bss_end = .;
    } :data

    . = ALIGN(4096);
    kernel_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
    }
}
