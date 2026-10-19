# MC-1 1.1.0 - dual-core firmware

Version 1.1.0 uses both FM-1 CPU cores. It retains eight melodic voices and six
drum voices: CPU0 owns sequencing/MIDI/control and five melodic DSP voices,
while CPU1 processes three melodic voices, all drums and all four shared FX
banks. The browser emulator shares the C engine and renderer; it does not
simulate WL82 core timing or hardware exception recovery.

## New in 1.1.0

- Four independent banks: R/D1, C/F1, R/D2 and C/F2, with four sends per part.
- Melodic Analog variance with bounded detuning and smooth drift.
- Warm second-part reverb/delay and PCM-forward third parts in both demos.
- Readable white 6x9 small text, populated-only flowing playback underlines,
  continuously lit menu/OCT buttons, selected-menu blinking and BPM-synchronous
  green PLAY. HOME stays lit during playback.
- Source-pinned hardware evidence, private worker-state ownership and bounded
  failure fallback. Eight melodic / three worker voices are locked in the
  target builder; larger configurations are not supported.

## Choosing a firmware

The installer offers 1.1.0 dual-core and the unchanged original 1.0.2 single-core
package. The latter is not a current-source single-core build. Its SHA256 is
`a09e763e6000f25dec63eb5feeb6251389dce33bf5abe2eb4746f1f1c7091ab5`.
Neither option changes the bootloader, project-bank addresses or flash allowlist.
FM-1_900 identifies the update protocol, not the installed MC-1 version.

1.1.0 imports project formats 1-7 and saves format 8. 1.0.2 cannot open format-8
saves. A downgrade preserves bank bytes but does not convert them; subsequent
saves may replace earlier projects. Keep compatible backups where available.

## Failure and recovery boundaries

For a CPU1 timeout/protocol failure, the engine stops using the worker, rejects
its incomplete state and recomputes that work on CPU0. No voice reduction or
automatic FX bypass is performed. Single-core fallback can exceed the audio
deadline under heavy load; it is not guaranteed seamless playback.

A fatal system exception stops audio and attempts UBOOT entry after CPU1 hold.
If hold cannot be confirmed, audio stops without forcing that transition.
Power cycling retries the installed app; there is no automatic rollback.
Startup faults can therefore recur. ROM WL80UBOOT1.00 is not the MIDI updater
and cannot be recovered by this web installer.

The eight-voice candidate passed the tested four-FX/Analog/USB workloads and
controlled worker-failure playback. Matched two-bank rendering improved
27.1-27.6%, below the retained 30% comparison target. Maximum-load display
refresh remains load-dependent; no universally seamless UI/audio claim is made.
Exact images, limits, rejected configurations and recovery readbacks are in
[the hardware report](FOUR_BANK_HARDWARE_2026-10-19.md).

The release rebuild is byte-identical to that tested eight-voice package:
610,089 bytes, SHA256
`e0afbf5254eb910133cfab170928dd306d5d9e8eb5a47afc29094c4906649228`.
The raw app SHA256 is
`d7654e41eb9ef375339127abed6b8bc8da026d59560d694001fbe72ba80e177f`.
This identity was checked by full byte comparison, not inferred from settings.

Release checks include all 30 native C executables, 51 Python cases, a fresh
30-executable ASan/UBSan build, pi32v2/package/loader simulations and reproducible
target reports. Twenty-six actual C-renderer screenshots were refreshed.
Both 32-bar demo exports are stereo 44.1 kHz/320 kbps MP3, with nonzero decoded
PCM, exact source frame counts and zero integer rail clips.
