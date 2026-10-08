/*
 * SeldOS Kernel Embedded Userspace Binary Blob (RISC-V)
 * GPLv3 Licensed.
 */

.section .rodata
.global userspace_blob_start
.global userspace_blob_end
.global userspace_blob_size
.align 4

userspace_blob_start:
    .incbin "build/rv64/userspace.bin"
userspace_blob_end:

userspace_blob_size:
    .quad userspace_blob_end - userspace_blob_start
