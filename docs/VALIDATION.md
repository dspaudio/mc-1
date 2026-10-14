# MC-1 0.1.2 validation record

Host tests pass on the development Mac: 64 distinct presets, eight kits with 128 distinct lane renders,
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
without signed overflow. ASan/UBSan passes. Host engine size: 269,352 B;
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
native HTTP checks, all 65 audio renders, pi32v2 memory guards, package build and
sanitized installer simulation passed. No hardware timing result is implied.
Compiler installation paths differ between local Docker and CI; the checked-in
target report retains the CI compiler path.

| Target region | Used | Bound | Remaining |
|---|---:|---:|---:|
| App load image | 59,772 B | 581,564 B | 521,792 B |
| Data + BSS | 11,472 B | 98,304 B | 86,832 B |
| RAM code | 2,784 B | 24,576 B | 21,792 B |
| Pool | 269,352 B | 344,064 B | 74,712 B |

The app SHA-256 is `7de88201fe5d6bfb796586b0551ce408ed0e89b5c4b1f5792aa0586038e9c2b7`.
The experimental package is 610,019 B, SHA-256
`6a0ce8c18a75e3039946e6edce4abdb31ea231aa5559468f6b9dd583a07d4b62`.
The RAM loader is 8,140 B, BSS end `0x01C0F3A0`. Static app RAM above sums to
283,608 B excluding stacks/reservations; no measured stack high-water is claimed.

Each kit passes all 16 non-silent, distinct-output, decay and hat-choke checks;
changing the selected kit leaves existing drum voices unchanged. The 64-second
[kit audition](audio/kit-showcase.wav) plays eight seconds per kit in list order.
Original 48 preset PCM hashes, factory demo and five existing listening files are
byte-identical to the previous revision. The new short RATCHET PLUCK was adjusted
after its initial RMS fell below the existing audio threshold; the final 65 renders
pass the unchanged energy, DC, clipping and release checks.

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
