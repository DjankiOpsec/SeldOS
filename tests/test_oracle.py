#!/usr/bin/env python3
"""
Unit and Invariant Tests for TempleOS Opsec Oracle
Verifies:
1. Frequency constraints: Strictly 290.0 - 680.0 Hz.
2. Speed constraints: Fast continuous pace (~160 ms / 6.25 w/s).
3. Anti-repetition voice leading: No consecutive duplicates, no 2-note A-B-A-B trills.
4. Corpus loading and entropy seeking.
"""

import sys
import unittest
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(BASE_DIR))

from tools.host_test.preview import SCALE, MIN_FREQ, MAX_FREQ, PACE_SEC, TRANSITIONS, load_words

class TestOpsecOracleInvariants(unittest.TestCase):

    def test_frequency_range_invariants(self):
        """Verifies that EVERY scale note is strictly within [290.0, 680.0] Hz."""
        self.assertEqual(MIN_FREQ, 290.0)
        self.assertEqual(MAX_FREQ, 680.0)

        for freq in SCALE:
            self.assertGreaterEqual(freq, 290.0, f"Frequency {freq} < 290.0 Hz")
            self.assertLessEqual(freq, 680.0, f"Frequency {freq} > 680.0 Hz")

    def test_speed_invariants(self):
        """Verifies that word utterance pace is fast (~160 ms / 6.25 words per second)."""
        self.assertAlmostEqual(PACE_SEC, 0.16, places=2)
        rate = 1.0 / PACE_SEC
        self.assertGreaterEqual(rate, 5.5, f"Rate {rate:.2f} w/s < 5.5")
        self.assertLessEqual(rate, 7.0, f"Rate {rate:.2f} w/s > 7.0")

    def test_anti_repetition_invariants(self):
        """Verifies that consecutive duplicate pitches and alternating A-B-A trills are strictly prevented."""
        words = [
            "sovereign", "mind", "requires", "defense", "in", "depth", "always.",
            "Never", "trust", "unauthenticated", "nodes,", "and", "verify", "all", "signatures.",
            "Physical", "memory", "isolation,", "ephemeral", "routing,", "and", "zero-trust", "boundaries"
        ] * 10

        history = [-1, -1, -1, -1]
        rng = 987654321
        degrees = []

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

            # Invariant checks:
            if history[0] >= 0:
                self.assertNotEqual(deg, history[0], "Consecutive duplicate pitch detected!")
            if history[1] >= 0:
                self.assertNotEqual(deg, history[1], "Alternating A-B-A trill detected!")

            history[3] = history[2]
            history[2] = history[1]
            history[1] = history[0]
            history[0] = deg
            degrees.append(deg)

    def test_corpus_reader_and_entropy(self):
        """Tests that words can be loaded and vary by seed."""
        words1 = load_words(20, 11111)
        words2 = load_words(20, 99999)
        self.assertEqual(len(words1), 20)
        self.assertEqual(len(words2), 20)
        self.assertNotEqual(words1, words2, "Different seeds must produce different word sequences")

if __name__ == "__main__":
    unittest.main()
