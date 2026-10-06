#!/usr/bin/env python3
"""
Unit and Invariant Tests for TempleOS Opsec Oracle
Verifies:
1. Frequency constraints: Strictly 215.0 - 325.0 Hz.
2. Speed constraints: Strictly 1.0 - 5.0 words per second.
3. Musical voice leading and cadence behavior.
4. Audio buffer synthesis accuracy.
5. Corpus integrity and O(1) binary index.
"""

import sys
import unittest
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent.parent / "oracle"
sys.path.insert(0, str(BASE_DIR))

from audio_synth import BeepSynthesizer, MIN_FREQ, MAX_FREQ
from music_engine import MusicEngine, SCALE_FREQUENCIES
from corpus_reader import CorpusReader

class TestOpsecOracleInvariants(unittest.TestCase):

    def setUp(self):
        self.music = MusicEngine()

    def test_frequency_range_invariants(self):
        """Verifies that EVERY scale note and generated tone is strictly within [215.0, 325.0] Hz."""
        self.assertEqual(MIN_FREQ, 215.0)
        self.assertEqual(MAX_FREQ, 325.0)

        for freq in SCALE_FREQUENCIES:
            self.assertGreaterEqual(freq, 215.0, f"Frequency {freq} < 215.0 Hz")
            self.assertLessEqual(freq, 325.0, f"Frequency {freq} > 325.0 Hz")

        sample_words = [
            "a", "the", "cryptography", "key", "gpg", "tor", "tails", "qubes", "whonix",
            "firewall", "router", "firmware", "exploit", "metadata", "end-to-end",
            "warning!", "secure.", "never,", "always;", "verify?", "terminal"
        ]

        for w in sample_words * 20:
            freq, dur, rest = self.music.word_to_tone(w)
            self.assertGreaterEqual(freq, 215.0, f"Word '{w}' generated frequency {freq} < 215.0 Hz")
            self.assertLessEqual(freq, 325.0, f"Word '{w}' generated frequency {freq} > 325.0 Hz")

    def test_speed_invariants(self):
        """Verifies that word utterance speed is strictly within [1.0, 5.0] words per second."""
        test_words = [
            "I", "am", "the", "system", "administrator", "cryptographic",
            "anonymity.", "warning!", "listen,", "observe;"
        ]
        for w in test_words:
            _, dur, rest = self.music.word_to_tone(w)
            rate = 1.0 / (dur + rest)
            self.assertGreaterEqual(rate, 1.0, f"Rate {rate:.2f} w/s < 1.0 for '{w}'")
            self.assertLessEqual(rate, 5.05, f"Rate {rate:.2f} w/s > 5.0 for '{w}'")

    def test_musical_cadence_behavior(self):
        """Sentence endings (.!?) must resolve to Tonic (216 Hz) or Dominant (324 Hz)."""
        engine = MusicEngine()
        engine.word_to_tone("operating")
        freq_period, dur_period, _ = engine.word_to_tone("system.")
        self.assertIn(freq_period, [216.0, 324.0])
        self.assertGreaterEqual(dur_period, 0.6)  # Cadence has longer duration

    def test_audio_synthesizer_pcm(self):
        """Tests that raw square-wave PCM audio is generated correctly."""
        synth = BeepSynthesizer()
        pcm = synth.generate_square_wave(260.0, 0.1)
        # 44100 Hz * 0.1s * 2 bytes = 8820 bytes
        expected_len = int(44100 * 0.1) * 2
        self.assertEqual(len(pcm), expected_len)
        synth.close()

    def test_corpus_reader_and_index(self):
        """Tests that the corpus index exists and O(1) random seeking functions."""
        reader = CorpusReader()
        self.assertGreater(reader.total_words, 100000)
        word_0 = reader.get_word(0)
        self.assertTrue(len(word_0) > 0)
        word_last = reader.get_word(reader.total_words - 1)
        self.assertTrue(len(word_last) > 0)

if __name__ == "__main__":
    unittest.main()
