/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * Virtual Memory Manager (VMM) Implementation
 * - Higher-Half Direct Map (HHDM at 0xFFFF800000000000)
 * - Higher-Half Kernel Base (0xFFFFFFFF80000000)
 * - 4-Level Page Table walk & map (PML4 -> PDPT -> PD -> PT)
 * - Memory protection flags (RW, User/Supervisor, No-Execute)
 * GPLv3 Licensed.
 */

#include "vmm.h"
#include "pmm.h"
#include "string.h"
#include "serial.h"
#include "vga.h"

#define PML4_INDEX(va) (((va) >> 39) & 0x1FF)
#define PDPT_INDEX(va) (((va) >> 30) & 0x1FF)
#define PD_INDEX(va)   (((va) >> 21) & 0x1FF)
#define PT_INDEX(va)   (((va) >> 12) & 0x1FF)

uint64_t kernel_pml4_phys = 0;
static uint64_t* kernel_pml4_virt = NULL;

uint64_t vmm_get_kernel_pml4(void) {
    return kernel_pml4_phys;
}

#if defined(__riscv)

#define RISCV_L2_INDEX(va) (((va) >> 30) & 0x1FF)
#define RISCV_L1_INDEX(va) (((va) >> 21) & 0x1FF)
#define RISCV_L0_INDEX(va) (((va) >> 12) & 0x1FF)

#define RISCV_PTE_V (1ULL << 0)
#define RISCV_PTE_R (1ULL << 1)
#define RISCV_PTE_W (1ULL << 2)
#define RISCV_PTE_X (1ULL << 3)
#define RISCV_PTE_U (1ULL << 4)
#define RISCV_PTE_G (1ULL << 5)
#define RISCV_PTE_A (1ULL << 6)
#define RISCV_PTE_D (1ULL << 7)

#define RISCV_PTE_PA(pte) (((pte) >> 10) << 12)
#define RISCV_PA_PTE(pa)  (((pa) >> 12) << 10)

void vmm_switch_pml4(uint64_t phys_pml4) {
    uint64_t satp = (8ULL << 60) | (phys_pml4 >> 12);
    __asm__ volatile ("csrw satp, %0\n\tsfence.vma" : : "r"(satp) : "memory");
}

int vmm_map_page(uint64_t* pml4_virt, uint64_t virt, uint64_t phys, uint64_t flags) {
    size_t l2_i = RISCV_L2_INDEX(virt);
    size_t l1_i = RISCV_L1_INDEX(virt);
    size_t l0_i = RISCV_L0_INDEX(virt);

    if (!(pml4_virt[l2_i] & RISCV_PTE_V)) {
        uint64_t new_table = (uint64_t)pmm_alloc_frame();
        if (!new_table) return -1;
        memset(phys_to_virt(new_table), 0, PAGE_SIZE);
        pml4_virt[l2_i] = RISCV_PA_PTE(new_table) | RISCV_PTE_V;
    }

    uint64_t* l1_virt = (uint64_t*)phys_to_virt(RISCV_PTE_PA(pml4_virt[l2_i]));

    if (!(l1_virt[l1_i] & RISCV_PTE_V)) {
        uint64_t new_table = (uint64_t)pmm_alloc_frame();
        if (!new_table) return -1;
        memset(phys_to_virt(new_table), 0, PAGE_SIZE);
        l1_virt[l1_i] = RISCV_PA_PTE(new_table) | RISCV_PTE_V;
    }

    uint64_t* l0_virt = (uint64_t*)phys_to_virt(RISCV_PTE_PA(l1_virt[l1_i]));

    uint64_t pte_flags = RISCV_PTE_V | RISCV_PTE_R | RISCV_PTE_A | RISCV_PTE_D;
    if (flags & VMM_FLAG_WRITABLE) pte_flags |= RISCV_PTE_W;
    if (!(flags & VMM_FLAG_NO_EXECUTE)) pte_flags |= RISCV_PTE_X;
    if (flags & VMM_FLAG_USER) pte_flags |= RISCV_PTE_U;

    l0_virt[l0_i] = RISCV_PA_PTE(phys) | pte_flags;

    __asm__ volatile ("sfence.vma %0, zero" : : "r"(virt) : "memory");
    return 0;
}

int vmm_unmap_page(uint64_t* pml4_virt, uint64_t virt) {
    size_t l2_i = RISCV_L2_INDEX(virt);
    if (!(pml4_virt[l2_i] & RISCV_PTE_V)) return -1;

    uint64_t* l1_virt = (uint64_t*)phys_to_virt(RISCV_PTE_PA(pml4_virt[l2_i]));
    size_t l1_i = RISCV_L1_INDEX(virt);
    if (!(l1_virt[l1_i] & RISCV_PTE_V)) return -1;

    uint64_t* l0_virt = (uint64_t*)phys_to_virt(RISCV_PTE_PA(l1_virt[l1_i]));
    size_t l0_i = RISCV_L0_INDEX(virt);

    l0_virt[l0_i] = 0;
    __asm__ volatile ("sfence.vma %0, zero" : : "r"(virt) : "memory");
    return 0;
}

uint64_t vmm_get_mapping(uint64_t* pml4_virt, uint64_t virt) {
    if (!pml4_virt) return 0;
    size_t l2_i = RISCV_L2_INDEX(virt);
    if (!(pml4_virt[l2_i] & RISCV_PTE_V)) return 0;

    uint64_t* l1_virt = (uint64_t*)phys_to_virt(RISCV_PTE_PA(pml4_virt[l2_i]));
    size_t l1_i = RISCV_L1_INDEX(virt);
    if (!(l1_virt[l1_i] & RISCV_PTE_V)) return 0;

    uint64_t* l0_virt = (uint64_t*)phys_to_virt(RISCV_PTE_PA(l1_virt[l1_i]));
    size_t l0_i = RISCV_L0_INDEX(virt);
    if (!(l0_virt[l0_i] & RISCV_PTE_V)) return 0;

    return RISCV_PTE_PA(l0_virt[l0_i]);
}

uint64_t* vmm_create_address_space(void) {
    uint64_t pml4_phys = (uint64_t)pmm_alloc_frame();
    if (!pml4_phys) return NULL;

    uint64_t* pml4_virt = (uint64_t*)phys_to_virt(pml4_phys);
    memset(pml4_virt, 0, PAGE_SIZE);

    // Guarantee kernel page table higher-half mappings are always intact
    kernel_pml4_virt[256] = RISCV_PA_PTE(0x00000000ULL) | RISCV_PTE_V | RISCV_PTE_R | RISCV_PTE_W | RISCV_PTE_A | RISCV_PTE_D;
    kernel_pml4_virt[258] = RISCV_PA_PTE(0x80000000ULL) | RISCV_PTE_V | RISCV_PTE_R | RISCV_PTE_W | RISCV_PTE_A | RISCV_PTE_D;
    kernel_pml4_virt[510] = RISCV_PA_PTE(0x80000000ULL) | RISCV_PTE_V | RISCV_PTE_R | RISCV_PTE_W | RISCV_PTE_X | RISCV_PTE_A | RISCV_PTE_D;

    for (size_t i = 256; i < 512; i++) {
        pml4_virt[i] = kernel_pml4_virt[i];
    }

    // Explicitly guarantee essential supervisor Higher-Half mappings in child space
    pml4_virt[256] = kernel_pml4_virt[256];
    pml4_virt[258] = kernel_pml4_virt[258];
    pml4_virt[510] = kernel_pml4_virt[510];

    return pml4_virt;
}

void vmm_destroy_address_space(uint64_t* pml4_virt) {
    if (!pml4_virt) return;

    for (size_t i = 0; i < 256; i++) {
        if ((pml4_virt[i] & RISCV_PTE_V) && !(pml4_virt[i] & (RISCV_PTE_R | RISCV_PTE_W | RISCV_PTE_X))) {
            uint64_t* l1_virt = (uint64_t*)phys_to_virt(RISCV_PTE_PA(pml4_virt[i]));
            for (size_t j = 0; j < 512; j++) {
                if ((l1_virt[j] & RISCV_PTE_V) && !(l1_virt[j] & (RISCV_PTE_R | RISCV_PTE_W | RISCV_PTE_X))) {
                    uint64_t* l0_virt = (uint64_t*)phys_to_virt(RISCV_PTE_PA(l1_virt[j]));
                    for (size_t k = 0; k < 512; k++) {
                        if (l0_virt[k] & RISCV_PTE_V) {
                            uint64_t frame_phys = RISCV_PTE_PA(l0_virt[k]);
                            if (frame_phys >= 0x80000000ULL && frame_phys < 0x100000000ULL) {
                                memset(phys_to_virt(frame_phys), 0, PAGE_SIZE);
                            }
                            pmm_free_frame((void*)frame_phys);
                        }
                    }
                    pmm_free_frame((void*)RISCV_PTE_PA(l1_virt[j]));
                }
            }
            pmm_free_frame((void*)RISCV_PTE_PA(pml4_virt[i]));
        }
    }

    pmm_free_frame((void*)virt_to_phys(pml4_virt));
}

void vmm_init(void) {
    kernel_pml4_phys = (uint64_t)pmm_alloc_frame();
    kernel_pml4_virt = (uint64_t*)phys_to_virt(kernel_pml4_phys);
    memset(kernel_pml4_virt, 0, PAGE_SIZE);

    // HHDM MMIO (0xFFFFFFC000000000 -> 0x00000000, 1 GiB)
    kernel_pml4_virt[256] = RISCV_PA_PTE(0x00000000ULL) | RISCV_PTE_V | RISCV_PTE_R | RISCV_PTE_W | RISCV_PTE_A | RISCV_PTE_D;
    // HHDM RAM (0xFFFFFFC080000000 -> 0x80000000, 1 GiB)
    kernel_pml4_virt[258] = RISCV_PA_PTE(0x80000000ULL) | RISCV_PTE_V | RISCV_PTE_R | RISCV_PTE_W | RISCV_PTE_A | RISCV_PTE_D;
    // Higher-half Kernel (0xFFFFFFFF80000000 -> 0x80000000, 1 GiB)
    kernel_pml4_virt[510] = RISCV_PA_PTE(0x80000000ULL) | RISCV_PTE_V | RISCV_PTE_R | RISCV_PTE_W | RISCV_PTE_X | RISCV_PTE_A | RISCV_PTE_D;

    vmm_switch_pml4(kernel_pml4_phys);
    serial_puts("[+] VMM: RISC-V Sv39 Higher-Half Paging active. kernel_pml4_phys=");
    serial_print_hex(kernel_pml4_phys);
    serial_puts(" k510=");
    serial_print_hex(kernel_pml4_virt[510]);
    serial_puts("\n");
}

#else

void vmm_switch_pml4(uint64_t phys_pml4) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(phys_pml4) : "memory");
}

int vmm_map_page(uint64_t* pml4_virt, uint64_t virt, uint64_t phys, uint64_t flags) {
    size_t pml4_i = PML4_INDEX(virt);
    size_t pdpt_i = PDPT_INDEX(virt);
    size_t pd_i   = PD_INDEX(virt);
    size_t pt_i   = PT_INDEX(virt);

    // PML4 entry
    if (!(pml4_virt[pml4_i] & VMM_FLAG_PRESENT)) {
        uint64_t new_table = (uint64_t)pmm_alloc_frame();
        if (!new_table) return -1;
        memset(phys_to_virt(new_table), 0, PAGE_SIZE);
        pml4_virt[pml4_i] = new_table | VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | (flags & VMM_FLAG_USER);
    } else if (flags & VMM_FLAG_USER) {
        pml4_virt[pml4_i] |= VMM_FLAG_USER;
    }

    uint64_t* pdpt_virt = (uint64_t*)phys_to_virt(pml4_virt[pml4_i] & ~0xFFFULL & ~VMM_FLAG_NO_EXECUTE);

    // PDPT entry
    if (!(pdpt_virt[pdpt_i] & VMM_FLAG_PRESENT)) {
        uint64_t new_table = (uint64_t)pmm_alloc_frame();
        if (!new_table) return -1;
        memset(phys_to_virt(new_table), 0, PAGE_SIZE);
        pdpt_virt[pdpt_i] = new_table | VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | (flags & VMM_FLAG_USER);
    } else if (flags & VMM_FLAG_USER) {
        pdpt_virt[pdpt_i] |= VMM_FLAG_USER;
    }

    uint64_t* pd_virt = (uint64_t*)phys_to_virt(pdpt_virt[pdpt_i] & ~0xFFFULL & ~VMM_FLAG_NO_EXECUTE);

    // PD entry
    if (!(pd_virt[pd_i] & VMM_FLAG_PRESENT)) {
        uint64_t new_table = (uint64_t)pmm_alloc_frame();
        if (!new_table) return -1;
        memset(phys_to_virt(new_table), 0, PAGE_SIZE);
        pd_virt[pd_i] = new_table | VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | (flags & VMM_FLAG_USER);
    } else if (flags & VMM_FLAG_USER) {
        pd_virt[pd_i] |= VMM_FLAG_USER;
    }

    uint64_t* pt_virt = (uint64_t*)phys_to_virt(pd_virt[pd_i] & ~0xFFFULL & ~VMM_FLAG_NO_EXECUTE);

    // PT entry
    pt_virt[pt_i] = (phys & ~0xFFFULL) | flags | VMM_FLAG_PRESENT;

    // Flush TLB for this virtual address
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");

    return 0;
}

int vmm_unmap_page(uint64_t* pml4_virt, uint64_t virt) {
    size_t pml4_i = PML4_INDEX(virt);
    if (!(pml4_virt[pml4_i] & VMM_FLAG_PRESENT)) return -1;

    uint64_t* pdpt_virt = (uint64_t*)phys_to_virt(pml4_virt[pml4_i] & ~0xFFFULL);
    size_t pdpt_i = PDPT_INDEX(virt);
    if (!(pdpt_virt[pdpt_i] & VMM_FLAG_PRESENT)) return -1;

    uint64_t* pd_virt = (uint64_t*)phys_to_virt(pdpt_virt[pdpt_i] & ~0xFFFULL);
    size_t pd_i = PD_INDEX(virt);
    if (!(pd_virt[pd_i] & VMM_FLAG_PRESENT)) return -1;

    uint64_t* pt_virt = (uint64_t*)phys_to_virt(pd_virt[pd_i] & ~0xFFFULL);
    size_t pt_i = PT_INDEX(virt);

    pt_virt[pt_i] = 0;
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
    return 0;
}

uint64_t vmm_get_mapping(uint64_t* pml4_virt, uint64_t virt) {
    if (!pml4_virt) return 0;
    size_t pml4_i = PML4_INDEX(virt);
    if (!(pml4_virt[pml4_i] & VMM_FLAG_PRESENT)) return 0;

    uint64_t* pdpt_virt = (uint64_t*)phys_to_virt(pml4_virt[pml4_i] & ~0xFFFULL & ~VMM_FLAG_NO_EXECUTE);
    size_t pdpt_i = PDPT_INDEX(virt);
    if (!(pdpt_virt[pdpt_i] & VMM_FLAG_PRESENT)) return 0;

    uint64_t* pd_virt = (uint64_t*)phys_to_virt(pdpt_virt[pdpt_i] & ~0xFFFULL & ~VMM_FLAG_NO_EXECUTE);
    size_t pd_i = PD_INDEX(virt);
    if (!(pd_virt[pd_i] & VMM_FLAG_PRESENT)) return 0;

    uint64_t* pt_virt = (uint64_t*)phys_to_virt(pd_virt[pd_i] & ~0xFFFULL & ~VMM_FLAG_NO_EXECUTE);
    size_t pt_i = PT_INDEX(virt);
    if (!(pt_virt[pt_i] & VMM_FLAG_PRESENT)) return 0;

    return pt_virt[pt_i] & ~0xFFFULL & ~VMM_FLAG_NO_EXECUTE;
}

uint64_t* vmm_create_address_space(void) {
    uint64_t pml4_phys = (uint64_t)pmm_alloc_frame();
    if (!pml4_phys) return NULL;

    uint64_t* pml4_virt = (uint64_t*)phys_to_virt(pml4_phys);
    memset(pml4_virt, 0, PAGE_SIZE);

    // Share Higher-Half kernel space (entries 256..511)
    for (size_t i = 256; i < 512; i++) {
        pml4_virt[i] = kernel_pml4_virt[i];
    }

    return pml4_virt;
}

void vmm_destroy_address_space(uint64_t* pml4_virt) {
    if (!pml4_virt) return;

    struct fb_info* kfb = vga_get_fb_info();
    uint64_t fb_start = kfb ? kfb->phys_addr : 0;
    uint64_t fb_end = (kfb && kfb->phys_addr) ? (kfb->phys_addr + (uint64_t)kfb->pitch * kfb->height) : 0;

    // Scan userspace PML4 entries (0..255)
    for (size_t i = 0; i < 256; i++) {
        if (pml4_virt[i] & VMM_FLAG_PRESENT) {
            uint64_t* pdpt_virt = (uint64_t*)phys_to_virt(pml4_virt[i] & ~0xFFFULL & ~VMM_FLAG_NO_EXECUTE);
            for (size_t j = 0; j < 512; j++) {
                if (pdpt_virt[j] & VMM_FLAG_PRESENT) {
                    uint64_t* pd_virt = (uint64_t*)phys_to_virt(pdpt_virt[j] & ~0xFFFULL & ~VMM_FLAG_NO_EXECUTE);
                    for (size_t k = 0; k < 512; k++) {
                        if (pd_virt[k] & VMM_FLAG_PRESENT) {
                            uint64_t* pt_virt = (uint64_t*)phys_to_virt(pd_virt[k] & ~0xFFFULL & ~VMM_FLAG_NO_EXECUTE);
                            for (size_t m = 0; m < 512; m++) {
                                if (pt_virt[m] & VMM_FLAG_PRESENT) {
                                    uint64_t frame_phys = pt_virt[m] & ~0xFFFULL & ~VMM_FLAG_NO_EXECUTE;
                                    if (fb_start && frame_phys >= fb_start && frame_phys < fb_end) {
                                        // Framebuffer physical memory: skip
                                    } else {
                                        // OpSec: Wipe process page memory to prevent data remanence
                                        if (frame_phys < 0x100000000ULL) {
                                            memset(phys_to_virt(frame_phys), 0, PAGE_SIZE);
                                        }
                                        pmm_free_frame((void*)frame_phys);
                                    }
                                }
                            }
                            pmm_free_frame((void*)(pd_virt[k] & ~0xFFFULL & ~VMM_FLAG_NO_EXECUTE));
                        }
                    }
                    pmm_free_frame((void*)(pdpt_virt[j] & ~0xFFFULL & ~VMM_FLAG_NO_EXECUTE));
                }
            }
            pmm_free_frame((void*)(pml4_virt[i] & ~0xFFFULL & ~VMM_FLAG_NO_EXECUTE));
        }
    }

    pmm_free_frame((void*)virt_to_phys(pml4_virt));
}

extern char text_start[], text_end[];
extern char rodata_start[], rodata_end[];
extern char data_start[], data_end[];
extern char bss_start[], bss_end[];

void vmm_init(void) {
    // Read current boot CR3
    uint64_t boot_cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(boot_cr3));

    // Allocate fine-grained, permanent kernel PML4
    kernel_pml4_phys = (uint64_t)pmm_alloc_frame();
    kernel_pml4_virt = (uint64_t*)phys_to_virt(kernel_pml4_phys);
    memset(kernel_pml4_virt, 0, PAGE_SIZE);

    // 1. Direct Physical Map (HHDM at 0xFFFF800000000000) for first 4GiB using 2MiB huge pages
    uint64_t hhdm_pdpt_phys = (uint64_t)pmm_alloc_frame();
    uint64_t* hhdm_pdpt_virt = (uint64_t*)phys_to_virt(hhdm_pdpt_phys);
    memset(hhdm_pdpt_virt, 0, PAGE_SIZE);

    for (size_t g = 0; g < 4; g++) {
        uint64_t hhdm_pd_phys = (uint64_t)pmm_alloc_frame();
        uint64_t* hhdm_pd_virt = (uint64_t*)phys_to_virt(hhdm_pd_phys);
        for (size_t i = 0; i < 512; i++) {
            uint64_t pa = (g * 0x40000000ULL) + (i * 0x200000ULL);
            hhdm_pd_virt[i] = pa | VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | (1ULL << 7) /* Huge Page */ | VMM_FLAG_NO_EXECUTE;
        }
        hhdm_pdpt_virt[g] = hhdm_pd_phys | VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE;
    }

    kernel_pml4_virt[256] = hhdm_pdpt_phys | VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE;

    // 2. Map Kernel Sections (Higher-Half at 0xFFFFFFFF80000000) with strict W^X permissions:
    // .text:   Present, Read-Only, Executable (NO_EXECUTE = 0)
    // .rodata: Present, Read-Only, No-Execute (NO_EXECUTE = 1)
    // .data:   Present, Writable, No-Execute (NO_EXECUTE = 1)
    // .bss:    Present, Writable, No-Execute (NO_EXECUTE = 1)

    uint64_t text_s = (uint64_t)text_start & ~0xFFFULL;
    uint64_t text_e = ((uint64_t)text_end + PAGE_SIZE - 1) & ~0xFFFULL;
    for (uint64_t va = text_s; va < text_e; va += PAGE_SIZE) {
        uint64_t pa = va - KERNEL_VIRT_OFFSET;
        vmm_map_page(kernel_pml4_virt, va, pa, VMM_FLAG_PRESENT);
    }

    uint64_t ro_s = (uint64_t)rodata_start & ~0xFFFULL;
    uint64_t ro_e = ((uint64_t)rodata_end + PAGE_SIZE - 1) & ~0xFFFULL;
    for (uint64_t va = ro_s; va < ro_e; va += PAGE_SIZE) {
        uint64_t pa = va - KERNEL_VIRT_OFFSET;
        vmm_map_page(kernel_pml4_virt, va, pa, VMM_FLAG_PRESENT | VMM_FLAG_NO_EXECUTE);
    }

    uint64_t data_s = (uint64_t)data_start & ~0xFFFULL;
    uint64_t data_e = ((uint64_t)data_end + PAGE_SIZE - 1) & ~0xFFFULL;
    for (uint64_t va = data_s; va < data_e; va += PAGE_SIZE) {
        uint64_t pa = va - KERNEL_VIRT_OFFSET;
        vmm_map_page(kernel_pml4_virt, va, pa, VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_NO_EXECUTE);
    }

    uint64_t bss_s = (uint64_t)bss_start & ~0xFFFULL;
    uint64_t bss_e = ((uint64_t)bss_end + PAGE_SIZE - 1) & ~0xFFFULL;
    for (uint64_t va = bss_s; va < bss_e; va += PAGE_SIZE) {
        uint64_t pa = va - KERNEL_VIRT_OFFSET;
        vmm_map_page(kernel_pml4_virt, va, pa, VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_NO_EXECUTE);
    }

    // Switch to permanent, protected kernel PML4
    vmm_switch_pml4(kernel_pml4_phys);

    serial_puts("[+] VMM: Higher-Half paging active. Strict W^X protection enforced on kernel sections.\n");
}
#endif
