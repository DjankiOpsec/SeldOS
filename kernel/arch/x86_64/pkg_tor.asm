; SeldOS Sovereign Package Repository - Tor Browser Mirror Blob
; Ring 0 OpSec Autonomous Package Mirror
; GPLv3 Licensed.

bits 64
section .rodata
global pkg_tor_blob_start
global pkg_tor_blob_end
global pkg_tor_blob_size

pkg_tor_blob_start:
    incbin "build/bin/tor"
pkg_tor_blob_end:

pkg_tor_blob_size:
    dq pkg_tor_blob_end - pkg_tor_blob_start
