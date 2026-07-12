/*
 * Amiga backend: Exec/graphics.library/intuition.library, a custom
 * screen with non-interleaved bitplanes (standard Amiga BitMap layout,
 * as opposed to the ST's word-interleaved planes -- see gfx_atari.c for
 * the contrast). Built with vbcc (m68k-amigaos target) or bebbo's gcc
 * (m68k-amigaos), see build/Makefile.amiga.
 *
 * Not verified on real hardware/emulator from this sandbox (no Amiga
 * cross toolchain available here); written directly against documented
 * graphics.library/intuition.library APIs.
 *
 * Double buffering swaps the ViewPort's displayed BitMap with
 * ChangeVPBitMap() + WaitTOF(), the standard pre-AGA-and-still-works
 * technique. gfx_blit() uses the blitter (BltBitMap) instead of a
 * per-pixel copy, since that's the whole point of having one.
 */
#include "gfx.h"

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/view.h>
#include <graphics/gfxmacros.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

struct GfxBase *GfxBase;
struct IntuitionBase *IntuitionBase;

#define AMIGA_DEPTH 4 /* 16 colours: OCS/ECS-safe default */

static GFX_Bitmap g_screen;
static struct Screen *g_intuition_screen;
static struct BitMap *g_back_bitmap; /* NULL unless double-buffered */
static int g_double_buffered;

GFX_Error gfx_init(const GFX_ScreenMode *mode)
{
    struct NewScreen ns;
    int16_t width = 320, height = 256;

    if (mode != NULL) {
        if (mode->width > 0)  width = mode->width;
        if (mode->height > 0) height = mode->height;
    }

    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 0);
    if (GfxBase == NULL) return GFX_ERR_INIT_FAILED;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 0);
    if (IntuitionBase == NULL) {
        CloseLibrary((struct Library *)GfxBase);
        return GFX_ERR_INIT_FAILED;
    }

    ns.LeftEdge = 0;
    ns.TopEdge = 0;
    ns.Width = width;
    ns.Height = height;
    ns.Depth = AMIGA_DEPTH;
    ns.DetailPen = 0;
    ns.BlockPen = 1;
    ns.ViewModes = 0; /* normal (non-interlaced, non-HAM) */
    ns.Type = CUSTOMSCREEN;
    ns.Font = NULL;
    ns.DefaultTitle = (STRPTR) "GFX68k";
    ns.Gadgets = NULL;
    ns.CustomBitMap = NULL;

    g_intuition_screen = OpenScreen(&ns);
    if (g_intuition_screen == NULL) {
        CloseLibrary((struct Library *)IntuitionBase);
        CloseLibrary((struct Library *)GfxBase);
        return GFX_ERR_INIT_FAILED;
    }

    g_double_buffered = (mode != NULL) && mode->double_buffer;
    g_back_bitmap = NULL;

    if (g_double_buffered) {
        g_back_bitmap = AllocBitMap(width, height, AMIGA_DEPTH, BMF_CLEAR,
                                     g_intuition_screen->RastPort.BitMap);
        if (g_back_bitmap == NULL) {
            CloseScreen(g_intuition_screen);
            CloseLibrary((struct Library *)IntuitionBase);
            CloseLibrary((struct Library *)GfxBase);
            return GFX_ERR_NO_MEMORY;
        }
    }

    g_screen.width = width;
    g_screen.height = height;
    g_screen.bpp = AMIGA_DEPTH;
    g_screen.pixels = NULL;
    /* Draw into the back buffer if double-buffered, else straight into
     * the visible screen bitmap. */
    g_screen.backend_data = g_double_buffered
        ? (void *)g_back_bitmap
        : (void *)g_intuition_screen->RastPort.BitMap;

    return GFX_OK;
}

void gfx_shutdown(void)
{
    if (g_back_bitmap != NULL) {
        FreeBitMap(g_back_bitmap);
        g_back_bitmap = NULL;
    }
    if (g_intuition_screen != NULL) {
        CloseScreen(g_intuition_screen);
        g_intuition_screen = NULL;
    }
    if (IntuitionBase != NULL) { CloseLibrary((struct Library *)IntuitionBase); IntuitionBase = NULL; }
    if (GfxBase != NULL) { CloseLibrary((struct Library *)GfxBase); GfxBase = NULL; }
    g_screen.backend_data = NULL;
}

GFX_Bitmap *gfx_get_screen(void)
{
    return &g_screen;
}

void gfx_flip(void)
{
    if (g_double_buffered) {
        struct BitMap *shown = g_intuition_screen->RastPort.BitMap;
        ChangeVPBitMap(&g_intuition_screen->ViewPort, (struct BitMap *)g_screen.backend_data,
                       &g_intuition_screen->RastPort);
        g_screen.backend_data = shown; /* now-offscreen buffer becomes the draw target */
    }
    WaitTOF();
}

void gfx_set_palette_entry(uint8_t index, GFX_RGB color)
{
    /* 4 bits/channel (OCS/ECS colour registers). */
    SetRGB4(&g_intuition_screen->ViewPort, index,
             (UBYTE)(color.r >> 4), (UBYTE)(color.g >> 4), (UBYTE)(color.b >> 4));
}

GFX_RGB gfx_get_palette_entry(uint8_t index)
{
    /* graphics.library has no direct "get colour register" call; read
     * back through the ViewPort's colour map table instead. */
    struct ColorMap *cm = g_intuition_screen->ViewPort.ColorMap;
    ULONG rgb = GetRGB4(cm, index); /* 0x0RGB, 4 bits/channel */
    GFX_RGB c;
    c.r = (uint8_t)(((rgb >> 8) & 0x0F) << 4);
    c.g = (uint8_t)(((rgb >> 4) & 0x0F) << 4);
    c.b = (uint8_t)((rgb & 0x0F) << 4);
    return c;
}

void gfx_set_palette(const GFX_RGB *colors, uint8_t start, uint16_t count)
{
    uint16_t i;
    for (i = 0; i < count; i++) {
        uint16_t idx = (uint16_t)(start + i);
        if (idx >= (1u << AMIGA_DEPTH)) break;
        gfx_set_palette_entry((uint8_t)idx, colors[i]);
    }
}

static struct BitMap *bitmap_of(const GFX_Bitmap *bmp)
{
    return (struct BitMap *)bmp->backend_data;
}

void gfx_put_pixel(GFX_Bitmap *bmp, int16_t x, int16_t y, GFX_Color color)
{
    struct BitMap *bm;
    long row_offset;
    uint8_t bit, plane, depth;

    if (bmp == NULL || x < 0 || y < 0 || x >= bmp->width || y >= bmp->height) return;

    bm = bitmap_of(bmp);
    row_offset = (long)y * bm->BytesPerRow + (x >> 3);
    bit = (uint8_t)(0x80 >> (x & 7));
    depth = bm->Depth;

    for (plane = 0; plane < depth; plane++) {
        UBYTE *p = bm->Planes[plane] + row_offset;
        if (color & (1 << plane)) *p = (UBYTE)(*p | bit);
        else                      *p = (UBYTE)(*p & ~bit);
    }
}

GFX_Color gfx_get_pixel(const GFX_Bitmap *bmp, int16_t x, int16_t y)
{
    struct BitMap *bm;
    long row_offset;
    uint8_t bit, plane, depth;
    GFX_Color color = 0;

    if (bmp == NULL || x < 0 || y < 0 || x >= bmp->width || y >= bmp->height) return 0;

    bm = bitmap_of(bmp);
    row_offset = (long)y * bm->BytesPerRow + (x >> 3);
    bit = (uint8_t)(0x80 >> (x & 7));
    depth = bm->Depth;

    for (plane = 0; plane < depth; plane++) {
        if (bm->Planes[plane][row_offset] & bit) color = (GFX_Color)(color | (1 << plane));
    }
    return color;
}

void gfx_hline(GFX_Bitmap *bmp, int16_t x, int16_t y, int16_t w, GFX_Color color)
{
    int16_t x_end, cx;

    if (bmp == NULL || y < 0 || y >= bmp->height || w <= 0) return;

    x_end = (int16_t)(x + w);
    if (x < 0) x = 0;
    if (x_end > bmp->width) x_end = bmp->width;

    for (cx = x; cx < x_end; cx++) gfx_put_pixel(bmp, cx, y, color);
}

void gfx_vline(GFX_Bitmap *bmp, int16_t x, int16_t y, int16_t h, GFX_Color color)
{
    int16_t y_end, cy;

    if (bmp == NULL || x < 0 || x >= bmp->width || h <= 0) return;

    y_end = (int16_t)(y + h);
    if (y < 0) y = 0;
    if (y_end > bmp->height) y_end = bmp->height;

    for (cy = y; cy < y_end; cy++) gfx_put_pixel(bmp, x, cy, color);
}

void gfx_blit(GFX_Bitmap *dst, int16_t dx, int16_t dy,
              const GFX_Bitmap *src, const GFX_Rect *src_rect)
{
    GFX_Rect full;

    if (dst == NULL || src == NULL) return;

    if (src_rect == NULL) {
        full.x = 0; full.y = 0; full.w = src->width; full.h = src->height;
        src_rect = &full;
    }

    /* 0xC0 is the standard Blitter "minterm" cookie for a plain source
     * copy (D = S), ignoring the mask/tempA area-fill machinery. */
    BltBitMap(bitmap_of(src), src_rect->x, src_rect->y,
              bitmap_of(dst), dx, dy,
              src_rect->w, src_rect->h,
              0xC0, 0xFF, NULL);
}

GFX_Bitmap *gfx_bitmap_create(int16_t width, int16_t height)
{
    GFX_Bitmap *bmp;
    struct BitMap *bm;
    struct BitMap *friend_bm = g_intuition_screen != NULL ? g_intuition_screen->RastPort.BitMap : NULL;

    if (width <= 0 || height <= 0) return NULL;

    bmp = (GFX_Bitmap *)AllocMem(sizeof(GFX_Bitmap), MEMF_PUBLIC | MEMF_CLEAR);
    if (bmp == NULL) return NULL;

    bm = AllocBitMap(width, height, AMIGA_DEPTH, BMF_CLEAR, friend_bm);
    if (bm == NULL) {
        FreeMem(bmp, sizeof(GFX_Bitmap));
        return NULL;
    }

    bmp->width = width;
    bmp->height = height;
    bmp->bpp = AMIGA_DEPTH;
    bmp->pixels = NULL;
    bmp->backend_data = bm;

    return bmp;
}

void gfx_bitmap_destroy(GFX_Bitmap *bmp)
{
    if (bmp == NULL) return;
    if (bmp->backend_data != NULL) FreeBitMap((struct BitMap *)bmp->backend_data);
    FreeMem(bmp, sizeof(GFX_Bitmap));
}
