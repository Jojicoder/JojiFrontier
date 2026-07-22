#include <cassert>

#ifdef NDEBUG
#error "jf_content_tests requires assertions; NDEBUG must not be defined"
#endif

#include <array>
#include <cstdint>
#include <iostream>
#include <queue>
#include <set>

#include "jf/battle/BattleFactory.hpp"
#include "jf/battle/Movement.hpp"
#include "jf/battle/ObjectiveTracker.hpp"
#include "jf/core/Region.hpp"

// docs/implementation_roadmap.md M1-E slice7 "コンテンツ検査ツール": for every
// shipped Region/Stage, regenerate the battle across many seeds and check the
// structural invariants a hand-authored StageDescriptor/StageContentData
// entry could otherwise violate silently - no route across the board, a
// spawn landing out of bounds or overlapping another spawn, an
// ObjectPlacementRule/HerbPatchGenerationRule placing the wrong count. This
// is meant to catch data-authoring mistakes in `data/regions.json` (and its
// future per-M9-region siblings) at CTest time rather than in play - the
// same role jf_locale_tests plays for `data/locales/*.json`.
//
// Deliberately NOT a balance check (that's jf_forest_balance's job, and it's
// a manually-run tool, not a CTest gate) - this only asks "is the generated
// battle structurally valid," never "is it winnable/fair."

namespace {

constexpr std::uint32_t kSeedCount = 100;

// Movement-point-agnostic reachability (mirrors BattleFactory.cpp's
// internal hasRouteAcross()/hasRouteAcrossWithBlockedTiles(), which aren't
// exported): a raw BFS over passable, not-blocksMovement-Object Tiles, left
// edge to right edge. Deliberately ignores Unit occupancy and MOV stats -
// those are per-turn tactical constraints, not permanent board blockages,
// and computeReachableTiles() (which respects both) is the wrong tool here:
// an 8-column board and a MOV-4 unit legitimately can't reach column 7 in
// one move, which isn't a route-generation bug.
bool hasRouteAcrossBoard(const jf::BattleState& battle) {
    std::array<bool, jf::kGridRows * jf::kGridCols> visited{};
    std::queue<jf::GridPos> open;
    auto blocked = [&](jf::GridPos pos) { return battle.objectBlocksMovementAt(pos); };
    for (int row = 0; row < jf::kGridRows; ++row) {
        jf::GridPos start{row, 0};
        if (jf::isPassable(battle.terrainAt(start)) && !blocked(start)) {
            visited[row * jf::kGridCols] = true;
            open.push(start);
        }
    }
    constexpr jf::GridPos directions[] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    while (!open.empty()) {
        jf::GridPos current = open.front();
        open.pop();
        if (current.col == jf::kGridCols - 1) return true;
        for (jf::GridPos direction : directions) {
            jf::GridPos next{current.row + direction.row, current.col + direction.col};
            if (!jf::isInBounds(next)) continue;
            int key = next.row * jf::kGridCols + next.col;
            if (visited[key] || !jf::isPassable(battle.terrainAt(next)) || blocked(next)) continue;
            visited[key] = true;
            open.push(next);
        }
    }
    return false;
}

void checkStageWithOutcome(const jf::GameData& data, const jf::StageDescriptor& stage,
                           const jf::ExplorationOutcome& outcome, std::uint32_t seedCount) {
    for (std::uint32_t seed = 0; seed < seedCount; ++seed) {
        const jf::BattleState battle = jf::createScenarioBattle(data, stage, seed, outcome);

        std::set<std::pair<int, int>> occupied;
        int playerCount = 0, enemyCount = 0;
        for (const jf::Unit& unit : battle.units()) {
            assert(unit.position.row >= 0 && unit.position.row < jf::kGridRows);
            assert(unit.position.col >= 0 && unit.position.col < jf::kGridCols);
            assert(jf::isPassable(battle.terrainAt(unit.position)));
            const auto key = std::make_pair(unit.position.row, unit.position.col);
            assert(!occupied.contains(key) && "two Units spawned on the same Tile");
            occupied.insert(key);
            if (unit.team == jf::Team::Player) ++playerCount;
            else ++enemyCount;
        }
        assert(playerCount > 0);
        const std::vector<jf::UnitTemplate>& roster =
            stage.enemyRoster.empty() ? data.enemyRoster : stage.enemyRoster;
        std::size_t expectedEnemies = std::min(stage.enemyCountOverride.value_or(roster.size()), roster.size());
        // BattleFactory.cpp's buildEnemies(): the chosen route can remove a
        // fixed count of enemies (e.g. ironwatch_stores' CollapsedSidePath
        // dropping the archer) on top of the base roster/override.
        expectedEnemies -= std::min(expectedEnemies, static_cast<std::size_t>(std::max(0, outcome.enemiesRemoved)));
        assert(static_cast<std::size_t>(enemyCount) == expectedEnemies);

        assert(hasRouteAcrossBoard(battle) && "no left-to-right route across the generated board");

        for (const jf::StageDescriptor::ObjectPlacementRule& rule : stage.objectPlacementRules) {
            int placed = 0;
            for (const jf::BattleObjectState& object : battle.objects()) {
                if (object.definitionId != rule.definition.definitionId) continue;
                ++placed;
                assert(object.position.col >= rule.zoneMinCol && object.position.col <= rule.zoneMaxCol);
                assert(!battle.unitAt(object.position) && "Object placed on an occupied Tile");
            }
            const int expectedCount =
                rule.count + (rule.scalesWithExtraBarrierOutcome ? outcome.extraBarrierCount : 0);
            assert(placed == expectedCount);
        }

        if (stage.herbPatchGeneration) {
            int herbTiles = 0;
            for (int row = 0; row < jf::kGridRows; ++row) {
                for (int col = 0; col < jf::kGridCols; ++col) {
                    if (battle.terrainAt({row, col}) == jf::TerrainType::HerbPatch) ++herbTiles;
                }
            }
            assert(herbTiles == stage.herbPatchGeneration->count);
        }

        // docs/implementation_roadmap.md M1-E「M9前ブロッカー」項目4(静的部分):
        // BattleFactory.cpp's createScenarioBattle() already runs
        // validateBattleMission() on every assembled battle, but a failure
        // there only prints to stderr - it doesn't fail the build. Assert it
        // here instead, so a broken Objective/BattleObject configuration
        // (missing primary group, dangling references, etc.) fails CTest.
        const std::vector<std::string> missionErrors =
            jf::validateBattleMission(battle.missionState(), battle);
        assert(missionErrors.empty() && "validateBattleMission() reported errors for this stage/route/seed");

        // Determinism: the same seed must reproduce the same battle exactly
        // (docs/battle_objects.md "ランダム生成"'s Snapshot-ability depends on
        // this - Save re-derives from Seed + Definition rather than storing
        // the full board).
        const jf::BattleState replay = jf::createScenarioBattle(data, stage, seed, outcome);
        assert(replay.units().size() == battle.units().size());
        for (std::size_t i = 0; i < battle.units().size(); ++i) {
            assert(replay.units()[i].position == battle.units()[i].position);
            assert(replay.units()[i].id == battle.units()[i].id);
        }
        for (int row = 0; row < jf::kGridRows; ++row) {
            for (int col = 0; col < jf::kGridCols; ++col) {
                assert(replay.terrainAt({row, col}) == battle.terrainAt({row, col}));
            }
        }
    }
}

// docs/implementation_roadmap.md M1-E「M9前ブロッカー」項目4(静的部分): the
// default-outcome-only loop above used to be the entire check, leaving every
// route's routeOutcomes override (e.g. signal_tower's OperateObject primary,
// last_signal's DefeatUnit primary) untested by CTest. Route Variants get a
// lighter Seed count than the default route to keep runtime reasonable -
// this is a structural check, not a balance measurement, so fewer Seeds are
// enough to catch a data-authoring mistake in a route-specific override.
constexpr std::uint32_t kRouteVariantSeedCount = 20;

void checkStage(const jf::GameData& data, const jf::StageDescriptor& stage) {
    // FrontalAdvance is each stage's default/primary route (some stages, e.g.
    // signal_tower/last_signal, still attach a non-default ExplorationOutcome
    // to it via routeOutcomes - never assume it equals ExplorationOutcome{}),
    // so it keeps the full Seed count; the other 2 choices are lighter checks.
    checkStageWithOutcome(data, stage, jf::stageRouteOutcome(stage, jf::ExplorationChoice::FrontalAdvance),
                         kSeedCount);
    for (jf::ExplorationChoice choice :
        {jf::ExplorationChoice::CollapsedSidePath, jf::ExplorationChoice::ScoutRoute}) {
        checkStageWithOutcome(data, stage, jf::stageRouteOutcome(stage, choice), kRouteVariantSeedCount);
    }
}

} // namespace

int main() {
    auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
    assert(data);

    for (jf::RegionId regionId :
        {jf::RegionId::CinderwatchGate, jf::RegionId::AshboughForest, jf::RegionId::AshironQuarry}) {
        const jf::RegionDescriptor region = jf::regionDescriptor(regionId, *data);
        assert(!region.stages.empty());
        for (const jf::StageDescriptor& stage : region.stages) {
            if (!stage.contentImplemented) continue;
            checkStage(*data, stage);
        }
    }

    std::cout << "Content tests PASSED" << std::endl;
    return 0;
}
