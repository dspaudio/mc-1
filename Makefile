# SPDX-License-Identifier: GPL-3.0-only
CC ?= cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Werror -Wconversion -Wno-sign-conversion
CORE = src/effects.c src/demo.c src/engine.c src/midi.c src/factory.c src/project.c src/store.c src/ui.c
HEADERS = src/mcfm.h src/midi.h src/presets.h src/recorded_pcm.h src/store.h src/ui.h src/tables.h
.PHONY: all test test-ota render clean assets target
all: build/mc1-host
build:
	mkdir -p build
build/mc1-host: $(CORE) $(HEADERS) tools/host.c | build
	$(CC) $(CFLAGS) -Isrc $(CORE) tools/host.c -o $@
build/test: $(CORE) $(HEADERS) tests/test.c tests/presets_012.h | build
	$(CC) $(CFLAGS) -Isrc $(CORE) tests/test.c -o $@
build/test-ui-input: $(CORE) $(HEADERS) tests/ui_input.c | build
	$(CC) $(CFLAGS) -Isrc $(CORE) tests/ui_input.c -o $@
build/test-performance: $(CORE) $(HEADERS) tests/performance.c | build
	$(CC) $(CFLAGS) -Isrc $(CORE) tests/performance.c -o $@
build/test-patterns: $(CORE) $(HEADERS) tests/patterns.c | build
	$(CC) $(CFLAGS) -Isrc $(CORE) tests/patterns.c -o $@
build/test-output: $(CORE) $(HEADERS) tests/output.c | build
	$(CC) $(CFLAGS) -Isrc $(CORE) tests/output.c -lm -o $@
build/test-metronome: $(CORE) $(HEADERS) tests/metronome.c | build
	$(CC) $(CFLAGS) -Isrc $(CORE) tests/metronome.c -o $@
build/test-song: $(CORE) $(HEADERS) tests/song.c | build
	$(CC) $(CFLAGS) -Isrc $(CORE) tests/song.c -o $@
build/test-midi: $(CORE) $(HEADERS) tests/midi.c | build
	$(CC) $(CFLAGS) -Isrc $(CORE) tests/midi.c -o $@
build/test-connections: firmware/update/usb.c firmware/connection/midi_uart.c firmware/hal/fm1_uart.h tests/connections.c | build
	$(CC) -O2 -std=c11 -Wall -Wextra -Werror -Wno-unused-function tests/connections.c -o $@
build/test-connection-routing: $(CORE) $(HEADERS) firmware/connection/routing.c tests/connection_routing.c | build
	$(CC) $(CFLAGS) -Isrc $(CORE) tests/connection_routing.c -o $@
build/test-storage: $(CORE) $(HEADERS) tests/storage.c | build
	$(CC) $(CFLAGS) -Isrc $(CORE) tests/storage.c -o $@
build/test-effects-modes: src/effects.c $(HEADERS) tests/effects_modes.c | build
	$(CC) $(CFLAGS) -Isrc src/effects.c tests/effects_modes.c -lm -o $@
build/test-panel-leds: firmware/panel_leds.h tests/panel_leds.c | build
	$(CC) $(CFLAGS) tests/panel_leds.c -o $@
build/test-ota: firmware/update/ota.c tests/ota_test.c | build
	$(CC) -O2 -std=c11 -Wall -Wextra -Werror -Wno-unused-function tests/ota_test.c -o $@
build/ldr-test: firmware/update/ota.c firmware/loader/ldr_core.c tests/ldr_test.c | build
	$(CC) -O2 -std=c11 -Wall -Wextra -Werror -Wno-unused-function tests/ldr_test.c -o $@
build/test-ota-entry: $(CORE) $(HEADERS) firmware/update/ota_app.c firmware/update/ota_entry.c firmware/connection/routing.c tests/ota_entry_test.c | build
	$(CC) $(CFLAGS) -Isrc $(CORE) tests/ota_entry_test.c -o $@
build/test-panel-refresh: firmware/panel_refresh.c tests/panel_refresh.c | build
	$(CC) $(CFLAGS) tests/panel_refresh.c -o $@
build/test-connections-ota: firmware/update/usb.c tests/connections.c | build
	$(CC) -O2 -std=c11 -Wall -Wextra -Werror -Wno-unused-function -DFELUCCA_OTA=1 tests/connections.c -o $@
build/test-perf-probe: firmware/perf_probe.h firmware/update/ota.c tests/perf_probe.c | build
	$(CC) $(CFLAGS) -Wno-unused-function tests/perf_probe.c -o $@
test-ota: build/test-ota build/ldr-test
	./build/test-ota build/mc-1-EXPERIMENTAL.fwsc
	./build/ldr-test build/previous-test-only.fwsc build/mc-1-EXPERIMENTAL.fwsc
test: build/test build/test-ui-input build/test-performance build/test-patterns build/test-output build/test-metronome build/test-song build/test-midi build/test-connections build/test-connection-routing build/test-storage build/test-effects-modes build/test-panel-leds build/test-ota-entry build/test-panel-refresh build/test-connections-ota build/test-perf-probe
	./build/test
	./build/test-ui-input
	./build/test-performance
	./build/test-patterns
	./build/test-output
	./build/test-metronome
	./build/test-song
	./build/test-midi
	./build/test-connections
	./build/test-connection-routing
	./build/test-storage
	./build/test-effects-modes
	./build/test-panel-leds
	./build/test-ota-entry
	./build/test-panel-refresh
	./build/test-connections-ota
	./build/test-perf-probe
	python3 -B tests/perf_capture_test.py
render: build/mc1-host
	./build/mc1-host render build/demo.wav 16
assets:
	python3 tools/gen_assets.py
target:
	python3 tools/build_target.py
clean:
	rm -rf build

build/showcase: $(CORE) $(HEADERS) tools/showcase.c | build
	$(CC) $(CFLAGS) -Isrc $(CORE) tools/showcase.c -o $@
showcase: build/showcase
	build/showcase la build/la-showcase.wav
	build/showcase 303 build/303-showcase.wav
	build/showcase 808 build/808-showcase.wav
	build/showcase kits build/kit-showcase.wav
	build/showcase recorded build/recorded-showcase.wav

build/trance: $(CORE) $(HEADERS) tools/trance.c | build
	$(CC) $(CFLAGS) -Isrc $(CORE) tools/trance.c -o $@
trance: build/trance
	build/trance build/neon-spiral.wav
