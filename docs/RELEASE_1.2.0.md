# MC-1 1.2.0 - rebuilt LA-PCM engine and sound bank

**A new LA-PCM engine. All 112 LA sounds replaced.**

This is an engine and instrument-data replacement, not just a preset expansion.
Real recorded attacks restore piano, nylon guitar and orchestral character.
Source-derived harmonic sustain frames share one phase accumulator, replacing
the old generic detuned blend and repeated sustain motion. The 16 Acid presets,
eight synthesized drum kits, eight melodic voices and four FX banks are retained.

## New sound engine and recordings

- All 112 LA presets use real PCM attacks plus synthesized harmonic sustain.
  This remains LA-PCM, not a GM/SoundFont player or a full-note sample loop.
- Twenty-four source zones cover 17 instrument families: six piano registers,
  three nylon-guitar registers and 15 other families.
- Individual attack colours, PCM contribution and envelopes distinguish each
  preset. Phase-coherent, energy-matched sustain avoids unintended beating and
  per-note swells; plucked instruments retain measured non-cyclic decay.
- Reverb return level and mode-specific decay make ROOM and HALL easier to hear
  and distinguish, using the existing delay storage.
- Shorter attack-cache snapshots and family-indexed zone lookup remove the
  deadline misses found in the first installed candidate without changing the
  accepted audio. All eight approved representative WAVs remain byte-identical.

The selected recordings are CC0-1.0: Meta Open Sound piano/nylon guitar and
VSCO 2 Community Edition's other 15 families. Firmware, original patches and
compositions remain GPL-3.0-only. See [NOTICE](../NOTICE.md), the
[24-zone source manifest](../assets/hybrid/manifest.json) and both retained
[Meta](../LICENSES/CC0-Meta.txt) / [VSCO](../LICENSES/CC0-VSCO.txt) licenses.
The downloaded 752,311,261-byte piano source ZIP is not installed on the FM-1.

## Hear 1.2.0

- [Hip-hop piano](audio/mc-1-hip-hop-piano.mp3), 86 BPM.
- [Pop nylon guitar](audio/mc-1-pop-guitar.mp3), 110 BPM.
- [Chamber strings](audio/mc-1-chamber-strings.mp3), 90 BPM.
- Updated [NIGHT CIRCUIT](audio/mc-1-night-circuit-fx.mp3) and
  [NEON SPIRAL](audio/mc-1-neon-spiral-fx.mp3) full-groove demos.

These original compositions are native shared-C renders, not FM-1 recordings.
The five 320 kbps MP3s have working native browser players and downloads on the
product page. [Audio hashes and export settings](release-audio.json) are retained.
The representative four timbres have listener approval; numerical checks are
not a blanket listening-approval claim for every remaining preset.

## Installed and measured on an FM-1

The final package was installed successfully, restarted and reidentified as
`FM-1_900`. This is the updater identity, not an installed-version readout.
The package hash below identifies the tested image. A new flash backup was
explicitly declined by the operator; none was taken.

Eleven captures cover both built-in demos and four maximum-load cohorts:
8 melodic / 6 drum voices with 0, 1 or 2 Acid parts, and three monophonic Acid
parts / 6 drums. The maximum-load settings use four FX banks, Analog 127,
kit 6, R/D type 5, C/F type 5, sends 127 and active USB audio.

All eleven frozen 8,192-block timing windows pass. The accompanying
44.1 kHz/16-bit stereo USB recordings total 495 seconds; each 45-second file
includes warmup, the active measurement and a post-STOP tail.

- Deadline misses, DMA-half changes and USB error deltas: **0**.
- CPU1 remains READY, worker failures **0**, guards intact.
- Minimum unused CPU1 user stack: **3,524 bytes**.
- Every recording contains non-silent audio with no integer rail clipping.
- The originally failing first demo also passes three additional full captures.

The highest recorded whole-audio maximum is 69,184 raw timer ticks. This is
tested-workload evidence, not a guarantee for arbitrary arrangements. A few
captures contain physical master-ADC drift and are not matched speed comparisons.
No new power-loss or forced-worker-failure trial is claimed.
See [raw counters and recording hashes](la-pcm-hardware.json).

The first installed candidate failed with deadline/DMA/USB errors. A cache-only
candidate still failed the first demo once. Neither is the release image.

## Size and reproducible identity

| Resource | Final | Limit or previous budget |
|---|---:|---:|
| PCM audio data | 384,240 B | 390,144 B |
| App | 503,980 B | 581,564 B |
| Globals | 55,692 B | 65,536 B policy |
| Pool | 303,464 B | 335,872 B policy |
| FWSC package | 610,089 B | Includes loader and fixed padding |

App SHA256:
`9ebcd27952284c2298f4f8f51ac490b3f49cf012aa64ab15fe899d344ef51256`

Package SHA256:
`cb0b5454d3e459da11489f73818d4537093907aa13adee0230f3404227a701e4`

Target source fingerprint:
`54122b1698f1880d7b5895890e65ab7d87c57276dcbcc6f0747fd1f955081556`

Checks include 31 native C executables, 51 Python tests, a fresh 31-executable
ASan/UBSan build, exact single/dual-core PCM and complete state, all 4,096 FX
combinations, 129 current audio renders, deterministic assets, pi32v2 linking,
package/OTA/loader simulation and product/installer resource checks.

## Installation and saved projects

Install `mc-1-1.2.0.fwsc` using the web installer or bundled desktop installer.
The unchanged original 1.0.2 single-core package remains available as a fallback,
not a single-core rebuild of 1.2.0. Verified restoration of official V15 still
requires the operator's original official file.

Project format remains 8, importing formats 1-7. Existing LA patches load with
the replacement sources and therefore sound different. Preset names/positions
remain stable. 1.0.2 cannot open format-8 saves; changing firmware is not project
conversion. The bootloader, project-bank addresses and flash allowlist are unchanged.
The new nylon-guitar PCM source is ID 40; 1.1.0 rejects projects containing it
even though both releases use project format 8.

CPU1 failure still falls back to CPU0 without reducing voices; full-load
real-time audio is not guaranteed in that state. A fatal fault can attempt
UBOOT, and power cycling is not automatic rollback. The web/desktop MIDI
installer cannot recover ROM `WL80UBOOT1.00`. Existing recovery boundaries are
documented in [the prior hardware record](FOUR_BANK_HARDWARE_2026-10-19.md).
