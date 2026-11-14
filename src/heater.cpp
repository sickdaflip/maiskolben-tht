// SPDX-License-Identifier: GPL-2.0-or-later
#include "heater.h"
#include "config.h"

#define HEATER_ON()  (HEATER_PORT |= _BV(HEATER_BIT))
#define HEATER_OFF() (HEATER_PORT &= ~_BV(HEATER_BIT))

namespace {

volatile uint8_t tick = 0;
volatile uint8_t requested = 0;
volatile uint8_t applied = 0;
volatile uint8_t stalePeriods = 0;
volatile uint16_t tripAdc = 1023;
volatile bool enabled = false;

volatile uint8_t adcStep = 0;
volatile uint16_t adcBuf[3];
volatile uint16_t adcResult = 1023;
volatile bool adcFresh = false;

uint16_t median3(uint16_t a, uint16_t b, uint16_t c) {
	if (a > b) { uint16_t t = a; a = b; b = t; }
	if (b > c) { b = c; }
	return a > b ? a : b;
}

void adcStart() {
	ADCSRA |= _BV(ADSC);
}

}  // namespace

ISR(TIMER1_COMPA_vect) {
	if (!enabled) {
		HEATER_OFF();
		return;
	}
	uint8_t t = tick;
	if (t == 0) {
		HEATER_OFF();
		if (stalePeriods < 255) stalePeriods++;
	} else if (t == CTRL_BLANK_MS) {
		adcStep = 0;
		adcStart();
	} else if (t == CTRL_BLANK_MS + 1) {
		uint16_t adc = adcResult;
		bool safe = stalePeriods <= FAULT_WATCHDOG_PERIODS && adc < ADC_NO_TIP && adc < tripAdc;
		applied = safe ? requested : 0;
		if (applied) HEATER_ON();
	} else if (t >= CTRL_BLANK_MS + 1 + applied) {
		HEATER_OFF();
	}
	if (++t >= CTRL_PERIOD_MS) t = 0;
	tick = t;
}

// First conversion is discarded, the next three are median-filtered.
ISR(ADC_vect) {
	uint16_t v = ADC;
	uint8_t s = adcStep;
	if (s > 0) adcBuf[s - 1] = v;
	if (++s <= 3) {
		adcStep = s;
		adcStart();
	} else {
		adcResult = median3(adcBuf[0], adcBuf[1], adcBuf[2]);
		adcFresh = true;
	}
}

namespace Heater {

void begin() {
	HEATER_OFF();
	pinMode(PIN_HEATER, OUTPUT);
	HEATER_OFF();

	// ADC: AVcc reference, channel A0, prescaler 128 (125 kHz), interrupt on.
	ADMUX = _BV(REFS0) | (PIN_TEMP - A0);
	ADCSRA = _BV(ADEN) | _BV(ADIE) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);

	// Timer1: CTC, prescaler 64, 1 kHz.
	cli();
	TCCR1A = 0;
	TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10);
	TCNT1 = 0;
	OCR1A = (F_CPU / 64 / 1000) - 1;
	TIMSK1 = _BV(OCIE1A);
	enabled = true;
	sei();
}

void setOnTime(uint8_t ms) {
	if (ms > CTRL_MAX_ON_MS) ms = CTRL_MAX_ON_MS;
	uint8_t sreg = SREG;
	cli();
	requested = ms;
	stalePeriods = 0;
	SREG = sreg;
}

void setTripAdc(uint16_t adc) {
	uint8_t sreg = SREG;
	cli();
	tripAdc = adc;
	SREG = sreg;
}

bool takeSample(uint16_t &adc) {
	uint8_t sreg = SREG;
	cli();
	bool fresh = adcFresh;
	adcFresh = false;
	adc = adcResult;
	SREG = sreg;
	return fresh;
}

uint8_t appliedOnTime() {
	return applied;
}

void settleTrace(Print &out, uint16_t maxAdc) {
	const uint8_t SAMPLES = 48;
	const uint16_t STEP_US = 250;
	uint16_t trace[SAMPLES];

	enabled = false;
	HEATER_OFF();
	ADCSRA &= ~_BV(ADIE);
	delay(5);

	auto read = []() -> uint16_t {
		ADCSRA |= _BV(ADSC);
		while (ADCSRA & _BV(ADSC)) {}
		return ADC;
	};

	uint16_t start = read();
	if (start >= ADC_NO_TIP || start > maxAdc) {
		out.println(F("# settle trace refused: tip missing or too hot"));
	} else {
		HEATER_ON();
		delay(300);
		HEATER_OFF();
		uint32_t t0 = micros();
		for (uint8_t i = 0; i < SAMPLES; i++) {
			while (micros() - t0 < (uint32_t)i * STEP_US) {}
			trace[i] = read();
		}
		out.println(F("# us;adc  (heater switched off at 0 us)"));
		for (uint8_t i = 0; i < SAMPLES; i++) {
			out.print((uint32_t)i * STEP_US);
			out.print(';');
			out.println(trace[i]);
		}
	}
	ADCSRA |= _BV(ADIF) | _BV(ADIE);
	stalePeriods = 0;
	requested = 0;
	enabled = true;
}

}  // namespace Heater
