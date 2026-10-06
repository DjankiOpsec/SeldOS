/*
 * SeldOS - Humboldt Kernel Project
 * In-Memory Ramdisk (Initrd) Driver Implementation
 * Multiboot 1 / Multiboot 2 module parser and memory-backed block I/O.
 * GPLv3 Licensed.
 */

#include "ramdisk.h"
#include "pmm.h"
#include "vmm.h"
#include "string.h"
#include "serial.h"

static struct ramdisk_info s_ramdisk = {
    .phys_start = 0,
    .phys_end = 0,
    .size = 0,
    .virt_base = NULL,
    .is_present = 0
};

void ramdisk_init(uint64_t mb_magic, uint64_t mb_info_addr) {
    s_ramdisk.phys_start = 0;
    s_ramdisk.phys_end = 0;
    s_ramdisk.size = 0;
    s_ramdisk.virt_base = NULL;
    s_ramdisk.is_present = 0;

    if (mb_info_addr == 0) return;

    if (mb_magic == 0x36d76289) { // Multiboot 2
        uint8_t* tag_ptr = (uint8_t*)(mb_info_addr + 8);
        while (1) {
            uint32_t type = *(uint32_t*)tag_ptr;
            uint32_t size = *(uint32_t*)(tag_ptr + 4);
            if (type == 0) break; // End tag

            if (type == 3) { // Multiboot 2 Module tag
                uint32_t mod_start = *(uint32_t*)(tag_ptr + 8);
                uint32_t mod_end   = *(uint32_t*)(tag_ptr + 12);
                if (mod_end > mod_start) {
                    s_ramdisk.phys_start = mod_start;
                    s_ramdisk.phys_end = mod_end;
                    s_ramdisk.size = (uint64_t)mod_end - mod_start;
                    s_ramdisk.virt_base = (uint8_t*)phys_to_virt(mod_start);
                    s_ramdisk.is_present = 1;
                    break;
                }
            }
            tag_ptr += ((size + 7) & ~7);
        }
    } else if (mb_magic == 0x2badb002) { // Multiboot 1
        uint32_t flags = *(uint32_t*)mb_info_addr;
        if (flags & (1 << 3)) { // Modules valid
            uint32_t mods_count = *(uint32_t*)(mb_info_addr + 20);
            uint32_t mods_addr  = *(uint32_t*)(mb_info_addr + 24);
            if (mods_count > 0 && mods_addr != 0) {
                uint32_t* mod_entry = (uint32_t*)phys_to_virt(mods_addr);
                uint32_t mod_start = mod_entry[0];
                uint32_t mod_end   = mod_entry[1];
                if (mod_end > mod_start) {
                    s_ramdisk.phys_start = mod_start;
                    s_ramdisk.phys_end = mod_end;
                    s_ramdisk.size = (uint64_t)mod_end - mod_start;
                    s_ramdisk.virt_base = (uint8_t*)phys_to_virt(mod_start);
                    s_ramdisk.is_present = 1;
                }
            }
        }
    }

    if (s_ramdisk.is_present) {
        serial_puts("[+] Ramdisk: Multiboot module detected at Phys ");
        serial_print_hex(s_ramdisk.phys_start);
        serial_puts(" - ");
        serial_print_hex(s_ramdisk.phys_end);
        serial_puts(" (Size: ");
        serial_print_dec((uint32_t)(s_ramdisk.size / 1024));
        serial_puts(" KiB)\n");
    }
}

#define RAMDISK_EXPAND_CAPACITY (34 * 1024 * 1024) // 34 MiB capacity (65536 data blocks + 2081 metadata blocks)

void ramdisk_expand_in_ram(void) {
    if (!s_ramdisk.is_present || !s_ramdisk.virt_base || s_ramdisk.size >= RAMDISK_EXPAND_CAPACITY) {
        return;
    }

    size_t target_size = RAMDISK_EXPAND_CAPACITY;
    size_t target_frames = target_size / PAGE_SIZE;
    void* new_phys = pmm_alloc_frames(target_frames);

    if (!new_phys) {
        // Fallback to 16 MiB if 34 MiB contiguous is not available
        target_size = 16 * 1024 * 1024;
        target_frames = target_size / PAGE_SIZE;
        if (target_size > s_ramdisk.size) {
            new_phys = pmm_alloc_frames(target_frames);
        }
    }

    if (!new_phys) {
        serial_puts("[-] Ramdisk: Unable to allocate expanded buffer, retaining packed image.\n");
        return;
    }

    uint8_t* new_virt = (uint8_t*)phys_to_virt((uint64_t)new_phys);
    memset(new_virt, 0, target_size);
    memcpy(new_virt, s_ramdisk.virt_base, s_ramdisk.size);

    // Free original frames if valid
    if (s_ramdisk.phys_start != 0 && s_ramdisk.phys_end > s_ramdisk.phys_start) {
        size_t old_start_frame = s_ramdisk.phys_start / PAGE_SIZE;
        size_t old_count = (s_ramdisk.phys_end - s_ramdisk.phys_start + PAGE_SIZE - 1) / PAGE_SIZE;
        pmm_free_frames((void*)(old_start_frame * PAGE_SIZE), old_count);
    }

    s_ramdisk.phys_start = (uint64_t)new_phys;
    s_ramdisk.phys_end = (uint64_t)new_phys + target_size;
    s_ramdisk.size = target_size;
    s_ramdisk.virt_base = new_virt;

    serial_puts("[+] Ramdisk: Expanded in RAM to ");
    serial_print_dec((uint32_t)(target_size / (1024 * 1024)));
    serial_puts(" MiB (Full SeldFS Read/Write capability online)\n");
}

struct ramdisk_info* ramdisk_get_info(void) {
    return &s_ramdisk;
}

int ramdisk_read_sectors(uint32_t lba, uint32_t count, void* buffer) {
    if (!s_ramdisk.is_present || !s_ramdisk.virt_base || !buffer) {
        return -1;
    }

    uint64_t offset = (uint64_t)lba * 512;
    uint64_t bytes = (uint64_t)count * 512;

    if (offset + bytes > s_ramdisk.size) {
        return -1;
    }

    memcpy(buffer, s_ramdisk.virt_base + offset, bytes);
    return 0;
}

int ramdisk_write_sectors(uint32_t lba, uint32_t count, const void* buffer) {
    if (!s_ramdisk.is_present || !s_ramdisk.virt_base || !buffer) {
        return -1;
    }

    uint64_t offset = (uint64_t)lba * 512;
    uint64_t bytes = (uint64_t)count * 512;

    if (offset + bytes > s_ramdisk.size) {
        return -1;
    }

    memcpy(s_ramdisk.virt_base + offset, buffer, bytes);
    return 0;
}
