#pragma once

#include "jf/battle/BattleState.hpp"

namespace jf {

// Deterministic behavior, driven purely by Manhattan distance:
//   1. Find the nearest living player unit.
//   2. Attack immediately if already in range.
//   3. Otherwise move as close as movement allows, then attack if now in range.
//   4. Otherwise end the turn without acting.
// Kept as a free function (rather than a method per class) so future
// class-specific AI can be swapped in per unit without touching BattleState.
// Returns the unit that was attacked this turn (nullptr if none) so the
// caller (BattleController) can report an attack event for UI purposes
// (e.g. driving a front-end attack animation) without EnemyAI knowing
// anything about rendering.
Unit* takeEnemyTurn(BattleState& battle, Unit& enemy);

} // namespace jf
