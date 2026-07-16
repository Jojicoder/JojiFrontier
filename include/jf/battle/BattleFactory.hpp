#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "jf/battle/BattleState.hpp"
#include "jf/core/Exploration.hpp"
#include "jf/core/TerrainProfile.hpp"
#include "jf/data/GameData.hpp"

namespace jf {

// jf/core/Region.hpp - forward-declared to keep the battle factory independent
// from campaign definitions. BattleFactory.cpp includes the full definition.
struct StageDescriptor;

std::array<TerrainType, kGridRows * kGridCols> generateFieldTerrain(const TerrainProfile& profile,
                                                                    std::uint32_t seed);

// Per-unit loadouts selected from the unit page at the base.
using WeaponOverrides = std::unordered_map<std::string, std::string>;

Unit instantiateUnit(const GameData& data, const UnitTemplate& unitTemplate, Team team, GridPos pos,
                     const WeaponOverrides* weaponOverrides = nullptr);

// Fresh 4-vs-4 battle: full-HP player party plus a brand new enemy roster,
// laid out on the 3x8 battlefield with randomized terrain and formation.
BattleState createFreshBattle(const GameData& data);

// Used by "Continue Expedition": keeps the surviving players' current HP
// (defeated units stay gone) but spawns a brand new enemy roster.
BattleState createContinuationBattle(const GameData& data, const std::vector<Unit>& survivingPlayers);

// `customPlayerPositions`, when supplied, must have one entry per
// data.playerParty member (same order) and overrides the randomized formation
// spawns - used for free deployment (see ExplorationOutcome::enableFreeDeployment).
// If `stage.surveyObjectiveId` is set, a secondary SecureTile objective is
// added at a randomly chosen valid (passable, unoccupied) tile among the
// enemy-side columns, after terrain/units are placed.
BattleState createScenarioBattle(const GameData& data, const StageDescriptor& stage, std::uint32_t seed,
                                 ExplorationOutcome outcome = {},
                                 const std::vector<GridPos>* customPlayerPositions = nullptr,
                                 const WeaponOverrides* weaponOverrides = nullptr);
BattleState createScenarioContinuationBattle(const GameData& data,
                                               const std::vector<Unit>& survivingPlayers,
                                               const StageDescriptor& stage,
                                               std::uint32_t seed,
                                               ExplorationOutcome outcome = {},
                                               const std::vector<GridPos>* customPlayerPositions = nullptr);

// Builds the enemy roster for a stage/outcome without touching player units
// or terrain - used to show the player what they're walking into during
// free deployment, before the battle actually starts. `seed` must match the
// seed createScenarioBattle() will use so the preview's random starting
// positions are the same ones the real battle spawns them at.
// `livingPlayerCount` should be the actual incoming party's living member
// count so StageDescriptor::understaffedReinforcement previews correctly;
// left at its default (never triggers understaffed reinforcement) by any
// caller that doesn't pass it explicitly.
std::vector<Unit> previewEnemies(const GameData& data, const StageDescriptor& stage, std::uint32_t seed,
                                 ExplorationOutcome outcome = {}, int livingPlayerCount = 1000000);

} // namespace jf
