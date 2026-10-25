/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * Kernel Dynamic Memory Allocator (kmalloc / kfree) Implementation
 * Free-list first-fit allocator backed by PMM pages.
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#include "kmalloc.h"
#include "pmm.h"
#include "vmm.h"
#include "string.h"
#include "serial.h"
#include "panic.h"

#define HEAP_INITIAL_PAGES 64   // 256 KiB initial heap (reduced from 1024 KiB)

static struct block_header* heap_head = NULL;
static uint64_t heap_base = 0;
static uint64_t heap_total_size = 0;
static size_t active_allocs = 0;

void kmalloc_init(void) {
    size_t initial_pages = HEAP_INITIAL_PAGES;
    void* initial_frames = pmm_alloc_frames(initial_pages);
    if (!initial_frames) {
        // Fallback for extreme low-memory environments
        initial_pages = 32;
        initial_frames = pmm_alloc_frames(initial_pages);
        if (!initial_frames) {
            initial_pages = 16;
            initial_frames = pmm_alloc_frames(initial_pages);
            if (!initial_frames) {
                initial_pages = 8;
                initial_frames = pmm_alloc_frames(initial_pages);
                if (!initial_frames) {
                    initial_pages = 4;
                    initial_frames = pmm_alloc_frames(initial_pages);
                }
            }
        }
    }
    if (!initial_frames) {
        serial_puts("[-] FATAL: Failed to allocate initial heap pages!\n");
        return;
    }

    heap_base = (uint64_t)phys_to_virt((uint64_t)initial_frames);
    heap_total_size = initial_pages * PAGE_SIZE;

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
            if (block->next->magic != KMALLOC_MAGIC || block->next->prev != block) {
                serial_puts("[-] FATAL: Kernel heap block corruption in split_block!\n");
                kernel_panic("Kernel heap corruption detected: forward link broken in split");
                return;
            }
            block->next->prev = new_block;
        }
        block->next = new_block;
        block->size = size;
    }
}

static void merge_blocks(struct block_header* block) {
    if (!block || block->magic != KMALLOC_MAGIC) return;

    // Validate bidirectional integrity to prevent unlink exploitation
    if (block->next && (block->next->magic != KMALLOC_MAGIC || block->next->prev != block)) {
        serial_puts("[-] FATAL: Kernel heap block corruption in block->next!\n");
        kernel_panic("Kernel heap corruption detected: forward link broken");
        return;
    }
    if (block->prev && (block->prev->magic != KMALLOC_MAGIC || block->prev->next != block)) {
        serial_puts("[-] FATAL: Kernel heap block corruption in block->prev!\n");
        kernel_panic("Kernel heap corruption detected: backward link broken");
        return;
    }

    // Merge with next block if free and strictly contiguous in memory
    if (block->next && block->next->is_free &&
        ((uint8_t*)block + BLOCK_HEADER_SIZE + block->size == (uint8_t*)block->next)) {
        block->next->magic = 0;
        block->size += BLOCK_HEADER_SIZE + block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }
    // Merge with prev block if free and strictly contiguous in memory
    if (block->prev && block->prev->is_free &&
        ((uint8_t*)block->prev + BLOCK_HEADER_SIZE + block->prev->size == (uint8_t*)block)) {
        block->magic = 0;
        block->prev->size += BLOCK_HEADER_SIZE + block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
    }
}

static struct block_header* heap_grow(size_t min_bytes) {
    size_t min_pages = (min_bytes + BLOCK_HEADER_SIZE + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t alloc_pages = min_pages;
    if (alloc_pages < 16) {
        alloc_pages = 16;
    }

    void* new_pages = pmm_alloc_frames(alloc_pages);
    if (!new_pages && alloc_pages > min_pages) {
        // Fallback to minimal required pages if 16 contiguous frames unavailable
        alloc_pages = min_pages;
        new_pages = pmm_alloc_frames(alloc_pages);
    }
    if (!new_pages) {
        return NULL; // Out of physical frames
    }

    // Append new block in Higher-Half Direct Map
    struct block_header* new_block = (struct block_header*)phys_to_virt((uint64_t)new_pages);
    new_block->magic = KMALLOC_MAGIC;
    new_block->is_free = 1;
    new_block->size = (alloc_pages * PAGE_SIZE) - BLOCK_HEADER_SIZE;
    new_block->next = NULL;
    new_block->prev = NULL;

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

    heap_total_size += alloc_pages * PAGE_SIZE;
    return new_block;
}

void* kmalloc(size_t size) {
    if (!ram_is_driver_enabled()) {
        if (!kernel_panic_in_progress()) {
            kernel_panic("kernel heap allocation failed: physical memory manager exhausted (out of memory)");
        }
        return NULL;
    }
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

    // Dynamic heap expansion via heap_grow
    struct block_header* new_block = heap_grow(size);
    if (!new_block) {
        return NULL;
    }

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

    if (block->is_free) {
        serial_puts("[-] ERROR: kfree double free detected at ");
        serial_print_hex((uint64_t)ptr);
        serial_puts("\n");
        return;
    }

    // OpSec Zero-on-Free: erase payload to destroy residual keys and secrets
    memset(ptr, 0, block->size);

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
