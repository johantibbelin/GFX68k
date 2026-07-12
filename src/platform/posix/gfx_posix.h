/*
 * Extra, POSIX-only helpers not part of the portable gfx.h API.
 *
 * This backend is NOT one of the three shipped targets (Atari ST, Amiga,
 * Mac 68k) -- it's an in-memory chunky 8bpp software framebuffer that
 * compiles and runs with an ordinary host gcc, used purely so the
 * platform-agnostic src/common/ code can be built, run and inspected on a
 * normal machine (see tests/). Ship builds never link this in.
 */
#ifndef GFX68K_POSIX_H
#define GFX68K_POSIX_H

#include "gfx.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Dumps `bmp` to a binary PPM (P6) file at `path` using the current
 * palette, for visual inspection (e.g. `open`/`eog`/`feh` the result, or
 * diff two runs). Returns GFX_OK on success. */
GFX_Error gfx_posix_save_ppm(const GFX_Bitmap *bmp, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* GFX68K_POSIX_H */
