/*
 * Minimal classic Mac OS demo: opens a 320x200 window backed by an
 * 8-bit indexed GWorld, draws a few primitives and some text, flips,
 * waits for a mouse click or key press.
 *
 * Build: see examples/mac/CMakeLists.txt (Retro68).
 */
#include "gfx.h"

#include <Events.h>

int main(void)
{
    GFX_ScreenMode mode;
    GFX_Bitmap *screen;
    GFX_Rect r;
    EventRecord event;
    GFX_RGB palette[16] = {
        {0, 0, 0}, {255, 255, 255}, {255, 0, 0}, {0, 255, 0},
        {0, 0, 255}, {255, 255, 0}, {0, 255, 255}, {255, 0, 255},
        {128, 128, 128}, {192, 192, 192}, {128, 0, 0}, {0, 128, 0},
        {0, 0, 128}, {128, 128, 0}, {0, 128, 128}, {128, 0, 128}
    };

    mode.width = 320;
    mode.height = 200;
    mode.bpp = 8;
    mode.double_buffer = 0;

    if (gfx_init(&mode) != GFX_OK) {
        return 1;
    }

    gfx_set_palette(palette, 0, 16);
    screen = gfx_get_screen();

    gfx_clear(screen, 1); /* white background */

    r.x = 20; r.y = 20; r.w = 80; r.h = 40;
    gfx_fill_rect(screen, &r, 2);
    gfx_rect(screen, &r, 0);

    gfx_line(screen, 0, 0, 319, 199, 4);
    gfx_line(screen, 319, 0, 0, 199, 6);

    gfx_draw_text(screen, 8, 170, "GFX68k on Mac 68k", &gfx_font_default, 0, -1);

    gfx_flip();

    for (;;) {
        if (WaitNextEvent(everyEvent, &event, 30, NULL)) {
            if (event.what == mouseDown || event.what == keyDown) break;
        }
    }

    gfx_shutdown();
    return 0;
}
