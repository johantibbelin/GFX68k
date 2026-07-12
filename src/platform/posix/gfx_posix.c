/*
 * POSIX development/test backend: a chunky 8-bit-per-pixel software
 * framebuffer allocated with plain malloc(). See gfx_posix.h for why
 * this exists.
 */
#include "gfx.h"
#include "gfx_posix.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GFX_Bitmap g_front;
static GFX_Bitmap g_back;
static GFX_Bitmap *g_screen; /* points at g_back if double-buffered, else g_front */
static int g_double_buffered;
static GFX_RGB g_palette[256];
static int g_initialized = 0;

static GFX_Bitmap *bitmap_alloc(int16_t width, int16_t height)
{
    GFX_Bitmap *bmp = (GFX_Bitmap *)malloc(sizeof(GFX_Bitmap));
    if (bmp == NULL) return NULL;

    bmp->width = width;
    bmp->height = height;
    bmp->bpp = 8;
    bmp->pixels = calloc((size_t)width * (size_t)height, 1);
    bmp->backend_data = NULL;

    if (bmp->pixels == NULL) {
        free(bmp);
        return NULL;
    }
    return bmp;
}

GFX_Error gfx_init(const GFX_ScreenMode *mode)
{
    int16_t width = 320, height = 200;
    uint16_t i;

    if (g_initialized) return GFX_OK;

    if (mode != NULL) {
        if (mode->width > 0)  width = mode->width;
        if (mode->height > 0) height = mode->height;
    }

    g_front.width = width;
    g_front.height = height;
    g_front.bpp = 8;
    g_front.pixels = calloc((size_t)width * (size_t)height, 1);
    g_front.backend_data = NULL;
    if (g_front.pixels == NULL) return GFX_ERR_NO_MEMORY;

    g_double_buffered = (mode != NULL) && mode->double_buffer;
    if (g_double_buffered) {
        g_back.width = width;
        g_back.height = height;
        g_back.bpp = 8;
        g_back.pixels = calloc((size_t)width * (size_t)height, 1);
        g_back.backend_data = NULL;
        if (g_back.pixels == NULL) {
            free(g_front.pixels);
            return GFX_ERR_NO_MEMORY;
        }
        g_screen = &g_back;
    } else {
        g_screen = &g_front;
    }

    /* Default grayscale ramp so an un-palettized demo still shows something
     * sane in the PPM dump. */
    for (i = 0; i < 256; i++) {
        g_palette[i].r = (uint8_t)i;
        g_palette[i].g = (uint8_t)i;
        g_palette[i].b = (uint8_t)i;
    }

    g_initialized = 1;
    return GFX_OK;
}

void gfx_shutdown(void)
{
    if (!g_initialized) return;
    free(g_front.pixels);
    g_front.pixels = NULL;
    if (g_double_buffered) {
        free(g_back.pixels);
        g_back.pixels = NULL;
    }
    g_screen = NULL;
    g_initialized = 0;
}

GFX_Bitmap *gfx_get_screen(void)
{
    return g_screen;
}

void gfx_flip(void)
{
    if (!g_initialized) return;
    if (g_double_buffered) {
        memcpy(g_front.pixels, g_back.pixels, (size_t)g_front.width * (size_t)g_front.height);
    }
    /* No real vertical blank on a host machine -- nothing to wait for. */
}

void gfx_set_palette_entry(uint8_t index, GFX_RGB color)
{
    g_palette[index] = color;
}

GFX_RGB gfx_get_palette_entry(uint8_t index)
{
    return g_palette[index];
}

void gfx_set_palette(const GFX_RGB *colors, uint8_t start, uint16_t count)
{
    uint16_t i;
    for (i = 0; i < count; i++) {
        uint16_t idx = (uint16_t)(start + i);
        if (idx > 255) break;
        g_palette[idx] = colors[i];
    }
}

void gfx_put_pixel(GFX_Bitmap *bmp, int16_t x, int16_t y, GFX_Color color)
{
    uint8_t *row;
    if (bmp == NULL || x < 0 || y < 0 || x >= bmp->width || y >= bmp->height) return;
    row = (uint8_t *)bmp->pixels + (size_t)y * (size_t)bmp->width;
    row[x] = color;
}

GFX_Color gfx_get_pixel(const GFX_Bitmap *bmp, int16_t x, int16_t y)
{
    const uint8_t *row;
    if (bmp == NULL || x < 0 || y < 0 || x >= bmp->width || y >= bmp->height) return 0;
    row = (const uint8_t *)bmp->pixels + (size_t)y * (size_t)bmp->width;
    return row[x];
}

void gfx_hline(GFX_Bitmap *bmp, int16_t x, int16_t y, int16_t w, GFX_Color color)
{
    uint8_t *row;
    int16_t x_end, cx;

    if (bmp == NULL || y < 0 || y >= bmp->height || w <= 0) return;

    x_end = (int16_t)(x + w);
    if (x < 0) x = 0;
    if (x_end > bmp->width) x_end = bmp->width;
    if (x >= x_end) return;

    row = (uint8_t *)bmp->pixels + (size_t)y * (size_t)bmp->width;
    for (cx = x; cx < x_end; cx++) row[cx] = color;
}

void gfx_vline(GFX_Bitmap *bmp, int16_t x, int16_t y, int16_t h, GFX_Color color)
{
    int16_t y_end, cy;

    if (bmp == NULL || x < 0 || x >= bmp->width || h <= 0) return;

    y_end = (int16_t)(y + h);
    if (y < 0) y = 0;
    if (y_end > bmp->height) y_end = bmp->height;

    for (cy = y; cy < y_end; cy++) {
        ((uint8_t *)bmp->pixels)[(size_t)cy * (size_t)bmp->width + x] = color;
    }
}

void gfx_blit(GFX_Bitmap *dst, int16_t dx, int16_t dy,
              const GFX_Bitmap *src, const GFX_Rect *src_rect)
{
    GFX_Rect full;
    int16_t row;

    if (dst == NULL || src == NULL) return;

    if (src_rect == NULL) {
        full.x = 0; full.y = 0; full.w = src->width; full.h = src->height;
        src_rect = &full;
    }

    for (row = 0; row < src_rect->h; row++) {
        int16_t sy = (int16_t)(src_rect->y + row);
        int16_t ty = (int16_t)(dy + row);
        int16_t w = src_rect->w;
        int16_t sx = src_rect->x;
        int16_t tx = dx;

        if (sy < 0 || sy >= src->height || ty < 0 || ty >= dst->height) continue;

        /* Clip against both source and destination bounds in x. */
        if (sx < 0) { tx = (int16_t)(tx - sx); w = (int16_t)(w + sx); sx = 0; }
        if (tx < 0) { sx = (int16_t)(sx - tx); w = (int16_t)(w + tx); tx = 0; }
        if (sx + w > src->width)  w = (int16_t)(src->width - sx);
        if (tx + w > dst->width)  w = (int16_t)(dst->width - tx);
        if (w <= 0) continue;

        memmove((uint8_t *)dst->pixels + (size_t)ty * (size_t)dst->width + tx,
                (const uint8_t *)src->pixels + (size_t)sy * (size_t)src->width + sx,
                (size_t)w);
    }
}

GFX_Bitmap *gfx_bitmap_create(int16_t width, int16_t height)
{
    return bitmap_alloc(width, height);
}

void gfx_bitmap_destroy(GFX_Bitmap *bmp)
{
    if (bmp == NULL) return;
    free(bmp->pixels);
    free(bmp);
}

GFX_Error gfx_posix_save_ppm(const GFX_Bitmap *bmp, const char *path)
{
    FILE *f;
    int16_t x, y;

    if (bmp == NULL || path == NULL) return GFX_ERR_INVALID_ARG;

    f = fopen(path, "wb");
    if (f == NULL) return GFX_ERR_INIT_FAILED;

    fprintf(f, "P6\n%d %d\n255\n", bmp->width, bmp->height);
    for (y = 0; y < bmp->height; y++) {
        const uint8_t *row = (const uint8_t *)bmp->pixels + (size_t)y * (size_t)bmp->width;
        for (x = 0; x < bmp->width; x++) {
            GFX_RGB c = g_palette[row[x]];
            fputc(c.r, f);
            fputc(c.g, f);
            fputc(c.b, f);
        }
    }

    fclose(f);
    return GFX_OK;
}
