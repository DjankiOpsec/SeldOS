/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * Kernel Dynamic Memory Allocator (kmalloc / kfree) Interface
 * Boundary-tag first-fit heap allocator in Ring 0.
 * GPLv3 Licensed.
 */

#ifndef SELD_KMALLOC_H
#define SELD_KMALLOC_H

#include <stdint.h>
#include <stddef.h>

#define KMALLOC_MAGIC 0x5E1DCAFE

struct block_header {
    uint32_t magic;
    uint32_t is_free;
    size_t   size; // Usable payload size
    struct block_header* next;
    struct block_header* prev;
};

#define BLOCK_HEADER_SIZE sizeof(struct block_header)

struct heap_stats {
    uint64_t heap_start;
    uint64_t heap_size;
    uint64_t used_bytes;
    uint64_t free_bytes;
    size_t   allocations_count;
};

void kmalloc_init(void);
void* kmalloc(size_t size);
void* kzalloc(size_t size);
void kfree(void* ptr);
struct heap_stats kmalloc_get_stats(void);

#endif /* SELD_KMALLOC_H */
