/*
 * strip_render.h — software rendering into the score-strip framebuffer
 *
 * The strip is 640×60 pixels, 8 bits-per-pixel palette indices. Software
 * draws into a local 38,400-byte buffer with the primitives below, then
 * calls strip_present() to bulk-write the buffer to the FPGA back buffer
 * and arm a vsync-aligned swap.
 *
 * Palette (matches the 8-color case statement in go_peripheral.sv):
 *   0 = bg dark gray  1 = white text   2 = light gray   3 = green
 *   4 = gold (winner) 5 = red (error)  6 = blue         7 = burlywood
 */

#ifndef _STRIP_RENDER_H
#define _STRIP_RENDER_H

#include <stdint.h>

#define STRIP_W       640
#define STRIP_H       60
#define STRIP_BYTES   (STRIP_W * STRIP_H)   /* 38,400 */
#define STRIP_WORDS   (STRIP_BYTES / 4)     /* 9,600  */

#define COLOR_STRIP_BG       0
#define COLOR_STRIP_WHITE    1
#define COLOR_STRIP_GRAY     2
#define COLOR_STRIP_GREEN    3
#define COLOR_STRIP_GOLD     4
#define COLOR_STRIP_RED      5
#define COLOR_STRIP_BLUE     6
#define COLOR_STRIP_BURLY    7

/* Once at startup: the caller passes the mmap'd LW-bridge byte pointer
 * (so this module doesn't need its own /dev/mem code). */
void strip_init(volatile uint8_t *lw_bridge_base, uint32_t strip_offset,
                uint32_t reg_strip_swap_offset);

/* Buffer manipulation (no HW writes — buffer stays local until present()). */
void strip_clear(uint8_t color);
void strip_pixel(int x, int y, uint8_t color);
void strip_rect(int x, int y, int w, int h, uint8_t color);

/* Draw 5×7 character at (x, y). 'scale' multiplies font dimensions:
 *   scale=1: 5×7 px glyph     scale=2: 10×14 px glyph     etc.
 * fg is drawn for set bits; bg is drawn for clear bits.  */
void strip_char(int x, int y, char c, int scale, uint8_t fg, uint8_t bg);
void strip_text(int x, int y, const char *s, int scale,
                uint8_t fg, uint8_t bg);

/* Pixel width of `s` painted at `scale`. Matches strip_text exactly: N chars
 * occupy N*FONT_W*scale + (N-1)*scale pixels (no trailing inter-char gap). */
int  strip_text_width(const char *s, int scale);

/* strip_text with the start-x computed so the string is horizontally centered
 * in the 640-px-wide strip. Clamps x ≥ 0 if the string overflows. */
void strip_text_centered(int y, const char *s, int scale,
                         uint8_t fg, uint8_t bg);

/* Filled `bg`-colored box sized to fit `s` plus `pad` px of padding on every
 * side, with `s` painted on top in `fg`. Use for "selected" pills on menus. */
void strip_text_box(int x, int y, const char *s, int scale,
                    uint8_t fg, uint8_t bg, int pad);

/* Push the local buffer to FPGA (9,600 word writes, ~770 µs at 50 MHz LW)
 * and arm a vsync-aligned swap. After this returns, the next VS pulse
 * makes the new contents visible. */
void strip_present(void);

#endif
