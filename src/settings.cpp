// SPDX-License-Identifier: GPL-2.0-or-later
#include "settings.h"
#include <EEPROM.h>
#include "config.h"

namespace {

const uint16_t EE_BASE = 64;
const uint16_t EE_MAGIC = 0x4B4D;  // "MK"
const uint8_t EE_LAYOUT = 1;

// v3.1 layout (read-only)
const uint8_t V31_CHECK_ADDR = 0;
const uint8_t V31_CHECK = 42;
const uint8_t V31_PRESETS = 2;
const uint8_t V31_SET_T = 8;
const uint8_t V31_VERSION = 10;
const uint8_t V31_DISPLAY = 11;
const uint8_t V31_OPTIONS = 12;
const uint8_t V31_GAIN = 14;
const uint8_t V31_OFFSET = 18;
const uint8_t V31_BGR = 0x2;
const uint8_t V31_RGB = 0x0;

const uint8_t STANDBY_STEPS[] = {0, 1, 2, 3, 5, 10, 15, 30};

struct Stored {
	uint16_t magic;
	uint8_t layout;
	Settings s;
	uint8_t crc;
};

Settings cur;
bool dirty = false;
uint32_t lastTouch = 0;

uint8_t crc8(const uint8_t *p, uint8_t n) {
	uint8_t crc = 0;
	while (n--) {
		crc ^= *p++;
		for (uint8_t i = 0; i < 8; i++) crc = crc & 0x80 ? (crc << 1) ^ 0x07 : crc << 1;
	}
	return crc;
}

int16_t clampTemp(int16_t t, int16_t fallback) {
	return (t >= TEMP_SET_MIN && t <= TEMP_SET_MAX) ? t : fallback;
}

int16_t readBE16(uint8_t addr) {
	return (int16_t)((EEPROM.read(addr) << 8) | EEPROM.read(addr + 1));
}

bool plausibleCalibration(float gain, float offset) {
	return !isnan(gain) && !isnan(offset) && gain > 0.2f && gain < 1.5f &&
	       offset > -50.0f && offset < 150.0f;
}

void defaults() {
	cur.setTemp = 330;
	cur.presets[0] = 250;
	cur.presets[1] = 300;
	cur.presets[2] = 330;
	cur.adcGain = ADC_GAIN_DEFAULT;
	cur.adcOffset = ADC_OFFSET_DEFAULT;
	cur.trim = 0;
	cur.standbyMin = 5;
	cur.flags = 0;
	cur.language = 0;
}

void migrateV31() {
	if (EEPROM.read(V31_CHECK_ADDR) != V31_CHECK) return;
	for (uint8_t i = 0; i < 3; i++) {
		cur.presets[i] = clampTemp(readBE16(V31_PRESETS + i * 2), cur.presets[i]);
	}
	cur.setTemp = clampTemp(readBE16(V31_SET_T), cur.setTemp);

	uint8_t disp = EEPROM.read(V31_DISPLAY);
	if (disp == V31_BGR || disp == V31_RGB) {
		cur.flags |= FLAG_COLOR_KNOWN;
		if (disp == V31_BGR) cur.flags |= FLAG_BGR;
	}
	uint8_t ver = EEPROM.read(V31_VERSION);
	if (ver >= 23 && ver != 0xFF) {
		uint8_t opt = EEPROM.read(V31_OPTIONS);
		cur.standbyMin = (opt & 1) ? 4 : 0;  // v3.1 used a fixed 240 s timeout
		if (opt & 2) cur.flags |= FLAG_HEAT_ON_BOOT;
		if (opt & 4) cur.flags |= FLAG_FAHRENHEIT;
	}
	float gain, offset;
	EEPROM.get(V31_GAIN, gain);
	EEPROM.get(V31_OFFSET, offset);
	if (plausibleCalibration(gain, offset)) {
		cur.adcGain = gain;
		cur.adcOffset = offset;
	}
}

void write() {
	Stored st;
	st.magic = EE_MAGIC;
	st.layout = EE_LAYOUT;
	st.s = cur;
	st.crc = crc8((const uint8_t *)&st.s, sizeof st.s);
	const uint8_t *p = (const uint8_t *)&st;
	for (uint8_t i = 0; i < sizeof st; i++) EEPROM.update(EE_BASE + i, p[i]);
	dirty = false;
}

}  // namespace

namespace Config {

void begin() {
	Stored st;
	EEPROM.get(EE_BASE, st);
	if (st.magic == EE_MAGIC && st.layout == EE_LAYOUT &&
	    st.crc == crc8((const uint8_t *)&st.s, sizeof st.s)) {
		cur = st.s;
		// Sanitize in case of a partial write or manual tampering.
		cur.setTemp = clampTemp(cur.setTemp, 330);
		for (uint8_t i = 0; i < 3; i++) cur.presets[i] = clampTemp(cur.presets[i], 300);
		if (!plausibleCalibration(cur.adcGain, cur.adcOffset)) {
			cur.adcGain = ADC_GAIN_DEFAULT;
			cur.adcOffset = ADC_OFFSET_DEFAULT;
		}
		if (cur.trim < -30 || cur.trim > 30) cur.trim = 0;
		return;
	}
	defaults();
	migrateV31();
	write();
}

Settings &get() { return cur; }

bool flag(uint8_t f) { return cur.flags & f; }

void setFlag(uint8_t f, bool on) {
	if (on) cur.flags |= f;
	else cur.flags &= ~f;
	touch();
}

void touch() {
	dirty = true;
	lastTouch = millis();
}

void service(uint32_t now) {
	if (dirty && now - lastTouch >= SETTINGS_SAVE_DELAY_MS) write();
}

void flush() {
	if (dirty) write();
}

uint8_t nextStandby(uint8_t current, int8_t dir) {
	const uint8_t n = sizeof STANDBY_STEPS;
	uint8_t idx = 0;
	while (idx < n - 1 && STANDBY_STEPS[idx] < current) idx++;
	if (dir > 0 && idx < n - 1) idx++;
	else if (dir < 0 && idx > 0) idx--;
	return STANDBY_STEPS[idx];
}

}  // namespace Config
