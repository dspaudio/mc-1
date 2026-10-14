# Building MCFM-1

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
  src/engine.c src/factory.c src/project.c src/store.c src/ui.c tests/test.c \
  -o build/test-sanitized
build/test-sanitized
```

Regenerate assets with `python3 tools/gen_assets.py`; checked-in tables make normal builds
independent of Python floating-point rounding. Python standard-library scripts can render
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

Outputs are `build/mcfm-1.elf`, `build/mcfm-1.app.bin`, disassembly/symbols and
`build/target-report.json`. The script checks the entry stub, XIP placement, image size,
RAM sections, pool headroom and absence of calls from flash-off RAM code. Link failures
are errors, not waived warnings. Each compiler command has a 60-second timeout.

**The raw `.app.bin` is not a `.fwsc` update package. Do not feed it to an updater.**
No installation or flashing command is included. The AC79 SDK boot/config binaries and
OTA installer are not in this project. Hardware installation/recovery planning, boot checks,
calibration verification and real-time measurement remain separate work requiring permission.

The development Mac's Docker daemon left new ARM and x86 containers in Created state,
even for `uname`; existing unrelated containers were not restarted. CI on native Linux
provides the independent target compilation path. See the validation record for results.
