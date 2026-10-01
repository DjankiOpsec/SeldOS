#!/usr/bin/env python3
"""
mkfs_seldfs.py - Host disk image provisioning tool for SeldOS / SNL.
Formats a raw disk image with SeldFS (Version 2) and writes files/binaries.

Layout:
  Offset 0..1MiB (2048 sectors): Reserved / Boot area
  Sector 2048: Superblock (magic=0x53454C44, version=2, block_size=512, total_inodes=32)
  Sector 2049: Block Allocation Bitmap (512 bytes = 4096 blocks = 2048 KiB capacity)
  Sectors 2050..2065 (16 sectors): Inode Table (32 inodes * 84 bytes = 2688 bytes <= 8192 bytes)
  Sector 2066+: Data Blocks (512 bytes per block)
"""

import sys
import os
import struct
import hashlib

SECTOR_SIZE = 512
SB_START_LBA = 2048
SB_MAGIC = 0x53454C44  # "SELD"
SB_VERSION = 2
SELDFS_MAX_FILES = 32
SELDFS_MAX_FILENAME = 32
SELDFS_TOTAL_DATA_BLOCKS = 65536
SELDFS_BITMAP_SECTORS = 16
DATA_START_LBA = SB_START_LBA + 1 + SELDFS_BITMAP_SECTORS + 16  # 2081

SELD_TYPE_FILE = 1
SELD_TYPE_DIR = 2

INODE_FORMAT = "<II32sIII32s"  # used, type, filename, size, start_lba, block_count, sha256
INODE_SIZE = struct.calcsize(INODE_FORMAT)

def build_disk(image_path, disk_size_mb, files_to_add):
    total_bytes = disk_size_mb * 1024 * 1024
    disk = bytearray(total_bytes)

    # 1. Superblock (Sector 2048)
    sb_bytes = struct.pack(
        "<IIIII",
        SB_MAGIC,
        SB_VERSION,
        SECTOR_SIZE,
        SELDFS_MAX_FILES,
        DATA_START_LBA
    )
    sb_offset = SB_START_LBA * SECTOR_SIZE
    disk[sb_offset:sb_offset + len(sb_bytes)] = sb_bytes

    # 2. Block Allocation Bitmap (Sector 2049..2064)
    bitmap = bytearray(SELDFS_BITMAP_SECTORS * SECTOR_SIZE)

    # 3. Inodes & Data Blocks
    current_data_block = 0
    inodes_offset = (SB_START_LBA + 1 + SELDFS_BITMAP_SECTORS) * SECTOR_SIZE

    for idx, (target_name, host_file_path, file_type) in enumerate(files_to_add):
        if idx >= SELDFS_MAX_FILES:
            print(f"[!] Warning: Exceeded max files limit ({SELDFS_MAX_FILES}), skipping {target_name}")
            break

        if not os.path.exists(host_file_path):
            print(f"[!] Error: Host file not found: {host_file_path}")
            continue

        with open(host_file_path, "rb") as f:
            data = f.read()

        file_size = len(data)
        block_count = (file_size + SECTOR_SIZE - 1) // SECTOR_SIZE
        if block_count == 0:
            block_count = 1

        if current_data_block + block_count > SELDFS_TOTAL_DATA_BLOCKS:
            print(f"[!] Error: Disk out of data blocks for {target_name}")
            break

        start_lba = DATA_START_LBA + current_data_block

        # Mark bitmap
        for b in range(block_count):
            blk_idx = current_data_block + b
            bitmap[blk_idx // 8] |= (1 << (blk_idx % 8))

        # Write data blocks
        data_offset = start_lba * SECTOR_SIZE
        disk[data_offset:data_offset + file_size] = data

        # SHA-256
        sha256 = hashlib.sha256(data).digest()

        # Inode
        fname_bytes = target_name.encode('utf-8')[:SELDFS_MAX_FILENAME - 1]
        fname_padded = fname_bytes.ljust(SELDFS_MAX_FILENAME, b'\0')

        inode_bytes = struct.pack(
            INODE_FORMAT,
            1,                  # used
            file_type,          # type (1=file, 2=dir)
            fname_padded,       # filename
            file_size,          # size
            start_lba,          # start_lba
            block_count,        # block_count
            sha256              # sha256
        )

        in_off = inodes_offset + idx * INODE_SIZE
        disk[in_off:in_off + INODE_SIZE] = inode_bytes

        print(f"[+] SeldFS: Inode {idx:02d} | {target_name:16s} | {file_size:6d} B | {block_count:2d} blocks | LBA {start_lba:5d} | SHA {sha256.hex()[:12]}...")
        current_data_block += block_count

    # Write bitmap to Sector 2049..2064
    bitmap_offset = (SB_START_LBA + 1) * SECTOR_SIZE
    disk[bitmap_offset:bitmap_offset + len(bitmap)] = bitmap

    # Write image to disk
    with open(image_path, "wb") as f:
        f.write(disk)

    used_blocks = current_data_block
    free_blocks = SELDFS_TOTAL_DATA_BLOCKS - used_blocks
    print(f"[+] SeldFS: Disk formatted successfully! Used blocks: {used_blocks}, Free blocks: {free_blocks}")

def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <output_image> <size_mb> [target_name:host_path[:type] ...]")
        sys.exit(1)

    image_path = sys.argv[1]
    size_mb = int(sys.argv[2])

    files_to_add = []
    for arg in sys.argv[3:]:
        parts = arg.split(":")
        if len(parts) >= 2:
            target_name = parts[0]
            host_path = parts[1]
            ftype = SELD_TYPE_FILE
            if len(parts) >= 3 and parts[2].lower() in ("dir", "directory", "2"):
                ftype = SELD_TYPE_DIR
            files_to_add.append((target_name, host_path, ftype))

    build_disk(image_path, size_mb, files_to_add)

if __name__ == "__main__":
    main()
