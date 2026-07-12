# Builds and runs the host-side smoke tests against the POSIX dev/test
# backend, using the ordinary system gcc/clang -- no 68k cross toolchain
# needed. This is a development tool, not one of the shipped platform
# builds (see build/Makefile.*).

CC      ?= cc
CFLAGS  ?= -std=c99 -Wall -Wextra -O1 -g
INCLUDES = -I../include -I../src/platform/posix
OUTDIR   = out

SRCS = test_gfx.c \
       ../src/common/gfx_primitives.c \
       ../src/common/font/gfx_font.c \
       ../src/common/font/font_default.c \
       ../src/platform/posix/gfx_posix.c

BIN = $(OUTDIR)/test_gfx

.PHONY: all test clean

all: $(BIN)

$(BIN): $(SRCS) | $(OUTDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(SRCS)

$(OUTDIR):
	mkdir -p $(OUTDIR)

test: $(BIN)
	./$(BIN) $(OUTDIR)

clean:
	rm -rf $(OUTDIR)
