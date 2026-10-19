# Building MC-1

## Host

```sh
make test
make render
python3 host/serve.py --port 8091
```

For sanitizer checks, use a separate output binary (do not clean during another build):

```sh
cc -O1 -g -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-sanitize-recover=all -fno-omit-frame-pointer -Isrc \
  src/effects.c src/demo.c src/engine.c src/midi.c src/factory.c src/project.c src/store.c src/ui.c tests/test.c \
  -o build/test-sanitized
build/test-sanitized
```

Regenerate original mathematical assets with `python3 tools/gen_assets.py`; checked-in tables make normal builds
independent of Python floating-point rounding. Regenerate recorded PCM with Python 3.11+
using `python3.11 tools/gen_recorded_pcm.py`; retained CC0 source WAVs make this conversion
offline and independent of external packages. The manifest pins hashes and pitch roots; conversion measurements go to
`build/recorded-pcm-report.json`.
`python3 tools/gen_presets.py` regenerates the factory bank, symbolic IDs and sound lists. Python standard-library scripts can render
and inspect demos without installing modules. `host/serve.py` compiles a native shared library
and exposes only fixed loopback endpoints; it does not accept local filesystem paths.

## Raw FM-1 target app

Version 1.1.0 defaults to `MC_DUAL_CORE=1`, eight melodic / three worker voices.
CPU1 renders three melodic voices, six drums and all four FX banks while CPU0
keeps sequencing, controls and five melodic voices. Other target voice counts
are rejected. Original single-core 1.0.2 is an immutable release asset, not a
single-core rebuild of the newer format-8 engine.
`MC_RENDER_PROFILE=1` adds separate synthesis/four-bank/wait/transfer measurements,
exported alongside `MC_PERF_PROBE=1` using stage schema 5 (42 words / 7 pages).
Both default to 1 for the release configuration used in device trials;
diagnostics remain inactive until explicitly requested. Trace and voice probing
remain off. PLAY pulse is on.
See [four-bank design and test procedure](docs/FOUR_BANKS.md); the earlier
[two-bank report](docs/DUAL_CORE.md) is historical.
These flags do not install firmware or alter clock/voltage/flash partitions.

## Current demo audio

Both updated demos emphasize warm Analog variance plus simultaneous reverb and
delay on the second melodic part. The third part uses a recorded piano or
marimba with PCM mix 108/127, retaining a small LA oscillator contribution.
The native renderer plays 32 bars and five seconds of release/FX tail:

```sh
make build/render-demos
mkdir -p build/demos
build/render-demos 0 build/demos/night-circuit-fourfx-analog.wav
build/render-demos 1 build/demos/neon-spiral-fourfx-analog.wav
lame -b 320 build/demos/night-circuit-fourfx-analog.wav build/demos/night-circuit-fourfx-analog.mp3
lame -b 320 build/demos/neon-spiral-fourfx-analog.wav build/demos/neon-spiral-fourfx-analog.mp3
```

LAME is only an offline host export tool, not a firmware dependency. The WAVs
come from the shared C synth/FX engine without external mastering or synthetic
placeholder audio. These renders do not measure the FM-1's real-time performance.
The 1.1.0 site exports are stereo 44.1 kHz/320 kbps; encoder-specific MP3 bytes
need not match another encoder's output, while source PCM remains reproducible.

The CPU is the AC791N/WL82 pi32v2. Use JieLi's official Linux toolchain, with
`JIELI_TOOLCHAIN` pointing to the directory containing `pi32v2/` and `common/`.
On native Linux x86-64 it runs directly. On macOS it runs in Docker's Linux x86-64 container.

```sh
export JIELI_TOOLCHAIN=/path/to/jieli-linux-toolchains-20260730.1
make target
```

The default target keeps app/HAL/IRQ code at `-Os` and compiles only
`src/engine.c` and `src/effects.c` as separate `-O2` objects. This is the
hardware-validated optimization split. `MC_DSP_OPTIMIZATION=-Os` is available
for diagnostic comparisons; whole-app `-O2` is rejected because that earlier
experiment broke USB boot/update behavior. The target report records both
optimization settings.

The tested acquisition URL is the official vendor distribution:
`https://jl-update.oss-cn-shenzhen.aliyuncs.com/jieli-linux-toolchains-20260730.1.tar.xz`.
SHA-256: `a1545cddbc451a06bac43eb5cd96d831470c42a97c5c0b1f98879a20fc7647bd`.
Download/extract outside this repository; do not commit the toolchain. CI verifies this hash.

Outputs are `build/mc-1.elf`, `build/mc-1.app.bin`, disassembly/symbols and
`build/target-report.json`. The script checks the entry stub, XIP placement, image size,
RAM sections, pool headroom and absence of calls from flash-off RAM code. Link failures
are errors, not waived warnings. Each compiler command has a 60-second timeout.

**The raw `.app.bin` is not a `.fwsc` update package. Do not feed it to an updater.**
No installation or flashing command is included. Hardware installation, boot checks,
calibration verification and real-time measurement require a separate device test.

The development Mac's Linux x86-64 Docker build reproduces the native Linux CI app
and package bytes. The target report normalizes the compiler installation path to
`<toolchain>/pi32v2/bin`, so identical builds compare exactly across Docker and CI.
The compiler version, image hashes and section sizes remain checked. See the validation
record for the exact source revision, checks and measured section sizes.

## Experimental .fwsc package

Use the official SDK commit `d179b4484759423312073f5fbb232501aa491047` from
https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK (tag `AC79NN_SDK_V1.2.1_2023-12-13`).
The CI workflow shows the exact sparse checkout and toolchain acquisition.

```sh
export AC79_SDK=/path/to/fw-AC79_AIoT_SDK
python3 tools/build_package.py
cc -O2 -Wall -Wextra -Werror -Wno-unused-function tests/ldr_test.c -o build/ldr-test
build/ldr-test build/previous-test-only.fwsc build/mc-1-EXPERIMENTAL.fwsc
```

This rebuilds the app and a separate, entirely RAM-resident upstream loader, verifies
three SDK binary hashes, round-trips each LZ4 block, then packages both using the
upstream FM-1 format. The `.fwsc` is a candidate for the standard M-UPGRADE path
when entered from a compatible existing stock/SLOOP/Felucca/ORBIT updater. Physical
USB handshakes, repeated OTA install and boot were tested for the candidate
hashes listed in [the validation record](docs/VALIDATION.md); that evidence is
not a claim about an arbitrary rebuilt package. Protocol identity `FM-1_900`
is inherited and is not an MC-1 build-version check.

The loader writes app/config `0x4000..0x92FFF`, never boot head `0..0x3FFF`.
It scans `0x93000..0xFBFFF` for valid update records and erases only matching record
sectors. It leaves calibration/global/tail `0xFC000..0xFFFFF` untouched. This is a
separate installer policy; the audio app writes only its two 36 KiB project banks
in `0x99000..0xAAFFF`.
Host simulation covers CRC failure/retry, wrong key, unknown chip, interrupted host
completion, identical-image skip and persistent-region sentinels. It is not an
actual electrical power-loss or USB test. Never install `previous-test-only.fwsc`.

설치가 일부 진행된 뒤 플래시 식별에 실패해도 복구 기록을 유지하고 재부팅하지 않는다.
완료 응답은 `success\0` 전체를 확인하며, 복구 기록의 읽기·삭제·삭제 후 검증이
모두 성공해야 RAM 기록을 지우고 재부팅한다. 실패 시 설치기에 머물러 재시도를
기다린다. 호스트 오류 주입으로 이러한 경로를 검사하지만 실제 전원 차단 복구를
검증한 것은 아니다.

**업데이트·복구 경계:** 현재 앱은 `FELUCCA_OTA=1`로 Felucca 호환
M-UPGRADE의 식별·업데이트 진입을 활성화한다. JEDEC를 확인한 뒤 재생을
정지하고 로더를 staging하며, 실패하면 오디오·UART·일반 USB 서비스를
복구한다. 외부 MIDI 정리는 새 Note On보다 먼저 게시한다. 정상 부팅에도
staging 정리를 연결했다. 호스트 오류 주입 검사는 실제 USB 핸드셰이크·
재업데이트·부팅·전원 차단 복구의 검증을 대신하지 않는다.

로더 본문은 최대 16,352바이트다. 32바이트 JLFS 헤더와 함께
`0xE4000` 기록 섹터 앞에서 끝나야 하며, 초과 패키지는 삭제 전에 거부한다.
알려진 더 큰 공식 stock loader를 사용하는 원본 V15 패키지는 이 경로에서
수용하지 않는다.

The 1.0.2 installer verifies the original official V15 file's SHA256 and layout,
then preserves its official `flash.bin` while pairing it with the verified small
MC-1 RAM loader. Existing MC-1 installations can therefore restore the app without
widening size limits or flash-write allowlists. This does not execute the large
official loader, back up user data, rewrite the separate factory USR patch bank
or perform a factory reset.

재생 정지 상태에서 OCT−/OCT+를 5초 누르면 UBOOT로 진입하는 코드는
유지한다. 이번 수정본에서 그 물리 복구 경로는 미검증이다. 패키지 빌드는
실기기 쓰기 권한이나 가역적인 설치 검증을 대신하지 않는다.
SDK binaries remain outside source; packages carry the Apache-2.0 assets described
in NOTICE.md and LICENSES/Apache-2.0-AC79.txt.

## Product Pages and web installer

The firmware installer is served by this repository at `install.html`; mc-1-web
contains only the emulator. Like ORBIT's Pages pipeline, the build verifies a
release package and bundles it with the installer for same-origin delivery.
`FIRMWARE.json` pins the version, protocol identity, filename, size and SHA256.
The builder checks the package CRCs and loader before producing any site output.

```sh
python3 tools/make_site.py --out build/site
node tests/firmware-ota.mjs
node tests/firmware-stock.mjs
node tests/firmware-install.mjs
python3 tests/site_test.py build/site
```

Use Node.js 22+ and Python 3. For offline preview, add `--package` with a local
copy of the exact pinned current release and `--alternative-package` with the
original 1.0.2 package. A different rebuilt package is rejected.
Pages includes the product page, installer, package metadata, release `.fwsc`,
license notices and `SHA256SUMS`; no SDK or compiler is needed to assemble it.
The workflow assembles and checks a published release or a manual main-branch
dispatch, so a main push does not race an unpublished package. The Pages
environment accepts only main: after publishing, dispatch `pages.yml` on main
to deploy; tag-triggered runs validate without deploying. Publish after main
CI passes. The builder fails closed until both pinned release assets exist.

The 1.1.0 pin uses dual core, stage/performance probes and PLAY pulse enabled,
with CPU1 trace and voice probing disabled. Reproduce the release configuration:

```sh
MC_DUAL_CORE=1 MC_PERF_PROBE=1 MC_RENDER_PROFILE=1 MC_PLAY_LED_PULSE=1 \
MC_CPU1_TRACE=0 MC_VOICE_PROBE=0 python3 tools/build_package.py
```

The builder's internal file remains `build/mc-1-EXPERIMENTAL.fwsc`; publish its
verified bytes as `mc-1-1.1.0.fwsc`, never under the experimental filename.
The retained 1.0.2 SHA256 is
`a09e763e6000f25dec63eb5feeb6251389dce33bf5abe2eb4746f1f1c7091ab5`.
Do not reproduce 1.0.2 by selecting single core in current source: its original
format-7 code and package are preserved under tag v1.0.2.

For a private, offline V15 restore package, use the same browser implementation:

```sh
node tools/prepare_stock.mjs /path/to/FM-1.fwsc /path/to/mc-1-1.0.2.fwsc build/stock-v15-restore.fwsc
node tests/firmware-stock.mjs /path/to/FM-1.fwsc /path/to/mc-1-1.0.2.fwsc
make build/test-ota build/ldr-test
build/test-ota build/stock-v15-restore.fwsc
build/ldr-test /path/to/mc-1-1.0.2.fwsc build/stock-v15-restore.fwsc
```

Preparation never accesses a device and refuses to overwrite an existing output.
It requires the exact official file and the MC-1 package pinned in `FIRMWARE.json`.
The output contains M-VAVE application bytes: keep it private and do not commit or
publish it. Normal users select their own official file in the web installer.
