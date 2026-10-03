#!/usr/bin/env python3
"""
SeldOS - Humboldt Kernel Project
SeldFS Disk Image Provisioning Tool (scripts/mkdisk.py)
Formats a raw disk image with SeldFS (v2 with allocation bitmap)
and provisions all compiled ELF-64 userland binaries (/bin/*) and assets.
GPLv3 Licensed.
"""

import sys
import os
import struct
import hashlib

DISK_SIZE_BYTES = 32 * 1024 * 1024  # 32 MiB
SECTOR_SIZE = 512

SELDFS_MAGIC = 0x53454C44  # "SELD" in ASCII
SELDFS_VERSION = 2
SELDFS_MAX_FILES = 32
SELDFS_MAX_FILENAME = 32
SELDFS_TOTAL_DATA_BLOCKS = 65536
SELDFS_BITMAP_SECTORS = 16

SB_START_LBA = 2048
SB_SECTOR_LBA = SB_START_LBA + 0       # Sector 2048: Superblock
BITMAP_SECTOR_LBA = SB_START_LBA + 1   # Sector 2049..2064: Bitmap (16 sectors)
INODES_START_LBA = SB_START_LBA + 1 + SELDFS_BITMAP_SECTORS  # Sector 2065..2080: Inodes (16 sectors)
INODES_SECTOR_COUNT = 16
DATA_START_LBA = INODES_START_LBA + INODES_SECTOR_COUNT     # Sector 2081+: Data Blocks

# struct seldfs_inode format:
# uint32_t used;
# uint32_t type;  // SELD_TYPE_FILE (1), SELD_TYPE_DIR (2)
# char filename[32];
# uint32_t size;
# uint32_t start_lba;
# uint32_t block_count;
# uint8_t sha256[32];
INODE_FMT = "<II32sIII32s"
INODE_SIZE = struct.calcsize(INODE_FMT)  # 84 bytes

# Superblock header first 20 bytes:
# uint32_t magic;
# uint32_t version;
# uint32_t block_size;
# uint32_t total_inodes;
# uint32_t free_data_lba;
SB_FMT = "<IIIII"

def create_seldfs_image(output_path, files_to_write):
    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)

    print(f"[*] Initializing {DISK_SIZE_BYTES // (1024 * 1024)} MiB SeldFS Raw Disk Image: {output_path}")

    # Create raw image buffer initialized to zero
    with open(output_path, "wb") as f:
        f.seek(DISK_SIZE_BYTES - 1)
        f.write(b"\0")

    # Open read/write
    with open(output_path, "r+b") as img:
        bitmap = bytearray(SELDFS_BITMAP_SECTORS * SECTOR_SIZE)  # 16 * 512 bytes = 65536 bits
        inodes = []
        cur_data_block = 0  # relative to DATA_START_LBA

        print("[*] Provisioning Userland Binaries & Files into SeldFS Root Index:")
        print(f"    {'TARGET PATH':<20} {'SIZE (BYTES)':<14} {'BLOCKS':<8} {'SHA-256 CHECKSUM'}")
        print("    " + "-" * 78)

        for target_name, source_data in files_to_write:
            if len(inodes) >= SELDFS_MAX_FILES:
                print(f"[-] Error: Exceeded SELDFS_MAX_FILES ({SELDFS_MAX_FILES})")
                sys.exit(1)

            file_size = len(source_data)
            block_count = (file_size + SECTOR_SIZE - 1) // SECTOR_SIZE
            if block_count == 0:
                block_count = 1

            if cur_data_block + block_count > SELDFS_TOTAL_DATA_BLOCKS:
                print(f"[-] Error: Disk space full in SeldFS data region")
                sys.exit(1)

            start_lba = DATA_START_LBA + cur_data_block
            file_sha256 = hashlib.sha256(source_data).digest()

            # Mark bits in bitmap
            for b in range(block_count):
                blk_idx = cur_data_block + b
                bitmap[blk_idx // 8] |= (1 << (blk_idx % 8))

            # Write file payload into data blocks
            img.seek(start_lba * SECTOR_SIZE)
            img.write(source_data)
            # Pad sector if needed
            pad_len = (block_count * SECTOR_SIZE) - file_size
            if pad_len > 0:
                img.write(b"\0" * pad_len)

            # Inode record
            encoded_name = target_name.encode("utf-8")[:31]
            padded_name = encoded_name + b"\0" * (SELDFS_MAX_FILENAME - len(encoded_name))

            inode_bytes = struct.pack(
                INODE_FMT,
                1,  # used = 1
                1,  # type = SELD_TYPE_FILE (1)
                padded_name,
                file_size,
                start_lba,
                block_count,
                file_sha256
            )
            inodes.append(inode_bytes)

            cur_data_block += block_count
            print(f"    {target_name:<20} {file_size:<14} {block_count:<8} {file_sha256.hex()}")

        # Fill remaining inodes with empty records
        while len(inodes) < SELDFS_MAX_FILES:
            inodes.append(b"\0" * INODE_SIZE)

        # 1. Write Superblock at Sector 2048
        sb_header = struct.pack(
            SB_FMT,
            SELDFS_MAGIC,
            SELDFS_VERSION,
            SECTOR_SIZE,
            SELDFS_MAX_FILES,
            DATA_START_LBA + cur_data_block
        )
        sb_sector = bytearray(SECTOR_SIZE)
        sb_sector[:len(sb_header)] = sb_header

        img.seek(SB_SECTOR_LBA * SECTOR_SIZE)
        img.write(sb_sector)

        # 2. Write Bitmap at Sector 2049
        img.seek(BITMAP_SECTOR_LBA * SECTOR_SIZE)
        img.write(bitmap)

        # 3. Write Inodes at Sectors 2050..2065
        inodes_data = bytearray(INODES_SECTOR_COUNT * SECTOR_SIZE)
        offset = 0
        for inode in inodes:
            inodes_data[offset:offset + INODE_SIZE] = inode
            offset += INODE_SIZE

        img.seek(INODES_START_LBA * SECTOR_SIZE)
        img.write(inodes_data)

    print("    " + "-" * 78)
    print(f"[+] SeldFS formatted successfully: {len(files_to_write)} files installed, {cur_data_block} blocks used.")
    print(f"[+] Output image ready at: {output_path}")

def main():
    output_disk = "build/disk.img"
    if len(sys.argv) > 1:
        output_disk = sys.argv[1]

    bin_dir = "build/bin"
    utilities = [
        "init",
        "sh",
        "ls",
        "cat",
        "echo",
        "rm",
        "sha256sum",
        "uname",
        "ps",
        "fm",
        "download"
    ]

    files_to_write = []

    # Provision standard ELF-64 utilities
    for util in utilities:
        bin_path = os.path.join(bin_dir, util)
        if not os.path.exists(bin_path):
            print(f"[-] Error: Compiled binary {bin_path} not found. Please compile userland first.")
            sys.exit(1)

        with open(bin_path, "rb") as f:
            data = f.read()

        target_name = f"/bin/{util}"
        files_to_write.append((target_name, data))

    # Provision Doom binary if present
    doom_bin = os.path.join(bin_dir, "doom")
    if os.path.exists(doom_bin):
        with open(doom_bin, "rb") as f:
            data = f.read()
        files_to_write.append(("/bin/doom", data))

    # Provision Doom WAD if present
    wad_candidates = ["doom1.wad", "build/doom1.wad", "userspace/doom/doom1.wad"]
    for wad_path in wad_candidates:
        if os.path.exists(wad_path):
            print(f"[*] Found DOOM WAD: {wad_path} ({os.path.getsize(wad_path)} bytes)")
            with open(wad_path, "rb") as f:
                data = f.read()
            files_to_write.append(("doom1.wad", data))
            break

    sample_readme = (
        "Welcome to SeldOS (Humboldt Kernel Project)!\n"
        "SNL (Seld Not Linux) Sovereign Userland Ecosystem v0.1.\n"
        "Ring 3 Privilege Isolation with Fast SYSCALL/SYSRET Interface.\n"
        "GNU General Public License version 3 (GPLv3).\n\n"
        "Desktop / Mobile Mode Commands:\n"
        "- 'pc' : Disables mobile touch controls & on-screen keyboard.\n"
        "- 'mobile' : Restores mobile touch HUD & virtual keyboard.\n"
        "- 'beep [freq] [dur]' : Play audio tone via PC speaker/soundcard.\n"
    ).encode("utf-8")
    files_to_write.append(("readme.txt", sample_readme))

    sample_opsec = (
        "SeldOS OpSec & Zero-Trust Subsystems:\n"
        "1. Physical Memory Manager (4 KiB Bitmap Frame Allocator)\n"
        "2. Higher-Half Virtual Memory Manager (W^X Enforcement)\n"
        "3. SeldFS Hardened Block Filesystem with SHA-256 Inodes\n"
        "4. Cooperative Scheduler with Ring 0 / Ring 3 Privilege Gates\n"
    ).encode("utf-8")
    files_to_write.append(("opsec.txt", sample_opsec))

    create_seldfs_image(output_disk, files_to_write)

if __name__ == "__main__":
    main()
