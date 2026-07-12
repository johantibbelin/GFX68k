/*
 * Atari ST backend: XBIOS, ST low resolution (320x200, 4 bitplanes / 16
 * colors). Built with vbcc (m68k-atari-mint or m68k-atari-tos targets,
 * see build/Makefile.atari) -- untested on real hardware/emulator from
 * this sandbox (no 68k toolchain available here); written directly
 * against the documented XBIOS calls and the well-known ST low-rez
 * bitplane layout.
 *
 * Only ST low-rez is supported for now: it's the one 16-colour mode
 * every target machine in scope (ST/STE) has, and it keeps the palette
 * and bitplane math simple. Medium/high-rez support is future work.
 *
 * Screen layout: word-interleaved bitplanes. Each row is
 * (width/16) groups of 4 consecutive 16-bit words (one per plane, plane
 * 0 first), MSB of each word is the leftmost pixel of that 16-pixel
 * group. This is standard for the ST (and, not coincidentally, for the
 * Amiga -- see gfx_amiga.c).
 */
#include "gfx.h"
#include <osbind.h>

#define ATARI_PLANES     4
#define ATARI_WIDTH      320
#define ATARI_HEIGHT     200
#define ATARI_ROW_BYTES  ((ATARI_WIDTH / 16) * ATARI_PLANES * 2)
#define ATARI_SCREEN_BYTES ((long)ATARI_ROW_BYTES * ATARI_HEIGHT)

static GFX_Bitmap g_screen;
static void *g_screen_mem_raw[2];  /* unaligned allocations, for Mfree */
static int g_double_buffered;
static int16_t g_old_rez = -1;
static void *g_old_physbase;
static void *g_old_logbase;

/* Atari 68000s require the screen base to be 256-byte aligned. */
static void *alloc_screen_buffer(void **raw_out)
{
    uint8_t *raw = (uint8_t *)Malloc(ATARI_SCREEN_BYTES + 256);
    uint8_t *aligned;
    if (raw == NULL) return NULL;
    aligned = (uint8_t *)(((uint32_t)raw + 255) & ~255UL);
    *raw_out = raw;
    return aligned;
}

GFX_Error gfx_init(const GFX_ScreenMode *mode)
{
    void *buf0;

    if (mode != NULL && mode->width > 0 && mode->height > 0 &&
        (mode->width != ATARI_WIDTH || mode->height != ATARI_HEIGHT)) {
        return GFX_ERR_UNSUPPORTED_MODE;
    }

    g_double_buffered = (mode != NULL) && mode->double_buffer;

    buf0 = alloc_screen_buffer(&g_screen_mem_raw[0]);
    if (buf0 == NULL) return GFX_ERR_NO_MEMORY;

    if (g_double_buffered) {
        void *buf1 = alloc_screen_buffer(&g_screen_mem_raw[1]);
        if (buf1 == NULL) {
            Mfree(g_screen_mem_raw[0]);
            return GFX_ERR_NO_MEMORY;
        }
        /* Draw into buf1 while buf0 (still blank) is shown first. */
        g_screen.pixels = buf1;
        g_screen.backend_data = buf0;
    } else {
        g_screen.pixels = buf0;
        g_screen.backend_data = NULL;
    }

    g_screen.width = ATARI_WIDTH;
    g_screen.height = ATARI_HEIGHT;
    g_screen.bpp = ATARI_PLANES;

    g_old_physbase = Physbase();
    g_old_logbase = Logbase();
    g_old_rez = Getrez();

    /* logLoc = buffer we draw into, physLoc = buffer shown first (buf0
     * in both single- and double-buffered cases). */
    Setscreen((long)g_screen.pixels, (long)buf0, 0);

    return GFX_OK;
}

void gfx_shutdown(void)
{
    Setscreen((long)g_old_logbase, (long)g_old_physbase, g_old_rez);

    Mfree(g_screen_mem_raw[0]);
    if (g_double_buffered) Mfree(g_screen_mem_raw[1]);
    g_screen.pixels = NULL;
    g_screen.backend_data = NULL;
}

GFX_Bitmap *gfx_get_screen(void)
{
    return &g_screen;
}

void gfx_flip(void)
{
    if (g_double_buffered) {
        void *finished = g_screen.pixels;
        void *now_offscreen = g_screen.backend_data;

        Setscreen(-1L, (long)finished, -1);
        g_screen.pixels = now_offscreen;
        g_screen.backend_data = finished;
    }
    Vsync();
}

void gfx_set_palette_entry(uint8_t index, GFX_RGB color)
{
    /* Plain ST hardware: 3 bits/channel, word format 0000 0RRR 0GGG 0BBB. */
    uint16_t word = (uint16_t)(((color.r >> 5) << 8) |
                                ((color.g >> 5) << 4) |
                                 (color.b >> 5));
    Setcolor((int16_t)index, (int16_t)word);
}

GFX_RGB gfx_get_palette_entry(uint8_t index)
{
    /* Setcolor(-1) is the documented way to *read* a colour register
     * (any negative value leaves it unchanged and returns the old one). */
    int16_t word = Setcolor((int16_t)index, -1);
    GFX_RGB c;
    c.r = (uint8_t)(((word >> 8) & 0x07) << 5);
    c.g = (uint8_t)(((word >> 4) & 0x07) << 5);
    c.b = (uint8_t)((word & 0x07) << 5);
    return c;
}

void gfx_set_palette(const GFX_RGB *colors, uint8_t start, uint16_t count)
{
    uint16_t i;
    for (i = 0; i < count; i++) {
        uint16_t idx = (uint16_t)(start + i);
        if (idx > 15) break;
        gfx_set_palette_entry((uint8_t)idx, colors[i]);
    }
}

static uint16_t *plane_word(const GFX_Bitmap *bmp, int16_t x, int16_t y)
{
    long row_offset = (long)y * ATARI_ROW_BYTES;
    long group_offset = (long)(x >> 4) * (ATARI_PLANES * 2);
    return (uint16_t *)((uint8_t *)bmp->pixels + row_offset + group_offset);
}

void gfx_put_pixel(GFX_Bitmap *bmp, int16_t x, int16_t y, GFX_Color color)
{
    uint16_t *word;
    uint16_t bit;
    uint8_t plane;

    if (bmp == NULL || x < 0 || y < 0 || x >= bmp->width || y >= bmp->height) return;

    word = plane_word(bmp, x, y);
    bit = (uint16_t)(0x8000 >> (x & 15));

    for (plane = 0; plane < ATARI_PLANES; plane++) {
        if (color & (1 << plane)) {
            word[plane] = (uint16_t)(word[plane] | bit);
        } else {
            word[plane] = (uint16_t)(word[plane] & ~bit);
        }
    }
}

GFX_Color gfx_get_pixel(const GFX_Bitmap *bmp, int16_t x, int16_t y)
{
    const uint16_t *word;
    uint16_t bit;
    uint8_t plane;
    GFX_Color color = 0;

    if (bmp == NULL || x < 0 || y < 0 || x >= bmp->width || y >= bmp->height) return 0;

    word = plane_word(bmp, x, y);
    bit = (uint16_t)(0x8000 >> (x & 15));

    for (plane = 0; plane < ATARI_PLANES; plane++) {
        if (word[plane] & bit) color = (GFX_Color)(color | (1 << plane));
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
    /* Generic per-pixel fallback. A real implementation would special-case
     * the (very common) 16-pixel-aligned, same-width case with a raw word
     * copy loop; left as future work since it needs real-hardware timing
     * to validate. */
    GFX_Rect full;
    int16_t row, col;

    if (dst == NULL || src == NULL) return;

    if (src_rect == NULL) {
        full.x = 0; full.y = 0; full.w = src->width; full.h = src->height;
        src_rect = &full;
    }

    for (row = 0; row < src_rect->h; row++) {
        for (col = 0; col < src_rect->w; col++) {
            GFX_Color c = gfx_get_pixel(src, (int16_t)(src_rect->x + col), (int16_t)(src_rect->y + row));
            gfx_put_pixel(dst, (int16_t)(dx + col), (int16_t)(dy + row), c);
        }
    }
}

GFX_Bitmap *gfx_bitmap_create(int16_t width, int16_t height)
{
    GFX_Bitmap *bmp;
    long row_bytes = ((long)width / 16) * ATARI_PLANES * 2;
    long size = row_bytes * height;
    void *raw;
    void *aligned_mem;

    if (width <= 0 || height <= 0 || (width & 15) != 0) return NULL;

    bmp = (GFX_Bitmap *)Malloc(sizeof(GFX_Bitmap));
    if (bmp == NULL) return NULL;

    raw = Malloc(size + 2);
    if (raw == NULL) { Mfree(bmp); return NULL; }
    /* gfx_put_pixel() does word-sized plane accesses -- the 68000 bus
     * faults on an odd-address word access, so this must be even. */
    aligned_mem = (void *)(((uint32_t)raw + 1) & ~1UL);

    bmp->width = width;
    bmp->height = height;
    bmp->bpp = ATARI_PLANES;
    bmp->pixels = aligned_mem;
    bmp->backend_data = raw;

    {
        long i;
        uint8_t *p = (uint8_t *)aligned_mem;
        for (i = 0; i < size; i++) p[i] = 0;
    }

    return bmp;
}

void gfx_bitmap_destroy(GFX_Bitmap *bmp)
{
    if (bmp == NULL) return;
    Mfree(bmp->backend_data);
    Mfree(bmp);
}
