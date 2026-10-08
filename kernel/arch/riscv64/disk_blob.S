/*
 * SeldOS Kernel Embedded SeldFS Disk Image Blob (RISC-V)
 * GPLv3 Licensed.
 */

.section .rodata
.global disk_blob_start
.global disk_blob_end
.global disk_blob_size
.align 4

disk_blob_start:
    .incbin "build/disk_riscv64.img"
disk_blob_end:

disk_blob_size:
    .quad disk_blob_end - disk_blob_start
