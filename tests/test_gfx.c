/*
 * Host-side smoke test for the platform-agnostic src/common/ code,
 * exercised through the POSIX software backend. Not a target platform
 * (see src/platform/posix/gfx_posix.h) -- this is the only way to
 * actually run gfx68k code without an Atari/Amiga/Mac 68k cross toolchain
 * and an emulator.
 *
 * Checks primitive correctness with assertions, and writes a handful of
 * PPM files to the path given on argv[1] (default: ".") for a human to
 * eyeball text rendering, which is hard to assert on pixel-for-pixel.
 */
#include "gfx.h"
#include "gfx_posix.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static char g_outdir[512] = ".";

static void save(const GFX_Bitmap *bmp, const char *name)
{
    char path[600];
    GFX_Error err;
    snprintf(path, sizeof(path), "%s/%s.ppm", g_outdir, name);
    err = gfx_posix_save_ppm(bmp, path);
    assert(err == GFX_OK);
    printf("  wrote %s\n", path);
}

static void test_pixel_and_hline_vline(void)
{
    GFX_Bitmap *bmp;
    GFX_ScreenMode mode = {64, 64, 8, 0};
    int x, y;

    printf("test_pixel_and_hline_vline\n");
    assert(gfx_init(&mode) == GFX_OK);
    bmp = gfx_get_screen();
    assert(bmp != NULL);
    assert(bmp->width == 64 && bmp->height == 64);

    gfx_clear(bmp, 0);
    for (x = 0; x < 64; x++)
        for (y = 0; y < 64; y++)
            assert(gfx_get_pixel(bmp, x, y) == 0);

    gfx_put_pixel(bmp, 10, 20, 7);
    assert(gfx_get_pixel(bmp, 10, 20) == 7);
    assert(gfx_get_pixel(bmp, 11, 20) == 0);

    /* Out-of-bounds writes/reads must not crash and must read back 0. */
    gfx_put_pixel(bmp, -1, -1, 9);
    gfx_put_pixel(bmp, 1000, 1000, 9);
    assert(gfx_get_pixel(bmp, -1, -1) == 0);
    assert(gfx_get_pixel(bmp, 1000, 1000) == 0);

    gfx_hline(bmp, 5, 5, 10, 3);
    for (x = 5; x < 15; x++) assert(gfx_get_pixel(bmp, x, 5) == 3);
    assert(gfx_get_pixel(bmp, 4, 5) == 0);
    assert(gfx_get_pixel(bmp, 15, 5) == 0);

    gfx_vline(bmp, 30, 5, 10, 4);
    for (y = 5; y < 15; y++) assert(gfx_get_pixel(bmp, 30, y) == 4);
    assert(gfx_get_pixel(bmp, 30, 4) == 0);
    assert(gfx_get_pixel(bmp, 30, 15) == 0);

    /* Clipped hline/vline: partially off-screen must not crash and must
     * only draw the in-bounds portion. */
    gfx_hline(bmp, 60, 0, 10, 5);
    assert(gfx_get_pixel(bmp, 63, 0) == 5);
    gfx_vline(bmp, 0, 60, 10, 5);
    assert(gfx_get_pixel(bmp, 0, 63) == 5);

    gfx_shutdown();
    printf("  OK\n");
}

static void test_lines_and_rects(void)
{
    GFX_Bitmap *bmp;
    GFX_ScreenMode mode = {128, 128, 8, 0};
    GFX_Rect r;
    int x, y;

    printf("test_lines_and_rects\n");
    assert(gfx_init(&mode) == GFX_OK);
    bmp = gfx_get_screen();
    gfx_clear(bmp, 0);

    /* Diagonal line endpoints must land exactly on start/end. */
    gfx_line(bmp, 0, 0, 20, 20, 2);
    assert(gfx_get_pixel(bmp, 0, 0) == 2);
    assert(gfx_get_pixel(bmp, 20, 20) == 2);

    r.x = 10; r.y = 10; r.w = 30; r.h = 20;
    gfx_rect(bmp, &r, 6);
    /* Outline corners set... */
    assert(gfx_get_pixel(bmp, 10, 10) == 6);
    assert(gfx_get_pixel(bmp, 39, 10) == 6);
    assert(gfx_get_pixel(bmp, 10, 29) == 6);
    assert(gfx_get_pixel(bmp, 39, 29) == 6);
    /* ...but the interior must be untouched (outline, not filled). */
    assert(gfx_get_pixel(bmp, 25, 20) == 0);

    r.x = 60; r.y = 60; r.w = 20; r.h = 15;
    gfx_fill_rect(bmp, &r, 8);
    for (y = r.y; y < r.y + r.h; y++)
        for (x = r.x; x < r.x + r.w; x++)
            assert(gfx_get_pixel(bmp, x, y) == 8);
    assert(gfx_get_pixel(bmp, r.x - 1, r.y) == 0);
    assert(gfx_get_pixel(bmp, r.x + r.w, r.y) == 0);

    save(bmp, "lines_and_rects");
    gfx_shutdown();
    printf("  OK\n");
}

static void test_palette(void)
{
    GFX_ScreenMode mode = {16, 16, 8, 0};
    GFX_RGB c;
    GFX_RGB ramp[4] = {{1,2,3},{4,5,6},{7,8,9},{10,11,12}};

    printf("test_palette\n");
    assert(gfx_init(&mode) == GFX_OK);

    gfx_set_palette_entry(5, (GFX_RGB){255, 0, 128});
    c = gfx_get_palette_entry(5);
    assert(c.r == 255 && c.g == 0 && c.b == 128);

    gfx_set_palette(ramp, 100, 4);
    c = gfx_get_palette_entry(101);
    assert(c.r == 4 && c.g == 5 && c.b == 6);

    gfx_shutdown();
    printf("  OK\n");
}

static void test_blit_and_transparency(void)
{
    GFX_Bitmap *screen, *sprite;
    GFX_ScreenMode mode = {32, 32, 8, 0};
    GFX_Rect r;
    int x, y;

    printf("test_blit_and_transparency\n");
    assert(gfx_init(&mode) == GFX_OK);
    screen = gfx_get_screen();
    gfx_clear(screen, 1);

    sprite = gfx_bitmap_create(8, 8);
    assert(sprite != NULL);
    r.x = 0; r.y = 0; r.w = 8; r.h = 8;
    gfx_fill_rect(sprite, &r, 9);
    /* Punch a transparent-key hole in the middle. */
    gfx_put_pixel(sprite, 4, 4, 0);

    gfx_blit(screen, 2, 2, sprite, NULL);
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++)
            assert(gfx_get_pixel(screen, 2 + x, 2 + y) == (x == 4 && y == 4 ? 0 : 9));

    gfx_clear(screen, 1);
    gfx_blit_transparent(screen, 2, 2, sprite, NULL, 0);
    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            GFX_Color expect = (x == 4 && y == 4) ? 1 /* untouched background */ : 9;
            assert(gfx_get_pixel(screen, 2 + x, 2 + y) == expect);
        }
    }

    gfx_bitmap_destroy(sprite);
    gfx_shutdown();
    printf("  OK\n");
}

static void test_text(void)
{
    GFX_Bitmap *bmp;
    GFX_ScreenMode mode = {320, 64, 8, 0};
    int16_t w;

    printf("test_text\n");
    assert(gfx_init(&mode) == GFX_OK);
    bmp = gfx_get_screen();
    gfx_clear(bmp, 0);
    gfx_set_palette_entry(15, (GFX_RGB){255, 255, 255});

    w = gfx_text_width("GFX68k", &gfx_font_default);
    assert(w == 6 * gfx_font_default.glyph_w);

    gfx_draw_text(bmp, 4, 4, "GFX68k 0123456789", &gfx_font_default, 15, -1);
    gfx_draw_text(bmp, 4, 20, "!\"#$%&'()*+,-./:;<=>?@", &gfx_font_default, 15, -1);
    gfx_draw_text(bmp, 4, 36, "the quick brown FOX", &gfx_font_default, 15, -1);
    /* The glyph for 'G' (0x47) must have painted at least one foreground
     * pixel at the very top-left cell -- catches an all-zero/garbled font
     * table regression without requiring a human to look at every run. */
    {
        int found = 0, x, y;
        for (y = 4; y < 12 && !found; y++)
            for (x = 4; x < 12; x++)
                if (gfx_get_pixel(bmp, x, y) == 15) { found = 1; break; }
        assert(found);
    }

    save(bmp, "text");
    gfx_shutdown();
    printf("  OK\n");
}

int main(int argc, char **argv)
{
    if (argc > 1) {
        strncpy(g_outdir, argv[1], sizeof(g_outdir) - 1);
    }

    test_pixel_and_hline_vline();
    test_lines_and_rects();
    test_palette();
    test_blit_and_transparency();
    test_text();

    printf("All tests passed.\n");
    return 0;
}
