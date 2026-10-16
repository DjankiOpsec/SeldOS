#!/usr/bin/env python3
"""
Integration & Verification Suite for SeldOS CIA/Five-Eyes OpSec Hardening:
1. Zero-Trust PCI DMA & Option ROM Lockdown (Revoking untrusted Bus Masters)
2. Poisson Cover Traffic & Uniform Packet Padding (Anti-Correlation GPA Defense)
3. TEMPEST Video RF Dispersion & Subpixel Dithering (Van Eck Phreaking Evasion)
4. Microarchitectural State Flushing (IBPB, CPU Caches, TLB)
5. Acoustic Keystroke Analysis Evasion: Scrambled On-Screen Pinpad (/bin/pinpad)
"""

import socket
import subprocess
import time
import os
import sys
import json
import select
from PIL import Image

QMP_SOCK = "/tmp/seldos_cia_qmp.sock"
SERIAL_LOG = "/tmp/seldos_cia_serial.log"

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

    print("[*] Launching QEMU instance for CIA/Five-Eyes OpSec validation...")
    proc = subprocess.Popen(qemu_cmd)

    try:
        for _ in range(50):
            if os.path.exists(QMP_SOCK):
                break
            time.sleep(0.1)

        qmp = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        qmp.connect(QMP_SOCK)

        data = qmp.recv(2048)
        qmp.sendall(b'{"execute":"qmp_capabilities"}\n')
        drain_sock(qmp)

        print("[*] Waiting for SeldOS boot into SNL Sovereign Shell...")
        booted = False
        for _ in range(60):
            if os.path.exists(SERIAL_LOG):
                with open(SERIAL_LOG, "r", errors="ignore") as f:
                    content = f.read()
                    if "snl$" in content or "SeldShell" in content or "INIT: Launched" in content:
                        booted = True
                        break
            time.sleep(0.2)

        if not booted:
            print("[-] Timeout waiting for boot into shell!")
            sys.exit(1)

        print("[+] System successfully booted into Sovereign Userland!")
        time.sleep(0.5)

        # 1. Disarm Dead Man's Grip for script interaction
        send_string(qmp, "grip off\n")
        time.sleep(0.4)

        # 2. Verify PCI DMA & Option ROM Lockdown at boot
        with open(SERIAL_LOG, "r", errors="ignore") as f:
            boot_log = f.read()
            if "PCI DMA Lockdown active" in boot_log:
                print("[+] VERIFIED: PCI Zero-Trust DMA & Option ROM Lockdown triggered at boot.")
            else:
                print("[-] WARNING: Boot log does not mention PCI DMA Lockdown.")

        def wait_for_pattern(pattern, timeout=4.0):
            t0 = time.time()
            while time.time() - t0 < timeout:
                if os.path.exists(SERIAL_LOG):
                    with open(SERIAL_LOG, "r", errors="ignore") as f:
                        if pattern in f.read():
                            return True
                time.sleep(0.1)
            return False

        # 3. Test 'pci lockdown' command
        print("[*] Testing 'pci lockdown' command...")
        send_string(qmp, "pci lockdown\n")
        if wait_for_pattern("PCI Zero-Trust Lockdown", 4.0):
            print("[+] Verified: 'pci lockdown' executed.")
        else:
            print("[-] Timeout waiting for 'pci lockdown'")

        # 4. Test 'chaff on' and 'chaff status'
        print("[*] Testing 'chaff on' (Poisson cover traffic & uniform packet padding)...")
        send_string(qmp, "chaff on\n")
        if wait_for_pattern("Poisson Chaff & Uniform Padding ACTIVE", 4.0):
            print("[+] Verified: 'chaff on' activated.")
        else:
            print("[-] Timeout waiting for 'chaff on'")
        send_string(qmp, "chaff status\n")
        time.sleep(0.5)

        # 5. Test 'tempest on' and 'tempest status'
        print("[*] Testing 'tempest on' (Video RF dispersion / subpixel dithering)...")
        send_string(qmp, "tempest on\n")
        if wait_for_pattern("TEMPEST: Video RF dispersion", 4.0):
            print("[+] Verified: 'tempest on' activated.")
        else:
            print("[-] Timeout waiting for 'tempest on'")
        send_string(qmp, "tempest status\n")
        time.sleep(0.5)

        # 6. Test 'uarch flush'
        print("[*] Testing 'uarch flush' (Microarchitectural IBPB, cache, TLB wipe)...")
        send_string(qmp, "uarch flush\n")
        if wait_for_pattern("Microarchitectural state sanitized", 4.0):
            print("[+] Verified: 'uarch flush' executed.")
        else:
            print("[-] Timeout waiting for 'uarch flush'")

        # 7. Test '/bin/pinpad'
        print("[*] Testing '/bin/pinpad' (Acoustic Keystroke Evasion)...")
        send_string(qmp, "pinpad\n")
        if wait_for_pattern("SCRAMBLED ON-SCREEN PINPAD", 4.0):
            print("[+] Verified: pinpad started.")
        else:
            print("[-] Timeout waiting for pinpad to start")

        time.sleep(0.5)
        # Send 'q' to cleanly exit pinpad
        send_string(qmp, "q")
        if wait_for_pattern("Pinpad: Aborted by operator. Plaintext wiped", 4.0):
            print("[+] Verified: pinpad exited cleanly.")
        else:
            print("[-] Timeout waiting for pinpad exit")

        time.sleep(0.5)
        # Capture final screenshot
        out_png = "tests/62_cia_opsec_hardened.png"
        capture_screenshot(qmp, out_png)

        # Verify serial log
        with open(SERIAL_LOG, "r", errors="ignore") as f:
            log_data = f.read()

        verifications = [
            ("PCI DMA Lockdown", "PCI DMA Lockdown active"),
            ("Poisson Chaff Activation", "Poisson Chaff & Uniform Padding ACTIVE"),
            ("TEMPEST Evasion", "TEMPEST: Video RF dispersion"),
            ("Microarchitectural Flush", "Microarchitectural state sanitized"),
            ("Pinpad Execution", "Pinpad: Aborted by operator. Plaintext wiped")
        ]

        print("\n=======================================================")
        print(" SELDOS CIA/FIVE-EYES OPSEC VERIFICATION REPORT")
        print("=======================================================")
        all_passed = True
        for name, pattern in verifications:
            if pattern in log_data:
                print(f"[+] PASSED: {name}")
            else:
                print(f"[-] FAILED: {name} (pattern '{pattern}' not found)")
                all_passed = False

        if all_passed:
            print("\n[+] ALL CIA-GRADE OPSEC SUBSYSTEMS FULLY VERIFIED!")
        else:
            print("\n[-] SOME OPSEC CHECKS FAILED!")
            sys.exit(1)

    finally:
        proc.terminate()
        proc.wait()
        for f in [QMP_SOCK, SERIAL_LOG]:
            if os.path.exists(f):
                os.remove(f)

if __name__ == "__main__":
    main()
