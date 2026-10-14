# Preview delivery audit

This is source inspection and executable host/target-build evidence, not hardware QA.

## Entry, initialization and memory

- `firmware/crt0.S` retains the BSP `goto +4; rts` entry at XIP `0x02000120`,
  saves inherited CPU state, disables IRQs and installs separate user/system stacks.
- `app.ld` preserves the upstream XIP, RAM-text, globals, pool, guarded stack,
  noinit and boot/vector boundaries. Target checks the entry bytes, section sizes,
  aligned load images and absence of calls from flash-off RAM routines.
- `fm1_cstart` initializes fatal vectors, clears BSS/pool, copies data and RAM-text,
  then enables guards. Audio/input handlers are attached before foreground flash
  reads can re-enable IRQs. Persistent project loading happens while transport and
  voices are initially silent. Device execution of this sequence is unverified.
- Timer ticks use the explicit 24 MHz source; CPU frequency is inherited, not measured.
  The audio callback has a 128-frame/2,902-microsecond budget and records maximum
  elapsed microseconds and missed deadlines. Static RAM is not a stack measurement.
- The separate installer links entirely into RAM starting at `0x01C0A800`.
  Section end checks, linker limits and LZ4 round trips run in CI. The upstream
  loader recognition marker remains intact for existing updater compatibility.

## Persistence and installer boundaries

- App read/erase/program hooks use the 0.2.0 `0x99000..0xAAFFF` allowlist,
  with the known `0x856014` JEDEC gate. No auto-save or boot-time format.
- Load now recovers a valid other bank when one read hook fails. Save refuses
  any unreadable bank before erase: inability to read is not proof a bank is empty.
- The new record payload precedes its commit header. Full readback must match
  every encoded byte; CRC/semantic decoding is also checked. Tests retain the old
  bank through modeled erase, page-program and commit-header interruption boundaries.
- First save replaces the previous firmware's project slot 0. All other app
  regions are outside the write allowlist. No calibration import is implemented.
- The installer writes only `0x4000..0x92FFF`, plus valid update-record cleanup
  below `0xFC000`. Host sentinels verify project, calibration/global/tail retention;
  boot head is compared byte-for-byte. Wrong chip key refuses before app erase;
  corrupted transfer retries, persistent corruption/no host finish retains recovery
  record; unknown JEDEC refuses application writes.
- This is upstream loader protocol simulation with generated old/new package images,
  not USB testing or electrical fault injection. The audio app has no USB updater:
  see BUILDING.md for the unverified UBOOT/recovery boundary before any device trial.

## Control and audio audit

- LA resonance previously fed back from unused filter stage 4 although LA uses
  two stages. It now uses the final active stage. Acid continues using stage 4.
- PCM comparisons assert audible-output changes from all four LA/acid performance
  controls and all four envelope controls; acid glide tests use a slide note.
  Max LA resonance is exercised across low/high cutoff and MIDI notes 12–108.
- Drum SOUND/STEP/HIT/LEVEL state and level-to-silence are tested. GLO tempo/swing/
  pattern/part-level and melodic STEP/VEL/GATE/A-S plus note entry are exercised.
  GLO TEMPO now displays BPM itself; the previous value was incorrectly halved.
- Sequencer timing, swing, queued bar switching, mute/stop, voice limits and all
  screen strip bounds are tested. These checks establish state/PCM behavior, not
  correct physical encoder direction, key wiring, analog noise or subjective quality.
- New listening examples use the shared native core. NEON SPIRAL is an original
  broad psy-trance study with native built-in performance automation. It is not an
  MC-505 song transcription, device recording, or general-purpose song editor.

## Still requires an authorized physical device trial

USB updater/version acceptance, loader handoff, installation, cold/warm boot,
UBOOT recovery, LCD/keys/encoder/ADC mapping, codec listening, actual NOR saves,
battery behavior, CPU clock, stack high-water, interrupt timing and long stability.
No hardware was flashed or accessed during this work.

The 0.1.0 revision adds four native effects, a built-in demo library, version 1 migration
and version 2 persistent effect/demo settings. New effect decay, demo completion and
maximum-effects stress tests pass on host; physical timing remains unverified.
