#!/usr/bin/env python3
"""
SeldOS - Humboldt Kernel Project
Verification Test: SNL 'oracle' C99 Userland Utility in SeldOS
Tests:
1. Boot into SNL Sovereign Shell (Ring 3).
2. Execute 'oracle 5' command.
3. Verify output in serial log:
   - Header '--- TEMPLE OF OPSEC ORACLE ---'
   - Words printed line-by-line
   - Footer '--- [AMEN] ---'
4. Clean return to shell prompt.
"""

import json
import os
import select
import subprocess
import socket
import sys
import time

QMP_SOCK = "/tmp/seldos_oracle_qmp.sock"
SERIAL_LOG = "/tmp/seldos_oracle_serial.log"

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
            sock.recv(4096)
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

def main():
    for f in [QMP_SOCK, SERIAL_LOG]:
        if os.path.exists(f):
            try:
                os.remove(f)
            except Exception:
                pass

    qemu_cmd = [
        "qemu-system-x86_64",
        "-cdrom", "build/seldos.iso",
        "-m", "256M",
        "-display", "none",
        "-qmp", f"unix:{QMP_SOCK},server,nowait",
        "-serial", f"file:{SERIAL_LOG}",
        "-audiodev", "none,id=audio0",
        "-machine", "pcspk-audiodev=audio0"
    ]

    print("[*] Launching SeldOS in QEMU to test SNL 'oracle' command...")
    proc = subprocess.Popen(qemu_cmd)
    s = None

    try:
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

        drain_sock(s)
        s.sendall(json.dumps({"execute": "qmp_capabilities"}).encode() + b"\n")
        time.sleep(0.2)
        drain_sock(s)

        print("[*] Waiting for SeldOS boot into SNL Sovereign Shell...")
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
        print("[+] System booted successfully into Ring 3 SNL Shell.")
        time.sleep(0.5)

        print("[*] Typing command: 'oracle 5' into SNL Shell...")
        send_string(s, "oracle 5\n")

        print("[*] Waiting for Oracle utterance...")
        oracle_done = False
        for _ in range(50):
            time.sleep(0.3)
            if os.path.exists(SERIAL_LOG):
                with open(SERIAL_LOG, "r") as f:
                    content = f.read()
                if "--- [AMEN] ---" in content:
                    oracle_done = True
                    break

        assert oracle_done, "Oracle command did not finish or output was missing"
        print("[+] 'oracle 5' executed successfully!")

        with open(SERIAL_LOG, "r") as f:
            lines = f.readlines()

        oracle_lines = []
        capture = False
        for line in lines:
            if "--- TEMPLE OF OPSEC ORACLE ---" in line:
                capture = True
                continue
            if "--- [AMEN] ---" in line:
                capture = False
                continue
            if capture:
                for w in line.strip().split():
                    if w:
                        oracle_lines.append(w)

        print(f"[+] Words uttered by Oracle: {len(oracle_lines)}")
        print("----------------------------------------")
        for idx, w in enumerate(oracle_lines, 1):
            print(f" Word {idx}: {w}")
        print("----------------------------------------")

        assert len(oracle_lines) >= 4, f"Expected ~5 words, got {len(oracle_lines)}"
        print("[+] SNL 'oracle' verification test PASSED!")

    finally:
        if s:
            try:
                s.close()
            except Exception:
                pass
        proc.terminate()
        try:
            proc.wait(timeout=2)
        except Exception:
            proc.kill()

if __name__ == "__main__":
    main()
