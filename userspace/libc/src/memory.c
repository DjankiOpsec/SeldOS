/*
 * SeldOS - Humboldt Kernel Project
 * SNL C Standard Library (libsnl)
 * User-Mode Dynamic Heap Allocator (malloc / free / realloc / calloc)
 * First-Fit with Bidirectional Coalescing & Block Header Integrity.
 * GPLv3 Licensed.
 */

#include "stdlib.h"
#include "unistd.h"
#include "string.h"
#include "seld.h"

#define BLOCK_MAGIC 0x534C4448ULL /* 'SLDH' - Seld Heap */

struct block_header {
    uint64_t magic;
    size_t   size;               /* Size of user payload */
    uint32_t is_free;
    uint32_t pad;
    struct block_header* next;
    struct block_header* prev;
};

#define BLOCK_HEADER_SIZE ((sizeof(struct block_header) + 15) & ~15ULL)
#define ALIGN16(x) (((x) + 15) & ~15ULL)
#define MIN_SPLIT_SIZE 16

static struct block_header* heap_head = NULL;
static void* current_heap_brk = NULL;

void* sbrk(intptr_t increment) {
    if (current_heap_brk == NULL) {
        current_heap_brk = seld_brk(NULL);
        if (current_heap_brk == (void*)-1 || current_heap_brk == NULL) {
            current_heap_brk = (void*)0x10000000ULL;
            void* res = seld_brk(current_heap_brk);
            if (res != (void*)-1 && res != NULL) {
                current_heap_brk = res;
            }
        }
    }

    if (increment == 0) {
        return current_heap_brk;
    }

    void* old_brk = current_heap_brk;
    void* new_brk = (void*)((uintptr_t)old_brk + increment);
    void* res = seld_brk(new_brk);
    if (res == (void*)-1 || res < new_brk) {
        return (void*)-1;
    }

    current_heap_brk = new_brk;
    return old_brk;
}

int brk(void* addr) {
    void* res = seld_brk(addr);
    if (res == (void*)-1 || res != addr) {
        return -1;
    }
    current_heap_brk = res;
    return 0;
}

void* malloc(size_t size) {
    if (size == 0) return NULL;

    size = ALIGN16(size);

    /* 1. Search free list for suitable block (First-Fit) */
    struct block_header* curr = heap_head;
    while (curr != NULL) {
        if (curr->magic != BLOCK_MAGIC) {
            /* Heap corruption detected */
            return NULL;
        }

        if (curr->is_free && curr->size >= size) {
            /* Check if block can be split */
            if (curr->size >= size + BLOCK_HEADER_SIZE + MIN_SPLIT_SIZE) {
                struct block_header* split = (struct block_header*)((uint8_t*)curr + BLOCK_HEADER_SIZE + size);
                split->magic = BLOCK_MAGIC;
                split->is_free = 1;
                split->pad = 0;
                split->size = curr->size - size - BLOCK_HEADER_SIZE;
                split->next = curr->next;
                split->prev = curr;

                if (curr->next != NULL) {
                    curr->next->prev = split;
                }
                curr->next = split;
                curr->size = size;
            }

            curr->is_free = 0;
            return (void*)((uint8_t*)curr + BLOCK_HEADER_SIZE);
        }

        curr = curr->next;
    }

    /* 2. No suitable block found: request new segment via sbrk() */
    size_t total_needed = size + BLOCK_HEADER_SIZE;
    size_t chunk_size = (total_needed + 4095) & ~4095ULL;
    if (chunk_size < 16384) {
        chunk_size = 16384; /* Minimum allocation request: 16 KiB */
    }

    void* mem = sbrk((intptr_t)chunk_size);
    if (mem == (void*)-1 || mem == NULL) {
        return NULL;
    }

    struct block_header* new_block = (struct block_header*)mem;
    new_block->magic = BLOCK_MAGIC;
    new_block->is_free = 0;
    new_block->pad = 0;
    new_block->size = size;
    new_block->next = NULL;
    new_block->prev = NULL;

    /* Append to heap list */
    if (heap_head == NULL) {
        heap_head = new_block;
    } else {
        struct block_header* tail = heap_head;
        while (tail->next != NULL) {
            tail = tail->next;
        }
        tail->next = new_block;
        new_block->prev = tail;
    }

    /* Split trailing free space if chunk had extra room */
    if (chunk_size >= total_needed + BLOCK_HEADER_SIZE + MIN_SPLIT_SIZE) {
        struct block_header* split = (struct block_header*)((uint8_t*)new_block + BLOCK_HEADER_SIZE + size);
        split->magic = BLOCK_MAGIC;
        split->is_free = 1;
        split->pad = 0;
        split->size = chunk_size - total_needed - BLOCK_HEADER_SIZE;
        split->next = new_block->next;
        split->prev = new_block;
        new_block->next = split;
    }

    return (void*)((uint8_t*)new_block + BLOCK_HEADER_SIZE);
}

void free(void* ptr) {
    if (!ptr) return;

    struct block_header* block = (struct block_header*)((uint8_t*)ptr - BLOCK_HEADER_SIZE);
    if (block->magic != BLOCK_MAGIC) {
        return; /* Corrupted header or invalid pointer */
    }

    // OpSec Zero-on-Free: erase payload to destroy residual cryptographic material & buffers
    memset(ptr, 0, block->size);

    block->is_free = 1;

    /* Coalesce with forward adjacent block if free */
    if (block->next != NULL && block->next->magic == BLOCK_MAGIC && block->next->is_free) {
        if ((uint8_t*)block + BLOCK_HEADER_SIZE + block->size == (uint8_t*)block->next) {
            block->size += BLOCK_HEADER_SIZE + block->next->size;
            block->next = block->next->next;
            if (block->next != NULL && block->next->magic == BLOCK_MAGIC) {
                block->next->prev = block;
            }
        }
    }

    /* Coalesce with backward adjacent block if free */
    if (block->prev != NULL && block->prev->magic == BLOCK_MAGIC && block->prev->is_free) {
        if ((uint8_t*)block->prev + BLOCK_HEADER_SIZE + block->prev->size == (uint8_t*)block) {
            block->prev->size += BLOCK_HEADER_SIZE + block->size;
            block->prev->next = block->next;
            if (block->next != NULL && block->next->magic == BLOCK_MAGIC) {
                block->next->prev = block->prev;
            }
        }
    }
}

void* realloc(void* ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    struct block_header* block = (struct block_header*)((uint8_t*)ptr - BLOCK_HEADER_SIZE);
    if (block->magic != BLOCK_MAGIC) return NULL;

    size = ALIGN16(size);

    /* Case A: Current block payload already satisfies request */
    if (block->size >= size) {
        return ptr;
    }

    /* Case B: Next adjacent block is free and combined size satisfies request */
    if (block->next != NULL && block->next->is_free) {
        if ((uint8_t*)block + BLOCK_HEADER_SIZE + block->size == (uint8_t*)block->next) {
            size_t combined = block->size + BLOCK_HEADER_SIZE + block->next->size;
            if (combined >= size) {
                block->next = block->next->next;
                if (block->next != NULL) {
                    block->next->prev = block;
                }
                block->size = combined;
                return ptr;
            }
        }
    }

    /* Case C: Allocate new block, copy existing content, and free old block */
    void* new_ptr = malloc(size);
    if (!new_ptr) return NULL;

    size_t copy_len = (block->size < size) ? block->size : size;
    memcpy(new_ptr, ptr, copy_len);
    free(ptr);

    return new_ptr;
}

void* calloc(size_t nmemb, size_t size) {
    if (nmemb != 0 && size > (size_t)-1 / nmemb) {
        return NULL; /* Overflow protection */
    }

    size_t total = nmemb * size;
    void* ptr = malloc(total);
    if (ptr != NULL) {
        memset(ptr, 0, total);
    }
    return ptr;
}
