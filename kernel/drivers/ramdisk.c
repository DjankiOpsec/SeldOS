/*
 * SeldOS - Humboldt Kernel Project
 * In-Memory Ramdisk (Initrd) Driver Implementation
 * Multiboot 1 / Multiboot 2 module parser and memory-backed block I/O.
 * GNU AGPLv3 Licensed - Humboldt Underground.
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

extern const uint8_t disk_blob_start[] __attribute__((weak));
extern const uint8_t disk_blob_end[] __attribute__((weak));
extern const uint64_t disk_blob_size __attribute__((weak));

#define RAMDISK_EXPAND_CAPACITY (34 * 1024 * 1024) // 34 MiB capacity (65536 data blocks + 2081 metadata blocks)
#define RAMDISK_PAGE_SIZE 4096
#define RAMDISK_MAX_PAGES (RAMDISK_EXPAND_CAPACITY / RAMDISK_PAGE_SIZE) // 8704 pages

static uint8_t* s_sparse_pages[RAMDISK_MAX_PAGES];
static uint8_t* s_initial_base = NULL;
static uint64_t s_initial_size = 0;
static int s_is_sparse = 0;

void ramdisk_init(uint64_t mb_magic, uint64_t mb_info_addr) {
    if (s_ramdisk.is_present && s_is_sparse) {
        return;
    }
    s_ramdisk.phys_start = 0;
    s_ramdisk.phys_end = 0;
    s_ramdisk.size = 0;
    s_ramdisk.virt_base = NULL;
    s_ramdisk.is_present = 0;
    s_is_sparse = 0;
    s_initial_base = NULL;
    s_initial_size = 0;
    memset(s_sparse_pages, 0, sizeof(s_sparse_pages));

    if (disk_blob_start && (uintptr_t)disk_blob_end > (uintptr_t)disk_blob_start) {
        s_ramdisk.virt_base = (uint8_t*)disk_blob_start;
        s_ramdisk.size = (uint64_t)(disk_blob_end - disk_blob_start);
#if defined(__riscv)
        s_ramdisk.phys_start = (uint64_t)disk_blob_start - KERNEL_VIRT_OFFSET + 0x80000000ULL;
#else
        s_ramdisk.phys_start = (uint64_t)disk_blob_start - KERNEL_VIRT_OFFSET;
#endif
        s_ramdisk.phys_end = s_ramdisk.phys_start + s_ramdisk.size;
        s_ramdisk.is_present = 1;
        serial_puts("[+] Ramdisk: Embedded SeldFS storage blob active (Size: ");
        serial_print_dec((uint32_t)(s_ramdisk.size / 1024));
        serial_puts(" KiB)\n");
        return;
    }

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

void ramdisk_expand_in_ram(void) {
    if (!s_ramdisk.is_present || !s_ramdisk.virt_base || s_ramdisk.size >= RAMDISK_EXPAND_CAPACITY) {
        return;
    }

    s_initial_base = s_ramdisk.virt_base;
    s_initial_size = s_ramdisk.size;
    memset(s_sparse_pages, 0, sizeof(s_sparse_pages));
    s_is_sparse = 1;
    s_ramdisk.size = RAMDISK_EXPAND_CAPACITY;

    serial_puts("[+] Ramdisk: Sparse On-Demand Ramdisk initialized (Capacity: ");
    serial_print_dec((uint32_t)(s_ramdisk.size / (1024 * 1024)));
    serial_puts(" MiB, On-Demand Allocation active)\n");
}

struct ramdisk_info* ramdisk_get_info(void) {
    return &s_ramdisk;
}

int ramdisk_read_sectors(uint32_t lba, uint32_t count, void* buffer) {
    if (!s_ramdisk.is_present || !buffer) {
        return -1;
    }
    if (count == 0) return 0;

    uint64_t offset = (uint64_t)lba * 512;
    uint64_t bytes = (uint64_t)count * 512;

    if (offset + bytes > s_ramdisk.size || offset + bytes < offset) {
        return -1;
    }

    if (!s_is_sparse) {
        if (!s_ramdisk.virt_base) return -1;
        memcpy(buffer, s_ramdisk.virt_base + offset, bytes);
        return 0;
    }

    uint8_t* dst = (uint8_t*)buffer;
    uint64_t cur_offset = offset;
    uint64_t remaining = bytes;

    while (remaining > 0) {
        size_t page_idx = (size_t)(cur_offset / RAMDISK_PAGE_SIZE);
        if (page_idx >= RAMDISK_MAX_PAGES) return -1;

        size_t offset_in_page = (size_t)(cur_offset % RAMDISK_PAGE_SIZE);
        size_t chunk_len = RAMDISK_PAGE_SIZE - offset_in_page;
        if (chunk_len > remaining) {
            chunk_len = (size_t)remaining;
        }

        if (s_sparse_pages[page_idx] != NULL) {
            memcpy(dst, s_sparse_pages[page_idx] + offset_in_page, chunk_len);
        } else if (cur_offset < s_initial_size && s_initial_base != NULL) {
            size_t avail = (cur_offset + chunk_len <= s_initial_size) ? chunk_len : (size_t)(s_initial_size - cur_offset);
            memcpy(dst, s_initial_base + cur_offset, avail);
            if (chunk_len > avail) {
                memset(dst + avail, 0, chunk_len - avail);
            }
        } else {
            memset(dst, 0, chunk_len);
        }

        dst += chunk_len;
        cur_offset += chunk_len;
        remaining -= chunk_len;
    }

    return 0;
}

int ramdisk_write_sectors(uint32_t lba, uint32_t count, const void* buffer) {
    if (!s_ramdisk.is_present || !buffer) {
        return -1;
    }
    if (count == 0) return 0;

    uint64_t offset = (uint64_t)lba * 512;
    uint64_t bytes = (uint64_t)count * 512;

    if (offset + bytes > s_ramdisk.size || offset + bytes < offset) {
        return -1;
    }

    if (!s_is_sparse) {
        if (!s_ramdisk.virt_base) return -1;
        memcpy(s_ramdisk.virt_base + offset, buffer, bytes);
        return 0;
    }

    const uint8_t* src = (const uint8_t*)buffer;
    uint64_t cur_offset = offset;
    uint64_t remaining = bytes;

    while (remaining > 0) {
        size_t page_idx = (size_t)(cur_offset / RAMDISK_PAGE_SIZE);
        if (page_idx >= RAMDISK_MAX_PAGES) return -1;

        size_t offset_in_page = (size_t)(cur_offset % RAMDISK_PAGE_SIZE);
        size_t chunk_len = RAMDISK_PAGE_SIZE - offset_in_page;
        if (chunk_len > remaining) {
            chunk_len = (size_t)remaining;
        }

        if (s_sparse_pages[page_idx] == NULL) {
            void* frame = pmm_alloc_frame();
            if (!frame) {
                serial_puts("[-] Ramdisk: Out of memory allocating sparse sector frame\n");
                return -1;
            }
            uint8_t* frame_virt = (uint8_t*)phys_to_virt((uint64_t)frame);
            uint64_t page_start = (uint64_t)page_idx * RAMDISK_PAGE_SIZE;

            if (page_start < s_initial_size && s_initial_base != NULL) {
                size_t avail = (page_start + RAMDISK_PAGE_SIZE <= s_initial_size) ? RAMDISK_PAGE_SIZE : (size_t)(s_initial_size - page_start);
                memcpy(frame_virt, s_initial_base + page_start, avail);
                if (avail < RAMDISK_PAGE_SIZE) {
                    memset(frame_virt + avail, 0, RAMDISK_PAGE_SIZE - avail);
                }
            } else {
                memset(frame_virt, 0, RAMDISK_PAGE_SIZE);
            }
            s_sparse_pages[page_idx] = frame_virt;
        }

        memcpy(s_sparse_pages[page_idx] + offset_in_page, src, chunk_len);

        src += chunk_len;
        cur_offset += chunk_len;
        remaining -= chunk_len;
    }

    return 0;
}
