// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Debounced buttons with click, long press and auto repeat events.
 */
#pragma once
#include <Arduino.h>

enum Button : uint8_t { BTN_PWR = 0, BTN_UP, BTN_DOWN, BTN_T1, BTN_T2, BTN_T3, BTN_COUNT };

enum ButtonEvent : uint8_t {
	EV_NONE = 0,
	EV_PRESS,    // went down
	EV_CLICK,    // released before the long-press time
	EV_LONG,     // held for BTN_LONG_MS (fires once)
	EV_REPEAT,   // auto repeat while held (up/down only)
	EV_RELEASE   // released after a long press
};

struct Event {
	uint8_t button;
	uint8_t type;
};

namespace Buttons {

void begin();
void update(uint32_t now);
bool next(Event &ev);
bool isDown(uint8_t b);
uint32_t heldFor(uint8_t b, uint32_t now);

/** Discards queued events and suppresses events of buttons held right now. */
void flush();

}  // namespace Buttons
