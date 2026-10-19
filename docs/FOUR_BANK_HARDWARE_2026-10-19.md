# Authorized four-bank hardware trials, October 19, 2026

This record separates source-pinned device observations from host proofs and
from later display revisions. No release or project commit was requested.
Raw evidence is retained under `build/hardware-fourfx-20261019/`.
The source checkout is based on `869ed1265000075d0a52011d379e3da8060fcd70`
with intentional uncommitted changes; that commit alone does not identify
any measured image.

## Recovered: eight voices selected

The 10-melodic/5-worker experiment failed reboot and was removed. The user
selected eight voices and explicitly authorized the app-only restoration by
typing RESTORE in the operator terminal. Larger-count hardware experiments
are stopped; the target builder now rejects anything except eight melodic /
three worker voices.

The known-good recovery target is the exact eight-voice package
`e0afbf5254eb910133cfab170928dd306d5d9e8eb5a47afc29094c4906649228`.
The default build was independently reproduced byte-identically, including
after the builder was restricted to eight melodic / three worker voices.
Latest source fingerprint:
`d11cfd79d2735a375aacdb1861448dd3aa1179f3c2f2e235ecf16122e60ea0f2`.
Earlier raw captures retain their original source fingerprints; they are not
rewritten to the later fingerprint.

Administrator AppleScript execution could not claim the ROM USB interface:
`IOCreatePlugInInterfaceForService: out of resources`, even as uid 0.
The same reviewed read-only helper worked through the operator's regular
Terminal/sudo session. No macOS security policy or USB/flash allowlist was
relaxed. Two full reads agreed on the fresh backup SHA256
`c7fbded3bf6fdb48fa5fc845809d40372fc44196096c3fae58556ddbd0c1fb03`.

The fresh backup's app range exactly matched the failed 10/5 package.
Only the 128 differing app sectors, `0x4000..0x83FFF`, were rewritten,
high to low. Every sector and then the full 1 MiB were read back; the final
image exactly matched SHA256
`6ec1bd6389aa369a5b191687052034c534cfab26380475ed00059382df68448b`.
The boot head, entire data region and both project banks are byte-preserved:

| Preserved region | SHA256 before and after |
| --- | --- |
| boot head | `d9f43191777656de01c7bed4f9e9ba2e34e94832c5d2ed02c3fc7ab7a6d9bd67` |
| data region | `979af8df3e0b628eea9af0af5132a32ad38d7b98615bddabe56eee7efaf57023` |
| project banks | `ab47f12356e6179cc34c765bc2090e4630d48c1c59cda5e9d2ed03363c153c07` |

The user confirmed the instrument returned. An independent new MIDI connection
identified `FM-1_900` on `Felucca`, followed by a fresh NIGHT CIRCUIT USB capture.
Its 8,192-block window has zero deadline misses, DMA-half changes and all four
USB error counters. CPU1 is READY, failures 0, stack guards intact, unused
user stack 3,604 bytes; maximum whole ISR is 61,862 ticks. The workload is
stopped at completion. ADC master 10,821→10,731 makes this a functional
recovery observation, not a new frozen speed comparison.
Evidence: `recovery-backup-result.json`, `eight-voice-restore-result.json`,
the referenced append-only sector journal and `eight-restored-check/`.
The final USB WAV has 2,205,407 nonzero samples, peak 0.047333 and no rail
clips. CLK_CON0, PLL2_CON1 and decoded VDC14/SYSVDD selections match the
pre-recovery observation. No LRCLK measurement is inferred from these values.

Before the capacity experiment, this exact eight-voice image also completed
three PLAY/STOP program sweeps: 48 device-quarter-note batches of three
channel Program Changes per pass, 432 messages total. Deadline/DMA/worker
failures were zero and guards intact in all passes. Both factory-demo
45-second USB captures had zero audio/DMA/USB/worker errors. The controlled
RAM-only protocol-failure test acknowledged a safely held CPU1, reported the
one expected failure/status 4, and kept NIGHT CIRCUIT's ISR at 60,939 ticks or less
with zero deadline/DMA errors. Subsequent image reinstall and the final
post-recovery capture restore READY and zero failures.

## Capacity observations, not a new supported ceiling

Optional-count native oracles retain exact PCM and complete engine-state
comparison. Each of 8/3, 9/4, 10/5 and 9/5 additionally passed 3,700,392
probe-enabled frames. These do not prove target interrupt-stack safety.
The experimental schema-8 snapshot confirms actual melodic activity before
and after the hardware window; six drum slots remain separate.

| Melodic/worker | Workload | Max whole ISR | Deadline / DMA changes | USB errors |
| --- | --- | ---: | --- | --- |
| 8/3 | sends 0, FX probe bypass, Analog off, USB | 55,047 | 0 / 0 | 0 |
| 8/3 | four FX, Analog 127, USB | 68,938 | 0 / 0 | 0 |
| 9/4 | sends 0, FX probe bypass, Analog off, USB | 59,076 | 0 / 0 | 0 |
| 9/4 | four FX, Analog 127, USB | 75,639 | 7,663 / 7,631 | 26,245 underruns |

All four hardware windows retained their full configured melodic activity,
READY, zero worker failures and intact worker guards. The overloaded 9/4
window outlasted its 45-second recorder, so UAC start/end flags differ; that
record is not a frozen speed comparison. A longer recorder option was prepared
but the user stopped the sweep before its next hardware use. No maximum above
eight is established by the unbooted 10/5 image.

Read-only disassembly of the built, uninstalled 9/5 image found a renderer
frame of 8,020 bytes; its audio IRQ wrapper/handler brings this to at least
8,176 before callees, exceeding the 7,936-byte supervisor allocation and
writing inside the guard. This is a concrete unsafe target configuration,
and supports stack exhaustion as the leading 10/5 failure hypothesis.
The exact 10/5 exception PC/frame has not been measured. Neither 9/5 nor
another larger-count image will be installed.

## Audio ownership and unchanged limits

CPU0 retains sequencing, MIDI, gate/envelope control, five melodic DSP voices,
mixing and output. CPU1 renders the other three melodic voices, six drums and
all four shared FX banks from private copied state. Total melodic capacity
remains eight. The four banks are R/D1, C/F1, R/D2 and C/F2; Analog is optional
per-melodic-part variance. No voice shedding, automatic FX bypass, reduced
sample format or increased audio deadline was introduced.

The normal worker bound remains 12,000 TIMER4 ticks (500 microseconds).
Each audio half contains 128 frames; its whole-ISR deadline remains 69,648
ticks, approximately 2.902 milliseconds at the diagnostic 24 MHz timer rate.
Stage timers on different cores overlap and must not be summed as elapsed time.

No SAVE command, software gain override, CPU/PLL/divider/voltage write or
bootloader/partition/flash-allowlist change was made. The agent did not adjust
the physical master control. ADC changes still invalidate frozen comparisons;
they are not averaged away or hidden by clamping the master value.

## Matched 6x9 single/dual comparison

The unchanged frozen comparator accepts the three repeats per side for all
three original two-bank workloads. All words 24..40 match, including physical
master 10,731, timer status, clocks, BPM, playback and step positions.

| Acid parts | Serial mean ticks | Dual mean ticks | Reduction | 30% timing gate |
| --- | ---: | ---: | ---: | --- |
| 0 | 75,917.416 | 55,337.284 | 27.109% | FAIL |
| 1 | 75,045.979 | 54,585.813 | 27.264% | FAIL |
| 2 | 74,341.571 | 53,814.447 | 27.612% | FAIL |

The gate remains 0.70; these results are not rounded or relabeled as passes.
Every selected serial record has 8,192 deadline misses and 8,192 DMA-half
changes; every selected dual record has zero of both. Real-time improvement,
PCM/state equivalence and the 30% mean-time target are distinct results.

The dual collector retained 21 attempts and selected the first matching three
per workload; the serial collector retained 14. Exclusions depend only on frozen
conditions, never on speed or deadline outcome. Early serial attempts had raw
TIMER4 CON 9 instead of 32,777; no register was written to force a match.
All attempts and exclusion reasons remain in `font9-dual-cohort/` and
`font9-single-cohort/`. Exact ratios, chosen paths and counters are preserved in
`font9-comparison.json`. The comparator command exits 1 for the three FAIL
verdicts; that exit is retained rather than suppressed.

## Selected 6x9 candidate

The user's final font choice is 6x9, not the briefly proposed 8x12.
White secondary text, populated-track playback underlines, all fourteen button
backlights, BPM PLAY blinking and steady HOME during playback are included.
All sixteen drum rows remain visible at a ten-pixel pitch. The installed image
retains the normal 500-microsecond worker bound and has CPU1 tracing disabled.

- Source input SHA256:
  `dd8a7f91be00750851ce76e88e279b55771afcde901621314488e855f99ec2e4`.
- Verified eight-voice dual profiling package, 610,089 bytes:
  `e0afbf5254eb910133cfab170928dd306d5d9e8eb5a47afc29094c4906649228`.
- Verified eight-voice app, 521,784 bytes:
  `d7654e41eb9ef375339127abed6b8bc8da026d59560d694001fbe72ba80e177f`.
- Matching single profiling package:
  `e42807109d147138028eb9cad6383f5ab614cf82720c27de58a2c3fde7ce7ff5`.
- Normal, unprofiled dual package, offline-verified but not installed:
  `a84ade1ac4c6cfd5f83c864856ec9b05c97339a914d27e3616b24834a03f3d8e`.
- Normal, unprofiled single package, offline-verified but not installed:
  `63a75bd455a74b946ee5d2dd636ecb6e9ebc13d05fe8d36b551cae30bac8fe95`.

All four actual package hashes were independently recomputed and matched their
frozen reports and one source fingerprint. OTA and loader simulation pass for
each. Dual globals use 55,692 bytes with profiling or 54,968 without; the pool
uses 305,576 of 335,872 usable bytes, leaving 30,296 after the stack reservation.
Single pool use is 229,488 bytes. These build figures are not runtime margin.

The complete final `make test` passes 30 C executables and 47+4 Python tests;
`make render` produces 705,600 frames with no clipping or steals. Separate
Homebrew LLVM ASan/UBSan UI and dual-core builds pass, as does ThreadSanitizer.
The probe-enabled dual-core oracle compares 3,700,392 frames and every byte
of the 114,288-byte engine. The regular native oracle compares 3,695,784 frames.
The system Apple sanitizer runtime failed before test entry, so its failed
launch is not represented as a code failure or a passing sanitizer run.

Thirty fresh actual-C contact sheets cover 120 UI states. Every page/part,
popup, drum selection, FX type, pan, empty/muted track, edit/message and song-end
state was viewed. Replacing the initial generic font scaler's per-pixel divisions
with constant lookup maps, and restoring the original large-text rectangle
path, preserves every pixel in that complete regenerated matrix.

The final four-case maximum-load USB cohort uses the same explicit workloads
as the earlier white-text cohort, with the following observed values:

| Action | Mean render ticks | Max whole-ISR ticks | Deadline / DMA / USB errors | Master start/end |
| --- | ---: | ---: | --- | --- |
| 12 | 62,967.515 | 68,792 | 0 / 0 / 0 | 10,731 / 10,731 |
| 13 | 62,502.925 | 68,556 | 0 / 0 / 0 | 10,731 / 10,731 |
| 14 | 62,156.597 | 68,374 | 0 / 0 / 0 | 10,821 / 10,731 |
| 15 | 46,690.181 | 52,390 | 0 / 0 / 0 | 10,731 / 10,821 |

All four have CPU1 READY, zero worker failures, intact guards and 3,588 unused
user-stack bytes. All four actual 45-second USB WAVs contain nonzero 44.1 kHz,
16-bit stereo PCM with no rail-clipped samples. The smallest observed whole-ISR
headroom is 856 ticks, about 35.7 microseconds or 1.23%.

Actions 14 and 15 fail the existing frozen master-pair check. Their error
counters and recordings remain observations under the recorded changing gain;
they are not eligible for a fixed-condition speed comparison. The 10,731-era
records must not be compared against earlier 4,340-gain records as though the
conditions were identical. No software gain override was used.

LCD estimates are 7.32, 7.87, 8.00 and 15.99 frames/second, with maximum gaps
145, 136, 133 and 67 ms respectively. The bigger font remains subject to the
measured foreground budget. Coherent frames and correct underlines are not
a claim that visible motion is perfectly smooth at maximum audio load.
Raw final evidence is in `font9-usb-max/`.

## Earlier white-text candidate: complete maximum-load USB cohort

This is the white 5x7-text build, before the user's subsequent 6x9-font request.
It is not evidence that the later font image has already been tested.

- Source input SHA256:
  `9032fe6e738eb9b95d7ac310439760a558751b001bb2cb3d473e2d9f1899a71d`.
- Installed dual profiling package SHA256:
  `4746627903493c5bfb10662028c01d37727c63d18b8604d3c0e07f7519e4b43d`.
- App SHA256:
  `371cae1d852ae1348d47801e5abc99f84dd3fa4a293936499d29cc62f89497dc`.
- Matching single profiling package SHA256:
  `0e3c17bc578681ccd7b109d2fff55e9ab547ff7b496be44e124053117ce07fd2`.
- Both core settings also built and passed OTA/loader simulation without
  profiling/probes. All four configurations retain the same source fingerprint.
- Device installation, reboot and FM-1_900 reidentification completed.

Every trial used all four sends at 127, both R/D types 5, both C/F types 5,
kit 6, Analog 127 on all three melodic parts, actual USB audio capture and
the active LCD. Each frozen capture contains 8,192 blocks after 1,024 warmup
blocks. Conditions were stable at master 4,340 in all four records.

| Action | Melodic condition | Mean render ticks | Max whole-ISR ticks | Deadline / DMA / USB errors |
| --- | --- | ---: | ---: | --- |
| 12 | Eight melodic voices, no Acid | 62,900.579 | 68,697 | 0 / 0 / 0 |
| 13 | Eight melodic voices, one Acid part | 62,487.400 | 68,625 | 0 / 0 / 0 |
| 14 | Eight melodic voices, two Acid parts | 62,191.621 | 68,279 | 0 / 0 / 0 |
| 15 | Separate three-Acid monophonic case | 46,660.577 | 52,517 | 0 / 0 / 0 |

All cases include six drums. Action 15 is deliberately not described as
eight simultaneous Acid voices. USB error columns include underruns, overruns,
stalls and missed packets. CPU1 remained READY with zero failures, intact
stack guards and 3,588 unused user-stack bytes in every capture.

The smallest observed whole-ISR headroom is 951 ticks, about 39.6 microseconds
or 1.37%. Passing this cohort does not establish a 10% timing margin or
unbounded stability. Each associated 45-second USB WAV is non-silent 44.1 kHz,
16-bit stereo, with finite samples and no samples at digital full scale.
This rules out the earlier all-zero-recording failure, not every possible
audible or analog-output defect.

Raw files are in `bright-usb-max/`: each action has the fixed 42-word record,
seven-page stage record, schema-7 panel snapshot, USB log and WAV.
The panel estimates were approximately 8.34, 8.74, 8.90 and 15.99 frames/second
respectively. These estimates include warmup and host queries, not only the
frozen audio window. Maximum completion gaps were 128, 122, 122 and 69 ms.
The display is not claimed to run at 60 Hz or to have passed subjective
smoothness inspection merely because its audio deadline passed.

## Repeated playback and real MIDI program changes

The same `bright` image passed three additional 8,192-block captures while
the host changed all three melodic channels on device-generated MIDI clocks.
Each playback pass issued 48 quarter-note-triggered batches, three Program
Change messages per batch, cycling programs 0, 17, 31, 63, 95 and 127.
Across the three passes this is 432 messages, with STOP/PLAY between passes.
There are no sleeps or wall-clock polling in the trigger mechanism.

All three captures have zero whole-ISR deadline misses, DMA-half changes and
USB diagnostic errors; CPU1 remains READY with zero failures and valid guards.
Their frozen scalar checks are valid. These are live-changing-patch workloads,
not fixed-patch speed-comparison baselines. Raw records, program event logs,
stage records and before/after register snapshots are in `bright-playback-soak/`.

## Display and button changes

The LCD retains a single live DMA strip buffer. A 115,200-byte native-RGB565
last-submitted cache in the existing SRAM pool permits unchanged strips to
be skipped and changed strips to send only their bounding x range. Rectangle
fills clip before row-span writes. Thirty strips share one 32-byte transport
snapshot and a prepared/copied UI rather than sampling a different step per
strip. Tests cover 28,920 dirty-rectangle extents, 460,800 clipped fills and
264 complete frozen frames with transport/UI mutation between strips.

The green playback box was replaced at the user's request by a short flowing
underline, only for populated unmuted tracks/drum rows. Rest steps within a
populated track retain progress. Edit selection stays WHITE and separate.
Secondary gray text was promoted to WHITE without whitening the grid or
inactive graphical controls.

All twelve menu/transport buttons and both OCT buttons are normally lit.
Selected menus/modes blink at 2 Hz. Running PLAY instead blinks green from
quarter-note sequencer phase, independently of REC. HOME stays steady during
playback and blinks only when selected while stopped. Physical-map tests keep
the PLAY red die off and preserve all held-note/step LEDs.

## Rejected candidates and recovery

The first CPU1 job originally exceeded the normal 500-microsecond bound.
Diagnostic tracing exposed a slow cold clear/copy path, not proof that a
longer timeout was acceptable. Aligned ordinary-memory copies and effect
clears reduced a traced cold job from 18,581 to 2,465 ticks. The longer
diagnostic bound was removed; subsequent normal-bound captures complete.
The full FX-history snapshot was replaced by exact sparse read preparation
and dirty-tap commits, with poisoned-private-history host oracles.

One later candidate lost the USB MIDI device after switching from action 4
to action 8. Its package SHA256 was
`b6b0a9566dab3e20f68f1ca923ea9e570a31fc98f643e76ef869e0a2ba9ff690`.
It combined a two-voice worker/result-view experiment with a new four-byte
copy unroll. The user confirmed that the device had remained connected and
appeared down. After the user's power cycle, the known drum-only package
`85e4806a81d62c9979096781ae9271faca5f6331ffabdb2871ff57c9979ef2c1`
was restored and independently identified. The new four-byte unroll was
removed; the proven aligned eight-byte path and ordinary four-byte loop remain.
The cause of that disconnect was not established. Later successful trials
are not retrospective proof of a particular root cause.

RAM-resident synthesis and per-sample unity-send branching were rejected
because they did not improve the measured workload. No extra executable-RAM
section remains. The retained routing optimization hoists send cases outside
the sample loop and is checked against an independent signed-division oracle.

Some earlier captures changed master between 4,340 and 4,397; those raw
records are retained and are not valid frozen comparisons. A six-case `beat`
host run reached its five-minute monitor limit during its last USB recording.
A subsequent `lights` run was cancelled to apply the user's HOME behavior
correction; STOP was rejected while a frozen capture was still active.
Neither host interruption is classified as a complete cohort or silently
merged into the complete `bright` cohort above.
