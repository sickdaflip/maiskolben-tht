// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Host-side preview: runs the real UI and drawing code against a frame
 * buffer and writes one PPM image per scenario.
 *
 * Build: g++ -std=c++17 -DMAISKOLBEN_SIM -Itools/sim -Isrc \
 *        tools/sim/sim.cpp src/ui.cpp src/gfx.cpp src/i18n.cpp -o sim
 */
#include <Arduino.h>
#include "buttons.h"
#include "i18n.h"
#include "settings.h"
#include "st7735.h"
#include "station.h"
#include "ui.h"

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
static uint16_t fb[TFT_H][TFT_W];
static int16_t wx0, wy0, wx1, wy1, cx, cy;
static uint32_t fakeNow = 100000;

uint32_t millis() { return fakeNow; }

namespace Tft {
void begin(bool) {}
void setBgr(bool) {}
void window(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
	wx0 = x0; wy0 = y0; wx1 = x1; wy1 = y1; cx = x0; cy = y0;
}
void end() {}
void push(uint16_t c) {
	if (cx >= 0 && cx < TFT_W && cy >= 0 && cy < TFT_H) fb[cy][cx] = c;
	if (++cx > wx1) { cx = wx0; ++cy; }
}
void pushN(uint16_t c, uint16_t n) { while (n--) push(c); }
void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
	for (int16_t j = y; j < y + h; j++)
		for (int16_t i = x; i < x + w; i++)
			if (i >= 0 && i < TFT_W && j >= 0 && j < TFT_H) fb[j][i] = c;
}
void fillScreen(uint16_t c) { fillRect(0, 0, TFT_W, TFT_H, c); }
}  // namespace Tft

static void savePpm(const char *name) {
	FILE *f = fopen(name, "wb");
	fprintf(f, "P6 %d %d 255\n", TFT_W, TFT_H);
	for (int y = 0; y < TFT_H; y++)
		for (int x = 0; x < TFT_W; x++) {
			uint16_t c = fb[y][x];
			uint8_t rgb[3] = {(uint8_t)((c >> 11) << 3), (uint8_t)(((c >> 5) & 0x3F) << 2),
			                  (uint8_t)((c & 0x1F) << 3)};
			fwrite(rgb, 1, 3, f);
		}
	fclose(f);
}

// ---------------------------------------------------------------------------
// Settings stub
// ---------------------------------------------------------------------------
static Settings cfg = {330, {250, 300, 330}, 0.54f, 42.8f, 0, 5, FLAG_COLOR_KNOWN | FLAG_BGR, 0};
namespace Config {
void begin() {}
Settings &get() { return cfg; }
bool flag(uint8_t f) { return cfg.flags & f; }
void setFlag(uint8_t f, bool on) { if (on) cfg.flags |= f; else cfg.flags &= ~f; }
void touch() {}
void service(uint32_t) {}
void flush() {}
uint8_t nextStandby(uint8_t c, int8_t) { return c; }
}  // namespace Config

// ---------------------------------------------------------------------------
// Station stub
// ---------------------------------------------------------------------------
static Mode sMode = Mode::On;
static Fault sFault = Fault::None;
static int16_t sTemp = 327;
static bool sTip = true;
static uint8_t sPct = 38;
static int16_t sCountdown = 272;
namespace Station {
void begin() {}
void update(uint32_t) {}
Mode mode() { return sMode; }
Fault fault() { return sFault; }
void clearFault() {}
void setOn(bool) {}
void setStandby(bool) {}
void activity() {}
int16_t temperature() { return sTemp; }
bool tipPresent() { return sTip; }
int16_t target() { return sMode == Mode::On ? cfg.setTemp : sMode == Mode::Standby ? 150 : 0; }
uint8_t powerPercent() { return sPct; }
float output() { return 0; }
int16_t countdown(uint32_t) { return sMode == Mode::Off ? -1 : sCountdown; }
void refreshCalibration() {}
}  // namespace Station

namespace Buttons {
void begin() {}
void update(uint32_t) {}
bool next(Event &) { return false; }
bool isDown(uint8_t) { return false; }
uint32_t heldFor(uint8_t, uint32_t) { return 0; }
void flush() {}
}  // namespace Buttons

// ---------------------------------------------------------------------------
// Scenarios
// ---------------------------------------------------------------------------
static void scene(const char *file, Mode m, Fault f, int16_t t, uint8_t pct, uint8_t lang,
                  bool fahrenheit = false) {
	sMode = m; sFault = f; sTemp = t; sPct = pct; sTip = f != Fault::NoTip;
	cfg.language = lang;
	i18nSetLanguage(lang);
	Config::setFlag(FLAG_FAHRENHEIT, fahrenheit);
	Ui::showMain();
	fakeNow += 100;
	Ui::update(fakeNow);
	savePpm(file);
}

int main() {
	scene("01_ready_en.ppm", Mode::On, Fault::None, 329, 7, LANG_EN_US);
	scene("02_heating_de.ppm", Mode::On, Fault::None, 214, 90, LANG_DE);
	scene("03_standby_fr.ppm", Mode::Standby, Fault::None, 151, 4, LANG_FR);
	scene("04_off_es.ppm", Mode::Off, Fault::None, 48, 0, LANG_ES);
	scene("05_notip_de.ppm", Mode::Off, Fault::NoTip, 0, 0, LANG_DE);
	scene("06_ready_f_gb.ppm", Mode::On, Fault::None, 331, 9, LANG_EN_GB, true);
	cfg.language = LANG_DE;
	i18nSetLanguage(LANG_DE);
	Config::setFlag(FLAG_FAHRENHEIT, false);
	Ui::showMenu();
	savePpm("07_menu_de.ppm");
	return 0;
}
