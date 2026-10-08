/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * SeldFS - Hardened Ring 0 Simple Block File System Implementation
 * Flat inode structure with Sector 1 Block Allocation Bitmap and SHA-256 verification.
 * GPLv3 Licensed.
 */

#include "seldfs.h"
#include "ata.h"
#include "ramdisk.h"
#include "string.h"
#include "serial.h"
#include "sha256.h"

#define SB_START_LBA         2048 // Start after first 1 MiB (for MBR/bootloader safety)
#define SB_SECTOR_LBA        (SB_START_LBA + 0)   // Sector 0: Superblock
#define BITMAP_SECTOR_LBA    (SB_START_LBA + 1)   // Sector 1..16: Block Allocation Bitmap (16 sectors)
#define INODES_START_LBA     (SB_START_LBA + 1 + SELDFS_BITMAP_SECTORS)   // Sector 17..32: Inode Table (16 sectors)
#define INODES_SECTOR_COUNT  16
#define DATA_START_LBA       (INODES_START_LBA + INODES_SECTOR_COUNT)     // Sector 33+: Data Blocks

static struct seldfs_superblock sb;
static uint8_t block_bitmap[SELDFS_BITMAP_SECTORS * SELDFS_BLOCK_SIZE];
static int fs_mounted = 0;

static int bdev_read_sectors(uint32_t lba, uint32_t count, void* buffer) {
    struct ramdisk_info* rd = ramdisk_get_info();
    if (rd && rd->is_present) {
        return ramdisk_read_sectors(lba, count, buffer);
    }
    return ata_read_sectors(lba, count, buffer);
}

static int bdev_write_sectors(uint32_t lba, uint32_t count, const void* buffer) {
    struct ramdisk_info* rd = ramdisk_get_info();
    if (rd && rd->is_present) {
        return ramdisk_write_sectors(lba, count, buffer);
    }
    return ata_write_sectors(lba, count, buffer);
}

static inline int bitmap_get_bit(uint32_t block_idx) {
    if (block_idx >= SELDFS_TOTAL_DATA_BLOCKS) return 1;
    return (block_bitmap[block_idx / 8] >> (block_idx % 8)) & 1;
}

static inline void bitmap_set_bit(uint32_t block_idx) {
    if (block_idx < SELDFS_TOTAL_DATA_BLOCKS) {
        block_bitmap[block_idx / 8] |= (uint8_t)(1 << (block_idx % 8));
    }
}

static inline void bitmap_clear_bit(uint32_t block_idx) {
    if (block_idx < SELDFS_TOTAL_DATA_BLOCKS) {
        block_bitmap[block_idx / 8] &= (uint8_t)~(1 << (block_idx % 8));
    }
}

static int flush_bitmap(void) {
    return bdev_write_sectors(BITMAP_SECTOR_LBA, SELDFS_BITMAP_SECTORS, block_bitmap);
}

static int flush_inodes(void) {
    uint8_t buffer[INODES_SECTOR_COUNT * ATA_SECTOR_SIZE];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, sb.inodes, sizeof(sb.inodes));
    return bdev_write_sectors(INODES_START_LBA, INODES_SECTOR_COUNT, buffer);
}

static int flush_all_metadata(void) {
    uint8_t sb_buf[ATA_SECTOR_SIZE];
    memset(sb_buf, 0, sizeof(sb_buf));
    memcpy(sb_buf, &sb, 20); // First 20 bytes: magic, version, block_size, total_inodes, free_data_lba
    if (bdev_write_sectors(SB_SECTOR_LBA, 1, sb_buf) != 0) {
        return -1;
    }
    if (flush_bitmap() != 0) {
        return -1;
    }
    if (flush_inodes() != 0) {
        return -1;
    }
    return 0;
}

static int allocate_blocks(uint32_t count, uint32_t* out_lba) {
    if (count == 0 || count > SELDFS_DATA_BLOCKS_PER_FILE) {
        return -1;
    }

    uint32_t run = 0;
    uint32_t start_idx = 0;

    for (uint32_t i = 0; i < SELDFS_TOTAL_DATA_BLOCKS; i++) {
        if (!bitmap_get_bit(i)) {
            if (run == 0) {
                start_idx = i;
            }
            run++;
            if (run == count) {
                // Found contiguous range
                for (uint32_t b = 0; b < count; b++) {
                    bitmap_set_bit(start_idx + b);
                }
                *out_lba = DATA_START_LBA + start_idx;
                return 0;
            }
        } else {
            run = 0;
        }
    }

    return -1; // Out of free data blocks
}

static void free_blocks(uint32_t start_lba, uint32_t count) {
    if (start_lba < DATA_START_LBA) {
        return;
    }
    uint32_t start_idx = start_lba - DATA_START_LBA;
    for (uint32_t b = 0; b < count; b++) {
        if (start_idx + b < SELDFS_TOTAL_DATA_BLOCKS) {
            bitmap_clear_bit(start_idx + b);
        }
    }
}

void seldfs_init(void) {
    fs_mounted = 0;
    struct ramdisk_info* rd = ramdisk_get_info();
    if (rd && rd->is_present) {
        serial_puts("[+] SeldFS: Using High-Speed In-Memory Ramdisk Storage (");
        serial_print_dec((uint32_t)(rd->size / (1024 * 1024)));
        serial_puts(" MiB)\n");
    } else {
        struct ata_device_info dev = ata_get_primary_master();
        if (!dev.present) {
            serial_puts("[-] SeldFS: No storage medium detected (neither Ramdisk nor ATA disk)\n");
            return;
        }
    }

    uint8_t sb_buf[ATA_SECTOR_SIZE];
    if (bdev_read_sectors(SB_SECTOR_LBA, 1, sb_buf) != 0) {
        serial_puts("[-] SeldFS: Failed to read superblock from storage.\n");
        return;
    }

    memcpy(&sb, sb_buf, 20);

    if (sb.magic == SELDFS_MAGIC || sb.magic == 0x5E1DF501) {
        if (sb.version >= 2) {
            // Read Block Allocation Bitmap (SELDFS_BITMAP_SECTORS sectors)
            if (bdev_read_sectors(BITMAP_SECTOR_LBA, SELDFS_BITMAP_SECTORS, block_bitmap) != 0) {
                serial_puts("[-] SeldFS: Failed to read bitmap from disk.\n");
                return;
            }
            // Read Sectors 2..17: Inode Table
            uint8_t inodes_buf[INODES_SECTOR_COUNT * ATA_SECTOR_SIZE];
            if (bdev_read_sectors(INODES_START_LBA, INODES_SECTOR_COUNT, inodes_buf) != 0) {
                serial_puts("[-] SeldFS: Failed to read inode table from disk.\n");
                return;
            }
            memcpy(sb.inodes, inodes_buf, sizeof(sb.inodes));
        } else {
            // Legacy version 1 format: read contiguous sectors
            uint8_t legacy_buf[6 * ATA_SECTOR_SIZE];
            if (bdev_read_sectors(SB_START_LBA, 6, legacy_buf) == 0) {
                memcpy(&sb, legacy_buf, sizeof(struct seldfs_superblock));
                // Reconstruct bitmap from existing inodes
                memset(block_bitmap, 0, sizeof(block_bitmap));
                for (size_t i = 0; i < SELDFS_MAX_FILES; i++) {
                    if (sb.inodes[i].used && sb.inodes[i].start_lba >= DATA_START_LBA) {
                        uint32_t blk = sb.inodes[i].start_lba - DATA_START_LBA;
                        for (uint32_t b = 0; b < sb.inodes[i].block_count; b++) {
                            bitmap_set_bit(blk + b);
                        }
                    }
                }
            }
        }

        fs_mounted = 1;
        serial_puts("[+] SeldFS: Mounted successfully (Version: ");
        serial_print_dec(sb.version);
        serial_puts(", Bitmap Sector: 1, Block Size: ");
        serial_print_dec(sb.block_size);
        serial_puts(")\n");
    } else {
        serial_puts("[!] SeldFS: Superblock not found. Drive requires formatting (use 'mkfs').\n");
    }
}

int seldfs_format(void) {
    struct ramdisk_info* rd = ramdisk_get_info();
    if (!rd || !rd->is_present) {
        struct ata_device_info dev = ata_get_primary_master();
        if (!dev.present) {
            return -1;
        }
    }

    memset(&sb, 0, sizeof(struct seldfs_superblock));
    sb.magic = SELDFS_MAGIC;
    sb.version = 2; // Version 2: True Block Allocation Bitmap in Sector 1
    sb.block_size = SELDFS_BLOCK_SIZE;
    sb.total_inodes = SELDFS_MAX_FILES;
    sb.free_data_lba = DATA_START_LBA;

    // Clear bitmap
    memset(block_bitmap, 0, sizeof(block_bitmap));

    // Flush all metadata: Superblock (Sector 0), Bitmap (Sector 1), Inodes (Sectors 2..17)
    if (flush_all_metadata() != 0) {
        return -1;
    }

    fs_mounted = 1;
    serial_puts("[+] SeldFS: File system formatted with Sector 1 bitmap and mounted successfully.\n");
    return 0;
}

int seldfs_list_files(void (*callback)(const char* name, uint32_t size)) {
    if (!fs_mounted) return -1;
    int count = 0;
    for (size_t i = 0; i < SELDFS_MAX_FILES; i++) {
        if (sb.inodes[i].used) {
            callback(sb.inodes[i].filename, sb.inodes[i].size);
            count++;
        }
    }
    return count;
}

int seldfs_read_file(const char* filename, void* buf, size_t max_len, size_t* out_len) {
    if (!fs_mounted || !filename || !buf) return -1;

    for (size_t i = 0; i < SELDFS_MAX_FILES; i++) {
        if (sb.inodes[i].used && strcmp(sb.inodes[i].filename, filename) == 0) {
            uint32_t to_read = sb.inodes[i].size;
            if (to_read > max_len) to_read = (uint32_t)max_len;

            uint8_t sector_buf[SELDFS_BLOCK_SIZE];
            uint8_t* dest = (uint8_t*)buf;
            uint32_t remaining = to_read;

            for (uint32_t b = 0; b < sb.inodes[i].block_count && remaining > 0; b++) {
                if (bdev_read_sectors(sb.inodes[i].start_lba + b, 1, sector_buf) != 0) {
                    return -1;
                }
                uint32_t chunk = (remaining > SELDFS_BLOCK_SIZE) ? SELDFS_BLOCK_SIZE : remaining;
                memcpy(dest, sector_buf, chunk);
                dest += chunk;
                remaining -= chunk;
            }

            if (out_len) *out_len = to_read;

            // Verify integrity of read data if entire file was read
            if (to_read == sb.inodes[i].size) {
                uint8_t read_hash[32];
                sha256_hash(buf, to_read, read_hash);
                if (memcmp(read_hash, sb.inodes[i].sha256, 32) != 0) {
                    serial_puts("[-] SeldFS: Integrity verification warning: file '");
                    serial_puts(filename);
                    serial_puts("' SHA-256 hash mismatch!\n");
                }
            }

            return 0;
        }
    }
    return -1; // File not found
}

int seldfs_read_file_offset(const char* filename, uint32_t offset, void* buf, size_t len, size_t* out_read) {
    if (!fs_mounted || !filename || !buf || len == 0) return -1;

    for (size_t i = 0; i < SELDFS_MAX_FILES; i++) {
        if (sb.inodes[i].used && strcmp(sb.inodes[i].filename, filename) == 0) {
            uint32_t fsize = sb.inodes[i].size;
            if (offset >= fsize) {
                if (out_read) *out_read = 0;
                return 0; // EOF
            }

            uint32_t to_read = (uint32_t)len;
            if (offset + to_read > fsize) {
                to_read = fsize - offset;
            }

            uint8_t sector_buf[SELDFS_BLOCK_SIZE];
            uint8_t* dest = (uint8_t*)buf;
            uint32_t remaining = to_read;
            uint32_t cur_offset = offset;

            while (remaining > 0) {
                uint32_t block_index = cur_offset / SELDFS_BLOCK_SIZE;
                uint32_t block_offset = cur_offset % SELDFS_BLOCK_SIZE;
                uint32_t chunk = SELDFS_BLOCK_SIZE - block_offset;
                if (chunk > remaining) chunk = remaining;

                if (block_index >= sb.inodes[i].block_count) {
                    break;
                }

                if (bdev_read_sectors(sb.inodes[i].start_lba + block_index, 1, sector_buf) != 0) {
                    return -1;
                }

                memcpy(dest, sector_buf + block_offset, chunk);
                dest += chunk;
                cur_offset += chunk;
                remaining -= chunk;
            }

            if (out_read) *out_read = (to_read - remaining);
            return 0;
        }
    }
    return -1; // File not found
}

int seldfs_write_file(const char* filename, const void* data, size_t len) {
    if (!fs_mounted || !filename || (!data && len > 0)) return -1;
    if (len > SELDFS_DATA_BLOCKS_PER_FILE * SELDFS_BLOCK_SIZE) {
        return -1; // Exceeds file limit
    }

    int target_inode = -1;
    // Check if file already exists
    for (size_t i = 0; i < SELDFS_MAX_FILES; i++) {
        if (sb.inodes[i].used && strcmp(sb.inodes[i].filename, filename) == 0) {
            target_inode = (int)i;
            break;
        }
    }

    uint32_t blocks = (len + SELDFS_BLOCK_SIZE - 1) / SELDFS_BLOCK_SIZE;
    if (blocks == 0) blocks = 1;

    uint32_t start_lba = 0;

    if (target_inode != -1) {
        // File exists: check if existing allocation can be reused
        if (blocks <= sb.inodes[target_inode].block_count) {
            start_lba = sb.inodes[target_inode].start_lba;
            if (blocks < sb.inodes[target_inode].block_count) {
                // Free unused trailing blocks
                free_blocks(start_lba + blocks, sb.inodes[target_inode].block_count - blocks);
            }
        } else {
            // Need larger allocation: free old blocks, allocate new
            free_blocks(sb.inodes[target_inode].start_lba, sb.inodes[target_inode].block_count);
            if (allocate_blocks(blocks, &start_lba) != 0) {
                // Restore old allocation if possible
                allocate_blocks(sb.inodes[target_inode].block_count, &start_lba);
                return -1; // Out of disk space
            }
        }
    } else {
        // Find free inode slot
        for (size_t i = 0; i < SELDFS_MAX_FILES; i++) {
            if (!sb.inodes[i].used) {
                target_inode = (int)i;
                break;
            }
        }
        if (target_inode == -1) {
            return -1; // Inodes exhausted
        }

        // Allocate blocks from bitmap
        if (allocate_blocks(blocks, &start_lba) != 0) {
            return -1; // Out of disk space
        }
    }

    // Compute cryptographic SHA-256 digest
    sha256_hash(data, len, sb.inodes[target_inode].sha256);

    sb.inodes[target_inode].used = 1;
    sb.inodes[target_inode].type = SELD_TYPE_FILE;
    sb.inodes[target_inode].size = (uint32_t)len;
    sb.inodes[target_inode].start_lba = start_lba;
    sb.inodes[target_inode].block_count = blocks;

    size_t name_len = strlen(filename);
    if (name_len >= SELDFS_MAX_FILENAME) name_len = SELDFS_MAX_FILENAME - 1;
    memset(sb.inodes[target_inode].filename, 0, SELDFS_MAX_FILENAME);
    memcpy(sb.inodes[target_inode].filename, filename, name_len);

    // Write file sectors
    const uint8_t* src = (const uint8_t*)data;
    uint32_t remaining = (uint32_t)len;
    uint8_t sector_buf[SELDFS_BLOCK_SIZE];

    for (uint32_t b = 0; b < blocks; b++) {
        memset(sector_buf, 0, SELDFS_BLOCK_SIZE);
        uint32_t chunk = (remaining > SELDFS_BLOCK_SIZE) ? SELDFS_BLOCK_SIZE : remaining;
        if (chunk > 0 && src != NULL) {
            memcpy(sector_buf, src, chunk);
            src += chunk;
            remaining -= chunk;
        }
        if (bdev_write_sectors(start_lba + b, 1, sector_buf) != 0) {
            return -1;
        }
    }

    return flush_all_metadata();
}

int seldfs_delete_file(const char* filename) {
    if (!fs_mounted || !filename) return -1;

    for (size_t i = 0; i < SELDFS_MAX_FILES; i++) {
        if (sb.inodes[i].used && strcmp(sb.inodes[i].filename, filename) == 0) {
            // Free allocated data blocks in bitmap
            if (sb.inodes[i].block_count > 0 && sb.inodes[i].start_lba >= DATA_START_LBA) {
                free_blocks(sb.inodes[i].start_lba, sb.inodes[i].block_count);
            }

            sb.inodes[i].used = 0;
            memset(sb.inodes[i].filename, 0, sizeof(sb.inodes[i].filename));
            sb.inodes[i].size = 0;
            sb.inodes[i].start_lba = 0;
            sb.inodes[i].block_count = 0;
            memset(sb.inodes[i].sha256, 0, sizeof(sb.inodes[i].sha256));

            return flush_all_metadata();
        }
    }
    return -1; // File not found
}

int seldfs_is_authorized_file(const char* filename) {
    if (!filename) return 0;
    const char* base = filename;
    if (strncmp(base, "/bin/", 5) == 0) base += 5;
    else if (base[0] == '/') base += 1;

    static const char* const authorized[] = {
        "init", "sh", "ls", "cat", "echo", "rm", "sha256sum",
        "reboot", "poweroff", "fetch", "fm", "oracle", "ps",
        "uname", "download", "tor", "torbrowser", "doom", "purge", "stealth",
        "diode",
        "readme.txt", "opsec.txt", "oracle.txt", "doom1.wad", "pcmode",
        "secret.txt", "airgap.key",
        NULL
    };
    for (int i = 0; authorized[i] != NULL; i++) {
        if (strcmp(base, authorized[i]) == 0) return 1;
    }
    return 0;
}

int seldfs_verify_file(const char* filename) {
    if (!fs_mounted || !filename) return -1;

    size_t target_idx = (size_t)-1;
    for (size_t i = 0; i < SELDFS_MAX_FILES; i++) {
        if (!sb.inodes[i].used) continue;
        if (strcmp(sb.inodes[i].filename, filename) == 0) {
            target_idx = i;
            break;
        }
    }
    if (target_idx == (size_t)-1) {
        const char* base = filename;
        if (strncmp(base, "/bin/", 5) == 0) base += 5;
        else if (base[0] == '/') base += 1;

        for (size_t i = 0; i < SELDFS_MAX_FILES; i++) {
            if (!sb.inodes[i].used) continue;
            const char* in_base = sb.inodes[i].filename;
            if (strncmp(in_base, "/bin/", 5) == 0) in_base += 5;
            else if (in_base[0] == '/') in_base += 1;
            if (strcmp(in_base, base) == 0) {
                target_idx = i;
                break;
            }
        }
    }
    if (target_idx == (size_t)-1) return -1; // File not found

    uint32_t file_size = sb.inodes[target_idx].size;
    uint32_t remaining = file_size;
    uint8_t sector_buf[SELDFS_BLOCK_SIZE];
    struct sha256_ctx ctx;
    sha256_init(&ctx);

    for (uint32_t b = 0; b < sb.inodes[target_idx].block_count && remaining > 0; b++) {
        if (bdev_read_sectors(sb.inodes[target_idx].start_lba + b, 1, sector_buf) != 0) {
            serial_puts("[-] SeldFS: I/O read failure during integrity verification.\n");
            return -1;
        }
        uint32_t chunk = (remaining > SELDFS_BLOCK_SIZE) ? SELDFS_BLOCK_SIZE : remaining;
        sha256_update(&ctx, sector_buf, chunk);
        remaining -= chunk;
    }

    uint8_t computed_hash[32];
    sha256_final(&ctx, computed_hash);

    if (memcmp(computed_hash, sb.inodes[target_idx].sha256, 32) == 0) {
        return 0; // Verified OK
    } else {
        serial_puts("[-] SeldFS: SHA-256 integrity check FAILED for: ");
        serial_puts(filename);
        serial_puts(" (hash mismatch)\n");
        return -2; // Hash mismatch
    }
}

int seldfs_purge_untrusted(void) {
    if (!fs_mounted) return 0;
    int purged = 0;
    uint8_t zero_block[SELDFS_BLOCK_SIZE];
    memset(zero_block, 0, sizeof(zero_block));

    for (size_t i = 0; i < SELDFS_MAX_FILES; i++) {
        if (!sb.inodes[i].used) continue;

        int is_auth = seldfs_is_authorized_file(sb.inodes[i].filename);
        int verify_res = seldfs_verify_file(sb.inodes[i].filename);

        if (!is_auth || verify_res != 0) {
            serial_puts("[!] OpSec FS Purge: Removing rogue/tampered file '");
            serial_puts(sb.inodes[i].filename);
            serial_puts("'\n");

            for (uint32_t b = 0; b < sb.inodes[i].block_count; b++) {
                bdev_write_sectors(sb.inodes[i].start_lba + b, 1, zero_block);
            }

            seldfs_delete_file(sb.inodes[i].filename);
            purged++;
        }
    }
    return purged;
}

int seldfs_get_file_info(const char* filename, struct seldfs_inode* out_inode) {
    if (!fs_mounted || !filename || !out_inode) return -1;
    for (size_t i = 0; i < SELDFS_MAX_FILES; i++) {
        if (sb.inodes[i].used && strcmp(sb.inodes[i].filename, filename) == 0) {
            *out_inode = sb.inodes[i];
            return 0;
        }
    }
    return -1;
}

int seldfs_find_file(const char* filename, struct seldfs_inode* out_inode) {
    if (!fs_mounted || !filename || !out_inode) return -1;
    if (seldfs_get_file_info(filename, out_inode) == 0) return 0;

    if (strncmp(filename, "/bin/", 5) == 0) {
        if (seldfs_get_file_info(filename + 5, out_inode) == 0) return 0;
    }
    if (filename[0] == '/') {
        if (seldfs_get_file_info(filename + 1, out_inode) == 0) return 0;
    } else {
        char alt[SELDFS_MAX_FILENAME];
        alt[0] = '/'; alt[1] = 'b'; alt[2] = 'i'; alt[3] = 'n'; alt[4] = '/';
        strncpy(alt + 5, filename, sizeof(alt) - 6);
        alt[sizeof(alt) - 1] = '\0';
        if (seldfs_get_file_info(alt, out_inode) == 0) return 0;
    }
    return -1;
}

int seldfs_get_inode_by_index(size_t index, struct seldfs_inode* out_inode) {
    if (!fs_mounted || !out_inode || index >= SELDFS_MAX_FILES) return 0;
    if (sb.inodes[index].used) {
        *out_inode = sb.inodes[index];
        return 1;
    }
    return 0;
}

int seldfs_get_all_inodes(struct seldfs_inode* out_inodes, size_t max_count) {
    if (!fs_mounted || !out_inodes) return -1;
    size_t count = 0;
    for (size_t i = 0; i < SELDFS_MAX_FILES && count < max_count; i++) {
        if (sb.inodes[i].used) {
            out_inodes[count++] = sb.inodes[i];
        }
    }
    return (int)count;
}

int seldfs_get_file_count(void) {
    if (!fs_mounted) return 0;
    int count = 0;
    for (size_t i = 0; i < SELDFS_MAX_FILES; i++) {
        if (sb.inodes[i].used) count++;
    }
    return count;
}

void seldfs_get_bitmap_stats(uint32_t* total_blocks, uint32_t* used_blocks, uint32_t* free_blocks) {
    if (total_blocks) *total_blocks = SELDFS_TOTAL_DATA_BLOCKS;
    uint32_t used = 0;
    for (uint32_t i = 0; i < SELDFS_TOTAL_DATA_BLOCKS; i++) {
        if (bitmap_get_bit(i)) used++;
    }
    if (used_blocks) *used_blocks = used;
    if (free_blocks) *free_blocks = SELDFS_TOTAL_DATA_BLOCKS - used;
}
