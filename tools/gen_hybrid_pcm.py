#!/usr/bin/env -S uv run --script
# SPDX-License-Identifier: GPL-3.0-only
# /// script
# requires-python = ">=3.12"
# dependencies = ["numpy==2.4.2"]
# ///
# ─── How to run ───
# Install uv: curl -LsSf https://astral.sh/uv/install.sh | sh
# Run: uv run tools/gen_hybrid_pcm.py [output-header] [metrics-json]
# Retained WAVs make subsequent generation entirely offline.
"""Extract real attacks and phase-locked, non-vibrato harmonic sustain frames."""
from __future__ import annotations

import hashlib
import json
import math
import sys
import wave
import zipfile
from dataclasses import dataclass
from pathlib import Path
from typing import Final, TypedDict

import numpy as np
from numpy.typing import NDArray

ROOT: Final = Path(__file__).resolve().parents[1]
RATE: Final = 16000
OUTPUT_RATE: Final = 44100
LENGTHS: Final = (128, 64, 32, 16, 8, 4, 4)
META_COMMIT: Final = "e2ff154845994fe4135becc4d5b857c19f172784"
META_REPO: Final = "https://github.com/facebookincubator/Meta-open-sound"
PIANO_HASHES: Final = (
    "373d6214e746dba0e35029ce15816135022aa5a7b4126798dc2abe262f253054",
    "d5c09f36cef0b4a108ff9078af3dc2b203224e14ff55b358e441446da0d267e0",
    "b37311c07f3fbf6f0b02b6b3e01dab5d44176ad927273c6972893c43fd2150c9",
    "446e397d1690e623faa06c07206ddabc0273f6edd95df795aa89fea292111360",
    "6838079a03875c4b180ce3f98187373ab604c55d65f320c3c090a7f984b04a48",
    "59c32c9c3f577f714568c948c033b6fad87c6b338ea521f100f4e4cb6d759cf5",
)
GUITARS: Final = (
    ("INSGuitarNylon_StringLowE_Vel01Finger_C3_01.wav",
     "f63cf2bf0065f2d02e13a6a30be421ee882e8f7dd8b37ebdbe0b072814d6fb15"),
    ("INSGuitarNylon_StringD_Vel01Finger_C4_01.wav",
     "22a99b38476d83d4bf8f60c13613c59ac3fda82a164608bc2c6bacf2a209c8bc"),
    ("INSGuitarNylon_StringHighE_Vel01Finger_C5_01.wav",
     "184337d5dd1efed6e6fcd25bd4f5b5923c855a8606b063db9f21f94c509b7f21"),
)
FloatArray = NDArray[np.float64]
ComplexArray = NDArray[np.complex128]
PCMArray = NDArray[np.int16]


class RecordedEntry(TypedDict):
    """Fields consumed from the existing provenance manifest."""

    id: int
    root_midi: int
    name: str
    file: str
    sha256: str
    loop: bool


class RecordedManifest(TypedDict):
    """Existing source metadata, without importing its mutable generator."""

    samples: list[RecordedEntry]
    source_repository: str
    source_commit: str


class ZoneReport(TypedDict):
    """Machine-readable compatibility mapping and extraction measurements."""

    index: int
    family: int
    root: int
    name: str
    source: str
    source_sha256: str
    source_rate: int
    channel: int
    onset_source_frame: int
    attack_frames: int
    attack_lengths: list[int]
    pcm_bytes: int
    gain: float
    fitted_reference_hz: float
    analysis_windows_seconds: tuple[tuple[float, float], tuple[float, float]]
    local_pitch_ranges_hz: list[list[float]]
    common_harmonic_phases: list[float]
    early_magnitudes: list[float]
    late_magnitudes: list[float]
    phase_start: int
    harmonic_limits: list[int]
    phase_errors_radians: list[float]
    nyquist_quantization_residue: list[float]
    decay: list[int]
    source_repository: str
    source_commit: str
    source_release: str


@dataclass(frozen=True, slots=True)
class Source:
    family: int
    root: int
    name: str
    path: str
    sha256: str
    attack: int
    pluck: bool
    repository: str
    commit: str
    release: str


def digest(path: Path) -> str:
    """Hash exact retained bytes, not decoded audio."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def sources() -> list[Source]:
    """Retain only selected, hash-pinned Meta WAVs; reuse existing VSCO originals."""
    retained = ROOT / "assets/hybrid"
    retained.mkdir(parents=True, exist_ok=True)
    originals = ROOT / "build/pcm-quality/sources"
    result: list[Source] = []
    for octave, sha in zip(range(2, 8), PIANO_HASHES, strict=True):
        filename = f"INSPiano_Vel03_C{octave}_01.wav"
        destination = retained / filename
        if not destination.exists():
            loose = originals / filename
            if loose.exists():
                data = loose.read_bytes()
            else:
                with zipfile.ZipFile(originals / "meta-open-sound-meta-piano-wav.zip") as archive:
                    data = archive.read(filename)
            assert hashlib.sha256(data).hexdigest() == sha, filename
            _ = destination.write_bytes(data)
        result.append(Source(0, 12 * (octave + 1), "Piano", str(destination.relative_to(ROOT)),
                             sha, 4096, True, META_REPO, META_COMMIT, "meta-piano-v1.0.0"))
    recorded: RecordedManifest = json.loads((ROOT / "assets/recorded/manifest.json").read_text())
    for entry in recorded["samples"][1:]:
        result.append(Source(entry["id"], entry["root_midi"], entry["name"],
                             f'assets/recorded/{entry["file"]}', entry["sha256"],
                             3584, not entry["loop"], recorded["source_repository"],
                             recorded["source_commit"], ""))
    for root, (filename, sha) in zip((48, 60, 72), GUITARS, strict=True):
        destination = retained / filename
        if not destination.exists():
            data = (originals / filename).read_bytes()
            assert hashlib.sha256(data).hexdigest() == sha, filename
            _ = destination.write_bytes(data)
        result.append(Source(16, root, "Nylon guitar", str(destination.relative_to(ROOT)),
                             sha, 4096, True, META_REPO, META_COMMIT, "meta-nylon-guitar-v1.0.0"))
    return result


def decode(source: Source) -> tuple[FloatArray, int]:
    """Choose channel zero: never phase-cancel stereo by averaging channels."""
    path = ROOT / source.path
    assert digest(path) == source.sha256, path
    with wave.open(str(path), "rb") as wav:
        channels, width, rate, count, compression, _ = wav.getparams()
        assert compression == "NONE" and width in (2, 3), path
        raw = wav.readframes(count)
    if width == 2:
        data = np.frombuffer(raw, dtype="<i2").astype(np.float64)
    else:
        octets = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        integers = octets[:, 0] | (octets[:, 1] << 8) | (octets[:, 2] << 16)
        data = ((integers ^ 0x800000) - 0x800000).astype(np.float64) / 256
    return data.reshape(-1, channels)[:, 0].copy(), rate


def resample(data: FloatArray, ratio: float, count: int) -> FloatArray:
    """Zero-delay 128-tap Blackman sinc at 90% of destination Nyquist."""
    result = np.empty(count, dtype=np.float64)
    taps = np.arange(-63, 65)
    cutoff = 0.45 / max(1.0, ratio)
    for first in range(0, count, 2048):
        positions = np.arange(first, min(first + 2048, count)) * ratio
        centers = np.floor(positions).astype(np.int64)
        distances = taps[None, :] - (positions - centers)[:, None]
        weights: FloatArray = 2 * cutoff * np.sinc(2 * cutoff * distances)
        weights *= (0.42 + 0.5 * np.cos(np.pi * distances / 64)
                    + 0.08 * np.cos(2 * np.pi * distances / 64))
        weights /= weights.sum(axis=1)[:, None]
        indexes = np.clip(centers[:, None] + taps[None, :], 0, len(data) - 1)
        result[first:first + len(positions)] = (data[indexes] * weights).sum(axis=1)
    return result


def harmonics(data: FloatArray, center: float, nominal: float) -> tuple[float, ComplexArray]:
    """Estimate local f0 and complex partials; local analysis removes pitch vibrato."""
    size = max(1024, math.ceil(8 * RATE / nominal))
    middle = round(center * RATE)
    indexes = np.arange(size) + middle - size // 2
    frame = data[np.clip(indexes, 0, len(data) - 1)]
    window = np.hanning(size)
    frame = (frame - np.mean(frame)) * window
    spectrum = np.abs(np.fft.rfft(frame, 32768))
    candidates = nominal * 2 ** (np.arange(-100, 101) / 1200)
    scores = np.zeros(len(candidates))
    for harmonic in range(1, min(9, int(7200 / nominal))):
        scores += np.interp(candidates * harmonic * 32768 / RATE,
                            np.arange(len(spectrum)), spectrum) ** 2 / harmonic
    frequency = candidates[scores.argmax()].item()
    order = np.arange(1, min(63, int(7200 / frequency)) + 1)
    seconds = (indexes - middle) / RATE
    basis = np.exp(-2j * np.pi * frequency * order[:, None] * seconds[None, :])
    return frequency, np.asarray(2 * (basis @ frame) / window.sum(), dtype=np.complex128)


def envelope(data: FloatArray, attack: int, pluck: bool) -> NDArray[np.uint16]:
    """Slow measured RMS decay, held at unity during attack, never cyclic."""
    if not pluck:
        return np.full(128, 32767, dtype=np.uint16)
    radius = round(0.10 * RATE)
    power = np.concatenate((np.zeros(1), np.cumsum(data * data)))
    times = np.maximum(np.arange(128) * 2048 / OUTPUT_RATE, attack / RATE)
    centers = np.rint(times * RATE).astype(np.int64)
    left = np.clip(centers - radius, 0, len(data))
    right = np.clip(centers + radius, 0, len(data))
    rms = np.sqrt((power[right] - power[left]) / np.maximum(1, right - left))
    bounded: FloatArray = np.minimum.accumulate(np.minimum(1, rms / rms[0]))
    return np.rint(32767 * bounded).astype(np.uint16)


def emit_array(header: list[str], name: str, data: PCMArray) -> None:
    """Emit each raw buffer once, so legacy metadata can alias its symbol."""
    header.append(f"static const int16_t {name}[{len(data)}] = {{")
    header.extend("    " + ",".join(str(data[j].item()) for j in range(i, min(i + 16, len(data)))) + ","
                  for i in range(0, len(data), 16))
    header.append("};")


def generate(source: Source, zone: int, header: list[str]) -> tuple[str, ZoneReport, int]:
    """Derive one common harmonic coordinate and one peak gain for the whole zone."""
    original, rate = decode(source)
    onset = np.flatnonzero(np.abs(original) >= np.abs(original).max() * 0.005)[0].item()
    offset = max(0, onset - round(rate * 0.002))
    data = resample(original[offset:], rate / RATE, math.floor((len(original) - offset) * RATE / rate))
    data -= np.mean(data)
    attack = data[:source.attack].copy()
    attack[:32] *= np.linspace(0, 1, 32)
    attacks = [attack]
    for _ in range(6):
        attacks.append(resample(attacks[-1], 2, len(attacks[-1]) // 2))
    decay = envelope(data, source.attack, source.pluck)
    nominal = 440 * 2 ** ((source.root - 69) / 12)
    reference = (source.attack - 128) / RATE
    frequency, coefficients = harmonics(data, reference, nominal)
    phases = np.angle(coefficients)
    magnitudes: list[FloatArray] = []
    pitch_ranges: list[list[float]] = []
    windows = ((reference - 0.025, reference + 0.025),
               (reference + 0.425, reference + 0.575))
    for start, end in windows:
        local = [harmonics(data, start + (end - start) * i / 6, nominal) for i in range(7)]
        count = min(len(coefficients), *(len(c) for _, c in local))
        amplitudes: FloatArray = np.array([np.abs(c[:count]) for _, c in local]).mean(axis=0)
        magnitudes.append(np.pad(amplitudes, (0, len(coefficients) - count)))
        pitch_ranges.append([min(f for f, _ in local), max(f for f, _ in local)])
    # The slow decay is represented once, by decay[]. Remove that factor from
    # late spectral magnitudes, rather than independently peak-normalizing frames.
    if source.pluck:
        age = (reference + 0.5) * OUTPUT_RATE / 2048
        first = math.floor(age)
        late_decay = (decay[first].item() * (1 - age + first)
                      + decay[first + 1].item() * (age - first)) / 32767
        magnitudes[1] /= max(late_decay, 1 / 32767)
        if source.family not in (0, 16):
            # Natural decay already controls the amplitude. The remaining
            # mallet/harp frames must not add a second swell or fade on top.
            magnitudes[1] *= np.linalg.norm(magnitudes[0]) / np.linalg.norm(magnitudes[1])
    else:
        # Held tone evolution must not become a new accent on every note.
        # Preserve the late body level; match the early spectral energy to it
        # before the shared peak gain is chosen for PCM and both frames.
        magnitudes[0] *= np.linalg.norm(magnitudes[1]) / np.linalg.norm(magnitudes[0])
    if source.family not in (0, 6, 7, 8, 16):
        # Match the real attack at the handover, not the unrelated level of a
        # later recording window. Keep the listener-approved source families exact.
        attack_rms = np.sqrt(np.mean(attack[-512:] ** 2))
        body_rms = np.linalg.norm(magnitudes[0]) / math.sqrt(2)
        join_gain = attack_rms / body_rms
        magnitudes[0] *= join_gain
        magnitudes[1] *= join_gain
    waves: list[tuple[FloatArray, FloatArray]] = []
    harmonic_limits: list[int] = []
    for mip, size in enumerate(LENGTHS):
        # Match the attack-rate mip selector, not just the wavetable Nyquist.
        # Empty/unreachable high bands retain their fundamental only.
        count = min(size // 2 - 1, len(phases), max(1, int(7200 / (nominal * 2 ** mip))))
        harmonic_limits.append(count)
        order = np.arange(1, count + 1)
        basis = np.cos(2 * np.pi * np.arange(size)[:, None] * order[None, :] / size + phases[:count])
        waves.append((basis @ magnitudes[0][:count], basis @ magnitudes[1][:count]))
    gain = 16000 / max(np.abs(a).max().item() for a in [*attacks, *(w for pair in waves for w in pair)])
    attack_descriptors: list[str] = []
    wave_descriptors: list[str] = []
    pcm_bytes = 0
    phase_errors: list[float] = []
    quantization_leak: list[float] = []
    for mip, values in enumerate(attacks):
        name = f"mc_hybrid_attack_{zone}_{mip}"
        pcm = np.rint(values * gain).astype(np.int16)
        emit_array(header, name, pcm)
        attack_descriptors.append(f"{{{name},{len(pcm)}}}")
        pcm_bytes += pcm.nbytes
    for mip, (early, late) in enumerate(waves):
        names = [f"mc_hybrid_wave_{zone}_{mip}_{tag}" for tag in ("early", "late")]
        pair = [np.rint(values * gain).astype(np.int16) for values in (early, late)]
        for name, pcm in zip(names, pair, strict=True):
            emit_array(header, name, pcm)
            pcm_bytes += pcm.nbytes
        wave_descriptors.append(f"{{{names[0]},{names[1]},{len(early)}}}")
        transforms = [np.fft.rfft(p.astype(np.float64)) for p in pair]
        audible = (np.abs(transforms[0][1:-1]) > 16) & (np.abs(transforms[1][1:-1]) > 16)
        delta = np.angle(transforms[0][1:-1] * np.conj(transforms[1][1:-1]))
        phase_errors.append(np.abs(delta[audible]).max(initial=0).item())
        quantization_leak.append(max(abs(f[-1]).item() for f in transforms))
    steps = [round(65536 * RATE / OUTPUT_RATE * 2 ** ((note - source.root) / 12)) for note in range(109)]
    # v.phase advances BEFORE rendering; pcm_pos is read BEFORE its advance.
    # At root, v.phase + phase_start therefore maps frame zero to recording t=0.
    # Reference coordinate is zero at the middle of the real-attack crossfade.
    phase_start = round((-(reference + 1 / OUTPUT_RATE) * nominal % 1) * 2 ** 32) % 2 ** 32
    descriptor = ("    {{" + ",".join(attack_descriptors) + "},{" + ",".join(wave_descriptors)
                  + "},{" + ",".join(map(str, steps)) + "},{" + ",".join(map(str, decay))
                  + f"}},{source.attack - 256},{source.attack},22050,{phase_start}u,{source.root},{source.family}}},")
    report = ZoneReport(index=zone, family=source.family, root=source.root, name=source.name,
                  source=source.path, source_sha256=source.sha256, source_rate=rate,
                  channel=0, onset_source_frame=offset, attack_frames=source.attack,
                  attack_lengths=[len(a) for a in attacks],
                  pcm_bytes=pcm_bytes, gain=gain, fitted_reference_hz=frequency,
                  analysis_windows_seconds=windows, local_pitch_ranges_hz=pitch_ranges,
                  common_harmonic_phases=[phases[i].item() for i in range(len(phases))],
                  early_magnitudes=[magnitudes[0][i].item() for i in range(len(phases))],
                  late_magnitudes=[magnitudes[1][i].item() for i in range(len(phases))], phase_start=phase_start,
                  harmonic_limits=harmonic_limits,
                  phase_errors_radians=phase_errors, nyquist_quantization_residue=quantization_leak,
                  decay=[decay[i].item() for i in range(len(decay))], source_repository=source.repository,
                  source_commit=source.commit, source_release=source.release)
    return descriptor, report, pcm_bytes


def main() -> None:
    """Write deterministic, standalone assets and exact per-source audit evidence."""
    selected = sources()
    assert len(selected) == 24
    header = [
        "/* Generated by tools/gen_hybrid_pcm.py; code GPL-3.0-only, recordings CC0-1.0. */",
        "/* Provenance and source hashes: assets/hybrid/manifest.json. */",
        "#ifndef MC_HYBRID_PCM_H", "#define MC_HYBRID_PCM_H", "#include <stdint.h>",
        "enum { MC_HYBRID_ZONES = 24, MC_HYBRID_FAMILIES = 17 };",
        "typedef struct {const int16_t *data; uint16_t length;} McHybridAttack;",
        "typedef struct {const int16_t *early,*late;uint16_t length;} McHybridWave;",
        "typedef struct {McHybridAttack attack[7];McHybridWave wave[7];uint32_t step[109];uint16_t decay[128];uint16_t fade_start,fade_end;uint32_t morph_frames,phase_start;uint8_t root,family;} McHybridZone;",
        "/* phase_start: Q32 cycle offset, ADD to v.phase after its pre-render advance.",
        " * attack pcm_pos is read before advancing. At root, phase_start compensates",
        " * one oscillator advance and aligns the shared harmonic coordinate with",
        " * real attack time zero. Detune/transposition changes the first-frame offset",
        " * by (increment-root_increment), not an accumulating phase error.",
        " * Fade bounds are base 16 kHz source frames. Morph is one-way over 22050",
        " * output frames. decay[i] is Q15 at output age i*2048, clamped at i=127.",
        " * Apply decay only to sustain: attack already contains its real envelope. */",
    ]
    descriptors, reports, counts = zip(*(generate(s, i, header) for i, s in enumerate(selected)), strict=True)
    pcm_bytes = sum(counts)
    assert pcm_bytes <= 390144, pcm_bytes
    header.extend(["static const McHybridZone mc_hybrid[MC_HYBRID_ZONES] = {",
                   *descriptors, "};",
                   "static const uint8_t mc_hybrid_family_first[MC_HYBRID_FAMILIES] = {"
                   + ",".join(str(next(i for i, s in enumerate(selected) if s.family == f)) for f in range(17)) + "};",
                   "static const uint8_t mc_hybrid_family_count[MC_HYBRID_FAMILIES] = {"
                   + ",".join(str(sum(s.family == f for s in selected)) for f in range(17)) + "};",
                   "#endif", ""])
    output = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "src/hybrid_pcm.h"
    _ = output.write_text("\n".join(header))
    measurements = list(reports)
    manifest = {"format": 1, "license": "CC0-1.0",
                "license_paths": ["LICENSES/CC0-Meta.txt", "LICENSES/CC0-VSCO.txt"],
                "license_sha256": [digest(ROOT / "LICENSES/CC0-Meta.txt"), digest(ROOT / "LICENSES/CC0-VSCO.txt")],
                "sample_rate": RATE, "peak": 16000, "mip_levels": 7, "zones": measurements}
    _ = (ROOT / "assets/hybrid/manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    report_path = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "build/pcm-quality/hybrid-report.json"
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report = {"pcm_bytes": pcm_bytes, "pcm_budget": 390144, "metadata_bytes_32bit": 24 * 848 + 34,
              "header_sha256": digest(output), "generator_sha256": digest(Path(__file__)),
              "source_hashes": [s.sha256 for s in selected], "zones": measurements}
    _ = report_path.write_text(json.dumps(report, indent=2) + "\n")
    print(f"{output}: {len(selected)} zones, {pcm_bytes} PCM bytes, {report_path}")


if __name__ == "__main__":
    main()
