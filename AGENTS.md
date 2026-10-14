# MCFM-1 development

Read README.md, docs/ARCHITECTURE.md and docs/VALIDATION.md before firmware changes.
Keep the shared C engine and UI authoritative: the host preview must use them.
Run `make test`, a clean ASan/UBSan test build, `make render`, and the pi32v2 build when available.
Do not claim host timing establishes FM-1 deadlines. Update measured reports after code changes.
Retain GPL-3.0-only and upstream copyright notices. All new UI, presets, demos and PCM must be original.
Never flash hardware without separate user permission. Do not widen the flash write allowlist.
Keep credentials, private files, vendor toolchains and SDK binaries out of this public repository.
