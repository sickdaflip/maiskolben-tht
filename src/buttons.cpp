// SPDX-License-Identifier: GPL-2.0-or-later
#include "buttons.h"
#include "config.h"

namespace {

const uint8_t PINS[BTN_COUNT] = {
	PIN_SW_PWR, PIN_SW_UP, PIN_SW_DOWN, PIN_SW_T1, PIN_SW_T2, PIN_SW_T3
};

struct State {
	bool raw;
	bool down;
	bool longFired;
	bool muted;  // ignore until released
	uint32_t changed;
	uint32_t pressed;
	uint32_t lastRepeat;
};

State st[BTN_COUNT];

const uint8_t QUEUE = 8;
Event queue[QUEUE];
uint8_t head = 0, tail = 0;
uint32_t lastPoll = 0;

void push(uint8_t b, uint8_t type) {
	if (st[b].muted) return;
	uint8_t n = (head + 1) % QUEUE;
	if (n == tail) return;  // full: drop
	queue[head].button = b;
	queue[head].type = type;
	head = n;
}

bool repeats(uint8_t b) {
	return b == BTN_UP || b == BTN_DOWN;
}

}  // namespace

namespace Buttons {

void begin() {
	for (uint8_t i = 0; i < BTN_COUNT; i++) {
		pinMode(PINS[i], INPUT_PULLUP);
		st[i] = State{false, false, false, false, 0, 0, 0};
	}
}

void update(uint32_t now) {
	if (now == lastPoll) return;
	lastPoll = now;
	for (uint8_t i = 0; i < BTN_COUNT; i++) {
		State &s = st[i];
		bool raw = !digitalRead(PINS[i]);
		if (raw != s.raw) {
			s.raw = raw;
			s.changed = now;
		}
		if (raw != s.down && now - s.changed >= BTN_DEBOUNCE_MS) {
			s.down = raw;
			if (raw) {
				s.pressed = now;
				s.lastRepeat = now;
				s.longFired = false;
				push(i, EV_PRESS);
			} else {
				push(i, s.longFired ? EV_RELEASE : EV_CLICK);
				s.muted = false;
			}
		}
		if (s.down) {
			uint32_t held = now - s.pressed;
			if (!s.longFired && held >= BTN_LONG_MS) {
				s.longFired = true;
				push(i, EV_LONG);
			}
			if (repeats(i) && held >= BTN_REPEAT_DELAY_MS && now - s.lastRepeat >= BTN_REPEAT_MS) {
				s.lastRepeat = now;
				push(i, EV_REPEAT);
			}
		}
	}
}

bool next(Event &ev) {
	if (head == tail) return false;
	ev = queue[tail];
	tail = (tail + 1) % QUEUE;
	return true;
}

bool isDown(uint8_t b) { return st[b].down; }

uint32_t heldFor(uint8_t b, uint32_t now) {
	return st[b].down ? now - st[b].pressed : 0;
}

void flush() {
	head = tail = 0;
	for (uint8_t i = 0; i < BTN_COUNT; i++) {
		if (st[i].down) st[i].muted = true;
	}
}

}  // namespace Buttons
