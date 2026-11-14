// SPDX-License-Identifier: GPL-2.0-or-later
#include "st7735.h"
#include <SPI.h>
#include "config.h"

// CS = D10 (PB2), DC = A5 (PC5)
#define CS_LOW()  (PORTB &= ~_BV(2))
#define CS_HIGH() (PORTB |= _BV(2))
#define DC_CMD()  (PORTC &= ~_BV(5))
#define DC_DATA() (PORTC |= _BV(5))

namespace {

const uint8_t MADCTL_BGR = 0x08;

const SPISettings spiCfg(8000000, MSBFIRST, SPI_MODE0);

void writeByte(uint8_t b) {
	SPDR = b;
	while (!(SPSR & _BV(SPIF))) {}
}

void command(uint8_t cmd) {
	DC_CMD();
	writeByte(cmd);
	DC_DATA();
}

/**
 * Init table: command, argument count (bit 7 = delay follows), args...
 * Based on the common ST7735R/S "black tab" sequence.
 */
const uint8_t initSeq[] PROGMEM = {
	0x01, 0x80, 150,                    // SWRESET
	0x11, 0x80, 255,                    // SLPOUT
	0xB1, 3, 0x01, 0x2C, 0x2D,          // FRMCTR1
	0xB2, 3, 0x01, 0x2C, 0x2D,          // FRMCTR2
	0xB3, 6, 0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D,  // FRMCTR3
	0xB4, 1, 0x07,                      // INVCTR
	0xC0, 3, 0xA2, 0x02, 0x84,          // PWCTR1
	0xC1, 1, 0xC5,                      // PWCTR2
	0xC2, 2, 0x0A, 0x00,                // PWCTR3
	0xC3, 2, 0x8A, 0x2A,                // PWCTR4
	0xC4, 2, 0x8A, 0xEE,                // PWCTR5
	0xC5, 1, 0x0E,                      // VMCTR1
	0x3A, 1, 0x05,                      // COLMOD 16 bit
	0xE0, 16, 0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D,
	          0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10,  // GMCTRP1
	0xE1, 16, 0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,
	          0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10,  // GMCTRN1
	0x13, 0x80, 10,                     // NORON
	0x29, 0x80, 100,                    // DISPON
	0x00
};

}  // namespace

namespace Tft {

void begin(bool bgr) {
	pinMode(PIN_TFT_CS, OUTPUT);
	pinMode(PIN_TFT_DC, OUTPUT);
	CS_HIGH();
	SPI.begin();
	SPI.beginTransaction(spiCfg);
	CS_LOW();
	const uint8_t *p = initSeq;
	for (;;) {
		uint8_t cmd = pgm_read_byte(p++);
		if (!cmd) break;
		uint8_t n = pgm_read_byte(p++);
		bool wait = n & 0x80;
		n &= 0x7F;
		command(cmd);
		while (n--) writeByte(pgm_read_byte(p++));
		if (wait) {
			uint8_t ms = pgm_read_byte(p++);
			CS_HIGH();
			delay(ms == 255 ? 500 : ms);
			CS_LOW();
		}
	}
	command(TFT_INVERT ? 0x21 : 0x20);
	CS_HIGH();
	SPI.endTransaction();
	setBgr(bgr);
}

void setBgr(bool bgr) {
	SPI.beginTransaction(spiCfg);
	CS_LOW();
	command(0x36);
	writeByte(TFT_MADCTL | (bgr ? MADCTL_BGR : 0));
	CS_HIGH();
	SPI.endTransaction();
}

void window(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
	SPI.beginTransaction(spiCfg);
	CS_LOW();
	command(0x2A);
	x0 += TFT_OFFSET_X;
	x1 += TFT_OFFSET_X;
	writeByte(x0 >> 8); writeByte(x0);
	writeByte(x1 >> 8); writeByte(x1);
	command(0x2B);
	y0 += TFT_OFFSET_Y;
	y1 += TFT_OFFSET_Y;
	writeByte(y0 >> 8); writeByte(y0);
	writeByte(y1 >> 8); writeByte(y1);
	command(0x2C);
}

void end() {
	CS_HIGH();
	SPI.endTransaction();
}

void pushN(uint16_t c, uint16_t n) {
	uint8_t hi = c >> 8, lo = c;
	while (n--) {
		SPDR = hi;
		while (!(SPSR & _BV(SPIF))) {}
		SPDR = lo;
		while (!(SPSR & _BV(SPIF))) {}
	}
}

void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > TFT_W) w = TFT_W - x;
	if (y + h > TFT_H) h = TFT_H - y;
	if (w <= 0 || h <= 0) return;
	window(x, y, x + w - 1, y + h - 1);
	pushN(c, (uint16_t)w * h);
	end();
}

void fillScreen(uint16_t c) {
	fillRect(0, 0, TFT_W, TFT_H, c);
}

}  // namespace Tft
