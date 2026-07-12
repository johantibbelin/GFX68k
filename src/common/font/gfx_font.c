/*
 * Bitmap font rendering. Backend-agnostic: draws through gfx_put_pixel()
 * only, so it works identically on every platform (and is slow on
 * purpose -- if you need fast text, blit pre-rendered strings instead).
 */
#include "gfx.h"

static const uint8_t *glyph_row(const GFX_Font *font, uint8_t ch, uint8_t row)
{
    uint16_t glyph_index;
    uint16_t bytes_per_row;
    uint16_t glyph_bytes;

    bytes_per_row = (uint16_t)((font->glyph_w + 7) / 8);
    glyph_bytes = (uint16_t)(bytes_per_row * font->glyph_h);
    glyph_index = (uint16_t)(ch - font->first_char);

    return font->glyphs + (uint32_t)glyph_index * glyph_bytes + (uint32_t)row * bytes_per_row;
}

void gfx_draw_text(GFX_Bitmap *bmp, int16_t x, int16_t y, const char *text,
                    const GFX_Font *font, GFX_Color fg_color, int16_t bg_color)
{
    int16_t pen_x;
    const char *p;

    if (bmp == NULL || text == NULL || font == NULL) return;

    pen_x = x;
    for (p = text; *p != '\0'; p++) {
        uint8_t ch = (uint8_t)*p;
        uint8_t row;

        if (ch < font->first_char || ch > font->last_char) {
            pen_x = (int16_t)(pen_x + font->glyph_w);
            continue;
        }

        for (row = 0; row < font->glyph_h; row++) {
            const uint8_t *bits = glyph_row(font, ch, row);
            uint8_t col;
            for (col = 0; col < font->glyph_w; col++) {
                uint8_t byte = bits[col / 8];
                uint8_t mask = (uint8_t)(0x80 >> (col % 8));
                if (byte & mask) {
                    gfx_put_pixel(bmp, (int16_t)(pen_x + col), (int16_t)(y + row), fg_color);
                } else if (bg_color >= 0) {
                    gfx_put_pixel(bmp, (int16_t)(pen_x + col), (int16_t)(y + row), (GFX_Color)bg_color);
                }
            }
        }

        pen_x = (int16_t)(pen_x + font->glyph_w);
    }
}

int16_t gfx_text_width(const char *text, const GFX_Font *font)
{
    int16_t len = 0;
    const char *p;

    if (text == NULL || font == NULL) return 0;
    for (p = text; *p != '\0'; p++) len++;

    return (int16_t)(len * font->glyph_w);
}
