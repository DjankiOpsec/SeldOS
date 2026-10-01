import subprocess
import time
import json
import socket
import os
import select
from PIL import Image
import numpy as np

QMP_SOCK = "/tmp/qmp_test_pc.sock"

def drain_sock(sock):
    while True:
        r, _, _ = select.select([sock], [], [], 0.0)
        if r:
            data = sock.recv(4096)
            if not data: break
        else:
            break

def send_qmp(sock, cmd, args=None):
    payload = {"execute": cmd}
    if args:
        payload["arguments"] = args
    sock.sendall(json.dumps(payload).encode() + b"\n")
    time.sleep(0.05)
    drain_sock(sock)

KEY_MAP = {'\n': 'ret', ' ': 'spc', '-': 'minus', '/': 'slash', '.': 'dot'}

def send_key(sock, key):
    keys = [{"type": "qcode", "data": key}]
    payload = {"execute": "send-key", "arguments": {"keys": keys}}
    sock.sendall(json.dumps(payload).encode() + b"\n")
    drain_sock(sock)

def send_string(sock, s):
    for ch in s:
        k = KEY_MAP.get(ch, ch.lower())
        send_key(sock, k)
        time.sleep(0.04)

def capture(sock, path):
    ppm = "/tmp/dump_pc.ppm"
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
        "-qmp", f"unix:{QMP_SOCK},server,nowait"
    ]
    proc = subprocess.Popen(cmd)
    try:
        time.sleep(1.5)
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.connect(QMP_SOCK)
        s.recv(4096)
        send_qmp(s, "qmp_capabilities")
        time.sleep(2.5) # Wait for shell

        # 1. Capture initial shell with mobile HUD
        img_mobile = capture(s, "tests/09_shell_mobile_hud.png")
        print("[+] Captured mobile HUD screenshot: tests/09_shell_mobile_hud.png")

        # 2. Type 'pc\n'
        print("[*] Executing 'pc' command...")
        send_string(s, "pc\n")
        time.sleep(1.0)

        # 3. Capture PC mode screenshot
        img_pc = capture(s, "tests/10_shell_pc_mode.png")
        print("[+] Captured PC mode screenshot: tests/10_shell_pc_mode.png")

        arr_pc = np.array(img_pc)
        # Check lines 325..479: in PC mode, the bottom touch area must be pure black
        bottom_area = arr_pc[325:479, 0:640]
        max_val = np.max(bottom_area)
        print(f"[*] Bottom area maximum pixel value in PC mode: {max_val}")
        assert max_val == 0, f"Bottom area not completely cleared in PC mode! Max val: {max_val}"
        print("[+] PASS: Mobile HUD completely removed and screen cleared in PC mode!")

        # 4. Now launch doom in PC mode
        print("[*] Launching doom from PC mode...")
        send_string(s, "doom\n")
        time.sleep(2.5)

        img_doom_pc = capture(s, "tests/11_doom_pc_mode.png")
        print("[+] Captured doom in PC mode screenshot: tests/11_doom_pc_mode.png")

        arr_doom = np.array(img_doom_pc)
        # Verify D-Pad area (x: 10..195, y: 280..455) has NO virtual cyan border buttons
        # In mobile doom, buttons have color [0, 255, 204]
        dpad_sample = arr_doom[280:330, 70:130]
        cyan_count = np.sum((dpad_sample[:, :, 1] > 200) & (dpad_sample[:, :, 0] < 50))
        print(f"[*] D-Pad cyan pixels in PC mode: {cyan_count}")
        assert cyan_count == 0, "Touch D-Pad buttons still visible in PC mode DOOM!"
        print("[+] PASS: DOOM started cleanly in PC mode with NO touch overlays!")

    finally:
        proc.terminate()
        proc.wait()

if __name__ == "__main__":
    main()
