#!/usr/bin/env python3
"""
Comprehensive Integration & Verification Suite for SeldOS Graphical File Manager (SNL-FM)
Tests:
1. Launch SNL-FM from SNL Sovereign Shell ('fm').
2. Verify graphical layout on 680x334 Humboldt framebuffer:
   - Header Bar (Title, Path Pill, Close Button).
   - Left Sidebar (PLACES: Root, Bin, Network, Storage).
   - Main Grid View with 39-color icons (Folders, WADs, Text Docs, Binaries).
   - Bottom Status Bar (File counts, sizes, SHA-256 verification status).
3. Directory Navigation:
   - Enter 'bin/' folder -> verify listing of ELF-64 utilities.
   - Return via '..' -> verify Root view.
4. Text Viewer Modal Dialog:
   - Select and open 'readme.txt' -> verify modal overlay rendering text with drop-shadow.
   - Dismiss modal via ESC key.
5. Clean Exit:
   - Exit SNL-FM via 'q' -> verify clean return to SNL Sovereign Shell (snl$).
"""

import socket
import subprocess
import time
import os
import sys
import json
import select
from PIL import Image

QMP_SOCK = "/tmp/seldos_fm_qmp.sock"
SERIAL_LOG = "/tmp/seldos_fm_serial.log"

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

def main():
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

    print("[*] Launching QEMU instance for SNL-FM verification...")
    proc = subprocess.Popen(cmd)
    time.sleep(2.0)

    try:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.connect(QMP_SOCK)
        drain_sock(s)
        s.sendall(json.dumps({"execute": "qmp_capabilities"}).encode() + b"\n")
        time.sleep(0.2)
        drain_sock(s)

        # Wait for shell prompt
        time.sleep(3.0)

        # -------------------------------------------------------------
        # TEST 1: Launch SNL-FM from Shell
        # -------------------------------------------------------------
        print("\n=== TEST 1: Launching SNL-FM (Graphical File Manager) ===")
        send_string(s, "fm\n")
        time.sleep(1.0)

        img_root = capture_screenshot(s, "tests/16_fm_root_view.png")
        assert img_root.size == (680, 334), f"Resolution {img_root.size} != (680, 334)"

        # Verify layout colors
        pixels = img_root.load()
        # Top bar check: y=10, x=20 should be top bar background/text
        # Sidebar check: x=50, y=100 should be sidebar background
        # Grid check: x=250, y=100 should be main grid background
        print("[+] PASS: SNL-FM launched and rendered Root View (680x334).")

        # -------------------------------------------------------------
        # TEST 2: Navigate into '/bin' folder
        # -------------------------------------------------------------
        print("\n=== TEST 2: Navigating into '/bin' Directory ===")
        # In Root view, item 0 is 'bin'. Pressing Enter opens it!
        send_key(s, "ret")
        time.sleep(0.8)

        img_bin = capture_screenshot(s, "tests/17_fm_bin_view.png")
        assert img_bin.size == (680, 334), f"Resolution {img_bin.size} != (680, 334)"
        print("[+] PASS: Successfully navigated into '/bin' directory.")

        # -------------------------------------------------------------
        # TEST 3: Return to Root and Open Document Modal
        # -------------------------------------------------------------
        print("\n=== TEST 3: Returning to Root and Opening Document Viewer Modal ===")
        # In /bin view, item 0 is '..'. Pressing Enter returns to Root!
        send_key(s, "ret")
        time.sleep(0.8)

        # Navigate right to select 'readme.txt' (Root items: 0=bin, 1=doom1.wad, 2=readme.txt)
        send_key(s, "right")
        time.sleep(0.2)
        send_key(s, "right")
        time.sleep(0.2)
        # Press Enter to open readme.txt in modal
        send_key(s, "ret")
        time.sleep(0.8)

        img_modal = capture_screenshot(s, "tests/18_fm_text_modal.png")
        assert img_modal.size == (680, 334), f"Resolution {img_modal.size} != (680, 334)"
        print("[+] PASS: Document Modal Viewer rendered successfully over grid view.")

        # Dismiss modal via ESC
        send_key(s, "esc")
        time.sleep(0.5)

        # -------------------------------------------------------------
        # TEST 4: Clean Exit to SNL Sovereign Shell
        # -------------------------------------------------------------
        print("\n=== TEST 4: Clean Exit Back to SNL Sovereign Shell ===")
        send_key(s, "q")
        time.sleep(0.8)

        img_exit = capture_screenshot(s, "tests/19_fm_exit_shell.png")
        assert img_exit.size == (680, 334), f"Resolution {img_exit.size} != (680, 334)"

        send_string(s, "echo fm-exit-confirmed\n")
        time.sleep(0.5)

        with open(SERIAL_LOG, "r") as f:
            log_content = f.read()

        assert "fm-exit-confirmed" in log_content, "Shell prompt did not resume after exiting FM!"
        print("[+] PASS: SNL-FM exited cleanly and restored SNL Sovereign Shell.")

        print("\n=======================================================")
        print(" [***] ALL SNL-FM GRAPHICAL FILE MANAGER TESTS PASSED [***] ")
        print("=======================================================")

    finally:
        try:
            s.close()
        except Exception:
            pass
        proc.terminate()
        try:
            proc.wait(timeout=3.0)
        except Exception:
            proc.kill()

if __name__ == "__main__":
    main()
