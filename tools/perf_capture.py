#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""MC_PERF_PROBE 진단 호스트 수집기 (표준 라이브러리 중심).

- 기본 동작과 offline 하위 명령(decode/report/compare)은 MIDI 장치를 절대 열지 않는다.
- 하드웨어 접근은 명시적 `capture` 하위 명령에서만 일어나며, 그때만 mido를 지연 import한다.
- 재시도/폴링 없음. 완료는 7개 조각을 실제 수신한 이벤트로만 판정한다.
- 이 도구의 숫자는 raw tick과 호스트 monotonic 시간이다. PCM 동등성, 물리 증상은 다루지 않는다.
"""
import argparse
import json
import sys
import threading
import time
from fractions import Fraction

CMD_START = 0x70
CMD_DONE = 0x71
SCHEMA = 1
BLOCKS = 8192
BLOCK_FRAMES = 128
PIECES = 7
WORDS = 42
BODY_LEN = 32
RATIO_LIMIT = Fraction(70, 100)
MIN_REPEATS = 3

# 계약의 고정 결과 42 words 이름
WORD_NAMES = (
    "schema session blocks frames elapsed_ticks render_sum_lo render_sum_hi "
    "render_max_ticks audio_sum_lo audio_sum_hi audio_max_ticks interval_max_ticks "
    "deadline_misses_delta half_changes_delta timer5_sum_lo timer5_sum_hi timer5_count "
    "strip_count strip_gap_max_ticks led_count led_gap_max_ticks uac_underruns_delta "
    "uac_overruns_delta uac_stalls_delta timer4_con timer4_prd clk_con1 clk_con2 alnk_len "
    "bpm_start bpm_end playing_start playing_end clock_source_start clock_source_end "
    "master_start master_end step_start step_end uac_flags_start uac_flags_end "
    "uac_missed_delta"
).split()
assert len(WORD_NAMES) == WORDS


class FrameError(Exception):
    """손상/짧음/길이 불일치 프레임. kind: short|framing|length|checksum|encoding"""

    def __init__(self, kind, msg=""):
        super().__init__("%s: %s" % (kind, msg))
        self.kind = kind


def pack7(data):
    out, acc, nb = [], 0, 0
    for b in data:
        acc |= b << nb
        nb += 8
        while nb >= 7:
            out.append(acc & 0x7F)
            acc >>= 7
            nb -= 7
    if nb:
        out.append(acc & 0x7F)
    return bytes(out)


def unpack7(s):
    out, acc, nb = [], 0, 0
    for b in s:
        acc |= (b & 0x7F) << nb
        nb += 7
        while nb >= 8:
            out.append(acc & 0xFF)
            acc >>= 8
            nb -= 8
    return bytes(out)


def _frame(cmd, body):
    u = bytearray([0x00, 0x59, cmd, len(body) & 0xFF, (len(body) >> 8) & 0xFF, 0])
    u += body
    u.append(~sum(body) & 0xFF)
    return b"\xF0" + pack7(bytes(u)) + b"\xF7"


def build_request(session):
    if not 0 < session <= 0xFFFFFFFF:
        raise ValueError("session은 0이 아닌 u32여야 한다")
    return _frame(CMD_START, session.to_bytes(4, "little") + bytes([SCHEMA, 0, 0, 0]))


def build_response(session, index, total, schema, status, data):
    """테스트/모의 장치용 0x71 완료 응답 생성."""
    data = list(data) + [0] * (6 - len(data))
    body = session.to_bytes(4, "little") + bytes([index, total, schema, status])
    for w in data:
        body += (w & 0xFFFFFFFF).to_bytes(4, "little")
    return _frame(CMD_DONE, body)


def parse_response(raw):
    """-> dict(session,index,total,schema,status,data) 또는 None(타 제조사/타 명령=foreign).
    손상은 FrameError."""
    raw = bytes(raw)
    if len(raw) < 2:
        raise FrameError("short", "프레임 길이 %d" % len(raw))
    if raw[0] != 0xF0 or raw[-1] != 0xF7:
        raise FrameError("framing", "F0..F7 경계 필수")
    raw = raw[1:-1]
    if any(b & 0x80 for b in raw):
        raise FrameError("encoding", "7비트 아닌 바이트")
    u = unpack7(raw)
    if len(u) < 3:
        raise FrameError("short", "헤더 부족")
    if u[0] != 0x00 or u[1] != 0x59 or u[2] != CMD_DONE:
        return None
    if len(u) < 6 + BODY_LEN + 1:
        raise FrameError("short", "길이 %d" % len(u))
    ln = u[3] | (u[4] << 8) | (u[5] << 16)
    if ln != BODY_LEN or len(u) != 6 + ln + 1:
        raise FrameError("length", "len24=%d 실제=%d" % (ln, len(u)))
    body = u[6:6 + ln]
    if (~sum(body) & 0xFF) != u[-1]:
        raise FrameError("checksum")
    return {
        "session": int.from_bytes(body[0:4], "little"),
        "index": body[4], "total": body[5], "schema": body[6], "status": body[7],
        "data": [int.from_bytes(body[8 + 4 * i:12 + 4 * i], "little") for i in range(6)],
    }


class Collector:
    """수신 콜백이 feed()를 호출한다. 완료/치명 오류 시 done 이벤트를 set 한다."""

    def __init__(self, session):
        self.session = session
        self.done = threading.Event()
        self._lock = threading.Lock()
        self._pieces = {}
        self.status = "pending"   # pending|ok|busy|invalid|bad_frame|bad_schema|bad_status|
        #                           duplicate|bad_words|timeout|cancelled
        self.detail = ""
        self.foreign = 0
        self.other_session = 0

    def _finish(self, status, detail=""):
        self.status = status
        self.detail = detail
        self.done.set()

    def feed(self, raw):
        with self._lock:
            if self.done.is_set():
                return
            try:
                r = parse_response(raw)
            except FrameError as e:
                self._finish("bad_frame", str(e))
                return
            if r is None:
                self.foreign += 1
                return
            if r["session"] != self.session:
                self.other_session += 1   # 다른 세션 완료는 현재 요청의 완료가 아니다
                return
            if r["schema"] != SCHEMA:
                self._finish("bad_schema", "schema=%d" % r["schema"])
                return
            if r["status"] in (1, 2):
                if r["index"] or r["total"] or any(r["data"]):
                    self._finish("bad_frame", "오류 응답 index/total/data 비정상")
                else:
                    self._finish("busy" if r["status"] == 1 else "invalid")
                return
            if r["status"] != 0:
                self._finish("bad_status", "status=%d" % r["status"])
                return
            if r["total"] != PIECES or r["index"] >= PIECES:
                self._finish("bad_frame", "index=%d total=%d" % (r["index"], r["total"]))
                return
            if r["index"] in self._pieces:
                self._finish("duplicate", "index=%d" % r["index"])
                return
            self._pieces[r["index"]] = r["data"]
            if len(self._pieces) == PIECES:
                self._complete()

    def _complete(self):
        words = [w for i in range(PIECES) for w in self._pieces[i]]
        bad = validate_words(words, self.session)
        self._finish("bad_words" if bad else "ok", "; ".join(bad))

    def cancel(self, status="timeout"):
        with self._lock:
            if not self.done.is_set():
                self._finish(status, "수신 %d/%d" % (len(self._pieces), PIECES))

    def missing(self):
        with self._lock:
            return [i for i in range(PIECES) if i not in self._pieces]

    def words(self):
        with self._lock:
            if len(self._pieces) != PIECES:
                return None
            return [w for i in range(PIECES) for w in self._pieces[i]]


def validate_words(words, session):
    bad = []
    if len(words) != WORDS:
        return ["words=%d" % len(words)]
    if words[0] != SCHEMA:
        bad.append("schema=%d" % words[0])
    if words[1] != session:
        bad.append("session 불일치")
    if words[2] != BLOCKS:
        bad.append("blocks=%d" % words[2])
    if words[3] != BLOCKS * BLOCK_FRAMES:
        bad.append("frames=%d" % words[3])
    for idx in (39, 40):  # uac_flags_start/end: alt|go|feed(bit0..2)만 허용
        if words[idx] & ~7:
            bad.append("uac_flags[%d] 예약 비트" % idx)
    return bad


def capture(transport, session, timeout, workload, settings):
    """콜백 등록 -> 요청 전송 -> 제한시간 이벤트 대기. 재시도/폴링 없음."""
    col = Collector(session)
    start_ns = time.monotonic_ns()
    try:
        transport.subscribe(col.feed)      # 전송 전에 구독 (즉시 콜백 대비)
        transport.send(build_request(session))
        col.done.wait(timeout)
    finally:
        col.cancel("timeout")              # 이미 끝났으면 no-op, 늦은 콜백은 무시됨
        end_ns = time.monotonic_ns()
        transport.close()
    return make_record(col, workload, settings, start_ns, end_ns)


def make_record(col, workload, settings, start_ns, end_ns):
    return {
        "format": "perf_capture/1", "workload": workload, "settings": settings,
        "session": col.session, "status": col.status, "detail": col.detail,
        "missing": col.missing(), "words": col.words(),
        "foreign": col.foreign, "other_session": col.other_session,
        "host_start_ns": start_ns, "host_end_ns": end_ns,
        "host_elapsed_ns": end_ns - start_ns,
    }


class MidoTransport:
    """실제 포트. capture 명령에서만 생성된다 (mido 지연 import)."""

    def __init__(self, port):
        import mido  # noqa: PLC0415 - 지연 import
        self._mido = mido
        self._port = port
        self._in = None
        self._out = None

    def subscribe(self, cb):
        def on_msg(msg):
            if msg.type == "sysex":
                cb(bytes([0xF0]) + bytes(msg.data) + bytes([0xF7]))
        self._in = self._mido.open_input(self._port, callback=on_msg)

    def send(self, pkt):
        self._out = self._mido.open_output(self._port)
        self._out.send(self._mido.Message("sysex", data=list(pkt[1:-1])))

    def close(self):
        for p in (self._in, self._out):
            if p is not None:
                p.close()


def to_u64(lo, hi):
    return lo | (hi << 32)


def timer4_us_ok(words):
    return (words[24] & 0xFF) == 9 and words[25] == 0xFFFFFFFF


def format_report(rec, tick_hz=None):
    L = ["workload=%s settings=%s session=%s status=%s" %
         (rec["workload"], rec["settings"], rec["session"], rec["status"])]
    if rec.get("detail"):
        L.append("detail: " + rec["detail"])
    L.append("host_elapsed_ns=%d (호스트 monotonic: 워밍업+결과 전달 포함, TIMER4 캡처 구간 아님)"
             % rec["host_elapsed_ns"])
    w = rec.get("words")
    if rec["status"] != "ok" or not w:
        L.append("미완료/무효: missing=%s. 타이밍 결론 없음" % rec.get("missing"))
        return "\n".join(L)
    d = dict(zip(WORD_NAMES, w))
    d["render_sum"] = to_u64(w[5], w[6])
    d["audio_sum"] = to_u64(w[8], w[9])
    d["timer5_sum"] = to_u64(w[14], w[15])
    for k in ("blocks", "frames", "elapsed_ticks", "render_sum", "render_max_ticks",
              "audio_sum", "audio_max_ticks", "interval_max_ticks", "deadline_misses_delta",
              "timer5_sum", "timer5_count", "uac_underruns_delta", "uac_overruns_delta",
              "uac_stalls_delta", "uac_missed_delta"):
        L.append("%s=%d (raw ticks/count)" % (k, d[k]))
    if timer4_us_ok(w) and tick_hz:
        L.append("elapsed_us=%.3f (TIMER4 조건 충족, tick_hz=%s)" %
                 (d["elapsed_ticks"] * 1e6 / tick_hz, tick_hz))
    else:
        L.append("us 환산 안 함 (TIMER4 CON&0xFF==9, PRD==0xffffffff, --tick-hz 모두 필요)")
    L.append("TIMER4/호스트 대조는 LRCLK 측정이 아니다. PCM 증거와 물리 증상은 자동 판정 밖.")
    return "\n".join(L)


U32 = 0xFFFFFFFF
STABLE_PAIRS = ((29, 30), (31, 32), (33, 34), (35, 36), (39, 40))


def _all_u32(words):
    return (isinstance(words, list) and len(words) == WORDS
            and all(type(w) is int and 0 <= w <= U32 for w in words))


def timing_problems(w):
    """비교에 쓸 수 있는 타이밍 기준인지 검사한다. 위반 사유 목록(빈 목록=사용 가능)."""
    bad = []
    if (w[24] & 0xFF) != 9 or w[25] != U32:
        bad.append("TIMER4 CON&0xFF!=9 또는 PRD!=0xffffffff")
    rs, au, t5 = to_u64(w[5], w[6]), to_u64(w[8], w[9]), to_u64(w[14], w[15])
    for name, v in (("elapsed_ticks", w[4]), ("render_sum", rs), ("render_max", w[7]),
                    ("audio_sum", au), ("audio_max", w[10]), ("interval_max", w[11])):
        if v == 0:
            bad.append(name + "=0")
    for name, mx, total in (("render", w[7], rs), ("audio", w[10], au)):
        if mx > total or mx * w[2] < total:
            bad.append("%s max/sum 관계 위반" % name)
        if total > w[4]:
            bad.append("%s_sum > elapsed_ticks" % name)
    if w[11] > w[4]:
        bad.append("interval_max > elapsed_ticks")
    if (t5 == 0) != (w[16] == 0):
        bad.append("timer5 sum/count 관계 위반")
    for a, b in STABLE_PAIRS:
        if w[a] != w[b]:
            bad.append("%s != %s (캡처 중 변동)" % (WORD_NAMES[a], WORD_NAMES[b]))
    return bad


def _record_problems(rec):
    if (rec.get("format") != "perf_capture/1" or rec.get("status") != "ok"
            or rec.get("words") is None):
        return ["무효/불완전(status=%s)" % rec.get("status")]
    w = rec["words"]
    if not _all_u32(w):
        return ["words가 42개 unsigned u32가 아님"]
    return validate_words(w, rec.get("session")) + timing_problems(w)


def _mean_render(recs):
    per_block = [Fraction(to_u64(r["words"][5], r["words"][6]), r["words"][2]) for r in recs]
    return sum(per_block) / len(per_block)


def compare(base, cand):
    """-> (verdict, reasons, ratio). verdict: PASS|FAIL|INVALID.
    PASS는 평균 render 비율 기준만 뜻한다. PCM/상태 동등성, 기한·증상 개선은 별도 검증.
    비교 조건(words 24..40: 타이머/클록, BPM, playing, 클럭원, master, step, UAC 플래그)은
    모든 기록에서 같아야 하고, 캡처 중 변동이 있으면 INVALID다. 비율에서 LRCLK 등을 추정하지 않는다."""
    bad = []
    for name, side in (("baseline", base), ("candidate", cand)):
        if len(side) < MIN_REPEATS:
            bad.append("%s 반복 %d < %d" % (name, len(side), MIN_REPEATS))
        for i, r in enumerate(side):
            for p in _record_problems(r):
                bad.append("%s[%d] %s" % (name, i, p))
        if len({r.get("session") for r in side}) != len(side):
            bad.append("%s 세션 중복" % name)
    if len(base) != len(cand):
        bad.append("반복 횟수 불일치 %d != %d" % (len(base), len(cand)))
    allr = list(base) + list(cand)
    if allr:
        for key in ("workload", "settings"):
            if len({json.dumps(r.get(key), sort_keys=True) for r in allr}) != 1:
                bad.append("%s 불일치" % key)
    if bad:
        return "INVALID", bad, None
    if len({tuple(r["words"][24:41]) for r in allr}) != 1:
        return "INVALID", ["비교 조건 words 24..40(타이머/BPM/playing/클럭원/master/step/UAC) 불일치"], None
    bm = _mean_render(base)
    if bm == 0:
        return "INVALID", ["기준 평균 render 0"], None
    ratio = _mean_render(cand) / bm
    if ratio <= RATIO_LIMIT:
        return "PASS", ["평균 render 비율 %.4f <= 0.70 (타이밍 기준만; PCM/증상/LRCLK 추정 아님)" % float(ratio)], ratio
    return "FAIL", ["평균 render 비율 %.4f > 0.70" % float(ratio)], ratio


def decode_frames_text(text, session, workload, settings):
    """오프라인: 줄마다 hex SysEx 프레임을 Collector에 투입 (장치 접근 없음)."""
    col = Collector(session)
    t = time.monotonic_ns()
    for line in text.splitlines():
        line = line.split("#")[0].strip()
        if line:
            col.feed(bytes.fromhex(line.replace(" ", "")))
    col.cancel("cancelled")
    return make_record(col, workload, settings, t, t)


def _load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def _dump(rec, path):
    text = json.dumps(rec, indent=1, sort_keys=True)
    if path:
        with open(path, "w", encoding="utf-8") as f:
            f.write(text + "\n")
    else:
        print(text)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("capture", help="명시적 실기기 캡처 (MIDI 포트를 연다)")
    c.add_argument("--port", required=True)
    c.add_argument("--session", required=True, type=lambda s: int(s, 0))
    c.add_argument("--timeout", required=True, type=float)
    c.add_argument("--output", required=True)
    c.add_argument("--workload", required=True)
    c.add_argument("--settings", required=True)
    d = sub.add_parser("decode", help="오프라인: hex 프레임 파일 -> 레코드")
    d.add_argument("frames")
    d.add_argument("--session", required=True, type=lambda s: int(s, 0))
    d.add_argument("--workload", required=True)
    d.add_argument("--settings", required=True)
    d.add_argument("--output")
    r = sub.add_parser("report", help="오프라인: 레코드 JSON 보고")
    r.add_argument("record")
    r.add_argument("--tick-hz", type=float)
    k = sub.add_parser("compare", help="오프라인: 기준/후보 평균 render 비교")
    k.add_argument("--baseline", nargs="+", required=True)
    k.add_argument("--candidate", nargs="+", required=True)
    a = ap.parse_args(argv)

    if a.cmd == "capture":
        if not 0 < a.session <= U32:
            ap.error("--session은 0이 아니라 u32여야 한다")
        if not 0 < a.timeout <= threading.TIMEOUT_MAX:
            ap.error("--timeout은 유한한 양수여야 한다")
        rec = capture(MidoTransport(a.port), a.session, a.timeout, a.workload, a.settings)
        _dump(rec, a.output)
        print(format_report(rec))
        return 0 if rec["status"] == "ok" else 1
    if a.cmd == "decode":
        with open(a.frames, encoding="utf-8") as f:
            rec = decode_frames_text(f.read(), a.session, a.workload, a.settings)
        _dump(rec, a.output)
        print(format_report(rec), file=sys.stderr)
        return 0 if rec["status"] == "ok" else 1
    if a.cmd == "report":
        rec = _load(a.record)
        print(format_report(rec, a.tick_hz))
        return 0 if rec["status"] == "ok" else 1
    verdict, reasons, _ = compare([_load(p) for p in a.baseline], [_load(p) for p in a.candidate])
    print(verdict)
    for s in reasons:
        print(" - " + s)
    return 0 if verdict == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
