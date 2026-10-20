## Complete factory preset list

128 melodic presets: 112 source-backed LA hybrid PCM presets from 17 real instrument families and 16 original 303-inspired Acid presets, plus eight original synthesized drum kits with 16 lanes each. These are MC-1 presets, not Roland patch data.

The [official MC-505 patch listing (MC505PTC.pdf)](https://cdn.roland.com/assets/media/pdf/MC505PTC.pdf) informed broad sound families such as acid sequences, house keys and organs, voice-like tones and percussion hits. MC-1 uses original patch names and parameters; no Roland patches, ROMs or samples are copied.

All 128 names, symbolic IDs and category positions are retained. All 112 LA patch definitions are replaced, including the 48 previously mathematical synthesis patches; the 64 previously recorded presets keep their intended instrument families. The 16 Acid definitions and their source metadata, and all eight drum kits, are unchanged. Earlier LA renders are historical audio, not bit-identical examples of this bank.

Each LA sound uses a real recorded attack and two same-phase spectral sustain frames from its source family. PCM mix 127 retains the full recorded attack; the hybrid engine does not blend generic saw or square oscillators at any mix. Plucked sources use their natural per-source decay; bowed strings, organ and winds retain held sustain. Natural, one-pole soft and two-pole rounded attack colours preserve each source while distinguishing felt keys, rounded basses, breath-led winds and clear percussion. Attack contribution and ADSR articulation are tuned per preset; natural pluck decay remains independent. Held-frame evolution is level-matched instead of adding an accent or fade. Approved representative presets remain fixed while the other 106 presets are audited by category. The PCM data budget remains at most 390,144 bytes globally; source generation and engine validation establish the actual allocation.

The 17 source families are Piano, Organ, Marimba, Glockenspiel, Xylophone, Harp, Solo violin, Viola section, Cello section, Contrabass, Flute, Oboe, Clarinet, Bassoon, Trumpet, French horn and Nylon guitar (PCM IDs 24–40 in that order). Voice presets are processed woodwinds, not human vocal recordings. The names describe register and performance intent; they do not add transpose parameters, new instruments or General MIDI protocol support. These compact source-derived sounds are not full multisampled instruments, and this table is not a listening-validation claim.

The Source column identifies the actual recorded instrument. See the [recording manifest](../assets/hybrid/manifest.json) for source files and processing, and the [licensing and provenance](../NOTICE.md).

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
| 001 | Bass | LA-inspired | ROUND BASS | Bass | recorded PCM: Contrabass |
| 002 | Bass | LA-inspired | DEEP CELL | Bass | recorded PCM: Cello section |
| 003 | Bass | LA-inspired | CLUB PULSE | Bass | recorded PCM: Organ |
| 004 | Bass | LA-inspired | CLICK FLOOR | Bass | recorded PCM: Xylophone |
| 005 | Bass | LA-inspired | WIRE PICK | Bass | recorded PCM: Nylon guitar |
| 006 | Bass | LA-inspired | DUB CHAMBER | Bass | recorded PCM: Contrabass |
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
| 029 | Lead | LA-inspired | VECTOR LEAD | Lead | recorded PCM: Trumpet |
| 030 | Lead | LA-inspired | WIDE SAW | Lead | recorded PCM: Solo violin |
| 031 | Lead | LA-inspired | SOFT PIPE | Lead | recorded PCM: Flute |
| 032 | Lead | LA-inspired | POLY CHROME | Poly synth | recorded PCM: Trumpet |
| 033 | Lead | LA-inspired | TRUMPET LEAD | Lead | recorded PCM: Trumpet |
| 034 | Lead | LA-inspired | HORN SOLO | Lead | recorded PCM: French horn |
| 035 | Lead | LA-inspired | FLUTE LEAD | Lead | recorded PCM: Flute |
| 036 | Lead | LA-inspired | FLUTE SOFT ARC | Lead | recorded PCM: Flute |
| 037 | Lead | LA-inspired | OBOE BRIGHT | Lead | recorded PCM: Oboe |
| 038 | Lead | LA-inspired | OBOE SOLO | Lead | recorded PCM: Oboe |
| 039 | Lead | LA-inspired | CLARINET ARC | Lead | recorded PCM: Clarinet |
| 040 | Lead | LA-inspired | CLARINET BRIGHT | Lead | recorded PCM: Clarinet |
| 041 | Sequence | LA-inspired | SEQUENCE KEY | Sequence | recorded PCM: Piano |
| 042 | Sequence | LA-inspired | SPARK SEQ | Sequence | recorded PCM: Glockenspiel |
| 043 | Sequence | LA-inspired | RATCHET PLUCK | Sequence | recorded PCM: Nylon guitar |
| 044 | Sequence | LA-inspired | PIANO STEPS | Sequence | recorded PCM: Piano |
| 045 | Sequence | LA-inspired | MARIMBA RUN | Sequence | recorded PCM: Marimba |
| 046 | Sequence | LA-inspired | GLOCK GRID | Sequence | recorded PCM: Glockenspiel |
| 047 | Sequence | LA-inspired | XYLO WALK | Sequence | recorded PCM: Xylophone |
| 048 | Sequence | LA-inspired | HARP PATTERN | Sequence | recorded PCM: Harp |
| 049 | Sequence | LA-inspired | CLARINET GATE | Sequence | recorded PCM: Clarinet |
| 050 | Sequence | LA-inspired | VIOLIN PULSE | Sequence | recorded PCM: Solo violin |
| 051 | Keys | LA-inspired | HAMMER KEY | Keys | recorded PCM: Piano |
| 052 | Keys | LA-inspired | SOFT TINES | Keys | recorded PCM: Glockenspiel |
| 053 | Keys | LA-inspired | CLAVI CLICK | Keys | recorded PCM: Nylon guitar |
| 054 | Keys | LA-inspired | VELVET CHORD | House keys | recorded PCM: Piano |
| 055 | Keys | LA-inspired | AMBER PIANO | Keys | recorded PCM: Piano |
| 056 | Keys | LA-inspired | PRISM KEY | Keys | recorded PCM: Glockenspiel |
| 057 | Keys | LA-inspired | PIANO FELT | Keys | recorded PCM: Piano |
| 058 | Keys | LA-inspired | PIANO BRIGHT | Keys | recorded PCM: Piano |
| 059 | Keys | LA-inspired | PIANO ROOM | Keys | recorded PCM: Piano |
| 060 | Keys | LA-inspired | PIANO SHORT | Keys | recorded PCM: Piano |
| 061 | Organ | LA-inspired | DRAWBAR KEY | Organ | recorded PCM: Organ |
| 062 | Organ | LA-inspired | PERC ORGAN | Organ | recorded PCM: Organ |
| 063 | Organ | LA-inspired | CELLAR ORGAN | House organ | recorded PCM: Organ |
| 064 | Organ | LA-inspired | ORGAN MELLOW | Organ | recorded PCM: Organ |
| 065 | Organ | LA-inspired | ORGAN CHAPEL | Organ | recorded PCM: Organ |
| 066 | Organ | LA-inspired | ORGAN PERC | Organ | recorded PCM: Organ |
| 067 | Organ | LA-inspired | ORGAN FULL | Organ | recorded PCM: Organ |
| 068 | Organ | LA-inspired | ORGAN BRIGHT | Organ | recorded PCM: Organ |
| 069 | Brass/Reed | LA-inspired | REED STACK | Reed | recorded PCM: Oboe |
| 070 | Brass/Reed | LA-inspired | BRASS BLOCK | Brass | recorded PCM: Trumpet |
| 071 | Brass/Reed | LA-inspired | SOFT HORNS | Brass | recorded PCM: French horn |
| 072 | Brass/Reed | LA-inspired | TRUMPET SECTION | Brass/Reed | recorded PCM: Trumpet |
| 073 | Brass/Reed | LA-inspired | HORN MELLOW | Brass/Reed | recorded PCM: French horn |
| 074 | Brass/Reed | LA-inspired | BASSOON REED | Brass/Reed | recorded PCM: Bassoon |
| 075 | Brass/Reed | LA-inspired | TRUMPET FANFARE | Brass/Reed | recorded PCM: Trumpet |
| 076 | Brass/Reed | LA-inspired | HORN LOW | Brass/Reed | recorded PCM: French horn |
| 077 | Strings | LA-inspired | STRING VEIL | Strings | recorded PCM: Viola section |
| 078 | Strings | LA-inspired | VIOLIN BOW | Strings | recorded PCM: Solo violin |
| 079 | Strings | LA-inspired | VIOLA CHAMBER | Strings | recorded PCM: Viola section |
| 080 | Strings | LA-inspired | CELLO ENSEMBLE | Strings | recorded PCM: Cello section |
| 081 | Strings | LA-inspired | VIOLIN SILVER | Strings | recorded PCM: Solo violin |
| 082 | Strings | LA-inspired | CONTRABASS BOW | Strings | recorded PCM: Contrabass |
| 083 | Pad | LA-inspired | SILK PAD | Pad | recorded PCM: Viola section |
| 084 | Pad | LA-inspired | SLOW HALO | Pad | recorded PCM: French horn |
| 085 | Pad | LA-inspired | BELL CLOUD | Pad | recorded PCM: Glockenspiel |
| 086 | Pad | LA-inspired | DUSK SCREEN | Pad | recorded PCM: Cello section |
| 087 | Pad | LA-inspired | AIR CHAMBER | Pad | recorded PCM: Flute |
| 088 | Pad | LA-inspired | PIANO CLOUD | Pad | recorded PCM: Piano |
| 089 | Pad | LA-inspired | ORGAN HORIZON | Pad | recorded PCM: Organ |
| 090 | Pad | LA-inspired | VIOLIN HALO | Pad | recorded PCM: Solo violin |
| 091 | Pad | LA-inspired | VIOLA VEIL | Pad | recorded PCM: Viola section |
| 092 | Pad | LA-inspired | CELLO WARM PAD | Pad | recorded PCM: Cello section |
| 093 | Pad | LA-inspired | FLUTE AURORA | Pad | recorded PCM: Flute |
| 094 | Pad | LA-inspired | HORN SLOW PAD | Pad | recorded PCM: French horn |
| 095 | Pluck | LA-inspired | NYLON PLUCK | Pluck | recorded PCM: Nylon guitar |
| 096 | Pluck | LA-inspired | THUMB PLUCK | Pluck | recorded PCM: Nylon guitar |
| 097 | Pluck | LA-inspired | WIRE ZITHER | Pluck | recorded PCM: Harp |
| 098 | Pluck | LA-inspired | HARP BRIGHT | Pluck | recorded PCM: Harp |
| 099 | Pluck | LA-inspired | HARP RAIN | Pluck | recorded PCM: Harp |
| 100 | Pluck | LA-inspired | HARP MELLOW | Pluck | recorded PCM: Harp |
| 101 | Pluck | LA-inspired | HARP THREAD | Pluck | recorded PCM: Harp |
| 102 | Pluck | LA-inspired | PIANO PLUCK | Pluck | recorded PCM: Piano |
| 103 | Pluck | LA-inspired | MARIMBA PLUCK | Pluck | recorded PCM: Marimba |
| 104 | Pluck | LA-inspired | XYLO PLUCK | Pluck | recorded PCM: Xylophone |
| 105 | Mallet/Bell | LA-inspired | GLASS BELL | Bell | recorded PCM: Glockenspiel |
| 106 | Mallet/Bell | LA-inspired | WOOD MALLET | Mallet | recorded PCM: Marimba |
| 107 | Mallet/Bell | LA-inspired | GLASS TINES | Mallet | recorded PCM: Glockenspiel |
| 108 | Mallet/Bell | LA-inspired | GLOCK BELL | Mallet/Bell | recorded PCM: Glockenspiel |
| 109 | Mallet/Bell | LA-inspired | MARIMBA HOLLOW | Mallet/Bell | recorded PCM: Marimba |
| 110 | Mallet/Bell | LA-inspired | GLOCK DROPS | Mallet/Bell | recorded PCM: Glockenspiel |
| 111 | Mallet/Bell | LA-inspired | XYLO BAR | Mallet/Bell | recorded PCM: Xylophone |
| 112 | Mallet/Bell | LA-inspired | MARIMBA ROUND | Mallet/Bell | recorded PCM: Marimba |
| 113 | Voice | LA-inspired | VOWEL CHIME | Voice-like | recorded PCM: Oboe |
| 114 | Voice | LA-inspired | BREATH STACK | Voice-like | recorded PCM: Flute |
| 115 | Voice | LA-inspired | CHOIR FLICK | Voice-like | recorded PCM: Clarinet |
| 116 | Voice | LA-inspired | FLUTE CHOIR | Processed woodwind | recorded PCM: Flute |
| 117 | Voice | LA-inspired | OBOE VOWEL | Processed woodwind | recorded PCM: Oboe |
| 118 | Voice | LA-inspired | CLARINET VOICE | Processed woodwind | recorded PCM: Clarinet |
| 119 | Hit/Percussion | LA-inspired | CHORD STAB | Stab | recorded PCM: Piano |
| 120 | Hit/Percussion | LA-inspired | BRASS IMPACT | Hit | recorded PCM: Trumpet |
| 121 | Hit/Percussion | LA-inspired | TIMBER HIT | Percussion | recorded PCM: Xylophone |
| 122 | Hit/Percussion | LA-inspired | TUBE STRIKE | Percussion | recorded PCM: Glockenspiel |
| 123 | Hit/Percussion | LA-inspired | TRUMPET HIT | Hit/Percussion | recorded PCM: Trumpet |
| 124 | Texture | LA-inspired | ORBITAL MT | Texture | recorded PCM: French horn |
| 125 | Texture | LA-inspired | METAL RAIN | Texture | recorded PCM: Glockenspiel |
| 126 | Texture | LA-inspired | SPACE SHARD | Texture | recorded PCM: Glockenspiel |
| 127 | Texture | LA-inspired | DIGITAL MIST | Texture | recorded PCM: Flute |
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

[Listen to all eight kits](audio/kit-showcase.wav): 64 seconds in the table order, eight seconds per kit. Each segment plays all 16 lanes during the first four seconds, then a three-second groove and one second of tail.

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
