#include "jf/battle/BattleFactory.hpp"

namespace jf {

namespace {
// Matches the layout illustrated in the design doc:
//   0    P   .   .   .   .   .   E   E
//   1    P   P   .   .   .   E   .   .
//   2    .   P   .   .   .   .   E   .
const std::vector<GridPos> kPlayerSpawns = {{0, 0}, {1, 0}, {1, 1}, {2, 1}};
const std::vector<GridPos> kEnemySpawns = {{0, 6}, {0, 7}, {1, 5}, {2, 6}};
} // namespace

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

} // namespace jf
