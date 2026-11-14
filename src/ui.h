// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Screens and input handling: main screen, settings menu, first-start
 * color test and splash screen.
 */
#pragma once
#include <Arduino.h>
#include "buttons.h"

namespace Ui {

/** Initializes the panel, runs the color test if needed and shows the splash.
 *  Returns true if the user asked for the settings menu during start-up. */
bool begin();

void showMain();
void showMenu();

void handle(const Event &ev, uint32_t now);
void update(uint32_t now);

}  // namespace Ui
