# GFX68k

A small indexed-colour 2D graphics library in C for 68k machines: Atari
ST, Amiga, and classic Mac OS (Mac II-class and later, Color QuickDraw).

One public API (`include/gfx.h`), one platform backend compiled in at a
time (no runtime dispatch — you link against a single platform's object
files), and all the drawing logic that doesn't need to touch hardware
directly is shared, byte-for-byte, across all three.

## Status

The public API, the shared drawing/text code, and all three platform
backends are implemented. What's actually been *run*, as opposed to just
written against documented platform APIs:

- **Shared code (`src/common/`) and the API surface**: yes — exercised by
  `tests/test_gfx.c` against a software backend on plain Linux/gcc (see
  below), including pixel-level checks of every glyph in the bundled
  font.
- **Atari ST, Amiga, and Mac 68k backends**: no. This sandbox has none of
  the relevant cross toolchains (vbcc, bebbo's gcc, Retro68) or emulators
  available, so `src/platform/{atari,amiga,mac}/` are written directly
  against each platform's documented APIs (XBIOS, exec/graphics.library,
  Color QuickDraw) but have not been compiled or run. Treat them as a
  solid starting point, not as verified.

## Architecture

```
include/gfx.h          public API + GFX_ScreenMode
include/gfx_types.h     shared types: GFX_Bitmap, GFX_RGB, GFX_Rect, GFX_Font, ...

src/common/             platform-agnostic, compiled into every build
  gfx_primitives.c        line/rect/fill_rect/clear/blit_transparent
  font/gfx_font.c          bitmap font rendering engine
  font/font_default.c      bundled 8x8 font, printable ASCII 0x20-0x7E
                           (public-domain font8x8 data, see file header)

src/platform/<name>/    one backend per target, each implements:
  gfx_init/shutdown/get_screen/flip
  gfx_put_pixel/get_pixel/hline/vline
  gfx_blit, gfx_bitmap_create/destroy
  gfx_set_palette*/get_palette_entry
  atari/  ST low-rez (320x200x16), word-interleaved bitplanes, XBIOS
  amiga/  custom Screen, non-interleaved bitplanes, blitter-backed blit
  mac/    8-bit indexed GWorld presented via CopyBits, Color QuickDraw
  posix/  chunky 8bpp software backend for host-side testing only —
          not a shipped target, see src/platform/posix/gfx_posix.h
```

Everything in `src/common/` is written only in terms of the primitives
above (`gfx_put_pixel`, `gfx_get_pixel`, `gfx_hline`, `gfx_vline`), so it
never needs to know whether it's talking to ST bitplanes, Amiga
bitplanes, or a Mac PixMap.

`gfx_blit` is the one primitive each backend implements natively rather
than inheriting a generic per-pixel fallback: Amiga uses the blitter
(`BltBitMap`), Mac uses `CopyBits`. The ST backend still falls back to a
per-pixel loop for now (see comment in `gfx_atari.c`).

## Building

### Host tests (no cross toolchain needed)

```
cd tests
make test
```

Compiles `src/common/` and the POSIX software backend with the system
`cc`, runs `tests/test_gfx.c`, and writes a couple of `.ppm` files to
`tests/out/` you can open to see the rendering (lines, rects, text).

### Atari ST (vbcc)

Requires [vbcc](http://www.compilers.de/vbcc.html) with the
`vbcc68k_atari` target files, `vc` on `PATH`.

```
make -f build/Makefile.atari
```

Produces `build/out/atari/demo.tos`. Run in an ST emulator (Hatari) or on
real hardware.

### Amiga (vbcc or bebbo's gcc)

Requires either vbcc with the `vbcc68k_amigaos` target files, or
[bebbo's m68k-amigaos gcc](https://github.com/bebbo/amiga-gcc), plus the
Amiga NDK headers.

```
make -f build/Makefile.amiga                # vbcc (default)
make -f build/Makefile.amiga TOOLCHAIN=gcc   # bebbo's gcc
```

Produces `build/out/amiga/demo`. Run under WinUFAE/FS-UAE or on real
hardware.

### Classic Mac OS (Retro68)

Requires [Retro68](https://github.com/autc04/Retro68) built and
installed.

```
cd examples/mac
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=$RETRO68/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake ..
make
```

Produces a classic Mac application you can run under Mini vMac/Basilisk
II or on real Color QuickDraw-capable hardware (Mac II family or later,
or a compact Mac with System 6+ colour ROMs).

## API sketch

```c
GFX_ScreenMode mode = {320, 200, 4, /*double_buffer=*/1};
gfx_init(&mode);

GFX_Bitmap *screen = gfx_get_screen();
gfx_set_palette_entry(1, (GFX_RGB){255, 255, 255});

gfx_clear(screen, 0);
gfx_fill_rect(screen, &(GFX_Rect){20, 20, 80, 40}, 1);
gfx_line(screen, 0, 0, 319, 199, 1);
gfx_draw_text(screen, 8, 170, "hello", &gfx_font_default, 1, -1);

gfx_flip();
gfx_shutdown();
```

See `examples/{atari,amiga,mac}/demo.c` for complete, platform-specific
programs.

## License

GPLv3, see `LICENSE`.
