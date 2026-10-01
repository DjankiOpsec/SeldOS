/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * Kernel Dynamic Memory Allocator (kmalloc / kfree) Implementation
 * Free-list first-fit allocator backed by PMM pages.
 * GPLv3 Licensed.
 */

#include "kmalloc.h"
#include "pmm.h"
#include "vmm.h"
#include "string.h"
#include "serial.h"

#define HEAP_INITIAL_PAGES 256  // 1 MiB initial heap

static struct block_header* heap_head = NULL;
static uint64_t heap_base = 0;
static uint64_t heap_total_size = 0;
static size_t active_allocs = 0;

void kmalloc_init(void) {
    void* initial_frames = pmm_alloc_frames(HEAP_INITIAL_PAGES);
    if (!initial_frames) {
        serial_puts("[-] FATAL: Failed to allocate initial heap pages!\n");
        return;
    }

    heap_base = (uint64_t)phys_to_virt((uint64_t)initial_frames);
    heap_total_size = HEAP_INITIAL_PAGES * PAGE_SIZE;

    heap_head = (struct block_header*)heap_base;
    heap_head->magic = KMALLOC_MAGIC;
    heap_head->is_free = 1;
    heap_head->size = heap_total_size - BLOCK_HEADER_SIZE;
    heap_head->next = NULL;
    heap_head->prev = NULL;

    active_allocs = 0;

    serial_puts("[+] Kernel Heap Initialized: ");
    serial_print_dec(heap_total_size / 1024);
    serial_puts(" KiB at ");
    serial_print_hex(heap_base);
    serial_puts("\n");
}

static void split_block(struct block_header* block, size_t size) {
    // Check if remaining space is big enough to form a new block
    if (block->size >= size + BLOCK_HEADER_SIZE + 16) {
        struct block_header* new_block = (struct block_header*)((uint8_t*)block + BLOCK_HEADER_SIZE + size);
        new_block->magic = KMALLOC_MAGIC;
        new_block->is_free = 1;
        new_block->size = block->size - size - BLOCK_HEADER_SIZE;
        new_block->next = block->next;
        new_block->prev = block;

        if (block->next) {
            block->next->prev = new_block;
        }
        block->next = new_block;
        block->size = size;
    }
}

static void merge_blocks(struct block_header* block) {
    // Merge with next block if free
    if (block->next && block->next->is_free) {
        block->size += BLOCK_HEADER_SIZE + block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }
    // Merge with prev block if free
    if (block->prev && block->prev->is_free) {
        block->prev->size += BLOCK_HEADER_SIZE + block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
    }
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    // Align size to 16 bytes
    size = (size + 15) & ~15ULL;

    struct block_header* curr = heap_head;
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            split_block(curr, size);
            curr->is_free = 0;
            active_allocs++;
            return (void*)((uint8_t*)curr + BLOCK_HEADER_SIZE);
        }
        curr = curr->next;
    }

    // Try expanding heap if exhausted
    size_t needed_pages = (size + BLOCK_HEADER_SIZE + PAGE_SIZE - 1) / PAGE_SIZE;
    if (needed_pages < 16) needed_pages = 16;

    void* new_pages = pmm_alloc_frames(needed_pages);
    if (!new_pages) {
        return NULL; // Out of physical frames
    }

    // Append new block in Higher-Half Direct Map
    struct block_header* new_block = (struct block_header*)phys_to_virt((uint64_t)new_pages);
    new_block->magic = KMALLOC_MAGIC;
    new_block->is_free = 1;
    new_block->size = (needed_pages * PAGE_SIZE) - BLOCK_HEADER_SIZE;
    new_block->next = NULL;

    // Find tail
    struct block_header* tail = heap_head;
    while (tail && tail->next) {
        tail = tail->next;
    }
    if (tail) {
        tail->next = new_block;
        new_block->prev = tail;
    } else {
        heap_head = new_block;
    }

    heap_total_size += needed_pages * PAGE_SIZE;

    split_block(new_block, size);
    new_block->is_free = 0;
    active_allocs++;
    return (void*)((uint8_t*)new_block + BLOCK_HEADER_SIZE);
}

void* kzalloc(size_t size) {
    void* ptr = kmalloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void kfree(void* ptr) {
    if (!ptr) return;

    struct block_header* block = (struct block_header*)((uint8_t*)ptr - BLOCK_HEADER_SIZE);
    if (block->magic != KMALLOC_MAGIC) {
        serial_puts("[-] ERROR: kfree corrupted block or invalid pointer at ");
        serial_print_hex((uint64_t)ptr);
        serial_puts("\n");
        return;
    }

    block->is_free = 1;
    if (active_allocs > 0) active_allocs--;
    merge_blocks(block);
}

struct heap_stats kmalloc_get_stats(void) {
    struct heap_stats stats;
    stats.heap_start = heap_base;
    stats.heap_size = heap_total_size;
    stats.used_bytes = 0;
    stats.free_bytes = 0;
    stats.allocations_count = active_allocs;

    struct block_header* curr = heap_head;
    while (curr) {
        if (curr->is_free) {
            stats.free_bytes += curr->size;
        } else {
            stats.used_bytes += curr->size + BLOCK_HEADER_SIZE;
        }
        curr = curr->next;
    }
    return stats;
}
