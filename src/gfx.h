// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Flicker-free drawing primitives: every call paints its full bounding box
 * (foreground and background), so redraws never need a prior clear.
 */
#pragma once
#include <Arduino.h>
#include "fontdefs.h"

enum Align : uint8_t { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };

namespace Gfx {

uint16_t blend(uint16_t bg, uint16_t fg, uint8_t level);  // level 0..3

/** Draws one glyph of a fixed-cell font; unknown chars render as blank. */
void glyph(const FixedFont *font, char c, int16_t x, int16_t y, uint16_t fg, uint16_t bg);

/** Draws a RAM string with a fixed-cell font, returns the drawn width. */
int16_t fixedText(const FixedFont *font, const char *s, int16_t x, int16_t y,
                  uint16_t fg, uint16_t bg);

uint8_t fixedWidth(const FixedFont *font);
uint8_t fixedHeight(const FixedFont *font);

/** UTF-8 text; `pgm` selects flash or RAM storage. Returns the end x. */
int16_t text(const TextFont *font, const char *s, bool pgm, int16_t x, int16_t y,
             uint16_t fg, uint16_t bg);
int16_t textWidth(const TextFont *font, const char *s, bool pgm);
uint8_t textHeight(const TextFont *font);

/** Paints a box of width `w` with the text aligned inside it. */
void textBox(const TextFont *font, const char *s, bool pgm, int16_t x, int16_t y,
             int16_t w, Align align, uint16_t fg, uint16_t bg);

/** Rounded rectangle with anti-aliased corners blended against `outside`. */
void roundRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t fill, uint16_t outside);

/** Rounded outline: 1 px border, inner area filled with `fill`. */
void roundFrame(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t border,
                uint16_t fill, uint16_t outside);

/** Solid isosceles arrow, `up` = apex at the top. */
void arrow(int16_t cx, int16_t y, uint8_t halfWidth, uint8_t height, bool up, uint16_t c);

}  // namespace Gfx
