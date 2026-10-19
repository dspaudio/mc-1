# Dual-core audio implementation

This document records the initial **two-bank candidate**. The later four-bank
combined synthesis/FX worker, Analog variance and updated demos are described in
[FOUR_BANKS.md](FOUR_BANKS.md). Its current build/verification record supersedes
the sizes, worker payload and scheduling below. The historical two-bank build
was hardware-unverified at the time of this record. Authorized October 19, 2026
device trials of later builds are recorded separately in [VALIDATION.md](VALIDATION.md).

## Historical prototype status and scope

This is an **opt-in, hardware-unverified** implementation, based on MC-1 commit
`869ed1265000075d0a52011d379e3da8060fcd70`. `MC_DUAL_CORE=0` remains the
build default until an explicitly approved device comparison establishes an
advantage. Eight shared melodic voices, six drum voices, monophonic Acid parts,
all existing FX, the sequencer and project format 7 are unchanged.

There are no new effects, voice shedding, automatic quality changes, partition,
bootloader, recovery-record, PLL, CPU-frequency or voltage changes. Enabling
CPU1 necessarily uses its existing enable/hold controls and temporarily sets
the vendor CPU1 startup gate in SYS_DIV (`0x10008`), then restores the exact saved value.
It does not select a new clock rate. No hardware installation was performed.

## Pinned reference analysis

The source and both release descriptions were inspected, not just current main:

| Reference | Revision |
|---|---|
| [SLOOP 2.7](https://github.com/isod89/sloop-fm1/releases/tag/v2.7) | `ba54ba5f0e7f6e0a1a3499497809e1e6c1ad7e8c` |
| [SLOOP 2.7.1](https://github.com/isod89/sloop-fm1/tree/v2.7.1) | `4ec1ac829fd2bdf2c571ea215fb3d62477d01220` |
| [2.7.1 release](https://github.com/isod89/sloop-fm1/releases/tag/v2.7.1) | Published October 16, 2026 |
| [Official AC79 SDK](https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK/tree/d179b4484759423312073f5fbb232501aa491047) | `d179b4484759423312073f5fbb232501aa491047` |

SLOOP `firmware/hal/fm1_cpu1.{h,S}` starts CPU1 before audio/timers. It disables
the CPU1 IRQ bank, writes the entry at `0x01C7FFF8`, enables/releases C1_CON,
temporarily opens the PC protection window for the ROM trampoline, and waits
up to 20 ms for an alive flag. CPU1 gets separate user/supervisor stacks and
returns from its startup interrupt into a RAM waiting loop. `src/cpu1.c`
performs a 64-step LCG self-test through XIP before accepting audio work.

Its single outstanding function/argument job uses monotonically increasing
request/done words with `csync` publication. `src/fx.c:mix_block` sends a mask
of complete synth parts and the drum/metronome unit to CPU1. The default split
is the last synth part plus drums. CPU0 renders the other units into its own
accumulator/scratch set while CPU1 renders into a second set; sums are merged
before shared buses. Units whose random-state dependencies could reorder
draws remain on CPU0 (LFO wrap, certain PHYS changes, multiple FORMANT parts).
This is real overlap, not dispatch followed immediately by waiting.

SLOOP waits up to **8 ms after reaching the join**, longer than MC-1's whole
2.902 ms half deadline. On timeout it holds CPU1 and re-renders its units on
CPU0. The partially advanced voice/DSP state is not rolled back; its source
explicitly accepts a transition glitch. It then stays on one core until boot.
CPU1 busy/wait ticks and stack unused space are available in its console.

The 2.7.1 change is specifically the hold sequence in `hal/fm1_sys.h`:
temporarily mask bus-invalid debug traps, set hold bit 1, clear enable bit 3,
execute `csync`, allow 256 bounded iterations for delayed flags, clear
`DBG_MSG` bits 4/5/16..21, then restore the previous trap mask and lock state.
The old hold could produce a delayed invalid-bus exception on CPU0, reported
as a crash/reboot instead of falling back. 2.7.1 also parks CPU1 before its
fatal-error panel/reset paths. It does not make a missed deadline glitch-free.

Adopted: startup/stack protocol, bounded boot/self-test, `csync` ownership,
RAM idle/acknowledgement, and the 2.7.1 safe hold sequence. MC-1 uses an original
fixed payload and does **not** copy SLOOP's track/global shared-state structure.

Not adopted:

- SLOOP engine-specific `engine.poly`/`phys_poly` limits and `voice.c` shedding
  after two consecutive audio halves exceed 85% load. MC-1 retains all eight
  melodic voices even after fallback.
- Its 8 ms join timeout and in-place, partially advanced-state retry.
- Its `fm1_supplies_stock()` call (`src/main.c`), which selects VDC14 1.60 V
  and SYSVDD 1.38 V. MC-1 inherits existing settings without writing them.
- Its claimed approximate doubling of capacity: that is not an MC-1 result.

Imported/adapted HAL code remains GPL-3.0-only with Charles Vestal,
Kerem Kilic lineage, Leo Kuroshita/Hügelton Instruments and Andre Rezemini
attribution. See [NOTICE](../NOTICE.md). No SDK binary or new DSP is imported.

## Choice of parallel work

| Candidate | Decision |
|---|---|
| Independent R/D and C/F banks | Selected. Inputs are already independent; order inside each bank and final integer mix stays exact. |
| Voice/part synthesis | Deferred. Shared allocation, sequencer/MIDI events, randomness, Acid glide and per-frame note transitions require a larger ownership change. |
| Block/slice pipeline | Deferred. More overlap with synthesis is possible, but changes parameter snapshots, buffering and failure recovery; an extra block of latency is not justified without measurements. |

Each existing 1..16-frame slice runs as follows:

1. CPU0 advances the existing sequencer and synthesizes all eight voices and
   six drums, then builds both send inputs.
2. CPU0 copies C/F input into the worker's fixed 16-frame buffer and publishes
   a job number. CPU1 executes the original C/F kernel on its private state.
3. **Meanwhile CPU0 executes R/D.** This is the overlap interval.
4. CPU0 joins, commits only the completed C/F changes, and performs the
   unchanged integer dry/wet mix, limiter and output.

The ordinary `mc_render()` still selects the sequential reference route.
`mc_render_ex()` takes an optional context containing a worker and/or a
profiling clock. The engine itself remains 77,168 bytes and contains no
thread/firmware state. Host callers can compare both paths on identical input.

### Ownership and failure invariants

`src/parallel.h:McDual` is a single, statically allocated worker slot:
aligned 32-bit request/done words, sequence, immutable frame/type parameters,
private `McTrackFx`, fixed PCM and completion result. No engine, project,
UI, MIDI or CPU0-stack pointer is handed to CPU1.

CPU0 snapshots C/F history **once per render call** (4,160 bytes with the
current layout), covering demo/project resets. The steady-state commit copies
at most 32 changed Int16 chorus entries (64 bytes) plus phase/cursor. A type
change commits the whole cleared C/F state once. The optimized commit relies
on the current C/F kernel modifying only chorus history, phase and cursor;
the full-engine differential tests must accompany any future kernel change.

CPU0 owns payload preparation until release-publication of request. CPU1 owns
payload mutation until it returns from the XIP kernel into the RAM loop and
release-publishes done. CPU0's acquire observes that result before committing.
Job zero is skipped on sequence wrap. Mismatched completion/result, invalid
frames/type, outstanding work and a stopped timer have bounded failure paths.

Join is bounded by 12,000 TIMER4 ticks (500 us **from dispatch**) and 100,000
poll iterations, whichever fires first. These are conservative fault bounds,
not device-validated scheduling budgets; a failure can exceed the current
audio deadline. A normal job that is already complete at the join is accepted;
whole-ISR counters still report any overrun.

On failure CPU1 is held using the safe-hold sequence, its slot is quarantined
until reboot, and CPU0 runs the unchanged `mc_fx_cf_block()` once from the last
committed canonical state and original input. It does not consume partial
output, double-advance FX history, remove notes or bypass FX. Even if a hold
readback fails, the abandoned slot never aliases the engine. Such a failure
also blocks flash access/update entry rather than turning flash off under an
unconfirmed worker. The panel reports `CPU1 FAILED - 1 CORE`, or
`CPU1 FAILED - NO SAVE` when hold could not be confirmed.

Register readback and software tests are not a silicon bus-drain measurement.
Real delayed-bus/debug-flag behavior and the inherited supply settings remain
hardware acceptance items. No runtime re-enable is offered after a fault.

### MIDI, UI, stop, save and update

Foreground engine changes already exclude the CPU0 audio ISR. That alone is
not a dual-core lock: safety here follows because CPU1 never accesses the
engine, and every ISR joins/quarantines before returning to foreground.
TIMER5 continues the existing USB-consumer-only path while `in_audio` is set.
MIDI note/parameter changes enter before rendering; per-slice type snapshots
preserve existing sequencer automation semantics.

STOP, preset changes and project/demo reload retain existing behavior and
are reconciled by the next render-call snapshot. `audio_pause()` verifies idle
ownership. While flash routines run, an acknowledged worker polls only RAM
and cannot fetch XIP without a new request. Save and update do not publish
requests; CPU1 is held before update commit, UBOOT entry and fault reset.
The update loader, flash ranges and recovery algorithm are unchanged.

## WL82 synchronization and memory basis

The official SDK's `include_lib/driver/cpu/wl82/asm/cpu.h` declares two cores,
maps `NO_CACHE_ADDR` only for addresses at/above `0x04000000`, and implements
`arch_spin_lock/unlock` using testset plus `csync` before/after publication.
`include_lib/system/spinlock.h` distinguishes local IRQ exclusion from
cross-core locking. `asm/cache.h` declares cache maintenance functions; it
does not license treating arbitrary cached SDRAM like internal SRAM.

MC-1 keeps the slot/stacks in internal SRAM `.bss` (`0x01C08000..0x01C20000`),
not XIP/SDRAM/DMA memory. Target acquire/release use aligned single-writer
word accesses plus `csync` **and compiler memory clobbers**, following the
SDK/SLOOP SRAM protocol. Host builds use actual C11 acquire/release atomics.
`volatile` alone is not the synchronization argument. No cache mode/way or
flush configuration is changed. Moving this payload to SDRAM later requires
a new cache/coherence design.

The idle loop is a separate `.c1_text` load image copied into the existing
RAMTEXT region, adjacent to the unchanged flash-off `.ram_text`. PC limits
include it. The builder still rejects calls from flash-off `.ram_text`.
RAM-to-XIP calls are indirect because the pi32v2 near-call range cannot span
these sections. The emitted 56-byte idle loop was inspected: after the
indirect worker return it executes `csync`, stores done, then polls SRAM.

CPU1 reserves 4,096 bytes user stack, 512 bytes supervisor stack and three
32-byte guard bands in `.bss`; it does not steal CPU0 stack/reserved-top space.
Its unused-stack scan and guards are exported with diagnostics. CPU0's
existing stack guards remain. Static allocation does not establish stack
high-water under interrupt nesting; measure both cores on the device.

Both cores still fetch DSP from shared XIP and use shared SRAM/cache/buses.
Snapshot/commit traffic, dispatch barriers, instruction contention and waits
can consume the saved C/F time. No speedup is inferred from the split.

## Build and measurement

Use the existing toolchain/SDK setup from [BUILDING](../BUILDING.md).

```sh
MC_DUAL_CORE=0 MC_RENDER_PROFILE=0 python3 tools/build_target.py
MC_DUAL_CORE=1 MC_RENDER_PROFILE=0 python3 tools/build_target.py
MC_DUAL_CORE=0 MC_RENDER_PROFILE=1 MC_PERF_PROBE=1 python3 tools/build_package.py
MC_DUAL_CORE=1 MC_RENDER_PROFILE=1 MC_PERF_PROBE=1 python3 tools/build_package.py
```

Each build report includes source commit, dirty flag, a SHA256 of all C/H/S/ld
inputs under `src`/`firmware` plus `tools/build_target.py`, flags, compiler and
app hash. Keep the report with its binary before building another configuration.
These commands build locally; they do not install. Do not publish the candidate
as the unchanged 1.0.2 release or replace `FIRMWARE.json`.

`MC_RENDER_PROFILE=1` measures synthesis, routing, CPU0 R/D, C/F worker or
fallback time, join wait, transfer/commit, output, other ISR work and a
pre-aggregation ISR total. C/F worker time overlaps R/D, so **do not sum them
as wall time**. IRQ preemption is included in elapsed CPU0 stages. Whole-ISR,
deadline, DMA-half and USB counters remain in the existing v1 capture.

With `MC_PERF_PROBE=1`, stage sums/maxima freeze over exactly the same 1,024
warm-up and 8,192 measured blocks as v1. A startup minimum of 32 back-to-back
TIMER4 read pairs records a lower bound on timestamp cost. Per-stage
bookkeeping, calls and the final aggregation cost more than that bound; the
existing whole-ISR capture includes the aggregation. Compare probe-off and
profile-on configurations on hardware rather than subtracting a guessed cost.

After an approved device install and a normal `tools/perf_capture.py capture`,
read the same frozen session, without starting another capture:

```sh
python3 tools/stage_capture.py --port PORT --capture capture.json --timeout 5 \
  --source-commit COMMIT --firmware-sha256 PACKAGE_SHA256 \
  --build-settings 'MC_DUAL_CORE=1 MC_RENDER_PROFILE=1 MC_PERF_PROBE=1' \
  --output stages.json
```

The six version-4 pages are read-only and nonce-bound. Partial windows, stale
sessions, pending capture and OTA invalidation are rejected. The existing v1
capture's frozen-gain/register exclusions are still required; the stage tool
does not turn an excluded capture into a valid paired comparison.

Required device comparison (not performed):

- Same 8-melody/6-drum full-send load, all kits, 0/1/2 Acid, separate 3-Acid
  mono, all 64 FX pairs, long releases/tails and live glide.
- Actual MIDI input, parameter/preset changes, UI refresh, USB Audio,
  repeated cold start/STOP/PLAY, save and update-entry/return.
- CPU1 no-start, late/partial/no-response and wrong-number fault scenarios,
  including safe hold, bounded return and transient glitch/USB effects.
- Full ISR and both stage timelines, waiting/transfer, deadline misses,
  DMA-half changes, USB underruns/overruns, CPU0/CPU1 stack and guard safety.
- Exact source fingerprint, flags, firmware/package hashes, gain/clock/cache
  conditions, raw captures and exclusions for every comparison.

## Historical evidence reconciliation

The raw evidence exists locally in the **parent workspace's** `.omo/evidence`,
not in the public Git tree or this repository's own `.omo` directory.
`v1.0.1/hardware-summary.json` identifies source `3f0fb2e`, package
`a09e763e6000f25dec63eb5feeb6251389dce33bf5abe2eb4746f1f1c7091ab5`,
30 x 8,192 blocks, zero deadline/DMA-half/USB underruns and 67,687 peak full
audio ticks. The three audio files `engine.c`, `effects.c`, `app.c` at that
source were identical to this task's starting HEAD. Its approximately 2.816%
observed headroom applies to that image/workload, not this new implementation.

The October 16 failure note is also real:
`perf30-20261015/device/shared-stress-1/v8-d1-rd5-cf5-f0-k6-1.json`
uses a **different** package,
`c594027789d7c2f190a79cbd7268255d73dd51949e2d315ecca9bbfc829b3d20`.
It records eight voices, drums, PLATE/FLANGER, kit 6, render peak 80,603 ticks,
full-audio peak 81,243 and 8,192 deadline/half changes. Master changes 818 to
843, so it is not frozen-valid paired speedup evidence. That raw JSON does not
identify a source commit; do not invent one. It predates the later accepted
voice-preparation candidate rather than contradicting the later package's
record. Neither historical result independently verifies this dual-core build.

## Available verification and remaining budget

See [VALIDATION](VALIDATION.md#dual-core-candidate-2026-10-19) for executed
checks and build sizes. Native two-thread tests compare PCM and the entire
77,168-byte engine after each block, including partial-job fallback, across
1,303,670 frames. Existing DSP hash, USB publication, MIDI, storage and OTA
regressions remain enabled. The stage protocol has separate C and Python tests.

**Additional FX budget measured on this implementation: none yet.** The
available margin is unknown until the approved paired device test. More FX or
better quality is a separate change only after that result. If a future FX
cannot meet the original single-core deadline, it must have an explicit
user-visible fallback policy before release; silently dropping voices or FX
is not an acceptable fallback. The current implementation falls back to all
existing voices and effects and can still glitch during a real deadline miss.
