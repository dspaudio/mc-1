# MC-1 0.1.3 validation record

Host tests pass on the development Mac: 128 distinct presets, eight kits with 128 distinct lane renders,
release/choke/voice allocation, exact swing and bar timing, block-independent DSP,
UI and screen bounds, LA resonance/control regressions, version 1 migration and
version 2 migration, version 3 kit/track FX round trips, and 6,252 interrupted storage operations. Unreadable
banks recover safely; saving with an unreadable bank is refused.

Each of four effects is checked with an impulse for changed output, applicable
tail energy, bounded output and eventual decay. The built-in demo is exercised
through completion with zero saturation; all effects at maximum are included in
the 10-second multi-part stress test with all four track processors and master FX
at maximum (peak 28,589, zero clips on host). A separate coherent-voice test at six
tempos deliberately saturates cascaded delay/reverb and verifies the final clamp
without signed overflow. ASan/UBSan passes. Host engine size: 269,384 B;
project: 2,136 B; encoded record: 2,156 B.

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
the mixer → groove → demo page cycle. The clean ASan/UBSan binary, host render,
native HTTP checks, all 129 audio renders, pi32v2 memory guards, package build and
sanitized installer simulation passed. No hardware timing result is implied.
Compiler installation paths differ between local Docker and CI; the checked-in
target report retains the CI compiler path.

| Target region | Used | Bound | Remaining |
|---|---:|---:|---:|
| App load image | 461,096 B | 581,564 B | 120,468 B |
| Data + BSS | 11,472 B | 98,304 B | 86,832 B |
| RAM code | 2,784 B | 24,576 B | 21,792 B |
| Pool | 269,384 B | 344,064 B | 74,680 B |

The app SHA-256 is `2d6180ed1a371f844049389dff337564f118b38e2459839a74d103ad791e3b11`.
The experimental package is 610,019 B, SHA-256
`9619ad842204a7d7505f98507cd80f602955c698af75f0c637ed66808b6a6f3f`.
The RAM loader is 8,140 B, BSS end `0x01C0F3A0`. Static app RAM above sums to
283,640 B excluding stacks/reservations; no measured stack high-water is claimed.

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
sample ends. New patches round-trip through project version 3; invalid source IDs and
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
