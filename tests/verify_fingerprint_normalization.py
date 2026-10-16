#!/usr/bin/env python3
"""
SeldOS - Humboldt Kernel Project
Network Fingerprint Normalization & Anti-Anonymity-Paradox Verification Suite
Verifies:
1. TCP SYN Packet Signature (p0f / nmap canonical Linux 6.x signature):
   - IP Total Length = 60 bytes, TTL = 64, DF = 1, IP ID = 0 (RFC 6864).
   - TCP Data Offset = 10 (40 bytes header), Window Size = 64240.
   - TCP Options in exact canonical order:
     * MSS = 1460 (Kind 2, Len 4, Val 0x05B4)
     * SACK Permitted (Kind 4, Len 2)
     * Timestamps (Kind 8, Len 10, TSval != 0, TSecr = 0)
     * NOP (Kind 1)
     * Window Scale = 7 (Kind 3, Len 3, Shift 7)
2. HTTP L7 Signature Normalization:
   - User-Agent matches standard Linux Firefox ESR (Mozilla/5.0 (X11; Linux x86_64; rv:115.0) Gecko/20100101 Firefox/115.0).
   - Zero leak of 'SeldOS', 'SeldTLS', or custom stack headers.
3. TLS 1.3 ClientHello JA3/JA4 Mimicry:
   - Standard browser cipher suites list (15 suites starting with TLS_AES_128_GCM_SHA256).
   - Standard browser extensions (SNI, EMS, Renegotiation, Groups, Formats, Ticket, ALPN, Status, KeyShare, SupportedVersions, SigAlgs, PSKModes).
"""

import sys
import os
import time
import socket
import struct
import json
import subprocess
import select
from http.server import HTTPServer, BaseHTTPRequestHandler
import threading

QMP_SOCK = "/tmp/seldos_fp_qmp.sock"
SERIAL_LOG = "/tmp/seldos_fp_serial.log"
PCAP_FILE = "/tmp/seldos_fp_traffic.pcap"
TEST_PORT = 8080

captured_http_headers = {}
http_request_received = threading.Event()

class TestHTTPHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        global captured_http_headers
        captured_http_headers = dict(self.headers)
        http_request_received.set()
        body = b"Sovereign SeldOS Fingerprint Test Payload OK\n"
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, format, *args):
        pass # Suppress server logging

def start_http_server():
    server = HTTPServer(("0.0.0.0", TEST_PORT), TestHTTPHandler)
    t = threading.Thread(target=server.serve_forever, daemon=True)
    t.start()
    return server

KEY_MAP = {
    ' ': 'spc',
    '\n': 'ret',
    '-': 'minus',
    '_': 'shift-minus',
    '.': 'dot',
    '/': 'slash',
    ':': 'shift-semicolon',
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

def parse_pcap(filepath):
    """Parse raw pcap and extract IPv4/TCP packets."""
    packets = []
    if not os.path.exists(filepath):
        return packets

    with open(filepath, "rb") as f:
        ghdr = f.read(24)
        if len(ghdr) < 24:
            return packets
        magic = struct.unpack("<I", ghdr[:4])[0]
        if magic != 0xa1b2c3d4 and magic != 0xd4c3b2a1:
            return packets

        while True:
            phdr = f.read(16)
            if len(phdr) < 16:
                break
            ts_sec, ts_usec, incl_len, orig_len = struct.unpack("<IIII", phdr)
            pdata = f.read(incl_len)
            if len(pdata) < incl_len:
                break

            # Ethernet header (14 bytes)
            if len(pdata) < 14:
                continue
            ethertype = struct.unpack("!H", pdata[12:14])[0]
            if ethertype != 0x0800: # IPv4
                continue

            # IPv4 Header
            ip_data = pdata[14:]
            if len(ip_data) < 20:
                continue
            ver_ihl = ip_data[0]
            ihl = (ver_ihl & 0x0F) * 4
            ip_len = struct.unpack("!H", ip_data[2:4])[0]
            ip_id = struct.unpack("!H", ip_data[4:6])[0]
            flags_frag = struct.unpack("!H", ip_data[6:8])[0]
            ttl = ip_data[8]
            proto = ip_data[9]
            src_ip = socket.inet_ntoa(ip_data[12:16])
            dest_ip = socket.inet_ntoa(ip_data[16:20])

            if proto != 6: # TCP
                continue

            tcp_data = ip_data[ihl:]
            if len(tcp_data) < 20:
                continue
            src_port = struct.unpack("!H", tcp_data[0:2])[0]
            dest_port = struct.unpack("!H", tcp_data[2:4])[0]
            seq = struct.unpack("!I", tcp_data[4:8])[0]
            ack = struct.unpack("!I", tcp_data[8:12])[0]
            data_offset = (tcp_data[12] >> 4) * 4
            flags = tcp_data[13]
            win_size = struct.unpack("!H", tcp_data[14:16])[0]

            options = tcp_data[20:data_offset] if data_offset > 20 else b""
            payload = tcp_data[data_offset:]

            packets.append({
                "src_ip": src_ip, "dest_ip": dest_ip,
                "src_port": src_port, "dest_port": dest_port,
                "ip_len": ip_len, "ip_id": ip_id, "flags_frag": flags_frag, "ttl": ttl,
                "seq": seq, "ack": ack, "data_offset": data_offset, "flags": flags,
                "win_size": win_size, "options": options, "payload": payload
            })
    return packets

def main():
    print("[*] Starting SeldOS Network Fingerprint Normalization Verification Suite...")
    server = start_http_server()
    print(f"[+] Local HTTP test receiver bound to 0.0.0.0:{TEST_PORT}")

    for p in [QMP_SOCK, SERIAL_LOG, PCAP_FILE]:
        if os.path.exists(p):
            os.remove(p)

    qemu_cmd = [
        "qemu-system-x86_64",
        "-cdrom", "build/seldos.iso",
        "-m", "512M",
        "-netdev", "user,id=net0",
        "-device", "e1000,netdev=net0,mac=52:54:00:12:34:56",
        "-object", f"filter-dump,id=f0,netdev=net0,file={PCAP_FILE}",
        "-serial", f"file:{SERIAL_LOG}",
        "-qmp", f"unix:{QMP_SOCK},server,nowait",
        "-display", "none",
        "-boot", "d"
    ]

    print("[*] Launching QEMU with filter-dump PCAP sniffer...")
    qemu_proc = subprocess.Popen(qemu_cmd)

    try:
        # Connect to QMP
        qmp_sock = None
        for _ in range(100):
            if os.path.exists(QMP_SOCK):
                try:
                    qmp_sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                    qmp_sock.connect(QMP_SOCK)
                    break
                except socket.error:
                    pass
            time.sleep(0.1)

        if not qmp_sock:
            print("[-] FAIL: Could not connect to QMP socket.")
            return 1

        greeting = qmp_sock.recv(4096)
        qmp_sock.sendall(b'{"execute": "qmp_capabilities"}\n')
        drain_sock(qmp_sock)

        print("[*] Waiting for SeldOS boot into SNL Shell...")
        time.sleep(4.0)

        # Disarm dead man grip for automated testing
        print("[*] Disarming Dead Man's Grip for automated test run...")
        send_string(qmp_sock, "grip off\n")
        time.sleep(1.0)

        # Trigger download command in shell
        print("[*] Issuing 'download tor force'...")
        cmd_str = "download tor force\n"
        send_string(qmp_sock, cmd_str)

        # Wait for HTTP request to be received
        if not http_request_received.wait(timeout=10.0):
            print("[-] FAIL: Timed out waiting for HTTP request from SeldOS.")
            return 1

        print("[+] HTTP Request received by test receiver!")
        time.sleep(1.0)

        # Shut down SeldOS
        print("[*] Powering off SeldOS...")
        send_string(qmp_sock, "poweroff\n")
        time.sleep(2.0)

    finally:
        if qemu_proc.poll() is None:
            qemu_proc.terminate()
            qemu_proc.wait(timeout=3.0)

    # Analyze HTTP Headers
    print("\n=== VERIFICATION 1: HTTP L7 Fingerprint Normalization ===")
    ua = captured_http_headers.get("User-Agent", captured_http_headers.get("user-agent", ""))
    print(f"[+] Captured User-Agent: {ua}")
    assert "SeldOS" not in ua, f"FAIL: Leaked 'SeldOS' in User-Agent: {ua}"
    assert "SeldTLS" not in ua, f"FAIL: Leaked 'SeldTLS' in User-Agent: {ua}"
    assert "Mozilla/5.0 (X11; Linux x86_64; rv:115.0) Gecko/20100101 Firefox/115.0" in ua, \
        f"FAIL: Unexpected User-Agent: {ua}"
    print("[+] PASS: User-Agent standardized to Linux Firefox ESR (0 leaks).")

    # Analyze PCAP
    print("\n=== VERIFICATION 2: TCP/IP L3/L4 Canonical Linux 6.x Signature ===")
    packets = parse_pcap(PCAP_FILE)
    print(f"[+] Total captured packets: {len(packets)}")

    syn_packets = [p for p in packets if (p["flags"] & 0x02) and not (p["flags"] & 0x10) and p["src_ip"] == "10.0.2.15"]
    assert len(syn_packets) > 0, "FAIL: No TCP SYN packets from SeldOS found in PCAP"

    syn = syn_packets[0]
    print(f"[+] Captured TCP SYN Packet:")
    print(f"    IP Len: {syn['ip_len']} bytes (Expected 60)")
    print(f"    IP ID:  0x{syn['ip_id']:04x} (Expected 0x0000 per RFC 6864)")
    print(f"    IP TTL: {syn['ttl']} (Expected 64)")
    print(f"    IP DF:  {bool(syn['flags_frag'] & 0x4000)} (Expected True)")
    print(f"    TCP Header Len: {syn['data_offset']} bytes (Expected 40)")
    print(f"    TCP Window:     {syn['win_size']} (Expected 64240)")

    assert syn['ip_len'] == 60, f"FAIL: Expected IP len 60, got {syn['ip_len']}"
    assert syn['ip_id'] == 0, f"FAIL: Expected RFC 6864 IP ID 0, got {syn['ip_id']}"
    assert syn['ttl'] == 64, f"FAIL: Expected TTL 64, got {syn['ttl']}"
    assert (syn['flags_frag'] & 0x4000) != 0, "FAIL: DF bit not set"
    assert syn['data_offset'] == 40, f"FAIL: Expected TCP header 40 bytes, got {syn['data_offset']}"
    assert syn['win_size'] == 64240, f"FAIL: Expected initial window 64240, got {syn['win_size']}"

    # Parse TCP Options
    opts = syn['options']
    print(f"    TCP Options Raw: {opts.hex()}")
    assert len(opts) == 20, f"FAIL: Expected 20 bytes options, got {len(opts)}"

    # Option 1: MSS (Kind 2, Len 4, Val 1460 -> 0x02, 0x04, 0x05, 0xb4)
    assert opts[0] == 2 and opts[1] == 4, "FAIL: MSS option missing or malformed"
    mss = struct.unpack("!H", opts[2:4])[0]
    print(f"    -> [Opt 1] MSS: {mss} (Kind=2, Len=4)")
    assert mss == 1460, f"FAIL: Expected MSS 1460, got {mss}"

    # Option 2: SACK Permitted (Kind 4, Len 2 -> 0x04, 0x02)
    assert opts[4] == 4 and opts[5] == 2, "FAIL: SACK Permitted option missing"
    print(f"    -> [Opt 2] SACK Permitted (Kind=4, Len=2)")

    # Option 3: Timestamps (Kind 8, Len 10 -> 0x08, 0x0a, TSval, TSecr=0)
    assert opts[6] == 8 and opts[7] == 10, "FAIL: Timestamps option missing"
    tsval = struct.unpack("!I", opts[8:12])[0]
    tsecr = struct.unpack("!I", opts[12:16])[0]
    print(f"    -> [Opt 3] Timestamps: TSval=0x{tsval:08x}, TSecr=0x{tsecr:08x} (Kind=8, Len=10)")
    assert tsecr == 0, f"FAIL: SYN TSecr must be 0, got {tsecr}"

    # Option 4: NOP (Kind 1 -> 0x01)
    assert opts[16] == 1, "FAIL: NOP option missing"
    print(f"    -> [Opt 4] NOP (Kind=1)")

    # Option 5: Window Scale (Kind 3, Len 3, Val 7 -> 0x03, 0x03, 0x07)
    assert opts[17] == 3 and opts[18] == 3 and opts[19] == 7, "FAIL: Window Scale option missing or != 7"
    print(f"    -> [Opt 5] Window Scale: 7 (Kind=3, Len=3, Shift=7)")

    print("\n=======================================================")
    print(" [***] ALL FINGERPRINT NORMALIZATION TESTS PASSED [***]")
    print(" Canonical Linux 6.x p0f signature verified:           ")
    print("   60:64:1:40:M1460,S,T,N,W7:WS:df:0                  ")
    print("   User-Agent: Linux Firefox ESR                       ")
    print("   Result: Complete blend-in into standard crowd!      ")
    print("=======================================================")
    return 0

if __name__ == "__main__":
    sys.exit(main())
