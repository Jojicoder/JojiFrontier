#pragma once

#include <vector>

#include "jf/battle/BattleState.hpp"
#include "jf/data/GameData.hpp"

namespace jf {

Unit instantiateUnit(const GameData& data, const UnitTemplate& unitTemplate, Team team, GridPos pos);

// Fresh 4-vs-4 battle: full-HP player party plus a brand new enemy roster,
// laid out on the fixed 3x8 battlefield.
BattleState createFreshBattle(const GameData& data);

// Used by "Continue Expedition": keeps the surviving players' current HP
// (defeated units stay gone) but spawns a brand new enemy roster.
BattleState createContinuationBattle(const GameData& data, const std::vector<Unit>& survivingPlayers);

} // namespace jf
