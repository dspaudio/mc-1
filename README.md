# MC-1

**An original 1990s-style groovebox for the M-VAVE FM-1.**

Three melodic parts, each choosing an **LA-inspired** or **303-inspired** engine, plus a **16-lane synthesized drum part**. Four patterns, live playing, step editing, recording, accent, slide, part mute, queued pattern changes and hands-on controls.

The compact dark panel, orange accents and drum grid take inspiration from the Roland MC-series workflow. MC-1 is **not a Roland emulation**, is not affiliated with Roland or M-VAVE, and does not use Roland ROMs, samples, patches, logos or compatibility formats.

> **Experimental 0.1.1:** the native host engine, sequencer, display and storage tests work. Host checks pass; target validation for each revision is recorded separately; see the [validation record](docs/VALIDATION.md). No FM-1 boot, listening, stability, control calibration or audio-deadline test has been performed. An experimental `.fwsc` package build is available; installation on hardware remains unverified. The compiled raw target app is not an installer. See [package boundaries](BUILDING.md#experimental-fwsc-package).

![Actual shared C drum screen](docs/drums.png)

[Play the FM-1 web emulator](https://dspaudio.github.io/mc-1-web/) · [Emulator source](https://github.com/dspaudio/mc-1-web)

## Try it on your computer

Requires a C11 compiler, Make and Python 3. No Python packages, SDK or audio plugins are required.

```sh
make test
make render                    # build/demo.wav, actual sequencer and DSP
python3 host/serve.py           # http://127.0.0.1:8091
```

Click **ENABLE AUDIO**, then **PLAY / STOP**. Choose **808** for the drum grid. Turn the four knobs using − / + or the wheel. Export/import `.mcf` projects with the host panel; these files include a version and CRC. Audio is locally buffered by the browser, so preview latency is not device latency. Pause audio before closing the panel. The server binds loopback only and has no hardware access.

The command-line harness also works without a browser:

```sh
make
build/mc1-host render build/demo.wav 16
build/mc1-host preset 3 build/glass-bell.wav
build/mc1-host screen build/drums.ppm 3
build/mc1-host                 # interactive command help
```

[Listen to NIGHT CIRCUIT](docs/audio/night-circuit.wav): an original host-rendered four-pattern demo at 118 BPM. It is not a recording of an FM-1 or Roland device.

## Built-in demo and effects

While stopped, press **GLO three times**, select **NEON SPIRAL** with knob 1, turn
**LOAD** (knob 4) clockwise, then press **PLAY**. Loading replaces the current
project in RAM; save/export your work first. No flash write happens until SAVE
is explicitly confirmed. NIGHT CIRCUIT reloads the original dry pattern bank.
NEON SPIRAL plays 32 bars with automatic mutes, pattern changes and cutoff/resonance
sweeps, then stops with effect tails. Loading it again restores the original mix.

Press **HOME**, then **FX** for **DELAY / REVERB / DRIVE / CHORUS**. Each knob is
0–127; zero is dry. Effects amounts and demo mode are included in project version 2;
version 1 files import with dry effects. This is a fixed built-in arrangement,
not a general song editor. Four effects can be selected together; device deadline
stability is not established until an authorized FM-1 measurement.

[Download community MP3: MC-1 — NEON SPIRAL with FX (320 kbps)](docs/audio/mc-1-neon-spiral-fx.mp3)

## Listening demos

- [NEON SPIRAL — 59-second original psy-trance study](docs/audio/neon-spiral.wav)
- [LA lead, pad and bell](docs/audio/la-showcase.wav)
- [303 cutoff and slide](docs/audio/303-showcase.wav)
- [808 synthesized drums](docs/audio/808-showcase.wav)
- [Full factory mix](docs/audio/full-mix-preview.wav)

The MC-505 factory demo video's opening chapter is labeled
[Psy Trance](https://www.youtube.com/watch?v=d9VYCMy6IOc). NEON SPIRAL explores that
broad style using an original 140 BPM composition: rolling bass, acid sequence,
LA arpeggio, drum dropout and return, with cutoff and resonance sweeps.
It does not recreate or claim a listening match to the original demo.
At 27.4–34.3 seconds the isolated acid line sweeps cutoff and resonance; at 34.3
seconds drums/bass return, and at 41.1 seconds the LA arpeggio joins.
The built-in C demo performs its own mutes, pattern changes and filter automation on
both host and firmware, stops after 32 bars and lets effects decay. All five listening files use only
peak gain to −3.01 dBFS, without external instruments or external effects. They are not
device recordings or relative-loudness comparisons. Rebuild with
`python3 tools/showcase.py`; [metrics and hashes](docs/showcase-report.json).

## What is implemented

| Area | Implementation |
|---|---|
| LA-inspired sound | Twelve original 512-sample PCM attacks, dual detuned band-limited sustain oscillators, envelope and filter |
| 303-inspired sound | Per-part mono saw/square, resonant four-stage filter, accent and continuous pitch slide |
| Drums | 16 synthesized sounds: kicks, snares, hats, clap, toms, rim, crash, ride, shaker, cowbell and claves |
| Factory bank | 48 original presets spanning bass, leads, pads, keys, organ, brass, strings, mallets and textures |
| Sequencer | Four 16-step patterns, three-note melodic steps, 16 drum lanes, 40–240 BPM, swing, gate, accent and slide |
| Performance | Part mute, queued bar-boundary pattern changes, live keyboard, quantized recording and step entry |
| Screen | Shared C RGB565 renderer; 240×240 display with a 240×8 strip buffer on device |
| Persistence | Versioned 2,140-byte record, CRC and A/B commit-last storage; explicit save only |
| Effects | Native fixed-point tempo delay, damped-comb reverb, soft distortion and stereo chorus; four master amounts |
| Built-in demo | NEON SPIRAL: original 32-bar psy-trance performance with filter automation and all four effects |
| Hardware source | Inherited GPL FM-1 startup/HAL, I2S DMA, keypad/encoders, LCD, master ADC, watchdog and guarded flash |

**Resource limits are deliberate:** eight shared melodic voices, with oldest-voice stealing, and six shared drum voices for 16 sounds. Every part can use either melodic engine; each 303 part is monophonic. These are allocator limits, **not verified real-device polyphony**. No sample streaming, SD support, flash expansion, USB audio/MIDI, external MIDI sync, general song arranger or original-patch compatibility is promised in this version.

## FM-1 panel mapping

| Control | Action |
|---|---|
| SELECT / SEL | Select part 1–3 or drums |
| ALGORITHM | Browse melodic presets (both engines); on drums, queue pattern |
| PRESETS | Queue pattern 1–4; switches at the next bar while playing |
| PLAY / REC | Start or stop / toggle quantized recording |
| ARP | Mute selected part |
| SEQ or EDIT | Toggle step-entry mode; melodic knobs become STEP / VEL / GATE / A-S |
| HOME | Return to performance controls |
| ENV | Attack / decay / sustain / release for melodic parts |
| GLO | Cycle GLOBAL MIXER → tempo / swing / pattern / part level → DEMO LIBRARY |
| LFO / FX | Accent / effects page; FX toggles slide while step editing |
| OCT− / OCT+ | Keyboard octave; hold both five seconds while stopped to enter UBOOT |
| SAVE twice, stopped | Explicitly save project to the A/B project sectors |

On the **drum screen**, the knobs always address **SOUND / STEP / HIT / LEVEL** except on GLO. HIT clockwise sets the selected hit, counterclockwise removes it. LEVEL changes that sound's kit level. Playing keys select/audition the 16 sounds; in step-entry or recording mode they toggle hits. Melodic step entry writes one note; factory chords remain playable but chord entry is not yet exposed by the panel. Record is quantized to the current step, with fixed default velocity and gate.

**Storage warning:** the first confirmed device save uses `0x97000` and `0x98000`, which are the previous firmware's project slot 0 sectors. Back up that project before using MC-1 save. Boot never formats flash, there is no autosave, and the flash driver is restricted to those two sectors. Existing calibration/global settings, samples and bootloader regions are not written. The stock/default panel mapping is used; calibration import is not implemented.

The home grid includes pitch marks for up to three notes per melodic step. Higher
marks mean higher notes, using a shared range across each part's four patterns, so
phrase changes remain visible even when their rhythms match. The drum row shows hits.
Above the home-screen knobs, **PATTERN** shows the playing pattern and **NEXT**
shows a queued pattern until the next bar begins. Save/load messages take priority.

Press **GLO once** for **GLOBAL MIXER**. The four knobs control **PART1 / PART2 /
PART3 / DRUMS** levels (0–127), regardless of the selected part. Bars show saved
level settings; muted parts are dimmed and labeled **MUTE**. Select a part and use
**ARP** to mute it. GLO cycles through mixer, groove controls and demo library;
HOME returns to performance. Levels are included when saving a project.

## Build and verification

See [BUILDING.md](BUILDING.md), [architecture and memory boundaries](docs/ARCHITECTURE.md), [validation record](docs/VALIDATION.md) and [licensing/provenance](NOTICE.md).

Code and original generated assets: **GPL-3.0-only**. Hardware support is derived from [dspaudio/orbit](https://github.com/dspaudio/orbit), tracing to SLOOP/Felucca by Leo Kuroshita and Hügelton Instruments. Restricted upstream artwork and sample packs are excluded.

<!-- BEGIN PRESETS -->
## Complete factory preset list

48 original melodic presets: 36 LA-inspired and 12 303-inspired, plus 16 synthesized drum sounds. These are MC-1 presets, not Roland patch data.

| # | Sound engine | Preset name | Family |
|---|---|---|---|
| 01 | LA-inspired | VECTOR LEAD | Lead |
| 02 | LA-inspired | ROUND BASS | Bass |
| 03 | LA-inspired | SILK PAD | Pad |
| 04 | LA-inspired | GLASS BELL | Bell |
| 05 | LA-inspired | NYLON PLUCK | Pluck |
| 06 | LA-inspired | ORBITAL MT | Texture |
| 07 | 303-inspired | ACID SAW | Bass / lead |
| 08 | 303-inspired | SQUARE RUBBER | Bass / lead |
| 09 | 303-inspired | HIGH VOLTAGE | Lead |
| 10 | 303-inspired | SOFT STEP | Lead |
| 11 | LA-inspired | REED STACK | Reed |
| 12 | LA-inspired | METAL RAIN | Texture |
| 13 | LA-inspired | DEEP CELL | Bass |
| 14 | LA-inspired | CLUB PULSE | Bass |
| 15 | LA-inspired | CLICK FLOOR | Bass |
| 16 | LA-inspired | WIRE PICK | Bass |
| 17 | LA-inspired | DUB CHAMBER | Bass |
| 18 | 303-inspired | RESONA BASS | Bass |
| 19 | 303-inspired | LIQUID STEP | Bass |
| 20 | 303-inspired | BRIGHT COIL | Lead |
| 21 | 303-inspired | HOLLOW REED | Lead |
| 22 | 303-inspired | NEEDLE LINE | Lead |
| 23 | 303-inspired | BUBBLE RUN | Sequence |
| 24 | 303-inspired | VELVET MONO | Lead |
| 25 | 303-inspired | RAZOR MONO | Lead |
| 26 | LA-inspired | WIDE SAW | Lead |
| 27 | LA-inspired | SOFT PIPE | Lead |
| 28 | LA-inspired | SEQUENCE KEY | Sequence |
| 29 | LA-inspired | POLY CHROME | Poly synth |
| 30 | LA-inspired | BRASS BLOCK | Brass |
| 31 | LA-inspired | SOFT HORNS | Brass |
| 32 | LA-inspired | STRING VEIL | Strings |
| 33 | LA-inspired | SLOW HALO | Pad |
| 34 | LA-inspired | BELL CLOUD | Pad |
| 35 | LA-inspired | DUSK SCREEN | Pad |
| 36 | LA-inspired | AIR CHAMBER | Pad |
| 37 | LA-inspired | HAMMER KEY | Keys |
| 38 | LA-inspired | SOFT TINES | Keys |
| 39 | LA-inspired | CLAVI CLICK | Keys |
| 40 | LA-inspired | DRAWBAR KEY | Organ |
| 41 | LA-inspired | PERC ORGAN | Organ |
| 42 | LA-inspired | WOOD MALLET | Mallet |
| 43 | LA-inspired | GLASS TINES | Mallet |
| 44 | LA-inspired | THUMB PLUCK | Pluck |
| 45 | LA-inspired | WIRE ZITHER | Pluck |
| 46 | LA-inspired | CHORD STAB | Stab |
| 47 | LA-inspired | SPACE SHARD | Texture |
| 48 | LA-inspired | DIGITAL MIST | Texture |

| Drum lane | Sound engine | Preset name |
|---|---|---|
| 01 | 808-inspired synthesis | KICK |
| 02 | 808-inspired synthesis | SNARE |
| 03 | 808-inspired synthesis | CLOSED HAT |
| 04 | 808-inspired synthesis | OPEN HAT |
| 05 | 808-inspired synthesis | CLAP |
| 06 | 808-inspired synthesis | LOW TOM |
| 07 | 808-inspired synthesis | KICK 2 |
| 08 | 808-inspired synthesis | RIMSHOT |
| 09 | 808-inspired synthesis | SNARE 2 |
| 10 | 808-inspired synthesis | HIGH TOM |
| 11 | 808-inspired synthesis | CRASH |
| 12 | 808-inspired synthesis | RIDE |
| 13 | 808-inspired synthesis | SHAKER |
| 14 | 808-inspired synthesis | COWBELL |
| 15 | 808-inspired synthesis | CLAVES |
| 16 | 808-inspired synthesis | MID TOM |
<!-- END PRESETS -->
