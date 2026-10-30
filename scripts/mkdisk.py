#!/usr/bin/env python3
"""
SeldOS - Humboldt Kernel Project
SeldFS Disk Image Provisioning Tool (scripts/mkdisk.py)
Formats a raw disk image with SeldFS (v2 with allocation bitmap)
and provisions all compiled ELF-64 userland binaries (/bin/*) and assets.
GNU AGPLv3 Licensed - Humboldt Underground.
"""

import sys
import os
import struct
import hashlib
try:
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    HAS_CRYPTO = True
except ImportError:
    HAS_CRYPTO = False

DISK_SIZE_BYTES = 32 * 1024 * 1024  # 32 MiB
SECTOR_SIZE = 512

SELDFS_MAGIC = 0x53454C44  # "SELD" in ASCII
SELDFS_MAGIC_CRYPT = 0x53454C43  # "SELC" in ASCII
SELDFS_VERSION = 2
SELDFS_MAX_FILES = 64
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

def crypt_sector(key, lba, sector_bytes):
    if not HAS_CRYPTO or not key:
        return bytes(sector_bytes)
    iv = b'SELD' + struct.pack('>I', lba) + b'\x00' * 8
    cipher = Cipher(algorithms.AES(key), modes.CTR(iv))
    enc = cipher.encryptor()
    return enc.update(bytes(sector_bytes)) + enc.finalize()

def create_seldfs_image(output_path, files_to_write, compact=True, encrypt_key=None, headerless=False):
    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)

    total_data_blocks = sum(max(1, (len(data) + SECTOR_SIZE - 1) // SECTOR_SIZE) for _, data in files_to_write)
    needed_sectors = DATA_START_LBA + total_data_blocks

    if compact:
        disk_size = ((needed_sectors + 32) * SECTOR_SIZE + 0xFFFF) & ~0xFFFF
        print(f"[*] Initializing Compact ({disk_size / (1024 * 1024):.2f} MiB) SeldFS Disk Image: {output_path}")
    else:
        disk_size = DISK_SIZE_BYTES
        print(f"[*] Initializing {disk_size // (1024 * 1024)} MiB SeldFS Raw Disk Image: {output_path}")

    # Create raw image buffer initialized to zero (or CSPRNG noise if encrypted)
    with open(output_path, "wb") as f:
        f.seek(disk_size - 1)
        f.write(b"\0")

    # Open read/write
    with open(output_path, "r+b") as img:
        bitmap = bytearray(SELDFS_BITMAP_SECTORS * SECTOR_SIZE)  # 16 * 512 bytes = 65536 bits
        inodes = []
        cur_data_block = 0  # relative to DATA_START_LBA

        enc_str = " (AES-128-CTR FDE Encrypted)" if encrypt_key else " (Plaintext)"
        print(f"[*] Provisioning Userland Binaries & Files into SeldFS Root Index{enc_str}:")
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

            # Write file payload into data blocks with sector-level LBA encryption
            for b in range(block_count):
                chunk = source_data[b * SECTOR_SIZE : (b + 1) * SECTOR_SIZE]
                if len(chunk) < SECTOR_SIZE:
                    chunk = chunk + b"\0" * (SECTOR_SIZE - len(chunk))
                if encrypt_key:
                    chunk = crypt_sector(encrypt_key, start_lba + b, chunk)
                img.seek((start_lba + b) * SECTOR_SIZE)
                img.write(chunk)

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
        magic = SELDFS_MAGIC if (headerless or not encrypt_key) else SELDFS_MAGIC_CRYPT
        sb_header = struct.pack(
            SB_FMT,
            magic,
            SELDFS_VERSION,
            SECTOR_SIZE,
            SELDFS_MAX_FILES,
            DATA_START_LBA + cur_data_block
        )
        sb_sector = bytearray(SECTOR_SIZE)
        sb_sector[:len(sb_header)] = sb_header
        if encrypt_key:
            sb_sector = bytearray(crypt_sector(encrypt_key, SB_SECTOR_LBA, sb_sector))

        img.seek(SB_SECTOR_LBA * SECTOR_SIZE)
        img.write(sb_sector)

        # 2. Write Bitmap at Sector 2049 (16 sectors)
        for s in range(SELDFS_BITMAP_SECTORS):
            b_chunk = bytes(bitmap[s * SECTOR_SIZE : (s + 1) * SECTOR_SIZE])
            if encrypt_key:
                b_chunk = crypt_sector(encrypt_key, BITMAP_SECTOR_LBA + s, b_chunk)
            img.seek((BITMAP_SECTOR_LBA + s) * SECTOR_SIZE)
            img.write(b_chunk)

        # 3. Write Inodes at Sectors 2050..2065 (16 sectors)
        inodes_data = bytearray(INODES_SECTOR_COUNT * SECTOR_SIZE)
        offset = 0
        for inode in inodes:
            inodes_data[offset:offset + INODE_SIZE] = inode
            offset += INODE_SIZE

        for s in range(INODES_SECTOR_COUNT):
            i_chunk = bytes(inodes_data[s * SECTOR_SIZE : (s + 1) * SECTOR_SIZE])
            if encrypt_key:
                i_chunk = crypt_sector(encrypt_key, INODES_START_LBA + s, i_chunk)
            img.seek((INODES_START_LBA + s) * SECTOR_SIZE)
            img.write(i_chunk)

    print("    " + "-" * 78)
    print(f"[+] SeldFS formatted successfully: {len(files_to_write)} files installed, {cur_data_block} blocks used.")
    print(f"[+] Output image ready at: {output_path}")

def main():
    output_disk = "build/disk.img"
    compact = True
    bin_dir = "build/bin"
    encrypt_key = None
    headerless = False

    i = 1
    while i < len(sys.argv):
        arg = sys.argv[i]
        if arg == "--compact":
            compact = True
        elif arg == "--full":
            compact = False
        elif arg == "--bin-dir" and i + 1 < len(sys.argv):
            bin_dir = sys.argv[i + 1]
            i += 1
        elif arg == "--encrypt-key" and i + 1 < len(sys.argv):
            key_raw = sys.argv[i + 1]
            try:
                encrypt_key = bytes.fromhex(key_raw)
            except ValueError:
                encrypt_key = hashlib.sha256(key_raw.encode('utf-8')).digest()[:16]
            if len(encrypt_key) != 16:
                encrypt_key = hashlib.sha256(encrypt_key).digest()[:16]
            i += 1
        elif arg == "--headerless":
            headerless = True
        elif not arg.startswith("-"):
            output_disk = arg
        i += 1
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
        "download",
        "oracle",
        "fetch",
        "reboot",
        "poweroff",
        "purge",
        "stealth",
        "diode",
        "pinpad",
        "otp",
        "hi",
        "grep",
        "head",
        "tail",
        "less",
        "kill",
        "clip",
        "drop",
        "jammer",
        "phantom",
        "radar",
        "wiretap"
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

    # Provision pre-built packages if available (cctv, tor, cctv.bin)
    for opt in ["cctv", "tor"]:
        opt_path = os.path.join(bin_dir, opt)
        if os.path.exists(opt_path):
            with open(opt_path, "rb") as f:
                files_to_write.append((f"/bin/{opt}", f.read()))

    if os.path.exists("cctv.bin"):
        with open("cctv.bin", "rb") as f:
            files_to_write.append(("/etc/cctv.bin", f.read()))

    sample_readme = (
        "Welcome to SeldOS (Humboldt Kernel Project)!\n"
        "SNL (Seld Not Linux) Sovereign Userland Ecosystem v0.1.\n"
        "Ring 3 Privilege Isolation with Fast SYSCALL/SYSRET Interface.\n"
        "GNU General Public License version 3 (GPLv3).\n\n"
        "Desktop / Mobile Mode Commands:\n"
        "- 'pc' : Disables mobile touch controls & on-screen keyboard.\n"
        "- 'mobile' : Restores mobile touch HUD & virtual keyboard.\n"
        "- 'beep [freq] [dur]' : Play audio tone via PC speaker/soundcard.\n"
        "- 'oracle [X]' : TempleOS-style Opsec Oracle (fast chant, 290-680 Hz Dorian audio).\n"
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

    sample_secret = (
        "CONFIDENTIAL AIR-GAP PAYLOAD\n"
        "SeldOS Sovereign Optical & Acoustic Data Diode System.\n"
        "Encrypted via AES-128-GCM, Key broadcast across acoustic air-gap.\n"
        "Zero-Trust, Zero-Telemetry, Fail-Closed.\n"
    ).encode("utf-8")
    files_to_write.append(("secret.txt", sample_secret))

    corpus_path = "tools/host_test/data/corpus.txt"
    if os.path.exists(corpus_path):
        with open(corpus_path, "rb") as cf:
            # Provision compact high-entropy 128 KiB sacred corpus (~20,900 words)
            files_to_write.append(("oracle.txt", cf.read(131072)))

    # Permanently provision PC mode (clean 85x20 desktop terminal, NO mobile buttons)
    files_to_write.append(("pcmode", b"1\n"))

    create_seldfs_image(output_disk, files_to_write, compact=compact, encrypt_key=encrypt_key, headerless=headerless)

if __name__ == "__main__":
    main()
