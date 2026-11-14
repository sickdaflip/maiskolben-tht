// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <Arduino.h>

/**
 * Fixed-cell, 2-bit-per-pixel anti-aliased glyph set (digits).
 * Each glyph occupies ceil(width * height / 4) bytes, MSB first, row by row.
 */
struct FixedFont {
	const uint8_t *bits;
	const char *map;
	uint8_t width;
	uint8_t height;
	uint8_t count;
};

/**
 * Proportional 2-bit-per-pixel text font.
 * Glyphs 0..94 are ASCII 32..126; the rest are listed in `extra`
 * (Unicode code points, ascending). Every glyph is `height` rows tall and
 * `widths[i]` columns wide, so the advance width equals the bitmap width.
 */
struct TextFont {
	const uint8_t *bits;
	const uint16_t *offsets;
	const uint8_t *widths;
	const uint16_t *extra;
	uint8_t height;
	uint8_t extraCount;
};
