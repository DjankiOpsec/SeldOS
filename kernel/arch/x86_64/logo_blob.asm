; SeldOS Kernel Embedded SVGZ Logo Blob
; GPLv3 Licensed.

bits 64
section .rodata
global seldos_logo_svgz_start
global seldos_logo_svgz_end
global seldos_logo_svgz_size

seldos_logo_svgz_start:
    incbin "seldos_logo.svgz"
seldos_logo_svgz_end:

seldos_logo_svgz_size:
    dq seldos_logo_svgz_end - seldos_logo_svgz_start
