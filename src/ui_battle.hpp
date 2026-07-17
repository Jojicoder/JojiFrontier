// Public entry points for the Battle screen's rendering, defined in
// ui_battle.cpp. Split out of main.cpp; no behavior change.
#pragma once

#include <raylib.h>

#include "jf/battle/BattleController.hpp"
#include "jf/core/GameApp.hpp"

namespace jfui {

void drawPhaseBanner(const jf::BattleController& controller);
void drawGrid(const jf::BattleController& controller, float dt);
void drawBattleHud(jf::GameApp& app, Vector2 mouse, bool clicked);
void drawTurnChangeAnnouncement(jf::Phase currentPhase, float dt);
void drawHoverInfo(const jf::GameData& data, const jf::BattleController& controller, Vector2 mouse);
void drawCombatPreviewPopup(const jf::CombatPreview& preview);
void drawObjectAttackPreviewPopup(const jf::ObjectAttackPreview& preview);
void drawVictoryOverlay(jf::GameApp& app, Vector2 mouse, bool clicked);
void drawDefeatOverlay(jf::GameApp& app, Vector2 mouse, bool clicked);

}  // namespace jfui
