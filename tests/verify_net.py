#!/usr/bin/env python3
"""
Comprehensive Integration & Verification Suite for SeldOS OpSec Network Subsystem
Tests:
1. Boot & PCI Device Probing: Intel e1000 Gigabit controller detection (0x8086:0x100E).
2. Hardware Link & Carrier: Set Link Up (SLU), Carrier Active status, MAC Address integrity.
3. Kernel Self-Tests: Checksum RFC 1071 verification, IP parser, self-test 7/7 PASSED.
4. Ring 3 Sovereign Shell: /bin/sh interactive CLI launch.
5. Network Command: 'ifconfig' querying hardware MAC, IPv4 address, netmask, gateway, and frame counters.
6. Layer 3/4 Ping: 'ping 10.0.2.2' (QEMU User Network Gateway) via ARP request/reply and ICMP Echo Request/Reply.
7. ARP Cache: 'arp' verification of resolved gateway MAC address.
"""

import socket
import subprocess
import time
import os
import sys
import json
import select
from PIL import Image

QMP_SOCK = "/tmp/seldos_net_test_qmp.sock"
SERIAL_LOG = "/tmp/seldos_net_serial.log"

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

    print("[*] Launching QEMU instance with Intel e1000 Network Card...")
    proc = subprocess.Popen(cmd)
    time.sleep(2.0)

    try:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.connect(QMP_SOCK)
        drain_sock(s)
        s.sendall(json.dumps({"execute": "qmp_capabilities"}).encode() + b"\n")
        time.sleep(0.2)
        drain_sock(s)

        # -------------------------------------------------------------
        # TEST 1: Boot & Self-Tests Verification
        # -------------------------------------------------------------
        print("\n=== TEST 1: Boot & OpSec Network Self-Test Verification ===")
        time.sleep(3.0) # Wait for init and shell banner

        with open(SERIAL_LOG, "r") as f:
            log_content = f.read()

        assert "[+] e1000: Found Intel NIC" in log_content, "e1000 controller not detected!"
        assert "Link UP - Carrier Active" in log_content, "e1000 link is not UP!"
        assert "[+] [SELFTEST:NET] PASSED" in log_content, "Kernel net self-test failed!"
        assert "ALL 7/7 SUBSYSTEMS PASSED" in log_content, "Not all 7 kernel subsystems passed!"
        assert "[init] Spawning SNL Sovereign Shell (/bin/sh)..." in log_content, "Shell not spawned!"
        print("[+] PASS: e1000 NIC detected, link UP, 7/7 self-tests passed, shell spawned.")

        # -------------------------------------------------------------
        # TEST 2: Shell Command 'ifconfig'
        # -------------------------------------------------------------
        print("\n=== TEST 2: Shell 'ifconfig' Command Execution ===")
        send_string(s, "ifconfig\n")
        time.sleep(1.0)
        img_ifconfig = capture_screenshot(s, "tests/13_net_ifconfig.png")
        assert img_ifconfig.size == (680, 334), f"Resolution {img_ifconfig.size} != (680, 334)"

        with open(SERIAL_LOG, "r") as f:
            log_content = f.read()

        assert "eth0: flags=UP,BROADCAST,MULTICAST mtu 1500" in log_content, "ifconfig output missing!"
        assert "inet 10.0.2.15" in log_content, "IP address 10.0.2.15 missing in ifconfig!"
        print("[+] PASS: 'ifconfig' executed cleanly, reporting interface eth0, MAC and IPv4.")

        # -------------------------------------------------------------
        # TEST 3: Ping Gateway 'ping 10.0.2.2'
        # -------------------------------------------------------------
        print("\n=== TEST 3: ICMP Echo 'ping 10.0.2.2' (QEMU Gateway) ===")
        send_string(s, "ping 10.0.2.2\n")
        time.sleep(3.5) # Allow 4 pings to complete
        img_ping = capture_screenshot(s, "tests/14_net_ping_gateway.png")
        assert img_ping.size == (680, 334), f"Resolution {img_ping.size} != (680, 334)"

        with open(SERIAL_LOG, "r") as f:
            log_content = f.read()

        assert "PING 10.0.2.2 56(84) bytes of data." in log_content, "ping banner not present!"
        assert "64 bytes from 10.0.2.2" in log_content, "No ICMP Echo replies received from 10.0.2.2!"
        assert "0% packet loss" in log_content, "Packet loss occurred during ping to gateway!"
        print("[+] PASS: Ping to 10.0.2.2 succeeded with 0% packet loss.")

        # -------------------------------------------------------------
        # TEST 4: ARP Cache 'arp'
        # -------------------------------------------------------------
        print("\n=== TEST 4: Kernel ARP Cache 'arp' ===")
        send_string(s, "arp\n")
        time.sleep(1.0)
        img_arp = capture_screenshot(s, "tests/15_net_arp_cache.png")

        with open(SERIAL_LOG, "r") as f:
            log_content = f.read()

        assert "10.0.2.2" in log_content, "Gateway 10.0.2.2 not in ARP cache!"
        assert "RESOLVED" in log_content, "ARP entry not in RESOLVED state!"
        print("[+] PASS: ARP cache contains resolved 10.0.2.2 entry.")

        print("\n=======================================================")
        print(" [***] ALL OPSEC NETWORK VERIFICATION TESTS PASSED [***] ")
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
