/*
 * strip_render.c — score-strip framebuffer software renderer
 *
 * Buffers a full 38,400-byte 8bpp pixel array locally; flushes to FPGA back
 * buffer in one tight loop on strip_present(). The flush is 9,600 32-bit
 * word writes; at ~4 LW cycles/transaction × 50 MHz that's ~770 µs, well
 * under one frame budget.
 */

#include "strip_render.h"
#include "font5x7.h"

#include <string.h>

static uint8_t scratch[STRIP_BYTES];

/* Cached pointers set once by strip_init() */
static volatile uint32_t *strip_fb_words;     /* avalon_slave_1 base, 32-bit */
static volatile uint8_t  *reg_strip_swap;     /* avalon_slave_0 + 0x05 */

void strip_init(volatile uint8_t *lw_bridge_base, uint32_t strip_offset,
                uint32_t reg_strip_swap_offset)
{
    strip_fb_words = (volatile uint32_t *)(lw_bridge_base + strip_offset);
    reg_strip_swap = lw_bridge_base + reg_strip_swap_offset;
    strip_clear(COLOR_STRIP_BG);
}

void strip_clear(uint8_t color)
{
    memset(scratch, color, STRIP_BYTES);
}

void strip_pixel(int x, int y, uint8_t color)
{
    if (x < 0 || x >= STRIP_W || y < 0 || y >= STRIP_H) return;
    scratch[y * STRIP_W + x] = color;
}

void strip_rect(int x, int y, int w, int h, uint8_t color)
{
    int x1 = x + w, y1 = y + h;
    if (x  < 0)        x  = 0;
    if (y  < 0)        y  = 0;
    if (x1 > STRIP_W)  x1 = STRIP_W;
    if (y1 > STRIP_H)  y1 = STRIP_H;
    for (int yy = y; yy < y1; yy++) {
        memset(&scratch[yy * STRIP_W + x], color, x1 - x);
    }
}

void strip_char(int x, int y, char c, int scale, uint8_t fg, uint8_t bg)
{
    const uint8_t *g = font5x7_glyph(c);
    if (scale < 1) scale = 1;
    for (int gx = 0; gx < FONT_W; gx++) {
        uint8_t col = g[gx];
        for (int gy = 0; gy < FONT_H; gy++) {
            uint8_t color = (col & (1u << gy)) ? fg : bg;
            /* Plot a scale×scale block per font pixel */
            int px = x + gx * scale;
            int py = y + gy * scale;
            for (int dy = 0; dy < scale; dy++)
                for (int dx = 0; dx < scale; dx++)
                    strip_pixel(px + dx, py + dy, color);
        }
    }
}

void strip_text(int x, int y, const char *s, int scale,
                uint8_t fg, uint8_t bg)
{
    int cx = x;
    /* 1-pixel inter-character gap × scale */
    int advance = (FONT_W + 1) * scale;
    while (*s) {
        strip_char(cx, y, *s, scale, fg, bg);
        cx += advance;
        s++;
    }
}

int strip_text_width(const char *s, int scale)
{
    if (scale < 1) scale = 1;
    int n = 0;
    for (const char *p = s; *p; p++) n++;
    if (n == 0) return 0;
    return n * FONT_W * scale + (n - 1) * scale;
}

void strip_text_centered(int y, const char *s, int scale,
                         uint8_t fg, uint8_t bg)
{
    int w = strip_text_width(s, scale);
    int x = (STRIP_W - w) / 2;
    if (x < 0) x = 0;
    strip_text(x, y, s, scale, fg, bg);
}

void strip_text_box(int x, int y, const char *s, int scale,
                    uint8_t fg, uint8_t bg, int pad)
{
    if (scale < 1) scale = 1;
    if (pad   < 0) pad   = 0;
    int tw = strip_text_width(s, scale);
    int th = FONT_H * scale;
    strip_rect(x, y, tw + 2 * pad, th + 2 * pad, bg);
    strip_text(x + pad, y + pad, s, scale, fg, bg);
}

void strip_present(void)
{
    /* Push 9,600 32-bit words to the back buffer. Each word holds 4 packed
     * 8-bit pixels in little-endian: word w → pixels 4w, 4w+1, 4w+2, 4w+3 */
    const uint32_t *src = (const uint32_t *)scratch;
    for (int w = 0; w < STRIP_WORDS; w++) {
        strip_fb_words[w] = src[w];
    }
    /* Arm the swap. Hardware will toggle the active buffer on the next VS. */
    *reg_strip_swap = 1;
}
