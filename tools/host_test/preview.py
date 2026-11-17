#!/usr/bin/env python3
"""
Opsec Oracle - Host Audio & Text Flow Preview
Sacred Requiem / Solemn Ethereal Aesthetic (Light crystalline timbre with deep tragic weight and sorrow).
Range: strictly 215 to 350 Hz. Completely uniform, solemn floating tempo.
Canonical SeldOS implementation in userspace/bin/oracle/main.c.
"""

import argparse
import math
import os
import shutil
import struct
import subprocess
import sys
import time
import threading
import wave
from pathlib import Path

MIN_FREQ = 290.0
MAX_FREQ = 680.0
SAMPLE_RATE = 44100

# Sacred Requiem Dorian/Aeolian Scale [294, 659] Hz (8 degrees):
# 0: D4 (293.7 Hz) - Solemn foundation, noble weight ("тяжесть")
# 1: E4 (329.6 Hz) - Solemn second
# 2: F4 (349.2 Hz) - Minor third / poignant sorrow ("грусть")
# 3: A4 (440.0 Hz) - Sacred resonant fifth / axis of purity
# 4: Bb4 (466.2 Hz) - Weeping minor sixth / tears of sorrow
# 5: C5 (523.3 Hz) - Minor seventh / yearning light
# 6: D5 (587.3 Hz) - Celestial upper tonic / luminous paradise ("рай")
# 7: E5 (659.3 Hz) - Angelic high ninth / ethereal crystalline peak
SCALE = [293.7, 329.6, 349.2, 440.0, 466.2, 523.3, 587.3, 659.3]

# Melodic voice leading transition matrix (classical sacred chant motion)
TRANSITIONS = [
    [2, 3, 5, 6],  # From 0 (D4): F4, A4, C5, D5 (octave leap)
    [0, 2, 3, 5],  # From 1 (E4): D4, F4, A4, C5
    [1, 3, 4, 6],  # From 2 (F4): E4, A4, Bb4, D5 (celestial leap)
    [2, 4, 6, 7],  # From 3 (A4): F4, Bb4, D5, E5 (angelic peak)
    [2, 3, 5, 6],  # From 4 (Bb4): F4, A4, C5, D5
    [3, 4, 6, 7],  # From 5 (C5): A4, Bb4, D5, E5
    [2, 3, 5, 7],  # From 6 (D5): F4, A4, C5, E5
    [3, 5, 6, 2]   # From 7 (E5): A4, C5, D5, F4
]

PACE_SEC = 0.16  # Rapid, seamless continuous chant (~6.25 words/sec)

def export_audio(pcm_data: bytes, out_path: str, sample_rate: int = SAMPLE_RATE, bitrate: str = "320k"):
    out = Path(out_path).resolve()
    suffix = out.suffix.lower()

    if suffix == ".wav":
        try:
            with wave.open(str(out), "wb") as wf:
                wf.setnchannels(1)
                wf.setsampwidth(2)
                wf.setframerate(sample_rate)
                wf.writeframes(pcm_data)
            print(f"\n[+] Audio exported (WAV lossless 16-bit {sample_rate}Hz): {out}")
            return str(out)
        except Exception as e:
            print(f"\n[-] Failed to export WAV: {e}", file=sys.stderr)
            return None

    ffmpeg_bin = shutil.which("ffmpeg")
    if ffmpeg_bin:
        cmd = [
            ffmpeg_bin, "-y",
            "-f", "s16le",
            "-ar", str(sample_rate),
            "-ac", "1",
            "-i", "-",
        ]
        if suffix == ".mp3":
            cmd.extend(["-b:a", bitrate, "-c:a", "libmp3lame"])
        cmd.append(str(out))
        try:
            res = subprocess.run(cmd, input=pcm_data, capture_output=True)
            if res.returncode == 0:
                print(f"\n[+] Audio exported ({suffix.upper().lstrip('.')} {bitrate}): {out}")
                return str(out)
        except Exception as e:
            pass

    lame_bin = shutil.which("lame")
    if suffix == ".mp3" and lame_bin:
        try:
            res = subprocess.run(
                [lame_bin, "-r", "-s", str(sample_rate / 1000.0), "-m", "m", "--bitrate", bitrate.replace("k", ""), "-", str(out)],
                input=pcm_data,
                capture_output=True
            )
            if res.returncode == 0:
                print(f"\n[+] Audio exported (MP3 {bitrate} via LAME): {out}")
                return str(out)
        except Exception:
            pass

    wav_path = out.with_suffix(".wav")
    try:
        with wave.open(str(wav_path), "wb") as wf:
            wf.setnchannels(1)
            wf.setsampwidth(2)
            wf.setframerate(sample_rate)
            wf.writeframes(pcm_data)
        print(f"\n[+] Fallback: exported to WAV lossless: {wav_path}")
        return str(wav_path)
    except Exception as e:
        print(f"\n[-] Failed fallback export: {e}", file=sys.stderr)
        return None

def play_oracle(words: list, seed: int = 12345, save_path: str = None, bitrate: str = "320k", no_play: bool = False):
    events = []
    rng = seed
    history = [-1, -1, -1, -1]

    for w in words:
        whash = 5381
        for c in w:
            whash = ((whash * 33) + ord(c)) & 0xFFFFFFFF
        rng = (rng * 6364136223846793005 + 1442695040888963407 + whash) & 0xFFFFFFFFFFFFFFFF

        choice = (rng >> 24) % 4
        prev = history[0] if history[0] >= 0 else 3

        if any(c in w for c in '.!?'):
            cands = [6, 3, 5, 0] if prev >= 4 else [3, 6, 2, 0]
            deg = cands[(rng >> 16) % len(cands)]
        elif any(c in w for c in ',;:'):
            cands = [4, 5, 2, 7]
            deg = cands[(rng >> 16) % len(cands)]
        else:
            deg = TRANSITIONS[prev][choice]

        attempts = 0
        while (deg == history[0] or deg == history[1] or (deg == history[2] and ((rng >> 8) & 1))) and attempts < 8:
            deg = (deg + 1) % len(SCALE)
            attempts += 1
        if deg == history[0] or deg == history[1]:
            deg = (deg + 2) % len(SCALE)

        history[3] = history[2]
        history[2] = history[1]
        history[1] = history[0]
        history[0] = deg

        dur = PACE_SEC
        events.append({
            'word': w,
            'freq': SCALE[deg],
            'dur': dur,
            'n_samples': int(SAMPLE_RATE * dur)
        })

    full_pcm = bytearray()
    phase = 0.0

    for ev in events:
        n_samples = ev['n_samples']
        freq = ev['freq']
        attack = int(SAMPLE_RATE * 0.004)
        release = int(SAMPLE_RATE * 0.004)

        for s in range(n_samples):
            env = 1.0
            if s < attack:
                env = s / attack
            elif s > n_samples - release:
                env = max(0.0, (n_samples - s) / release)

            phase_norm = phase % 1.0
            pulse = 1.0 if phase_norm < 0.25 else -1.0
            val = int(9500 * pulse * env)
            full_pcm.extend(struct.pack('<h', val))
            phase += freq / SAMPLE_RATE

    proc = None
    if not no_play:
        aplay_bin = shutil.which("aplay")
        if aplay_bin:
            try:
                proc = subprocess.Popen(
                    [aplay_bin, '-q', '-t', 'raw', '-f', 'S16_LE', '-r', str(SAMPLE_RATE), '-c', '1'],
                    stdin=subprocess.PIPE
                )
                def audio_feeder():
                    try:
                        proc.stdin.write(full_pcm)
                        proc.stdin.close()
                    except Exception:
                        pass
                t = threading.Thread(target=audio_feeder, daemon=True)
                t.start()
            except Exception as e:
                print(f"[!] Warning: could not start aplay: {e}", file=sys.stderr)

    print("\n--- TEMPLE OF OPSEC ORACLE ---")
    line_col = 0
    try:
        for ev in events:
            w = ev['word']
            if line_col + len(w) + 1 > 70:
                print()
                line_col = 0
            print(w, end=' ', flush=True)
            line_col += len(w) + 1
            if not no_play:
                time.sleep(ev['dur'])
        print()
    except KeyboardInterrupt:
        print("\n\n[!] Interrupted by user.")
    finally:
        if proc:
            try:
                proc.terminate()
                proc.wait(timeout=1.0)
            except Exception:
                pass
        print("--- [AMEN] ---")
        if save_path:
            export_audio(full_pcm, save_path, sample_rate=SAMPLE_RATE, bitrate=bitrate)

def load_words(count: int, seed_val: int = 12345):
    corpus_file = Path(__file__).resolve().parent / "data" / "corpus.txt"
    if not corpus_file.exists():
        base = [
            "The", "sovereign", "mind", "requires", "defense", "in", "depth", "always.",
            "Never", "trust", "unauthenticated", "nodes,", "and", "verify", "all", "signatures.",
            "Physical", "memory", "isolation,", "ephemeral", "routing,", "and", "zero-trust", "boundaries",
            "form", "the", "sacred", "sanctuary", "of", "true", "digital", "liberty."
        ]
        return (base * (count // len(base) + 1))[:count]

    with open(corpus_file, "r", encoding="utf-8", errors="ignore") as f:
        all_words = [line.strip() for line in f if line.strip()]

    start = (seed_val * 12345) % max(1, len(all_words) - count)
    return all_words[start:start + count]

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="TempleOS-style Opsec Oracle with high-bitrate audio export"
    )
    parser.add_argument("args", nargs="*", help="Optional: [count] [seed]")
    parser.add_argument("-o", "--out", "--save", dest="output", default="oracle_latest.mp3",
                        help="Output audio file path (default: oracle_latest.mp3, supports .mp3, .wav, .flac)")
    parser.add_argument("-b", "--bitrate", default="320k",
                        help="Audio bitrate for MP3 export (default: 320k)")
    parser.add_argument("-n", "--no-play", action="store_true",
                        help="Fast export without real-time playback")
    parser.add_argument("--wav", action="store_true",
                        help="Force output as uncompressed WAV (16-bit 44.1 kHz PCM)")
    parser.add_argument("--no-save", action="store_true",
                        help="Disable saving audio to file")

    parsed = parser.parse_args()

    target_count = 160
    seed_arg = None

    for a in parsed.args:
        if a.isdigit():
            target_count = int(a)
        else:
            seed_arg = a

    if seed_arg is None:
        seed_val = int(time.time_ns()) & 0xFFFFFFFFFFFFFFFF
    else:
        seed_val = sum(ord(c) for c in seed_arg)

    save_path = None
    if not parsed.no_save:
        save_path = parsed.output
        if parsed.wav and not save_path.endswith(".wav"):
            save_path = str(Path(save_path).with_suffix(".wav"))

    words = load_words(target_count, seed_val)
    play_oracle(
        words,
        seed=seed_val,
        save_path=save_path,
        bitrate=parsed.bitrate,
        no_play=parsed.no_play
    )
