# MC-1 product site design system

## 0. Research log

- Product references: the thirteen actual 240 × 240 shared-C display captures from the 0.2.0 firmware validation run, including the 64-pattern counter. The same thirteen states were regenerated with the current C renderer. Preserve their bytes, aspect ratio, and pixel rendering.
- Embedded references: frontend design-system architecture, industrial-brutalist and image-to-code guidance; use a dark instrument surface, decisive type scale, aligned grids, and large unmodified screenshots. Product imagery takes precedence over generated mockups.
- Palette sanity check: UI/UX database music/synthesizer color search; the instrument's specified orange and green remain authoritative.
- Skipped lanes: generated imagery and external design research are unnecessary for these supplied product references. Early static-only review is historical; the 1.1.0 release adds direct Chrome viewport and interaction verification.

## 1. Atmosphere and identity

A compact instrument expanded into a readable product sheet. The signature is a large, untouched pixel display beside oversized MC-1 lettering, followed by open rows of real screens. Strong orange marks the invitation to play; green connects the page to the instrument's active playback. No imaginary enclosure or hardware photograph suggests an installed, tested device.

## 2. Color

| Token | Value | Role |
| --- | --- | --- |
| `--bg` | `#101110` | Main instrument surface |
| `--panel` | `#1c1e1b` | Alternate section and screen surround |
| `--ink` | `#f1f0e9` | Main text |
| `--muted` | `#b7b9ae` | Secondary text |
| `--line` | `#42453d` | Structural dividers |
| `--orange` | `#ff6500` | Primary action background and product identity |
| `--green` | `#63ff42` | Playback-linked numerals and focus |
| `--black` | `#090a08` | Screen surround and text on orange/green |

All CSS colors use these tokens. Dark substrate only; no gradients, glow, or shadows. Normal text uses ink or muted on bg/panel; action text uses black on orange. Green focus has an offset from the target.

## 3. Typography

Primary: Arial, Helvetica, sans-serif. Technical labels: ui-monospace, SFMono-Regular, Consolas, monospace. No downloaded fonts.

| Role | Scale | Line height |
| --- | --- | --- |
| Product name | `clamp(6rem, 15vw, 13rem)` bold | .85 |
| Section title | `clamp(2rem, 4vw, 3.75rem)` bold | 1.05 |
| Feature title | `1.5rem` bold | 1.2 |
| Lead | `clamp(1.25rem, 2vw, 1.5rem)` | 1.5 |
| Body | `1rem` | 1.65 |
| Label/caption | `.875rem` | 1.5 |
| Numeric feature | `clamp(3rem, 7vw, 6rem)` bold | 1 |

Body measure is 62 characters; labels may wrap. No text hidden behind ellipses.

## 4. Spacing and layout

4px base; spacing tokens are .25, .5, .75, 1, 1.5, 2, 3, 4, and 6rem. Content width 1280px; fluid gutters 1–3rem. The hero uses a 1.1/.9 column split at 960px, then stacks. Two-column features stack below 700px. Gallery uses intrinsic tracks with a 220px preferred minimum, shrinking to the available width. Screens stay square and never crop. The document owns vertical scrolling; no sticky bar or nested scrolling is required.

## 5. Components

- Navigation: semantic header/nav, wrapping link cluster, 44px minimum targets. Visible skip link on focus. Default text, hover underline, active orange text, offset green focus ring.
- Action: native anchor, filled orange primary or neutral outlined secondary. Equal padding and target size; hover/active use opacity, focus uses a ring. No disabled/loading state because destinations are immediate links.
- Screen figure: image with intrinsic 240px dimensions, descriptive alt, caption, pixelated scaling. Hero eager/high priority; remaining images lazy. No fabricated controls or clickable affordance on still images.
- Feature: section heading and short lead followed by open image grid or a two-column article. Dividers separate sections; no card nesting. Numeric summaries use definition lists.
- Audio: native audio controls, preload none, visible title and download fallback. Browser supplies keyboard/focus/transport states. No autoplay or custom transport. With multiple examples, align the introduction with the first player rather than vertically centering it.
- Inline sound lists: keep each multiword kit/effect name and its following separator together; wrap between complete items. Adjacent screen grids retain the same 2rem gap as their internal tracks.
- Control guide: semantic definition list with physical control labels and plain-language behavior. No simulated interactive knobs.

## 6. Motion and interaction

No automatic animation, parallax, or scroll reveal. Link hover/active feedback is immediate and changes no geometry. Native audio is the only on-page stateful control; navigation, emulator, source, and manual links are ordinary anchors. Reduced-motion users receive the same static layout.

### Firmware installer

Preserve the installer's existing inline dark technical theme, monospace type,
yellow action/focus token and native 44px controls. The release selector reuses
the button surface, border, type and disabled opacity, with a visible label.
Metadata and download/release links always describe the selected version, even
while verification is pending. Pending or failed verification disables device
actions. Changing versions clears V15 preparation and acknowledgement. Device
connection, installation and V15 restoration lock the selector; stale package
or V15 verification must never replace the current selection. Status and error
feedback retain the existing live regions; no new animation or palette.

## 7. Depth and surface

Borders and tonal shifts only, with square corners and no drop shadows. Screens have a black surround and neutral inset padding. Orange is used as action fill, never as a decorative card outline. Actual screenshot pixels keep their original palette.

## 8. Accessibility constraints and accepted debt

Target WCAG AA text contrast, visible keyboard focus, logical heading order, descriptive alt text, native audio controls, no autoplay, and reflow without fixed-width content. Color reinforces text labels rather than replacing them. The original C screenshots contain small bitmap text; adjacent HTML explains the feature, and originals remain available at their native resolution.

| Item | Reason and exit |
| --- | --- |
| Historical static-only review | Superseded by the 1.1.0 Chrome desktop/mobile release checks; do not interpret earlier missing browser evidence as a current verification claim. |
| Single dark theme | Matches the supplied instrument identity; system controls remain native. |

## 0.3.0 screen provenance

The Song and MIDI gallery originally used 240 × 240 RGB565 frames from `mc_ui_strip`. The 1.1.0 release refreshes all 26 screenshots from the current shared C renderer. BLE is explicitly unavailable; no device connection or hardware timing is implied. Existing responsive grid and image styles are reused. Release media and direct browser captures are retained in `build/release110-media/` and `build/release110/`.
