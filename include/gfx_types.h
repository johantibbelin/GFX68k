/*
 * GFX68k - common types shared by every platform backend.
 *
 * Kept deliberately small: 68k C compilers (vbcc, Retro68/gcc) all support
 * C89/C99 stdint.h, so we lean on it rather than hand-rolling fixed types.
 */
#ifndef GFX68K_TYPES_H
#define GFX68K_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GFX_Point {
    int16_t x, y;
} GFX_Point;

typedef struct GFX_Rect {
    int16_t x, y;
    int16_t w, h;
} GFX_Rect;

/* 8-bit per channel RGB. Backends quantize down to whatever the hardware
 * palette actually supports (e.g. 3 bits/channel on plain ST, 4 on Amiga
 * OCS/ECS, up to 8 on AGA/Mac colour QuickDraw). */
typedef struct GFX_RGB {
    uint8_t r, g, b;
} GFX_RGB;

/* A palette index. All drawing primitives take colors as indices into the
 * active palette -- every target platform in scope (ST, Amiga, 68k Mac) is
 * happiest in indexed colour at the resolutions this library targets. */
typedef uint8_t GFX_Color;

/*
 * A drawing surface. Backends allocate/own `pixels` and may stash extra
 * bookkeeping in `backend_data` (e.g. an Amiga struct BitMap*, or a Mac
 * PixMapHandle) -- common code never looks inside either field, it only
 * calls backend primitives to do so. Pixel *layout* (chunky vs. bitplane)
 * is entirely a backend concern.
 */
typedef struct GFX_Bitmap {
    int16_t width;
    int16_t height;
    uint8_t bpp;            /* bits per pixel, informational only */
    void   *pixels;         /* backend-owned pixel storage */
    void   *backend_data;   /* backend-private handle, may be NULL */
} GFX_Bitmap;

/* A fixed-size bitmap font. Glyphs are stored MSB-first, one bit per pixel,
 * `glyph_h` rows of `(glyph_w+7)/8` bytes each, glyphs packed back to back
 * starting at `first_char`. See gfx_font.h for the drawing API. */
typedef struct GFX_Font {
    const uint8_t *glyphs;
    uint8_t glyph_w;
    uint8_t glyph_h;
    uint8_t first_char;
    uint8_t last_char;
} GFX_Font;

typedef enum GFX_Error {
    GFX_OK = 0,
    GFX_ERR_INIT_FAILED,
    GFX_ERR_NO_MEMORY,
    GFX_ERR_UNSUPPORTED_MODE,
    GFX_ERR_INVALID_ARG
} GFX_Error;

#ifdef __cplusplus
}
#endif

#endif /* GFX68K_TYPES_H */
