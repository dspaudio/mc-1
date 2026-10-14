# SPDX-License-Identifier: GPL-3.0-only
CC ?= cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Werror -Wconversion -Wno-sign-conversion
CORE = src/effects.c src/demo.c src/engine.c src/factory.c src/project.c src/store.c src/ui.c
HEADERS = src/mcfm.h src/presets.h src/store.h src/ui.h src/tables.h
.PHONY: all test render clean assets target
all: build/mc1-host
build:
	mkdir -p build
build/mc1-host: $(CORE) $(HEADERS) tools/host.c | build
	$(CC) $(CFLAGS) -Isrc $(CORE) tools/host.c -o $@
build/test: $(CORE) $(HEADERS) tests/test.c tests/presets_012.h | build
	$(CC) $(CFLAGS) -Isrc $(CORE) tests/test.c -o $@
test: build/test
	./build/test
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

build/trance: $(CORE) $(HEADERS) tools/trance.c | build
	$(CC) $(CFLAGS) -Isrc $(CORE) tools/trance.c -o $@
trance: build/trance
	build/trance build/neon-spiral.wav
