#!/usr/bin/env python3
"""
SeldOS - Humboldt Kernel Project
Sovereign Optical & Acoustic Air-Gap Host Receiver (Arch Linux Utility)

Receives data exfiltrated from SeldOS via physical air-gap diodes:
1. Payload Channel: Optical Data Diode (Webcam captures animated QR code chunks from SeldOS monitor).
2. Key Channel: Acoustic Air-Gap (Microphone demodulates Bell 202 1200/2200 Hz FSK tones from SeldOS speaker).
3. Cryptography: AES-128-GCM AEAD decryption, HKDF-SHA256, plaintext SHA-256 integrity verification.

Zero BadUSB risk. Zero bidirectional network feedback. Strict fail-closed OpSec.
GPLv3 Licensed.
"""

import sys
import os
import time
import argparse
import struct
import base64
import hashlib
import binascii
import threading
import subprocess
import shutil
import json
import select
import numpy as np

# Ensure line-buffered output
try:
    sys.stdout.reconfigure(line_buffering=True)
except Exception:
    pass

# Cryptography library (AES-GCM)
try:
    from cryptography.hazmat.primitives.ciphers.aead import AESGCM
except ImportError:
    print("[-] Fatal: python-cryptography required. Run: sudo pacman -S python-cryptography")
    sys.exit(1)

# OpenCV for computer vision & QR decoding
try:
    import cv2
    HAS_OPENCV = True
except ImportError:
    HAS_OPENCV = False

import ctypes

class ZBarScanner:
    """
    Direct C-binding to libzbar.so for high-speed, sub-millisecond QR code decoding.
    Handles anti-aliased, small, or scaled QR codes on high-res displays where OpenCV fails.
    """
    def __init__(self):
        self.available = False
        try:
            self.lib = ctypes.CDLL("libzbar.so")
            self.lib.zbar_image_scanner_create.restype = ctypes.c_void_p
            self.lib.zbar_image_create.restype = ctypes.c_void_p
            self.lib.zbar_image_destroy.argtypes = [ctypes.c_void_p]
            self.lib.zbar_image_set_format.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
            self.lib.zbar_image_set_size.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint]
            self.lib.zbar_image_set_data.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_ulong, ctypes.c_void_p]
            self.lib.zbar_scan_image.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
            self.lib.zbar_scan_image.restype = ctypes.c_int
            self.lib.zbar_image_first_symbol.argtypes = [ctypes.c_void_p]
            self.lib.zbar_image_first_symbol.restype = ctypes.c_void_p
            self.lib.zbar_symbol_get_data.argtypes = [ctypes.c_void_p]
            self.lib.zbar_symbol_get_data.restype = ctypes.c_char_p
            self.lib.zbar_symbol_next.argtypes = [ctypes.c_void_p]
            self.lib.zbar_symbol_next.restype = ctypes.c_void_p
            self.scanner = self.lib.zbar_image_scanner_create()
            self.available = True
        except Exception:
            self.available = False

    def scan_gray(self, gray: np.ndarray) -> list[str]:
        if not self.available or gray is None:
            return []
        h, w = gray.shape[:2]
        fourcc_y800 = int.from_bytes(b"Y800", "little")
        zimg = self.lib.zbar_image_create()
        self.lib.zbar_image_set_format(zimg, fourcc_y800)
        self.lib.zbar_image_set_size(zimg, w, h)
        raw = gray.tobytes()
        self.lib.zbar_image_set_data(zimg, raw, len(raw), None)
        n = self.lib.zbar_scan_image(self.scanner, zimg)
        res = []
        if n > 0:
            sym = self.lib.zbar_image_first_symbol(zimg)
            while sym:
                data = self.lib.zbar_symbol_get_data(sym)
                if data:
                    res.append(data.decode("utf-8", errors="ignore"))
                sym = self.lib.zbar_symbol_next(sym)
        self.lib.zbar_image_destroy(zimg)
        return res


class UniversalQRDetector:
    """
    Multi-stage resilient QR detector:
    1. Direct full-frame scan with libzbar (5ms).
    2. Contour-based white box extraction & crop scan (detects SeldOS 0xFFFFFFFF canvas).
    3. OpenCV QRCodeDetector fallback.
    """
    def __init__(self):
        self.zbar = ZBarScanner()
        self.cv_det = cv2.QRCodeDetector() if HAS_OPENCV else None

    def detect_all(self, frame: np.ndarray) -> list[str]:
        if frame is None:
            return []

        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY) if frame.ndim == 3 else frame

        # Stage 1: Full-frame ZBar
        if self.zbar.available:
            codes = self.zbar.scan_gray(gray)
            if codes:
                return codes

        # Stage 2: Contour box cropping (SeldOS white square canvas)
        try:
            thresh = cv2.threshold(gray, 220, 255, cv2.THRESH_BINARY)[1]
            contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            for cnt in contours:
                x, y, w, h = cv2.boundingRect(cnt)
                if w >= 40 and h >= 40 and 0.60 <= (w / float(h)) <= 1.65:
                    pad = 16
                    y0, y1 = max(0, y - pad), min(gray.shape[0], y + h + pad)
                    x0, x1 = max(0, x - pad), min(gray.shape[1], x + w + pad)
                    crop = gray[y0:y1, x0:x1]

                    if self.zbar.available:
                        c_codes = self.zbar.scan_gray(crop)
                        if c_codes:
                            return c_codes

                    if self.cv_det:
                        val, _, _ = self.cv_det.detectAndDecode(crop)
                        if val:
                            return [val]
        except Exception:
            pass

        # Stage 3: OpenCV full frame fallback
        if self.cv_det:
            val, _, _ = self.cv_det.detectAndDecode(frame)
            if val:
                return [val]

        return []

# Protocol Constants
FSK_FREQ_SPACE = 1200.0     # Bit 0
FSK_FREQ_MARK = 2200.0      # Bit 1
FSK_FREQ_PREAMBLE = 1800.0  # Training / Alert Tone
FSK_SYNC_WORD = bytes([0x5E, 0x1D]) # SeldOS Humboldt sync magic

DIODE_MAGIC = 0x53454C44    # "SELD" in ASCII
ENVELOPE_HDR_FMT = "<IBI32sI12s16s32s"
ENVELOPE_HDR_SIZE = struct.calcsize(ENVELOPE_HDR_FMT) # 105 bytes


def crc32_ieee(data: bytes) -> int:
    """Standard IEEE 802.3 CRC32 (polynomial 0xEDB88320)."""
    return binascii.crc32(data) & 0xFFFFFFFF


def parse_40_line_key_sheet(sheet_text: str) -> tuple[bytes, int]:
    """
    Parses SeldOS 40-line key sheet text.
    Extracts hex tokens, computes HKDF-SHA256 master key (16 bytes) and Key ID (uint32).
    """
    clean_hex = []
    for line in sheet_text.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        # Strip label e.g. 'L01:' or 'LINE 01:'
        if ":" in line:
            line = line.split(":", 1)[1]
        # Keep only hex chars
        tokens = [c for c in line if c.upper() in "0123456789ABCDEF"]
        clean_hex.extend(tokens)

    key_material = "".join(clean_hex).upper().encode("utf-8")
    if len(key_material) < 32:
        raise ValueError(f"Insufficient key material: extracted {len(key_material)} chars")

    # Key ID = first 4 bytes of SHA256(key_material)
    sheet_sha = hashlib.sha256(key_material).digest()
    key_id = struct.unpack(">I", sheet_sha[:4])[0]

    # HKDF-Extract: Salt = "SeldOS-AirGap-KeySheet-v1"
    salt = b"SeldOS-AirGap-KeySheet-v1"
    prk = hashlib.sha256()
    # HMAC-SHA256(salt, IKM)
    import hmac
    prk_val = hmac.new(salt, key_material, hashlib.sha256).digest()

    # HKDF-Expand: Info = "AES-128-GCM-Payload-Key"
    info = b"AES-128-GCM-Payload-Key"
    okm = hmac.new(prk_val, info + b"\x01", hashlib.sha256).digest()
    master_key = okm[:16]

    return master_key, key_id


def parse_key_input(key_str: str) -> tuple[bytes, int]:
    """
    Parses key material provided as either:
    1. Standard SeldOS 40-line key sheet text.
    2. Direct 32-character hex master key (16 bytes).
    """
    key_str = key_str.strip()
    if not key_str:
        raise ValueError("Key input cannot be empty")

    # If short string with exactly 32 hex chars, treat as raw master key
    clean_hex = "".join(c for c in key_str if c.upper() in "0123456789ABCDEF")
    if len(clean_hex) == 32 and len(key_str.splitlines()) <= 2:
        master_key = bytes.fromhex(clean_hex)
        key_id = struct.unpack(">I", hashlib.sha256(master_key).digest()[:4])[0]
        return master_key, key_id

    # Otherwise parse as 40-line key sheet
    return parse_40_line_key_sheet(key_str)


class FSKDemodulator:
    """
    Synchronous Bell 202 FSK Demodulator with quadrature matched filter,
    float bit stepping, and multi-rate candidate drift tolerance.
    Recovers 31-byte SeldOS acoustic master key packet from PCM audio.
    """
    def __init__(self, sample_rate: int = 44100, bit_dur_sec: float = 0.030):
        self.sample_rate = sample_rate
        self.bit_dur_sec = bit_dur_sec
        self.samples_per_bit = int(sample_rate * bit_dur_sec)

        # 18 ms correlation window: sharp frequency discrimination without cross-symbol blur
        self.win_dur = min(0.018, bit_dur_sec)
        self.N_win = int(sample_rate * self.win_dur)
        t = np.arange(self.N_win) / sample_rate
        self.cos_s = np.cos(2 * np.pi * FSK_FREQ_SPACE * t).astype(np.float32)
        self.sin_s = np.sin(2 * np.pi * FSK_FREQ_SPACE * t).astype(np.float32)
        self.cos_m = np.cos(2 * np.pi * FSK_FREQ_MARK * t).astype(np.float32)
        self.sin_m = np.sin(2 * np.pi * FSK_FREQ_MARK * t).astype(np.float32)

    def demodulate(self, audio_data: np.ndarray) -> dict | None:
        """
        Demodulates audio samples into key packet dictionary:
        { 'session_id': uint32, 'key_id': uint32, 'master_key': bytes, 'crc32': uint32 }
        Returns None if sync or CRC invalid.
        """
        if len(audio_data) < self.N_win * 30:
            return None

        # Subtract DC bias to remove microphone/soundcard offset
        mean_val = float(np.mean(audio_data))
        if abs(mean_val) > 1e-5:
            audio_data = audio_data - mean_val

        # Step size: 2 ms
        step = max(1, int(self.sample_rate * 0.002))

        # Fast quadrature correlation
        try:
            from scipy.signal import fftconvolve
            im = fftconvolve(audio_data, self.cos_m[::-1], mode='valid')
            qm = fftconvolve(audio_data, self.sin_m[::-1], mode='valid')
            is_ = fftconvolve(audio_data, self.cos_s[::-1], mode='valid')
            qs = fftconvolve(audio_data, self.sin_s[::-1], mode='valid')
            d = (im**2 + qm**2) - (is_**2 + qs**2)
            diffs = d[::step]
        except Exception:
            diffs = []
            for i in range(0, len(audio_data) - self.N_win, step):
                w = audio_data[i:i + self.N_win]
                es = np.sum(w * self.cos_s)**2 + np.sum(w * self.sin_s)**2
                em = np.sum(w * self.cos_m)**2 + np.sum(w * self.sin_m)**2
                diffs.append(em - es)
            diffs = np.array(diffs, dtype=np.float32)

        if len(diffs) < 260:
            return None

        # Sync bits pattern: 0x5E, 0x1D (16 bits MSB first)
        sync_bits = []
        for b in [0x5E, 0x1D]:
            for bit_i in range(8):
                sync_bits.append((b >> (7 - bit_i)) & 1)

        pkt_len_bytes = 31 # Total key packet length
        total_data_bits = pkt_len_bytes * 8

        # Prioritized candidate durations around nominal bit duration
        nominal_dur = self.bit_dur_sec
        offsets = [0.0]
        for delta in [0.0005, -0.0005, 0.001, -0.001, 0.0015, -0.0015, 0.002, -0.002, 0.0025, -0.0025, 0.003, -0.003, 0.004, -0.004, 0.005, -0.005]:
            d_cand = nominal_dur + delta
            if 0.020 <= d_cand <= 0.040:
                offsets.append(delta)

        for delta in offsets:
            cand_dur = nominal_dur + delta
            spb_float = (cand_dur * self.sample_rate) / step
            if spb_float <= 1.0:
                continue

            # Sweep candidate phase offsets (16 subdivisions per bit)
            num_phases = 16
            for phase_step in np.linspace(0, spb_float, num_phases, endpoint=False):
                max_k = int((len(diffs) - phase_step) / spb_float)
                if max_k < total_data_bits + len(sync_bits):
                    continue

                idxs = np.round(phase_step + np.arange(max_k) * spb_float).astype(int)
                idxs = idxs[idxs < len(diffs)]
                bits = [1 if diffs[idx] > 0 else 0 for idx in idxs]

                # Search for sync bits
                for i in range(len(bits) - len(sync_bits) - total_data_bits + len(sync_bits)):
                    if bits[i:i + len(sync_bits)] == sync_bits:
                        candidate_bits = bits[i:i + total_data_bits]
                        if len(candidate_bits) < total_data_bits:
                            continue

                        raw_bytes = bytearray()
                        for byte_idx in range(pkt_len_bytes):
                            b_val = 0
                            for bit_idx in range(8):
                                b_val = (b_val << 1) | candidate_bits[byte_idx * 8 + bit_idx]
                            raw_bytes.append(b_val)

                        raw_bytes = bytes(raw_bytes)
                        calc_crc = crc32_ieee(raw_bytes[:27])
                        rx_crc = struct.unpack("<I", raw_bytes[27:31])[0]

                        if calc_crc == rx_crc:
                            sync = raw_bytes[:2]
                            pkt_type = raw_bytes[2]
                            session_id, key_id = struct.unpack("<II", raw_bytes[3:11])
                            master_key = raw_bytes[11:27]
                            return {
                                "session_id": session_id,
                                "key_id": key_id,
                                "master_key": master_key,
                                "crc32": rx_crc
                            }

        return None


class OpticalDiodeCollector:
    """
    Collects, reassembles and verifies chunked animated QR code frames:
    Protocol format: SELD1:<seq>/<total>:<session_id_hex>:<filename>:<chunk_base64>:<crc32_hex>
    """
    def __init__(self):
        self.sessions = {} # session_id -> { total: int, filename: str, chunks: {seq: bytes} }

    def process_qr_payload(self, text: str) -> tuple[bool, str]:
        """
        Parses QR frame text string.
        Returns (is_complete, status_message).
        """
        if not text or not text.startswith("SELD1:"):
            return False, "Not a SELD1 QR frame"

        parts = text.split(":")
        if len(parts) < 6:
            return False, "Malformed SELD1 frame structure"

        # parts: ["SELD1", "<seq>/<total>", "<session_id>", "<filename>", "<chunk_b64>", "<crc32>"]
        seq_str = parts[1]
        session_hex = parts[2]
        filename = parts[3]
        b64_data = parts[4]
        crc_hex = parts[5]

        try:
            seq, total = map(int, seq_str.split("/"))
            session_id = int(session_hex, 16)
            expected_crc = int(crc_hex, 16)
            chunk_bytes = base64.b64decode(b64_data)
        except Exception as e:
            return False, f"Frame decode error: {e}"

        # Verify chunk CRC32
        if crc32_ieee(chunk_bytes) != expected_crc:
            return False, f"CRC32 mismatch on frame {seq}/{total}"

        if session_id not in self.sessions:
            self.sessions[session_id] = {
                "total": total,
                "filename": filename,
                "chunks": {}
            }

        s_entry = self.sessions[session_id]
        s_entry["chunks"][seq] = chunk_bytes

        got = len(s_entry["chunks"])
        tot = s_entry["total"]
        pct = (got * 100) // tot

        is_complete = (got >= tot)
        msg = f"Session 0x{session_id:08X} [{filename}]: Frame {seq}/{total} captured ({got}/{tot} - {pct}%)"
        return is_complete, msg

    def get_reassembled_envelope(self, session_id: int) -> bytes | None:
        if session_id not in self.sessions:
            return None
        s_entry = self.sessions[session_id]
        if len(s_entry["chunks"]) < s_entry["total"]:
            return None

        # Reassemble ordered chunks
        full_envelope = bytearray()
        for seq in range(1, s_entry["total"] + 1):
            full_envelope.extend(s_entry["chunks"][seq])
        return bytes(full_envelope)


def decrypt_envelope(envelope_data: bytes, master_key: bytes) -> tuple[str, bytes]:
    """
    Decrypts SeldOS envelope with AES-128-GCM and verifies SHA-256 integrity.
    Returns (filename, plaintext).
    Raises ValueError on tampering or AEAD authentication failure.
    """
    if len(envelope_data) < ENVELOPE_HDR_SIZE:
        raise ValueError("Envelope too short")

    hdr_bytes = envelope_data[:ENVELOPE_HDR_SIZE]
    (magic, version, session_id, fn_bytes, pt_len, iv, tag, pt_sha) = struct.unpack(ENVELOPE_HDR_FMT, hdr_bytes)

    if magic != DIODE_MAGIC:
        raise ValueError(f"Invalid magic: {hex(magic)} (expected {hex(DIODE_MAGIC)})")
    if version != 1:
        raise ValueError(f"Unsupported envelope version: {version}")

    raw_filename = fn_bytes.decode("utf-8", errors="replace").rstrip("\x00")
    filename = os.path.basename(raw_filename.replace("\\", "/"))
    if not filename or filename in (".", ".."):
        filename = f"payload_0x{session_id:08X}.bin"
    expected_size = ENVELOPE_HDR_SIZE + pt_len
    if len(envelope_data) != expected_size:
        raise ValueError(f"Envelope size mismatch: {len(envelope_data)} != {expected_size}")

    ciphertext = envelope_data[ENVELOPE_HDR_SIZE:]

    # AAD is header from magic up to pt_len
    # offset of iv is 4 + 1 + 4 + 32 + 4 = 45
    aad_len = 4 + 1 + 4 + 32 + 4
    aad = hdr_bytes[:aad_len]

    # In cryptography library AESGCM, tag is appended to ciphertext
    ct_with_tag = ciphertext + tag

    aesgcm = AESGCM(master_key)
    try:
        plaintext = aesgcm.decrypt(iv, ct_with_tag, aad)
    except Exception as e:
        raise ValueError(f"AES-128-GCM Authentication Failed (AEAD tag mismatch / tampering): {e}")

    # Verify Plaintext SHA-256
    computed_sha = hashlib.sha256(plaintext).digest()
    if computed_sha != pt_sha:
        raise ValueError("Plaintext SHA-256 mismatch! Integrity verification failed.")

    return filename, plaintext


class ScreenGrabber:
    """
    High-performance desktop screen frame capture on Linux.
    Supports:
    1. Wayland (Hyprland / Sway) via grim (raw PPM stream to memory in ~20ms).
    2. Cross-platform via PIL.ImageGrab.
    3. X11 via ImageMagick 'import'.
    """
    def __init__(self, region: tuple[int, int, int, int] | None = None):
        self.region = region  # (x, y, w, h)
        self.has_grim = shutil.which("grim") is not None
        self.has_import = shutil.which("import") is not None

    def grab(self) -> np.ndarray | None:
        # 1. Native Wayland grim
        if self.has_grim:
            cmd = ["grim"]
            if self.region:
                cmd.extend(["-g", f"{self.region[0]},{self.region[1]} {self.region[2]}x{self.region[3]}"])
            cmd.extend(["-t", "ppm", "-"])
            try:
                p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, check=True)
                nparr = np.frombuffer(p.stdout, np.uint8)
                return cv2.imdecode(nparr, cv2.IMREAD_COLOR)
            except Exception:
                pass

        # 2. PIL.ImageGrab fallback
        try:
            from PIL import ImageGrab
            bbox = None
            if self.region:
                bbox = (self.region[0], self.region[1],
                        self.region[0] + self.region[2], self.region[1] + self.region[3])
            img = ImageGrab.grab(bbox=bbox)
            return cv2.cvtColor(np.array(img), cv2.COLOR_RGB2BGR)
        except Exception:
            pass

        # 3. ImageMagick import fallback
        if self.has_import:
            cmd = ["import", "-window", "root", "ppm:-"]
            try:
                p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, check=True)
                nparr = np.frombuffer(p.stdout, np.uint8)
                return cv2.imdecode(nparr, cv2.IMREAD_COLOR)
            except Exception:
                pass

        return None


class AudioCaptureThread(threading.Thread):
    """
    Background worker capturing live audio:
    - 'monitor' mode: captures headphones / desktop audio loopback (sound of QEMU or desktop playback).
    - 'mic' mode: captures physical microphone.
    Runs Bell 202 FSK demodulator on rolling buffer.
    """
    def __init__(self, demodulator: FSKDemodulator, on_key_callback, source_type: str = "monitor"):
        super().__init__(daemon=True)
        self.demodulator = demodulator
        self.on_key_callback = on_key_callback
        self.source_type = source_type
        self.running = True
        self.proc = None
        self.source_desc = ""
        self.last_rms = 0.0

    def find_recorder(self) -> tuple[list[str] | None, str]:
        if self.source_type == "monitor":
            # Detect active monitor source in PulseAudio / PipeWire
            monitor_dev = "@DEFAULT_MONITOR@"
            if shutil.which("pactl"):
                try:
                    res = subprocess.run(["pactl", "list", "sources", "short"],
                                         stdout=subprocess.PIPE, text=True, timeout=1)
                    for line in res.stdout.splitlines():
                        parts = line.split("\t")
                        if len(parts) >= 2 and ".monitor" in parts[1]:
                            monitor_dev = parts[1]
                            if "RUNNING" in line:
                                break
                except Exception:
                    pass

            if shutil.which("parec"):
                return ["parec", "-d", monitor_dev, "--format=s16le", "--rate=44100", "--channels=1"], f"Headphones / Desktop audio loopback ({monitor_dev})"
            elif shutil.which("pw-record"):
                return ["pw-record", "--rate=44100", "--channels=1", "--format=s16", "-"], f"PipeWire loopback (pw-record)"
            elif shutil.which("ffmpeg"):
                return ["ffmpeg", "-loglevel", "quiet", "-f", "pulse", "-i", monitor_dev, "-f", "s16le", "-ar", "44100", "-ac", "1", "-"], f"ffmpeg pulse monitor ({monitor_dev})"
        else:
            # Physical microphone
            if shutil.which("pw-record"):
                return ["pw-record", "--rate=44100", "--channels=1", "--format=s16", "-"], "Microphone (pw-record)"
            elif shutil.which("parecord"):
                return ["parecord", "--rate=44100", "--channels=1", "--format=s16le", "--raw"], "Microphone (parecord)"
            elif shutil.which("arecord"):
                return ["arecord", "-q", "-t", "raw", "-f", "S16_LE", "-r", "44100", "-c", "1"], "Microphone (arecord)"
            elif shutil.which("ffmpeg"):
                return ["ffmpeg", "-loglevel", "quiet", "-f", "pulse", "-i", "default", "-f", "s16le", "-ar", "44100", "-ac", "1", "-"], "Microphone (ffmpeg)"

        return None, "None"

    def stop(self):
        self.running = False
        if self.proc and self.proc.poll() is None:
            try:
                self.proc.terminate()
                self.proc.wait(timeout=0.5)
            except Exception:
                try:
                    self.proc.kill()
                except Exception:
                    pass

    def run(self):
        cmd, desc = self.find_recorder()
        self.source_desc = desc
        if not cmd:
            print(f"[-] Warning: No audio capture tool available for {self.source_type} mode.")
            return

        try:
            self.proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        except Exception as e:
            print(f"[-] Failed to start audio recorder {cmd[0]}: {e}")
            return

        sample_rate = self.demodulator.sample_rate
        # Rolling buffer: 90 seconds of mono 16-bit audio (prevents discarding early beeps)
        max_samples = sample_rate * 90
        audio_buffer = np.zeros(0, dtype=np.float32)

        chunk_size = int(sample_rate * 0.25) * 2  # 250ms chunks (11025 samples * 2 bytes)
        last_demod_attempt = 0.0

        while self.running and self.proc and self.proc.poll() is None:
            try:
                raw = self.proc.stdout.read(chunk_size)
            except Exception:
                break
            if not raw:
                break

            # Align to 16-bit boundaries
            if len(raw) % 2 != 0:
                raw = raw[:len(raw) - 1]
            if len(raw) == 0:
                continue

            samples = np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32768.0
            self.last_rms = float(np.sqrt(np.mean(samples**2)))
            audio_buffer = np.concatenate([audio_buffer, samples])
            if len(audio_buffer) > max_samples:
                audio_buffer = audio_buffer[-max_samples:]

            # Attempt demodulation once buffer has >= 7.5s (demodulate at most once per sec)
            now_t = time.time()
            if len(audio_buffer) >= int(sample_rate * 7.5) and (now_t - last_demod_attempt >= 1.0):
                last_demod_attempt = now_t
                res = self.demodulator.demodulate(audio_buffer)
                if res:
                    self.on_key_callback(res)
                    break

        self.stop()


def check_hyprland_windows() -> tuple[int | None, list[dict]]:
    """
    Checks Hyprland workspace environment.
    Alerts the user if SeldOS VM window is located on an inactive workspace.
    Returns (active_workspace_id, list_of_vm_clients).
    """
    if not shutil.which("hyprctl"):
        return None, []

    try:
        active_raw = subprocess.check_output(["hyprctl", "-j", "activeworkspace"], stderr=subprocess.DEVNULL)
        active_info = json.loads(active_raw)
        active_ws = active_info.get("id")

        clients_raw = subprocess.check_output(["hyprctl", "-j", "clients"], stderr=subprocess.DEVNULL)
        clients = json.loads(clients_raw)

        vm_clients = []
        for c in clients:
            cl = (c.get("class") or "").lower()
            title = (c.get("title") or "").lower()
            if any(k in cl or k in title for k in ["virtualbox", "qemu", "bochs", "vmware", "seldos"]):
                vm_clients.append(c)

        for vm in vm_clients:
            vm_ws = vm.get("workspace", {}).get("id")
            vm_title = vm.get("title", "Unknown")
            vm_class = vm.get("class", "Unknown")
            vm_pid = vm.get("pid", 0)

            if vm_ws is not None and active_ws is not None:
                if vm_ws != active_ws:
                    print("\n" + "!" * 71)
                    print(" [!] HYPRLAND WAYLAND WORKSPACE WARNING:")
                    print(f" [!] SeldOS VM window '{vm_title}' (class: {vm_class}) is on Workspace {vm_ws}.")
                    print(f" [!] Your active screen is Workspace {active_ws}.")
                    print(" [!] Under Wayland, screen capture ONLY sees the active visible workspace!")
                    print(f" [!] -> Move VM to Workspace {active_ws} or run in another terminal:")
                    print(f"        hyprctl dispatch movetoworkspacesilent {active_ws},pid:{vm_pid}")
                    print("!" * 71 + "\n")
                else:
                    at = vm.get("at", [0, 0])
                    size = vm.get("size", [0, 0])
                    print(f"[+] SeldOS VM detected on active Workspace {active_ws} ({vm_class} at {at[0]},{at[1]} {size[0]}x{size[1]})")

        return active_ws, vm_clients
    except Exception:
        return None, []


def main():
    parser = argparse.ArgumentParser(
        description="SeldOS Sovereign Air-Gap Host Receiver (Arch Linux Utility)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  1. Desktop Screen & Headphones Audio (Standard QEMU / Desktop Mode):
     seld_diode_receiver.py --screen --audio --output ./received_files
     # Or simply:
     seld_diode_receiver.py --output ./received_files

  2. Standalone acoustic listener on headphones / desktop output:
     seld_diode_receiver.py --audio
     # Or force microphone:
     seld_diode_receiver.py --audio --mic

  3. Crop screen to specific QEMU window region (X,Y,W,H):
     seld_diode_receiver.py --screen --region 100,200,800,600 --audio

  4. Physical webcam and microphone (Physical Hardware Air-Gap):
     seld_diode_receiver.py --webcam 0 --audio --mic

  5. Decrypt from pre-scanned QR images & audio file:
     seld_diode_receiver.py --qr-file ./qr_frames/ --audio-file ./key.wav

  6. Run automated self-test of both channels:
     seld_diode_receiver.py --test
        """
    )
    parser.add_argument("--screen", action="store_true", help="Capture screen directly (QEMU window / desktop) via grim/X11")
    parser.add_argument("--region", type=str, default=None, help="Crop region X,Y,W,H for screen capture (e.g. 100,200,800,600)")
    parser.add_argument("--timeout", type=int, default=60, help="Screen/camera capture timeout in seconds (default: 60, 0 for infinite)")
    parser.add_argument("--webcam", type=int, default=None, help="Physical webcam device ID (e.g. 0)")
    parser.add_argument("--audio", action="store_true", help="Enable live audio capture for acoustic key (defaults to headphones/desktop sound)")
    parser.add_argument("--no-audio", action="store_true", help="Disable acoustic key listening")
    parser.add_argument("--headphones", "--desktop-audio", action="store_true", dest="headphones", help="Capture sound directly from headphones/desktop audio loopback (@DEFAULT_MONITOR@)")
    parser.add_argument("--mic", action="store_true", help="Capture sound from physical microphone instead of headphones")
    parser.add_argument("--audio-source", choices=["monitor", "mic"], default=None, help="Audio input mode: 'monitor' (headphones) or 'mic' (microphone)")
    parser.add_argument("--audio-file", type=str, default=None, help="Input WAV file for offline FSK demodulation")
    parser.add_argument("--qr-file", type=str, default=None, help="Input image file or directory with QR frames")
    parser.add_argument("--key-file", type=str, default=None, help="Local 40-line key sheet path (verification/fallback)")
    parser.add_argument("--key-text", type=str, default=None, help="Direct 40-line key sheet text or 32-char hex key")
    parser.add_argument("--output", type=str, default="./received_payload", help="Output file/dir for decrypted payload")
    parser.add_argument("--test", action="store_true", help="Run comprehensive automated test suite")
    parser.add_argument("--gui", action="store_true", help="Display OpenCV monitor window")

    args = parser.parse_args()

    print("=======================================================================")
    print(" SeldOS Sovereign Air-Gap Host Receiver v0.1 (Arch Linux)")
    print(" Zero BadUSB / Unidirectional Optical Diode & Acoustic Air-Gap Receiver")
    print("=======================================================================")

    if len(sys.argv) == 1:
        parser.print_help()
        sys.exit(0)

    if args.test:
        run_self_test()
        sys.exit(0)

    has_action = (args.screen or args.webcam is not None or args.audio or
                  args.headphones or args.mic or args.audio_file or
                  args.qr_file or args.key_file or args.key_text)
    if not has_action and not args.output:
        parser.print_help()
        sys.exit(0)

    # Determine audio source mode
    if args.no_audio:
        audio_enabled = False
    else:
        audio_enabled = args.audio or args.headphones or args.mic or (not args.audio_file and not args.qr_file)

    if args.mic or args.audio_source == "mic":
        audio_src_type = "mic"
    else:
        audio_src_type = "monitor"  # Default to headphones/desktop monitor loopback!

    # Determine optical mode:
    if args.webcam is not None:
        screen_mode = False
    elif args.qr_file:
        screen_mode = False
    elif args.screen:
        screen_mode = True
    elif (args.audio or args.headphones or args.mic) and not args.output and not args.screen:
        # Pure audio key listener
        screen_mode = False
    else:
        screen_mode = True

    # State
    demodulator = FSKDemodulator()
    collector = OpticalDiodeCollector()

    recovered_key_info = None
    key_lock = threading.Lock()

    def on_key_found(key_dict):
        nonlocal recovered_key_info
        with key_lock:
            recovered_key_info = key_dict
            print("\n" + "=" * 71)
            print("[+] ACOUSTIC AIR-GAP CHANNEL: MASTER KEY LOCKED!")
            print(f"    Session ID : 0x{key_dict['session_id']:08X}")
            print(f"    Key ID     : 0x{key_dict['key_id']:08X}")
            print(f"    CRC32      : 0x{key_dict['crc32']:08X} (Verified)")
            print(f"    Master Key : {key_dict['master_key'].hex().upper()}")
            print("=" * 71 + "\n")
        try:
            cache_dir = os.path.expanduser("~/.cache/seld-airgap")
            os.makedirs(cache_dir, exist_ok=True)
            with open(os.path.join(cache_dir, "last_key.txt"), "w") as f:
                f.write(f"SESSION_ID=0x{key_dict['session_id']:08X}\n")
                f.write(f"KEY_ID=0x{key_dict['key_id']:08X}\n")
                f.write(f"MASTER_KEY={key_dict['master_key'].hex().upper()}\n")
        except Exception:
            pass

    audio_thread = None
    try:
        # 1. Parse local key if provided (--key-text or --key-file)
        fallback_key = None
        if args.key_text:
            try:
                m_key, k_id = parse_key_input(args.key_text)
                fallback_key = m_key
                print(f"[+] Loaded direct key material (Key ID: 0x{k_id:08X})")
            except Exception as e:
                print(f"[-] Warning: Failed to parse --key-text: {e}")

        if not fallback_key and args.key_file and os.path.exists(args.key_file):
            with open(args.key_file, "r") as f:
                sheet_content = f.read()
            try:
                m_key, k_id = parse_key_input(sheet_content)
                fallback_key = m_key
                print(f"[+] Loaded local key file: {args.key_file} (Key ID: 0x{k_id:08X})")
            except Exception as e:
                print(f"[-] Warning: Failed to parse {args.key_file}: {e}")

        # 2. Offline audio file demodulation
        if args.audio_file:
            print(f"[*] Demodulating audio from file: {args.audio_file}...")
            try:
                from scipy.io import wavfile
                sr, pcm = wavfile.read(args.audio_file)
                if pcm.ndim > 1:
                    pcm = pcm[:, 0]
                if pcm.dtype == np.int16:
                    samples = pcm.astype(np.float32) / 32768.0
                else:
                    samples = pcm.astype(np.float32)
                d = FSKDemodulator(sample_rate=sr)
                k_res = d.demodulate(samples)
                if k_res:
                    on_key_found(k_res)
                else:
                    print("[-] Could not decode FSK key packet from audio file.")
            except Exception as e:
                print(f"[-] Error reading audio file: {e}")

        # 3. Start live acoustic listener thread (Headphones/Desktop sound or Mic)
        if audio_enabled or (not args.audio_file and not fallback_key):
            audio_thread = AudioCaptureThread(demodulator, on_key_found, source_type=audio_src_type)
            cmd, desc = audio_thread.find_recorder()
            if cmd:
                print(f"[*] Live acoustic key listener started: {desc}")
                audio_thread.start()
            else:
                print(f"[-] Warning: No audio capture tool found for {audio_src_type}.")

        # 4. Standalone acoustic listener mode (if explicitly requested without screen/webcam/qr)
        if (args.audio or args.headphones) and not screen_mode and args.webcam is None and not args.qr_file:
            print("[*] Listening for SeldOS acoustic key broadcast (Ctrl+C to exit)...")
            while audio_thread and audio_thread.is_alive():
                with key_lock:
                    if recovered_key_info:
                        break
                time.sleep(0.2)

            if recovered_key_info and args.output:
                out_path = args.output
                if os.path.isdir(out_path):
                    out_path = os.path.join(out_path, f"key_0x{recovered_key_info['session_id']:08X}.txt")
                with open(out_path, "w") as f:
                    f.write(f"SESSION_ID=0x{recovered_key_info['session_id']:08X}\n")
                    f.write(f"KEY_ID=0x{recovered_key_info['key_id']:08X}\n")
                    f.write(f"MASTER_KEY={recovered_key_info['master_key'].hex().upper()}\n")
                print(f"[+] Key information saved to: {out_path}\n")
            return

        # 5. Process offline QR images if provided
        if args.qr_file:
            print(f"[*] Reading QR frames from {args.qr_file}...")
            files = []
            if os.path.isdir(args.qr_file):
                for fname in sorted(os.listdir(args.qr_file)):
                    if fname.lower().endswith((".png", ".jpg", ".bmp", ".ppm")):
                        files.append(os.path.join(args.qr_file, fname))
            else:
                files = [args.qr_file]

            detector = UniversalQRDetector()
            for img_path in files:
                img = cv2.imread(img_path)
                if img is not None:
                    codes = detector.detect_all(img)
                    for val in codes:
                        complete, msg = collector.process_qr_payload(val)
                        print(f"    {msg}")

        # 6. Live Screen Capture Loop (Desktop broadcast / QEMU window)
        elif screen_mode:
            if not HAS_OPENCV:
                print("[-] Fatal: OpenCV required for QR detection.")
                sys.exit(1)

            parsed_region = None
            if args.region:
                try:
                    parts = list(map(int, args.region.split(",")))
                    if len(parts) == 4:
                        parsed_region = tuple(parts)
                except Exception:
                    print(f"[-] Invalid region format '{args.region}', expected X,Y,W,H")

            # Check Hyprland workspace environment
            check_hyprland_windows()

            grabber = ScreenGrabber(region=parsed_region)
            detector = UniversalQRDetector()

            # PHASE 1: ACOUSTIC MASTER KEY (SeldOS transmits the key FIRST via PC Speaker)
            if audio_enabled and fallback_key is None and recovered_key_info is None:
                print("\n" + "=" * 71)
                print("[*] Phase 1/2: Listening for Acoustic Master Key (Bell 202 FSK)...")
                print("    (SeldOS transmits master key first via speaker/headphones for ~9.8s)")
                print("=" * 71)

                audio_wait_timeout = min(16.0, float(args.timeout)) if args.timeout > 0 else 16.0
                phase1_start = time.time()
                skip_phase1 = False

                while time.time() - phase1_start < audio_wait_timeout:
                    with key_lock:
                        if recovered_key_info:
                            break

                    # Probe: If SeldOS QR code is ALREADY cycling on screen, advance to Phase 2!
                    probe_frame = grabber.grab()
                    if probe_frame is not None:
                        probe_codes = detector.detect_all(probe_frame)
                        if any(c.startswith("SELD1:") for c in probe_codes):
                            print("\n[*] Optical SeldOS QR frame detected on screen! Advancing to Phase 2...")
                            skip_phase1 = True
                            break

                    # Allow skipping audio wait with Enter
                    if sys.stdin.isatty():
                        r, _, _ = select.select([sys.stdin], [], [], 0)
                        if r:
                            sys.stdin.readline()
                            print("\n[*] Audio listening skipped by user. Advancing to Phase 2...")
                            skip_phase1 = True
                            break

                    rem_t = max(0.0, audio_wait_timeout - (time.time() - phase1_start))
                    rms_val = audio_thread.last_rms if audio_thread else 0.0
                    status_str = f"\r\033[K[Acoustic Key] Listening... RMS: {rms_val:.3f} | Left: {rem_t:.1f}s [Enter to skip to QR]"
                    sys.stdout.write(status_str)
                    sys.stdout.flush()
                    time.sleep(0.1)

                print()
                with key_lock:
                    if recovered_key_info:
                        print(f"[+] Phase 1 COMPLETE: Master key locked for Session 0x{recovered_key_info['session_id']:08X}!")
                    elif not skip_phase1:
                        print("[-] Acoustic key not locked yet (background listener remains active).")

            # PHASE 2: OPTICAL DATA DIODE (Capturing QR frames from screen)
            print("\n" + "=" * 71)
            print("[*] Phase 2/2: Optical Data Diode (Capturing QR frames from screen)...")
            print("=" * 71)

            done = False
            last_msg = ""
            timeout_sec = args.timeout

            start_t = time.time()
            fps_count = 0
            fps_start = time.time()
            fps_val = 0.0

            while not done:
                now = time.time()
                elapsed = now - start_t
                if timeout_sec > 0 and elapsed >= timeout_sec:
                    print(f"\n[-] Optical capture timed out after {timeout_sec:.0f}s.")
                    break

                frame = grabber.grab()
                fps_count += 1
                if now - fps_start >= 1.0:
                    fps_val = fps_count / (now - fps_start)
                    fps_count = 0
                    fps_start = now

                if frame is None:
                    time.sleep(0.08)
                    continue

                codes = detector.detect_all(frame)
                for val in codes:
                    if val and val.startswith("SELD1:"):
                        complete, msg = collector.process_qr_payload(val)
                        if msg != last_msg:
                            print(f"\n[Optical Screen Diode] {msg}")
                            last_msg = msg

                        if complete:
                            print("\n[+] Phase 2 COMPLETE: All optical frames successfully captured from screen!")
                            done = True
                            break

                if not done:
                    rem = f"{int(timeout_sec - elapsed)}s left" if timeout_sec > 0 else "inf"
                    audio_status = "MUTED"
                    if audio_thread:
                        audio_status = f"RMS:{audio_thread.last_rms:.3f}"
                        with key_lock:
                            if recovered_key_info:
                                audio_status = "KEY LOCKED"

                    got_chunks = 0
                    tot_chunks = "?"
                    if collector.sessions:
                        s_first = list(collector.sessions.values())[0]
                        got_chunks = len(s_first["chunks"])
                        tot_chunks = str(s_first["total"])

                    status_line = f"\r\033[K[Diode] Frames: {got_chunks}/{tot_chunks} | Screen: {fps_val:.1f} FPS | Audio: {audio_status} | Time: {rem}"
                    sys.stdout.write(status_line)
                    sys.stdout.flush()

                time.sleep(0.04)  # ~25 FPS

        # 7. Live Webcam Loop (if explicitly requested)
        elif args.webcam is not None:
            if not HAS_OPENCV:
                print("[-] Fatal: OpenCV required for webcam capture.")
                sys.exit(1)

            # PHASE 1: ACOUSTIC MASTER KEY (SeldOS transmits the key FIRST via PC Speaker)
            if audio_enabled and fallback_key is None and recovered_key_info is None:
                print("\n" + "=" * 71)
                print("[*] Phase 1/2: Listening for Acoustic Master Key (Bell 202 FSK)...")
                print("    (SeldOS transmits master key first via speaker/microphone for ~9.8s)")
                print("=" * 71)

                audio_wait_timeout = min(16.0, float(args.timeout)) if args.timeout > 0 else 16.0
                phase1_start = time.time()
                skip_phase1 = False

                while time.time() - phase1_start < audio_wait_timeout:
                    with key_lock:
                        if recovered_key_info:
                            break

                    if sys.stdin.isatty():
                        r, _, _ = select.select([sys.stdin], [], [], 0)
                        if r:
                            sys.stdin.readline()
                            print("\n[*] Audio listening skipped by user. Advancing to Phase 2...")
                            skip_phase1 = True
                            break

                    rem_t = max(0.0, audio_wait_timeout - (time.time() - phase1_start))
                    rms_val = audio_thread.last_rms if audio_thread else 0.0
                    status_str = f"\r\033[K[Acoustic Key] Listening... RMS: {rms_val:.3f} | Left: {rem_t:.1f}s [Enter to skip to Webcam]"
                    sys.stdout.write(status_str)
                    sys.stdout.flush()
                    time.sleep(0.1)

                print()
                with key_lock:
                    if recovered_key_info:
                        print(f"[+] Phase 1 COMPLETE: Master key locked for Session 0x{recovered_key_info['session_id']:08X}!")
                    elif not skip_phase1:
                        print("[-] Acoustic key not locked yet (background listener remains active).")

            print("\n" + "=" * 71)
            print(f"[*] Phase 2/2: Optical Webcam Diode (Camera {args.webcam})...")
            print("=" * 71)
            print(f"[*] Opening webcam device {args.webcam} (Press 'q' or Ctrl+C to stop)...")
            cap = cv2.VideoCapture(args.webcam)
            if not cap.isOpened():
                print(f"[-] Error: Could not open camera {args.webcam}")
                sys.exit(1)

            detector = UniversalQRDetector()
            done = False
            last_msg = ""
            timeout_sec = args.timeout

            start_t = time.time()
            fps_count = 0
            fps_start = time.time()
            fps_val = 0.0

            while not done:
                now = time.time()
                elapsed = now - start_t
                if timeout_sec > 0 and elapsed >= timeout_sec:
                    print(f"\n[-] Webcam capture timed out after {timeout_sec:.0f}s.")
                    break

                ret, frame = cap.read()
                if not ret:
                    time.sleep(0.05)
                    continue

                fps_count += 1
                if now - fps_start >= 1.0:
                    fps_val = fps_count / (now - fps_start)
                    fps_count = 0
                    fps_start = now

                codes = detector.detect_all(frame)
                for val in codes:
                    if val and val.startswith("SELD1:"):
                        complete, msg = collector.process_qr_payload(val)
                        if msg != last_msg:
                            print(f"\n[Optical Webcam Diode] {msg}")
                            last_msg = msg

                        if complete:
                            print("\n[+] All optical frames successfully captured!")
                            done = True
                            break

                if not done:
                    rem = f"{int(timeout_sec - elapsed)}s left" if timeout_sec > 0 else "inf"
                    audio_status = "MUTED"
                    if audio_thread:
                        audio_status = f"RMS:{audio_thread.last_rms:.3f}"
                        with key_lock:
                            if recovered_key_info:
                                audio_status = "KEY LOCKED"

                    got_chunks = 0
                    tot_chunks = "?"
                    if collector.sessions:
                        s_first = list(collector.sessions.values())[0]
                        got_chunks = len(s_first["chunks"])
                        tot_chunks = str(s_first["total"])

                    status_line = f"\r\033[K[Diode] Frames: {got_chunks}/{tot_chunks} | Cam: {fps_val:.1f} FPS | Audio: {audio_status} | Time: {rem}"
                    sys.stdout.write(status_line)
                    sys.stdout.flush()

                if args.gui:
                    cv2.imshow("SeldOS Air-Gap Diode Receiver", frame)
                    if cv2.waitKey(1) & 0xFF == ord('q'):
                        break

            cap.release()
            if args.gui:
                cv2.destroyAllWindows()

        # 8. Reassembly & Decryption phase
        if not collector.sessions:
            print("\n[-] No SELD1 optical sessions received.")
            if screen_mode:
                print("[-] Diagnostic Hints:")
                print("    1. Under Hyprland/Wayland, the VM window MUST be on your active visible workspace.")
                print("    2. Verify SeldOS is running 'diode send <file>' or 'diode qr <file>'.")
                print("    3. If the VM is in windowed mode, make sure its display is not minimized.")
            sys.exit(1)

        for session_id in list(collector.sessions.keys()):
            envelope = collector.get_reassembled_envelope(session_id)
            if not envelope:
                print(f"[-] Session 0x{session_id:08X}: Incomplete frames, cannot reassemble.")
                continue

            print(f"\n[*] Reassembled complete envelope for Session 0x{session_id:08X} ({len(envelope)} bytes)")

            # Determine key: acoustic, fallback, wait, or interactive prompt
            use_key = None
            with key_lock:
                if recovered_key_info:
                    if recovered_key_info["session_id"] == session_id or len(collector.sessions) == 1:
                        use_key = recovered_key_info["master_key"]
                        print(f"[+] Using master key verified via Acoustic Air-Gap (Session 0x{recovered_key_info['session_id']:08X}).")
                elif fallback_key:
                    use_key = fallback_key
                    print("[+] Using local key sheet / key material fallback.")

            # Automatic check of common local key file locations
            if use_key is None:
                for cand in [
                    os.path.expanduser("~/.cache/seld-airgap/last_key.txt"),
                    "./airgap.key",
                    os.path.expanduser("~/airgap.key"),
                    "/tmp/airgap.key",
                    "/airgap.key"
                ]:
                    if os.path.exists(cand):
                        try:
                            with open(cand, "r") as kf:
                                k_text = kf.read()
                            for l in k_text.splitlines():
                                if l.startswith("MASTER_KEY="):
                                    k_text = l.split("=", 1)[1].strip()
                                    break
                            cand_key, cand_id = parse_key_input(k_text)
                            use_key = cand_key
                            print(f"[+] Automatically loaded cached/local fallback key: {cand} (Key ID: 0x{cand_id:08X})")
                            break
                        except Exception:
                            pass

            # Interactive key entry fallback
            if use_key is None and sys.stdin.isatty():
                print("\n[?] Acoustic key not received automatically.")
                try:
                    manual = input("    Enter 40-line key sheet path or 32-char hex key (Enter to abort): ").strip()
                    if manual:
                        if os.path.exists(manual):
                            with open(manual, "r") as f:
                                use_key, _ = parse_key_input(f.read())
                        else:
                            use_key, _ = parse_key_input(manual)
                except Exception as e:
                    print(f"[-] Invalid key input: {e}")

            if use_key is None:
                print("[-] No matching cryptographic key available for this session!")
                continue

            try:
                fn, plaintext = decrypt_envelope(envelope, use_key)
                print("=" * 71)
                print("[+] SELD-AIRGAP DECRYPTION & INTEGRITY ATTESTATION SUCCESSFUL!")
                print(f"    Original File : {fn}")
                print(f"    Plaintext Size: {len(plaintext)} bytes")
                print(f"    Plaintext SHA : {hashlib.sha256(plaintext).hexdigest()}")
                print("=" * 71)

                # Save decrypted file with path traversal defense
                safe_fn = os.path.basename(fn.replace("\\", "/"))
                if not safe_fn:
                    safe_fn = f"payload_0x{session_id:08X}.bin"

                out_path = args.output
                if os.path.isdir(out_path):
                    out_path = os.path.join(out_path, safe_fn)
                elif out_path.endswith("/") or out_path.endswith("\\"):
                    os.makedirs(out_path, exist_ok=True)
                    out_path = os.path.join(out_path, safe_fn)

                os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
                with open(out_path, "wb") as f:
                    f.write(plaintext)
                print(f"[+] Saved verified decrypted payload to: {out_path}\n")

            except Exception as e:
                print(f"[-] Cryptographic Decryption Failed: {e}")

    except KeyboardInterrupt:
        print("\n[*] Operation cancelled by user.")
    finally:
        if audio_thread:
            audio_thread.stop()


def run_self_test():
    """Comprehensive automated self-test of the full air-gap pipeline."""
    print("[*] Running SeldOS Air-Gap Pipeline Self-Test...")

    # 1. 40-line key sheet generation and parsing test
    sample_sheet = """# SELD-AIRGAP SOVEREIGN ONE-TIME KEY SHEET (40 LINES)
L01: 7F4A-9B1C-3D8E-20F5
L02: 8A2E-1D94-C3B5-706F
L03: 5E1D-4A8C-2B3F-019E
L04: 1122-3344-5566-7788
L05: 99AA-BBCC-DDEE-FF00
L06: 0102-0304-0506-0708
L07: A1A2-A3A4-A5A6-A7A8
L08: B1B2-B3B4-B5B6-B7B8
L09: C1C2-C3C4-C5C6-C7C8
L10: D1D2-D3D4-D5D6-D7D8
L11: E1E2-E3E4-E5E6-E7E8
L12: F1F2-F3F4-F5F6-F7F8
L13: 1234-5678-9ABC-DEF0
L14: FEED-FACE-CAFE-BEEF
L15: 5E1D-CAFE-5E1D-1337
L16: 0011-2233-4455-6677
L17: 8899-AABB-CCDD-EEFF
L18: 1337-BEEF-C001-D00D
L19: DEADBEEF-CAFEBABE
L20: 0123-4567-89AB-CDEF
L21: AABB-CCDD-EEFF-0011
L22: 2233-4455-6677-8899
L23: 1111-2222-3333-4444
L24: 5555-6666-7777-8888
L25: 9999-0000-AAAA-BBBB
L26: CCCC-DDDD-EEEE-FFFF
L27: 0A1B-2C3D-4E5F-6A7B
L28: 8C9D-0E1F-2A3B-4C5D
L29: 6E7F-8A9B-0C1D-2E3F
L30: 4A5B-6C7D-8E9F-0A1B
L31: 2C3D-4E5F-6A7B-8C9D
L32: 0E1F-2A3B-4C5D-6E7F
L33: 8A9B-0C1D-2E3F-4A5B
L34: 6C7D-8E9F-0A1B-2C3D
L35: 4E5F-6A7B-8C9D-0E1F
L36: 2A3B-4C5D-6E7F-8A9B
L37: 0C1D-2E3F-4A5B-6C7D
L38: 8E9F-0A1B-2C3D-4E5F
L39: 6A7B-8C9D-0E1F-2A3B
L40: 4C5D-6E7F-8A9B-0C1D
"""
    master_key, key_id = parse_40_line_key_sheet(sample_sheet)
    assert len(master_key) == 16
    print(f"[1/4] Key Sheet parsed: Master Key = {master_key.hex()}, Key ID = 0x{key_id:08X} [OK]")

    # 2. Crypto test: AES-128-GCM encryption & envelope verification
    secret_payload = b"TOP-SECRET AIR-GAP PAYLOAD FOR ARCH LINUX 2026. PROVE OPSEC PURITY."
    filename = "classified.txt"
    session_id = 0x5E1DCAFE
    iv = os.urandom(12)
    pt_sha = hashlib.sha256(secret_payload).digest()

    hdr_prefix = struct.pack("<IBI32sI", DIODE_MAGIC, 1, session_id, filename.encode("utf-8"), len(secret_payload))
    aad = hdr_prefix

    aesgcm = AESGCM(master_key)
    ct_with_tag = aesgcm.encrypt(iv, secret_payload, aad)
    ct = ct_with_tag[:-16]
    tag = ct_with_tag[-16:]

    envelope = struct.pack(ENVELOPE_HDR_FMT, DIODE_MAGIC, 1, session_id, filename.encode("utf-8"),
                           len(secret_payload), iv, tag, pt_sha) + ct

    dec_fn, dec_pt = decrypt_envelope(envelope, master_key)
    assert dec_fn == filename
    assert dec_pt == secret_payload
    print(f"[2/4] AES-128-GCM Envelope Encryption/Decryption Verified [OK]")

    # 3. QR Chunking & Reassembly
    collector = OpticalDiodeCollector()
    chunk_size = 32
    chunks = [envelope[i:i + chunk_size] for i in range(0, len(envelope), chunk_size)]
    for i, c in enumerate(chunks):
        b64 = base64.b64encode(c).decode("utf-8")
        crc = crc32_ieee(c)
        frame_str = f"SELD1:{i+1}/{len(chunks)}:{session_id:08X}:{filename}:{b64}:{crc:08X}"
        collector.process_qr_payload(frame_str)

    reassembled = collector.get_reassembled_envelope(session_id)
    assert reassembled == envelope
    print(f"[3/4] Optical Diode Frame Chunking & Reassembly Verified [OK]")

    # 4. FSK Acoustic Modulation & Demodulation with 15% noise
    pkt_raw = struct.pack("<2sBII16s", FSK_SYNC_WORD, 1, session_id, key_id, master_key)
    pkt_crc = crc32_ieee(pkt_raw)
    full_pkt = pkt_raw + struct.pack("<I", pkt_crc)

    sample_rate = 44100
    bit_dur = 0.030
    N = int(sample_rate * bit_dur)

    preamble_bits = [1, 0] * 12 # 24 bits
    data_bits = []
    for b in full_pkt:
        for bit_i in range(8):
            data_bits.append((b >> (7 - bit_i)) & 1)

    all_bits = preamble_bits + data_bits
    audio = []
    audio.append(np.zeros(int(sample_rate * 0.05)))
    for bit in all_bits:
        freq = FSK_FREQ_MARK if bit else FSK_FREQ_SPACE
        t = np.linspace(0, bit_dur, N, endpoint=False)
        audio.append(np.sin(2 * np.pi * freq * t))
    audio.append(np.zeros(int(sample_rate * 0.05)))
    audio = np.concatenate(audio)
    audio += np.random.normal(0, 0.10, len(audio)) # Add noise!

    demod = FSKDemodulator(sample_rate=sample_rate, bit_dur_sec=bit_dur)
    rx_key_info = demod.demodulate(audio)
    assert rx_key_info is not None
    assert rx_key_info["session_id"] == session_id
    assert rx_key_info["key_id"] == key_id
    assert rx_key_info["master_key"] == master_key
    assert rx_key_info["crc32"] == pkt_crc
    print(f"[4/4] Acoustic Air-Gap 1200/2200 Hz Synchronous Demodulation Verified [OK]")

    print("\n[+] ALL SELF-TESTS PASSED WITH 100% SUCCESS!")


if __name__ == "__main__":
    main()
