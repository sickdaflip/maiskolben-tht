// SPDX-License-Identifier: GPL-2.0-or-later
#include "gfx.h"
#include "st7735.h"

namespace {

void palette(uint16_t *pal, uint16_t fg, uint16_t bg) {
	for (uint8_t i = 0; i < 4; i++) pal[i] = Gfx::blend(bg, fg, i);
}

/** Streams `count` 2bpp pixels from flash through the palette. */
void stream2bpp(const uint8_t *p, uint16_t count, const uint16_t *pal) {
	uint8_t b = 0;
	for (uint16_t i = 0; i < count; i++) {
		if ((i & 3) == 0) b = pgm_read_byte(p++);
		Tft::push(pal[b >> 6]);
		b <<= 2;
	}
}

/** Decodes the next UTF-8 code point and advances the pointer. */
uint16_t nextCodepoint(const char *&s, bool pgm) {
	auto rd = [&](void) -> uint8_t {
		uint8_t c = pgm ? pgm_read_byte(s) : (uint8_t)*s;
		if (c) s++;
		return c;
	};
	uint8_t c = rd();
	if (c < 0x80) return c;
	if ((c & 0xE0) == 0xC0) return ((uint16_t)(c & 0x1F) << 6) | (rd() & 0x3F);
	if ((c & 0xF0) == 0xE0) {
		uint16_t cp = (uint16_t)(c & 0x0F) << 12;
		cp |= (uint16_t)(rd() & 0x3F) << 6;
		return cp | (rd() & 0x3F);
	}
	return '?';
}

int16_t glyphIndex(const TextFont &f, uint16_t cp) {
	if (cp >= 32 && cp <= 126) return cp - 32;
	for (uint8_t i = 0; i < f.extraCount; i++) {
		if (pgm_read_word(&f.extra[i]) == cp) return 95 + i;
	}
	return '?' - 32;
}

// Anti-aliased corner coverage (0..3) for the first two rows/columns.
const uint8_t CORNER[2][3] = {{0, 1, 3}, {1, 3, 3}};

}  // namespace

namespace Gfx {

uint16_t blend(uint16_t bg, uint16_t fg, uint8_t level) {
	if (level == 0) return bg;
	if (level >= 3) return fg;
	uint8_t a = level == 1 ? 85 : 170;
	uint8_t b = 255 - a;
	uint16_t r = (((fg >> 11) & 0x1F) * a + ((bg >> 11) & 0x1F) * b) / 255;
	uint16_t g = (((fg >> 5) & 0x3F) * a + ((bg >> 5) & 0x3F) * b) / 255;
	uint16_t bl = ((fg & 0x1F) * a + (bg & 0x1F) * b) / 255;
	return (r << 11) | (g << 5) | bl;
}

uint8_t fixedWidth(const FixedFont *font) { return pgm_read_byte(&font->width); }
uint8_t fixedHeight(const FixedFont *font) { return pgm_read_byte(&font->height); }
uint8_t textHeight(const TextFont *font) { return pgm_read_byte(&font->height); }

void glyph(const FixedFont *font, char c, int16_t x, int16_t y, uint16_t fg, uint16_t bg) {
	FixedFont f;
	memcpy_P(&f, font, sizeof f);
	int16_t idx = -1;
	for (uint8_t i = 0; i < f.count; i++) {
		if (pgm_read_byte(&f.map[i]) == (uint8_t)c) { idx = i; break; }
	}
	if (idx < 0) {
		Tft::fillRect(x, y, f.width, f.height, bg);
		return;
	}
	uint16_t pixels = (uint16_t)f.width * f.height;
	uint16_t bytes = (pixels + 3) / 4;
	uint16_t pal[4];
	palette(pal, fg, bg);
	Tft::window(x, y, x + f.width - 1, y + f.height - 1);
	stream2bpp(f.bits + idx * bytes, pixels, pal);
	Tft::end();
}

int16_t fixedText(const FixedFont *font, const char *s, int16_t x, int16_t y,
                  uint16_t fg, uint16_t bg) {
	uint8_t w = fixedWidth(font);
	int16_t x0 = x;
	while (*s) {
		glyph(font, *s++, x, y, fg, bg);
		x += w;
	}
	return x - x0;
}

int16_t textWidth(const TextFont *font, const char *s, bool pgm) {
	TextFont f;
	memcpy_P(&f, font, sizeof f);
	int16_t w = 0;
	for (;;) {
		uint16_t cp = nextCodepoint(s, pgm);
		if (!cp) break;
		w += pgm_read_byte(&f.widths[glyphIndex(f, cp)]);
	}
	return w;
}

int16_t text(const TextFont *font, const char *s, bool pgm, int16_t x, int16_t y,
             uint16_t fg, uint16_t bg) {
	TextFont f;
	memcpy_P(&f, font, sizeof f);
	uint16_t pal[4];
	palette(pal, fg, bg);
	for (;;) {
		uint16_t cp = nextCodepoint(s, pgm);
		if (!cp) break;
		int16_t i = glyphIndex(f, cp);
		uint8_t w = pgm_read_byte(&f.widths[i]);
		if (x + w > TFT_W) break;
		Tft::window(x, y, x + w - 1, y + f.height - 1);
		stream2bpp(f.bits + pgm_read_word(&f.offsets[i]), (uint16_t)w * f.height, pal);
		Tft::end();
		x += w;
	}
	return x;
}

void textBox(const TextFont *font, const char *s, bool pgm, int16_t x, int16_t y,
             int16_t w, Align align, uint16_t fg, uint16_t bg) {
	int16_t tw = textWidth(font, s, pgm);
	if (tw > w) tw = w;
	int16_t h = textHeight(font);
	int16_t sx = x;
	if (align == ALIGN_CENTER) sx = x + (w - tw) / 2;
	else if (align == ALIGN_RIGHT) sx = x + w - tw;
	if (sx > x) Tft::fillRect(x, y, sx - x, h, bg);
	int16_t ex = text(font, s, pgm, sx, y, fg, bg);
	if (ex < x + w) Tft::fillRect(ex, y, x + w - ex, h, bg);
}

void roundRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t fill, uint16_t outside) {
	uint16_t pal[4];
	palette(pal, fill, outside);
	Tft::window(x, y, x + w - 1, y + h - 1);
	for (int16_t row = 0; row < h; row++) {
		int8_t cr = row < 2 ? row : (row >= h - 2 ? h - 1 - row : -1);
		for (int16_t col = 0; col < w; col++) {
			if (cr >= 0) {
				int16_t cc = col < 3 ? col : (col >= w - 3 ? w - 1 - col : -1);
				if (cc >= 0) {
					Tft::push(pal[CORNER[cr][cc]]);
					continue;
				}
			}
			Tft::push(fill);
		}
	}
	Tft::end();
}

void roundFrame(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t border,
                uint16_t fill, uint16_t outside) {
	roundRect(x, y, w, h, border, outside);
	roundRect(x + 1, y + 1, w - 2, h - 2, fill, border);
}

void arrow(int16_t cx, int16_t y, uint8_t halfWidth, uint8_t height, bool up, uint16_t c) {
	for (uint8_t i = 0; i < height; i++) {
		uint8_t row = up ? i : height - 1 - i;
		int16_t hw = (int16_t)halfWidth * row / (height - 1);
		Tft::fillRect(cx - hw, y + i, hw * 2 + 1, 1, c);
	}
}

}  // namespace Gfx
