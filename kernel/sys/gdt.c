/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * GDT & TSS Implementation for Ring 0 / Ring 3 Privilege Separation
 * 0x00: Null
 * 0x08: Kernel Code 64 (DPL 0)
 * 0x10: Kernel Data 64 (DPL 0)
 * 0x18: User Data 64 (DPL 3, RPL 3 = 0x1B)
 * 0x20: User Code 64 (DPL 3, RPL 3 = 0x23)
 * 0x28: TSS 64-bit Descriptor (16 bytes)
 * GPLv3 Licensed.
 */

#include "gdt.h"
#include "string.h"
#include "serial.h"

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct tss_descriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  flags1;
    uint8_t  flags2;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed));

static uint64_t gdt_entries[7]; // 5 normal (0..4) + 2 for TSS (5, 6)
static struct tss_entry kernel_tss;
static struct gdt_ptr gdtp;

static uint8_t initial_kernel_stack[8192];

void tss_set_rsp0(uint64_t rsp0) {
    kernel_tss.rsp0 = rsp0;
}

void gdt_init(void) {
    memset(&kernel_tss, 0, sizeof(kernel_tss));
    kernel_tss.iopb_offset = sizeof(kernel_tss);
    kernel_tss.rsp0 = (uint64_t)initial_kernel_stack + sizeof(initial_kernel_stack);

    // 0x00: Null
    gdt_entries[0] = 0;

    // 0x08: Kernel Code (DPL 0, 64-bit, Executable, Readable)
    gdt_entries[1] = (1ULL << 43) | (1ULL << 44) | (1ULL << 47) | (1ULL << 53);

    // 0x10: Kernel Data (DPL 0, 64-bit, Writable)
    gdt_entries[2] = (1ULL << 41) | (1ULL << 44) | (1ULL << 47);

    // 0x18: User Data (DPL 3, 64-bit, Writable)
    gdt_entries[3] = (1ULL << 41) | (1ULL << 44) | (3ULL << 45) | (1ULL << 47);

    // 0x20: User Code (DPL 3, 64-bit, Executable, Readable)
    gdt_entries[4] = (1ULL << 43) | (1ULL << 44) | (3ULL << 45) | (1ULL << 47) | (1ULL << 53);

    // 0x28: TSS Descriptor (16 bytes = 2 uint64_t slots: 5 and 6)
    uint64_t tss_base = (uint64_t)&kernel_tss;
    uint32_t tss_limit = sizeof(kernel_tss) - 1;

    struct tss_descriptor* desc = (struct tss_descriptor*)&gdt_entries[5];
    desc->limit_low = (uint16_t)(tss_limit & 0xFFFF);
    desc->base_low  = (uint16_t)(tss_base & 0xFFFF);
    desc->base_mid  = (uint8_t)((tss_base >> 16) & 0xFF);
    desc->flags1    = 0x89; // Present, 64-bit TSS Available, DPL 0
    desc->flags2    = (uint8_t)((tss_limit >> 16) & 0x0F);
    desc->base_high = (uint8_t)((tss_base >> 24) & 0xFF);
    desc->base_upper = (uint32_t)(tss_base >> 32);
    desc->reserved  = 0;

    gdtp.limit = sizeof(gdt_entries) - 1;
    gdtp.base  = (uint64_t)&gdt_entries;

    // Load GDT
    __asm__ volatile ("lgdt %0" : : "m"(gdtp));

    // Reload Segment Registers
    __asm__ volatile (
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%ss\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        : : : "ax"
    );

    // Load Task Register
    __asm__ volatile ("ltr %%ax" : : "a"((uint16_t)0x28));

    serial_puts("[+] GDT: Loaded with Ring 0/Ring 3 descriptors and 64-bit TSS (LTR 0x28).\n");
}
