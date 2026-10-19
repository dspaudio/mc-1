# Shared instrument display contract

## Scope

The authoritative UI is the existing allocation-free 240x240 RGB565 renderer
in `src/ui.c`. Device, native host and browser clients use the same pixels.
This change extends the existing FX page; it does not redesign other pages or
introduce web components, fonts or themes.

## Existing visual system

- Use the renderer's existing background, WHITE, DIM, ORANGE and GREEN colors.
- Text requested in DIM renders in WHITE for LCD readability; gray grids and
  inactive graphical controls retain DIM/GRID. Semantic accent text stays colored.
- Use its existing bitmap `text`, `number`, `rect` and `knob_scaled` primitives.
- Small text uses 6x9 glyphs with a seven-pixel advance, enlarged from 5x7 for
  LCD readability. Page titles retain the existing 10x14 double-size font.
  Small-text placement must be checked on every page, drum row and popup.
- Keep the existing sixteen drum rows and SOUND/STEP/HIT/LEVEL controls, with
  playback and edit indicators separate from the enlarged text.
- Drum rows start at y=24 with a ten-pixel pitch, nine-pixel labels and the
  existing eight-pixel step boxes. Their duplicated PATTERN footer is omitted;
  the header retains current/next pattern, while messages and bank hints
  temporarily use the kit-name line. Home uses CUR-NEXT so its caption does
  not collide with the preset category.
- Preserve the common header, part selection, transport and footer regions.
- The panel has four parameter knobs at the existing horizontal centers
  26, 85, 145 and 205. Each FX bank maps directly to one knob.
- This hardware display has one fixed dark theme and fixed pixel dimensions;
  responsive web layout and OS window styling do not apply.

## FX states and interaction

- Bank order is R/D1, C/F1, R/D2, C/F2 on both FX pages.
- SENDS edits the selected part's four independent 0..127 sends.
- TYPES edits four global algorithm selections, each 0..7.
- ENV switches between SENDS and TYPES. FX retains the current page mode;
  returning from another page starts at SENDS, as before.
- Existing selected-part and drum labels remain visible above the bank list.
- Four compact rows name each bank, its value and its selected algorithm.
- Bottom knobs repeat bank labels and show their current value graphically.
- Sends use existing encoder acceleration. Algorithm enumerations do not.
- Existing recording writes send automation per part and type automation
  once at the canonical global address. Existing EDIT+FX slide behavior stays.

## Verification and constraints

Render both page modes for all four selected parts using the actual shared C
renderer. Check zero/max sends, type 0/7, longest algorithm names, no clipped
text, no overlapping hints or footer controls, and knob-to-bank correspondence.
Native pixel/input tests cover raw and accelerated encoders, bounds, all four
parts and automation. Hardware knob feel and LCD transfer remain device QA.

The panel retains one live DMA strip buffer. A 115,200-byte native-RGB565 cache
stores the last submitted image in the existing SRAM pool; it is not another
DMA buffer. Each rendered eight-row strip is compared against that image.
Unchanged strips submit nothing; changed strips submit the smallest bounding
x range, compacted and byte-swapped in the same buffer only after DMA is idle.
Restart invalidates every strip. The 50 ms schedule and transport urgency are
unchanged. Deterministic fake-LCD tests reconstruct the complete displayed
pixels for all 28,920 left-edge/width combinations and protect in-flight data.
Read-only diagnostic schema 7 exposes completion/gap and submission/skip
counters so hardware cadence is measured separately from pixel correctness.

## Analog page

GLO cycles from MIDI to ANALOG VARIANCE, then back to MIXER. The page uses the
existing title, selected-part indicator, status text and first parameter knob.
AMOUNT is 0..127 for the selected melodic part; the other three parameter knobs
are inactive. The drum part displays MELODIC PARTS ONLY. Amount zero displays
VARIANCE OFF and preserves the original DSP path. PLAY + REC records amount
changes per melodic part. No new colors, bitmap fonts or motion are introduced.

## Playback underline

Playback uses a GREEN underline instead of recoloring note boxes. The underline
is 11 pixels wide and follows the audio sequencer position across the 208-pixel
step lane, including silent steps within populated tracks. Empty tracks and
empty drum rows have no underline. Internal-clock progress uses the current
step accumulator and swing duration; external-clock progress uses received MIDI
ticks and never free-runs ahead of the transport. The right edge wraps to the
first step. Stop hides the underline; muted tracks show no green underline.
Overview lanes place a two-pixel underline at track top + 23, below the existing
WHITE edit marker. Drum rows use their one-pixel gap at row top + 8.
There is no easing, wall-clock extrapolation or added animation timer.

All strips in a device frame share one captured transport/UI snapshot. Rectangle
fills clip once to the strip before writing contiguous spans; neither rendering
nor capture changes audio engine state.

The 6x9 bitmap expansion uses constant row/bit maps, not per-pixel division.
Large text retains the original scaled-rectangle path. A regenerated 120-state
C-renderer matrix is pixel-identical before and after that hot-loop change.

The physical PLAY button blinks GREEN once per quarter note: two sixteenths
on and two off, with phase taken directly from the sequencer. Swing cancels
over each pair, so both halves retain their BPM timing. External MIDI clock
drives the same phase; STOP turns PLAY off and restart begins on the beat.
REC remains independent and the PLAY red die remains off.

All twelve menu/transport buttons and both OCT buttons are lit at rest.
Selected menu/mode indicators blink at 2 Hz (250 ms on, 250 ms off), rather
than using darkness to mark unselected buttons. HOME stays steadily lit during
playback; its selected-menu blink applies only while stopped. PLAY overrides the menu cadence
while running with its sequencer-synchronized quarter-note blink; stopped PLAY
returns to steady green. Recording REC blinks at the menu cadence, independently
of PLAY. OCT and momentary SEL remain steadily lit. This changes only physical
button LEDs, not held-key or step-cursor LEDs and not the LCD transport symbol.
