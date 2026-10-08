/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * Virtual Memory Manager (VMM) Interface
 * Higher-Half direct physical memory mapping and fine-grained 4KiB page mapping.
 * GPLv3 Licensed.
 */

#ifndef SELD_VMM_H
#define SELD_VMM_H

#include <stdint.h>
#include <stddef.h>

#if defined(__riscv)
#define KERNEL_VIRT_OFFSET 0xFFFFFFFF80000000ULL
#define HHDM_VIRT_OFFSET   0xFFFFFFC000000000ULL
#else
#define KERNEL_VIRT_OFFSET 0xFFFFFFFF80000000ULL
#define HHDM_VIRT_OFFSET   0xFFFF800000000000ULL
#endif

#define VMM_FLAG_PRESENT    (1ULL << 0)
#define VMM_FLAG_WRITABLE   (1ULL << 1)
#define VMM_FLAG_USER       (1ULL << 2)
#define VMM_FLAG_NO_EXECUTE (1ULL << 63)

// Convert physical address to Higher-Half Direct Map virtual address
static inline void* phys_to_virt(uint64_t phys) {
    return (void*)(phys + HHDM_VIRT_OFFSET);
}

// Convert HHDM virtual address to physical address
static inline uint64_t virt_to_phys(void* virt) {
    return (uint64_t)virt - HHDM_VIRT_OFFSET;
}

static inline uint64_t vmm_get_active_root(void) {
#if defined(__riscv)
    uint64_t satp;
    __asm__ volatile ("csrr %0, satp" : "=r"(satp));
    return (satp & 0x00000FFFFFFFFFFFULL) << 12;
#else
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
#endif
}

void vmm_init(void);
uint64_t* vmm_create_address_space(void);
void vmm_destroy_address_space(uint64_t* pml4_virt);
int vmm_map_page(uint64_t* pml4_virt, uint64_t virt, uint64_t phys, uint64_t flags);
int vmm_unmap_page(uint64_t* pml4_virt, uint64_t virt);
void vmm_switch_pml4(uint64_t phys_pml4);
uint64_t vmm_get_mapping(uint64_t* pml4_virt, uint64_t virt);
uint64_t vmm_get_kernel_pml4(void);

#endif /* SELD_VMM_H */
