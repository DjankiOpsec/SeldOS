#!/usr/bin/env python3
"""
Comprehensive Verification Suite for:
1. Linux-style boot animation with Humboldt SVGZ logo & real running init code lines.
2. Hardware driver disable commands: gpu drv off, cpu drv off, ram drv off.
3. Realistic, unscripted Kernel Panic triggering on actual subsystem demand.
4. Full registers dump, stack trace, and system halt.
"""

import socket
import subprocess
import time
import os
import json
import select
from PIL import Image

QMP_SOCK = "/tmp/seldos_panic_qmp.sock"
SERIAL_LOG = "/tmp/seldos_panic_serial.log"

KEY_MAP = {
    ' ': 'spc',
    '\n': 'ret',
    '-': 'minus',
    '_': 'shift-minus',
    '.': 'dot',
    '/': 'slash',
}

def drain_sock(sock):
    while True:
        r, _, _ = select.select([sock], [], [], 0.0)
        if r:
            data = sock.recv(4096)
            if not data:
                break
        else:
            break

def send_key(sock, key):
    if key.startswith("shift-"):
        real = key[6:]
        keys = [{"type": "qcode", "data": "shift"}, {"type": "qcode", "data": real}]
    else:
        keys = [{"type": "qcode", "data": key}]
    payload = {"execute": "send-key", "arguments": {"keys": keys}}
    sock.sendall(json.dumps(payload).encode() + b"\n")
    drain_sock(sock)

def send_string(sock, s):
    for ch in s:
        if ch in KEY_MAP:
            k = KEY_MAP[ch]
        elif ch.isalpha():
            k = f"shift-{ch.lower()}" if ch.isupper() else ch.lower()
        else:
            k = ch
        send_key(sock, k)
        time.sleep(0.04)

def capture_screenshot(sock, out_path):
    ppm_temp = f"/tmp/{os.path.basename(out_path)}.ppm"
    if os.path.exists(ppm_temp):
        os.remove(ppm_temp)
    payload = {"execute": "screendump", "arguments": {"filename": ppm_temp}}
    sock.sendall(json.dumps(payload).encode() + b"\n")
    time.sleep(0.3)
    drain_sock(sock)
    if os.path.exists(ppm_temp):
        img = Image.open(ppm_temp)
        img.save(out_path)
        os.remove(ppm_temp)
        print(f"[+] Saved screenshot: {out_path} ({img.size})")
        return img
    return None

def start_qemu():
    if os.path.exists(QMP_SOCK):
        os.remove(QMP_SOCK)
    if os.path.exists(SERIAL_LOG):
        os.remove(SERIAL_LOG)

    cmd = [
        "qemu-system-x86_64",
        "-cdrom", "build/seldos.iso",
        "-drive", "file=build/disk.img,format=raw",
        "-display", "none",
        "-qmp", f"unix:{QMP_SOCK},server,nowait",
        "-serial", f"file:{SERIAL_LOG}",
        "-net", "nic,model=e1000",
        "-net", "user"
    ]
    proc = subprocess.Popen(cmd)
    time.sleep(1.5)

    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(QMP_SOCK)
    drain_sock(s)
    s.sendall(json.dumps({"execute": "qmp_capabilities"}).encode() + b"\n")
    time.sleep(0.2)
    drain_sock(s)
    return proc, s

def test_boot_animation_and_logo():
    print("\n========================================================")
    print(" TEST 1: Linux-Style Boot Animation & seldos_logo.svgz")
    print("========================================================")
    proc, s = start_qemu()
    try:
        # Wait for boot
        time.sleep(2.5)
        img = capture_screenshot(s, "tests/panic_01_boot_anim.png")
        assert img is not None, "Failed to capture boot animation screenshot!"
        assert img.size == (680, 334), f"Unexpected size: {img.size}"

        with open(SERIAL_LOG, "r") as f:
            log = f.read()

        assert "Linux-Style Early Boot Pipeline Initialized" in log, \
            "Boot animation header not found in log!"
        assert "seldos_logo.svgz (GZIP compressed SVG, verified)" in log, \
            "Compressed SVGZ logo was not validated!"
        assert "[  OK  ] GDT/TSS" in log, "Running init line GDT missing!"
        assert "[  OK  ] IDT" in log, "Running init line IDT missing!"
        assert "[  OK  ] PMM" in log, "Running init line PMM missing!"
        assert "[  OK  ] VMM" in log, "Running init line VMM missing!"
        assert "[  OK  ] SYSCALL" in log, "Running init line SYSCALL missing!"
        assert "[  OK  ] KMALLOC" in log, "Running init line KMALLOC missing!"
        assert "[  OK  ] ATA" in log, "Running init line ATA missing!"
        assert "[  OK  ] SELDFS" in log, "Running init line SELDFS missing!"
        assert "[  OK  ] CRYPTO" in log, "Running init line CRYPTO missing!"
        assert "[  OK  ] PIT" in log, "Running init line PIT missing!"
        assert "[  OK  ] AUDIO" in log, "Running init line AUDIO missing!"
        assert "[  OK  ] SCHED" in log, "Running init line SCHED missing!"
        assert "[  OK  ] NET" in log, "Running init line NET missing!"
        assert "[  OK  ] SELFTEST" in log, "Running init line SELFTEST missing!"
        assert "[  OK  ] INIT" in log, "Running init line INIT missing!"

        print("[+] PASS: Boot animation rendered with SVGZ logo and 15 running init code lines!")
    finally:
        s.close()
        proc.terminate()
        proc.wait()

def test_gpu_drv_off():
    print("\n========================================================")
    print(" TEST 2: 'gpu drv off' -> Realistic Kernel Panic on render")
    print("========================================================")
    proc, s = start_qemu()
    try:
        time.sleep(2.5) # Wait for shell
        print("[*] Sending command: 'gpu drv off' to shell...")
        send_string(s, "gpu drv off\n")
        time.sleep(1.5)

        capture_screenshot(s, "tests/panic_02_gpu_panic.png")

        with open(SERIAL_LOG, "r") as f:
            log = f.read()

        assert "DRIVER: GPU video display controller brutally disabled!" in log, \
            "GPU driver disable event not logged!"
        assert "KERNEL PANIC: NOT SYNCING" in log, \
            "Kernel panic header missing from log!"
        assert "kernel/drivers/vga.c:" in log and ": error: in 'vga_putchar':" in log, \
            "Compiler diagnostic location for vga_putchar missing from log!"
        assert "glyph render aborted" in log and "display controller unmapped" in log, \
            "Concrete technical reason missing from log!"
        assert "GPU Driver Offline" not in log, \
            "Scripted generic text 'GPU Driver Offline' must not be present!"
        assert "Press [ESC] to reboot." in log, \
            "Reboot control prompt missing from log!"
        assert "Register Dump:" in log and "RIP:" in log and "CR0:" in log, \
            "Panic register dump missing!"
        assert "0x0x" not in log, \
            "Malformed double 0x0x detected in register dump!"
        assert "0x7369645F" not in log, \
            "Corrupted RBP ASCII value detected in stack unwind!"
        assert "SYSTEM HALTED" in log, \
            "Machine halt message missing!"

        print("[+] PASS: 'gpu drv off' successfully killed GPU driver and provoked Kernel Panic!")
    finally:
        s.close()
        proc.terminate()
        proc.wait()

def test_cpu_drv_off():
    print("\n========================================================")
    print(" TEST 3: 'cpu drv off' -> Realistic Kernel Panic on sched")
    print("========================================================")
    proc, s = start_qemu()
    try:
        time.sleep(2.5) # Wait for shell
        print("[*] Sending command: 'cpu drv off' to shell...")
        send_string(s, "cpu drv off\n")
        time.sleep(1.5)

        img = capture_screenshot(s, "tests/panic_03_cpu_panic.png")
        assert img is not None, "Failed to capture CPU panic screenshot!"

        # Verify real registers rendered on graphical panic framebuffer
        crop = img.crop((30, 120, 500, 200))
        non_bg_colors = [c for cnt, c in (crop.getcolors(maxcolors=1000) or []) if c != (24, 24, 26)]
        assert len(non_bg_colors) >= 3, \
            f"Screen register dump is missing actual formatted registers! Colors: {non_bg_colors}"

        with open(SERIAL_LOG, "r") as f:
            log = f.read()

        assert "DRIVER: CPU execution supervisor and core scheduler brutally disabled!" in log, \
            "CPU driver disable event not logged!"
        assert "KERNEL PANIC: NOT SYNCING" in log, \
            "Kernel panic header missing from log!"
        assert "kernel/sched/sched.c:" in log and ": error: in 'sched_tick':" in log, \
            "Compiler diagnostic location for sched_tick missing from log!"
        assert "quantum preempt tick dropped" in log and "CPU scheduler pipeline inactive" in log, \
            "Concrete technical reason missing from log!"
        assert "CPU Driver Offline" not in log, \
            "Scripted generic text 'CPU Driver Offline' must not be present!"
        assert "Press [ESC] to reboot." in log, \
            "Reboot control prompt missing from log!"
        assert "Register Dump:" in log, \
            "Panic register dump missing!"
        assert "0x0x" not in log, \
            "Malformed double 0x0x detected in register dump!"
        assert "0x7369645F" not in log, \
            "Corrupted RBP ASCII value detected in stack unwind!"
        assert "SYSTEM HALTED" in log, \
            "Machine halt message missing!"

        print("[+] PASS: 'cpu drv off' successfully killed CPU driver and provoked Kernel Panic!")
    finally:
        s.close()
        proc.terminate()
        proc.wait()

def test_ram_drv_off():
    print("\n========================================================")
    print(" TEST 4: 'ram drv off' -> Realistic Kernel Panic on alloc")
    print("========================================================")
    proc, s = start_qemu()
    try:
        time.sleep(2.5) # Wait for shell
        print("[*] Sending command: 'ram drv off' to shell...")
        send_string(s, "ram drv off\n")
        time.sleep(1.5)

        img = capture_screenshot(s, "tests/panic_04_ram_panic.png")
        assert img is not None, "Failed to capture RAM panic screenshot!"

        # Verify real registers rendered on graphical panic framebuffer
        crop = img.crop((30, 120, 500, 200))
        non_bg_colors = [c for cnt, c in (crop.getcolors(maxcolors=1000) or []) if c != (24, 24, 26)]
        assert len(non_bg_colors) >= 3, \
            f"Screen register dump is missing actual formatted registers! Colors: {non_bg_colors}"

        with open(SERIAL_LOG, "r") as f:
            log = f.read()

        assert "Testing allocation..." not in log, \
            "Fake scripted allocation test detected in shell handler!"
        assert "DRIVER: RAM physical memory frame allocator brutally disabled!" in log, \
            "RAM driver disable event not logged!"
        assert "KERNEL PANIC: NOT SYNCING" in log, \
            "Kernel panic header missing from log!"
        assert (("kernel/sys/fast_syscall.c:" in log or "kernel/mm/pmm.c:" in log or "kernel/mm/kmalloc.c:" in log) and ": error:" in log), \
            "Compiler diagnostic location for RAM fault missing from log!"
        assert "physical memory manager exhausted" in log or "out of memory" in log, \
            "Concrete technical reason missing from log!"
        assert "RAM Driver Offline" not in log, \
            "Scripted generic text 'RAM Driver Offline' must not be present!"
        assert "ram_is_driver" not in log, \
            "Internal identifier 'ram_is_driver' must not be present in diagnostic output!"
        assert "Press [ESC] to reboot." in log, \
            "Reboot control prompt missing from log!"
        assert "Register Dump:" in log, \
            "Panic register dump missing!"
        assert "0x0x" not in log, \
            "Malformed double 0x0x detected in register dump!"
        assert "0x7369645F" not in log, \
            "Corrupted RBP ASCII value detected in stack unwind!"
        assert "SYSTEM HALTED" in log, \
            "Machine halt message missing!"

        print("[+] PASS: 'ram drv off' successfully killed RAM driver and provoked Kernel Panic!")
    finally:
        s.close()
        proc.terminate()
        proc.wait()

def main():
    print("[*] Running SeldOS Boot Animation & Kernel Panic Verification Suite...")
    test_boot_animation_and_logo()
    test_gpu_drv_off()
    test_cpu_drv_off()
    test_ram_drv_off()
    print("\n" + "=" * 64)
    print(" ALL 4 BOOT ANIMATION & KERNEL PANIC TESTS PASSED SUCCESSFULLY!")
    print("=" * 64 + "\n")

if __name__ == "__main__":
    main()
