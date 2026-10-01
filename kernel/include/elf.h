/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * ELF-64 Executable and Linkable Format Specification & Loader Interface
 * 64-bit x86_64 ELF loader and segment mapping.
 * GPLv3 Licensed.
 */

#ifndef SELD_ELF_H
#define SELD_ELF_H

#include <stdint.h>
#include <stddef.h>

/* ELF Identification Indices */
#define EI_MAG0       0
#define EI_MAG1       1
#define EI_MAG2       2
#define EI_MAG3       3
#define EI_CLASS      4
#define EI_DATA       5
#define EI_VERSION    6
#define EI_OSABI      7
#define EI_ABIVERSION 8
#define EI_PAD        9
#define EI_NIDENT     16

/* ELF Magic */
#define ELFMAG0 0x7F
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

/* ELF Classes */
#define ELFCLASSNONE 0
#define ELFCLASS32   1
#define ELFCLASS64   2

/* ELF Data Encodings */
#define ELFDATANONE 0
#define ELFDATA2LSB 1 /* Little-endian (2's complement) */
#define ELFDATA2MSB 2 /* Big-endian */

/* ELF Versions */
#define EV_NONE    0
#define EV_CURRENT 1

/* ELF Object File Types */
#define ET_NONE 0
#define ET_REL  1
#define ET_EXEC 2
#define ET_DYN  3
#define ET_CORE 4

/* ELF Machine Architecture */
#define EM_NONE   0
#define EM_386    3
#define EM_X86_64 62

/* Program Header Types */
#define PT_NULL         0
#define PT_LOAD         1
#define PT_DYNAMIC      2
#define PT_INTERP       3
#define PT_NOTE         4
#define PT_SHLIB        5
#define PT_PHDR         6
#define PT_TLS          7
#define PT_GNU_STACK    0x6474E551

/* Program Header Flags */
#define PF_X 0x1 /* Execute */
#define PF_W 0x2 /* Write */
#define PF_R 0x4 /* Read */

/* 64-bit ELF Types */
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t  Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t  Elf64_Sxword;

/* ELF-64 File Header */
typedef struct {
    unsigned char e_ident[EI_NIDENT]; /* ELF identification bytes */
    Elf64_Half    e_type;             /* Object file type */
    Elf64_Half    e_machine;          /* Architecture */
    Elf64_Word    e_version;          /* Object file version */
    Elf64_Addr    e_entry;            /* Entry point virtual address */
    Elf64_Off     e_phoff;            /* Program header table file offset */
    Elf64_Off     e_shoff;            /* Section header table file offset */
    Elf64_Word    e_flags;            /* Processor-specific flags */
    Elf64_Half    e_ehsize;           /* ELF header size in bytes */
    Elf64_Half    e_phentsize;        /* Program header table entry size */
    Elf64_Half    e_phnum;            /* Program header table entry count */
    Elf64_Half    e_shentsize;        /* Section header table entry size */
    Elf64_Half    e_shnum;            /* Section header table entry count */
    Elf64_Half    e_shstrndx;         /* Section header string table index */
} __attribute__((packed)) Elf64_Ehdr;

/* ELF-64 Program Header */
typedef struct {
    Elf64_Word  p_type;   /* Segment type */
    Elf64_Word  p_flags;  /* Segment flags */
    Elf64_Off   p_offset; /* Segment file offset */
    Elf64_Addr  p_vaddr;  /* Segment virtual address */
    Elf64_Addr  p_paddr;  /* Segment physical address */
    Elf64_Xword p_filesz; /* Segment size in file */
    Elf64_Xword p_memsz;  /* Segment size in memory */
    Elf64_Xword p_align;  /* Segment alignment */
} __attribute__((packed)) Elf64_Phdr;

/* Function prototypes */
int elf_validate_header(const Elf64_Ehdr* ehdr, size_t data_len);
int elf_load_binary(const void* elf_data, size_t data_len, uint64_t** out_pml4_virt, uint64_t* out_entry, uint64_t* out_pml4_phys);

#endif /* SELD_ELF_H */
