; SeldOS Kernel Embedded Userspace Binary Blob
; GPLv3 Licensed.

bits 64
section .rodata
global userspace_blob_start
global userspace_blob_end
global userspace_blob_size

userspace_blob_start:
    incbin "build/userspace.bin"
userspace_blob_end:

userspace_blob_size:
    dq userspace_blob_end - userspace_blob_start
