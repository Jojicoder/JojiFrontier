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

// Same reachability check as hasRouteAcross(), but against an already-
// assembled BattleState plus a set of not-yet-placed Object Tiles (so
// placeRandomObjects() below can ask "would placing a blocksMovement Object
// here still leave a route?" before actually placing it). `battle.objects()`
// itself is irrelevant here on purpose - a rule only needs to check against
// Objects its own retry loop is about to add, since nothing else calls
// placeRandomObjects() more than once per assembleScenario().
bool hasRouteAcrossWithBlockedTiles(const BattleState& battle, const std::vector<GridPos>& blocked) {
    auto isBlocked = [&](GridPos pos) {
        return std::find(blocked.begin(), blocked.end(), pos) != blocked.end();
    };
    std::array<bool, kGridRows * kGridCols> visited{};
    std::queue<GridPos> open;
    for (int row = 0; row < kGridRows; ++row) {
        GridPos start{row, 0};
        if (isPassable(battle.terrainAt(start)) && !isBlocked(start)) {
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
            if (visited[key] || !isPassable(battle.terrainAt(next)) || isBlocked(next)) continue;
            visited[key] = true;
            open.push(next);
        }
    }
    return false;
}

// docs/battle_objects.md "ランダム生成": StageDescriptor::objectPlacementRules
// generalizes what used to be Brokenwood Territory's fallen_log-only ad-hoc
// block, so a future region declares Object placement as data instead of a
// new `if (stage.terrainProfileId == ...)` branch here. Terrain (step 1) and
// Unit spawns (step 2) are already fixed inputs by the time this runs, so
// this only covers steps 3/5/part of 6: register each rule's Definition,
// pick candidate Tiles in its zone (excluding the first Enemy's row when
// asked), prefer distinct rows first the way the original fallen_log logic
// did, and skip any candidate that would leave zero route across the board.
// Step 7's "上限到達後は安全な固定配置へ戻す" fallback isn't implemented - a
// candidate that would sever the only route is simply skipped rather than
// retried against a region-specific fixed Tile (no shipped region needs
// more blocksMovement Objects than comfortably fit its zone yet).
void placeRandomObjects(BattleState& battle, const StageDescriptor& stage, const ExplorationOutcome& outcome,
                        std::uint32_t seed) {
    for (std::size_t ruleIndex = 0; ruleIndex < stage.objectPlacementRules.size(); ++ruleIndex) {
        const StageDescriptor::ObjectPlacementRule& rule = stage.objectPlacementRules[ruleIndex];
        battle.registerObjectDefinition(rule.definition);

        int avoidedRow = -1;
        if (rule.avoidFirstEnemyRow) {
            for (const Unit& unit : battle.units()) {
                if (unit.team == Team::Enemy) { avoidedRow = unit.position.row; break; }
            }
        }

        std::vector<GridPos> candidates;
        for (int row = 0; row < kGridRows; ++row) {
            if (row == avoidedRow) continue;
            for (int col = rule.zoneMinCol; col <= rule.zoneMaxCol; ++col) {
                GridPos pos{row, col};
                if (isPassable(battle.terrainAt(pos)) && !battle.unitAt(pos) && !battle.objectAt(pos))
                    candidates.push_back(pos);
            }
        }
        std::mt19937 rng(seed ^ (0x1B873593u + static_cast<std::uint32_t>(ruleIndex)));
        std::shuffle(candidates.begin(), candidates.end(), rng);

        const int targetCount =
            rule.count + (rule.scalesWithExtraBarrierOutcome ? std::max(0, outcome.extraBarrierCount) : 0);

        std::vector<GridPos> chosen;
        std::vector<int> usedRows;
        auto tryPlace = [&](GridPos pos) {
            if (static_cast<int>(chosen.size()) >= targetCount) return;
            if (rule.definition.blocksMovement && !hasRouteAcrossWithBlockedTiles(battle, [&] {
                    std::vector<GridPos> blocked = chosen;
                    blocked.push_back(pos);
                    return blocked;
                }())) {
                return;
            }
            chosen.push_back(pos);
            usedRows.push_back(pos.row);
        };
        for (GridPos candidate : candidates) {
            if (static_cast<int>(chosen.size()) >= targetCount) break;
            if (std::find(usedRows.begin(), usedRows.end(), candidate.row) != usedRows.end()) continue;
            tryPlace(candidate);
        }
        for (GridPos candidate : candidates) {
            if (static_cast<int>(chosen.size()) >= targetCount) break;
            if (std::find(chosen.begin(), chosen.end(), candidate) != chosen.end()) continue;
            tryPlace(candidate);
        }

        for (std::size_t i = 0; i < chosen.size(); ++i) {
            battle.placeObject({rule.idPrefix + "_" + std::to_string(i + 1), rule.definition.definitionId,
                               chosen[i], BattleObjectTeam::Neutral, BattleObjectStateKind::Active,
                               rule.definition.maxDurability, 0});
        }
    }
}

// docs/implementation_roadmap.md "Phase 1.5": enemy composition/count/boost
// now comes entirely from the StageDescriptor rather than a raw stage int,
// so a region never needs a parallel copy of this function.
std::vector<Unit> buildEnemies(const GameData& data, const StageDescriptor& stage, ExplorationOutcome outcome,
                               std::uint32_t seed, int livingPlayerCount) {
    const std::vector<UnitTemplate>& roster = stage.enemyRoster.empty() ? data.enemyRoster : stage.enemyRoster;
    std::vector<Unit> enemies;
    std::size_t enemyCount = std::min(stage.enemyCountOverride.value_or(roster.size()), roster.size());
    enemyCount -= std::min(enemyCount, static_cast<std::size_t>(std::max(0, outcome.enemiesRemoved)));
    // docs/regions/ashbough_forest.md "折れ木の縄張り": "灰角大猪は右2列の候補
    // から生成する" - narrower than the usual 3-column enemy zone
    // (docs/implementation_roadmap.md M1-E: driven by `stage.enemyZoneWidth`,
    // data/regions.json, rather than a name-based branch here).
    const int enemyZoneMinCol = kRightZoneMaxCol - stage.enemyZoneWidth.value_or(kSpawnZoneWidth) + 1;
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
    // docs/regions/ashbough_forest.md "折れ木の縄張り": one extra
    // reinforcement if the incoming party is understaffed - see
    // StageDescriptor::understaffedReinforcement's comment.
    if (stage.understaffedReinforcement && livingPlayerCount < stage.understaffedThreshold) {
        GridPos pos = enemyCount < spawns.size() ? spawns[enemyCount] : enemySpawnFallback();
        enemies.push_back(instantiateUnit(data, *stage.understaffedReinforcement, Team::Enemy, pos));
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

    int livingPlayerCount = 0;
    for (const Unit& unit : units) {
        if (unit.team == Team::Player && unit.isAlive()) ++livingPlayerCount;
    }
    for (Unit& enemy : buildEnemies(data, stage, outcome, seed, livingPlayerCount)) units.push_back(std::move(enemy));
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
    if (stage.herbPatchGeneration) {
        // docs/regions/ashbough_forest.md "薬草の沢": "盤面中央に浅瀬と薬草地点
        // 2マス" - chosen after units are placed (like chooseSurveyTile()
        // below) so they never land on an already-occupied tile - the herb
        // zone's edge columns can otherwise overlap a spawn zone's edge.
        const auto& rule = *stage.herbPatchGeneration;
        std::vector<GridPos> candidates;
        for (int row = 0; row < kGridRows; ++row) {
            for (int col = rule.zoneMinCol; col <= rule.zoneMaxCol; ++col) {
                GridPos pos{row, col};
                if (isPassable(battle.terrainAt(pos)) && !battle.unitAt(pos)) candidates.push_back(pos);
            }
        }
        std::mt19937 rng(seed ^ 0x51ED270Bu);
        std::shuffle(candidates.begin(), candidates.end(), rng);
        for (std::size_t i = 0; i < candidates.size() && static_cast<int>(i) < rule.count; ++i) {
            battle.setTerrain(candidates[i], TerrainType::HerbPatch);
            herbTiles.push_back(candidates[i]);
        }
    }

    placeRandomObjects(battle, stage, outcome, seed);

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

    if (outcome.enableReinforcementWave && stage.timedReinforcement) {
        const auto& definition = *stage.timedReinforcement;
        ReinforcementWave wave;
        wave.id = definition.id;
        wave.spawnRound = definition.spawnRound;
        wave.spawnPhase = definition.spawnPhase;
        wave.announceRoundsBefore = definition.announceRoundsBefore;
        wave.requiredForElimination = definition.requiredForElimination;
        wave.orderedSpawnCandidates = definition.orderedSpawnCandidates;
        for (const UnitTemplate& unitTemplate : definition.units) {
            wave.units.push_back({instantiateUnit(data, unitTemplate, Team::Enemy, {0, 0})});
        }
        if (!battle.addReinforcementWave(std::move(wave)))
            std::fprintf(stderr, "Invalid reinforcement wave for stage: %s\n", stage.id.c_str());
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
                                 ExplorationOutcome outcome, int livingPlayerCount) {
    return buildEnemies(data, stage, outcome, seed, livingPlayerCount);
}

} // namespace jf
