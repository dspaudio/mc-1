# MC-1

**An original 1990s-style groovebox for the M-VAVE FM-1.**

Three melodic parts, each choosing an **LA-inspired** or **303-inspired** engine from 128 presets, plus a **16-lane synthesized drum part with eight kits**. Four patterns, live playing, step editing, recording, accent, slide, part mute, queued pattern changes and hands-on controls.

The compact dark panel, orange selections, bright-green playback indicators and drum grid take inspiration from the Roland MC-series workflow. MC-1 is **not a Roland emulation**, is not affiliated with Roland or M-VAVE, and does not use Roland ROMs, samples, patches, logos or compatibility formats.

> **Experimental 0.1.4:** the native host engine, sequencer, display and storage tests work. Host checks pass; target validation for each revision is recorded separately; see the [validation record](docs/VALIDATION.md). No FM-1 boot, listening, stability, control calibration or audio-deadline test has been performed. An experimental `.fwsc` package build is available; installation on hardware remains unverified. The compiled raw target app is not an installer. See [package boundaries](BUILDING.md#experimental-fwsc-package).

![Actual shared C drum screen](docs/drums.png)

[Play the FM-1 web emulator](https://dspaudio.github.io/mc-1-web/) · [Emulator source](https://github.com/dspaudio/mc-1-web)

## New in 0.1.4

- Four delay types, four reverb types, four drive types and three modulation types, plus independent HPF/LPF on every track and the master.
- Six arpeggiator modes with rate, octave, gate and latch controls; PLAY + REC writes the generated notes into the sequencer.
- PLAY + REC also records sound, level and FX knob motion as step automation (384 events per project). Modified patches retain their original preset name with `*`.
- Bright green shows active playback and enabled ARP; orange remains the selection color.
- Turn encoders slowly for single steps; rapid same-direction turns accelerate larger parameter ranges. Parts, patterns, kits and other small choices remain precise.
- Turning **PRESETS** opens a seven-row sound or drum-kit list. The sound changes immediately; the list closes 500 ms after the last turn or when another control is used.

## Added in 0.1.3

- 128 melodic presets in 14 contiguous families: the previous 64 original synthesis patches plus 64 patches using 16 real instrument recordings from CC0 VSCO 2 Community Edition. The full list and source of each sound appear below.
- The previous 64 names and sound parameters are preserved; their display numbers change with the new ordering. Existing saved projects contain the actual patch and keep their sounds.
- The home screen shows the preset family and number. PRESETS browsing starts from an exact match to the current patch after loading; customized sounds retain the last browser position.
- Both built-in demos and all existing listening files retain their exact audio. Version 3 project format, 24 original attack waveforms and eight drum kits remain unchanged. New recorded patches use PCM mix 127 by default; lowering it blends in the oscillator.
- Earlier saved projects retain their sounds. Projects using new PCM IDs 24–39 cannot load in older firmware, whose validator rejects those IDs.

## Added in 0.1.2

- 64 melodic presets and 24 original mathematical PCM attacks, adding acid sequences, house organ and keys, voice-like tones and percussion hits. The first 48 preset definitions and first 12 attacks are preserved.
- Eight original drum kits, each with 16 lanes. BASE 808 retains the original drum recipe; the other kits vary pitch, noise, sweep and decay.
- Independent **DELAY / REVERB / DRIVE / CHORUS** amounts for each melodic part and the drum track, alongside the master effects.
- **ALGORITHM** selects patterns on every part; **PRESETS** selects melodic sounds or drum kits. Project version 3 saves kits and track effects while importing version 1 and 2 projects.

## Try it on your computer

Requires a C11 compiler, Make and Python 3. Normal builds use checked-in PCM tables. Regenerating recorded PCM requires Python 3.11+ (`python3.11 tools/gen_recorded_pcm.py`). No Python packages, SDK or audio plugins are required.

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

Press **HOME**, then **FX** for the selected track's effects. Press **FX** again to
switch between track and master. **ENV** cycles **AMOUNTS → TYPES → FILTERS**.
The four amount/type knobs follow **DELAY / REVERB / DRIVE / MOD**. Filter knob 1
controls HPF and knob 2 controls LPF; zero bypasses each filter. Increasing HPF
removes more low frequencies; increasing LPF removes more high frequencies.

| Family | Types |
|---|---|
| Delay | DIGITAL, TAPE, PINGPONG, MULTITAP |
| Reverb | ROOM, HALL, PLATE, SPRING |
| Drive | SOFT, HARD, FOLD, FUZZ |
| Modulation | CHORUS, PHASER, FLANGER |

Amounts are 0–127 and zero is dry. These are compact original fixed-point effects,
not models of specific hardware. Type changes reset the changed effect family's state.
The original zero-type, filter-bypassed configuration preserves all earlier demos.
Project version 4 saves types, filters, ARP settings, preset origins and automation,
as well as earlier settings. Version 1–3 imports keep their original sounds and
receive disabled ARP, empty automation, original effect types and bypassed filters.
Earlier firmware rejects version 4 files. Hardware deadline stability remains unverified.

## Arpeggiator and knob recording

Outside GLOBAL MIXER, **ARP** opens the selected melodic part's arpeggiator; press
it again or HOME to leave. The four knobs select **MODE / RATE / OCT / GATE**.
MODE cycles OFF, UP, DOWN, UP-DOWN, ORDER, RANDOM and CHORD. Rates are quarter,
eighth and sixteenth notes; octave range is 1–3 and gate is 1–100%. **LFO** toggles
latch. Changing mode or latch releases the current arp notes; play the keys again.
Up to 32 held notes are tracked; CHORD renders up to three notes per step, within
the existing shared eight-voice and monophonic-acid limits. Drums show an explanatory
page. In GLOBAL MIXER, ARP keeps its original selected-part mute function.

With **PLAY + REC**, held chords are expanded into generated sequencer notes;
long gates use continuation steps. Turn REC off and release the keys to hear the
recorded phrase. Ordinary melodic step entry still writes one note. While PLAY + REC
is active, sound/envelope, mixer levels, drum-lane levels and FX amount/type/filter
knobs record their final value at the current sixteenth-note step. Repeated changes
to the same parameter at that step replace its value. Step edits, tempo/swing and
ARP setup controls are settings rather than knob automation.

**GLO four times** opens KNOB AUTOMATION, showing the shared **384-event** capacity,
current-pattern count and REC state. **EDIT** clears only that pattern's automation.
At capacity, existing events can still be overwritten; new events show AUTOMATION
FULL and do not overwrite unrelated events. SAVE retains the performance data in a
3,746-byte version 4 record. HOME shows the stored source preset name followed by
`*` when its patch differs; imported custom sounds without a known origin retain
IMPORTED SOUND. Preset browsing continues from the stored origin, even after edits.

[Download community MP3: MC-1 — NEON SPIRAL with FX (320 kbps)](docs/audio/mc-1-neon-spiral-fx.mp3)

## Listening demos

- [NEON SPIRAL — 59-second original psy-trance study](docs/audio/neon-spiral.wav)
- [LA lead, pad and bell](docs/audio/la-showcase.wav)
- [303 cutoff and slide](docs/audio/303-showcase.wav)
- [808 synthesized drums](docs/audio/808-showcase.wav)
- [All eight drum kits — 64-second comparison](docs/audio/kit-showcase.wav)
- [Full factory mix](docs/audio/full-mix-preview.wav)
- [16 recorded instruments — 48-second comparison](docs/audio/recorded-showcase.wav)

The recorded comparison uses one preset per recording at MIDI note 60, three seconds each,
in manifest order: piano, organ, marimba, glockenspiel, xylophone, harp, violin, viola,
cello, contrabass, flute, oboe, clarinet, bassoon, trumpet and French horn. These are
compact mono excerpts with sustain loops, not full multisampled instruments.

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
both host and firmware, stops after 32 bars and lets effects decay. All seven listening files use only
peak gain to −3.01 dBFS, without external instruments or external effects. They are not
device recordings or relative-loudness comparisons. Rebuild with
`python3 tools/showcase.py`; [metrics and hashes](docs/showcase-report.json).

## What is implemented

| Area | Implementation |
|---|---|
| LA-inspired sound | 24 original 512-sample attacks or 16 recorded PCM sources with pitch-aware anti-aliasing and sustain loops; dual detuned oscillators, envelope and filter |
| 303-inspired sound | Per-part mono saw/square, resonant four-stage filter, accent and continuous pitch slide |
| Drums | Eight original kits with 16 lanes each: kicks, snares, hats, clap, toms, rim, crash, ride, shaker, cowbell and claves |
| Factory bank | 64 original synthesis and 64 recorded-PCM presets spanning bass, leads, sequences, pads, keys, organ, brass, strings, mallets, voice-like tones, hits and textures |
| Sequencer | Four 16-step patterns, three-note melodic steps, 16 drum lanes, 40–240 BPM, swing, gate, accent and slide |
| Performance | Part mute, queued pattern changes, six-mode ARP with generated-note recording, step entry and 384-event knob automation |
| Screen | Shared C RGB565 renderer; 240×240 display with a 240×8 strip buffer on device |
| Persistence | Version 4, 3,746-byte record, CRC and A/B commit-last storage; version 1–3 import; explicit save only |
| Effects | Four delay/reverb/drive types, three modulation types and HPF/LPF; independent track and master amounts |
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
| ARP | Open/close arpeggiator; in GLOBAL MIXER, mute selected part |
| SEQ or EDIT | Toggle step entry; on KNOB AUTOMATION, EDIT clears current-pattern events |
| HOME | Return to performance controls |
| ENV | Melodic envelope; on FX, cycle AMOUNTS / TYPES / FILTERS |
| GLO | Cycle GLOBAL MIXER → GROOVE → DEMO LIBRARY → KNOB AUTOMATION |
| LFO / FX | Accent (LATCH on ARP page) / effects; FX alternates track and master, or toggles slide in step entry |
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
**ARP** to mute it. GLO cycles through mixer, groove, demo library and automation;
HOME returns to performance. Levels are included when saving a project.

## Build and verification

See [BUILDING.md](BUILDING.md), [architecture and memory boundaries](docs/ARCHITECTURE.md), [validation record](docs/VALIDATION.md) and [licensing/provenance](NOTICE.md).

Code and original generated assets: **GPL-3.0-only**. The 16 VSCO recordings and converted PCM arrays are **CC0-1.0**; see [provenance](NOTICE.md) and [sample manifest](assets/recorded/manifest.json). Hardware support is derived from [dspaudio/orbit](https://github.com/dspaudio/orbit), tracing to SLOOP/Felucca by Leo Kuroshita and Hügelton Instruments. Restricted upstream artwork and sample packs are excluded.

<!-- BEGIN PRESETS -->
## Complete factory preset list

128 melodic presets: 64 original synthesis presets and 64 presets based on real instrument recordings. The engine allocation is 112 LA-inspired and 16 303-inspired, plus eight original synthesized drum kits with 16 lanes each. These are MC-1 presets, not Roland patch data.

The [official MC-505 patch listing (MC505PTC.pdf)](https://cdn.roland.com/assets/media/pdf/MC505PTC.pdf) informed broad sound families such as acid sequences, house keys and organs, voice-like tones and percussion hits. MC-1 uses original patch names and parameters; no Roland patches, ROMs or samples are copied.

The bank is ordered by category. All 64 earlier synthesis names, patch values and family descriptions are preserved. The 24 original mathematical PCM attacks and eight drum kits are unchanged. The 64 additions use 16 CC0 VSCO instrument recordings, with PCM mix set to 127 for recorded-only playback by default. Filter and envelope variations provide distinct presets from each recording. These compact excerpts and sustain loops are instrument sketches rather than full multisampled instruments. The Voice category additions process flute, oboe and clarinet recordings; they are not human vocal recordings.

The Source column identifies the actual recorded instrument. See the [recording manifest](assets/recorded/manifest.json) for source files and processing, and the [CC0 source license](LICENSES/CC0-VSCO.txt).

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

| # | Category | Sound engine | Preset name | Family | Source |
|---|---|---|---|---|---|
| 001 | Bass | LA-inspired | ROUND BASS | Bass | original synthesis |
| 002 | Bass | LA-inspired | DEEP CELL | Bass | original synthesis |
| 003 | Bass | LA-inspired | CLUB PULSE | Bass | original synthesis |
| 004 | Bass | LA-inspired | CLICK FLOOR | Bass | original synthesis |
| 005 | Bass | LA-inspired | WIRE PICK | Bass | original synthesis |
| 006 | Bass | LA-inspired | DUB CHAMBER | Bass | original synthesis |
| 007 | Bass | LA-inspired | CONTRABASS ROOT | Bass | recorded PCM: Contrabass |
| 008 | Bass | LA-inspired | CONTRABASS ROUND | Bass | recorded PCM: Contrabass |
| 009 | Bass | LA-inspired | CONTRABASS SNAP | Bass | recorded PCM: Contrabass |
| 010 | Bass | LA-inspired | CELLO LOW BOW | Bass | recorded PCM: Cello section |
| 011 | Bass | LA-inspired | CELLO BASS PULSE | Bass | recorded PCM: Cello section |
| 012 | Bass | LA-inspired | BASSOON ROOT | Bass | recorded PCM: Bassoon |
| 013 | Acid | 303-inspired | ACID SAW | Bass / lead | original synthesis |
| 014 | Acid | 303-inspired | SQUARE RUBBER | Bass / lead | original synthesis |
| 015 | Acid | 303-inspired | HIGH VOLTAGE | Lead | original synthesis |
| 016 | Acid | 303-inspired | SOFT STEP | Lead | original synthesis |
| 017 | Acid | 303-inspired | RESONA BASS | Bass | original synthesis |
| 018 | Acid | 303-inspired | LIQUID STEP | Bass | original synthesis |
| 019 | Acid | 303-inspired | BRIGHT COIL | Lead | original synthesis |
| 020 | Acid | 303-inspired | HOLLOW REED | Lead | original synthesis |
| 021 | Acid | 303-inspired | NEEDLE LINE | Lead | original synthesis |
| 022 | Acid | 303-inspired | BUBBLE RUN | Sequence | original synthesis |
| 023 | Acid | 303-inspired | VELVET MONO | Lead | original synthesis |
| 024 | Acid | 303-inspired | RAZOR MONO | Lead | original synthesis |
| 025 | Acid | 303-inspired | ACID RIBBON | Bass / lead | original synthesis |
| 026 | Acid | 303-inspired | COIL WALK | Acid sequence | original synthesis |
| 027 | Acid | 303-inspired | SQUELCH GRID | Acid sequence | original synthesis |
| 028 | Acid | 303-inspired | RUBBER CHASE | Acid sequence | original synthesis |
| 029 | Lead | LA-inspired | VECTOR LEAD | Lead | original synthesis |
| 030 | Lead | LA-inspired | WIDE SAW | Lead | original synthesis |
| 031 | Lead | LA-inspired | SOFT PIPE | Lead | original synthesis |
| 032 | Lead | LA-inspired | POLY CHROME | Poly synth | original synthesis |
| 033 | Lead | LA-inspired | TRUMPET LEAD | Lead | recorded PCM: Trumpet |
| 034 | Lead | LA-inspired | HORN SOLO | Lead | recorded PCM: French horn |
| 035 | Lead | LA-inspired | FLUTE LEAD | Lead | recorded PCM: Flute |
| 036 | Lead | LA-inspired | FLUTE SOFT ARC | Lead | recorded PCM: Flute |
| 037 | Lead | LA-inspired | OBOE BRIGHT | Lead | recorded PCM: Oboe |
| 038 | Lead | LA-inspired | OBOE SOLO | Lead | recorded PCM: Oboe |
| 039 | Lead | LA-inspired | CLARINET ARC | Lead | recorded PCM: Clarinet |
| 040 | Lead | LA-inspired | CLARINET BRIGHT | Lead | recorded PCM: Clarinet |
| 041 | Sequence | LA-inspired | SEQUENCE KEY | Sequence | original synthesis |
| 042 | Sequence | LA-inspired | SPARK SEQ | Sequence | original synthesis |
| 043 | Sequence | LA-inspired | RATCHET PLUCK | Sequence | original synthesis |
| 044 | Sequence | LA-inspired | PIANO STEPS | Sequence | recorded PCM: Piano |
| 045 | Sequence | LA-inspired | MARIMBA RUN | Sequence | recorded PCM: Marimba |
| 046 | Sequence | LA-inspired | GLOCK GRID | Sequence | recorded PCM: Glockenspiel |
| 047 | Sequence | LA-inspired | XYLO WALK | Sequence | recorded PCM: Xylophone |
| 048 | Sequence | LA-inspired | HARP PATTERN | Sequence | recorded PCM: Harp |
| 049 | Sequence | LA-inspired | CLARINET GATE | Sequence | recorded PCM: Clarinet |
| 050 | Sequence | LA-inspired | VIOLIN PULSE | Sequence | recorded PCM: Solo violin |
| 051 | Keys | LA-inspired | HAMMER KEY | Keys | original synthesis |
| 052 | Keys | LA-inspired | SOFT TINES | Keys | original synthesis |
| 053 | Keys | LA-inspired | CLAVI CLICK | Keys | original synthesis |
| 054 | Keys | LA-inspired | VELVET CHORD | House keys | original synthesis |
| 055 | Keys | LA-inspired | AMBER PIANO | Keys | original synthesis |
| 056 | Keys | LA-inspired | PRISM KEY | Keys | original synthesis |
| 057 | Keys | LA-inspired | PIANO FELT | Keys | recorded PCM: Piano |
| 058 | Keys | LA-inspired | PIANO BRIGHT | Keys | recorded PCM: Piano |
| 059 | Keys | LA-inspired | PIANO ROOM | Keys | recorded PCM: Piano |
| 060 | Keys | LA-inspired | PIANO SHORT | Keys | recorded PCM: Piano |
| 061 | Organ | LA-inspired | DRAWBAR KEY | Organ | original synthesis |
| 062 | Organ | LA-inspired | PERC ORGAN | Organ | original synthesis |
| 063 | Organ | LA-inspired | CELLAR ORGAN | House organ | original synthesis |
| 064 | Organ | LA-inspired | ORGAN MELLOW | Organ | recorded PCM: Organ |
| 065 | Organ | LA-inspired | ORGAN CHAPEL | Organ | recorded PCM: Organ |
| 066 | Organ | LA-inspired | ORGAN PERC | Organ | recorded PCM: Organ |
| 067 | Organ | LA-inspired | ORGAN FULL | Organ | recorded PCM: Organ |
| 068 | Organ | LA-inspired | ORGAN BRIGHT | Organ | recorded PCM: Organ |
| 069 | Brass/Reed | LA-inspired | REED STACK | Reed | original synthesis |
| 070 | Brass/Reed | LA-inspired | BRASS BLOCK | Brass | original synthesis |
| 071 | Brass/Reed | LA-inspired | SOFT HORNS | Brass | original synthesis |
| 072 | Brass/Reed | LA-inspired | TRUMPET SECTION | Brass/Reed | recorded PCM: Trumpet |
| 073 | Brass/Reed | LA-inspired | HORN MELLOW | Brass/Reed | recorded PCM: French horn |
| 074 | Brass/Reed | LA-inspired | BASSOON REED | Brass/Reed | recorded PCM: Bassoon |
| 075 | Brass/Reed | LA-inspired | TRUMPET FANFARE | Brass/Reed | recorded PCM: Trumpet |
| 076 | Brass/Reed | LA-inspired | HORN LOW | Brass/Reed | recorded PCM: French horn |
| 077 | Strings | LA-inspired | STRING VEIL | Strings | original synthesis |
| 078 | Strings | LA-inspired | VIOLIN BOW | Strings | recorded PCM: Solo violin |
| 079 | Strings | LA-inspired | VIOLA CHAMBER | Strings | recorded PCM: Viola section |
| 080 | Strings | LA-inspired | CELLO ENSEMBLE | Strings | recorded PCM: Cello section |
| 081 | Strings | LA-inspired | VIOLIN SILVER | Strings | recorded PCM: Solo violin |
| 082 | Strings | LA-inspired | CONTRABASS BOW | Strings | recorded PCM: Contrabass |
| 083 | Pad | LA-inspired | SILK PAD | Pad | original synthesis |
| 084 | Pad | LA-inspired | SLOW HALO | Pad | original synthesis |
| 085 | Pad | LA-inspired | BELL CLOUD | Pad | original synthesis |
| 086 | Pad | LA-inspired | DUSK SCREEN | Pad | original synthesis |
| 087 | Pad | LA-inspired | AIR CHAMBER | Pad | original synthesis |
| 088 | Pad | LA-inspired | PIANO CLOUD | Pad | recorded PCM: Piano |
| 089 | Pad | LA-inspired | ORGAN HORIZON | Pad | recorded PCM: Organ |
| 090 | Pad | LA-inspired | VIOLIN HALO | Pad | recorded PCM: Solo violin |
| 091 | Pad | LA-inspired | VIOLA VEIL | Pad | recorded PCM: Viola section |
| 092 | Pad | LA-inspired | CELLO WARM PAD | Pad | recorded PCM: Cello section |
| 093 | Pad | LA-inspired | FLUTE AURORA | Pad | recorded PCM: Flute |
| 094 | Pad | LA-inspired | HORN SLOW PAD | Pad | recorded PCM: French horn |
| 095 | Pluck | LA-inspired | NYLON PLUCK | Pluck | original synthesis |
| 096 | Pluck | LA-inspired | THUMB PLUCK | Pluck | original synthesis |
| 097 | Pluck | LA-inspired | WIRE ZITHER | Pluck | original synthesis |
| 098 | Pluck | LA-inspired | HARP BRIGHT | Pluck | recorded PCM: Harp |
| 099 | Pluck | LA-inspired | HARP RAIN | Pluck | recorded PCM: Harp |
| 100 | Pluck | LA-inspired | HARP MELLOW | Pluck | recorded PCM: Harp |
| 101 | Pluck | LA-inspired | HARP THREAD | Pluck | recorded PCM: Harp |
| 102 | Pluck | LA-inspired | PIANO PLUCK | Pluck | recorded PCM: Piano |
| 103 | Pluck | LA-inspired | MARIMBA PLUCK | Pluck | recorded PCM: Marimba |
| 104 | Pluck | LA-inspired | XYLO PLUCK | Pluck | recorded PCM: Xylophone |
| 105 | Mallet/Bell | LA-inspired | GLASS BELL | Bell | original synthesis |
| 106 | Mallet/Bell | LA-inspired | WOOD MALLET | Mallet | original synthesis |
| 107 | Mallet/Bell | LA-inspired | GLASS TINES | Mallet | original synthesis |
| 108 | Mallet/Bell | LA-inspired | GLOCK BELL | Mallet/Bell | recorded PCM: Glockenspiel |
| 109 | Mallet/Bell | LA-inspired | MARIMBA HOLLOW | Mallet/Bell | recorded PCM: Marimba |
| 110 | Mallet/Bell | LA-inspired | GLOCK DROPS | Mallet/Bell | recorded PCM: Glockenspiel |
| 111 | Mallet/Bell | LA-inspired | XYLO BAR | Mallet/Bell | recorded PCM: Xylophone |
| 112 | Mallet/Bell | LA-inspired | MARIMBA ROUND | Mallet/Bell | recorded PCM: Marimba |
| 113 | Voice | LA-inspired | VOWEL CHIME | Voice-like | original synthesis |
| 114 | Voice | LA-inspired | BREATH STACK | Voice-like | original synthesis |
| 115 | Voice | LA-inspired | CHOIR FLICK | Voice-like | original synthesis |
| 116 | Voice | LA-inspired | FLUTE CHOIR | Processed woodwind | recorded PCM: Flute |
| 117 | Voice | LA-inspired | OBOE VOWEL | Processed woodwind | recorded PCM: Oboe |
| 118 | Voice | LA-inspired | CLARINET VOICE | Processed woodwind | recorded PCM: Clarinet |
| 119 | Hit/Percussion | LA-inspired | CHORD STAB | Stab | original synthesis |
| 120 | Hit/Percussion | LA-inspired | BRASS IMPACT | Hit | original synthesis |
| 121 | Hit/Percussion | LA-inspired | TIMBER HIT | Percussion | original synthesis |
| 122 | Hit/Percussion | LA-inspired | TUBE STRIKE | Percussion | original synthesis |
| 123 | Hit/Percussion | LA-inspired | TRUMPET HIT | Hit/Percussion | recorded PCM: Trumpet |
| 124 | Texture | LA-inspired | ORBITAL MT | Texture | original synthesis |
| 125 | Texture | LA-inspired | METAL RAIN | Texture | original synthesis |
| 126 | Texture | LA-inspired | SPACE SHARD | Texture | original synthesis |
| 127 | Texture | LA-inspired | DIGITAL MIST | Texture | original synthesis |
| 128 | Texture | LA-inspired | VIOLA DRIFT | Texture | recorded PCM: Viola section |

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
