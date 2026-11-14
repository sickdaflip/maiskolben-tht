// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Minimal ST7735S driver for 1.8" 128x160 SPI panels (e.g. "KMR-1.8 SPI"),
 * hardware SPI at F_CPU / 2, landscape 160x128.
 *
 * The panel reset line on the Maiskolben THT is tied to the Arduino reset,
 * so only a software reset is issued here.
 */
#pragma once
#include <Arduino.h>

#define TFT_W 160
#define TFT_H 128

// Panel quirks: adjust if the image is shifted or colors are inverted.
#ifndef TFT_OFFSET_X
#define TFT_OFFSET_X 0
#endif
#ifndef TFT_OFFSET_Y
#define TFT_OFFSET_Y 0
#endif
#ifndef TFT_INVERT
#define TFT_INVERT 0
#endif
// Memory access control for landscape mode (MY | MV). The v3.1 firmware used
// the same orientation. Try 0x60 if the image is upside down.
#ifndef TFT_MADCTL
#define TFT_MADCTL 0xA0
#endif

namespace Tft {

void begin(bool bgr);
void setBgr(bool bgr);

/** Opens a drawing window; follow with push()/pushN() and end(). */
void window(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void end();

/** Sends one RGB565 pixel inside an open window. */
#ifdef MAISKOLBEN_SIM
void push(uint16_t c);  // host-side preview (tools/sim)
#else
static inline void push(uint16_t c) {
	SPDR = c >> 8;
	while (!(SPSR & _BV(SPIF))) {}
	SPDR = c;
	while (!(SPSR & _BV(SPIF))) {}
}
#endif

void pushN(uint16_t c, uint16_t n);
void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c);
void fillScreen(uint16_t c);

}  // namespace Tft
