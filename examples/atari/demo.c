/*
 * Minimal Atari ST demo: ST low-rez (320x200x16), draws a few primitives
 * and some text, flips a double-buffered screen, waits for a keypress.
 *
 * Build: see build/Makefile.atari (vbcc, +tos target).
 */
#include "gfx.h"
#include <osbind.h>

int main(void)
{
    GFX_ScreenMode mode;
    GFX_Bitmap *screen;
    GFX_Rect r;
    GFX_RGB palette[16] = {
        {0, 0, 0}, {255, 255, 255}, {255, 0, 0}, {0, 255, 0},
        {0, 0, 255}, {255, 255, 0}, {0, 255, 255}, {255, 0, 255},
        {128, 128, 128}, {192, 192, 192}, {128, 0, 0}, {0, 128, 0},
        {0, 0, 128}, {128, 128, 0}, {0, 128, 128}, {128, 0, 128}
    };

    mode.width = 320;
    mode.height = 200;
    mode.bpp = 4;
    mode.double_buffer = 1;

    if (gfx_init(&mode) != GFX_OK) {
        return 1;
    }

    gfx_set_palette(palette, 0, 16);
    screen = gfx_get_screen();

    gfx_clear(screen, 0);

    r.x = 20; r.y = 20; r.w = 80; r.h = 40;
    gfx_fill_rect(screen, &r, 2);
    gfx_rect(screen, &r, 1);

    gfx_line(screen, 0, 0, 319, 199, 3);
    gfx_line(screen, 319, 0, 0, 199, 4);

    gfx_draw_text(screen, 8, 170, "GFX68k on Atari ST", &gfx_font_default, 1, -1);

    gfx_flip();

    Cconin(); /* wait for a key before restoring the old screen mode */

    gfx_shutdown();
    return 0;
}
