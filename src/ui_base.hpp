// Public entry point for the Base screen's rendering, defined in
// ui_base.cpp. Split out of main.cpp; no behavior change.
#pragma once

#include <raylib.h>

#include "jf/core/GameApp.hpp"

namespace jfui {

void drawBaseScreen(jf::GameApp& app, Vector2 mouse, bool clicked);

}  // namespace jfui
