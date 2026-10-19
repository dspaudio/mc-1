# Four-bank FX and optional Analog variance

## Version 1.1.0 scope

This release extends the earlier two-bank dual-core implementation. Eight
shared melodic voices, six drum voices, monophonic Acid, all existing algorithms,
sample rate, bootloader, partitions, recovery, CPU frequency and voltage remain
unchanged. Authorized device trials are recorded separately from host proofs.

The four independent parallel send banks are **R/D1, C/F1, R/D2, C/F2**.
Each R/D bank selects one existing reverb or delay algorithm; each C/F bank
selects one existing chorus/flanger/short-delay algorithm. This is two instances
of each existing family, not a new distortion or four serial inserts. Each part
has four independent send amounts. Existing two-bank projects retain their
first two bank settings; new sends and Analog amounts import as zero.

`MC_DUAL_CORE=1` is the 1.1.0 target default, locked to eight melodic / three
worker voices. The published package is byte-identical to the restored,
hardware-tested eight-voice candidate. A successful build or matching host
render alone does not establish device FX headroom. See
[VALIDATION.md](VALIDATION.md) for the authorized hardware trials; the original
1.0.2 single-core asset remains separately selectable.

## CPU ownership and actual overlap

CPU0 owns sequencing, MIDI, input routing, all gate/envelope control, five melodic
DSP voices, mixing, the limiter, USB/DMA handoff and canonical engine state.
CPU1 renders the remaining three melodic voices, all six drums and all four FX
banks using copied private state. These are still eight melodic voices total.
No engine, project or CPU0 stack pointer is published to CPU1.

Each render call drains all its FX jobs before returning:

```text
CPU0: control/melody(0) -> join/route(0) -> control/melody(1) -> ...
CPU1: synth(0) -------------------------> FX(0)+synth(1) ----> ...
       ... -> FX(last), then the render call returns
```

Two fixed render slices contain at most 16 frames each. A combined worker job
processes the previous slice's FX and the current slice's private synthesis
while CPU0 renders its current voices. Their frame counts can differ. The next
job is published only after joining and consuming the current result. The last
FX job drains in the same call, with no added output block latency. A one-slice
call can overlap synthesis too, but still pays dispatch and FX drain overhead.

First-frame control precedes the synthesis snapshot. Slices do not cross
sequencer events or 64-frame demo controls; stopped live arpeggiators keep their
one-frame slices. CPU0 mirrors the worker voices' gate/envelope progression in
the original voice order, so MIDI messages, queue overflow and panic behavior
remain unchanged. The worker performs the same envelope math without MIDI I/O.
Raw mono voice sums are combined before part-level/pan truncation. The drum
PRNG advances in the original frame-then-slot order.

Every slice freezes input buses, algorithm types, send activation, tempo and
diagnostic bypass settings before lookahead sequencing can change them.
Limiter/output state is not advanced by synthesis. Within-bank arithmetic and
final mix order are preserved. With new sends zero and Analog off, the old
two-bank mix has the same arithmetic; four simultaneous wet signals can engage
the existing limiter more often, rather than silently reducing FX or voices.

Private histories and payloads use internal SRAM `.pool`, not SDRAM or XIP.
The earlier 73,888-byte full FX snapshot per call is eliminated. Before each
job CPU0 copies only the comb/delay taps and chorus interpolation taps that
the worker will read, plus small cursor/filter metadata. Normal completion
commits at most 384 bytes of changed history per 16 frames plus metadata.
Type changes retain the kernels' whole-family clear. Unprepared private taps
may be stale and must never be read or committed.

The synthesis payload copies the selected voice, PCM-cache, pitch-cache,
Analog, drum and PRNG state. Successful join exposes an immutable result view
until the next submission, avoiding a redundant whole-payload copy. CPU0
consumes that view and commits canonical state before reusing the slot.
Copies and SRAM/XIP contention remain measured costs. Future kernel changes
must update required reads, dirty commits and full-state comparisons together.

Aligned single-writer request/done words, target `csync` plus compiler memory
clobbers, and host C11 acquire/release preserve the earlier publication protocol.
CPU1 returns to its RAM loop before publishing completion. Local IRQ exclusion
is not used as a cross-core ownership argument.

On startup failure, timeout, invalid job/result or unconfirmed stop, the worker
slot is quarantined until reboot. CPU0 recomputes failed FX and synthesis from
preserved inputs and last committed state, without replaying MIDI control.
No failed result view is exposed or consumed. Existing safe-hold, flash-safety checks and visible
`CPU1 FAILED - 1 CORE` / `CPU1 FAILED - NO SAVE` policy remain. Neither this
fallback nor four-bank single-core operation is guaranteed to meet the audio
deadline until measured. There is no automatic voice shedding or FX bypass.

## Controls and compatibility

FX SENDS and FX TYPES use all four parameter knobs in bank order. ENV switches
between the selected part's sends and global types. Amounts remain 0..127 and
types remain 0..7. New send/type automation IDs are appended; existing IDs retain
their numeric values and existing global automation remains canonical.

Version 8 appends ten bytes for banks 3/4 and three melodic Analog amounts after
the unchanged 35,104-byte version-7 payload. The payload is 35,117 bytes and the
CRC-framed record is 35,137 bytes, fitting the unchanged 36 KiB storage bank.
Versions 1..7 remain importable; older firmware rejects version 8. Flash addresses,
nine-sector A/B banks and commit-last recovery are unchanged.

Existing MIDI mappings remain R/D1 CC91 (CC94 input alias), C/F1 CC93 and types
CC20/23. R/D2 and C/F2 use CC92/95 for sends and CC21/24 for types. The latter
were previously inactive legacy controls; external mappings should be checked.
Analog has no new MIDI CC mapping; it uses the existing parameter/automation API.

## Display transfer budget

The same RGB565 renderer now submits only changed x ranges within each eight-row
strip. Unchanged strips do not start DMA. A 115,200-byte last-submitted image
cache lives in SRAM `.pool`; the original single live DMA strip buffer and
50 ms/transport-urgent schedule remain. The RGB565 format and byte order are
unchanged. Cache flags reset on panel restart.

Rectangle fills clip once before writing row spans. All thirty strips share
one 32-byte transport snapshot and a copied/prepared UI, preventing different
parts of a frame from showing different steps. Playback is a short GREEN
underline following the audio step accumulator (or received external MIDI ticks),
not a recolored note box. Only populated tracks/drum rows have underlines;
rest steps within a populated track still show progress. Mute and STOP hide
the corresponding underline, and WHITE edit markers remain separate.
Small text is white 6x9 with a seven-pixel advance; large titles remain 10x14.
All sixteen drum rows remain available at a ten-pixel pitch without scrolling.
The real C-renderer matrix covers 120 states across every page and selected part.

All twelve physical menu/transport buttons and both OCT buttons are lit at
rest. Selected menus/modes blink at 2 Hz; running PLAY uses quarter-note
sequencer phase instead, and stopped PLAY returns to steady green.
HOME stays steadily lit during playback; it blinks only when selected while stopped.
Recording REC blinks independently. Button writes leave held-note/step LEDs untouched
and keep the PLAY red die off. See [DESIGN.md](DESIGN.md) for the display contract.

The current profiled dual build uses 305,576 bytes of the 335,872-byte usable
pool, leaving 30,296 bytes after the existing stack reservation. The matching
single build uses 229,488 bytes. These are build measurements, not proof of
hardware frame rate. Optional diagnostic schema 7 reads completed frames,
maximum completion gap, submitted rectangles, skipped strips, current y and
last completion time. Collector frame-rate estimates include warmup/query
time and are separate from the frozen 8,192-block audio window.

## Feasible subset of the Serum video

The referenced Undulae Music Serum 2 demonstration is
`https://youtu.be/EWrwVgzyu6U`. Direct video inspection found GLOBAL > VOICE CONTROL
at 1:25, noise-driven oscillator FM at 3:45, an S&H LFO targeting Main Tuning at
5:20, and Tube distortion at 7:30. The video demonstrates several techniques,
not one universal Warmth processor.

MC-1 implements an original, smaller **Analog variance** feature: independent
note-on tuning offsets and slowly changing smoothed random tuning per voice.
This runs with each voice's DSP owner, not in a shared FX bank. It does not reproduce
Serum's full sound, per-voice pan/cutoff/envelope variance, Noise FM, audio-rate
modulation or Tube model.

GLO cycles from MIDI to ANALOG VARIANCE. The first parameter knob sets the
selected melodic part's amount; zero is off and the other three knobs are
inactive. The drum part cannot enable it. PLAY + REC records amount automation.
Factory projects and imported older projects default to zero.

At full amount, the fixed Q16 frequency-ratio offset is at most about eight
cents and the drift target at most about 3.4 cents; their worst-case sum is
about 11.5 cents. These are small linear ratio offsets, not a new temperament.
An independent per-voice integer PRNG chooses targets every 16,384 active
samples; a 64-sample control update smooths toward them by 1/64 of the distance.
Existing drum and arpeggiator random sequences are unchanged. Acid legato retains the same
voice variance state. Existing base pitch caching is retained; optional tuning
is applied afterward using bounded 32-bit split products.

The disabled branch does not advance Analog randomness. Live amount changes
do not retrigger envelopes or reduce polyphony. This is deliberately not a
claim of identical analog hardware, perfectly continuous control or measured
hardware CPU cost.

## Verification procedure

Run `make test render` and clean separate sanitizer binaries, then build both
`MC_DUAL_CORE=0` and `1`, with profiling off/on. The four-bank stage protocol
is schema 5, seven pages / 42 words; use the matching `tools/stage_capture.py`.
It separates CPU0 synthesis, routing, all four bank times, wait, transfer and output.
Worker synthesis is part of the combined job, not an additional CPU0 stage.
Times on different cores overlap; summing all stage times is not the whole-ISR
elapsed time. Use the separately measured whole-ISR counters for deadlines.
The older schema-4 two-bank candidate needs its own older matching collector.
`MC_CPU1_TRACE=1` optionally retains diagnostic heartbeat, progress and peak-job
markers. Its default is zero and it never changes the 500-microsecond job bound.

Retain the original 8+6 two-bank stress condition and add four-bank and Analog
conditions explicitly. Record source fingerprint, settings, firmware hash,
whole ISR time, per-core FX time, join/transfer costs, deadline misses,
DMA-half changes, USB underruns, stack guards/high-water and bus/cache behavior.
Do not infer four-bank capacity from two-bank measurements or host CPU time.

The October 19, 2026 device installation and measurements were authorized.
Hardware failures and rejected candidates remain part of the validation record;
host equivalence does not turn them into passing trials.

## Updated demo arrangement and exports

| Demo | Second melodic part | Reverb + delay on that part | Third melodic part |
|---|---|---|---|
| NIGHT CIRCUIT | Analog 52 | HALL1 send 52 + PANNING DLY send 38 | PIANO STEPS, PCM mix 108/127 |
| NEON SPIRAL | Analog 60 | PLATE send 40 + DELAY send 44 | MARIMBA RUN, PCM mix 108/127 |

All four banks are used with restrained per-part sends. Analog on the third
part is only 8/6, preserving the sampled attack. NEON SPIRAL exposes the PCM
hook during its intro; cutoff automation keeps that part open enough to retain
its transient. Neither demo automation turns off part 2's Analog/reverb/delay.
Existing old-preset PCM and reconstructed 0.1.2 demo snapshots remain covered
independently of these intentional composition changes.

`tools/render_demos.c` renders the actual shared engine for 32 bars plus five
seconds of release/FX tail. `build/demos/` holds the WAVs and 256 kbps stereo
MP3 exports. Both MP3s were decoded back to non-silent 44.1 kHz stereo PCM with
the exact source frame count; their SHA256 and lengths are recorded in
[four-bank-validation.json](four-bank-validation.json). There is no external
normalization or mastering. Digital checks do not establish subjective listening
quality or actual hardware stability.
