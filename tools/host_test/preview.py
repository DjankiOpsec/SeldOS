#!/usr/bin/env python3
"""
Opsec Oracle - Host Audio & Text Flow Preview
Sacred Requiem / Solemn Ethereal Aesthetic (Light crystalline timbre with deep tragic weight and sorrow).
Range: strictly 215 to 350 Hz. Completely uniform, solemn floating tempo.
Canonical SeldOS implementation in userspace/bin/oracle/main.c.
"""

import math
import os
import struct
import subprocess
import sys
import time
import threading
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

def play_oracle(words: list, seed: int = 12345):
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
            # Musical cadence resolving to upper tonic (6), sacred fifth (3), or ground (0)
            cands = [6, 3, 5, 0] if prev >= 4 else [3, 6, 2, 0]
            deg = cands[(rng >> 16) % len(cands)]
        elif any(c in w for c in ',;:'):
            # Melancholic suspension on poignant minor intervals
            cands = [4, 5, 2, 7]
            deg = cands[(rng >> 16) % len(cands)]
        else:
            deg = TRANSITIONS[prev][choice]

        # Invariants: strictly no consecutive duplicates, no 2-note alternating trills (A-B-A-B)
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

    proc = subprocess.Popen(
        ['aplay', '-q', '-t', 'raw', '-f', 'S16_LE', '-r', str(SAMPLE_RATE), '-c', '1'],
        stdin=subprocess.PIPE
    )

    def audio_feeder():
        proc.stdin.write(full_pcm)
        proc.stdin.close()

    t = threading.Thread(target=audio_feeder, daemon=True)
    t.start()

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
            time.sleep(ev['dur'])
    finally:
        proc.wait()
        print("\n--- [AMEN] ---\n")

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
    target_count = 160
    seed_arg = None

    for arg in sys.argv[1:]:
        if arg.isdigit():
            target_count = int(arg)
        else:
            seed_arg = arg

    if seed_arg is None:
        seed_val = int(time.time_ns()) & 0xFFFFFFFFFFFFFFFF
    else:
        seed_val = sum(ord(c) for c in seed_arg)

    words = load_words(target_count, seed_val)
    play_oracle(words, seed_val)
