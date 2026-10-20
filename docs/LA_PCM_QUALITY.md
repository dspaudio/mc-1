# LA-PCM quality correction, 2026-10-20

This records the sound correction now prepared for MC-1 1.2.0. The final image
was installed and measured on an FM-1; see [release evidence](RELEASE_1.2.0.md).
Eight melodic voices, all 16 Acid presets and the synthesized drums are retained.

## Sound engine and sources

All 112 LA presets now combine a real PCM attack with a synthesized harmonic
sustain. The old generic detuned oscillator blend is not used by LA. Twenty-four
zones cover 17 families: six piano registers, three nylon-guitar registers and
15 retained VSCO families. Two evolving sustain frames share their partial
phases and pitch accumulator. Plucked sounds use measured, non-cyclic decay;
held frames have matched spectral energy. This remains LA-PCM, not a GM
SoundFont player or a complete-note sample loop.

The full 752,311,261-byte Meta piano WAV archive was downloaded and matched
SHA256 `ee9023dbde849ecab393a098aa0cfbd6f2ef48d4a096b9c08808b72fc913f70e`.
Only selected hash-pinned WAVs are retained in `assets/hybrid/`; the full ZIP
stays under ignored `build/`. Source provenance and CC0 licenses are recorded
in `assets/hybrid/manifest.json`, `NOTICE.md` and `LICENSES/`.

| Resource | Bytes |
|---|---:|
| Real attacks plus synthesized sustain frames | 384,240 |
| Previous PCM budget | 390,144 |
| Hybrid metadata, 32-bit target | 20,386 |
| Legacy source-ID aliases, without duplicate PCM | 1,428 |

The LA lowpass now opens sufficiently to retain upper partials. Real attacks
remain outside that sustain filter. A coefficient-dependent feedback bound
prevents high-cutoff resonance from producing an alternating self-oscillation.
Original Acid patch definitions and independent legacy PCM hashes remain the
reference for Acid preservation.

## Bowed attacks and listener feedback

The initial detune/filter-only renders were rejected by the listener. Those
melody files were removed; they are not evidence that the final sound passed.

The new source-backed piano/guitar direction was retained. Subsequent feedback
identified accented violin/string attacks. Cello's early/late synthesized frame
RMS differed by 16.21 dB; matching the bowed spectral energies removes this
unintended per-note swell. Current early/late power ratios pass 0.97..1.03 for
violin, viola and cello. Violin BOW uses attack 60; the ensemble's VIOLA CHAMBER,
CELLO ENSEMBLE and VIOLIN SILVER use 70/76/70. The ensemble upper part is quieter.
No optional Analog variance or C/F bank is enabled in the listening pieces.

Latest listening files are under `build/pcm-quality/pieces-v4/`, with separate
`-dry.wav` and `-reverb.wav` files for piano, violin, strings and guitar.
Each is 888,268 frames (20.142 seconds), stereo 44.1 kHz/16-bit, with zero
clips and zero voice steals. The accepted piano/guitar dry files remain
byte-identical after the reverb correction. After the bowed energy/attack
correction, the listener explicitly approved all four timbres: "네 음색 모두 합격".
These six presets and their source arrays are frozen; numerical checks alone
did not establish that pass.

## Remaining 106 presets

The four-piece approval covers six presets, not the entire bank. All remaining
106 LA presets were subsequently corrected across 13 categories. Their mix,
attack colour and envelopes follow each named sound. The other 12 source
families have level-matched PCM/body joins; held frame energy and plucked spectral
energy are normalized separately from intentional decay. Reduced-mix bodies
start with a 64-frame ramp to avoid a first-sample discontinuity.

Current auditions are in `build/pcm-quality/remaining-bank-final/`: 13 category
WAVs and `bank-index.csv` with each preset's offset. All 106 four-second segments
have zero clips and zero voice steals. The eight approved dry/reverb piece WAVs
remain byte-identical after these changes, as do their 252 source arrays.
The remaining 106 presets have not yet received listener approval.

## Reverb

Existing delay storage now supports orthogonal feedback and mode-specific
decay/density. Return gain compensates the actual mixer's wet attenuation.
At send80, impulse T20 intervals are ROOM2 0.097 s, HALL1 0.499 s and HALL2
0.765 s; these are measured -10..-30 dB intervals, not extrapolated RT60.
See `build/pcm-quality/reverb-report.md` for before/after wet/dry energy,
percussive comparisons, zero-send identity and all-bank safety evidence.

## Reproduction and verification

```sh
uv run tools/gen_hybrid_pcm.py
python3 tools/gen_recorded_pcm.py
python3 tools/gen_presets.py
make test
make render
make build/render-instruments
build/render-instruments build/pcm-quality/pieces-v4
make build/render-pcm-bank
build/render-pcm-bank build/pcm-quality/remaining-bank-final
python3 tools/build_target.py
```

Final checks pass:

- `make test`: 31 native C executables and 47+4 Python tests, exit 0.
- Fresh separate Clang 23 ASan/UBSan build: all 31 C executables, exit 0;
  no sanitizer findings. The Python suite passes in `make test` above.
- The initial complete replacement passed 448 C3/C4/C5/chord renders without
  clipping. After the remaining-preset corrections, all 106 category auditions
  have zero clips/steals and the eight approved piece WAVs remain byte-identical.
- Dual-core comparisons: 3,695,784 frames and the complete 112,752-byte engine
  match exactly, including all 4,096 four-bank combinations.
- Independent asset regeneration: hybrid and alias headers are byte-identical.
- pi32v2 build: app 503,980/581,564 B; globals 55,692/65,536 B policy;
  pool 303,464/335,872 B policy; eight melodic / three worker voices.
  App SHA256: `9ebcd27952284c2298f4f8f51ac490b3f49cf012aa64ab15fe899d344ef51256`.
- FWSC packaging and offline OTA/loader simulation pass. The 610,089-byte
  container includes the loader and fixed padding, not just the app slot.
  Package SHA256: `cb0b5454d3e459da11489f73818d4537093907aa13adee0230f3404227a701e4`.
  Tested local candidate: `build/release120/dual120-indexed-profile.fwsc`.

The retained eight-voice cap is unchanged. Shorter remaining-preset releases
remove the earlier factory arrangement's 45 oldest-voice steals; the final
16-second factory render has zero clips and zero steals. This is not a claim
that every possible arrangement is steal-free.
Hardware CPU deadlines are not established by host rendering. The final device
image uses 32-sample attack-cache windows and family-indexed zone lookup.
Eleven normal-workload captures and 495 seconds of USB audio have zero
deadline/DMA/USB errors and healthy CPU1 state; raw evidence is in
[the hardware record](la-pcm-hardware.json). Earlier failing candidates are not
the release image. Host audio approval and measured device stability remain
separate evidence.
