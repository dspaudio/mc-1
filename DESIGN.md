# MC-1 product site and firmware installer

The landing page preserves the reusable shell, navigation, buttons, spacing and
responsive layouts in `site/styles.css`: charcoal `#101110`, panel `#1c1e1b`,
ink `#f1f0e9`, muted `#b7b9ae`, orange `#ff6500` and green `#63ff42`.
Installer links reuse the existing `.button`, `.primary` and navigation styles.
The installer is a separate utility page on this repository's Pages site;
the mc-1-web repository only runs the emulator.

## 1. Atmosphere and identity

Preserve the existing installer interface: charcoal surfaces, monospace labels,
amber actions, and green playback/success indicators. The firmware installer is
a separate utility page, not an overlay on the playable instrument. It must make
the selected device, package, and current installation stage unambiguous.

## 2. Color

The installer retains its existing dark palette in both system color modes.
It does not introduce a light instrument skin.

| Token | Value | Use |
| --- | --- | --- |
| `--bg` | `#101114` | Page |
| `--surface` | `#1c1f24` | Recessed sections |
| `--control` | `#25282e` | Controls |
| `--txt` | `#c9cdd4` | Main text |
| `--muted` | `#9aa1ab` | Readable secondary text |
| `--border` | `#747b86` | Input boundaries |
| `--accent` | `#e8b84b` | Primary action and links |
| `--success` | `#63ff42` | Verified completion |
| `--error` | `#ffb4ab` | Failure text |
| `--ink` | `#101114` | Text on amber buttons |

Status always includes text; color is supplementary.

## 3. Typography

Use `"Terminus (TTF)", "DejaVu Sans Mono", ui-monospace, monospace`, without
network font dependencies. Installer body and controls are 14px with 1.6 line
height; the page heading is 24px/1.3, section headings 16px/1.5, and metadata
12px/1.6. Hashes wrap without horizontal scrolling and remain selectable.

## 4. Spacing and layout

Use 4, 8, 12, 16, 24, and 32px spacing steps. Main content is at most 720px
wide, centered with 16px mobile gutters and 32px desktop vertical margins.
Sections form one vertical stack. Form controls wrap into a single column
when their labels and selected values no longer fit. The document owns
scrolling; no fixed panel may cover controls or install progress.

## 5. Components

- **Utility navigation:** ordinary underlined links to the emulator, firmware
  source, and release. Keyboard focus is visible.
- **Section:** a heading and related controls on a charcoal surface with 24px
  padding and an 8px radius. No decorative illustrations or new card grid.
- **Button:** at least 44px tall, 12px horizontal padding, 5px radius.
  Primary uses amber with dark text; secondary uses the control surface.
  Disabled uses the native disabled state and must remain distinguishable.
  Enabled hover uses an amber border (text-colored for the primary button).
  Pressed secondary uses amber with dark text; pressed primary uses the text
  surface with dark text. Feedback is immediate and never changes geometry.
- **Connection:** one explicit Connect FM-1 action automatically discovers a
  uniquely matched MIDI pair and reads its identity. No input/output selectors.
  Missing or multiple devices block installation and provide a retry action.
- **Status:** persistent `role="status"` text; fatal failures use `role="alert"`.
  It covers unsupported, disconnected, permission denied, connecting, ready,
  checking, writing, rebooting, verified, and failed states.
- **Progress:** a native labeled progress element plus stage and percentage text.
  Indeterminate progress is reserved for discovery/reboot, not byte transfer.
- **Package metadata:** version, identity, byte size, SHA256 and source/release
  links. A package must be verified before installation is enabled.
- **Official V15 restore:** a visible section after MC-1 installation links to
  `https://www.m-vave.com/download`. A native labeled `.fwsc` file input fills
  the available width and wraps within the existing section. A native checkbox
  with a wrapping label explicitly acknowledges that MC-1 projects/patches
  cannot be used in V15 and that restoration is not a backup. The secondary
  restore button requires a verified file, acknowledgement and identified device.
  File and acknowledgement controls use the existing spacing, focus and accent
  tokens; checkbox labels have a minimum 44px touch height.
- **Restore validation status:** persistent live status, using the same error
  and verified colors as installation. States are empty, validating, verified
  and rejected. A file change immediately clears the prepared image and the
  acknowledgement; only the latest selection may publish a result.

## 6. Motion and interaction

No decorative animation. Native focus and pressed feedback are sufficient.
Connection requests require a button click; installation requires a separate,
explicit action after identifying both device and package. Opening the page
never sends an update command. During flash writes, conflicting actions are
disabled and the page reports that USB and power must remain connected.
Both transfer actions share the connection lock and unload guard; each action's
section shows its own progress and persistent status beside the initiating button.
During either transfer, connection, installation, file selection, acknowledgement
and restoration are disabled. File selection validates locally without requesting
MIDI or writing firmware. Restoration retains official V15 flash content while
using the verified release's small MC-1 loader, and succeeds only after the
installer verifies the restarted `FM-1_015` identity. Recovery allows reconnecting
MC-1 or its `loader900` ports and retrying the same prepared restore.
Restoration replaces the official application, not the separate factory USR
patch bank, and is not a factory reset or a guarantee of stored-data retention.

## 7. Accessibility and verification

Use semantic headings, form labels, native buttons/selects, visible 2px amber
focus outlines with a 3px offset, readable contrast, and live status updates.
The full path is keyboard operable. Permission denial, missing ports, unsupported
browsers, package damage, disconnection and retry each need an actionable state.
Test the actual page at 390px and 1440px, plus a wide desktop; inspect wrapping,
bottom-of-page content and each critical installation state.

## 8. Scope and accepted constraints

The existing emulator retains its instrument layout and compact rotation behavior.
This work adds a utility page and a discoverable link rather than redesigning
the instrument. Web MIDI SysEx availability is detected at runtime. Unsupported
browsers receive an explanation and the desktop installer/release path, never
a false success. Hardware installation is verified separately from visual QA.
