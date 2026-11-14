// SPDX-License-Identifier: GPL-2.0-or-later
#include "ui.h"
#include "colors.h"
#include "config.h"
#include "fonts.h"
#include "gfx.h"
#include "i18n.h"
#include "settings.h"
#include "st7735.h"
#include "station.h"

namespace {

// ---------------------------------------------------------------------------
// Layout (landscape 160 x 128)
// ---------------------------------------------------------------------------
const int16_t PILL_X = 5, PILL_Y = 4, PILL_H = 13, PILL_MAX_W = 82;
const int16_t TOP_RIGHT_X = 118, TOP_RIGHT_W = 38, TOP_Y = 5;
const int16_t COUNT_X = 88, COUNT_W = 28;
const int16_t BIG_X = 4, BIG_Y = 21;
const int16_t UNIT_X = 99, UNIT_Y = 23;
const int16_t COL_X = 124, COL_W = 36, COL_CX = 142;
const int16_t SET_LABEL_Y = 19, ARROW_UP_Y = 32, SET_Y = 42, ARROW_DN_Y = 58;
const int16_t ARROW_HALF = 6, ARROW_H = 6;
const int16_t BAR_X = 6, BAR_W = 148, BAR_Y = 71, BAR_H = 5;
const int16_t MARK_Y = 68, MARK_H = 11;
const int16_t PWR_Y = 82, PWR_H = 2;
const int16_t MSG_Y = 68, MSG_H = 28;
const int16_t CHIP_Y = 100, CHIP_W = 48, CHIP_H = 23;
const int16_t CHIP_X[3] = {4, 56, 108};

const uint16_t STORE_FLASH_MS = 700;

enum Screen : uint8_t { SCR_MAIN, SCR_MENU };
Screen screen = SCR_MAIN;

enum ViewState : uint8_t { VS_OFF, VS_HEAT, VS_READY, VS_STANDBY, VS_ERROR, VS_NONE };

// Cached main screen state; -1 / sentinel values force a redraw.
struct MainCache {
	uint8_t state;
	char digits[4];
	uint16_t digitColor;
	uint8_t fahrenheit;  // 0/1, 0xFF = unknown
	int16_t set;
	uint16_t setColor;
	uint8_t upEnabled, downEnabled;
	int16_t barW;
	uint16_t barColor;
	int16_t markX;
	int16_t pwrW;
	int8_t pwrPct;
	int16_t countdown;
	uint16_t countColor;
	int16_t presets[3];
	int8_t active;
	int8_t flashing;
	uint8_t fault;
	uint8_t pillW;
};
MainCache mc;

int8_t storeFlash = -1;
uint32_t storeFlashUntil = 0;

uint8_t menuSel = 0;
enum MenuItem : uint8_t { M_STANDBY, M_BOOTHEAT, M_UNIT, M_LANG, M_TRIM, M_SWAP, M_COUNT };

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
bool fahrenheit() { return Config::flag(FLAG_FAHRENHEIT); }

int16_t toUnit(int16_t c) {
	return fahrenheit() ? (int16_t)lround(c * 1.8f + 32) : c;
}

void formatTemp(char *buf, int16_t value) {
	if (value < 0) value = 0;
	if (value > 999) value = 999;
	buf[0] = value >= 100 ? '0' + value / 100 : ' ';
	buf[1] = value >= 10 ? '0' + (value / 10) % 10 : ' ';
	buf[2] = '0' + value % 10;
	buf[3] = 0;
}

/** Appends an unsigned decimal number, returns the new end pointer. */
char *putUInt(char *p, uint16_t v, uint8_t minDigits = 1) {
	char tmp[6];
	uint8_t n = 0;
	do {
		tmp[n++] = '0' + v % 10;
		v /= 10;
	} while (v || n < minDigits);
	while (n) *p++ = tmp[--n];
	*p = 0;
	return p;
}

char *putPgm(char *p, const char *s) {
	strcpy_P(p, s);
	return p + strlen(p);
}

void chip(uint8_t i, const char *label, bool pgm, bool active, uint16_t activeColor) {
	int16_t x = CHIP_X[i];
	if (active) {
		Gfx::roundRect(x, CHIP_Y, CHIP_W, CHIP_H, activeColor, C_BG);
	} else {
		Gfx::roundFrame(x, CHIP_Y, CHIP_W, CHIP_H, C_BORDER, C_BG, C_BG);
	}
	uint16_t bg = active ? activeColor : C_BG;
	uint16_t fg = active ? C_WHITE : C_TEXT;
	Gfx::textBox(&FONT_TEXT, label, pgm, x + 3, CHIP_Y + 6, CHIP_W - 6, ALIGN_CENTER, fg, bg);
}

void presetChip(uint8_t i, int16_t value, bool active, bool flashing) {
	int16_t x = CHIP_X[i];
	uint16_t fill = flashing ? C_OK : C_ACCENT;
	bool filled = active || flashing;
	if (filled) Gfx::roundRect(x, CHIP_Y, CHIP_W, CHIP_H, fill, C_BG);
	else Gfx::roundFrame(x, CHIP_Y, CHIP_W, CHIP_H, C_BORDER, C_BG, C_BG);
	char buf[4];
	formatTemp(buf, toUnit(value));
	// Center the digits without the leading blank cells.
	const char *p = buf;
	while (*p == ' ') p++;
	uint8_t cw = Gfx::fixedWidth(&FONT_MED);
	int16_t w = strlen(p) * cw;
	Gfx::fixedText(&FONT_MED, p, x + (CHIP_W - w) / 2, CHIP_Y + 6,
	               filled ? C_WHITE : C_TEXT, filled ? fill : C_BG);
}

// ---------------------------------------------------------------------------
// Main screen
// ---------------------------------------------------------------------------
void resetMainCache() {
	memset(&mc, 0xFF, sizeof mc);
	mc.digits[0] = 0;
	mc.pillW = 0;
}

ViewState viewState(int16_t t) {
	if (Station::fault() != Fault::None) return VS_ERROR;
	switch (Station::mode()) {
		case Mode::Off: return VS_OFF;
		case Mode::Standby: return VS_STANDBY;
		default: break;
	}
	return abs(t - Station::target()) <= READY_BAND ? VS_READY : VS_HEAT;
}

void drawPill(uint8_t state) {
	StrId label;
	uint16_t bg, fg, dot;
	switch (state) {
		case VS_HEAT: label = S_STATE_HEAT; bg = C_WARN_BG; fg = C_WARN_FG; dot = C_WARN; break;
		case VS_READY: label = S_STATE_READY; bg = C_OK_BG; fg = C_OK_FG; dot = C_OK; break;
		case VS_STANDBY: label = S_STATE_STANDBY; bg = C_TRACK; fg = C_ACCENT_LIGHT; dot = C_ACCENT_LIGHT; break;
		case VS_ERROR: label = S_STATE_ERROR; bg = C_ERR_BG; fg = C_ERR_FG; dot = C_ERR; break;
		default: label = S_STATE_OFF; bg = C_TRACK; fg = C_SUBTLE; dot = C_MUTED; break;
	}
	const char *s = tr(label);
	int16_t w = Gfx::textWidth(&FONT_TEXT, s, true) + 18;
	if (w > PILL_MAX_W) w = PILL_MAX_W;
	Gfx::roundRect(PILL_X, PILL_Y, w, PILL_H, bg, C_BG);
	Gfx::roundRect(PILL_X + 4, PILL_Y + 4, 5, 5, dot, bg);
	Gfx::textBox(&FONT_TEXT, s, true, PILL_X + 12, PILL_Y + 1, w - 16, ALIGN_LEFT, fg, bg);
	if (mc.pillW != 0xFF && mc.pillW > w) {
		Tft::fillRect(PILL_X + w, PILL_Y, mc.pillW - w, PILL_H, C_BG);
	}
	mc.pillW = w;
}

void drawBars(int16_t barW, uint16_t barColor, int16_t markX, int16_t pwrW) {
	if (markX != mc.markX && mc.markX >= 0) {
		Tft::fillRect(mc.markX, MARK_Y, 2, MARK_H, C_BG);
	}
	Tft::fillRect(BAR_X, BAR_Y, barW, BAR_H, barColor);
	Tft::fillRect(BAR_X + barW, BAR_Y, BAR_W - barW, BAR_H, C_TRACK);
	if (markX >= 0) Tft::fillRect(markX, MARK_Y, 2, MARK_H, C_WHITE);
	Tft::fillRect(BAR_X, PWR_Y, pwrW, PWR_H, C_WARN);
	Tft::fillRect(BAR_X + pwrW, PWR_Y, BAR_W - pwrW, PWR_H, C_TRACK);
}

void drawFault(Fault f) {
	StrId a, b;
	switch (f) {
		case Fault::NoTip: a = S_ERR_NO_TIP_1; b = S_ERR_NO_TIP_2; break;
		case Fault::NotHeating: a = S_ERR_NOT_HEATING_1; b = S_ERR_NOT_HEATING_2; break;
		case Fault::SensorDrop: a = S_ERR_DROP_1; b = S_ERR_DROP_2; break;
		default: a = S_ERR_OVERTEMP_1; b = S_ERR_OVERTEMP_2; break;
	}
	Tft::fillRect(0, MSG_Y, TFT_W, MSG_H, C_BG);
	Gfx::textBox(&FONT_TEXT, tr(a), true, BAR_X, MSG_Y + 2, BAR_W, ALIGN_LEFT, C_ERR_FG, C_BG);
	Gfx::textBox(&FONT_TEXT, tr(b), true, BAR_X, MSG_Y + 15, BAR_W, ALIGN_LEFT, C_SUBTLE, C_BG);
}

void drawMainStatic() {
	Tft::fillScreen(C_BG);
	resetMainCache();
	Gfx::textBox(&FONT_TEXT, tr(S_SET), true, COL_X, SET_LABEL_Y, COL_W, ALIGN_CENTER, C_MUTED, C_BG);
}

void updateMain(uint32_t now) {
	const Settings &cfg = Config::get();
	Fault fault = Station::fault();
	Mode mode = Station::mode();
	bool tip = Station::tipPresent();
	int16_t t = Station::temperature();
	int16_t tgt = Station::target();
	uint8_t state = viewState(t);

	if (state != mc.state) {
		drawPill(state);
		mc.state = state;
	}

	// Countdown and power readout in the top row.
	int16_t cd = Station::countdown(now);
	uint16_t cdColor = mode == Mode::Standby ? C_ACCENT_LIGHT : C_MUTED;
	if (cd != mc.countdown || cdColor != mc.countColor) {
		char buf[8] = "";
		if (cd >= 0) {
			char *p = putUInt(buf, cd / 60);
			*p++ = ':';
			putUInt(p, cd % 60, 2);
		}
		Gfx::textBox(&FONT_TEXT, buf, false, COUNT_X, TOP_Y, COUNT_W, ALIGN_RIGHT, cdColor, C_BG);
		mc.countdown = cd;
		mc.countColor = cdColor;
	}
	int8_t pct = mode == Mode::Off ? -1 : Station::powerPercent();
	if (pct != mc.pwrPct) {
		char buf[6] = "";
		if (pct >= 0) putPgm(putUInt(buf, pct), PSTR(" %"));
		Gfx::textBox(&FONT_TEXT, buf, false, TOP_RIGHT_X, TOP_Y, TOP_RIGHT_W, ALIGN_RIGHT, C_SUBTLE, C_BG);
		mc.pwrPct = pct;
	}

	// Big actual temperature.
	float cold = cfg.adcOffset + cfg.trim + 15;
	char digits[4];
	if (!tip || fault == Fault::NoTip || t < cold) strcpy_P(digits, PSTR("---"));
	else formatTemp(digits, toUnit(t));
	uint16_t dColor;
	int16_t diff = t - tgt;
	if (fault != Fault::None) dColor = C_ERR;
	else if (mode == Mode::Off) dColor = C_MUTED;
	else if (abs(diff) <= READY_BAND) dColor = C_WHITE;
	else dColor = diff < 0 ? C_WARN : C_COOL;
	if (strcmp(digits, mc.digits) != 0 || dColor != mc.digitColor) {
		uint8_t cw = Gfx::fixedWidth(&FONT_BIG);
		for (uint8_t i = 0; i < 3; i++) {
			if (digits[i] != mc.digits[i] || dColor != mc.digitColor) {
				Gfx::glyph(&FONT_BIG, digits[i], BIG_X + i * cw, BIG_Y, dColor, C_BG);
			}
		}
		memcpy(mc.digits, digits, 4);
		mc.digitColor = dColor;
	}

	bool fh = fahrenheit();
	if ((uint8_t)fh != mc.fahrenheit) {
		Gfx::glyph(&FONT_UNIT, fh ? 'F' : 'C', UNIT_X, UNIT_Y, C_SUBTLE, C_BG);
		mc.fahrenheit = fh;
	}

	// Setpoint column.
	uint16_t sColor = mode == Mode::On ? C_ACCENT_LIGHT : C_SUBTLE;
	int16_t setU = toUnit(cfg.setTemp);
	if (setU != mc.set || sColor != mc.setColor) {
		char buf[4];
		formatTemp(buf, setU);
		int16_t w = 3 * Gfx::fixedWidth(&FONT_MED);
		Gfx::fixedText(&FONT_MED, buf, COL_X + (COL_W - w) / 2, SET_Y, sColor, C_BG);
		mc.set = setU;
		mc.setColor = sColor;
	}
	bool up = cfg.setTemp < TEMP_SET_MAX, down = cfg.setTemp > TEMP_SET_MIN;
	if ((uint8_t)up != mc.upEnabled) {
		Gfx::arrow(COL_CX, ARROW_UP_Y, ARROW_HALF, ARROW_H, true, up ? C_TEXT : C_BORDER);
		mc.upEnabled = up;
	}
	if ((uint8_t)down != mc.downEnabled) {
		Gfx::arrow(COL_CX, ARROW_DN_Y, ARROW_HALF, ARROW_H, false, down ? C_TEXT : C_BORDER);
		mc.downEnabled = down;
	}

	// Bars or fault message.
	uint8_t f = (uint8_t)fault;
	if (fault != Fault::None) {
		if (f != mc.fault) drawFault(fault);
		mc.fault = f;
	} else {
		if (f != mc.fault) {
			Tft::fillRect(0, MSG_Y, TFT_W, MSG_H, C_BG);
			mc.barW = mc.markX = mc.pwrW = -1;
			mc.fault = f;
		}
		int16_t span = TEMP_BAR_MAX - TEMP_BAR_MIN;
		int16_t barW = tip ? constrain((int32_t)(t - TEMP_BAR_MIN) * BAR_W / span, 0, BAR_W) : 0;
		uint16_t barColor = mode == Mode::Off ? C_BORDER : dColor == C_WHITE ? C_OK : dColor;
		int16_t markX = mode == Mode::Off ? -1
		                : BAR_X + (int32_t)(tgt - TEMP_BAR_MIN) * BAR_W / span - 1;
		int16_t pwrW = mode == Mode::Off ? 0 : (int16_t)Station::powerPercent() * BAR_W / 100;
		if (barW != mc.barW || barColor != mc.barColor || markX != mc.markX || pwrW != mc.pwrW) {
			drawBars(barW, barColor, markX, pwrW);
			mc.barW = barW;
			mc.barColor = barColor;
			mc.markX = markX;
			mc.pwrW = pwrW;
		}
	}

	// Preset chips; the third one doubles as "OK" while a fault is shown.
	if (storeFlash >= 0 && (int32_t)(now - storeFlashUntil) >= 0) storeFlash = -1;
	int8_t active = -1;
	for (uint8_t i = 0; i < 3; i++) {
		if (cfg.presets[i] == cfg.setTemp) { active = i; break; }
	}
	for (uint8_t i = 0; i < 3; i++) {
		int16_t pv = toUnit(cfg.presets[i]);
		bool changed = pv != mc.presets[i] || active != mc.active || storeFlash != mc.flashing ||
		               f != mc.fault;
		if (!changed) continue;
		if (fault != Fault::None && i == 2) chip(2, PSTR("OK"), true, true, C_ERR_BG);
		else presetChip(i, cfg.presets[i], i == active, i == storeFlash);
		mc.presets[i] = pv;
	}
	mc.active = active;
	mc.flashing = storeFlash;
}

void stepSetpoint(int8_t dir, uint32_t heldMs) {
	Settings &cfg = Config::get();
	int16_t t = cfg.setTemp;
	if (heldMs >= BTN_FAST_AFTER_MS) {
		t = dir > 0 ? (t / 5 + 1) * 5 : ((t + 4) / 5 - 1) * 5;
	} else {
		t += dir;
	}
	t = constrain(t, TEMP_SET_MIN, TEMP_SET_MAX);
	if (t != cfg.setTemp) {
		cfg.setTemp = t;
		Config::touch();
	}
	Station::activity();
}

void handleMain(const Event &ev, uint32_t now) {
	Settings &cfg = Config::get();
	bool faulted = Station::fault() != Fault::None;
	switch (ev.button) {
		case BTN_PWR:
			if (ev.type == EV_CLICK) {
				if (faulted) Station::clearFault();
				else if (Station::mode() == Mode::Off) Station::setOn(true);
				else Station::setStandby(Station::mode() != Mode::Standby);
			} else if (ev.type == EV_LONG) {
				if (Station::mode() == Mode::Off) {
					Buttons::flush();
					Ui::showMenu();
				} else {
					Station::setOn(false);
				}
			}
			break;
		case BTN_UP:
		case BTN_DOWN:
			if (ev.type == EV_PRESS || ev.type == EV_REPEAT) {
				stepSetpoint(ev.button == BTN_UP ? 1 : -1, Buttons::heldFor(ev.button, now));
			}
			break;
		default: {
			uint8_t i = ev.button - BTN_T1;
			if (faulted) {
				if (i == 2 && ev.type == EV_CLICK) Station::clearFault();
				break;
			}
			if (ev.type == EV_CLICK) {
				if (cfg.setTemp != cfg.presets[i]) {
					cfg.setTemp = cfg.presets[i];
					Config::touch();
				}
				Station::activity();
			} else if (ev.type == EV_LONG) {
				cfg.presets[i] = cfg.setTemp;
				Config::touch();
				storeFlash = i;
				storeFlashUntil = now + STORE_FLASH_MS;
			}
			break;
		}
	}
}

// ---------------------------------------------------------------------------
// Settings menu
// ---------------------------------------------------------------------------
const int16_t MENU_Y = 19, MENU_ROW = 13;

void menuValue(uint8_t item, char *buf, const char *&pgmStr) {
	const Settings &cfg = Config::get();
	pgmStr = nullptr;
	buf[0] = 0;
	switch (item) {
		case M_STANDBY:
			if (!cfg.standbyMin) pgmStr = tr(S_OFF);
			else putPgm(putUInt(buf, cfg.standbyMin), PSTR(" min"));
			break;
		case M_BOOTHEAT:
			pgmStr = tr(Config::flag(FLAG_HEAT_ON_BOOT) ? S_YES : S_NO);
			break;
		case M_UNIT:
			strcpy_P(buf, fahrenheit() ? PSTR("\xC2\xB0" "F") : PSTR("\xC2\xB0" "C"));
			break;
		case M_LANG:
			pgmStr = tr(S_LANG_NAME);
			break;
		case M_TRIM:
			buf[0] = cfg.trim < 0 ? '-' : '+';
			putPgm(putUInt(buf + 1, abs(cfg.trim)), PSTR(" \xC2\xB0" "C"));
			break;
		case M_SWAP:
			pgmStr = Config::flag(FLAG_BGR) ? PSTR("BGR") : PSTR("RGB");
			break;
	}
}

void drawMenuRow(uint8_t i) {
	static const StrId labels[M_COUNT] = {
		S_MENU_STANDBY, S_MENU_BOOTHEAT, S_MENU_UNIT, S_MENU_LANGUAGE, S_MENU_TRIM, S_MENU_SWAP_RB
	};
	int16_t y = MENU_Y + i * MENU_ROW;
	bool sel = i == menuSel;
	uint16_t bg = sel ? C_TRACK : C_BG;
	if (sel) Gfx::roundRect(2, y, TFT_W - 4, MENU_ROW, bg, C_BG);
	else Tft::fillRect(2, y, TFT_W - 4, MENU_ROW, C_BG);
	if (sel) Tft::fillRect(2, y + 3, 2, MENU_ROW - 6, C_ACCENT_LIGHT);
	Gfx::textBox(&FONT_TEXT, tr(labels[i]), true, 8, y + 1, 92, ALIGN_LEFT, sel ? C_WHITE : C_TEXT, bg);
	char buf[10];
	const char *p;
	menuValue(i, buf, p);
	Gfx::textBox(&FONT_TEXT, p ? p : buf, p != nullptr, 100, y + 1, 54, ALIGN_RIGHT,
	             sel ? C_ACCENT_LIGHT : C_SUBTLE, bg);
}

void drawMenu() {
	Tft::fillScreen(C_BG);
	Gfx::textBox(&FONT_TEXT, tr(S_MENU_TITLE), true, 6, 3, 100, ALIGN_LEFT, C_WHITE, C_BG);
	Gfx::textBox(&FONT_TEXT, PSTR("v" FW_VERSION), true, 106, 3, 50, ALIGN_RIGHT, C_MUTED, C_BG);
	Tft::fillRect(0, 16, TFT_W, 1, C_TRACK);
	for (uint8_t i = 0; i < M_COUNT; i++) drawMenuRow(i);
	chip(0, PSTR("\xE2\x80\x93"), true, false, 0);
	chip(1, PSTR("+"), true, false, 0);
	chip(2, tr(S_DONE), true, true, C_ACCENT);
}

void changeMenuValue(int8_t dir) {
	Settings &cfg = Config::get();
	switch (menuSel) {
		case M_STANDBY:
			cfg.standbyMin = Config::nextStandby(cfg.standbyMin, dir);
			Config::touch();
			break;
		case M_BOOTHEAT:
			Config::setFlag(FLAG_HEAT_ON_BOOT, !Config::flag(FLAG_HEAT_ON_BOOT));
			break;
		case M_UNIT:
			Config::setFlag(FLAG_FAHRENHEIT, !fahrenheit());
			break;
		case M_LANG:
			cfg.language = (cfg.language + LANG_COUNT + dir) % LANG_COUNT;
			i18nSetLanguage(cfg.language);
			Config::touch();
			drawMenu();
			return;
		case M_TRIM:
			cfg.trim = constrain(cfg.trim + dir, -30, 30);
			Config::touch();
			Station::refreshCalibration();
			break;
		case M_SWAP:
			Config::setFlag(FLAG_BGR, !Config::flag(FLAG_BGR));
			Tft::setBgr(Config::flag(FLAG_BGR));
			break;
	}
	drawMenuRow(menuSel);
}

void handleMenu(const Event &ev) {
	if (ev.type != EV_CLICK && ev.type != EV_PRESS && ev.type != EV_REPEAT) return;
	bool step = ev.type == EV_PRESS || ev.type == EV_REPEAT;
	switch (ev.button) {
		case BTN_UP:
		case BTN_DOWN: {
			if (!step) break;
			uint8_t old = menuSel;
			menuSel = ev.button == BTN_UP ? (menuSel + M_COUNT - 1) % M_COUNT : (menuSel + 1) % M_COUNT;
			drawMenuRow(old);
			drawMenuRow(menuSel);
			break;
		}
		case BTN_T1:
			if (ev.type == EV_CLICK) changeMenuValue(-1);
			break;
		case BTN_T2:
			if (ev.type == EV_CLICK) changeMenuValue(1);
			break;
		case BTN_T3:
		case BTN_PWR:
			if (ev.type == EV_CLICK) {
				Config::flush();
				Ui::showMain();
			}
			break;
	}
}

// ---------------------------------------------------------------------------
// Start-up screens
// ---------------------------------------------------------------------------
void colorTest() {
	Tft::fillScreen(C_BG);
	Tft::fillRect(0, 0, TFT_W, 72, C_PURE_RED);
	Gfx::textBox(&FONT_TEXT, tr(S_COLOR_QUESTION), true, 0, 80, TFT_W, ALIGN_CENTER, C_WHITE, C_BG);
	chip(0, tr(S_RED), true, false, 0);
	chip(2, tr(S_BLUE), true, false, 0);
	for (;;) {
		Buttons::update(millis());
		Event ev;
		while (Buttons::next(ev)) {
			if (ev.type != EV_CLICK) continue;
			if (ev.button == BTN_T1 || ev.button == BTN_T3) {
				// The panel starts in BGR mode; "blue" means it needs RGB.
				Config::setFlag(FLAG_BGR, ev.button == BTN_T1);
				Config::setFlag(FLAG_COLOR_KNOWN, true);
				Config::flush();
				Tft::setBgr(Config::flag(FLAG_BGR));
				return;
			}
		}
	}
}

bool splash() {
	Tft::fillScreen(C_BG);
	Gfx::roundRect(56, 30, 48, 4, C_ACCENT, C_BG);
	Gfx::textBox(&FONT_TEXT, PSTR(FW_NAME), true, 0, 44, TFT_W, ALIGN_CENTER, C_WHITE, C_BG);
	Gfx::textBox(&FONT_TEXT, PSTR("Firmware " FW_VERSION), true, 0, 58, TFT_W, ALIGN_CENTER, C_MUTED, C_BG);
	Gfx::textBox(&FONT_TEXT, tr(S_HOLD_FOR_SETTINGS), true, 0, 104, TFT_W, ALIGN_CENTER, C_SUBTLE, C_BG);
	uint32_t start = millis();
	while (millis() - start < 1500) {
		uint32_t now = millis();
		Buttons::update(now);
		if (Buttons::heldFor(BTN_PWR, now) >= 600) {
			Buttons::flush();
			return true;
		}
	}
	Buttons::flush();
	return false;
}

}  // namespace

namespace Ui {

bool begin() {
	i18nSetLanguage(Config::get().language);
	bool known = Config::flag(FLAG_COLOR_KNOWN);
	Tft::begin(known ? Config::flag(FLAG_BGR) : true);
	if (!known) colorTest();
	return splash();
}

void showMain() {
	screen = SCR_MAIN;
	drawMainStatic();
	updateMain(millis());
}

void showMenu() {
	screen = SCR_MENU;
	menuSel = 0;
	drawMenu();
}

void handle(const Event &ev, uint32_t now) {
	if (screen == SCR_MENU) handleMenu(ev);
	else handleMain(ev, now);
}

void update(uint32_t now) {
	if (screen == SCR_MAIN) updateMain(now);
}

}  // namespace Ui
