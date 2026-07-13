#include "jf/battle/BattleFactory.hpp"

#include <array>
#include <algorithm>
#include <random>
#include <queue>

namespace jf {

namespace {
// Matches the layout illustrated in the design doc:
//   0    P   .   .   .   .   .   E   E
//   1    P   P   .   .   .   E   .   .
//   2    .   P   .   .   .   .   E   .
const std::vector<GridPos> kPlayerSpawns = {{0, 0}, {1, 0}, {1, 1}, {2, 1}};
const std::vector<GridPos> kEnemySpawns = {{0, 6}, {0, 7}, {1, 5}, {2, 6}};

bool isSpawnTile(GridPos pos) {
    return std::find(kPlayerSpawns.begin(), kPlayerSpawns.end(), pos) != kPlayerSpawns.end() ||
           std::find(kEnemySpawns.begin(), kEnemySpawns.end(), pos) != kEnemySpawns.end();
}

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

std::vector<Unit> buildEnemies(const GameData& data, int stage, ExplorationOutcome outcome) {
    std::vector<Unit> enemies;
    std::size_t enemyCount = stage == 0 ? 3 : data.enemyRoster.size();
    enemyCount -= std::min(enemyCount, static_cast<std::size_t>(std::max(0, outcome.enemiesRemoved)));
    for (std::size_t i = 0; i < enemyCount; ++i) {
        Unit enemy = instantiateUnit(data, data.enemyRoster[i], Team::Enemy, kEnemySpawns[i]);
        if (stage == 2 && i == 0) {
            enemy.name = "Former Captain";
            enemy.stats.maxHp += 10;
            enemy.currentHp = enemy.stats.maxHp;
            enemy.stats.defense += 2;
        }
        enemies.push_back(enemy);
    }
    return enemies;
}

BattleState assembleScenario(const GameData& data, const std::vector<Unit>* survivors, int stage,
                             std::uint32_t seed, ExplorationOutcome outcome = {},
                             const std::vector<GridPos>* customPlayerPositions = nullptr) {
    std::vector<Unit> units;
    std::size_t spawnIndex = 0;
    if (survivors) {
        for (const Unit& survivor : *survivors) {
            if (survivor.team != Team::Player) continue;
            Unit unit = survivor;
            unit.position = kPlayerSpawns[spawnIndex++];
            unit.hasActed = false;
            units.push_back(unit);
        }
    } else {
        for (std::size_t i = 0; i < data.playerParty.size(); ++i) {
            GridPos pos = (customPlayerPositions && i < customPlayerPositions->size())
                              ? (*customPlayerPositions)[i]
                              : kPlayerSpawns[i];
            units.push_back(instantiateUnit(data, data.playerParty[i], Team::Player, pos));
        }
    }

    for (Unit& enemy : buildEnemies(data, stage, outcome)) units.push_back(std::move(enemy));
    for (Unit& unit : units) {
        if (unit.team == Team::Player && unit.isAlive())
            unit.currentHp = std::max(1, unit.currentHp - std::max(0, outcome.partyDamage));
    }
    return BattleState(std::move(units), generateFieldTerrain(fieldTypeForStage(stage), seed));
}
} // namespace

FieldType fieldTypeForStage(int stage) {
    if (stage <= 0) return FieldType::CinderwatchOutpost;
    if (stage == 1) return FieldType::AshRoad;
    return FieldType::SignalTower;
}

std::array<TerrainType, kGridRows * kGridCols> generateFieldTerrain(FieldType field,
                                                                    std::uint32_t seed) {
    std::array<TerrainType, kGridRows * kGridCols> terrain{};
    std::mt19937 rng(seed ^ (static_cast<std::uint32_t>(field) * 0x9e3779b9u));
    std::uniform_int_distribution<int> roll(0, 99);
    std::array<int, kGridCols> barriersPerColumn{};
    std::vector<GridPos> candidates;

    for (int row = 0; row < kGridRows; ++row) {
        for (int col = 0; col < kGridCols; ++col) {
            GridPos pos{row, col};
            if (isSpawnTile(pos)) continue;
            candidates.push_back(pos);

            int value = roll(rng);
            TerrainType type = TerrainType::Floor;
            if (field == FieldType::CinderwatchOutpost) {
                if (value < 24) type = TerrainType::Rubble;
                else if (value < 34) type = TerrainType::WatchPost;
                else if (value < 42) type = TerrainType::Ash;
                else if (value < 48) type = TerrainType::Barrier;
            } else if (field == FieldType::AshRoad) {
                if (value < 38) type = TerrainType::Ash;
                else if (value < 55) type = TerrainType::Rubble;
                else if (value < 62) type = TerrainType::WatchPost;
                else if (value < 68) type = TerrainType::Barrier;
            } else {
                if (value < 23) type = TerrainType::WatchPost;
                else if (value < 43) type = TerrainType::Rubble;
                else if (value < 54) type = TerrainType::Ash;
                else if (value < 61) type = TerrainType::Barrier;
            }

            if (type == TerrainType::Barrier) {
                if (col < 2 || col > 5 || barriersPerColumn[col] >= 1) type = TerrainType::Rubble;
                else ++barriersPerColumn[col];
            }
            terrain[row * kGridCols + col] = type;
        }
    }

    // Every field always has at least one tile that communicates its identity.
    std::shuffle(candidates.begin(), candidates.end(), rng);
    if (!candidates.empty()) {
        TerrainType signature = field == FieldType::AshRoad ? TerrainType::Ash
                               : field == FieldType::SignalTower ? TerrainType::WatchPost
                                                                 : TerrainType::Rubble;
        GridPos pos = candidates.front();
        terrain[pos.row * kGridCols + pos.col] = signature;
    }
    if (!hasRouteAcross(terrain)) {
        for (TerrainType& tile : terrain) {
            if (tile == TerrainType::Barrier) tile = TerrainType::Rubble;
        }
    }
    return terrain;
}

Unit instantiateUnit(const GameData& data, const UnitTemplate& unitTemplate, Team team, GridPos pos) {
    Unit unit;
    unit.id = unitTemplate.id;
    unit.name = unitTemplate.name;
    unit.unitClass = unitTemplate.classId;
    unit.team = team;
    unit.stats = data.classDefinition(unitTemplate.classId).baseStats;
    unit.currentHp = unit.stats.maxHp;
    unit.weapon = data.weaponFor(unitTemplate.classId);
    unit.position = pos;
    unit.hasActed = false;
    unit.tilesMovedThisAction = 0;
    return unit;
}

BattleState createFreshBattle(const GameData& data) {
    std::vector<Unit> units;
    units.reserve(data.playerParty.size() + data.enemyRoster.size());

    for (std::size_t i = 0; i < data.playerParty.size(); ++i) {
        GridPos pos = i < kPlayerSpawns.size() ? kPlayerSpawns[i] : GridPos{0, 0};
        units.push_back(instantiateUnit(data, data.playerParty[i], Team::Player, pos));
    }
    for (std::size_t i = 0; i < data.enemyRoster.size(); ++i) {
        GridPos pos = i < kEnemySpawns.size() ? kEnemySpawns[i] : GridPos{0, 7};
        units.push_back(instantiateUnit(data, data.enemyRoster[i], Team::Enemy, pos));
    }

    return BattleState(std::move(units));
}

BattleState createContinuationBattle(const GameData& data, const std::vector<Unit>& survivingPlayers) {
    std::vector<Unit> units;
    units.reserve(survivingPlayers.size() + data.enemyRoster.size());

    std::size_t spawnIndex = 0;
    for (const Unit& survivor : survivingPlayers) {
        if (!survivor.isAlive()) continue;
        Unit unit = survivor;
        unit.position = spawnIndex < kPlayerSpawns.size() ? kPlayerSpawns[spawnIndex] : GridPos{0, 0};
        unit.hasActed = false;
        units.push_back(unit);
        ++spawnIndex;
    }
    for (std::size_t i = 0; i < data.enemyRoster.size(); ++i) {
        GridPos pos = i < kEnemySpawns.size() ? kEnemySpawns[i] : GridPos{0, 7};
        units.push_back(instantiateUnit(data, data.enemyRoster[i], Team::Enemy, pos));
    }

    return BattleState(std::move(units));
}

BattleState createScenarioBattle(const GameData& data, int stage, std::uint32_t seed,
                                 ExplorationOutcome outcome, const std::vector<GridPos>* customPlayerPositions) {
    return assembleScenario(data, nullptr, stage, seed, outcome, customPlayerPositions);
}

BattleState createScenarioContinuationBattle(const GameData& data,
                                               const std::vector<Unit>& survivingPlayers,
                                               int stage,
                                               std::uint32_t seed) {
    return assembleScenario(data, &survivingPlayers, stage, seed);
}

std::vector<Unit> previewEnemies(const GameData& data, int stage, ExplorationOutcome outcome) {
    return buildEnemies(data, stage, outcome);
}

} // namespace jf
