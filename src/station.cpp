// SPDX-License-Identifier: GPL-2.0-or-later
#include "station.h"
#include "config.h"
#include "heater.h"
#include "settings.h"

namespace {

Mode curMode = Mode::Off;
Fault curFault = Fault::None;

bool tipOk = false;
bool haveSample = false;
float tLast = 0;       // last raw temperature
float tFilt = 0;       // low-pass filtered temperature
int16_t tShown = 0;    // display value with hysteresis

float integ = 0;
float residual = 0;
float out = 0;
bool inBand = false;
float holdEst = CTRL_HOLD_GUESS;  // learned hold power (ms)
int16_t holdAt = 300;             // target the estimate belongs to

bool heatCheck = false;
uint32_t heatCheckSince = 0;
float heatCheckTemp = 0;

uint32_t lastActivity = 0;
uint32_t standbySince = 0;
bool standbyReached = false;

float toCelsius(uint16_t adc) {
	const Settings &s = Config::get();
	return adc * s.adcGain + s.adcOffset + s.trim;
}

void raise(Fault f) {
	if (curFault == Fault::None) curFault = f;
	curMode = Mode::Off;
	integ = 0;
	residual = 0;
	out = 0;
	Heater::setOnTime(0);
}

int16_t targetFor(Mode m) {
	switch (m) {
		case Mode::On: return Config::get().setTemp;
		case Mode::Standby: return TEMP_STANDBY;
		default: return 0;
	}
}

void runController(float t, uint32_t now) {
	int16_t tgt = targetFor(curMode);
	float e = tgt - t;

	if (fabs(e) <= CTRL_I_BAND) {
		if (!inBand) {
			// Seed the integrator with the expected hold power, scaled by the
			// temperature rise over ambient, to avoid a slow creep-in.
			float scale = (float)(tgt - 25) / (float)(holdAt - 25);
			integ = constrain(holdEst * scale, 0.0f, (float)CTRL_MAX_ON_MS);
			inBand = true;
		}
		float next = integ + CTRL_KI * e;
		float trial = CTRL_KP * e + next;
		bool windupHigh = trial > CTRL_MAX_ON_MS && e > 0;
		bool windupLow = trial < 0 && e < 0;
		if (!windupHigh && !windupLow) integ = constrain(next, 0.0f, (float)CTRL_MAX_ON_MS);
		if (fabs(e) < 2.0f) {
			holdEst += 0.02f * (integ - holdEst);
			holdAt = tgt;
		}
	} else {
		inBand = false;
	}

	out = constrain(CTRL_KP * e + integ, 0.0f, (float)CTRL_MAX_ON_MS);

	// Error diffusion keeps the 1 ms resolution from limiting hold accuracy.
	float want = out + residual;
	uint8_t ms = (uint8_t)want;
	residual = constrain(want - ms, 0.0f, 1.0f);
	Heater::setOnTime(ms);

	// Not heating: saturated output must raise the temperature noticeably.
	if (out >= CTRL_MAX_ON_MS - 0.5f && e > 30) {
		if (!heatCheck) {
			heatCheck = true;
			heatCheckSince = now;
			heatCheckTemp = t;
		} else if (now - heatCheckSince >= FAULT_HEAT_WINDOW_MS) {
			if (t - heatCheckTemp < FAULT_HEAT_MIN_RISE) {
				raise(Fault::NotHeating);
				return;
			}
			heatCheckSince = now;
			heatCheckTemp = t;
		}
	} else {
		heatCheck = false;
	}
}

void autoStandby(float t, uint32_t now) {
	uint8_t minutes = Config::get().standbyMin;
	if (curMode == Mode::On) {
		// A loaded tip drops below target; that counts as "in use".
		if (Config::get().setTemp - t >= ACTIVITY_DROP) lastActivity = now;
		if (minutes && now - lastActivity >= (uint32_t)minutes * 60000UL) {
			Station::setStandby(true);
		}
	} else if (curMode == Mode::Standby) {
		float e = TEMP_STANDBY - t;
		if (fabs(e) <= READY_BAND) standbyReached = true;
		// Wiping the tip on the sponge or touching a joint wakes it up.
		if (standbyReached && e >= WAKE_DROP) {
			Station::setStandby(false);
			return;
		}
		if (minutes && now - standbySince >= (uint32_t)AUTO_OFF_MIN * 60000UL) {
			Station::setOn(false);
		}
	}
}

}  // namespace

namespace Station {

void begin() {
	refreshCalibration();
	Heater::begin();
	lastActivity = millis();
}

void refreshCalibration() {
	const Settings &s = Config::get();
	float adc = (TEMP_OVERSHOOT_TRIP - s.adcOffset - s.trim) / s.adcGain;
	Heater::setTripAdc(adc > 1023 ? 1023 : (uint16_t)adc);
}

void update(uint32_t now) {
	uint16_t adc;
	if (!Heater::takeSample(adc)) return;

	if (adc >= ADC_NO_TIP) {
		tipOk = false;
		haveSample = false;
		if (curMode != Mode::Off) raise(Fault::NoTip);
		Heater::setOnTime(0);
		return;
	}
	tipOk = true;

	float t = toCelsius(adc);
	if (haveSample && curMode != Mode::Off && tLast - t > FAULT_DROP_PER_SAMPLE) {
		raise(Fault::SensorDrop);
	}
	if (t > TEMP_OVERSHOOT_TRIP) raise(Fault::OverTemp);

	tFilt = haveSample ? tFilt + 0.3f * (t - tFilt) : t;
	tLast = t;
	haveSample = true;
	if (fabs(tFilt - tShown) >= 0.75f) tShown = (int16_t)lround(tFilt);

	if (curMode == Mode::Off) {
		integ = 0;
		residual = 0;
		out = 0;
		inBand = false;
		heatCheck = false;
		Heater::setOnTime(0);
		return;
	}
	runController(t, now);
	if (curMode != Mode::Off) autoStandby(t, now);
}

Mode mode() { return curMode; }
Fault fault() { return curFault; }

void clearFault() {
	curFault = Fault::None;
}

void setOn(bool on) {
	if (on) {
		if (curFault != Fault::None || curMode != Mode::Off) return;
		curMode = Mode::On;
		inBand = false;
		heatCheck = false;
		residual = 0;
		lastActivity = millis();
	} else {
		curMode = Mode::Off;
		Heater::setOnTime(0);
	}
}

void setStandby(bool standby) {
	if (curMode == Mode::Off) return;
	Mode next = standby ? Mode::Standby : Mode::On;
	if (next == curMode) return;
	curMode = next;
	inBand = false;
	heatCheck = false;
	standbyReached = false;
	standbySince = millis();
	lastActivity = millis();
}

void activity() {
	lastActivity = millis();
	if (curMode == Mode::Standby) setStandby(false);
}

int16_t temperature() { return tShown; }
bool tipPresent() { return tipOk; }
int16_t target() { return targetFor(curMode); }

uint8_t powerPercent() {
	return (uint16_t)Heater::appliedOnTime() * 100 / CTRL_PERIOD_MS;
}

float output() { return out; }

int16_t countdown(uint32_t now) {
	uint8_t minutes = Config::get().standbyMin;
	if (!minutes) return -1;
	uint32_t limit, since;
	if (curMode == Mode::On) {
		limit = (uint32_t)minutes * 60000UL;
		since = lastActivity;
	} else if (curMode == Mode::Standby) {
		limit = (uint32_t)AUTO_OFF_MIN * 60000UL;
		since = standbySince;
	} else {
		return -1;
	}
	uint32_t elapsed = now - since;
	return elapsed >= limit ? 0 : (int16_t)((limit - elapsed + 999) / 1000);
}

}  // namespace Station
