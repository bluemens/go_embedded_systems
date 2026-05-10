/*
 * font5x7.h — minimal 5x7 ASCII bitmap font
 *
 * Each glyph is 5 columns × 7 rows. Storage is 5 bytes per glyph, one byte
 * per column. Bit 0 of each byte is the TOP row, bit 6 is the BOTTOM row;
 * bit 7 is unused.
 *
 * Coverage: digits 0-9, uppercase A-Z, and punctuation enough for score and
 * menu text (' ', '.', ':', '=', '-', '/'). Lowercase, symbols, and other
 * characters render as a solid block (the "?" placeholder).
 *
 * Glyph data is taken from the standard 5x7 font (e.g., Adafruit-GFX
 * stockfont) which is widely re-used and BSD-licensed.
 */

#ifndef _FONT5X7_H
#define _FONT5X7_H

#include <stdint.h>

#define FONT_W 5
#define FONT_H 7

/* Returns a pointer to 5 bytes describing the columns of glyph c.
 * Always returns a valid pointer; unsupported chars get a placeholder. */
const uint8_t *font5x7_glyph(char c);

#endif
