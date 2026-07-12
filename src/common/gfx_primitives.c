/*
 * Backend-agnostic drawing built purely on top of the primitives every
 * platform backend provides (gfx_put_pixel, gfx_get_pixel, gfx_hline,
 * gfx_vline, gfx_blit). Compiled once, linked into every platform build.
 */
#include "gfx.h"

static int16_t min16(int16_t a, int16_t b) { return a < b ? a : b; }
static int16_t max16(int16_t a, int16_t b) { return a > b ? a : b; }

void gfx_clear(GFX_Bitmap *bmp, GFX_Color color)
{
    int16_t y;
    if (bmp == NULL) return;
    for (y = 0; y < bmp->height; y++) {
        gfx_hline(bmp, 0, y, bmp->width, color);
    }
}

void gfx_line(GFX_Bitmap *bmp, int16_t x0, int16_t y0, int16_t x1, int16_t y1, GFX_Color color)
{
    int16_t dx, dy, sx, sy, err, e2;

    if (bmp == NULL) return;

    if (y0 == y1) {
        int16_t x = min16(x0, x1);
        int16_t w = (int16_t)(max16(x0, x1) - x + 1);
        gfx_hline(bmp, x, y0, w, color);
        return;
    }
    if (x0 == x1) {
        int16_t y = min16(y0, y1);
        int16_t h = (int16_t)(max16(y0, y1) - y + 1);
        gfx_vline(bmp, x0, y, h, color);
        return;
    }

    /* Bresenham, integer-only -- fine for a 16MHz 68000. */
    dx = (int16_t)(x1 > x0 ? x1 - x0 : x0 - x1);
    dy = (int16_t)(y1 > y0 ? y0 - y1 : y1 - y0); /* negative abs(dy) */
    sx = (int16_t)(x0 < x1 ? 1 : -1);
    sy = (int16_t)(y0 < y1 ? 1 : -1);
    err = (int16_t)(dx + dy);

    for (;;) {
        gfx_put_pixel(bmp, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = (int16_t)(2 * err);
        if (e2 >= dy) { err = (int16_t)(err + dy); x0 = (int16_t)(x0 + sx); }
        if (e2 <= dx) { err = (int16_t)(err + dx); y0 = (int16_t)(y0 + sy); }
    }
}

void gfx_rect(GFX_Bitmap *bmp, const GFX_Rect *r, GFX_Color color)
{
    if (bmp == NULL || r == NULL || r->w <= 0 || r->h <= 0) return;

    gfx_hline(bmp, r->x, r->y, r->w, color);
    if (r->h > 1) {
        gfx_hline(bmp, r->x, (int16_t)(r->y + r->h - 1), r->w, color);
    }
    if (r->h > 2) {
        gfx_vline(bmp, r->x, (int16_t)(r->y + 1), (int16_t)(r->h - 2), color);
        if (r->w > 1) {
            gfx_vline(bmp, (int16_t)(r->x + r->w - 1), (int16_t)(r->y + 1), (int16_t)(r->h - 2), color);
        }
    }
}

void gfx_fill_rect(GFX_Bitmap *bmp, const GFX_Rect *r, GFX_Color color)
{
    int16_t y, y_end;

    if (bmp == NULL || r == NULL || r->w <= 0 || r->h <= 0) return;

    y_end = (int16_t)(r->y + r->h);
    for (y = r->y; y < y_end; y++) {
        gfx_hline(bmp, r->x, y, r->w, color);
    }
}

void gfx_blit_transparent(GFX_Bitmap *dst, int16_t dx, int16_t dy,
                           const GFX_Bitmap *src, const GFX_Rect *src_rect,
                           GFX_Color key_color)
{
    GFX_Rect full;
    int16_t sx, sy, x, y;

    if (dst == NULL || src == NULL) return;

    if (src_rect == NULL) {
        full.x = 0; full.y = 0; full.w = src->width; full.h = src->height;
        src_rect = &full;
    }

    for (y = 0; y < src_rect->h; y++) {
        sy = (int16_t)(src_rect->y + y);
        for (x = 0; x < src_rect->w; x++) {
            GFX_Color c;
            sx = (int16_t)(src_rect->x + x);
            c = gfx_get_pixel(src, sx, sy);
            if (c != key_color) {
                gfx_put_pixel(dst, (int16_t)(dx + x), (int16_t)(dy + y), c);
            }
        }
    }
}
