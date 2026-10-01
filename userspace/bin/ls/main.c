/*
 * SeldOS - Humboldt Kernel Project
 * SNL Coreutils: ls - Directory and File Lister
 * Lists files on SeldFS with size, block count, and SHA-256 hash using listdir / stat.
 * GPLv3 Licensed.
 */

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "seld.h"

static void print_sha256_hex(const uint8_t hash[32]) {
    const char hex_chars[] = "0123456789abcdef";
    char hex[65];
    for (int i = 0; i < 32; i++) {
        hex[i * 2]     = hex_chars[(hash[i] >> 4) & 0xF];
        hex[i * 2 + 1] = hex_chars[hash[i] & 0xF];
    }
    hex[64] = '\0';
    printf("%s", hex);
}

static void print_entry(const char* name, uint32_t size, uint32_t blocks, const uint8_t hash[32]) {
    const char hex_chars[] = "0123456789abcdef";
    char hash_str[36];
    for (int i = 0; i < 7; i++) {
        hash_str[i * 2]     = hex_chars[(hash[i] >> 4) & 0xF];
        hash_str[i * 2 + 1] = hex_chars[hash[i] & 0xF];
    }
    hash_str[14] = '.';
    hash_str[15] = '.';
    for (int i = 25; i < 32; i++) {
        hash_str[16 + (i - 25) * 2]     = hex_chars[(hash[i] >> 4) & 0xF];
        hash_str[16 + (i - 25) * 2 + 1] = hex_chars[hash[i] & 0xF];
    }
    hash_str[30] = '\0';

    printf("%-20s %10u %7u  %s\n", name, size, blocks, hash_str);
}

int main(int argc, char* argv[]) {
    if (argc >= 2) {
        // Individual file stat
        for (int i = 1; i < argc; i++) {
            struct seld_stat st;
            if (stat(argv[i], &st) == 0) {
                printf("  File: %s\n", argv[i]);
                printf("  Size: %u bytes\n", st.size);
                printf("Blocks: %u (512-byte sectors)\n", st.block_count);
                printf("SHA256: ");
                print_sha256_hex(st.sha256);
                printf("\n\n");
            } else {
                printf("ls: cannot access '%s': No such file or directory\n", argv[i]);
            }
        }
        return 0;
    }

    // List all files on SeldFS volume
    struct seld_dirent entries[32];
    int count = listdir("/", entries, 32);
    if (count < 0) {
        printf("ls: cannot access SeldFS volume\n");
        return 1;
    }

    if (count == 0) {
        printf("SeldFS: [empty directory]\n");
        return 0;
    }

    printf("%-20s %10s %7s  %-30s\n", "FILENAME", "SIZE (B)", "BLOCKS", "SHA-256 (DIGEST)");
    printf("-------------------- ---------- -------  ------------------------------\n");

    for (int i = 0; i < count; i++) {
        // Also call stat to verify inode metadata
        struct seld_stat st;
        if (stat(entries[i].name, &st) == 0) {
            print_entry(entries[i].name, st.size, st.block_count, st.sha256);
        } else {
            print_entry(entries[i].name, entries[i].size, entries[i].block_count, entries[i].sha256);
        }
    }

    printf("-------------------- ---------- -------  ------------------------------\n");
    printf("Total files: %d\n", count);

    return 0;
}
