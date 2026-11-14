// SPDX-License-Identifier: GPL-2.0-or-later
// Minimal Arduino API surface for the host-side UI preview.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define PROGMEM
#define PSTR(s) (s)
#define F(s) (s)
#define pgm_read_byte(p) (*(const uint8_t *)(p))
#define pgm_read_word(p) (*(const uint16_t *)(p))
#define pgm_read_ptr(p) (*(const void *const *)(p))
#define memcpy_P memcpy
#define strcpy_P strcpy
#define A0 14
#define A5 19

typedef bool boolean;
class __FlashStringHelper;

uint32_t millis();
template <class T, class L, class H>
auto constrain(T x, L lo, H hi) -> decltype(x + lo + hi) {
	return x < lo ? lo : (x > hi ? hi : x);
}
using std::abs;
using std::lround;
using std::fabs;
