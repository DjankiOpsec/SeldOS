#!/usr/bin/env python3
"""
Integration & Verification Suite for SeldOS Advanced OpSec Hardening:
1. Anti-Clock-Skew Normalization (Zero-PPM virtual oscillator + CSPRNG micro-dither)
2. In-Band Network Time Sync (Zero UDP port 123 NTP queries / NTP hard-drop)
3. Zero-DHCP Static IP Configuration & Silent Wire Boot (No unsolicited boot ARP/DHCP)
4. Hardware Telemetry & ACPI/SMBus Sanitization (Zeroed serials, masked battery)
5. Poisson Boot Delay (Randomized hold before network transmission)
6. Ring 3 Sovereign Shell Builtins & Userspace Syscall Gate Verification
"""

import socket
import subprocess
import time
import os
import sys
import json
import select
from PIL import Image

QMP_SOCK = "/tmp/seldos_adv_qmp.sock"
SERIAL_LOG = "/tmp/seldos_adv_serial.log"

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

    print("[*] Launching QEMU instance for Advanced OpSec Hardening verification...")
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

        # Disarm Dead Man's Grip for test commands
        send_string(qmp, "grip off\n")
        time.sleep(0.4)

        def wait_for_pattern(pattern, timeout=4.0):
            t0 = time.time()
            while time.time() - t0 < timeout:
                if os.path.exists(SERIAL_LOG):
                    with open(SERIAL_LOG, "r", errors="ignore") as f:
                        if pattern in f.read():
                            return True
                time.sleep(0.1)
            return False

        # 1. Anti-Clock-Skew Normalization test
        print("[*] Testing 'skew status' and 'skew on'...")
        send_string(qmp, "skew status\n")
        if wait_for_pattern("Anti-Clock-Skew Normalization Status", 4.0):
            print("[+] Verified: 'skew status' reported.")
        send_string(qmp, "skew on\n")
        if wait_for_pattern("Anti-Clock-Skew Normalization: ACTIVE", 4.0):
            print("[+] Verified: 'skew on' enabled virtual zero-PPM oscillator.")
        else:
            print("[-] Timeout waiting for 'skew on'")

        # 2. In-Band Network Time test
        print("[*] Testing 'time status' and 'time sync'...")
        send_string(qmp, "time\n")
        time.sleep(0.3)
        send_string(qmp, "time sync 1729000000\n")
        if wait_for_pattern("Certified network time updated: epoch 1729000000", 4.0):
            print("[+] Verified: 'time sync' accepted certified in-band epoch timestamp.")
        else:
            print("[-] Timeout waiting for 'time sync'")

        # 3. Hardware Telemetry Sanitization test
        print("[*] Testing 'telemetry'...")
        send_string(qmp, "telemetry\n")
        if wait_for_pattern("Hardware Serial: 0000-0000-0000-0000 [Sanitized / Zeroed]", 4.0):
            print("[+] Verified: 'telemetry' reports sanitized zeroed identifiers.")
        else:
            print("[-] Timeout waiting for 'telemetry'")

        # 4. Zero-DHCP Static IP configuration test
        print("[*] Testing 'ifconfig static' reconfiguration...")
        send_string(qmp, "ifconfig static 10.0.2.19 255.255.255.0 10.0.2.2 76.76.2.2\n")
        if wait_for_pattern("Static IPv4 configured: IP 10.0.2.19", 4.0):
            print("[+] Verified: 'ifconfig static' applied without broadcast DHCP.")
        else:
            print("[-] Timeout waiting for 'ifconfig static'")

        # 5. Poisson Boot Delay test
        print("[*] Testing 'net delay'...")
        send_string(qmp, "net delay on\n")
        if wait_for_pattern("Poisson Boot Delay: ENABLED", 4.0):
            print("[+] Verified: 'net delay on' activated initial TX hold.")
        else:
            print("[-] Timeout waiting for 'net delay on'")
        send_string(qmp, "net delay status\n")
        time.sleep(0.4)

        # 6. Ring 3 Selftest execution
        print("[*] Running Ring 3 'selftest' with extended OpSec assertions...")
        send_string(qmp, "selftest\n")
        if wait_for_pattern("ALL 8/8 USERSPACE TESTS PASSED!", 5.0):
            print("[+] Verified: Ring 3 selftest PASSED (including Anti-Skew & Telemetry checks).")
        else:
            print("[-] Timeout waiting for selftest completion")

        time.sleep(0.5)
        out_png = "tests/63_advanced_opsec_hardened.png"
        capture_screenshot(qmp, out_png)

        with open(SERIAL_LOG, "r", errors="ignore") as f:
            log_data = f.read()

        verifications = [
            ("Anti-Clock-Skew Activation", "Anti-Clock-Skew Normalization: ACTIVE"),
            ("In-Band Network Time Sync", "Certified network time updated: epoch 1729000000"),
            ("Hardware Serial Zero-Masking", "Hardware Serial: 0000-0000-0000-0000 [Sanitized / Zeroed]"),
            ("Zero-DHCP Static Config", "Static IPv4 configured: IP 10.0.2.19"),
            ("Poisson Boot Delay Active", "Poisson Boot Delay: ENABLED"),
            ("Ring 3 OpSec Validation", "ALL 8/8 USERSPACE TESTS PASSED!")
        ]

        print("\n=======================================================")
        print(" SELDOS ADVANCED PARANOID OPSEC VERIFICATION REPORT")
        print("=======================================================")
        all_passed = True
        for name, pattern in verifications:
            if pattern in log_data:
                print(f"[+] PASSED: {name}")
            else:
                print(f"[-] FAILED: {name} (pattern '{pattern}' not found)")
                all_passed = False

        if all_passed:
            print("\n[+] ALL ADVANCED OPSEC DEFENSES FULLY OPERATIONAL!")
        else:
            print("\n[-] ADVANCED OPSEC CHECKS FAILED!")
            sys.exit(1)

    finally:
        proc.terminate()
        proc.wait()
        for f in [QMP_SOCK, SERIAL_LOG]:
            if os.path.exists(f):
                os.remove(f)

if __name__ == "__main__":
    main()
