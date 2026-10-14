# MC-1 0.2 validation record

Host tests pass on the development Mac: 12 distinct presets, 16 drum models,
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

Target/package reports will be replaced with this FX revision's CI evidence before
release. Earlier reports are historical and do not establish this revision's size.
See [source audit](AUDIT.md) and [build/recovery boundaries](../BUILDING.md).

No FM-1 has been flashed or accessed. Installation, cold/warm boot, physical panel,
codec sound, actual flash writes, clock, stack usage, worst-case audio deadline,
UBOOT recovery and long-duration stability remain unverified. The audio app has no
USB updater; an actual device trial needs a known recovery path and separate permission.
