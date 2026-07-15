# gpuwatch - GPU core + VRAM temperature watcher
#
# Targets:
#   make            build libgpuwatch.a and the gpuwatch binary
#   make lib        build just the static library
#   make install    install binary to $(PREFIX)/bin, lib+header to $(PREFIX)
#   make uninstall  remove the installed files
#   make dist       build a source+binary tarball in build/
#   make deb rpm    build .deb / .rpm packages (needs nfpm)
#   make packages   build tarball + deb + rpm
#   make clean

VERSION := $(shell cat VERSION)

PREFIX  ?= /usr/local
CC      ?= cc
CFLAGS  ?= -O2 -std=c11 -Wall -Wextra
CFLAGS  += -Ilib -DGPUWATCH_VERSION=\"$(VERSION)\"
LDLIBS   = -lpci -lpthread -lm

BUILD    = build
LIB      = $(BUILD)/libgpuwatch.a
BIN      = $(BUILD)/gpuwatch

.PHONY: all lib install uninstall dist deb rpm packages clean
all: $(BIN)

$(BUILD):
	mkdir -p $(BUILD)

# static library (the reader)
$(LIB): lib/gpuwatch.c lib/gpuwatch.h | $(BUILD)
	$(CC) $(CFLAGS) -c lib/gpuwatch.c -o $(BUILD)/gpuwatch.o
	$(AR) rcs $@ $(BUILD)/gpuwatch.o

lib: $(LIB)

# app (TUI/ascii/json) links the library + flux.h (header-only)
$(BIN): app/gpuwatch_tui.c $(LIB) flux.h | $(BUILD)
	$(CC) $(CFLAGS) -I. app/gpuwatch_tui.c $(LIB) -o $@ $(LDLIBS)

install: all
	install -Dm755 $(BIN) $(DESTDIR)$(PREFIX)/bin/gpuwatch
	install -Dm644 $(LIB) $(DESTDIR)$(PREFIX)/lib/libgpuwatch.a
	install -Dm644 lib/gpuwatch.h $(DESTDIR)$(PREFIX)/include/gpuwatch.h

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/gpuwatch
	rm -f $(DESTDIR)$(PREFIX)/lib/libgpuwatch.a
	rm -f $(DESTDIR)$(PREFIX)/include/gpuwatch.h

# portable binary tarball: gpuwatch-<ver>-linux-x86_64.tar.gz
dist: all
	@mkdir -p $(BUILD)/dist/gpuwatch-$(VERSION)
	cp $(BIN) $(BUILD)/dist/gpuwatch-$(VERSION)/
	cp README.md LICENSE VERSION $(BUILD)/dist/gpuwatch-$(VERSION)/
	tar -C $(BUILD)/dist -czf $(BUILD)/gpuwatch-$(VERSION)-linux-x86_64.tar.gz gpuwatch-$(VERSION)
	@echo "built $(BUILD)/gpuwatch-$(VERSION)-linux-x86_64.tar.gz"

# .deb / .rpm via nfpm (https://nfpm.goreleaser.com). VERSION is passed to the
# nfpm config, which expands ${VERSION}.
NFPM := $(shell command -v nfpm 2>/dev/null)
deb: all
	@[ -n "$(NFPM)" ] || { echo "nfpm not found - install from https://nfpm.goreleaser.com"; exit 1; }
	VERSION=$(VERSION) nfpm package -f packaging/nfpm.yaml -p deb -t $(BUILD)/gpuwatch_amd64.deb
	@echo "built $(BUILD)/gpuwatch_amd64.deb"

rpm: all
	@[ -n "$(NFPM)" ] || { echo "nfpm not found - install from https://nfpm.goreleaser.com"; exit 1; }
	VERSION=$(VERSION) nfpm package -f packaging/nfpm.yaml -p rpm -t $(BUILD)/gpuwatch.x86_64.rpm
	@echo "built $(BUILD)/gpuwatch.x86_64.rpm"

packages: dist deb rpm

clean:
	rm -rf $(BUILD)
