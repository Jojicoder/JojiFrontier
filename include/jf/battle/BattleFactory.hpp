#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "jf/battle/BattleState.hpp"
#include "jf/data/GameData.hpp"
#include "jf/core/Exploration.hpp"

namespace jf {

enum class FieldType {
    CinderwatchOutpost,
    AshRoad,
    SignalTower
};

FieldType fieldTypeForStage(int stage);
std::array<TerrainType, kGridRows * kGridCols> generateFieldTerrain(FieldType field,
                                                                    std::uint32_t seed);

Unit instantiateUnit(const GameData& data, const UnitTemplate& unitTemplate, Team team, GridPos pos);

// Fresh 4-vs-4 battle: full-HP player party plus a brand new enemy roster,
// laid out on the fixed 3x8 battlefield.
BattleState createFreshBattle(const GameData& data);

// Used by "Continue Expedition": keeps the surviving players' current HP
// (defeated units stay gone) but spawns a brand new enemy roster.
BattleState createContinuationBattle(const GameData& data, const std::vector<Unit>& survivingPlayers);

// `customPlayerPositions`, when supplied, must have one entry per
// data.playerParty member (same order) and overrides the fixed formation
// spawns - used for free deployment (see ExplorationOutcome::enableFreeDeployment).
BattleState createScenarioBattle(const GameData& data, int stage, std::uint32_t seed,
                                 ExplorationOutcome outcome = {},
                                 const std::vector<GridPos>* customPlayerPositions = nullptr);
BattleState createScenarioContinuationBattle(const GameData& data,
                                               const std::vector<Unit>& survivingPlayers,
                                               int stage,
                                               std::uint32_t seed);

// Builds the enemy roster for a stage/outcome without touching player units
// or terrain - used to show the player what they're walking into during
// free deployment, before the battle actually starts.
std::vector<Unit> previewEnemies(const GameData& data, int stage, ExplorationOutcome outcome = {});

} // namespace jf
