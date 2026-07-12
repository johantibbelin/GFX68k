/*
 * GFX68k public API.
 *
 * One header, one API, three backends: exactly one of GFX_PLATFORM_ATARI,
 * GFX_PLATFORM_AMIGA or GFX_PLATFORM_MAC is defined by the platform's
 * Makefile before this header is included (see build directory). There is no
 * runtime backend dispatch: you link against the single platform's object
 * files, so calls resolve at link time with zero indirection overhead --
 * important on 8MHz/16MHz 68000 parts.
 *
 * Functions are split into two groups:
 *
 *   - Primitives (put/get pixel, hline, vline, blit, palette, bitmap
 *     alloc, mode init) are implemented once per platform in
 *     src/platform/<platform>/gfx_<platform>.c, in whatever way is native
 *     and fast for that hardware (bitplane, chunky, QuickDraw, ...).
 *
 *   - Everything else (line, rect, fill_rect, clear, text, transparent
 *     blit) is implemented once in src/common/ purely in terms of the
 *     primitives above, so it's shared byte-for-byte across all three
 *     platforms.
 */
#ifndef GFX68K_H
#define GFX68K_H

#include "gfx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Screen modes -------------------------------------------------- */

/*
 * Requested display mode. Backends pick the closest native mode and may
 * reject combinations they can't support (see gfx_init()). width/height
 * of 0 means "use the platform's default/native mode".
 */
typedef struct GFX_ScreenMode {
    int16_t width;
    int16_t height;
    uint8_t bpp;       /* bits per pixel: 1, 2, 4 or 8 */
    uint8_t double_buffer; /* nonzero: allocate a back buffer, see gfx_flip() */
} GFX_ScreenMode;

/* ---- Lifecycle ------------------------------------------------------ */

/* Initializes the display in the requested mode. Returns GFX_OK on
 * success; on failure the library is left uninitialized and gfx_init()
 * may be retried with a different mode. */
GFX_Error   gfx_init(const GFX_ScreenMode *mode);

/* Restores whatever display state the platform had before gfx_init(). */
void        gfx_shutdown(void);

/* Returns the screen bitmap: the one you draw to and that gfx_flip()
 * presents. Never destroy it yourself. */
GFX_Bitmap *gfx_get_screen(void);

/* Presents the back buffer (if double_buffer was requested) and waits for
 * vertical blank. If double buffering wasn't requested this is just a
 * vsync wait. */
void        gfx_flip(void);

/* ---- Palette ---------------------------------------------------------
 * No-ops (but safe to call) on backends running in a mode with no
 * indexed palette (e.g. 1-bit Mac QuickDraw). */

void        gfx_set_palette_entry(uint8_t index, GFX_RGB color);
GFX_RGB     gfx_get_palette_entry(uint8_t index);
void        gfx_set_palette(const GFX_RGB *colors, uint8_t start, uint16_t count);

/* ---- Primitives (backend-implemented) -------------------------------- */

void        gfx_put_pixel(GFX_Bitmap *bmp, int16_t x, int16_t y, GFX_Color color);
GFX_Color   gfx_get_pixel(const GFX_Bitmap *bmp, int16_t x, int16_t y);
void        gfx_hline(GFX_Bitmap *bmp, int16_t x, int16_t y, int16_t w, GFX_Color color);
void        gfx_vline(GFX_Bitmap *bmp, int16_t x, int16_t y, int16_t h, GFX_Color color);

/* Raw, unclipped, unkeyed rectangular copy of src_rect (in src's own
 * coordinates) to (dx, dy) in dst. Pass src_rect == NULL to copy all of
 * src. This is the one primitive worth optimizing per-platform (Amiga
 * blitter hardware, Mac CopyBits, ST VDI). */
void        gfx_blit(GFX_Bitmap *dst, int16_t dx, int16_t dy,
                      const GFX_Bitmap *src, const GFX_Rect *src_rect);

/* Allocates an offscreen bitmap in whatever memory the platform needs
 * (e.g. Amiga chip RAM) using the screen's current pixel format. */
GFX_Bitmap *gfx_bitmap_create(int16_t width, int16_t height);
void        gfx_bitmap_destroy(GFX_Bitmap *bmp);

/* ---- Derived drawing (shared, src/common) ----------------------------- */

void        gfx_clear(GFX_Bitmap *bmp, GFX_Color color);
void        gfx_line(GFX_Bitmap *bmp, int16_t x0, int16_t y0, int16_t x1, int16_t y1, GFX_Color color);
void        gfx_rect(GFX_Bitmap *bmp, const GFX_Rect *r, GFX_Color color);
void        gfx_fill_rect(GFX_Bitmap *bmp, const GFX_Rect *r, GFX_Color color);

/* Per-pixel copy of src_rect to (dx, dy) skipping any source pixel equal
 * to key_color. Generic fallback (uses get/put pixel), so it's the same
 * cost on every backend -- fine for sprites, not for big background
 * blits (use gfx_blit for those). */
void        gfx_blit_transparent(GFX_Bitmap *dst, int16_t dx, int16_t dy,
                                  const GFX_Bitmap *src, const GFX_Rect *src_rect,
                                  GFX_Color key_color);

/* ---- Text (shared, src/common) ---------------------------------------- */

/* Built-in minimal font: digits, uppercase A-Z, space and basic
 * punctuation (see src/common/font/font_default.c for the exact
 * coverage). Enough for HUDs/debug text; supply your own GFX_Font for
 * anything more. */
extern const GFX_Font gfx_font_default;

/* Draws `text` with glyphs from `font` starting at (x, y), advancing by
 * font->glyph_w per character (no kerning, no wrapping). Characters
 * outside [font->first_char, font->last_char] are skipped. If bg_color
 * is negative, background pixels are left untouched (transparent text);
 * otherwise pass a GFX_Color widened to int16_t. */
void        gfx_draw_text(GFX_Bitmap *bmp, int16_t x, int16_t y, const char *text,
                           const GFX_Font *font, GFX_Color fg_color, int16_t bg_color);

/* Width in pixels gfx_draw_text() would occupy for `text` in `font`. */
int16_t     gfx_text_width(const char *text, const GFX_Font *font);

#ifdef __cplusplus
}
#endif

#endif /* GFX68K_H */
