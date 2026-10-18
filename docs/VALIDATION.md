# MC-1 validation record

## 1.0.2 official V15 application restore (2026-10-18)

The installer accepts the user's original V15 file only at 699,956 bytes and
SHA256 `db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a`.
It checks the UFW header/list and official flash image, then preserves all
`0x93000` bytes of official `flash.bin` while retaining the SHA-pinned MC-1
small RAM loader. The generic package validator is unchanged. The stock
package's separate USR entry is not installed; this is application restoration,
not factory reset, user-data recovery or saved-project compatibility.

The 1.0.2 release package is byte-identical to the pinned 1.0.1 diagnostic
package: 610,089 bytes, SHA256
`a09e763e6000f25dec63eb5feeb6251389dce33bf5abe2eb4746f1f1c7091ab5`.
The firmware staging code, flash allowlists, loader and audio engine are unchanged.
The version, web installer and Pages metadata identify this installer release.
The device's `FM-1_900` protocol identity does not identify a release version.

Checks completed:

- `make test render`: all 25 C executables and 47 Python tests pass; the shared-C
  render has 705,600 frames, peak 26,649 and zero clips.
- A fresh `make -B test` with Homebrew LLVM ASan/UBSan passes. Separate sanitized
  OTA and loader fault-injection runs also pass using the actual prepared V15
  image. Native loader simulation passes in both MC-1-to-V15 and V15-to-MC-1
  directions, including head/data sentinels and interrupted-transfer cases.
- pi32v2 compilation, flash-off code guards, SDK hashes, LZ4 round trips and
  package construction pass. The diagnostic app is 507,420 bytes, globals
  50,424 bytes, pool 77,168 bytes and RAM text 5,568 bytes.
- The browser codec passes synthetic boundary tests and a real-file integration:
  official flash bytes and MC-1 loader bytes are unchanged, and prepared package
  SHA256 is `8b7ac1f7e84467fba38ec695b51d1b633f6c880a83914577c7f2025f48e9934e`.
  This private package is not distributed in Pages or the source repository.
- Existing 32 protocol scenarios and 25 installer UI scenarios pass, including
  invalid file rejection, acknowledgement, asynchronous file-selection races,
  transfer locks, progress, unload protection and loader retry. Pages assembly
  checks the pinned release, resource hashes and rejected manifest inputs.
- Final-source visual captures cover both pages at 390, 1440 and 1920 CSS pixels,
  both system themes, top/end positions and file rejection/verification: 36
  original PNGs, all with correct dimensions and no horizontal overflow.
  Twenty additional component-state images cover rest, hover, pressed, actual
  keyboard Tab focus and CSS 200% zoom. These state-harness checks do not access
  MIDI or install firmware. Body/secondary contrast is 10.36/6.34:1; control
  borders are 3.46:1 and focus outlines 8.96:1.
  Both independent visual reviewers pass the final implementation. The initial
  missing hover/pressed feedback was fixed using existing tokens and rechecked;
  the final 36-frame set and 20 state frames contain no blocking findings.

C diagnostics and actual warnings-as-errors builds are clean. JavaScript and
Python/YAML language servers are unavailable in this workspace; Node syntax,
runtime regressions and actual Pages/hardware checks provide the executed
evidence rather than a claim of language-server coverage.

**Hardware:** a normal MIDI restore from the running MC-1 completed and
reidentified as `FM-1_015`. The user confirmed that V15's screen and sound were
normal. Then the actual local Pages installer in real Chrome performed:

1. V15 to MC-1, verified as `FM-1_900`.
2. MC-1 to V15 with the page deliberately reloaded at 5% of the write stage.
3. Reconnection to `ota-FM-1_900`, selection of the same official file, resumed
   restoration, and verified restart as `FM-1_015`.
4. V15 to MC-1 again, verified as `FM-1_900`. An independent desktop identity
   query also confirms the final running MC-1.

All transfers use the normal M-UPGRADE path, not UBOOT. The interruption removes
the browser host only; no power cable was disconnected. Electrical power-loss
recovery, all factory patches, and stored-data retention remain unverified.
The user subsequently reported that operation appeared normal.
Evidence is in ignored `build/qa102/hardware-web.json`, the adjacent actual-page
captures, `build/v102-restore-hardware.log` and `build/v102-sanitizers.log`.
No firmware or site was published by this validation.

## 1.0.1 candidate (2026-10-16)

The firmware branch is `fix/1.0.1`; the sibling web emulator is not changed.
The six shared reverb returns have approximately 6.02 dB more gain. HALL1,
HALL2 and PLATE use higher existing feedback coefficients, with no added
buffer or DSP stage. Independent impulse checks measure HALL2's last nonzero
sample at 1,110.6 ms versus 786.9 ms before the change. This is a quantization
floor measurement, not RT60 or listening approval. Delay, C/F and legacy DSP
reference hashes remain unchanged.

New projects use ROOM2 sends 12 / 72 / 24 / 8 and no C/F send; stored projects
retain their own values. Both built-in demos use reverb and chorus, with PART2
R/D send 112. Lockstep wet/dry renders prove real PCM differences, both banks
active, and zero clips. GLOBAL MIXER uses ENV to switch LEVEL / PAN, with four
independent controls, range 0..127, center 64, and existing pan automation and
v7 persistence. The lower queued NEXT is removed; the upper destination stays.
PLAY ID 12 is routed to the separate PA5/column 8 green coordinate documented
by the existing FM-1 platform reference, with its red die off and REC separate.

Checks completed:

- Native regression: all 25 C executables and 47 Python capture tests pass.
- Clean separate ASan/UBSan builds: all 25 C executables pass. The final
  cosmetic PAN hint relocation was followed by fresh mixer, UI input, pattern
  title and ENV graph regressions and a new target/package build.
- Actual shared-C audio: 129 renders pass non-silence, saturation, DC and release
  checks; seven peak-normalized listening examples and 320 kbps MP3 regenerate.
- Final source UI: 20 fresh desktop/mobile light/dark captures cover HOME, ENV,
  DRUMS, LEVEL and PAN. Original dimensions are 1440x900 or 1170x2532
  (390x844 at DPR 3); no horizontal overflow. Main inspected every distinct
  frame and adjudicated two independent visual reports. An initial overlapping
  ENV hint was corrected before these captures.
- pi32v2, flash-off code checks, package CRC/loader simulation, 32 OTA protocol
  vectors, 15 installer UI scenarios and local Pages assembly all pass.

| Image | App | Globals | Pool | RAM text |
|---|---:|---:|---:|---:|
| Non-probe, CI reports | 505,004 B | 50,052 B | 77,168 B | 5,568 B |
| Diagnostic candidate | 507,420 B | 50,424 B | 77,168 B | 5,568 B |

Both images satisfy the documented flash/RAM ceilings. Link-section sizes are
not stack high-water measurements. App/HAL stays `-Os`, DSP stays `-O2`;
eight melodic and six drum voices and both eight-type banks are retained.

Pinned diagnostic package: 610,089 B, SHA256
`a09e763e6000f25dec63eb5feeb6251389dce33bf5abe2eb4746f1f1c7091ab5`.
Non-probe package SHA256:
`68e813a43bc22c5a63519c2e6e93c7509f758bad9747b57879fec68f0d8fe862`.
These are different images and are not interchangeable measurement evidence.

**Hardware status:** after the user's explicit installation request, the pinned
1.0.1 diagnostic package above completed OTA, reboot and `FM-1_900`
re-identification with exit 0. A separate final connection also identified the
running application. This is a known-package transfer and boot check, not a
flash readback hash. Source and the existing `v1.0.1` tag remain at `3f0fb2e`.

Actual device collection completed 30 captures, each with 1,024 warm-up blocks
followed by 8,192 measured 128-frame blocks: **245,760 measured blocks**.
All completed records have zero deadline misses, DMA-half changes, USB
underruns, overruns, stalls and missed packets.

| Capture group | Captures | Render peak, ticks | Audio-path peak, ticks |
|---|---:|---:|---:|
| 8 mixed LA + 6 drums, PLATE/FLANGER, repeat | 3 | 66,114 | 66,798 |
| All eight kits and three Acid/LA configurations | 11 | 66,113 | 66,805 |
| Seven additional full-load R/D-C/F paired types | 7 | 65,979 | 66,655 |
| FX-only PLATE/FLANGER | 3 | 17,589 | 18,120 |
| USB stress8, cold PLAY and STOP/PLAY | 2 | 66,630 | 67,687 |
| USB NIGHT CIRCUIT, cold PLAY and STOP/PLAY | 2 | 52,107 | 53,038 |
| USB NEON SPIRAL, cold PLAY and STOP/PLAY | 2 | 35,600 | 36,373 |

Stress fixtures retain eight melodic voices plus six drum voices, both banks
and four-part sends 127, with diagnostic skip mask 0. The Acid configurations
are 1 Acid + 7 LA, 2 Acid + 6 LA, and all three monophonic Acid parts. Paired
types 0/0 through 7/7 cover every type in each bank; this does not claim a
full 64-combination matrix. FX-only peak 17,589 is below the existing
approximately 21,900-tick ceiling.

At the configured 24 MHz TIMER4 tick rate, maximum render time is 2,776.250 us
and maximum complete audio-path time is **2,820.292 us** against the firmware's
2,902 us threshold (69,648 ticks). Observed worst audio-path headroom is only
**81.708 us / 2.816%**. The deadline passes, but the aspirational 10% reserve
is not met; these finite captures do not establish an unlimited-load guarantee.
Fifteen records pass the unchanged frozen-condition validator. The other
fifteen have master ADC changes and remain excluded from fixed-condition
paired comparisons. No percentage speedup over 1.0.0 is claimed.

All six USB recordings contain nonzero stereo Int16 PCM at 44.1 kHz, each
1,367,100 frames. Their performance windows have streaming flags 7/7 and
zero underruns, including full stress cold PLAY and STOP/PLAY. Clock/power
registers match before, throughout and after testing. CACHE_CON reads 0x4302
before installation, 0x302 immediately after and 0x4302 at the final snapshot;
no fixed-cache comparison is assumed. No SAVE or volume/PLL/PMU write was
sent; the final RAM fixture is cleared to stopped demo0.

The first coverage watcher reached its 300-second host limit after eleven
completed source captures. Missing type/FX-only cases then completed under a
600-second watcher. No interrupted or partial capture is counted. Raw records,
sessions, record hashes, WAV hashes and exclusions are indexed by
`.omo/evidence/v1.0.1/hardware-summary.json`; the install and final hardware
snapshots are alongside it. Reused USB-script outputs are the
`device/usb-restart-shared-voiceprep4-*-101.*` files under the earlier
`perf30-20261015` evidence directory, with the new package hash in every JSON.
The user confirmed that PLAY is green and does not blink to the beat:
it is a steady playback indicator. Physical knob and other LED checks,
analog listening and electrical power-loss recovery remain unverified.

Before release publication, the user separately authorized MP3-only publication.
Commits `0209f56` and
`2cb3087` publish only the MP3 and its hash report. The final public MP3 SHA256
is `f5fc281a96aafd7d9c77db2c9694073dac09f8e9abf6ea97b3e1b4b29bb25555`;
Pages run 38884307249 succeeded and the public bytes match. The first replacement
also decoded successfully to stereo 44.1 kHz Int16 PCM. At that stage, public
page HTML was unchanged and still pinned firmware 1.0.0; the 1.0.1 firmware
and local Pages preview had not yet been published.

Evidence: `.omo/evidence/v1.0.1/`, including the preserved diagnostic candidate,
independent DSP/default/PAN reports, `visual-captures.json`, `visual-verdict.md`
and `audio-fx.md`. HTML/YAML LSP servers were unavailable; C files whose LSP
context lacks `-Isrc` report missing includes. Actual warnings-as-errors builds
and runtime checks above provide compilation evidence instead.

## Accepted performance candidate (2026-10-16)

Both repositories now identify the working tree as **1.0.0**. The default
1.0.0 diagnostic package in `candidate-v1.0.0-probe/` is byte-for-byte
identical to the installed and measured package below. Updating version
metadata therefore does not substitute an unmeasured DSP image.

The measured specification is **eight shared melodic voices plus six drum
voices**, with both shared FX banks active at full part sends. No FX type or
processing rate was removed, and there is no automatic quality reduction.

| Workload | Highest observed render ticks | Deadline / DMA-half changes |
|---|---:|---:|
| FX-only PLATE + FLANGER, frozen-valid repeats | 17,621 | 0 / 0 |
| Eight mixed PCM/dual-oscillator voices + six drums, all eight kits | 66,323 | 0 / 0 |
| One Acid + seven LA voices + six drums | 65,495 | 0 / 0 |
| Two Acid + six LA voices + six drums | 64,505 | 0 / 0 |
| Three Acid voices + six drums | 38,426 | 0 / 0 |
| Eight LA + six drums, actual USB Audio cold/restart | 66,718 | 0 / 0 |
| Required audio deadline | 69,648 | — |

FX-only cost clears the approximately 21,900-tick half-cost bound. All 21 final
captures contain 8,192 measured 128-frame blocks after 1,024 warm-up blocks.
The six USB captures contain actual nonzero stereo PCM and zero USB underruns,
including both demos and the eight-voice stress workload, cold and STOP/PLAY.
There are 11 frozen-valid records; ten with master ADC changes (818/843) are
preserved and excluded from formal paired improvement comparisons. The
highest frozen-valid eight-LA/six-drum peak is 66,321 ticks. The table retains
conservative peaks from all observations where indicated, not a paired speedup
claim. The previous 64-type FX matrix remains a structural routing baseline;
the final arithmetic changes preserve PCM, MIDI and complete engine state.

Installed package SHA256:
`313ebd7b4b9173247d1adc3e1586f87c711a02c5807bb28c5675401a799a68b0`.
Evidence is under `.omo/evidence/perf30-20261015/`: the package is in
`candidate-shared-voiceprep4-probe/`, raw sets in
`device/shared-voiceprep4-zero/` and `device/shared-voiceprep4-1/`, and six
USB WAV/JSON/log sets use `device/usb-restart-shared-voiceprep4-*`.
Clock/power snapshots are unchanged; no physical volume, SAVE or persistence
allowlist changes were used. Hardware was left stopped after capture.

Native/render, 24 ASan/UBSan executables, 11 Web regressions, pi32v2 memory,
flash-off and sanitized OTA/loader checks pass. A 17,614,272-frame comparison
matches the pre-optimization PCM, MIDI and all 77,168 engine-state bytes.
The default app/HAL/IRQ `-Os`, engine/effects `-O2` build reproduces the tested
app and package byte-for-byte. Electrical power-loss recovery remains outside
these runtime measurements.

After the version change, pinned Web core hashes and all 11 Web regressions
pass. Actual 1.0.0 emulator, host-panel and project-site screens were inspected
in light/dark modes at 1440×900 and true 390×844 mobile emulation. All 20
original PNGs have the expected dimensions (mobile DPR 3: 1170×2532), no
horizontal overflow, and visible 1.0.0 text. AudioWorklet startup succeeds in
all four emulator combinations. The evidence directory is
`.omo/evidence/perf30-20261015/v1-ui-qa-final/`; the earlier mis-sized trial
captures are not acceptance evidence. Working browser/server/profile resources
were closed after QA. JavaScript LSP was unavailable; runtime and regression
checks were used rather than installing new tooling.

## Shared-bank hardware baseline (2026-10-16)

The current routing has two global original FX banks, R/D and C/F, with eight
types each and independent sends on all four parts. Master output remains volume
and linked limiter only. Version 7 appends ten serialized bytes; versions 1–6
import through validated FX/automation migration without saving the source banks.

Native `make test render`, all 24 clean ASan/UBSan executables, 11 web regression
programs, pinned core hashes and real Chrome SENDS/TYPES screens passed. The
screens were inspected at 1440×900 and 390×844 with light/dark preferences.
The pi32v2 package, flash-off and sanitized OTA/loader checks also passed.

### Zero voices, both sends at 127

Diagnostic package
`2508b5804d6a7b15d66c2821ca97aded5d36359a3ef538c061256a2f08c1fa18`
was installed and reboot identity verified. All 64 R/D × C/F combinations were
captured with zero melodic/drum voices, full sends, no probe skips, 1,024 warm-up
blocks and 8,192 measured 128-frame blocks each.

| Observation | TIMER4 ticks |
|---|---:|
| Highest mean, PLATE + FLANGER | 18,853.2003 |
| Highest individual render block across all 64 captures | 18,924 |
| Requested half-cost bound | approximately 21,900 |
| Audio deadline misses / DMA-half changes across all captures | 0 / 0 |

The worst mean is approximately 785.55 microseconds and the observed peak is
788.5 microseconds. This clears the requested absolute FX-only cost bound.
The earlier approximately 43,810-tick reference suggests a 56.97% reduction from
the replaced routing, not a bit-identical wet-DSP optimization comparison.
There are 33 frozen-valid captures. The other 31 are retained but excluded from
formal comparisons because master start/end differed by one ADC step
(818/843). No gain, PLL or power override was used. CLK_CON0, PLL2_CON1 and both
power-selection register readings remained unchanged.

Raw records: `.omo/evidence/perf30-20261015/device/shared-fx-1/`.
Each record preserves the package/MIDI hashes, hardware snapshot and unchanged
42-word capture schema.

### Maximum-source workload

Eight melodic voices are a required target; reducing that limit is not an
accepted solution. Six-voice measurements are diagnostic only. The RAM-only
workload covers recorded PCM
mixed with dual saw oscillators, maximum resonance/sustain, high-register cache
refills, full part levels/sends, and six repeatedly triggered drum voices.
Companion modes add one/two Acid voices with continuous slide, or three Acid
voices. Constant pitch bend does not invalidate the pitch cache every sample.

Native tests verify the live voice/drum counts and ongoing Acid slide after
warm-up throughout the complete capture window. PCM metadata confirms the chosen
three looping sources cover the maximum high-register mip step. Hardware
maximum-load captures, rather than these host tests, must establish deadline
compliance at eight voices.

The initial full eight-voice mixed-PCM plus six-drum workload measured mean
79,705 / peak 80,603 ticks and missed all 8,192 deadlines. A 64-frame rendering
batch preserved exact PCM/MIDI/state over 15,302,592 frames but measured mean
79,390 / peak 80,287, still missing every deadline. That batch change was reverted.

The constant-arithmetic candidate replaces repeated oscillator-band division, signed power-of-two
drum divisions and final division by three with exact equivalent arithmetic.
It retains eight voices, all FX types and their processing rates. Native tests
match the pre-optimization PCM, MIDI and complete 77,168-byte runtime state over
17,614,272 frames, including all note bands, 128 kit/sound combinations and all
64 shared-FX combinations. Native/render, all 24 ASan/UBSan, 11 web regressions
and target/package checks pass. Installed package
`2f65ba2ce3094ee67392da09e3a87ced2b059fbe05564fd674db5664568936e4`
measured mean 78,227.8508 / peak 79,062 ticks with eight mixed voices and six
drums, missing all 8,192 deadlines. The capture is frozen-valid, but it still
fails the required eight-voice deadline. Raw record:
`.omo/evidence/perf30-20261015/device/shared-divopt-1/v8-d1-rd5-cf5-f0-k6-1.json`.

A separate candidate placed only the unchanged sine/saw/square tables (9,216
bytes) in initialized RAM. Its target symbols confirmed all three RAM addresses;
native/render, 17,614,272-frame exact comparison, Web hash/WASM and sanitized
OTA/loader checks passed. Installed package
`e38917a5dbc0e8c5a3ac1011c1bbe46b9fed1ad6ce9e5a7520f45bba6c078f2b`
measured mean 78,553.2668 / peak 79,370 ticks, missing all 8,192 deadlines in a
frozen-valid eight-voice/six-drum capture. No benefit was established, and the
RAM placement was reverted. Raw record:
`.omo/evidence/perf30-20261015/device/shared-osc-ram-1/v8-d1-rd5-cf5-f0-k6-1.json`.

The source-arithmetic candidate reuses one oscillator-table selection for both LA phases
and replaces bounded drum divisions with exact reciprocal products. Tests
exhaust the signed numerator ranges for all eight kits and verify the
reciprocal error bound. These are arithmetic changes only; all voice counts,
FX types, processing rates, clocks and physical master volume remain unchanged.
Native/render, all 24 ASan/UBSan executables, all 11 Web regressions and the
pi32v2/flash-off/package/OTA-loader checks passed. The original-reference
comparison again matched PCM, MIDI and all 77,168 runtime-state bytes over
17,614,272 frames. Package
`8ca0f410fb598477ad7008ad7f5d3bfd2359a436903b5de4b686e1781be57a0c`
contains a 506,204-byte app, 50,424 bytes of globals, 77,168 pool bytes and
5,568 RAM-text bytes. After verified OTA/reboot, the frozen-valid eight-voice
mixed-PCM/six-drum capture measured mean 76,990.4799 / peak 77,890 ticks and
missed all 8,192 deadlines. This remains a failed eight-voice real-time result,
despite exact native behavior. Raw record:
`.omo/evidence/perf30-20261015/device/shared-sourceopt-1/v8-d1-rd5-cf5-f0-k6-1.json`.
Separate probe-skip captures are diagnostic cost decomposition only and must
not be reported as optimized full-FX performance.

With that same source-arithmetic package, diagnostic skips measured:

| Diagnostic workload | Mean / peak TIMER4 ticks | Frozen validity |
|---|---:|---|
| C/F bypassed, mask 1 | 76,210.3185 / 77,152 | valid |
| R/D bypassed, mask 2 | 74,176.4615 / 75,228 | master changed; excluded from formal comparisons |
| Both banks bypassed, mask 3 | 73,416.7714 / 74,528 | valid |

All three still missed all 8,192 deadlines. They are not optimized audio results:
they isolate the bottleneck and show that removing both wet processors is
insufficient to meet the eight-voice deadline. Raw records remain under
`.omo/evidence/perf30-20261015/device/shared-sourceopt-components-1/`.
The function-boundary candidate changed only `melodic`/`percussion` inlining.
Native/render, 17,614,272-frame PCM/MIDI/full-state equality and offline package
checks passed. Installed package
`d18c3503b11877e14c075de855ab0204b5502ac8f64ff7d61128ceb8f99be716`
measured mean 76,944.9902 / peak 77,748, still missing all 8,192 deadlines;
master changed during capture, so no formal improvement is claimed.

The next candidate prepares invariant voice coefficients, oscillator-table
selection, recorded metadata and stationary LA pitch once per rendering slice,
and inlines recorded interpolation into the voice function. Runtime state is
not enlarged. Stopped ARP uses one-frame preparation because its notes may
change on any sample. The first candidate was not installed: differential
testing found a demo PCM mismatch in the block starting at frame 342,718 because demo controls can
change every 64 frames independently of sequencer steps. The corrected
candidate refreshes preparation on those control boundaries as well.
The corrected candidate passes native/render, all 24 clean ASan/UBSan
executables, all 11 Web regressions and another 17,614,272-frame exact
PCM/MIDI/full-state comparison. Added regressions compare stopped three-part
ARP and within-slice demo control changes across 257-frame and one-frame
rendering, including complete runtime state. The target render stack frame is
1,452 bytes before saved registers; runtime/pool size remains 77,168 bytes.
The offline-validated candidate package is
`ff9927bd8f70377c4fbcfd884877b7b3ab7a2a70698e3be8cccfa9f938fd5b8f`;
app size is 507,052 bytes, globals 50,424 and flash-off RAM text 5,568.
OTA/reboot identity passed. Its frozen-valid full eight-voice/six-drum capture
measured mean 66,294.6399 / peak 67,402 ticks, with zero deadline misses and DMA
half changes over 8,192 blocks. However, the first two FX-only repeat captures
measured means 23,262.9060 / 23,271.6292 and peaks 23,324 / 23,333 ticks. Both
had master ADC changes and are excluded from formal comparisons; their cost
also exceeded the requested absolute half-cost bound. The expanded capture run
was stopped, preserving both completed records.

The next candidate rejects inactive voices before the non-inlined melodic
call, instead of entering its register-saving prologue only to return zero.
It preserves the complete audio algorithm and all eight voices. Native/render,
17,614,272-frame exact PCM/MIDI/state, all 24 ASan/UBSan, all 11 Web regressions
and the target/flash-off/OTA-loader checks passed. Its package is
`7ce04eea952125e8a00cd40a214cc9660391f2e8d4344429ab0e96b55cbf7dc0`,
with a 506,796-byte app; globals, pool and flash-off RAM sizes are unchanged.
Its first FX-only capture measured mean 17,800.5613 / peak 17,911 ticks;
the eight-voice/six-drum capture measured mean 67,354.9399 / peak 68,406.
Both recorded zero deadline misses, but both had master ADC changes and are
excluded from formal paired comparisons.

Actual USB Audio streaming then exposed insufficient margin: eight-voice cold
PLAY recorded 14 DMA-half changes and 58 USB underruns; STOP/PLAY recorded
91 DMA-half changes and 498 underruns. Their render peaks alone (2,865.5833 /
2,875.6667 microseconds) were below the nominal deadline, so render time alone
does not establish real-time success. Both USB captures had master ADC changes.
The WAVs contain actual nonzero USB PCM, but this candidate is not accepted as
the final eight-voice implementation.

The next candidate uses a shorter exact `/127` product for bounded engine
signals, unrolls the same two/four filter stages, and prepares the unchanged LA
cutoff clamp once per slice. The largest valid engine product is
`8 * 30000 * 127 = 30,480,000`, below the exact reciprocal bound 38,693,400.
All 30,480,001 magnitudes and their negative values are exhaustively checked;
the generic full-int32 divider remains in the FX paths. No FX type, processing
rate, voice count or sample is removed. USB stress validation now fails on any
deadline miss, DMA-half change or underrun rather than treating a nonzero WAV
alone as a successful performance result.

This candidate passed native/render, all 24 ASan/UBSan executables, all 11 Web
regressions and 17,614,272-frame exact PCM/MIDI/full-state comparison. The
installed package is
`313ebd7b4b9173247d1adc3e1586f87c711a02c5807bb28c5675401a799a68b0`.
App size is 506,812 bytes; globals 50,424, pool 77,168 and flash-off RAM text
5,568 bytes. Actual USB eight-voice/six-drum cold PLAY and STOP/PLAY measured
render peaks 2,774.5 and 2,779.9167 microseconds, with zero deadline misses,
DMA-half changes and USB underruns in both captures. Both had master ADC
changes and are excluded from formal paired improvement comparisons.

The subsequent frozen-valid FX-only repeats measured means 17,495.2793 /
17,495.2848 and peaks 17,621 / 17,621 ticks, below the approximately 21,900-tick
bound. Both recorded zero deadline misses and DMA-half changes. Completed
records remain under `device/shared-voiceprep4-zero/`; USB WAVs and raw records
use the `device/usb-restart-shared-voiceprep4-*` prefix. The expanded kit/Acid
and demo USB capture set completed with zero misses, half changes and USB underruns.

The default target build now keeps app/HAL/IRQ at `-Os` and engine/effects at
`-O2`. A fresh package with both optimization environment variables removed
passes the offline checks and is byte-for-byte identical, app and package,
to the installed image. The preserved older normal package was not overwritten.

## Master volume/limiter-only working tree (superseded routing)

Before the shared-bank change, the working tree preserved four independent track FX chains and removed
the master FX processor and page. Master volume and the linked limiter are
unchanged. The v6 record layout is unchanged; legacy master settings and
automation remain portable but inactive.

Native `make test render`, all 24 ASan/UBSan executables, the web native/WASM and
UI suites, and firmware OTA/installer tests passed. A separate pre-removal
binary, with only its master effects bypassed, matches the new implementation
over 6,340,576 frames: exact PCM and meaningful project, voice, track FX, MIDI,
cache and transport state after accounting for the removed master state and
inactive legacy metadata. This is not a claim that the former wet master mix is
unchanged. The independent dry-master NEON SPIRAL reference is
`-23.071376477 dBFS`; the native loudness regression now uses that tighter routing
reference rather than the obsolete wet-master range.

The pi32v2 diagnostic candidate is
`2e912db2025680ad7a91dd7417162a57833371e92494ddccd16ac4f0bd6009e0`:
499,852 app bytes, 50,420 globals, 253,644 pool bytes and 5,568 RAM-text bytes.
App/HAL/IRQ remain `-Os`; engine/effects use `-O2`. Package, flash-off and
sanitized OTA/loader checks passed. The web FX screens were inspected in Chrome (track amounts/types/filters; FX
again stays on the track page; no master FX page). The candidate was installed by
Felucca OTA (`INSTALL_EXIT=0`, FM-1_900 re-enumerated).

Raw device `render_sum`/blocks (valid trials; same FM-1, clock, USB and demo
loads; evidence under `.omo/evidence/perf30-20261015/device`):

| Load | e0d73a6 baseline | divide | this candidate | vs baseline |
|---|---|---|---|---|
| Demo 0 | 52,017 (n=7) | 41,697 (n=2) | 33,527 (n=1) | -35.5% |
| Demo 1 NEON SPIRAL | 39,134 (n=6) | 30,755 (n=3) | 20,171 (n=1) | -48.5% |
| Repro load | 120,069 / 136,139 (two baseline groups) | 108,377 (n=2) | 100,289 (n=2) | -16.5% / -26.3% |

Only one valid candidate trial exists for each demo; the other trials were
rejected because the master value changed during capture, but their render
values agree within 0.1%. Deadline counts (block = 128 frames, about 69,627
ticks): demo 0 missed 183-185 of 8192 blocks at baseline (maximum render about
71,760, mean block interval about 71,190) and 0 with this candidate (maximum
49,236, interval 69,627); demo 1 had 0 before and after. The repro load missed
8192 of 8192 blocks at baseline and still does (mean render 100,289 ticks, 144%
of the block period; the run takes twice the real-time duration), so it would need
about 30.6% further reduction to meet the deadline. It does not reach the 30%
target. The miss definition is inferred from the raw values. The reduction comes from removing master
FX, so audio differs from the former wet master mix. LCD progress display,
rhythm, noise and PLAY/STOP pop were not observed on the device.

## Historical steady-PLAY device observation — 2026-10-15

During this earlier comparison, the device ran the XIP, steady-PLAY-LED diagnostic package
`60560fe201459cd642190319ab04a540a15bf972cac9ba4cb32229300803bc6a`.
Installation and reboot completed successfully. The user reports that the
PLAY/STOP pop on the headphone/line output is not audible with this comparison
build and explicitly chose steady PLAY illumination for this release.
`MC_PLAY_LED_PULSE` now defaults to 0; pulsing remains an explicit diagnostic
option. The observation does not distinguish LED current coupling from execution
timing, nor establish that every noise condition is resolved.

The user also reports that GLASS BELL currently sounds acceptable and playback
is approaching a usable state. The required 30% reduction in real-device
`mc_render` time has not been demonstrated. Earlier RAM experiment entries below
describe historical builds, not the currently installed image.

That revision added the bright-green current-next pattern display,
the `CURRENT-NEXT` caption, a leading `*` for edited preset names, and the restored
small `PATTERN current/64` footer. Before pressure/glide integration, all nine web
verification scripts and the complete native `make test` passed with that layout.
An earlier caption snapshot also passed all 24 ASan/UBSan C executables.
The latest pre-pressure normal package is
`ed708f0f3c9cae431149db757b9c01de0f9dd37beb446467c0f5666f1bd0d1fc`
(493,284 application bytes, 306,820 pool bytes, 5,568 RAM-text bytes).
The installed steady comparison package passed OTA/loader simulation, and that
normal package was restored byte-for-byte after its diagnostic build. Later
source changes need a fresh full verification; these records do not establish
real-device render deadlines.

The LCD scheduler now bypasses its normal 50 ms post-frame idle gap when play,
stop, step, or pattern state changes. It finishes an active frame and waits for
DMA ownership before reusing the single strip buffer; a pending transport change
starts the following frame without the idle gap. The actual scheduler regression
test covers these cases and passed. It was installed in LCD diagnostic package
`91a9f4f67cefb7979b6666bbcbaadce878748f788f07a6eff5e6e3e4fa7da4a3`
and remains in the steady comparison build. It does not eliminate LCD transfer
time or the repeated 64-pattern pitch-range scans in the renderer.

Three raw captures each were collected for NIGHT CIRCUIT, NEON SPIRAL and
PCM8_TRACK_FX_SUSTAIN. Their deadline-miss counts were respectively 0/0/0,
0/0/0 and 8192/8192/8192. However, the observed master gain was around 9175,
not the predeclared baseline's 46966, and four captures also changed gain during
capture. The strict comparator returned INVALID for all three workloads; no
improvement ratio is reported. Raw records and exclusion reasons are retained
in `device-comparison-lcd.json` in the evidence directory.

The three initial device USB WAV files contain only zero-valued samples and are
invalid evidence for noise analysis. Successful callbacks and WAV creation did
not demonstrate capture of device audio. The capture helper now rejects
all-zero recordings. The initial recorder inherited herdr's execution context
without an authorized microphone entry. A dedicated, locally built
MC1USBAudioProbe.app requested normal macOS permission, and the user allowed it.
`MIC_PERMISSION_GRANTED=1` and subsequent authorized status were verified.

`device/demo0-lcd-usb-1.wav` contains 1,058,400 stereo frames at 44.1 kHz,
peak 1339, RMS 152.153, and 2,099,816 nonzero samples. A separate 12-second
PLAY/STOP capture contains 529,200 frames with peak 1313. Its event log records
host request/acknowledgement times, not exact device sample-edge timestamps.
These are valid digital-signal captures, not a recording of the analog headphone
path or proof that physical noise is absent.

The velocity-aware core input API is implemented and has native regression
coverage for live notes, recording, ARP, drums and MIDI. Physical velocity and
aftertouch sensing are not enabled. Pressure-core and live mono-glide integration
is in progress and must be verified separately before installation. The pinned
MONK-1 license/source audit is recorded in NOTICE.md; no upstream sensing code
has been imported yet.

## RAM 하위 함수 배치 보완 — 2026-10-15, 오프라인 검사 완료

2,908바이트 RAM 후보는 실제 기기에서 캡처를 완료했지만 NIGHT CIRCUIT
1/2회 평균2280.888/2280.878us, 최대2937.292/2937.958us, 기한 초과와
DMA half 변경은 각각171이었다. master46966과 기존 클록·전원·USB 조건은
일치했다. 코드 복사·실행 성공이 성능 개선을 뜻하지 않으며 이 회귀를 숨기지
않는다. 실제 링크를 확인하니 melodic·percussion 등 내부 함수가 XIP에 남았다.

후속 `ram-hot` 후보는 산술과 상태를 유지한 채 합성·기록 PCM·oscillator·
limiter 및 FX 하위 함수까지 같은 DSP 구간에 배치한다. 실제 남은 함수
심볼이 RAM 안에 있는지 추가 검사했다. 인라인된 private 함수에는 별도
심볼을 요구하지 않는다. DSP 코드6108바이트가 기존 8192바이트 예산에
들어가며 globals/pool/flash-off 구간과 저장 영역은 유지된다.

정상 및 probe 빌드의 pi32v2 메모리·분기 거리·flash-off·SDK·LZ4 검사와
각각의 ASan/UBSan OTA·로더 검사가 통과했다. 정상 app492824바이트의
SHA-256은 `93962802d550fa1c6b5e581c85d04f721ba1a4b2188dc32c7f2753748014d837`,
정상 패키지는 `75ddafb4972d1f52443427b8168294bcbbcfd97cbf65433f446130ea8cf81717`,
별도 `candidate-ram-hot-probe` 패키지는
`2864c28235ab96b0fc686a8aa9e5e941f3c34400c1ea6211348c247da7c63687`다.
이 보완 후보는 아직 설치하지 않았고, 기존 `ram` 후보의 나머지 부하를
수집 중이다. 현재 30% 달성이나 증상 해결은 확인되지 않았다.

## 후속 DSP RAM 후보 — 2026-10-15, 설치 완료·성능 대조 진행 중

첫 XIP 후보의 NIGHT CIRCUIT 첫 캡처는 평균 2048.867us, 최대2784.625us,
기한 초과0이었다. 기준 첫 유효 회차 평균2167.319us보다 짧지만 30% 목표의
약1517us에는 못 미쳤다. 이 한 회차로 최종 개선율을 판정하지 않는다.
추가 PCM8·FX 후보 1/2회는 평균5587.422/5587.383us로, 기준 약5672.454us에
가깝고 기한 초과도 모두8192였다. 3회차는 master47154로 기준46966과 달라
동일 조건 비교에서 제외한다. 다른 데모 회차에도 ADC 변동이 있으므로
이 첫 후보에 대해 완전한 3회 비교나 30% 달성을 주장하지 않는다.

후속 후보는 공유 DSP 산술·상태를 더 바꾸지 않고 FM-1의 코드 배치를 바꾼다.
`mc_render`, `mc_effects_process_typed`, `mc_midi_sample`을 2,908바이트의
`.dsp_text`에 배치했다. 부팅 시 `0x01C04000`부터 복사하며 끝은
`0x01C06000` 아래다. 기존 flash-off `.ram_text` 5,568바이트와 debug 예약,
globals, pool, 스택, 저장 영역은 유지한다. 23비트 분기 거리 오류는
샘플 MIDI 함수를 같은 구간에 배치하고 IRQ의 호출에 기존 `FL_FAR`를 사용해
해결했다. 경고나 링크 검사를 억제하지 않았다.

실제 pi32v2 빌드가 세 함수의 RAM 주소·구간 한계·새 LMA 이미지 배치를
검사하고, 기존 `.ram_text` 무호출·메모리·SDK·LZ4 검사도 통과했다.
IRQ 활성화와 PC 보호 설정 전에 DSP 복사를 끝내며, PC 허용 창은 DSP 끝까지만
확장한다. 새 정상 및 계측 패키지에 대해 각각 기존 ASan/UBSan OTA·로더
검사를 통과했다. 공유 코어가 바뀌지 않아 앞서 통과한 C/sanitizer23개와
PCM·상태 동등성 근거는 그대로이며, 실제 RAM 실행은 기기에서 별도 확인한다.

정상 app은 492536바이트이며 SHA-256은
`682b9d0565c3092d945a89ed4610a541e1fc08857accbacfb8f83da0388561f1`,
정상 패키지는
`8fd883d6d3713c7de9995d0fc3633e17c057deb6e97618756530c4fcfae2f34f`다.
별도 RAM probe 패키지
`0adcc34e1fd2b57f39b571db4eba89da59d766d2dc1ef9b890372afee5b9ef90`는
기존 승인 범위에서 실제 설치했다. 쓰기100%·재부팅·`FM-1_900` 확인이 exit0으로
완료됐다. 이전 XIP probe와 원시 캡처는 보존한다.

초기 캡처의 TIMER4 raw CON9와 기준32777을 동일 조건으로 처리하지 않는다.
원래 비교기를 완화하지 않고 자연 counter wrap 준비 후 모든 words24..40 및
클록·전원·core1·cache 설정을 대조한다. RAM 후보 캡처는 별도
`candidate-ram-*` 이름으로 저장한다. RAM 후보의 30% 개선, 실제 오디오 기한,
실물 LED·노브·청감 개선은 아직 판정 전이다.

## 실제 기준 계측과 UI 후보 — 2026-10-15, 비교 진행 중

사용자 승인으로 기존 DSP에 진단·RAM 데모/재생 제어만 더한 기준 계측본을
설치했고 재부팅 후 `FM-1_900`과 `Felucca`를 확인했다. 패키지 SHA-256은
`4bdb156354cf87276298d688eaa73bcdb4ba4a0acca5cc94046eb7bfaacd7a1e`다.
그 소스의 probe-off 패키지는 설치 기준 `e0d73a6`과 바이트가 일치한다.
기준본 설치 후에도 사용자는 재생 문제가 지속된다고 보고했다.

정지 중 읽은 레지스터는 CLK_CON0=`0x1c1`, PLL2_CON1=`0x0750310c`,
C1_CON=`0x01000002`, P3_ANA_CON6=`0x0b`, P3_ANA_CON9=`0x1b`다.
CLK_CON1=`0x10400`, CLK_CON2=`0x6`과 PLL feedback 268은 조사한
360MHz SYS 클록 프로파일과 일치한다. 주파수계 실측은 아니며, 아래 이전
오프라인 단계의 240MHz 가정을 현재 기기의 확정 클록으로 사용하지 않는다.
CACHE_CON은 `0x302` 또는 `0x4302`다. bit14는 cache wait handshake 상태로,
PLL lock이나 정적인 캐시 설정 변경으로 해석하지 않는다.

사용자가 지정한 `charlesvestal/fm1-x0x` issue #21의 Update 4와
커밋 `f2953562785bc8f26a1ca562df254ca032663fe4`의 전원 초기화를 대조했다.
현재 선택된 VDC14 level3, SYSVDD level11은 각각 명목 1.40V, 1.26V다.
stock 초기화의 level7/15(명목 1.60V/1.38V)와 다르다. 전압계 측정도,
낮은 설정이 이번 잡음의 원인이라는 증명도 아니다. 이번 코드는 PLL,
전원 레일과 C1 설정을 변경하지 않고 정지 중 읽기만 한다.

동일 원본 데모, 내부 클록, USB Audio 비활성, master=46966 조건에서
8192블록씩 수집했다. TIMER4의 24MHz 기준 시간으로 환산한 유효 기준값은:

| 부하 | 유효 회차 | 평균 mc_render | 최대 mc_render | 기한 초과 / DMA half 변경 |
| --- | --- | --- | --- | --- |
| NIGHT CIRCUIT | 1 | 2167.319us | 2989.542us | 184 / 184 |
| NIGHT CIRCUIT | 4 | 2167.318us | 2990.083us | 184 / 184 |
| NEON SPIRAL | 1 | 1630.648us | 2160.542us | 0 / 0 |
| NEON SPIRAL | 2 | 1630.512us | 2166.208us | 0 / 0 |
| NEON SPIRAL | 3 | 1630.606us | 2166.542us | 0 / 0 |

NIGHT CIRCUIT의 2/3/5회는 master ADC 한 단계 변동으로 비교 무효다.
원시 파일은 보존하고 동등 조건 검사를 완화하지 않았다. 이 표만으로
후보 개선율이나 전체 CPU 점유율을 주장하지 않는다. 이후 NIGHT CIRCUIT
7회차도 유효하게 확보했다. 후보 결과를 보기 전에 `baseline-cohort.json`에
각 부하의 최초 동등 조건 유효 3회를 확정했다(NIGHT:1/4/7, NEON:1/2/3,
수정 PCM8:4/5/6). 원래 비교기의 `_record_problems`와 고정 조건을 실제
재검증한 후, 기존 승인 범위에서 아래 새 probe 후보를 설치했다.
전체 OTA 쓰기100%·재부팅·`FM-1_900` 확인이 exit0으로 완료됐으며,
후보의 실기기 대조는 진행 중이다.

새 후보는 오른쪽 모드 LED, 실제 16분음표 step에 동기화한 PLAY 점멸,
melodic ENV의 ADSR 그래프를 포함한다. 실제 C 화면의 3파트×최소/중간/최대
9상태, 4노브 반응, strip/guard/드럼 화면 보존을 검사했다. 웹의 동일 C UI와
네이티브/WASM·AudioWorklet 9개 검사가 통과했고, 실제 브라우저의
1440×900 및 390×844 화면을 확인했다. 모바일의 기존 회전 패널 안에서도
곡선·값·노브가 잘리지 않았다. 후보 설치는 완료했고 실물 LED와 화면 조작
확인은 남아 있다.

노브는 실제 HAL의 2샘플 필터가 한 프레임짜리 빠른 quadrature 상태를 전부
버렸다. 수정 전 실제 HAL 회귀에서 16클릭이 slow=16, fast=0으로 실패했다.
일반 회전의 첫 샘플도 디코딩하되 최초 rest 확정과 1초 rest 재학습은
안정된 상태에서만 유지하도록 수정했다. 7개 encoder의 양방향·bounce·역회전,
실제 UI의 파라미터 3배·tempo 6배 가속이 통과했다. 실물 회전 확인은 남았다.

첫 XIP 후보 단계에서 `make test`의 C 23개와 Python 수집기 검사, ASan/UBSan C 23개,
정상 및 probe 패키지 각각의 sanitized OTA·loader 검사가 통과했다.
새 정상 app은 492828바이트, globals 50036바이트, pool 306820바이트,
RAM code 5568바이트다. pi32v2 메모리·flash-off call·SDK·LZ4 가드가 통과했다.
정상 패키지 SHA-256은
`e0ed2570feedab237c6be9ea633ce5bb4ad5f4244b47fa9b22acef57ee130298`,
새 probe 후보는
`2dda7340b43ab6b2e26ce6f8cfdef8737ddc141e133f4fc977e83ca8d6720e5c`다.
probe 준비 후 정상 산출물은 원래 바이트로 복원했다.

추가 재현 부하 `PCM8_TRACK_FX_SUSTAIN`은 정지한 원본 demo0에서 MIDI
177바이트로 세 recorded PCM 프리셋(77/34/9), sustain127의 8개 지속음,
네 파트의 delay/reverb/drive/modulation 및 필터를 구성한다. SAVE는 없다.
기준 커밋과 현재 후보를 같은 바이트로 구동한 별도 C 대조에서
1,179,648프레임·9,216블록의 PCM과 304708바이트 상태가 모두 일치했다.
종료 시 양쪽 모두 recorded PCM 8보이스 유지, 네 파트 FX 활성, clips0이다.

최초 fixture는 드럼의 실제 기본 채널9 대신 채널3으로 CC를 보냈다.
실제 C 상태 검사가 이를 잡았으며 기본 채널 정의를 확인해 수정했다.
그 전 원시 `baseline-repro-1..3.json`은 삭제하지 않지만 수정된 네 파트 FX
부하의 비교에서 제외한다. 구 fixture MIDI SHA-256은
`9683e339c0a92a926cc7ea51d8f8c68b4c5728ff6e6b62ce490df5649182e406`다.
기록의 settings에 MIDI 해시가 있으므로 수정된 fixture와 혼합하지 않는다.
수정 fixture의 MIDI SHA-256은
`1530126a4cc9c5bbb6e8a93f204160627ff676cc7263ff0c6b8b80b3ea89a391`이다.
실기기 `baseline-repro-4..6.json`은 master46966·정지·step0·USB Audio
비활성을 동일하게 유지한 유효 3회다. 평균 mc_render는 각각
5672.497/5672.413/5672.451us, 최대 5684.250/5684.333/5684.083us다.
세 회차 모두 8192블록 전체에서 기한 초과와 DMA half 변경이 관측됐다.
이 극단 부하는 이미 기준 기기의 오디오 예산을 초과하며, 후보의 상대 성능과
기한 결과를 따로 대조해야 한다.

기준 DSP에서 보고된 GLASS BELL 잡음은 별도 단일음 청음으로 분리 중이다.
호스트 실제 C의 4초 단일음은 176400프레임, peak4783, clips0, steals0지만,
이 수치만으로 찍찍거림이 없거나 원인을 해결했다고 판정하지 않는다.
실기기 30% 개선과 청감·진행 표시·리듬 개선은 아직 미확인이다.

## 이전 오프라인 후보 — 당시 240MHz 가정, 실기기 성능 미측정 — 2026-10-15

사용자는 기기 전원을 꺼뒀고 240MHz로 동작한다고 전했다. 이번 후보는 그 조건을
설계 기준으로 삼는다. CPU 클록·하드웨어 캐시·IRQ·샘플률은 바꾸지 않는다.
듀얼코어 계열 사양을 두 코어의 사용 가능한 DSP 처리량으로 계산하지 않는다.
실제 현재 클록, 전체 CPU 점유율과 오디오 기한은 기기 계측이 필요하다.

부호 있는 `/127`, `/254`와 2의 거듭제곱 나눗셈을 정확한 정수 연산으로
치환했다. 음수의 0 방향 절삭과 항별 계산 순서를 보존한다. amount 0의 무효
출력 계산과 비TAPE delay의 불필요한 길이 계산을 생략하지만 delay 슬롯의
0 쓰기·위치 전진, TAPE·comb·allpass·modulation 이력은 계속 갱신한다.
기록 PCM은 엔진별 128샘플 창 8개를 사용해 연속 읽기를 모은다. 추가 RAM은
2,112바이트다. 초기 후보의 샘플당 캐시 판정 함수 3회 호출은 실제 pi32v2
코드에서 확인하고 제거했다. 프로젝트 형식·발음 수·샘플 데이터는 유지한다.

메인이 실제 소스와 명령 결과를 직접 확인했다. 최종 소스의 `make test`
19개 C 검사 및 Python 47개, `make render`, 별도 새 ASan/UBSan 19개와
실제 새 패키지를 사용한 sanitized OTA·loader 2개가 통과했다. 처음의
Homebrew LLVM sysroot 실패는 실제 `xcrun` SDK 경로와 그 SDK의 deployment
target을 지정해 해결했으며 경고를 억제하지 않았다.

수정 전 기준본과 두 고정 데모 각 60초×3회, 128프리셋×3 mix의 bend·release를
직접 대조했다. 총 18,235,296프레임·142,464블록에서 PCM과 기존 엔진 상태
304,708바이트가 모두 일치했다. 끝에 추가한 캐시 메타데이터는 그 상태 prefix에
포함하지 않는다. 별도로 16개 FX 조합×8,192프레임의 출력과 전체 FX 상태도
직접 일치했다. 창 경계·원샷·루프·112 mip·소스 교체·8보이스 회귀가 통과했다.

두 데모·8 PCM·전체 FX·128프리셋의 132부하를 양쪽 각 3회 비교했다. 호스트
명령 수 +25% 회귀 위반과 호스트 CPU 시간 +35% 경고는 모두 0건이다.
명령 수는 오히려 최대 16.57% 늘었고 일관된 호스트 속도 개선도 없었다.
이는 회귀 문턱 검사이지 FM-1 성능이나 30% 개선의 증거가 아니다.
`make test` 로그의 반복 1회 `INVALID`는 수집기의 의도된 부정 테스트 출력이며
이 3회 반복 벤치의 실패가 아니다.

pi32v2의 정적 DIV 지점은 `mc_render` 31→18, `recorded` 1→0,
`mc_effects_process_typed` 42→7이다. 코드 지점 수는 동적 실행 횟수·CPI·XIP 대기나
IRQ 선점을 포함하지 않으며 전체 처리 시간의 상한이나 개선율로 사용하지 않는다.
새 app은 491,540바이트, globals 50,036바이트, pool 306,820바이트,
RAM code 5,568바이트다. 메모리·flash-off call·SDK 해시·LZ4 가드가 통과했다.
패키지 SHA-256은 `2e49284b974a9b261302152c72603d353d9f386520c304f3538c7481f4609987`다.
웹 코어 19파일의 해시와 변경 4파일의 바이트 일치, 새 WASM·네이티브 및
실제 AudioWorklet 등의 9개 검사가 통과했다. 웹 출처는 미커밋 작업 트리로 표시한다.

계측 비교용 패키지도 오프라인 준비했다. `7dac9c1`의 기준 계측본은 별도
detached 작업 트리에서 재현해 이전 SHA-256
`fa2155d6592791a4863588615893e2397a13203b8be6ae551392ceaa4cb16276`과
일치했고, 그 작업 트리는 정리했다. 후보의 동일 계측 빌드 SHA-256은
`a7db6118b6ccf569ca1fcd614ffca1d20a531e1e399b76c3c76bb8af6ba4f181`이다.
두 계측 패키지 모두 sanitized OTA·loader 검사를 통과했다. 후보 계측 app은
492,900바이트, globals 50,356바이트, pool 306,820바이트다. 정상 빌드의
app·패키지·ELF·disassembly·보고서는 계측 빌드 전 바이트로 복원해 확인했다.

이 후보는 설치하지 않았다. 동일 FM-1·클록·USB·두 데모 및 재현 부하에서
후보 평균 렌더 시간 ≤ 기준 평균×0.70, 기한 초과와 표시 끊김·노이즈·리듬을
검사해야 한다. 128프레임의 70%인 약 2.03ms는 별도의 설계 예산이며 그 상대
30% 개선 조건을 대신하지 않는다. 현재 30% 달성 여부와 증상 개선은 미확인이다.

## 추가 30% 개선을 위한 선택적 계측 준비 — 2026-10-14

`e0d73a6` 패키지를 실제 기기에 설치하고 재부팅·USB 재연결을 확인한 뒤,
사용자는 진행 표시 끊김·노이즈·리듬 느려짐이 남았다고 보고했다.
아래 작업은 그 기준본의 계측 준비이며, 추가 최적화나 30% 달성 증거가 아니다.

`MC_PERF_PROBE=0` 실제 빌드의 app은 설치 기준본과 바이트 단위로 동일하다.
선택적 `MC_PERF_PROBE=1` 빌드는 워밍업 1024블록 뒤 8192×128프레임의
렌더 시간·오디오 처리 구간·USB 누계·패널 제출 간격과 원시 시계 설정을
42워드로 고정하고, 정상 USB SysEx 서비스에서 캡처 종료 후 전송한다.
오디오 처리 구간은 계측 helper 진입 직후까지이며 최종 계측 정리와
어셈블리 복귀를 제외한다. ISR 전체 시간·IRQ 진입 지연·LRCLK 측정이 아니다.

호스트 17개 C 검사와 렌더, 새 ASan/UBSan 17개, 계측 패키지의 sanitized
OTA·loader 2개를 직접 검사했다. 호스트 수집기 47개와 실제 C 계측·OTA
인코더→Python의 7조각 교차 검증도 통과했다. 원시 응답 완료와 성능 비교
유효성은 별도 판정이며, 비교기는 상태 변동·무효 시간·3회 미만을 거부한다.
Python LSP는 미설치이고 구문 컴파일·실행 검사로 확인했다.

계측 app은 492132바이트, globals 50356바이트, pool 304708바이트,
RAM code 5568바이트다. 패키지 SHA-256은
`fa2155d6592791a4863588615893e2397a13203b8be6ae551392ceaa4cb16276`다.
메모리·flash-off call·SDK hash·LZ4 가드를 통과했다. 계측 패키지는 설치하지
않았고 실기기 처리 시간·PCM/상태 보존·기한과 세 증상의 추가 개선은 미검증이다.
이하 각 기록의 설치 여부는 해당 기록을 작성한 시점 기준이다.

## 실기기 피드백 후 DSP·strip 후속 최적화 — 2026-10-14

`ea1abd4` 패키지는 실제 stock `FM-1_015`에 설치했고 재부팅 뒤
`FM-1_900`과 USB 재연결을 확인했다. 무효 헤더를 제공하는 쓰기 전 OTA
재진입도 두 번 확인했다. 그러나 사용자는 데모·패턴 재생 중 소리 깨짐과
화면·조작 지연이 계속 발생한다고 보고했다. 설치 성공은 성능 문제 해결의
증거가 아니다.

후속 수정은 Q15 곱셈과 64비트 출력 gain의 2의 거듭제곱 나눗셈을
음수의 0 방향 절삭을 보존하는 시프트로 대체하고, 이미 unity인 limiter의
무효 release 나눗셈을 생략한다. 화면은 현재 strip과 겹치지 않는 text를
글리프 순회 전에 제외한다. 발음 수·필터 단계·샘플률·이펙트 상태·패널
스캔 주기나 IRQ 우선순위는 바꾸지 않는다.

Q15 product의 262,156개 산술 사례에서 불일치가 없었다. 동일 호스트의 두
데모를 각각 60초, 3회씩 128프레임으로 처리한 PCM 및 전체 엔진 상태가
전후 동일했다. 10페이지×4파트×16위치의 1행/8행 렌더 회귀를 추가했고,
10페이지×32프레임×3회의 전후 픽셀 해시도 동일했다. 실제 C 패널의
10페이지를 렌더해 글리프 누락이나 경계 변화가 없음을 확인했다.

위 측정의 호스트 명령 수 중앙값은 데모 0에서 0.89%, 데모 1에서 0.69%,
화면에서는 페이지별 61.29~81.69% 줄었다. 측정 호출 비용을 포함하는
호스트 수치이며, FM-1 시간·CPU 점유율·deadline의 개선율이 아니다.
pi32v2 `-Os` 코드의 정적 나눗셈 지점은 `mc_render` 36→31,
`mc_output` 6→2다. 실행 횟수나 전체 렌더 시간의 상한으로 취급하지 않는다.
동일 소스 `-O2` 실험은 `.text` 증가와 개선 근거 부족으로 채택하지 않았다.

새 앱은 490,772바이트, globals 50,036바이트, pool 304,708바이트,
RAM code 5,568바이트다. 패키지는 610,089바이트이며 SHA-256은
`bbae85d61267bd7b7300604b0168530d235e1febe6f5f7cabbc7cc5c4bb964a0`다.
호스트 16개·오디오 렌더 129개·HTTP 및 ASan/UBSan 16개와 새 패키지의
sanitized OTA·loader 2개를 검사했다. 웹 공유 코어에도 같은 수정을 적용한다.
새 LLVM WASM과 현재 소스의 네이티브 기준으로 웹 회귀 9개도 통과했다.
Apple clang의 wasm32 미지원은 기존 Homebrew LLVM 경로로 해결했다.
호스트 합성 명령은 테스트·렌더·HTTP 성공 뒤 마지막 `git diff --check`의
pager에서 제한 시간에 종료됐다. pager를 끈 공백 검사만 다시 실행해 통과했다.

이 후속 패키지는 아직 실기기에 설치하지 않았다. 사용자 증상의 주원인을
실기기 시간 계측으로 확정한 것은 아니며, 청음·조작 지연·오디오 deadline과
두 번째 전체 재설치는 여전히 별도 확인이 필요하다.

## 성능 최적화·USB OTA 통합 — 2026-10-14

기존 로컬 수정 위에서 LA 기록 PCM의 mix 127 경로가 버리는 오실레이터 평가와
코러스/플랜저 amount 0의 wet 보간만 생략했다. 위상·샘플 위치·루프·필터·
이펙트 이력과 정수 라운딩은 보존한다. 정상 LCD 전송은 단일 버퍼의 DMA 완료를
확인해 루프당 8행씩 진행하고, 마지막 완료 관측 후 50ms 간격을 둔다.

앱은 `FELUCCA_OTA=1`이다. JEDEC 실패는 오디오 정지 전에 요청을 소비하고,
정상 업데이트는 재생을 정지한다. 실패하면 오디오·UART·일반 USB를 재개하되
재생은 정지 상태다. 큐에 게시된 MIDI 정리는 전송 완료로 취급하지 않고,
폐기 후 Stop·CC64/120/123을 새 Note On보다 먼저 다시 게시한다.
프로젝트 복원·IRQ 초기화 뒤 부팅 staging 정리도 실행한다.

기록 섹터의 삭제 및 4KiB 전체 readback 확인이 로더 변경의 선행 조건이다.
지속적인 삭제·읽기·불일치 34개 위치와 재시도에서 로더 보존을 검사했다.
단발 기록 검증 오류와 정리까지 지속되는 오류를 별도로 검사한다. 무효
패키지 거절은 기존 복구 상태를 지우지 않는다. 로더 본문은 16,352바이트
이하이며 헤더와 함께 `0xE4000` 전에 끝난다. 더 큰 공식 stock loader의
원본 V15 패키지를 수용하는 경로로 주장하지 않는다.

최종 소스의 호스트 16개, 새 Homebrew LLVM ASan/UBSan 16개, 새 패키지를
사용한 sanitized OTA·loader 2개, HTTP 및 오디오 렌더 129개가 통과했다.
웹 공유 파일 18개가 일치하며 새 WASM과 AudioWorklet·MIDI·패널 등 9개
회귀도 통과했다. 오디오 측정 보고서는 수정 전과 동일하다.
독립 128프리셋/mix 전환/release PCM SHA-256은
`1c0b50337a12e354bf8e4a1778bab1bc165030033b3e039fe7f8058dad272971`로 동일하다.

동일 호스트의 8보이스 PCM 10초 렌더 7회 CPU 중앙값은
0.059632초에서 0.048633초로 줄었다. 128프리셋·idle·전체 FX mix의
명령어 비교 130개에서 +25% 초과는 없다. 이는 FM-1 CPU 기한의 증명이 아니다.

Orbit의 역방향 분기 구간 휴리스틱을 그대로 적용하면 `mc_render` 비용은
3980→6280으로 +10% 기준을 초과한다. 이를 통과했다고 표시하지 않는다.
실제 점프 테이블과 지배 관계를 분석하면 새 역방향 재합류 3개는 반복이 아니다.
동일 DIV=9/NEST=4로 반복 구간을 계산하면 3803→3643이며, 새 `recorded`
callee의 47개 명령과 나눗셈 1개의 추가 가중치를 두 호출 지점에 더한
구간 추정은 4083(+7.36%)다. 전체 자연 루프 노드를 포함하는 별도 모델은
3944→4236(+7.40%)다. 이 값들은 기존 callee 전체·루프 밖 작업·메모리 대기를
포괄하는 렌더 시간 상한이 아니다. 최초 계산의 나눗셈 누락은 독립 검토로 정정했다.
ISR 비용은 193→193, 이펙트는 782→796이다. 정적 추정은 실제 사이클이나
XIP 대기·IRQ 지연을 측정하지 않으며, 실기기 오디오 기한은 미검증이다.

pi32v2 앱 490,740바이트, globals 50,036바이트, pool 304,708바이트,
RAM 코드 5,568바이트 및 8,212바이트 RAM loader의 메모리·flash-off call
가드와 SDK 3개 해시·LZ4 roundtrip이 통과했다. 패키지는 610,089바이트이며
SHA-256은 `919ca955192d56f14e9e200c48b161b08e4aa8f20e8b27fbaafdc113ce31ec6c`다.
측정 JSON은 갱신했다. compiler 경로는 기존 CI 표기를 유지한다.

원본 Felucca `a53ecf428c29c780108fd70a31b71a32a6b5c313`와 로컬 Orbit을
대조했다. 공식 포럼 가이드는 참고만 했고, 공식 저장소는 조회 시 404여서
코드를 직접 검토한 근거로 사용하지 않았다.
이번 작업에서는 실기기 쓰기·커밋·푸시·배포를 하지 않았다. 실제 재업데이트·
부팅·전원 차단 복구·장시간 오디오·IRQ/USB 재열거·스택 최대 사용량은 미검증이다.

## 0.3.0 설치·MIDI 안전성 수정 — 2026-10-14

설치기에서 JEDEC 식별 실패 시 복구 기록을 유지하고, 호스트의 `success\0`
응답과 복구 기록 삭제·읽기 검증이 끝나야 재부팅하도록 수정했다. 시작 시 RAM
복구 기록도 보존한다. 여러 기록을 정리할 때는 섹터마다 USB와 watchdog을 서비스한다.
호스트 오류 주입은 부분 설치 후 재시도, 잘못된 완료 응답, 35개 정리 실패 위치,
여러 기록의 부분 정리와 재시도, 17개 섹터에 각각 500 ms 삭제 지연을 포함한다.
보호 영역의 보존도 확인했다. watchdog 서비스 수정을 제거하면 지연 회귀가 실패한다.

저장과 데모 교체는 UART RX/DMA를 멈추고 입력 음·sustain·ARP·파서를 정리한다.
재개 전에는 소프트웨어 큐와 보류된 USB 입력을 폐기하고 UART DMA 커서를 초기화한다.
실제 HAL 코드의 모의 레지스터 테스트에서 1,572바이트(약 503 ms) 수신 시도를
중단 구간에 주입하고, 재개 후 새 Note On/Off가 정상 처리됨을 확인했다.
라우팅 테스트는 실제 C 엔진과 연결하여 입력 정리와 렌더 후 무음을 검사한다.
부팅 프로젝트 복원은 오디오/MIDI 시작 전에 완료하며 읽기 중 IRQ를 켜지 않는다.

데모 교체는 소비자에게 이미 넘긴 Note On의 채널만 추적하여 이전 음을 정리한다.
아직 전달되지 않은 정리도 재로드를 거쳐 유지하고 새 연주보다 먼저 출력한다.
held key, latched ARP, sustain, 출력 정체, 재로드, 큐 overflow를 검사했다.
웹은 동일 코어를 사용하며 실제 AudioWorklet에서 두 데모 × 세 채널 × ACK 정상/지연의
12개 회귀가 통과했다. 이전 WASM에서는 이전 음이 남는 실패를 먼저 재현했다.

새로 빌드한 호스트 12개 테스트, Homebrew LLVM ASan/UBSan 12개 테스트,
HTTP 통합, pi32v2 메모리 가드와 패키지 생성, 새 패키지를 사용한 sanitized 설치기
시뮬레이션이 통과했다. 웹의 네이티브 검사 2개와 Node 검사 9개도 통과했다.
소스와 웹 코어 15개 파일의 바이트 일치를 확인했다. 독립 통합 검토에서도
추가 확정 결함은 발견하지 못했다.

129개 오디오 렌더와 7개 청취 예제도 다시 생성했다. 오디오 측정 보고서와
청취 예제 파일은 수정 전과 바이트 단위로 동일하다.

현재 앱은 482,684바이트(슬롯 여유 98,880바이트), globals 46,656바이트,
pool 304,708바이트, RAM 코드 2,784바이트이며 정적 합계는 354,148바이트다.
로더는 8,212바이트, BSS 끝은 `0x01C0F3F0`이다. staged loader 끝(미포함)은
`0xE1B15`로 복구 기록 섹터 `0xE4000`과 겹치지 않는다.
현재 해시와 크기는 `target-report.json`, `package-report.json`에 갱신했다.
타깃 보고서의 compiler 경로는 기존 CI 표기를 유지한다.

실기기 플래시는 하지 않았다. 실제 UART DMA 재시작, USB 열거·저장 중 연결 유지,
전원 차단·watchdog 복구, 오디오 처리 기한 및 스택 최대 사용량은 아직 미검증이다.

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
