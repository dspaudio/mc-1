#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 dspaudio
# /// script
# requires-python = ">=3.11"
# dependencies = ["lameenc==1.8.1"]
# ///
"""Export original shared-C release music; never label it as device audio."""
import hashlib
import json
from pathlib import Path
import subprocess
import wave

import lameenc


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    output = root / "build/release120-media"
    output.mkdir(parents=True, exist_ok=True)
    subprocess.run(["make", "build/render-release-samples", "build/render-demos"],
                   cwd=root, check=True)
    subprocess.run([str(root / "build/render-release-samples"), str(output)], check=True)
    for demo, name in enumerate(("night-circuit-fx", "neon-spiral-fx")):
        subprocess.run([str(root / "build/render-demos"), str(demo),
                        str(output / f"{name}.wav")], check=True)
    records = []
    for name in ("hip-hop-piano", "pop-guitar", "chamber-strings",
                 "night-circuit-fx", "neon-spiral-fx"):
        source = output / f"{name}.wav"
        with wave.open(str(source)) as stream:
            assert (stream.getnchannels(), stream.getsampwidth(), stream.getframerate()) == (2, 2, 44100)
            frames = stream.getnframes()
            pcm = stream.readframes(frames)
        encoder = lameenc.Encoder()
        encoder.set_bit_rate(320)
        encoder.set_in_sample_rate(44100)
        encoder.set_channels(2)
        encoder.set_quality(2)
        encoded = encoder.encode(pcm) + encoder.flush()
        destination = root / "docs/audio" / f"mc-1-{name}.mp3"
        destination.write_bytes(encoded)
        records.append({
            "file": destination.name, "frames": frames, "rate": 44100,
            "bytes": len(encoded), "sha256": hashlib.sha256(encoded).hexdigest(),
            "wav_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        })
    report = {
        "version": (root / "VERSION").read_text().strip(),
        "source": "Original compositions, actual shared C engine; host renders, not FM-1 recordings.",
        "encoder": "lameenc 1.8.1 / LAME, 320 kbps stereo, no normalization or post effects",
        "files": records,
    }
    (root / "docs/release-audio.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
