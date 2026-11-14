// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Persistent settings.
 *
 * The block lives at EEPROM address 64 and is protected by a magic word,
 * a layout version and a CRC-8. Addresses 0..63 (the v3.1 layout) are only
 * read, never written, so the original firmware can be flashed back and
 * will find its data unchanged.
 *
 * Writes are deferred until no setting changed for SETTINGS_SAVE_DELAY_MS,
 * which keeps EEPROM wear low while holding the up/down buttons.
 */
#pragma once
#include <Arduino.h>

#define FLAG_HEAT_ON_BOOT  0x01
#define FLAG_FAHRENHEIT    0x02
#define FLAG_BGR           0x04
#define FLAG_COLOR_KNOWN   0x08

struct Settings {
	int16_t setTemp;        // C
	int16_t presets[3];     // C
	float adcGain;
	float adcOffset;
	int8_t trim;            // C, user correction
	uint8_t standbyMin;     // 0 = disabled
	uint8_t flags;
	uint8_t language;
};

namespace Config {

/** Loads settings; migrates the v3.1 layout on first start. */
void begin();

Settings &get();
bool flag(uint8_t f);
void setFlag(uint8_t f, bool on);

/** Marks settings as changed; they are written after a quiet period. */
void touch();

/** Call from loop(); performs the deferred write. */
void service(uint32_t now);

/** Writes immediately if anything is pending. */
void flush();

/** Steps through the allowed auto standby values. */
uint8_t nextStandby(uint8_t current, int8_t dir);

}  // namespace Config
