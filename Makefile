# gpuwatch — GPU core + VRAM temperature watcher
#
# Targets:
#   make            build libgpuwatch.a and the gpuwatch binary
#   make lib        build just the static library
#   make install    install binary to $(PREFIX)/bin, lib+header to $(PREFIX)
#   make clean

PREFIX  ?= /usr/local
CC      ?= cc
CFLAGS  ?= -O2 -std=c11 -Wall -Wextra
CFLAGS  += -Ilib
LDLIBS   = -lpci -lpthread -lm

BUILD    = build
LIB      = $(BUILD)/libgpuwatch.a
BIN      = $(BUILD)/gpuwatch

.PHONY: all lib install clean
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

clean:
	rm -rf $(BUILD)
