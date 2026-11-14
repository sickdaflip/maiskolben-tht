// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Station state machine: on/off/standby, PI temperature control,
 * fault detection and auto standby.
 */
#pragma once
#include <Arduino.h>

enum class Mode : uint8_t { Off, On, Standby };

enum class Fault : uint8_t { None, NoTip, NotHeating, SensorDrop, OverTemp };

namespace Station {

void begin();

/** Processes a new ADC sample if available; call from loop(). */
void update(uint32_t now);

Mode mode();
Fault fault();
void clearFault();

void setOn(bool on);
void setStandby(bool standby);

/** Registers user interaction: resets the standby timer and wakes up. */
void activity();

/** Filtered tip temperature in C (display value). */
int16_t temperature();
bool tipPresent();

/** Current target in C (0 when off). */
int16_t target();

/** Heater output in percent of full power. */
uint8_t powerPercent();

/** Raw controller output in ms per period (telemetry). */
float output();

/** Seconds until the next automatic transition, or -1 if none. */
int16_t countdown(uint32_t now);

/** Recomputes the ADC trip level after calibration or trim changes. */
void refreshCalibration();

}  // namespace Station
