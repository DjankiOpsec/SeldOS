/*
 * SeldOS Kernel Embedded SeldFS Disk Image Blob (RISC-V)
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

.section .data
.global disk_blob_start
.global disk_blob_end
.global disk_blob_size
.align 4

disk_blob_start:
    .incbin "build/disk_riscv64.img"
disk_blob_end:

disk_blob_size:
    .quad disk_blob_end - disk_blob_start
