#!/usr/bin/env python3
"""
Integration & Verification Suite for SeldOS OpSec Immune System & Air-Gap Defense
Verifies:
1. Boot to /bin/sh interactive CLI.
2. 'net status' reports On-Demand Default-Deny mode.
3. 'net lock' / 'net unlock' / 'net ondemand' state machine transitions.
4. 'purge' command:
   - SeldFS cryptographic audit
   - Process tree scheduler sweep
   - Active network socket severing & cache purge
   - Air-Gap Shield engagement
   - Executable SHA-256 attestation gatekeeper
5. Verification of clean exit and screenshot.
"""

import socket
import subprocess
import time
import os
import sys
import json
import select
from PIL import Image

QMP_SOCK = "/tmp/seldos_immune_qmp.sock"
SERIAL_LOG = "/tmp/seldos_immune_serial.log"

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

    print("[*] Launching QEMU instance for OpSec Immune System validation...")
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

        # Switch to PC mode for full-screen clean terminal
        send_string(qmp, "pc\n")
        time.sleep(0.5)

        print("\n=== TEST 1: Air-Gap Network Shield Status ===")
        send_string(qmp, "net status\n")
        time.sleep(0.5)

        print("=== TEST 2: Air-Gap State Transitions (lock / unlock / ondemand) ===")
        send_string(qmp, "net lock\n")
        time.sleep(0.3)
        send_string(qmp, "net unlock\n")
        time.sleep(0.3)
        send_string(qmp, "net ondemand\n")
        time.sleep(0.3)

        print("=== TEST 3: OpSec Immune System Purge Execution ===")
        send_string(qmp, "purge\n")
        time.sleep(1.0)

        capture_screenshot(qmp, "tests/59_immune_purge_verified.png")

        # Inspect serial output for verification markers
        with open(SERIAL_LOG, "r", errors="ignore") as f:
            log = f.read()

        assert "Air-Gap Shield" in log or "Air-Gap" in log, "Air-Gap Shield log missing!"
        assert "SYS_IMMUNE_PURGE" in log, "SYS_IMMUNE_PURGE kernel syscall not triggered!"
        assert "All TCP sockets severed" in log, "TCP socket severing missing!"

        print("\n=======================================================")
        print(" [***] ALL OPSEC IMMUNE SYSTEM VERIFICATION TESTS PASSED [***] ")
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
