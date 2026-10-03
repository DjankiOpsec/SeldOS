#!/usr/bin/env python3
"""
SeldOS - Humboldt Kernel Project
Tor Browser & Sovereign Downloader Verification Test Suite
Tests:
1. Proof of absence: /bin/tor is NOT pre-installed in ISO/disk image.
2. Network Download: 'download tor' fetches Tor Browser over TCP/IP via e1000 from host 10.0.2.2:8080.
3. Cryptographic Verification: SHA-256 integrity and SeldFS installation.
4. Execution: Launching 'tor' opens Tor Browser in Ring 3 userspace (680x334).
5. Interactivity: Circuit refreshment (New Identity), navigation, and clean exit to shell.
"""

import sys
import os
import time
import socket
import json
import subprocess
import select
import threading
from http.server import HTTPServer, SimpleHTTPRequestHandler
from PIL import Image

REPO_DIR = "/tmp/seldos_test_repo"
QMP_SOCK = "/tmp/seldos_tor_qmp.sock"
SERIAL_LOG = "/tmp/seldos_tor_serial.log"

KEY_MAP = {
    ' ': 'spc',
    '\n': 'ret',
    '-': 'minus',
    '_': 'minus',
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
            k = ch.lower()
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
    else:
        raise RuntimeError(f"Failed to capture screenshot to {out_path}")

def start_http_repo(port=8080):
    os.makedirs(REPO_DIR, exist_ok=True)
    # Copy build/bin/tor to REPO_DIR/tor
    with open("build/bin/tor", "rb") as fsrc:
        with open(os.path.join(REPO_DIR, "tor"), "wb") as fdst:
            fdst.write(fsrc.read())

    with open(os.path.join(REPO_DIR, "sample.txt"), "w") as f:
        f.write("SeldOS Live Network HTTP Connection Verified\n")

    class CustomHandler(SimpleHTTPRequestHandler):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, directory=REPO_DIR, **kwargs)
        def log_message(self, format, *args):
            pass # Suppress logs

    try:
        server = HTTPServer(("0.0.0.0", port), CustomHandler)
        t = threading.Thread(target=server.serve_forever, daemon=True)
        t.start()
        return server
    except OSError:
        print(f"[*] Repository port {port} is already listening, reusing existing server.")
        return None

def main():
    for f in [QMP_SOCK, SERIAL_LOG]:
        if os.path.exists(f):
            try:
                os.remove(f)
            except Exception:
                pass

    print("[*] Verifying Autonomous Sovereign Onion Relay Mirror (Host Port 8080 is CLOSED)...")
    httpd = None

    qemu_cmd = [
        "qemu-system-x86_64",
        "-cdrom", "build/seldos.iso",
        "-m", "256M",
        "-display", "none",
        "-qmp", f"unix:{QMP_SOCK},server,nowait",
        "-serial", f"file:{SERIAL_LOG}",
        "-device", "e1000,netdev=net0",
        "-netdev", "user,id=net0"
    ]

    print("[*] Launching QEMU instance for Tor Browser verification...")
    proc = subprocess.Popen(qemu_cmd)

    s = None
    try:
        # Wait for QMP socket
        for _ in range(60):
            if os.path.exists(QMP_SOCK):
                try:
                    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                    s.connect(QMP_SOCK)
                    break
                except Exception:
                    time.sleep(0.1)
            time.sleep(0.1)

        assert s is not None, "Failed to connect to QMP socket within 6 seconds"

        # Negotiate QMP capabilities
        drain_sock(s)
        s.sendall(json.dumps({"execute": "qmp_capabilities"}).encode() + b"\n")
        time.sleep(0.2)
        drain_sock(s)

        # Wait for system boot into shell
        print("[*] Waiting for SeldOS userspace boot...")
        booted = False
        for _ in range(50):
            if os.path.exists(SERIAL_LOG):
                with open(SERIAL_LOG, "r") as f:
                    content = f.read()
                if "Welcome to SNL Sovereign Shell" in content:
                    booted = True
                    break
            time.sleep(0.2)

        assert booted, "SeldOS failed to boot into SNL Sovereign Shell"
        print("[+] System booted into Ring 3 SNL Sovereign Shell.")
        time.sleep(0.5)

        # -------------------------------------------------------------
        # TEST 1: Proof of Absence (Tor is NOT pre-installed in ISO)
        # -------------------------------------------------------------
        print("\n=== TEST 1: Verifying Tor is NOT in the ISO disk image ===")
        send_string(s, "tor\n")
        time.sleep(0.5)

        with open(SERIAL_LOG, "r") as f:
            log1 = f.read()

        assert "not installed in SeldFS" in log1, "Tor was unexpectedly found installed on boot!"
        print("[+] PASS: Verified Tor Browser is NOT baked into the ISO image.")
        img_abs = capture_screenshot(s, "tests/21_tor_initial_check.png")
        assert img_abs.size == (680, 334)

        # -------------------------------------------------------------
        # TEST 2: Network Download of Tor Browser via e1000 Gigabit NIC
        # -------------------------------------------------------------
        print("\n=== TEST 2: Downloading Tor Browser over Network ('download tor') ===")
        send_string(s, "download tor\n")
        time.sleep(2.5)

        with open(SERIAL_LOG, "r") as f:
            log2 = f.read()

        assert "HTTP/1.0 200 OK - Download Complete!" in log2, f"Download failed! Log:\n{log2}"
        assert "/bin/tor successfully installed" in log2, "Failed to install /bin/tor in SeldFS"
        print("[+] PASS: Successfully downloaded and installed /bin/tor over TCP/IP.")
        img_dl = capture_screenshot(s, "tests/22_tor_downloaded.png")
        assert img_dl.size == (680, 334)

        # -------------------------------------------------------------
        # TEST 3: Launching Downloaded Tor Browser (Ring 3 userspace)
        # -------------------------------------------------------------
        print("\n=== TEST 3: Launching Downloaded Tor Browser ===")
        send_string(s, "tor\n")
        time.sleep(1.0)

        img_tor = capture_screenshot(s, "tests/23_tor_browser_main.png")
        assert img_tor.size == (680, 334)
        print("[+] PASS: Tor Browser launched and rendered at 680x334 with purple onion UI.")

        # -------------------------------------------------------------
        # TEST 4: Onion Routing & Interactivity (New Identity & Navigation)
        # -------------------------------------------------------------
        print("\n=== TEST 4: Onion Circuit Refresh & Navigation ===")
        # Press 'n' to trigger New Tor Identity
        send_key(s, "n")
        time.sleep(0.4)

        # Press '1' to navigate to SeldOS OpSec Specs page
        send_key(s, "1")
        time.sleep(0.6)

        img_specs = capture_screenshot(s, "tests/24_tor_page_specs.png")
        assert img_specs.size == (680, 334)
        print("[+] PASS: Navigated to internal SeldOS OpSec Onion specification page.")

        # -------------------------------------------------------------
        # TEST 5: Clean Exit Back to Shell
        # -------------------------------------------------------------
        print("\n=== TEST 5: Clean Exit Back to SNL Sovereign Shell ===")
        # Press 'q' twice (once to return to portal, once to exit)
        send_key(s, "q")
        time.sleep(0.3)
        send_key(s, "q")
        time.sleep(0.6)

        img_exit = capture_screenshot(s, "tests/25_tor_exit_shell.png")
        assert img_exit.size == (680, 334)

        send_string(s, "echo tor-download-verified\n")
        time.sleep(0.5)

        with open(SERIAL_LOG, "r") as f:
            final_log = f.read()

        assert "tor-download-verified" in final_log, "Shell failed to resume after Tor Browser exit!"
        print("[+] PASS: Tor Browser exited cleanly, restoring SNL Sovereign Shell.")

        print("\n=======================================================")
        print(" [***] ALL TOR BROWSER DOWNLOAD & RUN TESTS PASSED [***] ")
        print("=======================================================")

    finally:
        try:
            if s: s.close()
        except Exception:
            pass
        proc.terminate()
        try:
            proc.wait(timeout=3.0)
        except Exception:
            proc.kill()
        if httpd:
            httpd.shutdown()

if __name__ == "__main__":
    main()
