# MC-1 validation record

## 0.3.0

Song mode stores 128 pattern/repeat entries in a 35,114-byte version 6 record.
Tests exercise empty songs, every entry, pattern 64, 1–16 repeats, bar-boundary
advance, end stop, restart/continue, looping, edits, malformed records and v5
imports. Song editing/save also runs through the actual native HTTP service.
The two existing 36 KiB flash banks are unchanged; storage fault injection covers
463 erase/program/header interruption points with the expanded record.

MIDI tests exercise separate source parsers, running status and interleaved
realtime bytes, notes/velocity, program and controller changes, sustain, pitch
bend, recording, generated ARP output, note-off ownership, queue overflow recovery
and transport at 60/120/240 BPM. Web AudioWorklet tests compare PCM, MIDI packets
and saved projects with native C, including disconnect and bounded queues.

Actual USB driver tests inspect composite MIDI/UAC descriptors, 44.1 kHz PCM
packetization, saturation, stream pause/re-prime and FIFO boundaries. UART tests
exercise running status and realtime parsing; routing tests verify USB/TRS source
identity, reset handling and output backpressure. These are host tests of the
driver code, not USB enumeration or electrical TRS tests.

Native LCD renders cover empty/editing/playing/completed songs, position 128,
16 repeats, MIDI clock/channel controls and melodic ADSR. Browser automation was
not used. Existing 129 audio-render reports are byte-identical to 0.2.0.

Onboard BLE MIDI is not implemented: the current runtime does not initialize
the official SDK Bluetooth controller, stack, RTOS or RF. See [MIDI.md](MIDI.md)
for source evidence and the distinction from OS-provided Web MIDI ports.
No device was flashed. FM-1 USB, MIDI, RF, CPU deadlines and listening still
require hardware validation.

The final pi32v2 app uses 482,268 bytes of its 581,564-byte slot, leaving 99,296.
Static RAM sections total 354,144 bytes: globals 46,656, engine pool 304,704 and
RAM code 2,784. The pool retains 39,360 bytes within its reserved region. These
linker measurements do not establish CPU deadlines or stack high-water marks.

## 0.2.0 historical validation

The project now contains 64 patterns × 4 parts × 16 steps. The version 5 portable
record is 34,850 bytes; its in-memory project is 35,214 bytes. Tests cover all 64
queued bar transitions, empty new patterns, recording and automation at pattern 64,
addresses through 319, malformed final-pattern data and unchanged destinations on
failed decode. Existing preset/patch snapshots still match at the explicit previous
master setting; demo arrangements and instrument parameters are unchanged.

Two 36 KiB banks occupy only `0x99000..0xAAFFF`. There is no automatic migration
of development-version flash saves. Storage tests exercise 460 erase, page-program
and header-commit interruption points, read failures, verification failures, range
guards and sequence rollover. A second integration suite checks 280 sampled cuts.
The installer simulation checks preservation of both the old 8 KiB and new 72 KiB
regions. No physical flash operation has been performed.

The final output gain is six times the previous setting. A fixed-point stereo-linked
limiter has a 30,000-sample ceiling, instantaneous attack and approximately 50 ms
release time constant. Tests cover unity transparency, stereo ratios, extreme inputs,
silence, reset, block independence and dense polyphony with all track/master effects.
The hot test reaches the ceiling without clipping. At native default gain, NIGHT
CIRCUIT measures RMS −21.408 dBFS and NEON SPIRAL −19.943 dBFS, about +15.57 dB
relative to the prior master setting, with no limiting in either default demo.
Web master defaults to full scale and shares the firmware's quadratic control map.
These are PCM measurements, not loudness-weighted LUFS or hardware listening results.

The recording metronome is configured with GROOVE knob 4 and saved in the project.
Native and WASM tests verify quarter-note timing, first-beat accent, swing, immediate
stop, and silence unless PLAY, REC and METRO are all enabled. Clicks bypass track
mute/effects and do not write notes or automation. ON/OFF project round trips,
invalid serialized settings and variable audio block sizes are covered.

The final pi32v2 build uses 471,876 bytes of the 581,564-byte app slot (109,688 free).
Static RAM sections total 348,028 bytes: 42,032 globals, 303,212 pool and 2,784 RAM
code. The pool has 40,852 bytes free within its region. Hardware CPU deadlines,
stack high-water marks and listening remain untested.

Current audio and build measurements are in [audio-report.json](audio-report.json),
[showcase-report.json](showcase-report.json), [target-report.json](target-report.json)
and [package-report.json](package-report.json). Listening examples and the MP3 have
been regenerated; older byte-identical audio claims below describe the 0.1.4 release.
Browser operation and real-device audio deadlines remain unverified.

## 0.1.4 historical validation

Host tests pass on the development Mac: 128 distinct presets, eight kits with 128 distinct lane renders,
release/choke/voice allocation, exact swing and bar timing, block-independent DSP,
UI and screen bounds, LA resonance/control regressions, version 1 migration and
version 2/3 migration, version 4 kit/track FX round trips, and 7,842 interrupted storage operations. Unreadable
banks recover safely; saving with an unreadable bank is refused.

Each of four effects is checked with an impulse for changed output, applicable
tail energy, bounded output and eventual decay. The built-in demo is exercised
through completion with zero saturation; all effects at maximum are included in
the 10-second multi-part stress test with all four track processors and master FX
at maximum (peak 28,589, zero clips on host). A separate coherent-voice test at six
tempos deliberately saturates cascaded delay/reverb and verifies the final clamp
without signed overflow. ASan/UBSan passes. Host engine size: 271,708 B;
project: 3,726 B; encoded record: 3,746 B.

The 0.1.4 additions have separate core, UI-input, performance and typed-effects
suites, all run optimized and in fresh ASan/UBSan builds. Input tests cover first
turn, 59/60 ms acceleration boundaries, reversal, context changes, clock rollover,
small-range exclusions, exact 499/500 ms popup expiry and all 128 list strip bounds.
The native HTTP test proves the popup expires while transport is stopped. Pixel
tests require green only for audible unmuted hits in both melodic overview and drum
detail; zero-velocity and muted steps remain non-green. Actual C captures cover
13 screens: ARP off/on/latch, FX amount/type/filter and master scope, preset origin
stars, popup, automation full/cleared and playing home/drums. Visual review found
no clipped labels or overlaps. Selection remains orange and playback green.

Performance tests cover six ARP orders, live key release/latch, generated-note
recording, automation overwrite/capacity/pattern clear and version 4 round trips.
Live ARP and recorded playback PCM match exactly for all three rates at swing 0
and 30. Type tests distinguish all 15 effects, verify delay impulse timing/stereo,
filter frequency response, phaser notch, state clearing and 192 combinations over
five cascaded processors. Legacy DSP impulse hashes at four tempos remain exact.
UI tests cover FX subpages, scoped parameter edits, ARP controls, per-lane levels,
automation recording and origin-name stars. Version 1–3 imports preserve patches;
unknown old modified-patch origins display IMPORTED SOUND.

NEON SPIRAL uses all four native effects. The MP3 is 320 kbps stereo 44.1 kHz,
58.906 seconds including encoding padding; WAV is 58.857 seconds including tails.
Apple AudioFile inspection recognized the MP3. Its provenance and hashes are in
[mp3-report.json](mp3-report.json); raw/normalized renders are in
[showcase-report.json](showcase-report.json). No external musical effects were used.

[CI 38512065587](https://github.com/dspaudio/mc-1/actions/runs/38512065587) passed
host, pi32v2 target, SDK hash checks and sanitized installer simulation at
`9e166c68396b4b6a5f4990266b66393450f9ea3f`, before the factory-bank expansion.
For the 48 presets and twelve PCM attacks at
`f41e6728cdbca74e10009dc11ced1ec388c4826a`,
[CI 38514033913](https://github.com/dspaudio/mc-1/actions/runs/38514033913)
passed host checks, target memory guards and package generation, then failed on
stale checked-in target-report bytes/hash; its installer test was not reached.
The report repair was verified by a local Docker pi32v2 rebuild reproducing that
run's app and package bytes on 2026-10-14, then by
[CI 38523690699](https://github.com/dspaudio/mc-1/actions/runs/38523690699)
at `57e6b55`, which passed both jobs.

The current reports below include the expanded bank, kits and per-track FX as well
as home-grid pitch marks and GLO global mixer. Local host tests prove all four demo patterns advance and change
the rendered melodic grid, including chords. The home PATTERN/NEXT status is
checked across queued bar transitions on both melodic and drum pages, with
save/load message priority and no overlap of the sixteenth drum row.
Mixer checks cover independent
part levels, clamps, audible output, mute indicators, the mixed-case
`SELECT-ARP to mute` hint, project serialization and
the mixer → groove → demo → automation page cycle. The clean ASan/UBSan binary, host render,
native HTTP checks, all 129 audio renders, pi32v2 memory guards, package build and
sanitized installer simulation passed. No hardware timing result is implied.
Compiler installation paths differ between local Docker and CI; the checked-in
target report retains the CI compiler path.

| Target region | Used | Bound | Remaining |
|---|---:|---:|---:|
| App load image | 469,896 B | 581,564 B | 111,668 B |
| Data + BSS | 14,672 B | 98,304 B | 83,632 B |
| RAM code | 2,784 B | 24,576 B | 21,792 B |
| Pool | 271,708 B | 344,064 B | 72,356 B |

The app SHA-256 is `f85711197fdc6ceff0288e54198f09ba8e8087bd54aa870976f38ae163e660b0`.
The experimental package is 610,019 B, SHA-256
`976db107928f37ae2b10abb379ccb2c3020b90f115a8d3de58c527846d2be168`.
The RAM loader is 8,140 B, BSS end `0x01C0F3A0`. Static app RAM above sums to
289,164 B excluding stacks/reservations; no measured stack high-water is claimed.

Each kit passes all 16 non-silent, distinct-output, decay and hat-choke checks;
changing the selected kit leaves existing drum voices unchanged. The 64-second
[kit audition](audio/kit-showcase.wav) plays eight seconds per kit in list order.
All 64 presets from 0.1.2 retain their names, parameters and PCM hashes after matching
by name across the reordered bank. Factory demo output and all six existing listening
WAVs, including NEON SPIRAL and the kit comparison, are byte-identical to 0.1.2.
Independent frozen fixtures check the old 64 patch/PCM results and both built-in
demo project records. All 129 current renders pass the unchanged energy, DC,
clipping and release checks.

The 64 new patches use all 16 CC0 instrument recordings, with PCM mix 127. Tests
change oscillator waveforms while confirming identical PCM-only output, then verify
that lowering PCM mix audibly changes the rendered samples. All 112 mip tables stay
within ±16,000 and valid interpolation/loop bounds. Eighty integration cases exercise
MIDI notes 12, 36, 60, 84 and 108 across all sources, block boundaries, held loops and
release. Independent sanitizer coverage checks all 97 supported notes and fractional
sample ends. New patches round-trip through project version 4; invalid source IDs and
recorded sources assigned to Acid are rejected. Earlier saves load with their original
patches; earlier firmware cannot load new recorded-source IDs.

Offline regeneration verifies the retained WAV and CC0 license hashes and produces
byte-identical PCM. Source commits, SFZ keycenters and conversion settings are in the
[sample manifest](../assets/recorded/manifest.json). The conversion uses 128-tap Blackman
resampling at 90% of Nyquist, 63-tap half-rate mip filtering, 2 ms onset margin, a
2 ms fade-in and 64 ms end fade or sustain-loop crossfade. Each base excerpt is
0.384 seconds at 16 kHz; looped instruments repeat 0.192–0.384 seconds. These are
compact single-note, single-velocity recordings, not full multisampled instruments.
All measured loop wrap jumps remain below each table's maximum adjacent jump;
this is a numeric discontinuity check, not a listening-quality guarantee. Pitch roots
follow the upstream SFZ mapping. Near-root autocorrelation estimates agree within
16.2 cents for 15 recordings; the inharmonic glockenspiel estimate differs by 68.3
cents, so no pitch correction was inferred or applied.

The [48-second recorded audition](audio/recorded-showcase.wav) plays one preset per
source at MIDI note 60 in manifest order, three seconds each, with no external effects.
An eight-voice recorded-PCM stress render with all track and master FX takes 0.061
seconds of host CPU for ten seconds of audio in the optimized Mac build. This does
not establish target CPU load, codec sound, IRQ latency or hardware stability.

The generator validates 128 unique names, patches and symbolic IDs, and 14 contiguous
category ranges. Tests browse all 128 sounds on each melodic part, wrap in both
directions and resume from the actual patch after project loading. Actual C frame
captures cover the first preset, Acid boundary, Mallet/Bell, long Hit/Percussion
category, last preset 128 and both demos; labels and number fit above the grid.

Track effects are tested across all 16 part/effect combinations for changed output,
isolation from other parts, muted-tail suppression and clearing buffers on dry bypass.
UI checks cover pattern/sound encoder roles, kit wrap, selected-track/master FX scope,
and saving all 16 amounts. Valid-CRC out-of-range kit/FX records are rejected without
changing the destination. Version 1/2 imports default to kit 0 and dry track FX.
Actual C frame captures of the first/last kits, edit mode, melodic/drum FX and master
FX were visually checked for labels and overlap. No browser was used for this revision.

Apple's MP3 decoder produced 44.1 kHz stereo PCM with peak 23,731 and zero saturated
samples. The public MP3 was fetched and verified against the local SHA-256.
During the earlier effects/demo revision, the in-app browser displayed the real C demo library, loaded NEON SPIRAL and showed
its four effect defaults (48/35/20/30).
See [source audit](AUDIT.md) and [build/recovery boundaries](../BUILDING.md).

No FM-1 has been flashed or accessed. Installation, cold/warm boot, physical panel,
codec sound, actual flash writes, clock, stack usage, worst-case audio deadline,
UBOOT recovery and long-duration stability remain unverified. The audio app has no
USB updater; an actual device trial needs a known recovery path and separate permission.
