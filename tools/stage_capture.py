#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Read the frozen stage profile AFTER perf_capture.py; never start or flash firmware."""
import argparse
import hashlib
import json
import threading
from pathlib import Path
from typing import Callable, Protocol
import perf_capture as perf

STAGES = ("synthesis", "routing", "rd", "cf", "wait", "transfer", "output",
          "rd2", "cf2", "other_audio", "audio_before_profile_aggregation")


class Transport(Protocol):
    def subscribe(self, callback: Callable[[bytes], None]) -> None: ...
    def send(self, packet: bytes) -> None: ...
    def close(self) -> None: ...


class StageError(Exception):
    """A missing, stale or invalid profile is not a performance result."""


def capture_pages(factory: Callable[[], Transport], session: int, timeout: float) -> list[int]:
    if not 0 < session <= 0xffffffff or not 0 < timeout <= threading.TIMEOUT_MAX:
        raise StageError("Invalid session or timeout")
    words: list[int] = []
    for index in range(7):
        done = threading.Event()
        lock = threading.Lock()
        page: list[int] = []
        errors: list[str] = []

        def receive(raw: bytes, *, expected: int = index,
                    result: list[int] = page, failures: list[str] = errors,
                    signal: threading.Event = done, guard=lock) -> None:
            with guard:
                if signal.is_set():
                    return
                try:
                    response = perf.parse_response(raw)
                except perf.FrameError as error:
                    failures.append(str(error))
                    signal.set()
                    return
                if response is None or response["session"] != session:
                    return
                if (response["schema"] != 5 or response["status"] or
                        response["total"] != 7 or response["index"] != expected):
                    failures.append("Rejected or mismatched stage page")
                else:
                    result.extend(response["data"])
                signal.set()

        transport = factory()
        try:
            transport.subscribe(receive)
            transport.send(perf._frame(perf.CMD_START,
                session.to_bytes(4, "little") + bytes([5, index, 0, 0])))
            if not done.wait(timeout):
                raise StageError("Stage page timeout")
        finally:
            with lock:
                done.set()
            transport.close()
        if errors:
            raise StageError(errors[0])
        words.extend(page)
    if len(words) != 42 or words[:3] != [5, session, 8192]:
        raise StageError("Stage profile does not match the completed 8192-block capture")
    return words


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--capture", required=True, help="Completed perf_capture JSON")
    parser.add_argument("--timeout", required=True, type=float)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--firmware-sha256", required=True)
    parser.add_argument("--build-settings", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    raw = Path(args.capture).read_bytes()
    baseline = json.loads(raw)
    if baseline["status"] != "ok":
        raise StageError("The base capture is incomplete")
    words = capture_pages(lambda: perf.MidoTransport(args.port), baseline["session"], args.timeout)
    record = {
        "format": "mc1-stages/2", "source_commit": args.source_commit,
        "firmware_sha256": args.firmware_sha256, "build_settings": args.build_settings,
        "capture_sha256": hashlib.sha256(raw).hexdigest(),
        "workload": baseline["workload"], "conditions": baseline["settings"],
        "session": words[1], "blocks": words[2], "timer_read_pair_ticks": words[3],
        "stages": {name: {"sum_ticks": words[4+3*i] | words[5+3*i] << 32,
                          "max_ticks": words[6+3*i]} for i, name in enumerate(STAGES)},
        "cpu1_status": words[37], "cpu1_jobs_since_boot": words[38],
        "cpu1_failures_since_boot": words[39], "cpu1_user_stack_free_bytes": words[40],
        "cpu1_stack_guards_ok": words[41], "words": words,
    }
    Path(args.output).write_text(json.dumps(record, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
