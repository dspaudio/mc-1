# MC-1

**An original 1990s-style groovebox for the M-VAVE FM-1.**

Three melodic parts, each choosing an **LA-inspired** or **303-inspired** engine from 128 presets, plus a **16-lane synthesized drum part with eight kits**. Four patterns, live playing, step editing, recording, accent, slide, part mute, queued pattern changes and hands-on controls.

The compact dark panel, orange accents and drum grid take inspiration from the Roland MC-series workflow. MC-1 is **not a Roland emulation**, is not affiliated with Roland or M-VAVE, and does not use Roland ROMs, samples, patches, logos or compatibility formats.

> **Experimental 0.1.3:** the native host engine, sequencer, display and storage tests work. Host checks pass; target validation for each revision is recorded separately; see the [validation record](docs/VALIDATION.md). No FM-1 boot, listening, stability, control calibration or audio-deadline test has been performed. An experimental `.fwsc` package build is available; installation on hardware remains unverified. The compiled raw target app is not an installer. See [package boundaries](BUILDING.md#experimental-fwsc-package).

![Actual shared C drum screen](docs/drums.png)

[Play the FM-1 web emulator](https://dspaudio.github.io/mc-1-web/) · [Emulator source](https://github.com/dspaudio/mc-1-web)

## New in 0.1.3

- 128 original melodic presets, arranged in 14 contiguous sound families. The full list and category ranges appear below.
- The previous 64 names and sound parameters are preserved; their display numbers change with the new ordering. Existing saved projects contain the actual patch and keep their sounds.
- The home screen shows the preset family and number. PRESETS browsing starts from an exact match to the current patch after loading; customized sounds retain the last browser position.
- Both built-in demos and all existing listening files retain their exact audio. Version 3 project format, 24 attack waveforms and eight drum kits remain unchanged.

## Added in 0.1.2

- 64 melodic presets and 24 original mathematical PCM attacks, adding acid sequences, house organ and keys, voice-like tones and percussion hits. The first 48 preset definitions and first 12 attacks are preserved.
- Eight original drum kits, each with 16 lanes. BASE 808 retains the original drum recipe; the other kits vary pitch, noise, sweep and decay.
- Independent **DELAY / REVERB / DRIVE / CHORUS** amounts for each melodic part and the drum track, alongside the master effects.
- **ALGORITHM** selects patterns on every part; **PRESETS** selects melodic sounds or drum kits. Project version 3 saves kits and track effects while importing version 1 and 2 projects.

## Try it on your computer

Requires a C11 compiler, Make and Python 3. No Python packages, SDK or audio plugins are required.

```sh
make test
make render                    # build/demo.wav, actual sequencer and DSP
python3 host/serve.py           # http://127.0.0.1:8091
```

Click **ENABLE AUDIO**, then **PLAY / STOP**. Choose **DRUMS** for the drum grid. Turn the four knobs using − / + or the wheel. Export/import `.mcf` projects with the host panel; these files include a version and CRC. Audio is locally buffered by the browser, so preview latency is not device latency. Pause audio before closing the panel. The server binds loopback only and has no hardware access.

The command-line harness also works without a browser:

```sh
make
build/mc1-host render build/demo.wav 16
build/mc1-host preset 104 build/glass-bell.wav
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

Press **HOME**, then **FX** for the selected track's **DELAY / REVERB / DRIVE / CHORUS**.
Each melodic part and the drum track has four independent amounts, 0–127; zero is dry.
Press **FX** again to switch to master effects; further presses alternate between
track and master effects. **HOME** exits. While step editing, FX toggles slide instead.
Project version 3 saves the selected drum kit, track effects, master effects and demo
mode. Version 1 and 2 imports select BASE 808 and zero track effects; version 2 keeps
its master effects and demo settings, while version 1 imports with dry master effects.
This is a fixed built-in arrangement,
not a general song editor. Four effects can be selected together; device deadline
stability is not established until an authorized FM-1 measurement.

[Download community MP3: MC-1 — NEON SPIRAL with FX (320 kbps)](docs/audio/mc-1-neon-spiral-fx.mp3)

## Listening demos

- [NEON SPIRAL — 59-second original psy-trance study](docs/audio/neon-spiral.wav)
- [LA lead, pad and bell](docs/audio/la-showcase.wav)
- [303 cutoff and slide](docs/audio/303-showcase.wav)
- [808 synthesized drums](docs/audio/808-showcase.wav)
- [All eight drum kits — 64-second comparison](docs/audio/kit-showcase.wav)
- [Full factory mix](docs/audio/full-mix-preview.wav)

The kit comparison follows the [kit table](#drum-kits) order, eight seconds per kit:
all 16 lanes in the first four seconds, a three-second groove, then one second of tail.

The MC-505 factory demo video's opening chapter is labeled
[Psy Trance](https://www.youtube.com/watch?v=d9VYCMy6IOc). NEON SPIRAL explores that
broad style using an original 140 BPM composition: rolling bass, acid sequence,
LA arpeggio, drum dropout and return, with cutoff and resonance sweeps.
It does not recreate or claim a listening match to the original demo.
At 27.4–34.3 seconds the isolated acid line sweeps cutoff and resonance; at 34.3
seconds drums/bass return, and at 41.1 seconds the LA arpeggio joins.
The built-in C demo performs its own mutes, pattern changes and filter automation on
both host and firmware, stops after 32 bars and lets effects decay. All six listening files use only
peak gain to −3.01 dBFS, without external instruments or external effects. They are not
device recordings or relative-loudness comparisons. Rebuild with
`python3 tools/showcase.py`; [metrics and hashes](docs/showcase-report.json).

## What is implemented

| Area | Implementation |
|---|---|
| LA-inspired sound | 24 original 512-sample PCM attacks, dual detuned band-limited sustain oscillators, envelope and filter |
| 303-inspired sound | Per-part mono saw/square, resonant four-stage filter, accent and continuous pitch slide |
| Drums | Eight original kits with 16 lanes each: kicks, snares, hats, clap, toms, rim, crash, ride, shaker, cowbell and claves |
| Factory bank | 128 original presets spanning bass, leads, sequences, pads, keys, organ, brass, strings, mallets, voice-like tones, hits and textures |
| Sequencer | Four 16-step patterns, three-note melodic steps, 16 drum lanes, 40–240 BPM, swing, gate, accent and slide |
| Performance | Part mute, queued bar-boundary pattern changes, live keyboard, quantized recording and step entry |
| Screen | Shared C RGB565 renderer; 240×240 display with a 240×8 strip buffer on device |
| Persistence | Version 3, 2,156-byte record, CRC and A/B commit-last storage; version 1/2 import; explicit save only |
| Effects | Native fixed-point delay, reverb, drive and chorus; four independent amounts on each of four tracks plus four master amounts |
| Built-in demo | NEON SPIRAL: original 32-bar psy-trance performance with filter automation and all four effects |
| Hardware source | Inherited GPL FM-1 startup/HAL, I2S DMA, keypad/encoders, LCD, master ADC, watchdog and guarded flash |

**Resource limits are deliberate:** eight shared melodic voices, with oldest-voice stealing, and six shared drum voices for 16 sounds. Every part can use either melodic engine; each 303 part is monophonic. These are allocator limits, **not verified real-device polyphony**. No sample streaming, SD support, flash expansion, USB audio/MIDI, external MIDI sync, general song arranger or original-patch compatibility is promised in this version.

## FM-1 panel mapping

| Control | Action |
|---|---|
| SELECT / SEL | Select part 1–3 or drums |
| ALGORITHM | Queue pattern 1–4 on every part; switches at the next bar while playing |
| PRESETS | Browse melodic presets (both engines), or select one of eight kits on drums |
| PLAY / REC | Start or stop / toggle quantized recording |
| ARP | Mute selected part |
| SEQ or EDIT | Toggle step-entry mode; melodic knobs become STEP / VEL / GATE / A-S |
| HOME | Return to performance controls |
| ENV | Attack / decay / sustain / release for melodic parts |
| GLO | Cycle GLOBAL MIXER → tempo / swing / pattern / part level → DEMO LIBRARY |
| LFO / FX | Accent / selected-track effects; repeated FX alternates track and master effects; FX toggles slide while step editing |
| OCT− / OCT+ | Keyboard octave; hold both five seconds while stopped to enter UBOOT |
| SAVE twice, stopped | Explicitly save project to the A/B project sectors |

On the **drum screen**, the knobs address **SOUND / STEP / HIT / LEVEL**; GLO and FX open their own controls. Turn **PRESETS** to select the kit shown in the header. HIT clockwise sets the selected hit, counterclockwise removes it. LEVEL changes that sound's kit level. Playing keys select/audition the 16 sounds; in step-entry or recording mode they toggle hits. Melodic step entry writes one note; factory chords remain playable but chord entry is not yet exposed by the panel. Record is quantized to the current step, with fixed default velocity and gate.

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

128 original melodic presets: 112 LA-inspired and 16 303-inspired, plus eight original synthesized drum kits with 16 lanes each. These are MC-1 presets, not Roland patch data.

The [official MC-505 patch listing (MC505PTC.pdf)](https://cdn.roland.com/assets/media/pdf/MC505PTC.pdf) informed broad sound families such as acid sequences, house keys and organs, voice-like tones and percussion hits. MC-1 uses original names, parameters and mathematical attacks; no Roland patches, ROMs or samples are copied.

The bank is ordered by category. All 64 previous names, patch values and family descriptions are preserved; their displayed numbers change to fit these groups. The 24 original PCM attacks and eight drum kits are unchanged.

### Category index

| Category | Presets | Count |
|---|---|---|
| Bass | 001–012 | 12 |
| Acid | 013–028 | 16 |
| Lead | 029–040 | 12 |
| Sequence | 041–050 | 10 |
| Keys | 051–060 | 10 |
| Organ | 061–068 | 8 |
| Brass/Reed | 069–076 | 8 |
| Strings | 077–082 | 6 |
| Pad | 083–094 | 12 |
| Pluck | 095–104 | 10 |
| Mallet/Bell | 105–112 | 8 |
| Voice | 113–118 | 6 |
| Hit/Percussion | 119–123 | 5 |
| Texture | 124–128 | 5 |

| # | Category | Sound engine | Preset name | Family |
|---|---|---|---|---|
| 001 | Bass | LA-inspired | ROUND BASS | Bass |
| 002 | Bass | LA-inspired | DEEP CELL | Bass |
| 003 | Bass | LA-inspired | CLUB PULSE | Bass |
| 004 | Bass | LA-inspired | CLICK FLOOR | Bass |
| 005 | Bass | LA-inspired | WIRE PICK | Bass |
| 006 | Bass | LA-inspired | DUB CHAMBER | Bass |
| 007 | Bass | LA-inspired | SUB ANCHOR | Bass |
| 008 | Bass | LA-inspired | PLUSH OCTAVE | Bass |
| 009 | Bass | LA-inspired | GRIT PULSE | Bass |
| 010 | Bass | LA-inspired | PLUCKED SUB | Bass |
| 011 | Bass | LA-inspired | GLASS FLOOR | Bass |
| 012 | Bass | LA-inspired | HOLLOW ROOT | Bass |
| 013 | Acid | 303-inspired | ACID SAW | Bass / lead |
| 014 | Acid | 303-inspired | SQUARE RUBBER | Bass / lead |
| 015 | Acid | 303-inspired | HIGH VOLTAGE | Lead |
| 016 | Acid | 303-inspired | SOFT STEP | Lead |
| 017 | Acid | 303-inspired | RESONA BASS | Bass |
| 018 | Acid | 303-inspired | LIQUID STEP | Bass |
| 019 | Acid | 303-inspired | BRIGHT COIL | Lead |
| 020 | Acid | 303-inspired | HOLLOW REED | Lead |
| 021 | Acid | 303-inspired | NEEDLE LINE | Lead |
| 022 | Acid | 303-inspired | BUBBLE RUN | Sequence |
| 023 | Acid | 303-inspired | VELVET MONO | Lead |
| 024 | Acid | 303-inspired | RAZOR MONO | Lead |
| 025 | Acid | 303-inspired | ACID RIBBON | Bass / lead |
| 026 | Acid | 303-inspired | COIL WALK | Acid sequence |
| 027 | Acid | 303-inspired | SQUELCH GRID | Acid sequence |
| 028 | Acid | 303-inspired | RUBBER CHASE | Acid sequence |
| 029 | Lead | LA-inspired | VECTOR LEAD | Lead |
| 030 | Lead | LA-inspired | WIDE SAW | Lead |
| 031 | Lead | LA-inspired | SOFT PIPE | Lead |
| 032 | Lead | LA-inspired | POLY CHROME | Poly synth |
| 033 | Lead | LA-inspired | COPPER EDGE | Lead |
| 034 | Lead | LA-inspired | SOLO ARC | Lead |
| 035 | Lead | LA-inspired | NEON FLUTE | Lead |
| 036 | Lead | LA-inspired | SAW CANOPY | Lead |
| 037 | Lead | LA-inspired | PULSE TORCH | Lead |
| 038 | Lead | LA-inspired | GLASS REED | Lead |
| 039 | Lead | LA-inspired | SOFT COMET | Lead |
| 040 | Lead | LA-inspired | BRIGHT VECTOR | Lead |
| 041 | Sequence | LA-inspired | SEQUENCE KEY | Sequence |
| 042 | Sequence | LA-inspired | SPARK SEQ | Sequence |
| 043 | Sequence | LA-inspired | RATCHET PLUCK | Sequence |
| 044 | Sequence | LA-inspired | CLOCK BLOOM | Sequence |
| 045 | Sequence | LA-inspired | PINWHEEL | Sequence |
| 046 | Sequence | LA-inspired | CRYSTAL GATE | Sequence |
| 047 | Sequence | LA-inspired | WOODEN STEP | Sequence |
| 048 | Sequence | LA-inspired | LANTERN RUN | Sequence |
| 049 | Sequence | LA-inspired | SILVER TICK | Sequence |
| 050 | Sequence | LA-inspired | POCKET PULSE | Sequence |
| 051 | Keys | LA-inspired | HAMMER KEY | Keys |
| 052 | Keys | LA-inspired | SOFT TINES | Keys |
| 053 | Keys | LA-inspired | CLAVI CLICK | Keys |
| 054 | Keys | LA-inspired | VELVET CHORD | House keys |
| 055 | Keys | LA-inspired | AMBER PIANO | Keys |
| 056 | Keys | LA-inspired | PRISM KEY | Keys |
| 057 | Keys | LA-inspired | FELT GLOW | Keys |
| 058 | Keys | LA-inspired | DIGITAL IVORY | Keys |
| 059 | Keys | LA-inspired | CHORDED TINE | Keys |
| 060 | Keys | LA-inspired | SOFT HAMMER | Keys |
| 061 | Organ | LA-inspired | DRAWBAR KEY | Organ |
| 062 | Organ | LA-inspired | PERC ORGAN | Organ |
| 063 | Organ | LA-inspired | CELLAR ORGAN | House organ |
| 064 | Organ | LA-inspired | DUSK DRAWBAR | Organ |
| 065 | Organ | LA-inspired | CHAPEL PULSE | Organ |
| 066 | Organ | LA-inspired | CLICK REED | Organ |
| 067 | Organ | LA-inspired | SOUL REGISTER | Organ |
| 068 | Organ | LA-inspired | BRIGHT MANUAL | Organ |
| 069 | Brass/Reed | LA-inspired | REED STACK | Reed |
| 070 | Brass/Reed | LA-inspired | BRASS BLOCK | Brass |
| 071 | Brass/Reed | LA-inspired | SOFT HORNS | Brass |
| 072 | Brass/Reed | LA-inspired | COPPER SECTION | Brass/Reed |
| 073 | Brass/Reed | LA-inspired | MELLOW STACK | Brass/Reed |
| 074 | Brass/Reed | LA-inspired | REED CHAMBER | Brass/Reed |
| 075 | Brass/Reed | LA-inspired | SHARP FANFARE | Brass/Reed |
| 076 | Brass/Reed | LA-inspired | LOW HORN | Brass/Reed |
| 077 | Strings | LA-inspired | STRING VEIL | Strings |
| 078 | Strings | LA-inspired | ROSIN VEIL | Strings |
| 079 | Strings | LA-inspired | CHAMBER LINE | Strings |
| 080 | Strings | LA-inspired | LOW ENSEMBLE | Strings |
| 081 | Strings | LA-inspired | SILVER BOW | Strings |
| 082 | Strings | LA-inspired | WARM OCTAVES | Strings |
| 083 | Pad | LA-inspired | SILK PAD | Pad |
| 084 | Pad | LA-inspired | SLOW HALO | Pad |
| 085 | Pad | LA-inspired | BELL CLOUD | Pad |
| 086 | Pad | LA-inspired | DUSK SCREEN | Pad |
| 087 | Pad | LA-inspired | AIR CHAMBER | Pad |
| 088 | Pad | LA-inspired | CLOUD ATRIUM | Pad |
| 089 | Pad | LA-inspired | WARM HORIZON | Pad |
| 090 | Pad | LA-inspired | LUNAR GLASS | Pad |
| 091 | Pad | LA-inspired | VELVET AIR | Pad |
| 092 | Pad | LA-inspired | SUNSET CHOIR | Pad |
| 093 | Pad | LA-inspired | FAINT AURORA | Pad |
| 094 | Pad | LA-inspired | SLOW CURRENT | Pad |
| 095 | Pluck | LA-inspired | NYLON PLUCK | Pluck |
| 096 | Pluck | LA-inspired | THUMB PLUCK | Pluck |
| 097 | Pluck | LA-inspired | WIRE ZITHER | Pluck |
| 098 | Pluck | LA-inspired | COPPER STRING | Pluck |
| 099 | Pluck | LA-inspired | RAIN PICK | Pluck |
| 100 | Pluck | LA-inspired | NYLON DUSK | Pluck |
| 101 | Pluck | LA-inspired | CRYSTAL THREAD | Pluck |
| 102 | Pluck | LA-inspired | WOODEN THUMB | Pluck |
| 103 | Pluck | LA-inspired | SOFT ZITHER | Pluck |
| 104 | Pluck | LA-inspired | SPRUCE SNAP | Pluck |
| 105 | Mallet/Bell | LA-inspired | GLASS BELL | Bell |
| 106 | Mallet/Bell | LA-inspired | WOOD MALLET | Mallet |
| 107 | Mallet/Bell | LA-inspired | GLASS TINES | Mallet |
| 108 | Mallet/Bell | LA-inspired | AMBER BELL | Mallet/Bell |
| 109 | Mallet/Bell | LA-inspired | HOLLOW BAR | Mallet/Bell |
| 110 | Mallet/Bell | LA-inspired | CHIME DROPS | Mallet/Bell |
| 111 | Mallet/Bell | LA-inspired | BRONZE PIN | Mallet/Bell |
| 112 | Mallet/Bell | LA-inspired | ROUND MARIMBA | Mallet/Bell |
| 113 | Voice | LA-inspired | VOWEL CHIME | Voice-like |
| 114 | Voice | LA-inspired | BREATH STACK | Voice-like |
| 115 | Voice | LA-inspired | CHOIR FLICK | Voice-like |
| 116 | Voice | LA-inspired | OPEN VOWEL | Voice |
| 117 | Voice | LA-inspired | SOFT CHOIR | Voice |
| 118 | Voice | LA-inspired | ROBOT HUM | Voice |
| 119 | Hit/Percussion | LA-inspired | CHORD STAB | Stab |
| 120 | Hit/Percussion | LA-inspired | BRASS IMPACT | Hit |
| 121 | Hit/Percussion | LA-inspired | TIMBER HIT | Percussion |
| 122 | Hit/Percussion | LA-inspired | TUBE STRIKE | Percussion |
| 123 | Hit/Percussion | LA-inspired | GLASS IMPACT | Hit/Percussion |
| 124 | Texture | LA-inspired | ORBITAL MT | Texture |
| 125 | Texture | LA-inspired | METAL RAIN | Texture |
| 126 | Texture | LA-inspired | SPACE SHARD | Texture |
| 127 | Texture | LA-inspired | DIGITAL MIST | Texture |
| 128 | Texture | LA-inspired | QUARTZ DRIFT | Texture |

### Drum kits

Each kit keeps the same 16 lane roles below and supplies its own original pitch, noise, sweep and decay recipe. Kits share six drum voices; 128 lane variations do not imply 128 simultaneous voices.

| # | Kit name | Character |
|---|---|---|
| 01 | BASE 808 | Original default kit: deep kicks, noise snares, hats, cymbals and pitched percussion. |
| 02 | CLUB 909 | Higher kick tuning, stronger pitch sweep and more noise in the snares for club grooves. |
| 03 | TIGHT 606 | Higher tuning and shorter decays for compact, tight drum patterns. |
| 04 | VINTAGE | Lower tuning, gentler sweeps and reduced snare noise for a softer vintage character. |
| 05 | HOUSE | Longer kick, open-hat and clap decays for house grooves. |
| 06 | BREAK JUNGLE | Bright tuning, pronounced sweeps and noisy snares for breakbeat and jungle patterns. |
| 07 | IRON WORKS | Low tuning, long decays and strong pitch sweeps for heavy metallic textures. |
| 08 | ELECTRO LATIN | High tom and percussion tuning with shorter clicks for electro and Latin patterns. |

[Listen to all eight kits](docs/audio/kit-showcase.wav): 64 seconds in the table order, eight seconds per kit. Each segment plays all 16 lanes during the first four seconds, then a three-second groove and one second of tail.

| Drum lane | Sound engine | Preset name |
|---|---|---|
| 01 | Original drum synthesis | KICK |
| 02 | Original drum synthesis | SNARE |
| 03 | Original drum synthesis | CLOSED HAT |
| 04 | Original drum synthesis | OPEN HAT |
| 05 | Original drum synthesis | CLAP |
| 06 | Original drum synthesis | LOW TOM |
| 07 | Original drum synthesis | KICK 2 |
| 08 | Original drum synthesis | RIMSHOT |
| 09 | Original drum synthesis | SNARE 2 |
| 10 | Original drum synthesis | HIGH TOM |
| 11 | Original drum synthesis | CRASH |
| 12 | Original drum synthesis | RIDE |
| 13 | Original drum synthesis | SHAKER |
| 14 | Original drum synthesis | COWBELL |
| 15 | Original drum synthesis | CLAVES |
| 16 | Original drum synthesis | MID TOM |
<!-- END PRESETS -->
