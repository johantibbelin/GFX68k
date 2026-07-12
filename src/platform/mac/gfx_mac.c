/*
 * Classic Mac OS (68k) backend: Color QuickDraw, via an offscreen 8-bit
 * indexed GWorld that we poke directly (chunky, one byte per pixel) and
 * present into a window with CopyBits(). Built with Retro68
 * (m68k-apple-macos gcc), see build/CMakeLists.mac.txt.
 *
 * Color QuickDraw (GWorld, 8-bit CLUT) requires a Mac II-class machine
 * or a compact Mac with System 6+ and a colour-capable ROM; it is not
 * available on the original 1-bit-only 128K/512K/Plus. That's the
 * tradeoff for a uniform indexed-colour API across all three platforms
 * in this library -- documented here rather than silently assumed.
 *
 * Not verified on real hardware/emulator from this sandbox (no Mac 68k
 * cross toolchain available here); written directly against documented
 * QuickDraw/GWorld APIs.
 */
#include "gfx.h"

#include <Quickdraw.h>
#include <QDOffscreen.h>
#include <Windows.h>
#include <Fonts.h>
#include <Menus.h>
#include <TextEdit.h>
#include <Dialogs.h>
#include <ToolUtils.h>

static GFX_Bitmap g_screen;
static WindowPtr   g_window;
static GWorldPtr   g_gworld;

static PixMapHandle pixmap_of(const GFX_Bitmap *bmp)
{
    return GetGWorldPixMap((GWorldPtr)bmp->backend_data);
}

GFX_Error gfx_init(const GFX_ScreenMode *mode)
{
    Rect bounds, win_bounds;
    int16_t width = 512, height = 342; /* compact Mac screen size default */

    if (mode != NULL) {
        if (mode->width > 0)  width = mode->width;
        if (mode->height > 0) height = mode->height;
    }

    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);
    InitCursor();

    SetRect(&bounds, 0, 0, width, height);
    if (NewGWorld(&g_gworld, 8, &bounds, NULL, NULL, 0) != noErr || g_gworld == NULL) {
        return GFX_ERR_INIT_FAILED;
    }
    LockPixels(GetGWorldPixMap(g_gworld));

    SetRect(&win_bounds, 40, 60, (short)(40 + width), (short)(60 + height));
    {
        /* Pascal string (length byte + chars) spelled out by hand rather
         * than relying on the MPW/CodeWarrior "\p" literal extension,
         * which plain GCC-based toolchains like Retro68 may not support. */
        static const unsigned char title[] = {6, 'G','F','X','6','8','k'};
        g_window = NewCWindow(NULL, &win_bounds, (ConstStr255Param)title,
                               true, documentProc, (WindowPtr)-1L, false, 0);
    }
    if (g_window == NULL) {
        DisposeGWorld(g_gworld);
        g_gworld = NULL;
        return GFX_ERR_INIT_FAILED;
    }
    SetPort((GrafPtr)g_window);

    g_screen.width = width;
    g_screen.height = height;
    g_screen.bpp = 8;
    g_screen.pixels = NULL; /* access is through the GWorld's PixMap, see pixmap_of() */
    g_screen.backend_data = g_gworld;

    return GFX_OK;
}

void gfx_shutdown(void)
{
    if (g_window != NULL) {
        DisposeWindow(g_window);
        g_window = NULL;
    }
    if (g_gworld != NULL) {
        DisposeGWorld(g_gworld);
        g_gworld = NULL;
    }
    g_screen.backend_data = NULL;
}

GFX_Bitmap *gfx_get_screen(void)
{
    return &g_screen;
}

void gfx_flip(void)
{
    PixMapHandle src_pm;
    Rect r;
    GrafPtr saved_port;

    if (g_gworld == NULL || g_window == NULL) return;

    GetPort(&saved_port);
    SetPort((GrafPtr)g_window);

    src_pm = GetGWorldPixMap(g_gworld);
    SetRect(&r, 0, 0, g_screen.width, g_screen.height);
    CopyBits((BitMap *)*src_pm, &((GrafPtr)g_window)->portBits, &r, &r, srcCopy, NULL);

    SetPort(saved_port);
}

void gfx_set_palette_entry(uint8_t index, GFX_RGB color)
{
    CTabHandle ctab;

    if (g_gworld == NULL) return;

    ctab = (**GetGWorldPixMap(g_gworld)).pmTable;
    (**ctab).ctTable[index].value = index;
    (**ctab).ctTable[index].rgb.red   = (unsigned short)((color.r << 8) | color.r);
    (**ctab).ctTable[index].rgb.green = (unsigned short)((color.g << 8) | color.g);
    (**ctab).ctTable[index].rgb.blue  = (unsigned short)((color.b << 8) | color.b);
    CTabChanged(ctab);
}

GFX_RGB gfx_get_palette_entry(uint8_t index)
{
    GFX_RGB c = {0, 0, 0};
    CTabHandle ctab;

    if (g_gworld == NULL) return c;

    ctab = (**GetGWorldPixMap(g_gworld)).pmTable;
    c.r = (uint8_t)((**ctab).ctTable[index].rgb.red >> 8);
    c.g = (uint8_t)((**ctab).ctTable[index].rgb.green >> 8);
    c.b = (uint8_t)((**ctab).ctTable[index].rgb.blue >> 8);
    return c;
}

void gfx_set_palette(const GFX_RGB *colors, uint8_t start, uint16_t count)
{
    uint16_t i;
    for (i = 0; i < count; i++) {
        uint16_t idx = (uint16_t)(start + i);
        if (idx > 255) break;
        gfx_set_palette_entry((uint8_t)idx, colors[i]);
    }
}

void gfx_put_pixel(GFX_Bitmap *bmp, int16_t x, int16_t y, GFX_Color color)
{
    PixMapHandle pm;
    Ptr base;
    short row_bytes;

    if (bmp == NULL || x < 0 || y < 0 || x >= bmp->width || y >= bmp->height) return;

    pm = pixmap_of(bmp);
    base = GetPixBaseAddr(pm);
    row_bytes = (short)((**pm).rowBytes & 0x3FFF);

    ((UInt8 *)base)[(long)y * row_bytes + x] = color;
}

GFX_Color gfx_get_pixel(const GFX_Bitmap *bmp, int16_t x, int16_t y)
{
    PixMapHandle pm;
    Ptr base;
    short row_bytes;

    if (bmp == NULL || x < 0 || y < 0 || x >= bmp->width || y >= bmp->height) return 0;

    pm = pixmap_of(bmp);
    base = GetPixBaseAddr(pm);
    row_bytes = (short)((**pm).rowBytes & 0x3FFF);

    return ((UInt8 *)base)[(long)y * row_bytes + x];
}

void gfx_hline(GFX_Bitmap *bmp, int16_t x, int16_t y, int16_t w, GFX_Color color)
{
    PixMapHandle pm;
    Ptr base;
    short row_bytes;
    int16_t x_end;

    if (bmp == NULL || y < 0 || y >= bmp->height || w <= 0) return;

    x_end = (int16_t)(x + w);
    if (x < 0) x = 0;
    if (x_end > bmp->width) x_end = bmp->width;
    if (x >= x_end) return;

    pm = pixmap_of(bmp);
    base = GetPixBaseAddr(pm);
    row_bytes = (short)((**pm).rowBytes & 0x3FFF);

    {
        UInt8 *row = (UInt8 *)base + (long)y * row_bytes;
        int16_t cx;
        for (cx = x; cx < x_end; cx++) row[cx] = color;
    }
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
    PixMapHandle src_pm, dst_pm;
    Rect src_r, dst_r;

    if (dst == NULL || src == NULL) return;

    src_pm = pixmap_of(src);
    dst_pm = pixmap_of(dst);

    if (src_rect != NULL) {
        SetRect(&src_r, src_rect->x, src_rect->y,
                (short)(src_rect->x + src_rect->w), (short)(src_rect->y + src_rect->h));
    } else {
        SetRect(&src_r, 0, 0, src->width, src->height);
    }
    SetRect(&dst_r, dx, dy,
            (short)(dx + (src_r.right - src_r.left)), (short)(dy + (src_r.bottom - src_r.top)));

    /* CopyBits does the actual work in the ROM/QuickDraw acceleration,
     * and clips for us -- the reason to prefer it over a per-pixel loop
     * here, same rationale as the Amiga blitter path. */
    CopyBits((BitMap *)*src_pm, (BitMap *)*dst_pm, &src_r, &dst_r, srcCopy, NULL);
}

GFX_Bitmap *gfx_bitmap_create(int16_t width, int16_t height)
{
    GFX_Bitmap *bmp;
    GWorldPtr gw;
    Rect bounds;

    if (width <= 0 || height <= 0) return NULL;

    SetRect(&bounds, 0, 0, width, height);
    if (NewGWorld(&gw, 8, &bounds, NULL, NULL, 0) != noErr || gw == NULL) return NULL;
    LockPixels(GetGWorldPixMap(gw));

    bmp = (GFX_Bitmap *)NewPtr(sizeof(GFX_Bitmap));
    if (bmp == NULL) {
        DisposeGWorld(gw);
        return NULL;
    }

    bmp->width = width;
    bmp->height = height;
    bmp->bpp = 8;
    bmp->pixels = NULL;
    bmp->backend_data = gw;

    return bmp;
}

void gfx_bitmap_destroy(GFX_Bitmap *bmp)
{
    if (bmp == NULL) return;
    if (bmp->backend_data != NULL) DisposeGWorld((GWorldPtr)bmp->backend_data);
    DisposePtr((Ptr)bmp);
}
