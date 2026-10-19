#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Offline stage transport tests; event delivery only, no MIDI devices or sleeps."""
import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import perf_capture as perf
import stage_capture as stage


class Fake:
    def __init__(self, index, words, bad=False, schema=5):
        self.index, self.words, self.bad, self.schema = index, words, bad, schema
        self.callback = None
        self.closed = False

    def subscribe(self, callback):
        self.callback = callback

    def send(self, packet):
        decoded = perf.unpack7(packet[1:-1])
        assert decoded[10:14] == bytes([5, self.index, 0, 0])
        self.callback(perf.build_response(123, self.index, 7, self.schema, int(self.bad),
            self.words[self.index*6:self.index*6+6]))

    def close(self):
        self.closed = True


class StageCapture(unittest.TestCase):
    def test_reads_exact_frozen_pages_and_closes_every_transport(self):
        words = [5, 123, 8192] + list(range(39))
        opened = []

        def factory():
            transport = Fake(len(opened), words)
            opened.append(transport)
            return transport

        self.assertEqual(stage.capture_pages(factory, 123, 1), words)
        self.assertEqual(len(opened), 7)
        self.assertTrue(all(t.closed for t in opened))

    def test_rejects_stale_capture_and_closes_port(self):
        transport = Fake(0, [5, 123, 8192] + [0]*39, True)
        with self.assertRaises(stage.StageError):
            stage.capture_pages(lambda: transport, 123, 1)
        self.assertTrue(transport.closed)

    def test_rejects_wrong_profile_window(self):
        opened = []
        words = [5, 123, 8191] + [0]*39

        def factory():
            transport = Fake(len(opened), words)
            opened.append(transport)
            return transport

        with self.assertRaises(stage.StageError):
            stage.capture_pages(factory, 123, 1)

    def test_rejects_older_two_bank_schema(self):
        transport = Fake(0, [4, 123, 8192] + [0]*33, schema=4)
        with self.assertRaises(stage.StageError):
            stage.capture_pages(lambda: transport, 123, 1)
        self.assertTrue(transport.closed)


if __name__ == "__main__":
    unittest.main()
