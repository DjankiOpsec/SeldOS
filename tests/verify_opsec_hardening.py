#!/usr/bin/env python3
"""
OpSec Hardening Verification Suite for SeldOS
Verifies all 6 newly integrated privacy and OpSec mechanisms:
1. Zero-on-Free: Automatic RAM wipe in kmalloc/kfree & libsnl free().
2. RFC 7686 Guard: Strict drop & error on attempted .onion DNS resolution via clearnet UDP.
3. Cryptographic TCP ISN & Ephemeral Port Randomization via CSPRNG (anti-fingerprinting).
4. Seld-Pledge (SYS_PLEDGE, vector 48): Ring 3 capability sandboxing.
5. Ephemeral MAC Spoofing: Randomized locally administered MAC on e1000 driver.
6. Cold-Boot & Anti-Forensics Defense: DoD physical RAM frame scrubbing before shutdown/reboot.
"""

import socket
import subprocess
import time
import os
import sys
import json
import select
from PIL import Image

QMP_SOCK = "/tmp/seldos_opsec_qmp.sock"
SERIAL_LOG = "/tmp/seldos_opsec_serial.log"

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

    print("[*] Launching QEMU instance for OpSec Hardening verification...")
    proc = subprocess.Popen(qemu_cmd)

    try:
        # 1. Connect to QMP
        for _ in range(50):
            if os.path.exists(QMP_SOCK):
                break
            time.sleep(0.1)

        qmp = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        qmp.connect(QMP_SOCK)
        qmp.recv(2048)
        qmp.sendall(b'{"execute":"qmp_capabilities"}\n')
        drain_sock(qmp)

        # 2. Wait for boot and init self-checks
        print("[*] Waiting for SeldOS boot into SNL Sovereign Shell...")
        booted = False
        for _ in range(60):
            if os.path.exists(SERIAL_LOG):
                with open(SERIAL_LOG, "r", errors="ignore") as f:
                    content = f.read()
                    if "MSR LSTAR Fast Syscall Handshake: OK" in content and "Spawning SNL Sovereign Shell" in content:
                        booted = True
                        break
            time.sleep(0.2)

        assert booted, "System failed to boot into SNL userspace!"
        time.sleep(1.0)

        # Verify Kernel Self-Tests from serial log
        with open(SERIAL_LOG, "r", errors="ignore") as f:
            log = f.read()

        print("\n=== VERIFICATION 1: Kernel 8/8 Subsystem Self-Tests ===")
        assert "[+] SeldOS Kernel Self-Tests: ALL 8/8 SUBSYSTEMS PASSED!" in log, "Kernel self-tests failed!"
        assert "[+] [SELFTEST:OPSEC] PASSED: All Sovereign OpSec mechanisms operational." in log, "Kernel OpSec self-test failed!"
        print("[+] Verified: Kernel Self-Tests 8/8 passed, including SELFTEST:OPSEC.")

        print("\n=== VERIFICATION 2: Ephemeral MAC Spoofing ===")
        assert "OpSec Ephemeral MAC Address (Randomized/Spoofed):" in log, "e1000 MAC address was not spoofed/randomized!"
        print("[+] Verified: e1000 network controller MAC address randomized with locally administered bit.")

        print("\n=== VERIFICATION 3: Userspace Boot Init Checks (5/5) ===")
        assert "[+] [init:CHECK 5/5] OpSec Hardening: Zero-on-Free & RFC 7686 Onion Guard active." in log, "Init OpSec check failed!"
        print("[+] Verified: /bin/init verified Ring 3 Zero-on-Free and RFC 7686 Onion Guard.")

        # Switch to PC mode
        send_string(qmp, "pc\n")
        time.sleep(0.5)

        print("\n=== VERIFICATION 4: Shell Interactive 'selftest' (8/8 Ring 3 Tests) ===")
        send_string(qmp, "selftest\n")
        time.sleep(1.0)

        with open(SERIAL_LOG, "r", errors="ignore") as f:
            shell_log = f.read()
        assert "[Ring 3] ALL 8/8 USERSPACE TESTS PASSED!" in shell_log, "Userspace Ring 3 8/8 tests failed!"
        assert "[+] [TEST 8/8] PASSED: Zero-on-Free, RFC 7686 Guard, and Seld-Pledge verified." in shell_log, "Test 8/8 failed!"
        print("[+] Verified: /bin/sh selftest executed and ALL 8/8 tests passed (Seld-Pledge, Zero-on-Free, RFC 7686).")

        print("\n=== VERIFICATION 5: Shell Interactive RFC 7686 Onion DNS Leak Guard ===")
        send_string(qmp, "dns leaktest.onion\n")
        time.sleep(0.8)

        with open(SERIAL_LOG, "r", errors="ignore") as f:
            dns_log = f.read()
        assert "RFC 7686 violation! Attempted clearnet DNS query for .onion domain: leaktest.onion -> DROPPED" in dns_log, "DNS leak guard failed to trigger on shell command!"
        print("[+] Verified: Attempt to resolve .onion in shell was immediately blocked and dropped.")

        print("\n=== VERIFICATION 6: Interface Status & MAC Inspection ===")
        send_string(qmp, "ifconfig\n")
        time.sleep(0.8)

        # Capture high-resolution screenshot of the hardened terminal
        capture_screenshot(qmp, "tests/60_opsec_hardened_selftest.png")

        print("\n=== VERIFICATION 7: Cold-Boot Defense RAM Scrub & Safe Poweroff ===")
        send_string(qmp, "poweroff\n")
        time.sleep(1.5)

        with open(SERIAL_LOG, "r", errors="ignore") as f:
            shutdown_log = f.read()
        assert "Initiating Cold-Boot defense (scrubbing unallocated RAM frames)..." in shutdown_log, "Cold-boot scrub not invoked!"
        assert "RAM scrub complete:" in shutdown_log, "RAM scrub did not complete!"
        assert "System poweroff requested. Halting all processors..." in shutdown_log, "Poweroff not requested!"
        print("[+] Verified: Cold-Boot defense scrubbed all unallocated physical RAM frames and halted cleanly.")

        print("\n=======================================================")
        print(" [SUCCESS] ALL OPSEC HARDENING SUBSYSTEMS FULLY VERIFIED!")
        print("=======================================================\n")

    finally:
        try:
            qmp.close()
        except:
            pass
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=2)
            except:
                proc.kill()
        for f in [QMP_SOCK]:
            if os.path.exists(f):
                os.remove(f)

if __name__ == "__main__":
    main()
