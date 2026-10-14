# Architecture and budgets

## Product boundary

`src/` is a small, original allocation-free C11 core, built unchanged by the host and FM-1.
The new sequencer, engines and pixel renderer replace ORBIT's product layers. Only GPL
startup, HAL, LCD-transfer and libc support is inherited. There is no hidden legacy sampler,
FM6, tape, radio, editor or ROM bank in the image.

Eight melodic voice structs are shared by three parts. LA uses two detuned band-limited
lookup oscillators, twelve short PCM attack choices with a crossfade to synthesized sustain,
an envelope and two lowpass stages. Acid uses a monophonic voice per part, destination-step
legato slide, accent and a bounded original four-stage feedback filter. This is a musical
approximation, not a circuit model. Controls are 0–127 macros. Envelopes are quantized linear
ramps; modulation, effects and fine editor pages can be extended later.

Six shared drum voice structs synthesize 16 lane sounds from noise, sine/pulse oscillators,
pitch sweeps and decays. Retrigger replaces the same sound; closed hat chokes open hat;
otherwise the oldest drum voice is stolen. Drum-lane count is not simultaneous polyphony.

All audio uses fixed-point integer arithmetic and lookup tables. No heap allocation,
filesystem streaming or floating-point runtime is required. Output has conservative gain
headroom plus a final safety clamp. Controls change current filter values; no claim of
fully smoothed, click-free automation or analog accuracy is made.

## Timing

44,100 Hz stereo; target DMA halves contain 128 frames, giving **2,902 microseconds** per half.
Audio IRQ priority 3; input scanning timer priority 4 at 10 kHz. The ISR records worst render
microseconds and deadline misses in `max_audio_us` / `deadline_misses` for debugger inspection.
A zero host overrun count cannot establish a device deadline. UI updates use eight-row DMA
strips and do not mask interrupts for full frames. Input mutations use short IRQ-off sections.
Flash save stops audio DMA, writes only when transport is stopped, then zeros/restarts DMA.

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

Host `sizeof(McEngine)` is 56,212 bytes, including a 2,120-byte project. This is not total device
RAM: add storage scratch, DMA, scan state, LCD strip, libc/HAL state and stack usage. The target
link report is authoritative for static sections; stack high-water and IRQ nesting need hardware
measurement. Twelve original attack tables occupy 12,288 bytes of XIP, not a sample-memory allocation.

The [target report](target-report.json) records a 44,756-byte app, 67,648-byte globals,
2,784-byte RAM code and zero pool. Host engine state is 56,212 B, including fixed effect buffers.

## Persistence and safety

Four 16-step patterns fit one portable 2,140-byte project record. Serialization is explicit
little endian; it does not dump compiler structs. The header includes magic, version, length,
sequence, body CRC and header CRC. Decode validates all engine indexes, notes and control ranges
before modifying the live project. Sequence comparison supports rollover.

Only `0x97000..0x98FFF` is writable: two 4 KiB sectors inherited from project slot 0. A save
preserves the last valid sector, erases the other, writes payload, then writes the commit header.
No boot-time formatting or autosave occurs. Users explicitly confirm SAVE twice while stopped.
If another firmware used these sectors, this replaces its project slot 0; the UI says SLOT 0.
Calibration/global sectors `0xFC000..0xFEFFF`, tail `0xFF000`, bootloader, app/configuration,
OTA staging and sample regions are excluded by the target driver's allowlist. JEDEC must match
`0x856014` for persistence. Host fault injection checks every byte interruption; real NOR electrical
failure modes still need device validation. Save-readback errors are reported on-screen.

## Current limitations

Default panel mapping only; no imported calibration. No USB/TRS MIDI stack, firmware updater,
general song chain/editor, per-hit velocity, sample import, per-part effect sends, undo or on-panel chord editing.
Melodic steps can store three notes; live/step recording currently writes one. Per-drum LEVEL is
a kit-level control, while each drum step shares velocity. Pattern sounds are project-wide.
Muting is immediate and may click. Tone quality, note transitions and noise floor need listening
review on the actual codec. The host browser requires one active audio client per server instance.

## Master effects and built-in demo

`effects.c` implements four original integer effects: soft saturation, stereo
triangle-modulated interpolated chorus, cross-feedback tempo delay (sixteenth note,
maximum 8,191 samples), and four damped feedback combs for compact reverb. The
permanent delay/comb/chorus buffers use 53,248 bytes; counters/damping add 36 bytes.
All amounts are 0–127, feedback is bounded below unity, and the mix retains the
existing final clamp and clip counter. Bypassed effects still advance/clear their
state so switching them back on does not revive a frozen tail. No heap allocation.

`demo.c` stores original note patterns and a 32-bar performance. The same sequencer,
mutes, cutoff/resonance controls and effects execute on both targets. Automated
filters update every 64 samples; manual filter edits may be overwritten while demo
mode is enabled. Demo loading resets state only after the device stops audio DMA.
Version 2 extends the portable body by eight bytes for effects/demo settings;
legacy version 1 projects migrate to dry effects/no automation. A/B sectors and
app flash bounds are unchanged. The larger state and four effects require new
real-device timing/stack checks; host speed is not a pi32v2 benchmark.
