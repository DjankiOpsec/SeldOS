/*
 * SeldOS - Humboldt Kernel Project
 * SNL Coreutils: sha256sum - Cryptographic Hash & Inode Integrity Verification
 * Calculates SHA-256 hash of file(s) and verifies integrity against inode.
 * GPLv3 Licensed.
 */

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "sha256.h"

#define MAX_FILE_SIZE 32768

static char file_buf[MAX_FILE_SIZE];

static void hex_encode(const uint8_t hash[32], char hex[65]) {
    const char hex_chars[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        hex[i * 2]     = hex_chars[(hash[i] >> 4) & 0xF];
        hex[i * 2 + 1] = hex_chars[hash[i] & 0xF];
    }
    hex[64] = '\0';
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: sha256sum <file...>\n");
        return 1;
    }

    int failures = 0;

    for (int i = 1; i < argc; i++) {
        int bytes = readfile(argv[i], file_buf, sizeof(file_buf));
        if (bytes < 0) {
            printf("sha256sum: %s: No such file or directory\n", argv[i]);
            failures++;
            continue;
        }

        uint8_t computed[32];
        sha256_hash(file_buf, (size_t)bytes, computed);

        char computed_hex[65];
        hex_encode(computed, computed_hex);

        struct seld_stat st;
        if (stat(argv[i], &st) == 0) {
            if (memcmp(computed, st.sha256, 32) == 0) {
                printf("%s  %s  [OK]\n", computed_hex, argv[i]);
            } else {
                char stored_hex[65];
                hex_encode(st.sha256, stored_hex);
                printf("%s  %s  [FAILED - INTEGRITY MISMATCH]\n", computed_hex, argv[i]);
                printf("  Stored Inode Hash: %s\n", stored_hex);
                failures++;
            }
        } else {
            printf("%s  %s  [OK - NO INODE CHECKSUM]\n", computed_hex, argv[i]);
        }
    }

    return (failures > 0) ? 1 : 0;
}
