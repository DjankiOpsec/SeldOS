/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * Physical Memory Manager (PMM) Implementation
 * Supports Multiboot 1 & Multiboot 2 memory map inspection.
 * Bitmap-based page frame tracking.
 * GPLv3 Licensed.
 */

#include "pmm.h"
#include "vmm.h"
#include "ramdisk.h"
#include "string.h"
#include "serial.h"
#include "panic.h"

extern char kernel_start[];
extern char kernel_end[];

#define BITMAP_INDEX(a) ((a) / 64)
#define BITMAP_OFFSET(a) ((a) % 64)

static uint64_t* pmm_bitmap = NULL;
static size_t total_frames = 0;
static size_t used_frames = 0;
static uint64_t total_physical_memory = 0;
static int s_ram_drv_enabled = 1;

static inline void bitmap_set(size_t frame) {
    pmm_bitmap[BITMAP_INDEX(frame)] |= (1ULL << BITMAP_OFFSET(frame));
}

static inline void bitmap_clear(size_t frame) {
    pmm_bitmap[BITMAP_INDEX(frame)] &= ~(1ULL << BITMAP_OFFSET(frame));
}

static inline int bitmap_test(size_t frame) {
    return (pmm_bitmap[BITMAP_INDEX(frame)] & (1ULL << BITMAP_OFFSET(frame))) != 0;
}

// Multiboot 1 Structures
struct multiboot1_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
} __attribute__((packed));

struct multiboot1_mmap_entry {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed));

// Multiboot 2 Structures
struct multiboot2_tag {
    uint32_t type;
    uint32_t size;
};

struct multiboot2_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
};

struct multiboot2_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
};

void pmm_init(uint64_t mb_magic, uint64_t mb_info_addr) {
    ramdisk_init(mb_magic, mb_info_addr);
    uint64_t highest_addr = 0x8000000; // Fallback: 128 MiB

    if (mb_magic == 0x36d76289 && mb_info_addr != 0) {
        // Multiboot 2
        uint8_t* tag_ptr = (uint8_t*)(mb_info_addr + 8);
        while (1) {
            struct multiboot2_tag* tag = (struct multiboot2_tag*)tag_ptr;
            if (tag->type == 0) {
                break; // End tag
            }
            if (tag->type == 6) { // Memory map tag
                struct multiboot2_tag_mmap* mmap_tag = (struct multiboot2_tag_mmap*)tag;
                uint8_t* entry_ptr = tag_ptr + sizeof(struct multiboot2_tag_mmap);
                uint8_t* end_ptr = tag_ptr + mmap_tag->size;

                while (entry_ptr < end_ptr) {
                    struct multiboot2_mmap_entry* entry = (struct multiboot2_mmap_entry*)entry_ptr;
                    if (entry->type == 1) { // Available RAM
                        uint64_t top = entry->addr + entry->len;
                        if (top > highest_addr) {
                            highest_addr = top;
                        }
                    }
                    entry_ptr += mmap_tag->entry_size;
                }
            }
            tag_ptr += ((tag->size + 7) & ~7);
        }
    } else if (mb_magic == 0x2badb002 && mb_info_addr != 0) {
        // Multiboot 1
        struct multiboot1_info* mb1 = (struct multiboot1_info*)mb_info_addr;
        if (mb1->flags & (1 << 6)) { // mmap valid
            uint8_t* mmap_ptr = (uint8_t*)((uint64_t)mb1->mmap_addr + 0xFFFF800000000000ULL);
            uint8_t* end_ptr = mmap_ptr + mb1->mmap_length;
            while (mmap_ptr < end_ptr) {
                struct multiboot1_mmap_entry* entry = (struct multiboot1_mmap_entry*)mmap_ptr;
                if (entry->type == 1) {
                    uint64_t top = entry->addr + entry->len;
                    if (top > highest_addr) {
                        highest_addr = top;
                    }
                }
                mmap_ptr += entry->size + sizeof(uint32_t);
            }
        }
    }

    // Cap at 1 GiB for now
    if (highest_addr > 0x40000000) {
        highest_addr = 0x40000000;
    }

    total_physical_memory = highest_addr;
    total_frames = total_physical_memory / PAGE_SIZE;

    // In Higher-Half, kernel_end is at 0xFFFFFFFF80xxxxxx.
    // Bitmap address in physical memory, mapped via HHDM:
    uint64_t bitmap_phys = ((uint64_t)kernel_end - KERNEL_VIRT_OFFSET + 4095) & ~4095ULL;
    pmm_bitmap = (uint64_t*)phys_to_virt(bitmap_phys);

    size_t bitmap_bytes = (total_frames / 8) + 1;
    size_t bitmap_frames = (bitmap_bytes + PAGE_SIZE - 1) / PAGE_SIZE;

    // Initialize all frames as free (0)
    memset(pmm_bitmap, 0, bitmap_bytes);
    used_frames = 0;

    // Mark the first 1MB as used (BIOS, IVT, BDA, VGA buffer 0xB8000)
    size_t first_mb_frames = 0x100000 / PAGE_SIZE;
    for (size_t i = 0; i < first_mb_frames; i++) {
        bitmap_set(i);
        used_frames++;
    }

    // Physical address of kernel start is 0x100000 (1MB)
    // Physical address of bitmap end:
    size_t kernel_start_frame = 0x100000 / PAGE_SIZE;
    size_t reserved_end_frame = (bitmap_phys / PAGE_SIZE) + bitmap_frames;
    for (size_t i = kernel_start_frame; i < reserved_end_frame; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_frames++;
        }
    }

    // Reserve In-Memory Ramdisk frames (if present)
    struct ramdisk_info* rd = ramdisk_get_info();
    if (rd && rd->is_present && rd->size > 0) {
        size_t rd_start_frame = rd->phys_start / PAGE_SIZE;
        size_t rd_end_frame = (rd->phys_end + PAGE_SIZE - 1) / PAGE_SIZE;
        for (size_t i = rd_start_frame; i < rd_end_frame && i < total_frames; i++) {
            if (!bitmap_test(i)) {
                bitmap_set(i);
                used_frames++;
            }
        }
    }

    ramdisk_expand_in_ram();

    serial_puts("[+] PMM Initialized: ");
    serial_print_dec(total_physical_memory / (1024 * 1024));
    serial_puts(" MiB Total (");
    serial_print_dec(total_frames - used_frames);
    serial_puts(" free frames)\n");
}

void* pmm_alloc_frame(void) {
    if (!s_ram_drv_enabled) {
        if (!kernel_panic_in_progress()) {
            kernel_panic("physical frame allocation failed: physical memory manager exhausted");
        }
        return NULL;
    }
    for (size_t i = 0; i < total_frames; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_frames++;
            return (void*)(i * PAGE_SIZE);
        }
    }
    return NULL; // Out of memory
}

void* pmm_alloc_frames(size_t count) {
    if (!s_ram_drv_enabled) {
        if (!kernel_panic_in_progress()) {
            kernel_panic("contiguous physical frames allocation failed: physical memory manager exhausted");
        }
        return NULL;
    }
    if (count == 0) return NULL;
    size_t contiguous = 0;
    size_t start_frame = 0;

    for (size_t i = 0; i < total_frames; i++) {
        if (!bitmap_test(i)) {
            if (contiguous == 0) {
                start_frame = i;
            }
            contiguous++;
            if (contiguous == count) {
                for (size_t j = start_frame; j < start_frame + count; j++) {
                    bitmap_set(j);
                }
                used_frames += count;
                return (void*)(start_frame * PAGE_SIZE);
            }
        } else {
            contiguous = 0;
        }
    }
    return NULL;
}

void pmm_free_frame(void* ptr) {
    uint64_t addr = (uint64_t)ptr;
    size_t frame = addr / PAGE_SIZE;
    if (frame < total_frames && bitmap_test(frame)) {
        bitmap_clear(frame);
        used_frames--;
    }
}

void pmm_free_frames(void* ptr, size_t count) {
    uint64_t addr = (uint64_t)ptr;
    size_t start_frame = addr / PAGE_SIZE;
    for (size_t i = 0; i < count; i++) {
        size_t frame = start_frame + i;
        if (frame < total_frames && bitmap_test(frame)) {
            bitmap_clear(frame);
            used_frames--;
        }
    }
}

int pmm_is_frame_allocated(void* ptr) {
    uint64_t addr = (uint64_t)ptr;
    size_t frame = addr / PAGE_SIZE;
    if (frame < total_frames) {
        return bitmap_test(frame);
    }
    return -1;
}

struct pmm_stats pmm_get_stats(void) {
    struct pmm_stats s;
    s.total_memory = total_physical_memory;
    s.used_memory = (uint64_t)used_frames * PAGE_SIZE;
    s.free_memory = s.total_memory - s.used_memory;
    s.total_frames = total_frames;
    s.used_frames = used_frames;
    return s;
}

int ram_driver_disable(void) {
    if (!s_ram_drv_enabled) return -1;
    s_ram_drv_enabled = 0;
    serial_puts("[!] DRIVER: RAM physical memory frame allocator brutally disabled!\n");
    return 0;
}

int ram_is_driver_enabled(void) {
    return s_ram_drv_enabled;
}

void pmm_secure_wipe_all_free(void) {
    serial_puts("[+] OpSec: Initiating Cold-Boot defense (scrubbing unallocated RAM frames)...\n");
    size_t scrubbed = 0;
    for (size_t i = 0; i < total_frames; i++) {
        if (!bitmap_test(i)) {
            uint64_t pa = (uint64_t)i * PAGE_SIZE;
            if (pa < 0x100000000ULL) { // within 4 GiB HHDM
                uint8_t* virt = (uint8_t*)phys_to_virt(pa);
                memset(virt, 0, PAGE_SIZE);
                scrubbed++;
            }
        }
    }
    // Invalidate processor caches
    __asm__ volatile ("wbinvd" ::: "memory");
    serial_puts("[+] OpSec: RAM scrub complete: ");
    serial_print_dec((uint64_t)scrubbed);
    serial_puts(" free physical frames wiped.\n");
}
