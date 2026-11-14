// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Maiskolben THT firmware 4.x
 *
 * Fork of the Maiskolben firmware by Arduino Hannover / HannIO (GPL-2.0).
 * Rewritten control loop, persistence and user interface.
 *
 * Serial protocol (115200 baud), compatible with Maiskolben_GUI:
 *   telemetry every 100 ms:
 *     p1;p2;p3;on;error;standby;0;set;cur;output;0.00;0.00;0.00
 *   commands:
 *     Tnnn   set temperature (C)
 *     Pnttt  store ttt (C) to preset n (1..3)
 *     C      clear fault
 *     S0/S1  leave / enter standby
 *     O0/O1  switch off / on
 *     D      print a heater settling trace (only while off)
 */
#include <Arduino.h>
#include "buttons.h"
#include "config.h"
#include "heater.h"
#include "settings.h"
#include "station.h"
#include "ui.h"

namespace {

uint32_t nextUi = 0;
uint32_t nextTelemetry = 0;

char rx[6];
uint8_t rxLen = 0;
uint32_t rxLast = 0;

uint8_t legacyError(Fault f) {
	switch (f) {
		case Fault::None: return 0;
		case Fault::SensorDrop:
		case Fault::OverTemp: return 1;
		case Fault::NotHeating: return 2;
		case Fault::NoTip: return 3;
	}
	return 0;
}

void telemetry() {
	const Settings &cfg = Config::get();
	for (uint8_t i = 0; i < 3; i++) {
		Serial.print(cfg.presets[i]);
		Serial.print(';');
	}
	Serial.print(Station::mode() != Mode::Off ? 1 : 0);
	Serial.print(';');
	Serial.print(legacyError(Station::fault()));
	Serial.print(';');
	Serial.print(Station::mode() == Mode::Standby ? 1 : 0);
	Serial.print(F(";0;"));
	Serial.print(cfg.setTemp);
	Serial.print(';');
	Serial.print(Station::tipPresent() ? Station::temperature() : 999);
	Serial.print(';');
	// Controller output as a 0..1 fraction with three decimals.
	uint16_t milli = (uint16_t)(Station::output() * 1000 / CTRL_MAX_ON_MS);
	Serial.print(milli / 1000);
	Serial.print('.');
	if (milli % 1000 < 100) Serial.print('0');
	if (milli % 1000 < 10) Serial.print('0');
	Serial.print(milli % 1000);
	Serial.println(F(";0.00;0.00;0.00"));
}

int16_t parseTemp(const char *p) {
	int16_t t = 0;
	for (uint8_t i = 0; i < 3; i++) {
		if (p[i] < '0' || p[i] > '9') return -1;
		t = t * 10 + (p[i] - '0');
	}
	return t;
}

bool validTemp(int16_t t) {
	return t >= TEMP_SET_MIN && t <= TEMP_SET_MAX;
}

/** Executes a complete command; returns false if more bytes are needed. */
bool runCommand() {
	Settings &cfg = Config::get();
	switch (rx[0]) {
		case 'T': {
			if (rxLen < 4) return false;
			int16_t t = parseTemp(rx + 1);
			if (validTemp(t)) {
				cfg.setTemp = t;
				Config::touch();
			}
			return true;
		}
		case 'P': {
			if (rxLen < 5) return false;
			uint8_t slot = rx[1] - '1';
			int16_t t = parseTemp(rx + 2);
			if (slot < 3 && validTemp(t)) {
				cfg.presets[slot] = t;
				Config::touch();
			}
			return true;
		}
		case 'S':
			if (rxLen < 2) return false;
			Station::setStandby(rx[1] == '1');
			return true;
		case 'O':
			if (rxLen < 2) return false;
			Station::setOn(rx[1] == '1');
			return true;
		case 'C':
			Station::clearFault();
			return true;
		case 'D':
			if (Station::mode() == Mode::Off) {
				const Settings &s = Config::get();
				uint16_t maxAdc = (uint16_t)((350 - s.adcOffset - s.trim) / s.adcGain);
				Heater::settleTrace(Serial, maxAdc);
			} else {
				Serial.println(F("# switch the iron off first"));
			}
			return true;
		default:
			return true;  // unknown: drop
	}
}

void serialInput(uint32_t now) {
	if (rxLen && now - rxLast > 200) rxLen = 0;  // stale partial command
	while (Serial.available()) {
		char c = Serial.read();
		if (c == '\r' || c == '\n') continue;
		rxLast = now;
		if (rxLen < sizeof rx) rx[rxLen++] = c;
		if (runCommand()) rxLen = 0;
	}
}

}  // namespace

void setup() {
	pinMode(PIN_HEATER, OUTPUT);
	digitalWrite(PIN_HEATER, LOW);
	pinMode(PIN_HEAT_LED, OUTPUT);
	Serial.begin(SERIAL_BAUD);

	Buttons::begin();
	Config::begin();
	bool menu = Ui::begin();

	Station::begin();
	if (menu) {
		Ui::showMenu();
	} else {
		Ui::showMain();
		if (Config::flag(FLAG_HEAT_ON_BOOT)) Station::setOn(true);
	}
}

void loop() {
	uint32_t now = millis();

	Station::update(now);
	analogWrite(PIN_HEAT_LED, (uint16_t)Heater::appliedOnTime() * 255 / CTRL_MAX_ON_MS);

	Buttons::update(now);
	Event ev;
	while (Buttons::next(ev)) Ui::handle(ev, now);

	if ((int32_t)(now - nextUi) >= 0) {
		nextUi = now + UI_REFRESH_MS;
		Ui::update(now);
	}
	if ((int32_t)(now - nextTelemetry) >= 0) {
		nextTelemetry = now + TELEMETRY_MS;
		telemetry();
	}
	serialInput(now);
	Config::service(now);
}
