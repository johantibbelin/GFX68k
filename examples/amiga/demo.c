/*
 * Minimal Amiga demo: 320x256x16 custom screen, draws a few primitives
 * and some text, flips a double-buffered screen, waits for a mouse
 * click.
 *
 * Build: see build/Makefile.amiga (vbcc +amigaos or bebbo gcc).
 */
#include "gfx.h"

#include <exec/types.h>
#include <proto/dos.h>

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
    mode.height = 256;
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

    gfx_line(screen, 0, 0, 319, 255, 3);
    gfx_line(screen, 319, 0, 0, 255, 4);

    gfx_draw_text(screen, 8, 230, "GFX68k on Amiga", &gfx_font_default, 1, -1);

    gfx_flip();

    Delay(250); /* ~5 seconds at 50Hz PAL vblank ticks */

    gfx_shutdown();
    return 0;
}
