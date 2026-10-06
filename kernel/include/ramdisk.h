/*
 * SeldOS - Humboldt Kernel Project
 * In-Memory Ramdisk (Initrd) Subsystem
 * Supports Multiboot 1 and Multiboot 2 module loading.
 * GPLv3 Licensed.
 */

#ifndef SELD_RAMDISK_H
#define SELD_RAMDISK_H

#include <stdint.h>
#include <stddef.h>

struct ramdisk_info {
    uint64_t phys_start;
    uint64_t phys_end;
    uint64_t size;
    uint8_t* virt_base;
    int is_present;
};

void ramdisk_init(uint64_t mb_magic, uint64_t mb_info_addr);
void ramdisk_expand_in_ram(void);
struct ramdisk_info* ramdisk_get_info(void);
int ramdisk_read_sectors(uint32_t lba, uint32_t count, void* buffer);
int ramdisk_write_sectors(uint32_t lba, uint32_t count, const void* buffer);

#endif
