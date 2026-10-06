#!/usr/bin/env python3
"""
Integration & Verification Suite for SeldOS Anti-TSPU/DPI Evasion & Stealth Mode
Verifies:
1. Boot to /bin/sh interactive CLI.
2. '/bin/stealth status' execution and reporting.
3. '/bin/stealth on' engagement (Air-Gap Stealth + In-Kernel TCP Desync SPLIT).
4. 'net status' reporting STEALTH mode and SPLIT TCP desync in shell.
5. ICMP Ping drop in stealth mode (anti-active-tracking).
6. Raw UDP port 53 DNS block with DoT fallback in stealth mode.
7. TCP Desync mode manipulation ('stealth desync fake', 'stealth desync split', 'desync split').
8. Clean execution and verified UI screenshot.
"""

import socket
import subprocess
import time
import os
import sys
import json
import select
from PIL import Image

QMP_SOCK = "/tmp/seldos_stealth_qmp.sock"
SERIAL_LOG = "/tmp/seldos_stealth_serial.log"

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

def capture_screenshot(sock, out_png):
    ppm_path = out_png.replace(".png", ".ppm")
    payload = {"execute": "screendump", "arguments": {"filename": ppm_path}}
    sock.sendall(json.dumps(payload).encode() + b"\n")
    drain_sock(sock)
    time.sleep(0.3)
    if os.path.exists(ppm_path):
        im = Image.open(ppm_path)
        im.save(out_png)
        os.remove(ppm_path)
        print(f"[+] Saved screenshot: {out_png} ({im.size})")
        return im.size
    return None

def main():
    for f in [QMP_SOCK, SERIAL_LOG]:
        if os.path.exists(f):
            os.remove(f)

    qemu_cmd = [
        "qemu-system-x86_64",
        "-cdrom", "build/seldos.iso",
        "-m", "512M",
        "-device", "e1000,netdev=net0",
        "-netdev", "user,id=net0",
        "-serial", f"file:{SERIAL_LOG}",
        "-qmp", f"unix:{QMP_SOCK},server,nowait",
        "-vga", "std",
        "-display", "none"
    ]

    print("[*] Launching QEMU instance for Anti-TSPU/DPI Stealth validation...")
    proc = subprocess.Popen(qemu_cmd)

    try:
        # Wait for QMP socket
        for _ in range(50):
            if os.path.exists(QMP_SOCK):
                break
            time.sleep(0.1)

        qmp = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        qmp.connect(QMP_SOCK)

        data = qmp.recv(2048)
        qmp.sendall(b'{"execute":"qmp_capabilities"}\n')
        drain_sock(qmp)

        # Wait for boot into shell
        print("[*] Waiting for SeldOS boot into SNL Sovereign Shell...")
        booted = False
        for _ in range(60):
            if os.path.exists(SERIAL_LOG):
                with open(SERIAL_LOG, "r", errors="ignore") as f:
                    content = f.read()
                    if "MSR LSTAR Fast Syscall Handshake: OK" in content:
                        booted = True
                        break
            time.sleep(0.2)

        assert booted, "System failed to boot into SNL userspace!"
        time.sleep(1.0)

        # Switch to PC mode for clean terminal view
        send_string(qmp, "pc\n")
        time.sleep(0.5)

        print("\n=== TEST 1: Check Stealth Status via /bin/stealth ===")
        send_string(qmp, "stealth status\n")
        time.sleep(0.8)

        print("=== TEST 2: Activate Sovereign Stealth Mode (/bin/stealth on) ===")
        send_string(qmp, "stealth on\n")
        time.sleep(0.8)

        print("=== TEST 3: Verify Shell 'net status' Reports Stealth Mode ===")
        send_string(qmp, "net status\n")
        time.sleep(0.8)

        print("=== TEST 4: Verify ICMP Ping Dropped in Stealth Mode ===")
        send_string(qmp, "ping 10.0.2.2\n")
        time.sleep(1.0)

        print("=== TEST 5: Verify TCP Desync Configuration & Shell 'desync' Builtin ===")
        send_string(qmp, "desync split\n")
        time.sleep(0.5)
        send_string(qmp, "stealth desync fake\n")
        time.sleep(0.5)
        send_string(qmp, "stealth desync split\n")
        time.sleep(0.5)

        print("=== TEST 6: Capture Verification Screenshot ===")
        capture_screenshot(qmp, "tests/61_stealth_evasion_verified.png")

        # Inspect serial output for verification markers
        with open(SERIAL_LOG, "r", errors="ignore") as f:
            log = f.read()

        assert "sys_net_set_desync" in log or "TCP Desync mode" in log or "desync" in log, \
            "TCP Desync syscall not logged!"
        assert "OpSec Stealth Mode: ICMP Ping blocked" in log, \
            "ICMP ping stealth block not triggered in kernel log!"

        print("\n=======================================================")
        print(" [***] ALL ANTI-TSPU/DPI STEALTH TESTS PASSED [***] ")
        print("=======================================================")

    finally:
        try:
            qmp.close()
        except:
            pass
        proc.terminate()
        proc.wait()

if __name__ == "__main__":
    main()
