// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * UI strings for en_US, en_GB, de, es and fr (UTF-8, stored in flash).
 */
#pragma once
#include <Arduino.h>

enum Lang : uint8_t {
	LANG_EN_US = 0,
	LANG_EN_GB,
	LANG_DE,
	LANG_ES,
	LANG_FR,
	LANG_COUNT
};

enum StrId : uint8_t {
	S_LANG_NAME = 0,
	S_STATE_HEAT,
	S_STATE_READY,
	S_STATE_STANDBY,
	S_STATE_OFF,
	S_STATE_ERROR,
	S_SET,
	S_ERR_NO_TIP_1,
	S_ERR_NO_TIP_2,
	S_ERR_NOT_HEATING_1,
	S_ERR_NOT_HEATING_2,
	S_ERR_DROP_1,
	S_ERR_DROP_2,
	S_ERR_OVERTEMP_1,
	S_ERR_OVERTEMP_2,
	S_MENU_TITLE,
	S_MENU_STANDBY,
	S_MENU_BOOTHEAT,
	S_MENU_UNIT,
	S_MENU_LANGUAGE,
	S_MENU_TRIM,
	S_MENU_SWAP_RB,
	S_OFF,
	S_YES,
	S_NO,
	S_DONE,
	S_COLOR_QUESTION,
	S_RED,
	S_BLUE,
	S_HOLD_FOR_SETTINGS,
	S_COUNT
};

void i18nSetLanguage(uint8_t lang);

/** Returns a flash pointer to the UTF-8 string in the active language. */
const char *tr(StrId id);

/** Same, for an explicit language (used by the language picker). */
const char *trLang(uint8_t lang, StrId id);
