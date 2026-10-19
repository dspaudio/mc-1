#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 dspaudio
"""Compare reproducible build reports, retaining source-input and binary hashes."""
import difflib
import json
from pathlib import Path


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    for name in ("target-report.json", "package-report.json"):
        expected = json.loads((root / "docs" / name).read_text())
        actual = json.loads((root / "build" / name).read_text())
        # Commit/dirty describe the invocation, not the reproducible build input.
        # source_input_sha256, configuration, layout and all binary hashes remain.
        for report in (expected, actual):
            report.pop("source_commit", None)
            report.pop("source_dirty", None)
        if expected != actual:
            print("".join(difflib.unified_diff(
                json.dumps(expected, indent=2, sort_keys=True).splitlines(True),
                json.dumps(actual, indent=2, sort_keys=True).splitlines(True),
                fromfile=f"docs/{name}", tofile=f"build/{name}",
            )))
            raise SystemExit(1)
    print("REPRODUCIBLE_TARGET_REPORTS_PASS")


if __name__ == "__main__":
    main()
