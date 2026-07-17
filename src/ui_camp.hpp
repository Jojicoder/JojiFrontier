// Public entry point for the Camp screen's rendering, defined in
// ui_camp.cpp. Split out of main.cpp; no behavior change.
#pragma once

#include <raylib.h>

#include "jf/core/GameApp.hpp"

namespace jfui {

void drawCampScreen(jf::GameApp& app, Vector2 mouse, bool clicked);

}  // namespace jfui
