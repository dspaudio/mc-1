# MC-1 0.1.0 validation record

Host tests pass on the development Mac: 48 distinct presets, 16 drum models,
release/choke/voice allocation, exact swing and bar timing, block-independent DSP,
UI and screen bounds, LA resonance/control regressions, version 1 migration and
version 2 FX/demo round trips, and 6,236 interrupted storage operations. Unreadable
banks recover safely; saving with an unreadable bank is refused.

Each of four effects is checked with an impulse for changed output, applicable
tail energy, bounded output and eventual decay. The built-in demo is exercised
through completion with zero saturation; all effects at maximum are included in
the 10-second multi-part stress test (peak 14,885, zero clips on host). ASan/UBSan
passes. Host engine size: 56,212 B; project: 2,120 B; encoded record: 2,140 B.

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
The reports below now match that run's downloaded artifacts. A local Docker
pi32v2 rebuild reproduced the app and package bytes on 2026-10-14, and the
sanitized host installer simulation passed. Host tests, clean ASan/UBSan binary,
render, native HTTP checks, generated assets and all 49 audio renders also passed.
Compiler installation paths differ between local Docker and CI; the checked-in
target report retains the CI compiler path.

| Target region | Used | Bound | Remaining |
|---|---:|---:|---:|
| App load image | 44,756 B | 581,564 B | 536,808 B |
| Data + BSS | 67,648 B | 98,304 B | 30,656 B |
| RAM code | 2,784 B | 24,576 B | 21,792 B |
| Pool | 0 B | 344,064 B | 344,064 B |

The app SHA-256 is `6fc8e05e555a8e33a2a22f7d496e6ebea7ecdaf63e0110204d9d1142aff94c35`.
The experimental package is 610,019 B, SHA-256
`3c88417678e694d70535501087e59993d717d5e9853f3024cfca60e482331017`.
The RAM loader is 8,140 B, BSS end `0x01C0F3A0`. Static app RAM above sums to
70,432 B excluding stacks/reservations; no measured stack high-water is claimed.

Apple's MP3 decoder produced 44.1 kHz stereo PCM with peak 23,731 and zero saturated
samples. The public MP3 was fetched and verified against the local SHA-256.
The in-app browser displayed the real C demo library, loaded NEON SPIRAL and showed
its four effect defaults (48/35/20/30).
See [source audit](AUDIT.md) and [build/recovery boundaries](../BUILDING.md).

No FM-1 has been flashed or accessed. Installation, cold/warm boot, physical panel,
codec sound, actual flash writes, clock, stack usage, worst-case audio deadline,
UBOOT recovery and long-duration stability remain unverified. The audio app has no
USB updater; an actual device trial needs a known recovery path and separate permission.
