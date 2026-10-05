/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * ELF-64 Binary Loader & Process Address Space Initializer
 * Parses 64-bit ELF executables, establishes user PML4 page tables,
 * enforces W^X permissions on segments, zeroes BSS, and maps user stack.
 * GPLv3 Licensed.
 */

#include "elf.h"
#include "vmm.h"
#include "pmm.h"
#include "string.h"
#include "serial.h"

int elf_validate_header(const Elf64_Ehdr* ehdr, size_t data_len) {
    if (!ehdr || data_len < sizeof(Elf64_Ehdr)) {
        return -1;
    }

    /* 1. Magic: \x7fELF */
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
        ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
        ehdr->e_ident[EI_MAG3] != ELFMAG3) {
        return -2;
    }

    /* 2. 64-bit architecture */
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        return -3;
    }

    /* 3. Little-endian encoding */
    if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        return -4;
    }

    /* 4. Machine type x86_64 */
    if (ehdr->e_machine != EM_X86_64) {
        return -5;
    }

    /* 5. Executable object (or dynamic PIE) */
    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
        return -6;
    }

    /* 6. Program header bounds validation */
    if (ehdr->e_phentsize < sizeof(Elf64_Phdr)) {
        return -7;
    }

    if (ehdr->e_phoff + (uint64_t)ehdr->e_phnum * ehdr->e_phentsize > data_len) {
        return -8;
    }

    return 0;
}

int elf_load_binary(const void* elf_data, size_t data_len, uint64_t** out_pml4_virt, uint64_t* out_entry, uint64_t* out_pml4_phys) {
    if (!elf_data || data_len < sizeof(Elf64_Ehdr) || !out_pml4_virt || !out_entry || !out_pml4_phys) {
        return -1;
    }

    const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)elf_data;
    int err = elf_validate_header(ehdr, data_len);
    if (err != 0) {
        serial_puts("[-] ELF: Header validation failed (code ");
        serial_print_dec((uint64_t)-err);
        serial_puts(")\n");
        return err;
    }

    /* 1. Allocate a new user PML4 using vmm_create_address_space() */
    uint64_t* user_pml4_virt = vmm_create_address_space();
    if (!user_pml4_virt) {
        serial_puts("[-] ELF: Out of memory creating address space.\n");
        return -9;
    }
    uint64_t user_pml4_phys = virt_to_phys(user_pml4_virt);

    /* 2. Allocate and map user stack at virtual 0x00007FFFFFFFE000ULL (64 KiB total stack) */
    /* Mapped from 0x00007FFFFFFF0000 to 0x00007FFFFFFFF000 (RW, NX, User) */
    for (uint64_t va = 0x00007FFFFFFF0000ULL; va < 0x00007FFFFFFFF000ULL; va += PAGE_SIZE) {
        void* stack_frame = pmm_alloc_frame();
        if (!stack_frame) {
            serial_puts("[-] ELF: Failed to allocate physical frame for user stack.\n");
            vmm_destroy_address_space(user_pml4_virt);
            return -10;
        }
        memset(phys_to_virt((uint64_t)stack_frame), 0, PAGE_SIZE);
        vmm_map_page(user_pml4_virt, va, (uint64_t)stack_frame,
                     VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_USER | VMM_FLAG_NO_EXECUTE);
    }

    /* 3. Parse PT_LOAD segments, allocate physical frames, copy data, zero BSS, and map with strict permissions */
    const uint8_t* elf_bytes = (const uint8_t*)elf_data;
    int load_segments = 0;

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr* ph = (const Elf64_Phdr*)(elf_bytes + ehdr->e_phoff + (i * ehdr->e_phentsize));

        if (ph->p_type != PT_LOAD) {
            continue;
        }

        if (ph->p_memsz == 0) {
            continue;
        }

        if (ph->p_offset + ph->p_filesz > data_len) {
            serial_puts("[-] ELF: Segment offset + filesz exceeds binary length.\n");
            vmm_destroy_address_space(user_pml4_virt);
            return -11;
        }

        if (ph->p_filesz > ph->p_memsz) {
            serial_puts("[-] ELF: Segment filesz exceeds memsz.\n");
            vmm_destroy_address_space(user_pml4_virt);
            return -12;
        }

        /* Verify canonical lower half userspace bounds (< 0x0000800000000000) */
        if (ph->p_vaddr >= 0x0000800000000000ULL || ph->p_vaddr + ph->p_memsz > 0x0000800000000000ULL) {
            serial_puts("[-] ELF: Segment virtual address out of userspace bounds.\n");
            vmm_destroy_address_space(user_pml4_virt);
            return -13;
        }

        /* Determine page permissions according to W^X OpSec principle:
         * - PF_X without PF_W: Read-Only + Executable (VMM_FLAG_PRESENT | VMM_FLAG_USER)
         * - PF_W: Read-Write + No-Execute (VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_USER | VMM_FLAG_NO_EXECUTE)
         * - neither: Read-Only + No-Execute (VMM_FLAG_PRESENT | VMM_FLAG_USER | VMM_FLAG_NO_EXECUTE)
         */
        uint64_t flags = VMM_FLAG_PRESENT | VMM_FLAG_USER;
        if (ph->p_flags & PF_W) {
            flags |= VMM_FLAG_WRITABLE | VMM_FLAG_NO_EXECUTE;
        } else if (!(ph->p_flags & PF_X)) {
            flags |= VMM_FLAG_NO_EXECUTE;
        }

        uint64_t page_start = ph->p_vaddr & ~0xFFFULL;
        uint64_t page_end   = (ph->p_vaddr + ph->p_memsz + PAGE_SIZE - 1) & ~0xFFFULL;

        for (uint64_t va = page_start; va < page_end; va += PAGE_SIZE) {
            uint64_t existing_pa = vmm_get_mapping(user_pml4_virt, va);
            void* frame = NULL;
            void* frame_virt = NULL;

            if (existing_pa) {
                frame = (void*)existing_pa;
                frame_virt = phys_to_virt(existing_pa);
            } else {
                frame = pmm_alloc_frame();
                if (!frame) {
                    serial_puts("[-] ELF: Failed to allocate frame for code/data segment.\n");
                    vmm_destroy_address_space(user_pml4_virt);
                    return -14;
                }
                frame_virt = phys_to_virt((uint64_t)frame);
                memset(frame_virt, 0, PAGE_SIZE);
            }

            /* Copy file data for this page if it overlaps with [p_vaddr, p_vaddr + p_filesz) */
            uint64_t file_start = ph->p_vaddr;
            uint64_t file_end   = ph->p_vaddr + ph->p_filesz;

            uint64_t copy_start = (va > file_start) ? va : file_start;
            uint64_t copy_end   = (va + PAGE_SIZE < file_end) ? (va + PAGE_SIZE) : file_end;

            if (copy_end > copy_start) {
                size_t copy_len = (size_t)(copy_end - copy_start);
                size_t page_offset = (size_t)(copy_start - va);
                size_t file_offset = (size_t)(ph->p_offset + (copy_start - ph->p_vaddr));
                memcpy((uint8_t*)frame_virt + page_offset, elf_bytes + file_offset, copy_len);
            }

            /* Note: BSS is automatically zeroed because the newly allocated frame was zeroed with memset. */
            vmm_map_page(user_pml4_virt, va, (uint64_t)frame, flags);
        }

        load_segments++;
    }

    if (load_segments == 0) {
        serial_puts("[-] ELF: No loadable PT_LOAD segments found.\n");
        vmm_destroy_address_space(user_pml4_virt);
        return -15;
    }

    *out_pml4_virt = user_pml4_virt;
    *out_entry     = ehdr->e_entry;
    *out_pml4_phys = user_pml4_phys;

    serial_puts("[+] ELF: Process loaded successfully. Entry: ");
    serial_print_hex(ehdr->e_entry);
    serial_puts(", PML4: ");
    serial_print_hex(user_pml4_phys);
    serial_puts("\n");

    return 0;
}
