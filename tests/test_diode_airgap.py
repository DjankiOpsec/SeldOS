#!/usr/bin/env python3
"""
Comprehensive Automated Unit and Integration Test Suite
for SeldOS Sovereign Optical & Acoustic Air-Gap Data Diode Subsystem
(tests/test_diode_airgap.py)

Covers:
1. 40-Line Cryptographic Key Sheet Generator: Format, Line Count, Entropy, Grouping.
2. HKDF-SHA256 Master Key Derivation & Key ID Binding.
3. AES-128-GCM AEAD Fail-Closed Encryption & Tamper Resistance.
4. Optical Data Diode (Animated QR Chunking, Frame Parsing, Out-of-Order Reassembly).
5. Acoustic Air-Gap Channel (Bell 202 FSK Modulation, Matched Filter Demodulation under Noise).
6. Full End-to-End Exfiltration Simulation Pipeline.
7. Verification of SeldOS ELF-64 Binary Compilation and Disk Image Packing.

GPLv3 Licensed.
"""

import sys
import os
import unittest
import struct
import hashlib
import binascii
import base64
import random
from pathlib import Path
import numpy as np

BASE_DIR = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(BASE_DIR))

from tools.host_diode.seld_diode_receiver import (
    FSKDemodulator,
    OpticalDiodeCollector,
    parse_40_line_key_sheet,
    parse_key_input,
    decrypt_envelope,
    crc32_ieee,
    FSK_FREQ_SPACE,
    FSK_FREQ_MARK,
    FSK_FREQ_PREAMBLE,
    FSK_SYNC_WORD,
    DIODE_MAGIC,
    ENVELOPE_HDR_FMT,
    ENVELOPE_HDR_SIZE
)
from cryptography.hazmat.primitives.ciphers.aead import AESGCM


class TestSeldAirGapDiode(unittest.TestCase):

    def generate_synthetic_40_line_keysheet(self) -> str:
        """Helper to generate a syntactically valid 40-line SeldOS key sheet."""
        lines = [
            "# SELD-AIRGAP SOVEREIGN ONE-TIME KEY SHEET (40 LINES)",
            "# OPSEC: ZERO-TRUST ACOUSTIC/OPTICAL TRANSMISSION BINDING"
        ]
        for i in range(1, 41):
            rand_hex = "".join(random.choices("0123456789ABCDEF", k=16))
            lines.append(f"L{i:02d}: {rand_hex[:4]}-{rand_hex[4:8]}-{rand_hex[8:12]}-{rand_hex[12:]}")
        return "\n".join(lines) + "\n"

    def test_40_line_keysheet_format_and_properties(self):
        """Validates that key sheets have exactly 40 lines and valid format."""
        sheet = self.generate_synthetic_40_line_keysheet()
        parsed_lines = [l for l in sheet.splitlines() if l.startswith("L")]
        self.assertEqual(len(parsed_lines), 40, "Key sheet must contain exactly 40 key lines")

        for idx, line in enumerate(parsed_lines, start=1):
            self.assertTrue(line.startswith(f"L{idx:02d}: "), f"Line {idx} label invalid: {line}")
            tokens = line.split(": ")[1].split("-")
            self.assertEqual(len(tokens), 4, f"Line {idx} must have 4 hyphen-separated groups")
            for grp in tokens:
                self.assertEqual(len(grp), 4, f"Group {grp} must be exactly 4 hex characters")
                int(grp, 16) # Must be valid hex

    def test_key_derivation_deterministic_and_avalanche(self):
        """Verifies HKDF-SHA256 key derivation determinism and avalanche effect."""
        sheet1 = self.generate_synthetic_40_line_keysheet()
        key1a, id1a = parse_40_line_key_sheet(sheet1)
        key1b, id1b = parse_40_line_key_sheet(sheet1)

        self.assertEqual(key1a, key1b, "Key derivation must be deterministic")
        self.assertEqual(id1a, id1b, "Key ID must be deterministic")
        self.assertEqual(len(key1a), 16, "Master key must be exactly 16 bytes for AES-128")

        # Flip hex char in line 1 of sheet
        sheet2 = sheet1.replace("L01: ", "L01: FFFF", 1)
        key2, id2 = parse_40_line_key_sheet(sheet2)
        self.assertNotEqual(key1a, key2, "Modification of key line must yield completely different key")
        self.assertNotEqual(id1a, id2, "Key ID must change upon key alteration")

    def test_aes128_gcm_fail_closed_tampering(self):
        """Verifies AEAD authentication and immediate fail-closed behavior upon tampering."""
        master_key = os.urandom(16)
        plaintext = b"Top secret operational plan for SeldOS air-gap transmission."
        filename = "plan.txt"
        session_id = 0x1A2B3C4D
        iv = os.urandom(12)
        pt_sha = hashlib.sha256(plaintext).digest()

        aad_len = 4 + 1 + 4 + 32 + 4
        hdr_prefix = struct.pack("<IBI32sI", DIODE_MAGIC, 1, session_id, filename.encode("utf-8"), len(plaintext))
        aad = hdr_prefix

        aesgcm = AESGCM(master_key)
        ct_with_tag = aesgcm.encrypt(iv, plaintext, aad)
        ct = ct_with_tag[:-16]
        tag = ct_with_tag[-16:]

        valid_env = struct.pack(ENVELOPE_HDR_FMT, DIODE_MAGIC, 1, session_id, filename.encode("utf-8"),
                                len(plaintext), iv, tag, pt_sha) + ct

        # Positive test
        fn, pt = decrypt_envelope(valid_env, master_key)
        self.assertEqual(fn, filename)
        self.assertEqual(pt, plaintext)

        # Tamper test 1: Flip bit in ciphertext
        tampered_ct = bytearray(valid_env)
        tampered_ct[ENVELOPE_HDR_SIZE + 5] ^= 0x01
        with self.assertRaises(ValueError):
            decrypt_envelope(bytes(tampered_ct), master_key)

        # Tamper test 2: Wrong key
        wrong_key = os.urandom(16)
        with self.assertRaises(ValueError):
            decrypt_envelope(valid_env, wrong_key)

        # Tamper test 3: Corrupt IV
        tampered_iv = bytearray(valid_env)
        tampered_iv[45] ^= 0xFF
        with self.assertRaises(ValueError):
            decrypt_envelope(bytes(tampered_iv), master_key)

        # Tamper test 4: Corrupt Auth Tag
        tampered_tag = bytearray(valid_env)
        tampered_tag[45 + 12] ^= 0xFF
        with self.assertRaises(ValueError):
            decrypt_envelope(bytes(tampered_tag), master_key)

    def test_optical_diode_chunking_and_out_of_order_reassembly(self):
        """Verifies chunking, CRC32 validation, and out-of-order frame reassembly."""
        raw_payload = b"Payload for optical diode: " + os.urandom(300)
        session_id = 0x5E1D1337
        filename = "data.bin"

        chunk_size = 48
        chunks = [raw_payload[i:i + chunk_size] for i in range(0, len(raw_payload), chunk_size)]
        total = len(chunks)

        frame_strings = []
        for i, c in enumerate(chunks, start=1):
            b64 = base64.b64encode(c).decode("utf-8")
            crc = crc32_ieee(c)
            f_str = f"SELD1:{i}/{total}:{session_id:08X}:{filename}:{b64}:{crc:08X}"
            frame_strings.append((i, f_str))

        # Shuffle frames randomly to simulate webcam picking up frames in arbitrary order
        shuffled = list(frame_strings)
        random.shuffle(shuffled)

        collector = OpticalDiodeCollector()
        completed = False
        for seq, f_str in shuffled:
            is_done, _ = collector.process_qr_payload(f_str)
            if is_done:
                completed = True

        self.assertTrue(completed, "Collector must report complete once all frames are seen")
        reassembled = collector.get_reassembled_envelope(session_id)
        self.assertEqual(reassembled, raw_payload, "Reassembled payload must match original byte-for-byte")

        # Test corrupted frame CRC rejection
        bad_frame = f"SELD1:1/{total}:{session_id:08X}:{filename}:{b64}:DEADBEEF"
        c2 = OpticalDiodeCollector()
        ok, msg = c2.process_qr_payload(bad_frame)
        self.assertFalse(ok)
        self.assertIn("CRC32 mismatch", msg)

    def test_fsk_modulator_demodulator_under_noise(self):
        """Verifies synchronous Bell 202 FSK demodulation in presence of Gaussian noise."""
        session_id = 0xCAFEBABE
        key_id = 0x12345678
        master_key = bytes([0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44,
                            0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0xEE, 0xFF])

        pkt_raw = struct.pack("<2sBII16s", FSK_SYNC_WORD, 1, session_id, key_id, master_key)
        crc = crc32_ieee(pkt_raw)
        full_pkt = pkt_raw + struct.pack("<I", crc)

        sample_rate = 44100
        bit_dur = 0.030
        N = int(sample_rate * bit_dur)

        # Synthesize audio with preamble
        preamble_bits = [1, 0] * 12 # 24 training bits
        data_bits = []
        for b in full_pkt:
            for bit_i in range(8):
                data_bits.append((b >> (7 - bit_i)) & 1)

        all_bits = preamble_bits + data_bits
        audio = [np.zeros(int(sample_rate * 0.05))] # 50ms silence
        for bit in all_bits:
            freq = FSK_FREQ_MARK if bit else FSK_FREQ_SPACE
            t = np.linspace(0, bit_dur, N, endpoint=False)
            audio.append(np.sin(2 * np.pi * freq * t))
        audio.append(np.zeros(int(sample_rate * 0.05)))
        audio = np.concatenate(audio)

        # Inject 15% Gaussian noise
        audio += np.random.normal(0, 0.15, len(audio))

        demod = FSKDemodulator(sample_rate=sample_rate, bit_dur_sec=bit_dur)
        res = demod.demodulate(audio)

        self.assertIsNotNone(res, "FSK Demodulator must successfully lock onto noisy signal")
        self.assertEqual(res["session_id"], session_id)
        self.assertEqual(res["key_id"], key_id)
        self.assertEqual(res["master_key"], master_key)
        self.assertEqual(res["crc32"], crc)

    def test_full_end_to_end_airgap_pipeline(self):
        """Simulates full end-to-end air-gap exfiltration pipeline."""
        # 1. Transmitter: Generate 40-line key sheet & derive master key
        sheet = self.generate_synthetic_40_line_keysheet()
        master_key, key_id = parse_40_line_key_sheet(sheet)
        session_id = random.randint(0x10000000, 0xEFFFFFFF)

        # 2. Transmitter: Encrypt secret file
        secret_content = b"TOP-SECRET MILITARY GRADE INTEL: SeldOS Diode Transmission 2026."
        filename = "classified_report.txt"
        iv = os.urandom(12)
        pt_sha = hashlib.sha256(secret_content).digest()

        hdr_prefix = struct.pack("<IBI32sI", DIODE_MAGIC, 1, session_id, filename.encode("utf-8"), len(secret_content))
        aesgcm = AESGCM(master_key)
        ct_with_tag = aesgcm.encrypt(iv, secret_content, hdr_prefix)
        ct = ct_with_tag[:-16]
        tag = ct_with_tag[-16:]

        envelope = struct.pack(ENVELOPE_HDR_FMT, DIODE_MAGIC, 1, session_id, filename.encode("utf-8"),
                               len(secret_content), iv, tag, pt_sha) + ct

        # 3. Channel 1 (Optical Diode): Chunk into QR frames
        collector = OpticalDiodeCollector()
        chunk_sz = 64
        chunks = [envelope[i:i + chunk_sz] for i in range(0, len(envelope), chunk_sz)]
        for idx, c in enumerate(chunks, start=1):
            b64 = base64.b64encode(c).decode("utf-8")
            c_crc = crc32_ieee(c)
            f_str = f"SELD1:{idx}/{len(chunks)}:{session_id:08X}:{filename}:{b64}:{c_crc:08X}"
            collector.process_qr_payload(f_str)

        # 4. Channel 2 (Acoustic Air-Gap): Audio synthesis & demodulation
        pkt_raw = struct.pack("<2sBII16s", FSK_SYNC_WORD, 1, session_id, key_id, master_key)
        fsk_crc = crc32_ieee(pkt_raw)
        full_pkt = pkt_raw + struct.pack("<I", fsk_crc)

        sample_rate = 44100
        bit_dur = 0.030
        N = int(sample_rate * bit_dur)
        bits = [1, 0] * 12 + [((b >> (7 - bi)) & 1) for b in full_pkt for bi in range(8)]
        audio_segments = [np.zeros(int(sample_rate * 0.05))]
        for bit in bits:
            freq = FSK_FREQ_MARK if bit else FSK_FREQ_SPACE
            t = np.linspace(0, bit_dur, N, endpoint=False)
            audio_segments.append(np.sin(2 * np.pi * freq * t))
        audio_segments.append(np.zeros(int(sample_rate * 0.05)))
        audio = np.concatenate(audio_segments) + np.random.normal(0, 0.05, len(np.concatenate(audio_segments)))

        demod = FSKDemodulator(sample_rate=sample_rate, bit_dur_sec=bit_dur)
        audio_key_dict = demod.demodulate(audio)
        self.assertIsNotNone(audio_key_dict)
        self.assertEqual(audio_key_dict["session_id"], session_id)

        # 5. Receiver: Reassemble & Decrypt
        reassembled_env = collector.get_reassembled_envelope(session_id)
        rx_filename, decrypted_pt = decrypt_envelope(reassembled_env, audio_key_dict["master_key"])

        self.assertEqual(rx_filename, filename)
        self.assertEqual(decrypted_pt, secret_content)

    def test_seldos_binary_presence_and_disk_image(self):
        """Verifies that SeldOS /bin/diode compiled binary exists and is packed in disk image."""
        bin_path = BASE_DIR / "build" / "bin" / "diode"
        disk_path = BASE_DIR / "build" / "disk.img"
        iso_path = BASE_DIR / "build" / "seldos.iso"

        self.assertTrue(bin_path.exists(), f"{bin_path} must exist")
        self.assertGreater(bin_path.stat().st_size, 50000, "diode binary size must exceed 50 KiB")

        # Verify ELF-64 header
        with open(bin_path, "rb") as f:
            elf_magic = f.read(4)
        self.assertEqual(elf_magic, b"\x7fELF", "diode must be a valid ELF binary")

        self.assertTrue(disk_path.exists(), f"{disk_path} must exist")
        self.assertTrue(iso_path.exists(), f"{iso_path} must exist")

        # Verify diode is listed inside disk image sectors
        with open(disk_path, "rb") as df:
            disk_content = df.read()
        self.assertIn(b"/bin/diode", disk_content, "/bin/diode must be present in SeldFS inodes")
        self.assertIn(b"secret.txt", disk_content, "secret.txt test payload must be present in SeldFS")

    def test_c_qrcodegen_opencv_interop(self):
        """Compiles C qrcodegen, generates matrix in C, renders and verifies OpenCV QRCodeDetector decodes it."""
        import ctypes
        import cv2

        so_path = "/tmp/libqrcodegen_test.so"
        if not os.path.exists(so_path):
            c_src = BASE_DIR / "userspace" / "bin" / "diode" / "qrcodegen.c"
            inc1 = BASE_DIR / "userspace" / "libc" / "include"
            inc2 = BASE_DIR / "userspace" / "bin" / "diode"
            cmd = f"gcc -shared -fPIC -O2 '{c_src}' -I'{inc1}' -I'{inc2}' -o '{so_path}'"
            self.assertEqual(os.system(cmd), 0, "Failed to compile qrcodegen to shared lib for test")

        lib = ctypes.CDLL(so_path)
        lib.qrcodegen_encodeText.argtypes = [
            ctypes.c_char_p,
            ctypes.POINTER(ctypes.c_uint8),
            ctypes.POINTER(ctypes.c_uint8),
            ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_bool
        ]
        lib.qrcodegen_encodeText.restype = ctypes.c_bool
        lib.qrcodegen_getSize.argtypes = [ctypes.POINTER(ctypes.c_uint8)]
        lib.qrcodegen_getSize.restype = ctypes.c_int
        lib.qrcodegen_getModule.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_int, ctypes.c_int]
        lib.qrcodegen_getModule.restype = ctypes.c_bool

        buf_len = 3918
        temp_buf = (ctypes.c_uint8 * buf_len)()
        qr_buf = (ctypes.c_uint8 * buf_len)()

        test_payload = b"SELD1:1/1:1A2B3C4D:secret.txt:SGVsbG8gU2VsZE9T:9F42C10A"
        ok = lib.qrcodegen_encodeText(test_payload, temp_buf, qr_buf, 0, 1, 10, -1, True)
        self.assertTrue(ok, "qrcodegen_encodeText failed")

        size = lib.qrcodegen_getSize(qr_buf)
        self.assertGreaterEqual(size, 21, "QR code size must be at least 21x21")

        quiet = 4
        scale = 6
        img_dim = (size + quiet * 2) * scale
        img = np.ones((img_dim, img_dim), dtype=np.uint8) * 255

        for y in range(size):
            for x in range(size):
                if lib.qrcodegen_getModule(qr_buf, x, y):
                    x0 = (x + quiet) * scale
                    y0 = (y + quiet) * scale
                    img[y0:y0 + scale, x0:x0 + scale] = 0

        detector = cv2.QRCodeDetector()
        decoded_text, _, _ = detector.detectAndDecode(img)
        self.assertEqual(decoded_text.encode("utf-8"), test_payload, "OpenCV must decode C-generated QR code exactly")

    def test_real_c_crypto_interoperability(self):
        """Compiles real C diode_crypto and tests two-way interoperability with Python cryptography."""
        import ctypes
        so_path = "/tmp/libdiode_crypto_unit_test.so"
        c_src = BASE_DIR / "userspace" / "bin" / "diode" / "diode_crypto.c"
        sha_src = BASE_DIR / "userspace" / "libc" / "src" / "sha256.c"
        hkdf_src = BASE_DIR / "userspace" / "libc" / "src" / "seld_hkdf.c"
        aes_src = BASE_DIR / "userspace" / "libc" / "src" / "seld_aes128_gcm.c"
        inc1 = BASE_DIR / "userspace" / "libc" / "include"
        inc2 = BASE_DIR / "userspace" / "bin" / "diode"

        stub_c = "/tmp/seld_test_stubs.c"
        with open(stub_c, "w") as sf:
            sf.write('#include <stdint.h>\nuint64_t seld_uptime(void){return 54321;}\nint seld_getpid(void){return 777;}\n')

        cmd = f"gcc -shared -fPIC -O2 '{c_src}' '{sha_src}' '{hkdf_src}' '{aes_src}' '{stub_c}' -I'{inc1}' -I'{inc2}' -o '{so_path}'"
        self.assertEqual(os.system(cmd), 0, "Compilation of C diode_crypto failed")

        lib = ctypes.CDLL(so_path)
        lib.diode_generate_keysheet.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
        lib.diode_generate_keysheet.restype = ctypes.c_int

        lib.diode_derive_master_key.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_uint8), ctypes.POINTER(ctypes.c_uint32)]
        lib.diode_derive_master_key.restype = ctypes.c_int

        lib.diode_encrypt_payload.argtypes = [
            ctypes.POINTER(ctypes.c_uint8), ctypes.c_char_p,
            ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t, ctypes.c_uint32,
            ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)), ctypes.POINTER(ctypes.c_size_t)
        ]
        lib.diode_encrypt_payload.restype = ctypes.c_int

        lib.diode_decrypt_payload.argtypes = [
            ctypes.POINTER(ctypes.c_uint8), ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t,
            ctypes.c_char_p, ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)), ctypes.POINTER(ctypes.c_size_t)
        ]
        lib.diode_decrypt_payload.restype = ctypes.c_int

        # 1. C generates keysheet -> Python parses
        buf = ctypes.create_string_buffer(2048)
        gen_res = lib.diode_generate_keysheet(buf, 2048)
        self.assertGreater(gen_res, 0)
        c_sheet = buf.value.decode("utf-8")

        c_master = (ctypes.c_uint8 * 16)()
        c_kid = ctypes.c_uint32()
        self.assertEqual(lib.diode_derive_master_key(buf.value, c_master, ctypes.byref(c_kid)), 0)

        py_master, py_kid = parse_40_line_key_sheet(c_sheet)
        self.assertEqual(bytes(c_master), py_master, "Master key must match between C and Python")
        self.assertEqual(c_kid.value, py_kid, "Key ID must match between C and Python")

        # 2. C encrypts -> Python decrypts
        test_payload = b"Top secret operational intelligence vector from SeldOS Ring 3."
        c_pt = (ctypes.c_uint8 * len(test_payload))(*test_payload)
        out_env = ctypes.POINTER(ctypes.c_uint8)()
        out_len = ctypes.c_size_t()
        enc_res = lib.diode_encrypt_payload(c_master, b"/secret_classified.txt", c_pt, len(test_payload), 0x5E1DCAFE, ctypes.byref(out_env), ctypes.byref(out_len))
        self.assertEqual(enc_res, 0)

        env_bytes = ctypes.string_at(out_env, out_len.value)
        rx_fn, rx_pt = decrypt_envelope(env_bytes, py_master)
        self.assertEqual(rx_fn, "secret_classified.txt", "Filename must be sanitized basename")
        self.assertEqual(rx_pt, test_payload, "Decrypted plaintext must match byte-for-byte")

        # 3. Python encrypts -> C decrypts
        py_aes = AESGCM(py_master)
        iv = os.urandom(12)
        pt_sha = hashlib.sha256(test_payload).digest()
        hdr_prefix = struct.pack("<IBI32sI", DIODE_MAGIC, 1, 0x1337BEEF, b"py_file.bin", len(test_payload))
        ct_with_tag = py_aes.encrypt(iv, test_payload, hdr_prefix)
        py_envelope = struct.pack(ENVELOPE_HDR_FMT, DIODE_MAGIC, 1, 0x1337BEEF, b"py_file.bin",
                                  len(test_payload), iv, ct_with_tag[-16:], pt_sha) + ct_with_tag[:-16]

        c_env_in = (ctypes.c_uint8 * len(py_envelope))(*py_envelope)
        c_rx_fn = ctypes.create_string_buffer(32)
        c_rx_pt = ctypes.POINTER(ctypes.c_uint8)()
        c_rx_len = ctypes.c_size_t()

        dec_res = lib.diode_decrypt_payload(c_master, c_env_in, len(py_envelope), c_rx_fn, ctypes.byref(c_rx_pt), ctypes.byref(c_rx_len))
        self.assertEqual(dec_res, 0)
        c_dec_bytes = ctypes.string_at(c_rx_pt, c_rx_len.value)
        self.assertEqual(c_dec_bytes, test_payload)
        self.assertEqual(c_rx_fn.value.decode("utf-8"), "py_file.bin")

        # 4. Tampering: C decrypt must reject modified tag/ciphertext
        tampered = bytearray(py_envelope)
        tampered[-1] ^= 0x01
        c_tampered = (ctypes.c_uint8 * len(tampered))(*tampered)
        self.assertNotEqual(lib.diode_decrypt_payload(c_master, c_tampered, len(tampered), c_rx_fn, ctypes.byref(c_rx_pt), ctypes.byref(c_rx_len)), 0)

    def test_path_traversal_defense(self):
        """Verifies that malicious or path-polluted filenames in envelopes cannot escape output boundaries."""
        key = os.urandom(16)
        pt = b"Path traversal probe payload"
        session_id = 0x99887766
        iv = os.urandom(12)
        pt_sha = hashlib.sha256(pt).digest()

        for hostile_name in ["/etc/passwd", "../../../root/.bashrc", "C:\\Windows\\system.ini", "/var/log/syslog"]:
            hdr = struct.pack("<IBI32sI", DIODE_MAGIC, 1, session_id, hostile_name.encode("utf-8"), len(pt))
            aes = AESGCM(key)
            ct_tag = aes.encrypt(iv, pt, hdr)
            env = struct.pack(ENVELOPE_HDR_FMT, DIODE_MAGIC, 1, session_id, hostile_name.encode("utf-8"),
                              len(pt), iv, ct_tag[-16:], pt_sha) + ct_tag[:-16]

            safe_fn, dec_pt = decrypt_envelope(env, key)
            self.assertFalse(safe_fn.startswith("/"), f"Sanitized filename must not start with /: {safe_fn}")
            self.assertFalse(safe_fn.startswith(".."), f"Sanitized filename must not start with ..: {safe_fn}")
            self.assertNotIn("/", safe_fn, f"Sanitized filename must not contain directory slashes: {safe_fn}")
            self.assertNotIn("\\", safe_fn, f"Sanitized filename must not contain backslashes: {safe_fn}")
            self.assertEqual(dec_pt, pt)

    def test_parse_key_input_modes(self):
        """Verifies parse_key_input handles both 40-line key sheets and direct 32-hex keys."""
        # 1. Direct 32-char hex key
        hex_key = "00112233445566778899AABBCCDDEEFF"
        k1, id1 = parse_key_input(hex_key)
        self.assertEqual(k1, bytes.fromhex(hex_key))
        self.assertIsInstance(id1, int)

        # 2. 40-line sheet
        sheet = self.generate_synthetic_40_line_keysheet()
        k2, id2 = parse_key_input(sheet)
        self.assertEqual(len(k2), 16)

        # 3. Invalid key input
        with self.assertRaises(ValueError):
            parse_key_input("")

    def test_fsk_demodulator_robustness_and_edge_cases(self):
        """Verifies FSK demodulator behavior on silence, DC offset, truncated audio, and odd buffer sizes."""
        demod = FSKDemodulator()

        # Pure silence
        silence = np.zeros(44100 * 5, dtype=np.float32)
        self.assertIsNone(demod.demodulate(silence))

        # Truncated audio (<30 bits)
        truncated = np.random.normal(0, 0.1, 44100 * 1)
        self.assertIsNone(demod.demodulate(truncated))

        # DC offset + valid signal
        session_id = 0x11223344
        key_id = 0x55667788
        master_key = os.urandom(16)
        pkt_raw = struct.pack("<2sBII16s", FSK_SYNC_WORD, 1, session_id, key_id, master_key)
        pkt_crc = crc32_ieee(pkt_raw)
        full_pkt = pkt_raw + struct.pack("<I", pkt_crc)

        sr = 44100
        dur = 0.030
        N = int(sr * dur)
        bits = [1, 0] * 12 + [((b >> (7 - bi)) & 1) for b in full_pkt for bi in range(8)]
        audio_segs = [np.zeros(int(sr * 0.05))]
        for b in bits:
            f = FSK_FREQ_MARK if b else FSK_FREQ_SPACE
            t = np.linspace(0, dur, N, endpoint=False)
            audio_segs.append(np.sin(2 * np.pi * f * t))
        audio_segs.append(np.zeros(int(sr * 0.05)))
        audio = np.concatenate(audio_segs)

        # Add heavy DC offset (e.g. +0.40) + noise
        audio += 0.40 + np.random.normal(0, 0.08, len(audio))
        rx = demod.demodulate(audio)
        self.assertIsNotNone(rx, "Demodulator must lock onto signal despite heavy DC offset")
        self.assertEqual(rx["session_id"], session_id)
        self.assertEqual(rx["master_key"], master_key)

    def test_receiver_cli_standalone_and_help(self):
        """Verifies Arch Linux host receiver CLI behavior without arguments and with --test."""
        import subprocess

        receiver_script = BASE_DIR / "tools" / "host_diode" / "seld_diode_receiver.py"

        # 1. Running with no arguments must show help and exit code 0
        proc_help = subprocess.run([sys.executable, str(receiver_script)], capture_output=True, text=True)
        self.assertEqual(proc_help.returncode, 0)
        self.assertIn("usage: seld_diode_receiver.py", proc_help.stdout)

        # 2. Running with --test must exit code 0 and pass all self-tests
        proc_test = subprocess.run([sys.executable, str(receiver_script), "--test"], capture_output=True, text=True)
        self.assertEqual(proc_test.returncode, 0)
        self.assertIn("ALL SELF-TESTS PASSED WITH 100% SUCCESS", proc_test.stdout)

    def test_screengrabber_and_monitor_audio(self):
        """Verifies ScreenGrabber initialization and AudioCaptureThread monitor selection."""
        from tools.host_diode.seld_diode_receiver import ScreenGrabber, AudioCaptureThread, FSKDemodulator

        # 1. ScreenGrabber grab test
        grabber = ScreenGrabber()
        frame = grabber.grab()
        if grabber.has_grim:
            self.assertIsNotNone(frame, "ScreenGrabber via grim must successfully capture desktop frame")
            self.assertEqual(len(frame.shape), 3, "Captured frame must be 3-channel color image")

        # 2. AudioCaptureThread monitor recorder detection
        demod = FSKDemodulator()
        th = AudioCaptureThread(demod, lambda k: None, source_type="monitor")
        cmd, desc = th.find_recorder()
        self.assertIsNotNone(cmd, "AudioCaptureThread must find a valid system audio capture backend")
        self.assertIn("loopback", desc.lower())

    def test_multirate_and_ac97_pulse_wave_demodulation(self):
        """Validates that FSKDemodulator recovers keys from 25% pulse waves and varied bit rates."""
        sr = 44100
        demod = FSKDemodulator(sample_rate=sr, bit_dur_sec=0.030)
        session_id = 0x5E1DCAFE
        key_id = 0x99887766
        master_key = b"TEST_PULSE_KEY_!"
        pkt_raw = struct.pack("<2sBII16s", FSK_SYNC_WORD, 1, session_id, key_id, master_key)
        pkt_crc = crc32_ieee(pkt_raw)
        full_pkt = pkt_raw + struct.pack("<I", pkt_crc)

        bits = [1, 0] * 12 + [((b >> (7 - bi)) & 1) for b in full_pkt for bi in range(8)]

        # Test rates: 26ms, 28.5ms, 30ms, 32ms
        for cand_dur in [0.026, 0.0285, 0.030, 0.032]:
            audio_segs = [np.zeros(int(sr * 0.05))]
            N_s = int(sr * cand_dur)
            for b in bits:
                f = FSK_FREQ_MARK if b else FSK_FREQ_SPACE
                phase = (f * np.arange(N_s) % sr) / sr
                # 25% duty cycle pulse wave (identical to AC97 hardware output)
                pulse = np.where(phase < 0.25, 0.5, -0.5)
                audio_segs.append(pulse)
            audio_segs.append(np.zeros(int(sr * 0.05)))
            audio = np.concatenate(audio_segs)
            audio += np.random.normal(0, 0.04, len(audio)) # Background noise

            res = demod.demodulate(audio)
            self.assertIsNotNone(res, f"Demodulator must decode {cand_dur*1000:.1f}ms AC97 pulse wave")
            self.assertEqual(res["session_id"], session_id)
            self.assertEqual(res["master_key"], master_key)


if __name__ == "__main__":
    unittest.main(verbosity=2)
