# Architecture and budgets

## Product boundary

`src/` is a small, original allocation-free C11 core, built unchanged by the host and FM-1.
The new sequencer, engines and pixel renderer replace ORBIT's product layers. Only GPL
startup, HAL, LCD-transfer and libc support is inherited. There is no hidden legacy sampler,
FM6, tape, radio, editor or ROM bank in the image.

Eight melodic voice structs are shared by three parts. LA uses two detuned band-limited
lookup oscillators, 24 short PCM attack choices with a crossfade to synthesized sustain,
an envelope and two lowpass stages. PCM IDs 24–39 instead play 16 recorded instrument
excerpts from CC0 VSCO 2 Community Edition, with linear interpolation, one-shot or
crossfaded sustain loops and seven offline anti-aliased mip levels. Each voice chooses
a level with Q16 advance at most one sample per output frame; SFZ pitch keycenters
determine tuning. The PCM mix retains its blend meaning; all 64 recorded presets
default to 127, so no oscillator contributes until the user lowers the mix.
Samples are mono 16-bit at a 16 kHz base rate, 6,144 frames each before mip reduction.
All levels are peak-bounded to 16,000, keeping interpolation products in signed 32-bit
range. The 390,144 bytes of PCM plus 8,320 bytes of 32-bit metadata reside in XIP;
only sample phase, rate and mip selection live in each voice.
Acid uses a monophonic voice per part, destination-step
legato slide, accent and a bounded original four-stage feedback filter. This is a musical
approximation, not a circuit model. Controls are 0–127 macros. Envelopes are quantized linear
ramps; modulation, effects and fine editor pages can be extended later.

The 128 melodic presets are grouped into 14 contiguous categories. Generated symbolic
IDs keep factory patterns, demos and showcases tied to the intended named sounds when
the bank order changes. The UI shows category and number, and resolves an exact patch
match for legacy project imports. Version 4 preserves the originating preset ID;
edited patches show its name plus `*` and browse relative to that origin. Unknown
legacy origins show IMPORTED SOUND.
Projects serialize patch parameters, not preset indexes, so reordered banks do not
change saved sounds. New recorded PCM IDs retain format version 3 but are outside
older firmware validators: old projects load in this release, while a project using
new recorded patches cannot be loaded by earlier releases. Version 4 additionally
saves ARP, effect types, filters and automation; earlier firmware rejects its version.

Six shared drum voice structs synthesize 16 lane sounds in each of eight original kits
from noise, sine/pulse oscillators, pitch sweeps and decays. Immutable kit recipes change
tuning, envelopes, noise filtering/blends, attack clicks and clap pulse spacing. A voice
keeps its triggered kit when the selected kit changes. Retrigger replaces the same sound;
closed hat chokes open hat; otherwise the oldest drum voice is stolen.
Drum-lane count is not simultaneous polyphony.

All audio uses fixed-point integer arithmetic and lookup tables. No heap allocation,
filesystem streaming or floating-point runtime is required. Output has conservative gain
headroom plus a final safety clamp. Controls change current filter values; no claim of
fully smoothed, click-free automation or analog accuracy is made.

## Timed panel input

The deterministic raw `mc_ui_encoder` API applies exact deltas for scripted use.
Physical and interactive clients call `mc_ui_encoder_at` with a monotonic millisecond
clock. Consecutive same-direction turns of the same encoder/context within 60 ms
use 3× for ranges above 40 or 6× for ranges above 150. The first turn, reversal,
context change and other control input reset acceleration. Small enumerations never
accelerate. `mc_ui_tick` expires the seven-row PRESETS/kit overlay at 500 ms from
the last nonzero valid PRESETS input, independently of audio transport. Unsigned
subtraction handles clock rollover. Firmware ticks every foreground iteration; the
HTTP host passes its monotonic clock before actions and screen requests. The popup
and normal screen use the same eight-row RGB565 renderer.

## Timing

Song mode references the same 64 patterns through 128 `{pattern, repeats}` entries.
It advances only at bar boundaries, stops at the end or loops, and keeps runtime
position separate from the UI edit cursor. Empty songs cannot start. Version 6
appends the song entries and channel/clock settings to the version 5 body; older
portable records import with an empty song and internal clock.

`midi.c` owns independent byte-parser state per transport, four part channels,
sustain/pitch state and a bounded outgoing message FIFO. External input calls the
shared synth and recording paths. Internal sequencing, ARP, note gates and parameter
changes produce MIDI output; incoming commands are not blindly echoed. External
Clock advances the sequencer at 24 PPQN and estimates tempo for gate/effect timing.
Firmware TIMER5 publishes USB/TRS input packets and services USB endpoints; the
audio ISR consumes messages, renders PCM and publishes output. USB Audio taps
the final 16-bit stereo block before the separate hardware DMA scaling.
See [connection details and unsupported BLE runtime](MIDI.md).

44,100 Hz stereo; target DMA halves contain 128 frames, giving **2,902 microseconds** per half.
Audio IRQ priority 3; input scanning timer priority 4 at 10 kHz. The ISR records worst render
microseconds and deadline misses in `max_audio_us` / `deadline_misses` for debugger inspection.
A zero host overrun count cannot establish a device deadline. UI updates use eight-row DMA
strips and do not mask interrupts for full frames. Input mutations use short IRQ-off sections.
Flash save stops audio DMA, writes only when transport is stopped, then zeros/restarts DMA.

부팅 시 프로젝트 복원은 UART·USB·오디오 시작 전에 IRQ를 끈 상태에서 완료한다.
저장과 데모 교체 동안에는 오디오 IRQ를 마스킹하고 TRS 수신 DMA를 멈춘다.
입력으로 유지되던 음·서스테인·ARP 상태를 정리하고, USB 서비스는 계속하면서
연주 입력은 폐기한다. 재개 전에 보류된 USB 입력과 소프트웨어 큐를 비우고
UART 파서와 DMA 커서를 초기화한다. 저장 중의 입력은 녹음하거나 나중에 재생하지 않는다.

The inherited startup/HAL does **not** configure or prove a 240 MHz CPU clock. It inherits
bootloader clock state; TIMER4 is explicitly sourced from the 24 MHz crystal. Reported stock
240 MHz is not a measured MC-1 clock. Inspect actual clock registers and ISR metrics on-device
before claiming stable polyphony or latency. No CPU-frequency assumption enters host tests.

## Memory and flash boundaries

Physical NOR is **1 MiB**, not expandable by software. The inherited app slot is 581,564 bytes
(`0x8DFBC`), mapped at XIP `0x02000120`; it corresponds to flash `0x4120` onward. We retain the
existing linker slot and boot/config boundaries rather than consuming freed space arbitrarily.
The 512 KiB RAM address map includes executable RAM, globals, stacks, guards and boot state.
It is not a 512 KiB free sample pool.

| Region | Bound |
|---|---|
| RAM-resident flash routines | `0x01C00000..0x01C06000`, 24 KiB |
| Reserved RAM-run debug gap | `0x01C06000..0x01C08000` |
| `.data + .bss` | `0x01C08000..0x01C20000`, 96 KiB |
| Optional `.pool` | `0x01C20000..0x01C74000`, 336 KiB, keep 8 KiB spare |
| Stack guards / stacks | `0x01C74000..0x01C7C000` |
| `.noinit` | `0x01C7C000..0x01C7FD50` |
| Boot info/mailbox/vectors | `0x01C7FD50..0x01C80000`, reserved |

Host `sizeof(McEngine)` is 304,708 bytes, including a 35,478-byte project. This is not total device
RAM: add storage scratch, DMA, scan state, LCD strip, libc/HAL state and stack usage. The target
link report is authoritative for static sections; stack high-water and IRQ nesting need hardware
measurement. Twenty-four original attack tables occupy 24,576 bytes of XIP, not a sample-memory allocation.

The [target report](target-report.json) records a 482,684-byte app, 46,656-byte globals,
2,784-byte RAM code and 304,708-byte pool. The engine is placed in `.pool`; startup clears
that section before `mc_init` initializes the engine and before audio interrupts begin.

## Persistence and safety

Sixty-four 16-step patterns and 128 song entries fit one portable 35,114-byte version 6 project record. Serialization is explicit
little endian; it does not dump compiler structs. The header includes magic, version, length,
sequence, body CRC and header CRC. Decode validates all engine indexes, notes and control ranges
before modifying the live project. Sequence comparison supports rollover.

Only `0x99000..0xAAFFF` is writable: two 36 KiB banks, each spanning nine 4 KiB sectors. A save
preserves the last valid bank, erases the other bank's sectors, writes payload, then writes the commit header.
No boot-time formatting or autosave occurs. Users explicitly confirm SAVE twice while stopped.
If another firmware used these sectors, confirmed SAVE replaces that data. Earlier development
flash saves are not migrated; their `0x97000..0x98FFF` sectors are untouched. One encoded-record
scratch buffer serves both banks, with a 256-byte readback buffer. Serialized validation runs
before decoding, so invalid input cannot partly overwrite a project and no 35 KiB project copy
is placed on the device stack. Automation addresses use 16 bits for 64 patterns × five scopes.
Calibration/global sectors `0xFC000..0xFEFFF`, tail `0xFF000`, bootloader, app/configuration,
OTA staging and sample regions are excluded by the target driver's allowlist. JEDEC must match
`0x856014` for persistence. Host fault injection checks every byte interruption; real NOR electrical
failure modes still need device validation. Save-readback errors are reported on-screen.

## Current limitations

Default panel mapping only; no imported calibration. No onboard BLE MIDI runtime, TRS output,
in-app firmware updater, sample import, effect sends, undo or on-panel chord editing.
Melodic steps can store three notes; live/step recording currently writes one. Per-drum LEVEL is
a kit-level control, while each drum step shares velocity. Pattern sounds are project-wide.
Muting is immediate and may click. Tone quality, note transitions and noise floor need listening
review on the actual codec. The host browser requires one active audio client per server instance.

## Master effects and built-in demo

`effects.c` implements four effect families per track and on the master: delay
(DIGITAL, TAPE, PINGPONG, MULTITAP), reverb (ROOM, HALL, PLATE, SPRING), drive
(SOFT, HARD, FOLD, FUZZ), and modulation (CHORUS, PHASER, FLANGER). Independent
HPF and LPF follow each processor; zero bypasses each filter. These are bounded
fixed-point designs, not circuit models. Type 0 and bypassed filters retain the
original DSP exactly. The phaser uses all-pass stages; flanger uses a short
modulated delay; state transitions clear the changed family's stored signal.
The delay/comb/chorus buffers use 53,248 bytes per processor; total state is 53,356
bytes including counters, typed-filter state and coefficients. Four independent
track processors precede the shared master. Tracks process after level/pan and
before summing. Muting removes dry and wet output. Dry processors are skipped;
returning all amounts and filters to zero clears buffers once. Master bypass
continues advancing its buffers, preserving legacy demo output. Amounts and filter
controls are 0–127; feedback is bounded below unity. Final master scaling uses a
wide product and safety clamp. No heap allocation.

Each melodic part has an arpeggiator with 32 held-key slots, six modes, quarter/
eighth/sixteenth-note rates, 1–3 octaves, gate and latch. Generated notes follow the
sequencer's swing clock while running, or the engine clock when stopped. CHORD
emits up to three notes; existing voice allocation and monophonic Acid still apply.
PLAY + REC writes generated notes and continuation steps; a tie flag preserves
long gates without repeated attacks. Part changes, stop, mode/latch changes and
explicit clear release held/generated state. Physical key-up retains its original
part/note owner so selecting another part does not leave held notes behind.

A central parameter setter applies sound, level, per-drum level, FX type/amount and
filter edits. During PLAY + REC it stores one final value per pattern/part/step/
parameter address. The project shares 384 five-byte serialized events; same-address writes
replace existing values, while full capacity rejects new events with a visible
status. Events replay before note triggers. Tempo, swing, ARP setup and step edits
are not automation parameters. Pattern clear compacts remaining events. Preset
origins, ARP settings, typed FX and events append to the legacy body in version 4;
version 1–3 imports receive safe defaults without changing the old patch/audio.

`demo.c` stores original note patterns and a 32-bar performance. The same sequencer,
mutes, cutoff/resonance controls and effects execute on both targets. Automated
filters update every 64 samples; manual filter edits may be overwritten while demo
mode is enabled. Demo loading resets state only after the device stops audio DMA.
Version 2 extended the portable body by eight bytes for effects/demo settings;
legacy version 1 projects migrate to dry master effects/no automation. Version 3
uses body byte 3 (formerly required zero) for the kit index and appends 16 track FX
amounts after the existing 2,120-byte body. Version 1/2 records still require that
reserved byte to be zero and import as BASE 808 with dry track FX. Their master
settings are retained. Version 3 rejects kit indexes outside 0–7 and FX amounts
outside 0–127. Old firmware rejects version 3 records rather than misreading them.
A/B sectors and app flash bounds are unchanged. The larger state and five processors require new
real-device timing/stack checks; host speed is not a pi32v2 benchmark.
