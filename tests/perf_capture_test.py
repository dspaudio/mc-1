#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""perf_capture 순수 호스트 테스트. 장치/포트/mido 접근 없음, sleep 없음."""
import json
import os
import sys
import tempfile
import threading
import unittest
from unittest import mock

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))
import perf_capture as P  # noqa: E402

TIMEOUT = 5.0  # 이벤트 대기 상한 (정상 경로에서는 즉시 반환)


def make_words(session, render_sum=8192 * 1000, con=0x109, prd=0xFFFFFFFF):
    w = [0] * P.WORDS
    w[0], w[1], w[2], w[3] = P.SCHEMA, session, P.BLOCKS, P.BLOCKS * P.BLOCK_FRAMES
    w[4] = 0x80000000
    w[5], w[6] = render_sum & 0xFFFFFFFF, render_sum >> 32
    w[7] = -(-render_sum // P.BLOCKS) + 5
    w[8], w[10], w[11] = P.BLOCKS * 10, 20, 100
    w[24], w[25] = con, prd
    return w


def frames_for(session, words, order=range(P.PIECES)):
    return [P.build_response(session, i, P.PIECES, P.SCHEMA, 0, words[6 * i:6 * i + 6])
            for i in order]


class FakeTransport:
    """가짜 전송. send() 시점에 frames를 deliver 정책대로 콜백에 전달한다."""

    def __init__(self, frames=(), threaded=False):
        self.frames = list(frames)
        self.threaded = threaded
        self.events = []
        self.sent = []
        self.cb = None
        self.closed = False
        self.thread = None

    def subscribe(self, cb):
        self.events.append("subscribe")
        self.cb = cb

    def send(self, pkt):
        self.events.append("send")
        self.sent.append(pkt)
        if self.threaded:
            self.thread = threading.Thread(target=self._deliver)
            self.thread.start()
        else:
            self._deliver()

    def _deliver(self):
        for f in self.frames:
            self.cb(f)

    def close(self):
        self.closed = True
        if self.thread:
            self.thread.join(TIMEOUT)


def run(frames, session=0x1234, timeout=TIMEOUT, threaded=False):
    t = FakeTransport(frames, threaded)
    return t, P.capture(t, session, timeout, "NIGHT CIRCUIT", "default")


class Codec(unittest.TestCase):
    def test_pack7_roundtrip(self):
        data = bytes(range(256))
        self.assertEqual(P.unpack7(P.pack7(data)), data)

    def test_request_layout(self):
        u = P.unpack7(P.build_request(0x01020304)[1:-1])
        self.assertEqual(list(u[:6]), [0, 0x59, 0x70, 8, 0, 0])
        self.assertEqual(bytes(u[6:14]), bytes([4, 3, 2, 1, 1, 0, 0, 0]))
        self.assertEqual(u[14], ~sum(u[6:14]) & 0xFF)

    def test_session_zero_rejected(self):
        with self.assertRaises(ValueError):
            P.build_request(0)

    def test_response_parse(self):
        r = P.parse_response(P.build_response(7, 2, 7, 1, 0, [1, 2, 3, 4, 5, 6]))
        self.assertEqual((r["session"], r["index"], r["total"], r["data"]), (7, 2, 7, [1, 2, 3, 4, 5, 6]))

    def test_delimiters_both_required(self):
        f = P.build_response(0x1234, 0, 7, 1, 0, [1] * 6)
        cases = {
            "no_f0": f[1:], "no_f7": f[:-1], "bare": f[1:-1],
            "prefix": b"\x00" + f, "trailing": f + b"\x00",
        }
        for name, raw in cases.items():
            with self.subTest(case=name):
                with self.assertRaises(P.FrameError) as cm:
                    P.parse_response(raw)
                self.assertEqual(cm.exception.kind, "framing")
                _, rec = run([raw])
                self.assertEqual(rec["status"], "bad_frame")

    def test_concatenated_or_tiny_frames_rejected(self):
        f = P.build_response(0x1234, 0, 7, 1, 0, [1] * 6)
        with self.assertRaises(P.FrameError):
            P.parse_response(f + f)
        for raw in (b"", b"\xF0", b"\xF7"):
            with self.subTest(raw=raw):
                with self.assertRaises(P.FrameError):
                    P.parse_response(raw)


class Capture(unittest.TestCase):
    def test_immediate_callback_after_subscribe(self):
        w = make_words(0x1234)
        t, rec = run(frames_for(0x1234, w))
        self.assertEqual(t.events, ["subscribe", "send"])
        self.assertEqual(rec["status"], "ok")
        self.assertEqual(rec["words"], w)
        self.assertEqual(rec["missing"], [])
        self.assertTrue(t.closed)
        self.assertGreaterEqual(rec["host_elapsed_ns"], 0)

    def test_out_of_order_async(self):
        w = make_words(0x1234)
        t, rec = run(frames_for(0x1234, w, order=[6, 2, 0, 5, 1, 4, 3]), threaded=True)
        self.assertEqual(rec["status"], "ok")
        self.assertEqual(rec["words"], w)

    def test_foreign_and_other_session_ignored(self):
        w = make_words(0x1234)
        foreign_cmd = P._frame(0x72, bytes(32))
        foreign_mfr = b"\xF0" + P.pack7(bytes([0, 0x41, 0x71]) + bytes(36)) + b"\xF7"
        other = P.build_response(0x9999, 0, 7, 1, 0, [1] * 6)
        _, rec = run([foreign_cmd, foreign_mfr, other] + frames_for(0x1234, w))
        self.assertEqual(rec["status"], "ok")
        self.assertEqual((rec["foreign"], rec["other_session"]), (2, 1))

    def test_other_session_never_completes(self):
        w = make_words(0x9999)
        _, rec = run(frames_for(0x9999, w), session=0x1234, timeout=0.01)
        self.assertEqual(rec["status"], "timeout")
        self.assertIsNone(rec["words"])

    def test_corrupt_checksum(self):
        bad = bytearray(P.build_response(0x1234, 0, 7, 1, 0, [1] * 6))
        bad[-2] ^= 0x01
        _, rec = run([bytes(bad)])
        self.assertEqual((rec["status"], rec["detail"][:8]), ("bad_frame", "checksum"))

    def test_short_frame(self):
        full = P.build_response(0x1234, 0, 7, 1, 0, [1] * 6)
        _, rec = run([full[:20] + b"\xF7"])
        self.assertEqual(rec["status"], "bad_frame")
        self.assertTrue(rec["detail"].startswith("short"))

    def test_length_mismatch(self):
        u = bytearray(P.unpack7(P.build_response(0x1234, 0, 7, 1, 0, [1] * 6)[1:-1]))
        u[3] = 31
        _, rec = run([b"\xF0" + P.pack7(bytes(u)) + b"\xF7"])
        self.assertTrue(rec["detail"].startswith("length"))

    def test_non7bit_byte(self):
        _, rec = run([b"\xF0\x00\x80\xF7"])
        self.assertTrue(rec["detail"].startswith("encoding"))

    def test_unsupported_schema(self):
        _, rec = run([P.build_response(0x1234, 0, 7, 2, 0, [1] * 6)])
        self.assertEqual(rec["status"], "bad_schema")

    def test_unsupported_status(self):
        _, rec = run([P.build_response(0x1234, 0, 7, 1, 3, [1] * 6)])
        self.assertEqual(rec["status"], "bad_status")

    def test_busy_and_invalid(self):
        _, rec = run([P.build_response(0x1234, 0, 0, 1, 1, [])])
        self.assertEqual(rec["status"], "busy")
        _, rec = run([P.build_response(0x1234, 0, 0, 1, 2, [])])
        self.assertEqual(rec["status"], "invalid")

    def test_error_response_with_data_is_corrupt(self):
        _, rec = run([P.build_response(0x1234, 0, 0, 1, 1, [5])])
        self.assertEqual(rec["status"], "bad_frame")

    def test_duplicate_piece_fails(self):
        f = frames_for(0x1234, make_words(0x1234))
        _, rec = run([f[0], f[1], f[1]] + f[2:])
        self.assertEqual(rec["status"], "duplicate")
        self.assertIsNone(rec["words"])

    def test_bad_total_and_index(self):
        _, rec = run([P.build_response(0x1234, 0, 6, 1, 0, [1] * 6)])
        self.assertEqual(rec["status"], "bad_frame")
        _, rec = run([P.build_response(0x1234, 7, 7, 1, 0, [1] * 6)])
        self.assertEqual(rec["status"], "bad_frame")

    def test_fixed_blocks_required(self):
        w = make_words(0x1234)
        w[2] = 4096
        _, rec = run(frames_for(0x1234, w))
        self.assertEqual(rec["status"], "bad_words")

    def test_uac_flags_reserved_bits_rejected(self):
        for idx in (39, 40):
            w = make_words(0x1234)
            w[idx] = 0x8
            _, rec = run(frames_for(0x1234, w))
            self.assertEqual(rec["status"], "bad_words", idx)

    def test_incomplete_cancelled_on_timeout(self):
        w = make_words(0x1234)
        t, rec = run(frames_for(0x1234, w)[:3], timeout=0.01)
        self.assertEqual(rec["status"], "timeout")
        self.assertEqual(rec["missing"], [3, 4, 5, 6])
        self.assertIsNone(rec["words"])
        self.assertTrue(t.closed)
        t.cb(frames_for(0x1234, w)[3])  # 취소 뒤 늦은 콜백은 무시
        self.assertEqual(rec["missing"], [3, 4, 5, 6])

    def test_no_missing_completion_claim(self):
        _, rec = run([], timeout=0.01)
        self.assertEqual((rec["status"], rec["missing"]), ("timeout", list(range(7))))

    def test_backpressure_gated_delivery(self):
        w = make_words(0x1234)
        frames = frames_for(0x1234, w)
        col = P.Collector(0x1234)
        gates = [threading.Event() for _ in frames]

        def producer():
            for g, f in zip(gates, frames):
                g.wait(TIMEOUT)
                col.feed(f)

        th = threading.Thread(target=producer)
        th.start()
        for g in gates[:-1]:
            g.set()
        self.assertFalse(col.done.is_set())
        gates[-1].set()
        self.assertTrue(col.done.wait(TIMEOUT))
        th.join(TIMEOUT)
        self.assertEqual(col.status, "ok")


class Offline(unittest.TestCase):
    def test_no_port_access_offline(self):
        sys.modules.pop("mido", None)
        w = make_words(0x1234)
        text = "\n".join(f.hex() + "  # piece" for f in frames_for(0x1234, w))
        rec = P.decode_frames_text(text, 0x1234, "NIGHT CIRCUIT", "default")
        self.assertEqual(rec["status"], "ok")
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, "r.json")
            with open(p, "w") as f:
                json.dump(rec, f)
            self.assertEqual(P.main(["report", p]), 0)
            self.assertEqual(P.main(["compare", "--baseline", p, "--candidate", p]), 1)
        self.assertNotIn("mido", sys.modules)

    def test_capture_requires_explicit_args(self):
        with self.assertRaises(SystemExit):
            P.main(["capture"])
        with self.assertRaises(SystemExit):
            P.main([])

    def test_us_conversion_gated(self):
        rec = {"workload": "w", "settings": "s", "session": 1, "status": "ok", "host_elapsed_ns": 5,
               "words": make_words(1)}
        self.assertIn("elapsed_us=", P.format_report(rec, tick_hz=1e6))
        self.assertNotIn("elapsed_us=", P.format_report(rec))
        rec["words"] = make_words(1, con=0x108)
        self.assertNotIn("elapsed_us=", P.format_report(rec, tick_hz=1e6))
        rec["words"] = make_words(1, prd=0xFFFF)
        self.assertNotIn("elapsed_us=", P.format_report(rec, tick_hz=1e6))


class CliBoundary(unittest.TestCase):
    ARGS = ["capture", "--port", "p", "--output", "o.json", "--workload", "w", "--settings", "s"]

    def test_invalid_inputs_never_construct_transport(self):
        cases = [("0", "1"), ("-1", "1"), (str(1 << 32), "1"), ("1", "0"), ("1", "-1"),
                 ("1", "nan"), ("1", "inf"), ("1", "-inf"), ("1", "1e400")]
        for session, timeout in cases:
            with self.subTest(session=session, timeout=timeout):
                with mock.patch.object(P, "MidoTransport") as ctor:
                    with self.assertRaises(SystemExit) as cm:
                        P.main(self.ARGS + ["--session", session, "--timeout", timeout])
                    self.assertEqual(cm.exception.code, 2)
                    ctor.assert_not_called()

    def test_valid_inputs_reach_transport_constructor(self):
        w = make_words(0x1234)
        fake = FakeTransport(frames_for(0x1234, w))
        with tempfile.TemporaryDirectory() as d:
            out = os.path.join(d, "o.json")
            args = list(self.ARGS)
            args[args.index("o.json")] = out
            with mock.patch.object(P, "MidoTransport", return_value=fake) as ctor:
                self.assertEqual(P.main(args + ["--session", "0x1234", "--timeout", "5"]), 0)
            ctor.assert_called_once_with("p")
            with open(out) as f:
                self.assertEqual(json.load(f)["status"], "ok")


def rec_of(session, render_per_block, workload="NIGHT CIRCUIT", settings="default", **kw):
    return {"format": "perf_capture/1", "workload": workload, "settings": settings,
            "session": session, "status": "ok", "missing": [],
            "words": make_words(session, render_per_block * P.BLOCKS, **kw)}


class Compare(unittest.TestCase):
    def sides(self, base=1000, cand=700, n=3, **ckw):
        return ([rec_of(i + 1, base) for i in range(n)],
                [rec_of(i + 100, cand, **ckw) for i in range(n)])

    def test_exact_threshold_pass(self):
        v, _, ratio = P.compare(*self.sides(1000, 700))
        self.assertEqual(v, "PASS")
        self.assertEqual(float(ratio), 0.7)

    def test_above_threshold_fail(self):
        self.assertEqual(P.compare(*self.sides(1000, 701))[0], "FAIL")

    def test_mean_not_best_run(self):
        b, c = self.sides(1000, 600)
        c[2] = rec_of(102, 2000)
        self.assertEqual(P.compare(b, c)[0], "FAIL")

    def test_fewer_than_three_repeats_invalid(self):
        self.assertEqual(P.compare(*self.sides(n=2))[0], "INVALID")

    def test_count_mismatch_invalid(self):
        b, c = self.sides(n=4)
        self.assertEqual(P.compare(b, c[:3])[0], "INVALID")

    def test_incomplete_repeat_invalid(self):
        b, c = self.sides()
        c[1]["status"], c[1]["words"] = "timeout", None
        self.assertEqual(P.compare(b, c)[0], "INVALID")

    def test_workload_settings_mismatch_invalid(self):
        b, c = self.sides()
        c[0]["workload"] = "NEON SPIRAL"
        self.assertEqual(P.compare(b, c)[0], "INVALID")
        b, c = self.sides()
        c[0]["settings"] = "other"
        self.assertEqual(P.compare(b, c)[0], "INVALID")

    def test_timer_config_mismatch_invalid(self):
        self.assertEqual(P.compare(*self.sides(con=0x108))[0], "INVALID")

    def test_duplicate_session_invalid(self):
        b, c = self.sides()
        c[1]["session"] = c[0]["session"]
        c[1]["words"][1] = c[0]["session"]
        self.assertEqual(P.compare(b, c)[0], "INVALID")

    def test_wrong_blocks_invalid(self):
        b, c = self.sides()
        c[0]["words"][2] = 4096
        self.assertEqual(P.compare(b, c)[0], "INVALID")

    def test_min_repeats_not_weakenable(self):
        b, c = self.sides(n=2)
        with self.assertRaises(TypeError):
            P.compare(b, c, min_repeats=1)
        self.assertGreaterEqual(P.MIN_REPEATS, 3)

    def test_state_mismatch_across_records_invalid(self):
        for a, z in P.STABLE_PAIRS + ((37, 38),):
            with self.subTest(words=(a, z)):
                b, c = self.sides(300, 100)
                for r in c:
                    r["words"][a] = r["words"][z] = 5
                self.assertEqual(P.compare(b, c)[0], "INVALID")

    def test_state_change_within_capture_invalid(self):
        for a, z in P.STABLE_PAIRS:
            with self.subTest(words=(a, z)):
                b, c = self.sides(300, 100)
                c[0]["words"][z] = 1
                self.assertEqual(P.compare(b, c)[0], "INVALID")
                b, c = self.sides(300, 100)
                for r in b + c:
                    r["words"][z] = 1
                self.assertEqual(P.compare(b, c)[0], "INVALID")

    def test_zero_or_inconsistent_sums_invalid(self):
        def zero_render(w):
            w[5] = w[6] = 0

        def zero_audio(w):
            w[8] = w[9] = 0

        def zero_elapsed(w):
            w[4] = 0

        def render_max_too_small(w):
            w[7] = 1

        def render_over_elapsed(w):
            w[4] = 1

        def timer5_sum_without_count(w):
            w[14], w[16] = 5, 0

        for mutate in (zero_render, zero_audio, zero_elapsed, render_max_too_small,
                       render_over_elapsed, timer5_sum_without_count):
            with self.subTest(case=mutate.__name__):
                b, c = self.sides(300, 100)
                for r in c:
                    mutate(r["words"])
                self.assertEqual(P.compare(b, c)[0], "INVALID")

    def test_unsupported_timer4_basis_invalid_even_if_equal(self):
        for kw in ({"con": 0x108}, {"prd": 0xFFFF}):
            with self.subTest(kw=kw):
                b = [rec_of(i + 1, 1000, **kw) for i in range(3)]
                c = [rec_of(i + 100, 700, **kw) for i in range(3)]
                self.assertEqual(P.compare(b, c)[0], "INVALID")

    def test_non_u32_words_invalid(self):
        for bad in (-1, 1 << 32, True, 1.0, "1"):
            with self.subTest(bad=bad):
                b, c = self.sides()
                c[0]["words"][12] = bad
                self.assertEqual(P.compare(b, c)[0], "INVALID")

    def test_valid_equal_state_still_passes(self):
        b, c = self.sides()
        for r in b + c:
            r["words"][29:41] = [120, 120, 1, 1, 0, 0, 1, 1, 7, 9, 3, 3]
        self.assertEqual(P.compare(b, c)[0], "PASS")


if __name__ == "__main__":
    unittest.main()
