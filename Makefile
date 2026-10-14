# SPDX-License-Identifier: GPL-3.0-only
CC ?= cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Werror -Wconversion -Wno-sign-conversion
CORE = src/engine.c src/factory.c src/project.c src/store.c src/ui.c
HEADERS = src/mcfm.h src/store.h src/ui.h src/tables.h
.PHONY: all test render clean assets target
all: build/mcfm-host
build:
	mkdir -p build
build/mcfm-host: $(CORE) $(HEADERS) tools/host.c | build
	$(CC) $(CFLAGS) -Isrc $(CORE) tools/host.c -o $@
build/test: $(CORE) $(HEADERS) tests/test.c | build
	$(CC) $(CFLAGS) -Isrc $(CORE) tests/test.c -o $@
test: build/test
	./build/test
render: build/mcfm-host
	./build/mcfm-host render build/demo.wav 16
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
