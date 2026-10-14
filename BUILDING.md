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

The CPU is the AC791N/WL82 pi32v2. Use JieLi's official Linux toolchain, with
`JIELI_TOOLCHAIN` pointing to the directory containing `pi32v2/` and `common/`.
On native Linux x86-64 it runs directly. On macOS it runs in Docker's Linux x86-64 container.

```sh
export JIELI_TOOLCHAIN=/path/to/jieli-linux-toolchains-20260730.1
make target
```

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
and package bytes. The target report includes the compiler installation path, which
differs between Docker (`/opt/jieli`) and CI (`/tmp/mcfm-toolchain/...`); compare image
hashes and section sizes when checking across environments. See the validation
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
USB handshakes, install and boot are **not tested**. Protocol identity `FM-1_900`
is inherited; this is not an official vendor release or verified version check.

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

**Recovery boundary:** this preview audio app has no USB updater. After installing,
it cannot itself start another M-UPGRADE session. The implemented stopped-transport
five-second OCT−/OCT+ hold enters UBOOT, but the physical recovery path and the
necessary external recovery utility have not been verified. Do not treat a package
build as permission to flash or as a verified reversible installation. Retain a
known working recovery method before a separately authorized hardware trial.
SDK binaries remain outside source; packages carry the Apache-2.0 assets described
in NOTICE.md and LICENSES/Apache-2.0-AC79.txt.
