// Public entry point for the Pre-Battle Deployment screen's rendering,
// defined in ui_deployment.cpp. Split out of main.cpp; no behavior change.
#pragma once

#include <raylib.h>

#include "jf/core/GameApp.hpp"

namespace jfui {

void drawPreBattleDeploymentScreen(jf::GameApp& app, Vector2 mouse, bool clicked);

}  // namespace jfui
