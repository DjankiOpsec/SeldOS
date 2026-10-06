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

MIN_FREQ = 215.0
MAX_FREQ = 350.0
SAMPLE_RATE = 44100

# Solemn Requiem Minor Scale [220, 350] Hz:
# 0: A3 (220.0 Hz) - Heavy dark root ground
# 1: C4 (261.6 Hz) - Pure poignant minor third (eradicates 'весело')
# 2: D4 (293.7 Hz) - Solemn subdominant
# 3: E4 (329.6 Hz) - Resonant fifth
# 4: F4 (349.2 Hz) - The weeping minor sixth (tragic bittersweet beauty)
SCALE = [220.0, 261.6, 293.7, 329.6, 349.2]

# Melodic voice leading transition matrix (classical Gregorian motion, guarantees deg != prev_deg)
TRANSITIONS = [
    [1, 2, 3, 1], # From 0 (A3): rise to poignant 3rd (1), 4th (2), or 5th (3)
    [0, 2, 3, 0], # From 1 (C4): fall to dark root (0), or step to 2, 3
    [1, 3, 4, 0], # From 2 (D4): step to 1, 3, weep at 6th (4), or fall to root
    [2, 4, 1, 0], # From 3 (E4): step to 2, weep at 6th (4), or drop to 1, 0
    [3, 2, 1, 3]  # From 4 (F4): sorrowful resolution downwards to 3, 2, 1
]

PACE_SEC = 0.33  # Calm, solemn, steady pace (~3.0 words/sec)

def play_oracle(words: list, seed: int = 12345):
    events = []
    rng = seed
    prev_deg = seed % len(SCALE)

    for w in words:
        whash = 5381
        for c in w:
            whash = ((whash * 33) + ord(c)) & 0xFFFFFFFF
        rng = (rng * 6364136223846793005 + 1442695040888963407 + whash) & 0xFFFFFFFFFFFFFFFF

        dur = PACE_SEC
        if any(c in w for c in '.!?'):
            deg = 0  # Resolve to heavy dark root (220 Hz)
            dur = PACE_SEC * 1.15
        elif any(c in w for c in ',;:'):
            deg = 3 if prev_deg == 1 else 1  # Melancholic suspension on minor 3rd or 5th
            dur = PACE_SEC * 1.05
        else:
            choice = (rng >> 24) % 4
            deg = TRANSITIONS[prev_deg][choice]

        if deg == prev_deg:
            deg = 1 if deg == 0 else 0
        prev_deg = deg

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

def load_words(count: int, seed_str: str = "oracle"):
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

    seed = sum(ord(c) for c in seed_str)
    start = (seed * 12345) % max(1, len(all_words) - count)
    return all_words[start:start + count]

if __name__ == "__main__":
    # Default without count is 160 words (~1 minute of music)
    target_count = 160
    seed_arg = "oracle"

    for arg in sys.argv[1:]:
        if arg.isdigit():
            target_count = int(arg)
        else:
            seed_arg = arg

    seed_val = sum(ord(c) for c in seed_arg)
    words = load_words(target_count, seed_arg)
    play_oracle(words, seed_val)
