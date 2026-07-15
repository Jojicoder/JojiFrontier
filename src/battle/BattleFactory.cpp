#include "jf/battle/BattleFactory.hpp"

#include "jf/battle/Objective.hpp"
#include "jf/battle/ObjectiveTracker.hpp"
#include "jf/core/Region.hpp"

#include <array>
#include <algorithm>
#include <cstdio>
#include <optional>
#include <random>
#include <queue>
#include <utility>

namespace jf {

namespace {
// Both teams start somewhere random within their own edge 3x3 block (all 3
// rows, the 3 columns nearest their edge) rather than a fixed formation.
// Terrain is generated across the full board. If a spawn lands on an
// impassable tile, only that exact tile is opened during assembly.
constexpr int kSpawnZoneWidth = 3;
constexpr int kLeftZoneMinCol = 0;
constexpr int kLeftZoneMaxCol = kSpawnZoneWidth - 1;                 // 2
constexpr int kRightZoneMinCol = kGridCols - kSpawnZoneWidth;        // 5
constexpr int kRightZoneMaxCol = kGridCols - 1;                      // 7
constexpr std::uint32_t kPlayerSpawnSalt = 0x9e3779b9u;
constexpr std::uint32_t kEnemySpawnSalt = 0x85ebca6bu;

std::vector<GridPos> zoneTiles(int minCol, int maxCol) {
    std::vector<GridPos> tiles;
    for (int row = 0; row < kGridRows; ++row) {
        for (int col = minCol; col <= maxCol; ++col) tiles.push_back(GridPos{row, col});
    }
    return tiles;
}

// Deterministic per-seed shuffle - the same seed always produces the same
// starting formation, matching generateFieldTerrain()'s determinism.
std::vector<GridPos> randomSpawnPositions(int minCol, int maxCol, std::uint32_t seed) {
    std::vector<GridPos> tiles = zoneTiles(minCol, maxCol);
    std::mt19937 rng(seed);
    std::shuffle(tiles.begin(), tiles.end(), rng);
    return tiles;
}

GridPos playerSpawnFallback() { return GridPos{0, kLeftZoneMinCol}; }
GridPos enemySpawnFallback() { return GridPos{0, kRightZoneMaxCol}; }

bool hasRouteAcross(const std::array<TerrainType, kGridRows * kGridCols>& terrain) {
    std::array<bool, kGridRows * kGridCols> visited{};
    std::queue<GridPos> open;
    for (int row = 0; row < kGridRows; ++row) {
        GridPos start{row, 0};
        if (isPassable(terrain[row * kGridCols])) {
            visited[row * kGridCols] = true;
            open.push(start);
        }
    }
    constexpr GridPos directions[] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    while (!open.empty()) {
        GridPos current = open.front();
        open.pop();
        if (current.col == kGridCols - 1) return true;
        for (GridPos direction : directions) {
            GridPos next{current.row + direction.row, current.col + direction.col};
            if (!isInBounds(next)) continue;
            int key = next.row * kGridCols + next.col;
            if (visited[key] || !isPassable(terrain[key])) continue;
            visited[key] = true;
            open.push(next);
        }
    }
    return false;
}

// docs/implementation_roadmap.md "Phase 1.5": enemy composition/count/boost
// now comes entirely from the StageDescriptor rather than a raw stage int,
// so a region never needs a parallel copy of this function.
std::vector<Unit> buildEnemies(const GameData& data, const StageDescriptor& stage, ExplorationOutcome outcome,
                               std::uint32_t seed) {
    const std::vector<UnitTemplate>& roster = stage.enemyRoster.empty() ? data.enemyRoster : stage.enemyRoster;
    std::vector<Unit> enemies;
    std::size_t enemyCount = std::min(stage.enemyCountOverride.value_or(roster.size()), roster.size());
    enemyCount -= std::min(enemyCount, static_cast<std::size_t>(std::max(0, outcome.enemiesRemoved)));
    // docs/regions/ashbough_forest.md "折れ木の縄張り": "灰角大猪は右2列の候補
    // から生成する" - narrower than the usual 3-column enemy zone.
    const int enemyZoneMinCol = stage.terrainProfileId == kBrokenwoodTerritoryTerrain
                                    ? kRightZoneMaxCol - 1
                                    : kRightZoneMinCol;
    std::vector<GridPos> spawns = randomSpawnPositions(enemyZoneMinCol, kRightZoneMaxCol, seed ^ kEnemySpawnSalt);
    for (std::size_t i = 0; i < enemyCount; ++i) {
        GridPos pos = i < spawns.size() ? spawns[i] : enemySpawnFallback();
        Unit enemy = instantiateUnit(data, roster[i], Team::Enemy, pos);
        if (stage.boostedFirstEnemy && i == 0) {
            enemy.name = stage.boostedFirstEnemy->displayName;
            enemy.stats.maxHp += stage.boostedFirstEnemy->maxHpBonus;
            enemy.currentHp = enemy.stats.maxHp;
            enemy.stats.defense += stage.boostedFirstEnemy->defenseBonus;
        }
        enemies.push_back(enemy);
    }
    return enemies;
}

// docs/regions/ashbough_forest.md "地点別素材・タイル正式表"/docs/
// mission_objectives.md: a survey-point SecureTile objective's tile must
// never land on an impassable or initial-spawn-occupied position. Chosen
// from the enemy-side 3 columns, deterministically from `seed`.
GridPos chooseSurveyTile(const BattleState& battle, std::uint32_t seed) {
    std::vector<GridPos> candidates;
    for (int row = 0; row < kGridRows; ++row) {
        for (int col = kRightZoneMinCol; col <= kRightZoneMaxCol; ++col) {
            GridPos pos{row, col};
            if (isPassable(battle.terrainAt(pos)) && !battle.unitAt(pos)) candidates.push_back(pos);
        }
    }
    if (candidates.empty()) return GridPos{0, kRightZoneMinCol};
    std::mt19937 rng(seed ^ 0xA5A5A5A5u);
    std::uniform_int_distribution<std::size_t> pick(0, candidates.size() - 1);
    return candidates[pick(rng)];
}

BattleState assembleScenario(const GameData& data, const std::vector<Unit>* survivors, const StageDescriptor& stage,
                             std::uint32_t seed, ExplorationOutcome outcome = {},
                             const std::vector<GridPos>* customPlayerPositions = nullptr,
                             const WeaponOverrides* weaponOverrides = nullptr) {
    std::vector<Unit> units;
    // docs/regions/ashbough_forest.md "薬草の沢"衛生兵ルート: auto-random
    // placement can be confined to fewer than the usual 3 left-edge columns.
    // Only meaningful for a fresh (non-continuation, non-custom-positions)
    // battle - that's the only path that receives a real ExplorationOutcome.
    const int playerZoneMaxCol =
        std::min(kLeftZoneMaxCol, outcome.restrictedAutoSpawnMaxColumn.value_or(kLeftZoneMaxCol));
    std::vector<GridPos> playerSpawns =
        randomSpawnPositions(kLeftZoneMinCol, playerZoneMaxCol, seed ^ kPlayerSpawnSalt);
    std::size_t spawnIndex = 0;
    if (survivors) {
        for (const Unit& survivor : *survivors) {
            if (survivor.team != Team::Player || !survivor.isAlive()) continue;
            Unit unit = survivor;
            unit.position = (customPlayerPositions && spawnIndex < customPlayerPositions->size())
                                ? (*customPlayerPositions)[spawnIndex]
                                : (spawnIndex < playerSpawns.size() ? playerSpawns[spawnIndex]
                                                                    : playerSpawnFallback());
            ++spawnIndex;
            unit.hasActed = false;
            units.push_back(unit);
        }
    } else {
        for (std::size_t i = 0; i < data.playerParty.size(); ++i) {
            GridPos pos = (customPlayerPositions && i < customPlayerPositions->size())
                              ? (*customPlayerPositions)[i]
                              : (i < playerSpawns.size() ? playerSpawns[i] : playerSpawnFallback());
            units.push_back(instantiateUnit(data, data.playerParty[i], Team::Player, pos, weaponOverrides));
        }
    }

    for (Unit& enemy : buildEnemies(data, stage, outcome, seed)) units.push_back(std::move(enemy));
    for (Unit& unit : units) {
        if (unit.team == Team::Player && unit.isAlive())
            unit.currentHp = std::max(1, unit.currentHp - std::max(0, outcome.partyDamage));
    }
    auto terrain = generateFieldTerrain(data.terrainProfile(stage.terrainProfileId), seed);
    for (const Unit& unit : units) {
        const int key = unit.position.row * kGridCols + unit.position.col;
        if (!isPassable(terrain[key])) terrain[key] = TerrainType::Floor;
    }
    BattleState battle(std::move(units), terrain, seed);

    std::vector<GridPos> herbTiles;
    if (stage.terrainProfileId == kHerbwaterHollowTerrain) {
        // docs/regions/ashbough_forest.md "薬草の沢": "盤面中央に浅瀬と薬草地点
        // 2マス" - exactly 2 HerbPatch tiles in the center 4 columns (2-5),
        // chosen after units are placed (like chooseSurveyTile() below) so
        // they never land on an already-occupied tile - the herb zone's
        // edge columns (2 and 5) can otherwise overlap a spawn zone's edge.
        std::vector<GridPos> candidates;
        for (int row = 0; row < kGridRows; ++row) {
            for (int col = 2; col <= 5; ++col) {
                GridPos pos{row, col};
                if (isPassable(battle.terrainAt(pos)) && !battle.unitAt(pos)) candidates.push_back(pos);
            }
        }
        std::mt19937 rng(seed ^ 0x51ED270Bu);
        std::shuffle(candidates.begin(), candidates.end(), rng);
        for (std::size_t i = 0; i < candidates.size() && i < 2; ++i) {
            battle.setTerrain(candidates[i], TerrainType::HerbPatch);
            herbTiles.push_back(candidates[i]);
        }
    }

    if (stage.terrainProfileId == kBrokenwoodTerritoryTerrain) {
        // docs/regions/ashbough_forest.md "折れ木の縄張り"/"ランダム初期配置":
        // 1 fallen log in the center 4 columns (2-5) by default, +1 more
        // (route B) preferring a distinct row from the first. Never on the
        // boss's own row, so a charge can't trivially collide turn one.
        BattleObjectDefinition logDef;
        logDef.definitionId = "fallen_log";
        logDef.kind = BattleObjectKind::Barrier;
        logDef.blocksMovement = true;
        battle.registerObjectDefinition(logDef);

        const Unit* boss = nullptr;
        for (const Unit& unit : battle.units()) {
            if (unit.team == Team::Enemy) { boss = &unit; break; }
        }
        std::vector<GridPos> logCandidates;
        for (int row = 0; row < kGridRows; ++row) {
            if (boss && row == boss->position.row) continue;
            for (int col = 2; col <= 5; ++col) {
                GridPos pos{row, col};
                if (isPassable(battle.terrainAt(pos)) && !battle.unitAt(pos)) logCandidates.push_back(pos);
            }
        }
        std::mt19937 logRng(seed ^ 0x1B873593u);
        std::shuffle(logCandidates.begin(), logCandidates.end(), logRng);

        const int logCount = 1 + std::max(0, outcome.extraBarrierCount);
        std::vector<GridPos> chosenLogs;
        std::vector<int> usedRows;
        for (GridPos candidate : logCandidates) {
            if (static_cast<int>(chosenLogs.size()) >= logCount) break;
            if (std::find(usedRows.begin(), usedRows.end(), candidate.row) != usedRows.end()) continue;
            chosenLogs.push_back(candidate);
            usedRows.push_back(candidate.row);
        }
        for (GridPos candidate : logCandidates) {
            if (static_cast<int>(chosenLogs.size()) >= logCount) break;
            if (std::find(chosenLogs.begin(), chosenLogs.end(), candidate) != chosenLogs.end()) continue;
            chosenLogs.push_back(candidate);
        }
        for (std::size_t i = 0; i < chosenLogs.size(); ++i) {
            battle.placeObject({"fallen_log_" + std::to_string(i + 1), "fallen_log", chosenLogs[i],
                               BattleObjectTeam::Neutral, BattleObjectStateKind::Active, 0, 0});
        }
    }

    if (stage.surveyObjectiveId) {
        // docs/regions/ashbough_forest.md "薬草の沢"'s common secondary is
        // satisfied by ending a turn on EITHER of the 2 HerbPatch tiles -
        // one SecureTile objective per tile, grouped Any under the stage's
        // surveyObjectiveId. A stage with no HerbPatch tiles (e.g. 灰枝の
        // 林縁) falls back to the original single-random-tile pick. Reusing
        // the objective id as the group id is safe: objective ids and group
        // ids are independent namespaces (see validateBattleMission()).
        battle.missionState().groups.push_back({*stage.surveyObjectiveId, ObjectiveGroupRule::Any});
        std::vector<GridPos> surveyTiles = herbTiles.empty()
                                               ? std::vector<GridPos>{chooseSurveyTile(battle, seed)}
                                               : herbTiles;
        for (std::size_t i = 0; i < surveyTiles.size(); ++i) {
            ObjectiveDefinition survey;
            survey.id = surveyTiles.size() == 1 ? *stage.surveyObjectiveId
                                                : *stage.surveyObjectiveId + "_" + std::to_string(i + 1);
            survey.kind = ObjectiveKind::SecureTile;
            survey.primary = false;
            survey.groupId = *stage.surveyObjectiveId;
            survey.target.tile = surveyTiles[i];
            survey.target.securingTeam = Team::Player;
            battle.missionState().definitions.push_back(survey);
            battle.missionState().progress[survey.id] = ObjectiveProgress{survey.id};
        }
    }

    // docs/mission_objectives.md "戦闘開始時の検証": always checked (cheap -
    // battle creation is not a hot path), but never crashes production. No
    // "return to mission start" screen exists yet to redirect to, so a
    // failure is surfaced to stderr as a visible development diagnostic
    // rather than silently starting a broken battle or aborting on a
    // player's machine - see docs/mission_objectives.md status notes for
    // what's deferred.
    for (const std::string& error : validateBattleMission(battle.missionState(), battle)) {
        std::fprintf(stderr, "Invalid battle mission: %s\n", error.c_str());
    }
    return battle;
}
} // namespace

std::array<TerrainType, kGridRows * kGridCols> generateFieldTerrain(const TerrainProfile& profile,
                                                                    std::uint32_t seed) {
    std::array<TerrainType, kGridRows * kGridCols> terrain{};
    std::mt19937 rng(seed ^ (profile.seedSalt * 0x9e3779b9u));
    std::uniform_int_distribution<int> roll(0, 99);
    std::array<int, kGridCols> barriersPerColumn{};
    std::vector<GridPos> candidates;

    for (int row = 0; row < kGridRows; ++row) {
        for (int col = 0; col < kGridCols; ++col) {
            GridPos pos{row, col};
            candidates.push_back(pos);

            const int value = roll(rng);
            TerrainType type = TerrainType::Floor;
            int upperBound = 0;
            for (const WeightedTerrain& entry : profile.weights) {
                upperBound += entry.weight;
                if (value < upperBound) {
                    type = entry.terrain;
                    break;
                }
            }

            // At most one barrier per column prevents a complete vertical
            // wall while allowing every edge-zone tile to be generated.
            if (type == TerrainType::Barrier && profile.maxBarriersPerColumn > 0) {
                if (barriersPerColumn[col] >= profile.maxBarriersPerColumn) type = TerrainType::Rubble;
                else ++barriersPerColumn[col];
            }
            terrain[row * kGridCols + col] = type;
        }
    }

    // Every field always has at least one tile that communicates its identity.
    std::shuffle(candidates.begin(), candidates.end(), rng);
    if (!candidates.empty()) {
        GridPos pos = candidates.front();
        terrain[pos.row * kGridCols + pos.col] = profile.signatureTerrain;
    }
    if (profile.countBounds) {
        const auto [boundedTerrain, minimum, maximum] = *profile.countBounds;
        std::vector<std::size_t> boundedTiles;
        for (std::size_t i = 0; i < terrain.size(); ++i) {
            if (terrain[i] == boundedTerrain) boundedTiles.push_back(i);
        }
        std::shuffle(boundedTiles.begin(), boundedTiles.end(), rng);
        while (static_cast<int>(boundedTiles.size()) > maximum) {
            terrain[boundedTiles.back()] = TerrainType::Floor;
            boundedTiles.pop_back();
        }
        for (GridPos pos : candidates) {
            if (static_cast<int>(boundedTiles.size()) >= minimum) break;
            std::size_t index = static_cast<std::size_t>(pos.row * kGridCols + pos.col);
            if (terrain[index] == TerrainType::Floor) {
                terrain[index] = boundedTerrain;
                boundedTiles.push_back(index);
            }
        }
    }
    if (profile.ensureHorizontalRoute && !hasRouteAcross(terrain)) {
        for (TerrainType& tile : terrain) {
            if (tile == TerrainType::Barrier) tile = TerrainType::Rubble;
        }
    }
    return terrain;
}

Unit instantiateUnit(const GameData& data, const UnitTemplate& unitTemplate, Team team, GridPos pos,
                     const WeaponOverrides* weaponOverrides) {
    Unit unit;
    unit.id = unitTemplate.id;
    unit.name = unitTemplate.name;
    unit.unitClass = unitTemplate.classId;
    unit.team = team;
    unit.stats = data.classDefinition(unitTemplate.classId).baseStats;
    unit.currentHp = unit.stats.maxHp;
    unit.weapon = data.weaponFor(unitTemplate.classId);
    if (weaponOverrides) {
        auto overrideIt = weaponOverrides->find(unitTemplate.id);
        if (overrideIt != weaponOverrides->end()) {
            auto weaponIt = data.weaponsById.find(overrideIt->second);
            if (weaponIt != data.weaponsById.end()) unit.weapon = weaponIt->second;
        }
    }
    unit.stats.move = std::max(1, unit.stats.move + unit.weapon.moveModifier);
    unit.position = pos;
    unit.hasActed = false;
    unit.tilesMovedThisAction = 0;
    return unit;
}

BattleState createFreshBattle(const GameData& data) {
    std::vector<Unit> units;
    units.reserve(data.playerParty.size() + data.enemyRoster.size());

    std::vector<GridPos> playerSpawns = randomSpawnPositions(kLeftZoneMinCol, kLeftZoneMaxCol, kPlayerSpawnSalt);
    std::vector<GridPos> enemySpawns = randomSpawnPositions(kRightZoneMinCol, kRightZoneMaxCol, kEnemySpawnSalt);
    for (std::size_t i = 0; i < data.playerParty.size(); ++i) {
        GridPos pos = i < playerSpawns.size() ? playerSpawns[i] : playerSpawnFallback();
        units.push_back(instantiateUnit(data, data.playerParty[i], Team::Player, pos));
    }
    for (std::size_t i = 0; i < data.enemyRoster.size(); ++i) {
        GridPos pos = i < enemySpawns.size() ? enemySpawns[i] : enemySpawnFallback();
        units.push_back(instantiateUnit(data, data.enemyRoster[i], Team::Enemy, pos));
    }

    return BattleState(std::move(units));
}

BattleState createContinuationBattle(const GameData& data, const std::vector<Unit>& survivingPlayers) {
    std::vector<Unit> units;
    units.reserve(survivingPlayers.size() + data.enemyRoster.size());

    std::vector<GridPos> playerSpawns = randomSpawnPositions(kLeftZoneMinCol, kLeftZoneMaxCol, kPlayerSpawnSalt);
    std::vector<GridPos> enemySpawns = randomSpawnPositions(kRightZoneMinCol, kRightZoneMaxCol, kEnemySpawnSalt);
    std::size_t spawnIndex = 0;
    for (const Unit& survivor : survivingPlayers) {
        if (!survivor.isAlive()) continue;
        Unit unit = survivor;
        unit.position = spawnIndex < playerSpawns.size() ? playerSpawns[spawnIndex] : playerSpawnFallback();
        unit.hasActed = false;
        units.push_back(unit);
        ++spawnIndex;
    }
    for (std::size_t i = 0; i < data.enemyRoster.size(); ++i) {
        GridPos pos = i < enemySpawns.size() ? enemySpawns[i] : enemySpawnFallback();
        units.push_back(instantiateUnit(data, data.enemyRoster[i], Team::Enemy, pos));
    }

    return BattleState(std::move(units));
}

BattleState createScenarioBattle(const GameData& data, const StageDescriptor& stage, std::uint32_t seed,
                                 ExplorationOutcome outcome, const std::vector<GridPos>* customPlayerPositions,
                                 const WeaponOverrides* weaponOverrides) {
    return assembleScenario(data, nullptr, stage, seed, outcome, customPlayerPositions, weaponOverrides);
}

BattleState createScenarioContinuationBattle(const GameData& data,
                                               const std::vector<Unit>& survivingPlayers,
                                               const StageDescriptor& stage,
                                               std::uint32_t seed,
                                               ExplorationOutcome outcome,
                                               const std::vector<GridPos>* customPlayerPositions) {
    return assembleScenario(data, &survivingPlayers, stage, seed, outcome, customPlayerPositions);
}

std::vector<Unit> previewEnemies(const GameData& data, const StageDescriptor& stage, std::uint32_t seed,
                                 ExplorationOutcome outcome) {
    return buildEnemies(data, stage, outcome, seed);
}

} // namespace jf
