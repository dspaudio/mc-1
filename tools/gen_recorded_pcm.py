#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
# ─── How to run ───
# python3 tools/gen_recorded_pcm.py [output-header] [metrics-json]
"""Convert retained CC0 recordings to mono PCM and seven anti-aliased mip levels."""
from __future__ import annotations

import hashlib
import json
import math
from dataclasses import dataclass
from pathlib import Path
import struct
import sys
from typing import Final
import wave

ROOT: Final = Path(__file__).resolve().parents[1]
RATE: Final = 16000
LEVELS: Final = 7
PEAK: Final = 16000


@dataclass(frozen=True, slots=True)
class Sample:
    id: int
    name: str
    file: str
    sha256: str
    root_midi: int
    frames: int
    loop: bool
    loop_start: int
    loop_crossfade: int


@dataclass(frozen=True, slots=True)
class Signal:
    data: list[float]
    rate: int
    loop_start: int = 0


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_source(sample: Sample) -> Signal:
    """Decode original integer PCM, averaging channels without changing pitch."""
    path = ROOT / 'assets/recorded' / sample.file
    assert digest(path) == sample.sha256, f'Source hash mismatch: {path}'
    with wave.open(str(path), 'rb') as source:
        channels, width, rate, count, compression, _ = source.getparams()
        assert compression == 'NONE' and width in (2, 3), path
        raw = source.readframes(count)
    values = [int.from_bytes(raw[i:i + width], 'little', signed=True)
              / (1 << (8 * width - 16)) for i in range(0, len(raw), width)]
    return Signal([sum(values[i:i + channels]) / channels
                   for i in range(0, len(values), channels)], rate)


def resample(signal: Signal, output_frames: int) -> list[float]:
    """128-tap Blackman-windowed sinc, cutoff 90% of output Nyquist."""
    ratio = signal.rate / RATE
    cutoff = 0.45 / ratio
    radius = 64
    phases: dict[int, list[float]] = {}
    result = []
    for i in range(output_frames):
        numerator = i * signal.rate
        center, phase = divmod(numerator, RATE)
        if phase not in phases:
            weights = []
            for tap in range(-radius + 1, radius + 1):
                distance = tap - phase / RATE
                sinc = (2 * cutoff if abs(distance) < 1e-12 else
                        math.sin(2 * math.pi * cutoff * distance) / (math.pi * distance))
                window = (0.42 + 0.5 * math.cos(math.pi * distance / radius)
                          + 0.08 * math.cos(2 * math.pi * distance / radius))
                weights.append(sinc * window)
            total = sum(weights)
            phases[phase] = [value / total for value in weights]
        result.append(sum(signal.data[min(max(center + tap, 0), len(signal.data) - 1)] * weight
                          for tap, weight in zip(range(-radius + 1, radius + 1), phases[phase])))
    return result


def decimate(signal: Signal) -> Signal:
    """63-tap low-pass /2; sustained tail convolution wraps within the loop."""
    taps = []
    for offset in range(-31, 32):
        sinc = 0.45 if offset == 0 else math.sin(math.pi * 0.45 * offset) / (math.pi * offset)
        taps.append(sinc * (0.42 + 0.5 * math.cos(math.pi * offset / 32)
                            + 0.08 * math.cos(2 * math.pi * offset / 32)))
    total = sum(taps)
    weights = [value / total for value in taps]
    size = len(signal.data)
    result = []
    for i in range(0, size, 2):
        value = 0.0
        for offset, weight in zip(range(-31, 32), weights):
            index = i + offset
            if index >= size and signal.loop_start:
                index = signal.loop_start + (index - size) % (size - signal.loop_start)
            index = min(max(index, 0), size - 1)
            value += signal.data[index] * weight
        result.append(value)
    return Signal(result, signal.rate // 2, signal.loop_start // 2)


def prepare(sample: Sample, source: Signal) -> tuple[Signal, int]:
    """Trim leading silence; retain natural attack, then crossfade or fade out."""
    source_peak = max(abs(value) for value in source.data)
    onset = next(i for i, value in enumerate(source.data) if abs(value) >= source_peak * 0.005)
    offset = max(0, onset - round(source.rate * 0.002))
    needed = math.ceil(sample.frames * source.rate / RATE) + 128
    data = resample(Signal(source.data[offset:offset + needed], source.rate), sample.frames)
    dc = sum(data) / len(data)
    data = [value - dc for value in data]
    for i in range(32):
        data[i] *= i / 31
    if sample.loop:
        width = sample.loop_crossfade
        for i in range(width):
            weight = (1 - math.cos(math.pi * i / (width - 1))) * 0.5
            tail = len(data) - width + i
            data[tail] = data[tail] * (1 - weight) + data[sample.loop_start - width + i] * weight
    else:
        for i in range(1024):
            data[-1024 + i] *= (1 + math.cos(math.pi * i / 1023)) * 0.5
    return Signal(data, RATE, sample.loop_start), offset


def main() -> None:
    """Verify retained inputs and emit a deterministic C header plus measurements."""
    manifest_path = ROOT / 'assets/recorded/manifest.json'
    manifest = json.loads(manifest_path.read_text())
    assert digest(ROOT / manifest['license_path']) == manifest['license_sha256']
    samples = [Sample(**{key: entry[key] for key in Sample.__dataclass_fields__})
               for entry in manifest['samples']]
    assert len(samples) == 16 and [sample.id for sample in samples] == list(range(16))
    header = ['/* Generated by tools/gen_recorded_pcm.py (GPL-3.0-only); PCM data: CC0-1.0. */',
              '/* Recording provenance: assets/recorded/manifest.json; license: LICENSES/CC0-VSCO.txt. */',
              '#ifndef MC_RECORDED_PCM_H', '#define MC_RECORDED_PCM_H', '#include <stdint.h>',
              '#ifndef MC_RECORDED_SAMPLES', '#define MC_RECORDED_SAMPLES 16', '#endif',
              'typedef struct { const int16_t *data; uint16_t length, loop_start, loop_end; } McRecordedLevel;',
              'typedef struct { McRecordedLevel levels[7]; uint32_t step[109]; } McRecordedSample;']
    descriptors = []
    measurements = []
    total_frames = 0
    for sample in samples:
        source = read_source(sample)
        base, offset = prepare(sample, source)
        levels = [base]
        for _ in range(LEVELS - 1):
            levels.append(decimate(levels[-1]))
        gain = PEAK / max(abs(value) for level in levels for value in level.data)
        level_descriptors = []
        level_metrics = []
        for index, level in enumerate(levels):
            pcm = [round(value * gain) for value in level.data]
            size = len(pcm)
            start = level.loop_start
            end = size if start else 0
            assert 2 <= size < 32768 and max(abs(value) for value in pcm) <= PEAK
            assert (start == end == 0) or 0 < start < end <= size
            name = f'mc_recorded_{sample.id}_{index}'
            header.append(f'static const int16_t {name}[{size}] = {{')
            header.extend('    ' + ','.join(str(value) for value in pcm[i:i + 16]) + ','
                          for i in range(0, size, 16))
            header.append('};')
            level_descriptors.append(f'{{{name},{size},{start},{end}}}')
            level_metrics.append({'frames': size, 'peak': max(abs(value) for value in pcm),
                                  'dc': sum(pcm) / size, 'loop_start': start, 'loop_end': end,
                                  'loop_jump': abs(pcm[end - 1] - pcm[start]) if start else 0,
                                  'max_adjacent_jump': max(abs(a - b) for a, b in zip(pcm, pcm[1:])),
                                  'pcm_sha256': hashlib.sha256(struct.pack(f'<{size}h', *pcm)).hexdigest()})
            total_frames += size
        steps = [round(65536 * RATE / 44100 * 2 ** ((note - sample.root_midi) / 12)) for note in range(109)]
        assert max(steps[12:]) >> 6 <= 65536
        descriptors.append('    {{' + ','.join(level_descriptors) + '},{' + ','.join(map(str, steps)) + '}},')
        measurements.append({'id': sample.id, 'name': sample.name, 'root_midi': sample.root_midi,
                             'source_sha256': sample.sha256, 'source_sample_rate': source.rate,
                             'source_peak': max(abs(value) for value in source.data),
                             'source_dc': sum(source.data) / len(source.data),
                             'source_start_frame': offset, 'source_start_seconds': offset / source.rate,
                             'source_excerpt_seconds': sample.frames / RATE, 'levels': level_metrics})
    header.extend(['static const McRecordedSample mc_recorded[MC_RECORDED_SAMPLES] = {',
                   *descriptors, '};', '#endif', ''])
    output = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'src/recorded_pcm.h'
    output.write_text('\n'.join(header))
    metrics = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / 'build/recorded-pcm-report.json'
    metrics.parent.mkdir(parents=True, exist_ok=True)
    report = {'generator_sha256': digest(Path(__file__)), 'header_sha256': digest(output),
              'pcm_bytes': total_frames * 2, 'table_bytes_32bit': 16 * (7 * 12 + 109 * 4),
              'samples': measurements}
    assert report['pcm_bytes'] + report['table_bytes_32bit'] <= 400 * 1024
    metrics.write_text(json.dumps(report, indent=2) + '\n')
    print(f'{output}: {total_frames * 2} PCM bytes; {report["table_bytes_32bit"]} table bytes; {metrics}')


if __name__ == '__main__':
    main()
