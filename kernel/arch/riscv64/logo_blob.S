/*
 * SeldOS Kernel Embedded SVGZ Logo Blob (RISC-V)
 * GPLv3 Licensed.
 */

.section .rodata
.global seldos_logo_svgz_start
.global seldos_logo_svgz_end
.global seldos_logo_svgz_size
.align 4

seldos_logo_svgz_start:
    .incbin "seldos_logo.svgz"
seldos_logo_svgz_end:

seldos_logo_svgz_size:
    .quad seldos_logo_svgz_end - seldos_logo_svgz_start
