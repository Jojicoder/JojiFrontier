// Public entry point for the Exploration screen's rendering, defined in
// ui_exploration.cpp. Split out of main.cpp; no behavior change.
#pragma once

#include <raylib.h>

#include "jf/core/GameApp.hpp"

namespace jfui {

void drawExplorationScreen(jf::GameApp& app, Vector2 mouse, bool clicked);

}  // namespace jfui
