/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * GDT, TSS, and Privilege Boundary Interface
 * Sets up 64-bit Kernel Code/Data (Ring 0), User Code/Data (Ring 3), and TSS.
 * GPLv3 Licensed.
 */

#ifndef SELD_GDT_H
#define SELD_GDT_H

#include <stdint.h>
#include <stddef.h>

#define KERNEL_CS 0x08
#define KERNEL_DS 0x10
#define USER_DS   0x1B // 0x18 | 3
#define USER_CS   0x23 // 0x20 | 3

struct tss_entry {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed));

void gdt_init(void);
void tss_set_rsp0(uint64_t rsp0);

#endif /* SELD_GDT_H */
