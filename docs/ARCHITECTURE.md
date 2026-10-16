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
range. The 390,144 bytes of PCM plus 8,320 bytes of 32-bit metadata reside in XIP.
각 엔진은 소스·mip·범위 태그를 포함해 2,112바이트인 128샘플 PCM 읽기 창
8개를 추가로 소유한다. 미스 때 유효한 연속 샘플만 복사하고 창 밖의 루프
보간점은 원본을 직접 읽는다. 캐시는 프로젝트에 저장하지 않는 내부 실행
데이터다. 실제 FM-1의 전체 후보 처리 시간과 기한은 별도 원시 계측으로
대조하며 캐시 단독의 기여율로 해석하지 않는다.
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

FM-1 입력 HAL은 일반 quadrature 전이를 첫 샘플부터 누적한다. 최초 rest의
안정 확정과 정지 1초 후 rest 재학습은 유지하고, 완전한 detent 복귀 때만
클릭을 게시한다. 빠른 상태를 2샘플 미만이라는 이유로 버리지 않는다.
melodic ENV 화면은 실제 ADSR 계수에 대응하는 곡선과 네 제어점을 그린다.
Mode LEDs update only when their state changes. PLAY remains steadily lit during
playback; the pulsing diagnostic option defaults off after the user's physical
pop-noise comparison. Keyboard LEDs show the union of physically held keys and
the white-key playback cursor, so releasing a key does not erase that cursor.

The physical button-edge API defers an OCT- tap until release. While held, the
first four black keys select one of four banks, and the 16 white keys select a
pattern in that bank. Chord keys are consumed through their release, even if OCT-
is released first; they do not transpose, sound, or record notes. Selection uses
the existing bar-quantized queue and is rejected in SONG mode. The legacy
single-button API retains its immediate action for scripted clients.

Before a device/web frame, `mc_ui_prepare_frame` computes each melodic part's
pitch range across all 64 patterns once. The eight-row renderer reads this
transient UI cache without mutating it; callers that do not prepare a frame
retain the uncached path. Newly recorded out-of-range notes are clamped to the
current frame's pitch marks until its next preparation. Pixel equivalence is
tested across ten pages, four parts and all 16 playback steps.

Overlapping live ACID keys use the existing legato slide path when GLIDE is
nonzero, preserving oscillator/filter/envelope state. Newest presses take
priority; releasing the active key returns to the highest remaining held note.
GLIDE zero retriggers instead. Live-recorded destinations retain the SLIDE flag.
Transient held-key bitsets are cleared by the existing part/all-notes-off path.

Physical velocity/aftertouch sensing, pressure modulation, and battery display
are excluded from this release. Unsupported MIDI pressure messages are consumed
without changing voices or desynchronizing the MIDI parser.

Each melodic voice caches its exact phase increments by source increment and
Q16 pitch-bend ratio. Stable notes reuse both oscillator increments; gliding
notes retain the original per-sample calculation. Bend ratios are computed once
per render call, after MIDI input has been consumed. Cached fields are transient
and do not alter the common engine prefix, patch data, or project format.

Within each at-most-16-frame slice, active voices prepare unchanged sustain and
cutoff coefficients, oscillator-table selection, recorded-level metadata and
stationary LA pitch once. Slices do not cross internal sequencer steps; demo
controls refresh preparation at their 64-frame boundaries. Stopped ARP uses
one-frame preparation because it may trigger a note on any frame. Recorded
interpolation is inlined into the separate melodic renderer; inactive voices
are rejected before that function call. Preparation uses stack memory, not
additional engine state or serialized fields. Drum reciprocal products preserve
signed truncation over the full bounded numerator range of every kit.

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
The USB ring reserves 208..256 frames at render start (232-frame prime) because
the app publishes the whole 128-frame block only after DSP completes. TIMER5
consumes the previous block during rendering. The ring regression models this
publication order at up to 98% render load, including restart and clock drift;
it must not publish artificial 32-frame sub-blocks during DSP. This USB reserve
does not add latency to the physical DMA or sequencer.
See [connection details and unsupported BLE runtime](MIDI.md).

44,100 Hz stereo; target DMA halves contain 128 frames, giving **2,902 microseconds** per half.
오디오 IRQ 우선순위는 3, 10 kHz 입력 TIMER5는 4다. `max_render_us`는 렌더
구간 경과 시간, `max_audio_us`는 ISR 처리 경과 시간이며 선점 시간을 포함한다.
`max_audio_interval_us`는 ISR 시작 간격의 최댓값이다. `deadline_misses`는
처리 시간 초과 또는 처리 중 DMA free-half 변경을 세며, 후자는
`audio_half_changes`에도 기록한다. 진입 지연이나 두 반절 이상 누락을
완전히 검출하는 지표는 아니다. 호스트의 0회 overrun은 기기 기한을 입증하지 않는다.
정상 화면 갱신은 DMA가 끝난 단일 버퍼에 루프당 8행을 렌더·전송한다.
마지막 전송 완료 관측 후 50ms를 기다리므로 입력 처리를 한 프레임 전체 동안
막지 않는다. OTA 진행 표시는 동기 draw를 사용한다. Input mutations use short IRQ-off sections.
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
| RAM-resident flash routines | `.ram_text`, `0x01C00000..0x01C06000` capacity; 5,568 bytes used |
| Reserved RAM-run debug gap | `0x01C06000..0x01C08000` |
| `.data + .bss` | `0x01C08000..0x01C20000`, 96 KiB |
| Optional `.pool` | `0x01C20000..0x01C74000`, 336 KiB, keep 8 KiB spare |
| Stack guards / stacks | `0x01C74000..0x01C7C000` |
| `.noinit` | `0x01C7C000..0x01C7FD50` |
| Boot info/mailbox/vectors | `0x01C7FD50..0x01C80000`, reserved |

Host `sizeof(McEngine)` is 77,168 bytes, including a 35,488-byte project. This is not total device
RAM: add storage scratch, DMA, scan state, LCD strip, libc/HAL state and stack usage. The target
link report is authoritative for static sections; stack high-water and IRQ nesting need hardware
measurement. Twenty-four original attack tables occupy 24,576 bytes of XIP, not a sample-memory allocation.

The engine is cleared before `mc_init` and audio interrupts. Two shared FX banks
replace the four independent track processors; master output has no FX processor.
Raw engine offsets are not a project-file ABI. Version 7 retains the previous
serialized body as a prefix and appends ten bytes for sends and global types.

DSP executes in XIP. The experimental `.dsp_text` relocation was removed after
device regression and adverse noise feedback. Only the existing flash-off
routines are copied into executable RAM and checked for flash-off calls. PC
protection does not permit execution from globals, the pool or the reserved
debug gap. Historical RAM experiment reports are retained as failed experiments,
not as the current link layout or evidence of a speed improvement.

## Persistence and safety

Sixty-four 16-step patterns and 128 song entries fit one portable 35,124-byte version 7 project record. Serialization is explicit
little endian; it does not dump compiler structs. The header includes magic, version, length,
sequence, body CRC and header CRC. Decode validates all engine indexes, notes and control ranges
before modifying the live project. Sequence comparison supports rollover.

프로젝트 저장 드라이버의 쓰기 범위는 `0x99000..0xAAFFF`다.
별도의 OTA 드라이버는 `0xE0000..0xE4FFF` staging만 쓴다. 기록 섹터 전체의
삭제·readback 검증이 끝나기 전에는 로더 섹터를 변경하지 않는다. 두 범위를
서로 넓히지 않는다. Two 36 KiB project banks each span nine 4 KiB sectors. A save
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
sample import, undo or on-panel chord editing.
앱은 Felucca 호환 USB 업데이터를 활성화하며 실기기 전체 재업데이트와
재부팅을 확인했다. 실제 쓰기 중 전원 차단 복구는 미검증이다.
업데이트 진입 시 재생을 정지하고, 실패 복귀 후에도 정지 상태를 유지한다.
외부 MIDI 채널·transport 정리는 TX 큐 폐기 뒤 다시 게시하며 새 Note On보다
먼저 처리한다. 크기가 기록 섹터를 침범하는 loader 패키지는 거부한다.
Melodic steps can store three notes; live/step recording currently writes one. Per-drum LEVEL is
a kit-level control, while each drum step shares velocity. Pattern sounds are project-wide.
Muting is immediate and may click. Tone quality, note transitions and noise floor need listening
review on the actual codec. The host browser requires one active audio client per server instance.

## Shared effects, master output and built-in demo

`effects.c` implements two global fixed-point FX banks. R/D selects ROOM1,
ROOM2, ROOM3, HALL1, HALL2, PLATE, DELAY or PANNING DLY. C/F selects CHORUS1,
CHORUS2, CHORUS3, CHORUS4, FB CHORUS, FLANGER, SHORT DLY or
FB SHORT DLY. These are original approximations, not Roland algorithm
emulation. Each of the four parts has independent R/D and C/F sends, 0–127.
After part level/pan, each signal enters the dry mix and its two send sums.
Each shared bank processes its sum once, and both returns join the dry mix.
Muting stops a part's dry signal and new sends; an existing shared tail can remain.
Changing a bank's type clears only that bank's state. With no sends, a bank
bypasses and clears its stored signal once. There is no master FX processor,
per-part effect type, drive processor or HPF/LPF stage. The final sum goes through
master volume and the linked limiter, with a wide product and safety clamp.
Feedback stays below unity and rendering allocates no heap memory.

The FX page edits the selected part's two sends; ENV switches SENDS/TYPES.
TYPES edits the two global banks and clamps each choice to 0–7. KNOB3/KNOB4
are unused on both pages. Automation parameters 40/41 address part sends;
42/43 address global types in canonical scope 4. Type output uses part 1's MIDI
channel, not a master audio processor.

Version 7 appends eight sends and two types to the existing body. Readers accept
versions 1–6 at their original exact sizes, retaining legacy FX fields for data
round trips but not processing them. Import chooses the delay/reverb family by
aggregate old amounts, uses each part's larger delay/reverb amount as its R/D
send, and maps modulation amounts to C/F sends. The first active part supplies
each global type. Matching send/type automation is converted; other legacy FX
events stay stored but inactive. Import never saves or erases the source banks.

Each melodic part has an arpeggiator with 32 held-key slots, six modes, quarter/
eighth/sixteenth-note rates, 1–3 octaves, gate and latch. Generated notes follow the
sequencer's swing clock while running, or the engine clock when stopped. CHORD
emits up to three notes; existing voice allocation and monophonic Acid still apply.
PLAY + REC writes generated notes and continuation steps; a tie flag preserves
long gates without repeated attacks. Part changes, stop, mode/latch changes and
explicit clear release held/generated state. Physical key-up retains its original
part/note owner so selecting another part does not leave held notes behind.

A central parameter setter applies sound, level, per-drum level, shared FX type
and send edits. During PLAY + REC it stores one final value per pattern/part/step/
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
A/B sectors and app flash bounds are unchanged. Shared-bank device timing and
stack checks must be measured independently; host speed is not a pi32v2 benchmark.
