/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * Physical Memory Manager (PMM) Interface
 * Bitmap frame allocator for 4KiB physical pages.
 * GPLv3 Licensed.
 */

#ifndef SELD_PMM_H
#define SELD_PMM_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096

struct pmm_stats {
    uint64_t total_memory;
    uint64_t free_memory;
    uint64_t used_memory;
    uint64_t total_frames;
    uint64_t used_frames;
};

void pmm_init(uint64_t mb_magic, uint64_t mb_info_addr);
void* pmm_alloc_frame(void);
void* pmm_alloc_frames(size_t count);
void pmm_free_frame(void* ptr);
void pmm_free_frames(void* ptr, size_t count);
int pmm_is_frame_allocated(void* ptr);
struct pmm_stats pmm_get_stats(void);

#endif /* SELD_PMM_H */
