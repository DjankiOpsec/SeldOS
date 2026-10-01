#!/usr/bin/env python3
import socket
import subprocess
import time
import os
import json
import select
from PIL import Image

QMP_SOCK = "/tmp/seldos_qmp.sock"

KEY_MAP = {
    ' ': 'spc',
    '\n': 'ret',
    '-': 'minus',
    '_': 'shift-minus',
    '.': 'dot',
    '/': 'slash',
    '\\': 'backslash',
    '=': 'equal',
    '+': 'shift-equal',
    '0': '0', '1': '1', '2': '2', '3': '3', '4': '4',
    '5': '5', '6': '6', '7': '7', '8': '8', '9': '9',
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
        time.sleep(0.035)

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

def main():
    if os.path.exists(QMP_SOCK):
        os.remove(QMP_SOCK)

    cmd = [
        "qemu-system-x86_64",
        "-cdrom", "build/seldos.iso",
        "-drive", "file=build/disk.img,format=raw",
        "-display", "none",
        "-qmp", f"unix:{QMP_SOCK},server,nowait"
    ]

    print("[*] Launching QEMU instance for SNL screenshots...")
    proc = subprocess.Popen(cmd)
    time.sleep(1.2)

    try:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.connect(QMP_SOCK)
        drain_sock(s)
        s.sendall(json.dumps({"execute": "qmp_capabilities"}).encode() + b"\n")
        time.sleep(0.1)
        drain_sock(s)

        # 1. Boot, Selftests validation (during the 1.5s kernel selftest pause)
        print("[*] Capturing 01_kernel_boot_selftest.png...")
        capture_screenshot(s, "01_kernel_boot_selftest.png")

        # 2. Wait for init boot & shell banner
        print("[*] Waiting for /bin/init boot...")
        time.sleep(2.0)
        print("[*] Capturing 02_snl_init_boot.png...")
        capture_screenshot(s, "02_snl_init_boot.png")

        # 3. Interactive Shell & Utilities: uname and ps
        print("[*] Capturing 03_snl_shell_commands.png...")
        send_string(s, "clear\n")
        time.sleep(0.5)
        send_string(s, "uname\nps\n")
        time.sleep(1.0)
        capture_screenshot(s, "03_snl_shell_commands.png")

        # 4. Storage & Directory Index: ls and cat
        print("[*] Capturing 04_snl_storage_ls.png...")
        send_string(s, "clear\n")
        time.sleep(0.5)
        send_string(s, "ls\ncat readme.txt\n")
        time.sleep(1.0)
        capture_screenshot(s, "04_snl_storage_ls.png")

        # 5. Ring 3 Userspace Runtime Self-Test Suite
        print("[*] Capturing 05_snl_ring3_selftest.png...")
        send_string(s, "clear\n")
        time.sleep(0.5)
        send_string(s, "selftest\n")
        time.sleep(1.2)
        capture_screenshot(s, "05_snl_ring3_selftest.png")

        s.close()
    finally:
        proc.terminate()
        proc.wait()
        if os.path.exists(QMP_SOCK):
            os.remove(QMP_SOCK)

    print("[+] All 5 SNL screenshots successfully generated!")

if __name__ == "__main__":
    main()
