/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * SeldFS - Hardened Ring 0 Simple Block File System Interface
 * Flat inode structure with magic signature, direct block mapping, and atomic flush.
 * GPLv3 Licensed.
 */

#ifndef SELD_FS_H
#define SELD_FS_H

#include <stdint.h>
#include <stddef.h>

#define SELDFS_MAGIC 0x53454C44 // "SELD" in ASCII
#define SELDFS_MAX_FILES 32
#define SELDFS_MAX_FILENAME 32
#define SELDFS_BLOCK_SIZE 512
#define SELDFS_DATA_BLOCKS_PER_FILE 32768 // 16 MiB max per file
#define SELDFS_TOTAL_DATA_BLOCKS    65536 // 32 MiB data capacity
#define SELDFS_BITMAP_SECTORS       16    // 16 * 512 = 8192 bytes = 65536 bits

#define SELD_TYPE_FILE 1
#define SELD_TYPE_DIR  2

struct seldfs_inode {
    uint32_t used;
    uint32_t type; // SELD_TYPE_FILE (1) or SELD_TYPE_DIR (2)
    char     filename[SELDFS_MAX_FILENAME];
    uint32_t size;
    uint32_t start_lba;
    uint32_t block_count;
    union {
        uint8_t sha256_hash[32];
        uint8_t sha256[32];
    };
};

struct seldfs_superblock {
    uint32_t magic;
    uint32_t version;
    uint32_t block_size;
    uint32_t total_inodes;
    uint32_t free_data_lba;
    struct seldfs_inode inodes[SELDFS_MAX_FILES];
};

void seldfs_init(void);
int seldfs_format(void);
int seldfs_list_files(void (*callback)(const char* name, uint32_t size));
int seldfs_read_file(const char* filename, void* buf, size_t max_len, size_t* out_len);
int seldfs_read_file_offset(const char* filename, uint32_t offset, void* buf, size_t len, size_t* out_read);
int seldfs_write_file(const char* filename, const void* data, size_t len);
int seldfs_delete_file(const char* filename);
int seldfs_verify_file(const char* filename);
int seldfs_get_file_info(const char* filename, struct seldfs_inode* out_inode);
int seldfs_find_file(const char* filename, struct seldfs_inode* out_inode);
int seldfs_get_inode_by_index(size_t index, struct seldfs_inode* out_inode);
int seldfs_get_all_inodes(struct seldfs_inode* out_inodes, size_t max_count);
int seldfs_get_file_count(void);
void seldfs_get_bitmap_stats(uint32_t* total_blocks, uint32_t* used_blocks, uint32_t* free_blocks);
int seldfs_is_authorized_file(const char* filename);
int seldfs_purge_untrusted(void);

#endif /* SELD_FS_H */
