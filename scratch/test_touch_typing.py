import subprocess
import time
import json
import socket
import os
from PIL import Image

QMP_SOCK = "/tmp/qmp_test_touch.sock"

def drain_sock(sock):
    sock.setblocking(False)
    try:
        while True:
            data = sock.recv(4096)
            if not data:
                break
    except BlockingIOError:
        pass
    sock.setblocking(True)

def send_qmp(sock, cmd, args=None):
    payload = {"execute": cmd}
    if args:
        payload["arguments"] = args
    sock.sendall(json.dumps(payload).encode() + b"\n")
    time.sleep(0.05)
    drain_sock(sock)

def send_rel(sock, dx, dy):
    evs = []
    if dx != 0:
        evs.append({"type": "rel", "data": {"axis": "x", "value": dx}})
    if dy != 0:
        evs.append({"type": "rel", "data": {"axis": "y", "value": dy}})
    sock.sendall(json.dumps({"execute": "input-send-event", "arguments": {"events": evs}}).encode() + b"\n")
    time.sleep(0.08)
    drain_sock(sock)

def send_btn(sock, down):
    evs = [{"type": "btn", "data": {"down": down, "button": "left"}}]
    sock.sendall(json.dumps({"execute": "input-send-event", "arguments": {"events": evs}}).encode() + b"\n")
    time.sleep(0.08)
    drain_sock(sock)

def capture(sock, path):
    ppm = "/tmp/scr_dump.ppm"
    if os.path.exists(ppm):
        os.remove(ppm)
    sock.sendall(json.dumps({"execute": "screendump", "arguments": {"filename": ppm}}).encode() + b"\n")
    time.sleep(0.3)
    drain_sock(sock)
    if os.path.exists(ppm):
        img = Image.open(ppm)
        img.save(path)
        os.remove(ppm)
        return img
    return None

def main():
    if os.path.exists(QMP_SOCK):
        os.remove(QMP_SOCK)

    cmd = [
        "qemu-system-x86_64",
        "-cdrom", "build/seldos.iso",
        "-drive", "file=build/disk.img,format=raw",
        "-m", "512M",
        "-display", "none",
        "-serial", "stdio",
        "-qmp", f"unix:{QMP_SOCK},server,nowait"
    ]
    proc = subprocess.Popen(cmd)
    try:
        time.sleep(1.5)
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.connect(QMP_SOCK)
        s.recv(4096)
        send_qmp(s, "qmp_capabilities")
        time.sleep(2.5) # Wait for boot to shell

        # Initial mouse: (320, 240). Target: (120, 340) for [ LS ] button.
        # dx total = 120 - 320 = -200 (4 steps of -50)
        # dy total on screen = +(340 - 240) = +100 (4 steps of +25)
        print("[*] Moving mouse in 4 steps towards [ LS ] button (120, 340)...")
        for i in range(4):
            send_rel(s, -50, 25)
            time.sleep(0.05)

        time.sleep(0.2)
        print("[*] Clicking [ LS ] button (press down)...")
        send_btn(s, True)
        time.sleep(0.15)
        print("[*] Releasing [ LS ] button...")
        send_btn(s, False)
        time.sleep(1.5)

        img = capture(s, "tests/08_touch_ls_verified.png")
        print("[+] Captured screenshot to tests/08_touch_ls_verified.png")
    finally:
        proc.terminate()
        proc.wait()

if __name__ == "__main__":
    main()
