// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Heater power stage and temperature sampling.
 *
 * Timer1 ticks at 1 kHz. Each control period of CTRL_PERIOD_MS runs:
 *   tick 0 ............ heater off, blanking starts
 *   tick BLANK ........ ADC conversions start (interrupt driven, ~0.42 ms)
 *   tick BLANK+1 ...... heater on for the requested number of ms
 *
 * The main loop picks up each new sample, runs the controller and hands the
 * next on-time back with setOnTime(). If the main loop stops doing that, the
 * ISR switches the heater off on its own after FAULT_WATCHDOG_PERIODS. The
 * ISR also cuts the heater immediately on an open sensor or an ADC reading
 * above the overtemperature limit.
 */
#pragma once
#include <Arduino.h>

namespace Heater {

void begin();

/** Heater on-time for the next period(s), 0..CTRL_MAX_ON_MS. */
void setOnTime(uint8_t ms);

/** ADC value above which the ISR cuts power without waiting for the loop. */
void setTripAdc(uint16_t adc);

/** True once per new sample; returns the median ADC value. */
bool takeSample(uint16_t &adc);

/** On-time actually applied in the current period. */
uint8_t appliedOnTime();

/**
 * Diagnostic: heats briefly at full power, then prints the ADC settling
 * curve after switch-off (time in us, raw ADC) to `out`. Used to choose
 * CTRL_BLANK_MS without an oscilloscope. Refuses if the tip is above 350 C
 * or missing.
 */
void settleTrace(Print &out, uint16_t maxAdc);

}  // namespace Heater
