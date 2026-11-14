// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Maiskolben THT firmware - build-time configuration.
 *
 * Target: Maiskolben THT (board rev. 1.3c), Arduino Nano (ATmega328P, 16 MHz),
 * 1.8" 128x160 SPI TFT, Weller RT-series tips.
 */
#pragma once
#include <Arduino.h>

#define FW_NAME    "Maiskolben"
#define FW_VERSION "4.0.1"

// ---------------------------------------------------------------------------
// Pin map (identical to the original v3.1 firmware / THT schematic)
// ---------------------------------------------------------------------------
#define PIN_SW_PWR     2   // power / standby button
#define PIN_HEATER     3   // gate driver (2N7000 -> IRF5305), active high
#define PIN_SW_DOWN    4
#define PIN_HEAT_LED   5   // PWM capable (Timer0)
#define PIN_SW_UP      6
#define PIN_SW_T3      7
#define PIN_SW_T2      8
#define PIN_SW_T1      9
#define PIN_TFT_CS     10
#define PIN_TFT_DC     A5
#define PIN_TEMP       A0  // thermocouple amplifier output (OPA703, gain 681)

// Direct port access for the heater output (D3 = PD3).
#define HEATER_PORT    PORTD
#define HEATER_BIT     3

// ---------------------------------------------------------------------------
// Temperature limits in degrees Celsius
// Weller RT tips are specified for 450 C max.
// ---------------------------------------------------------------------------
#define TEMP_SET_MIN        200
#define TEMP_SET_MAX        450
#define TEMP_STANDBY        150
#define TEMP_OVERSHOOT_TRIP (TEMP_SET_MAX + 40)  // hard cut-off
#define TEMP_BAR_MIN        50                   // temperature bar scale
#define TEMP_BAR_MAX        450

// ---------------------------------------------------------------------------
// Heater timing
//
// The tip carries heater and thermocouple on shared conductors, so the
// temperature can only be read while the heater is off. Each control period
// consists of a blanking phase (heater off, amplifier settles), one ADC
// measurement and an on-phase whose length is set by the controller.
//
// The analog front end on the THT board has a time constant of ~56 us
// (5k6 / 10 nF on both amplifier input and output). The original firmware
// blanked for 10 ms out of every 20 ms, which capped the heater at 50 %.
// 3 ms is conservative; verify on your unit with the serial 'D' command
// (prints a settling trace) before going lower.
// ---------------------------------------------------------------------------
#define CTRL_PERIOD_MS   40  // one control cycle = 25 Hz
#define CTRL_BLANK_MS     3  // heater off before sampling
#define CTRL_MAX_ON_MS   (CTRL_PERIOD_MS - CTRL_BLANK_MS - 1)  // 36 ms = 90 %

// ---------------------------------------------------------------------------
// PI controller (output unit: heater on-time in ms per control period)
// Start values; watch the serial telemetry while tuning.
// ---------------------------------------------------------------------------
#define CTRL_KP            1.0f   // ms per degree of error
#define CTRL_KI            0.02f  // ms per degree per period
#define CTRL_I_BAND        20     // integrate only within +/- this error
#define CTRL_HOLD_GUESS    2.0f   // initial hold-power estimate in ms

// ---------------------------------------------------------------------------
// ADC -> temperature (defaults if no calibration is stored in EEPROM)
// ---------------------------------------------------------------------------
#define ADC_GAIN_DEFAULT    0.54f
#define ADC_OFFSET_DEFAULT  42.8f
#define ADC_NO_TIP          900   // open sensor reads near full scale

// ---------------------------------------------------------------------------
// Fault detection
// ---------------------------------------------------------------------------
#define FAULT_DROP_PER_SAMPLE  50    // C drop within one period -> sensor short
#define FAULT_HEAT_WINDOW_MS   3000  // full power must raise the temperature...
#define FAULT_HEAT_MIN_RISE    10    // ...by at least this many degrees
#define FAULT_WATCHDOG_PERIODS 4     // heater off if main loop stalls

// ---------------------------------------------------------------------------
// Auto standby / auto off
// ---------------------------------------------------------------------------
#define AUTO_OFF_MIN           15    // minutes in standby before switching off
#define ACTIVITY_DROP          3     // C below target counts as "in use"
#define WAKE_DROP              8     // C below standby temp wakes the iron
#define READY_BAND             5     // +/- C considered "ready"

// ---------------------------------------------------------------------------
// User interface
// ---------------------------------------------------------------------------
#define UI_REFRESH_MS          100
#define SETTINGS_SAVE_DELAY_MS 3000  // defer EEPROM writes to save wear
#define BTN_DEBOUNCE_MS        25
#define BTN_LONG_MS            900
#define BTN_REPEAT_DELAY_MS    400
#define BTN_REPEAT_MS          70
#define BTN_FAST_AFTER_MS      2000  // switch to 5-degree steps after this

#define SERIAL_BAUD            115200
#define TELEMETRY_MS           100
