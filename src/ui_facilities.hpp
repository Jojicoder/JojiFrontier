// Public entry points for the Facilities and Unit screens' rendering,
// defined in ui_facilities.cpp. Split out of main.cpp; no behavior change.
#pragma once

#include <raylib.h>

#include "jf/core/GameApp.hpp"

namespace jfui {

void drawFacilitiesScreen(jf::GameApp& app, Vector2 mouse, bool clicked);
void drawUnitScreen(jf::GameApp& app, Vector2 mouse, bool clicked);

}  // namespace jfui
