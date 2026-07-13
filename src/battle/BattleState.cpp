#include "jf/battle/BattleState.hpp"

#include <algorithm>

namespace jf {

BattleState::BattleState(std::vector<Unit> units,
                         std::array<TerrainType, kGridRows * kGridCols> terrain)
    : units_(std::move(units)), terrain_(terrain) {}

TerrainType BattleState::terrainAt(GridPos pos) const {
    if (!isInBounds(pos)) return TerrainType::Barrier;
    return terrain_[pos.row * kGridCols + pos.col];
}

void BattleState::setTerrain(GridPos pos, TerrainType terrain) {
    if (isInBounds(pos)) terrain_[pos.row * kGridCols + pos.col] = terrain;
}

int BattleState::combatDefenseBonus(const Unit& defender, const Unit& attacker) const {
    int bonus = defenseBonus(terrainAt(defender.position));
    for (const Unit& ally : units_) {
        if (!ally.isAlive() || ally.team != defender.team || !providesFormationBonus(ally.unitClass)) continue;
        if (&ally != &defender && manhattanDistance(ally.position, defender.position) == 1) {
            ++bonus;
            break;
        }
    }
    if (hasBrace(defender.unitClass) && attacker.tilesMovedThisAction >= 2) bonus += 2;
    return bonus;
}

Unit* BattleState::unitAt(GridPos pos) {
    for (auto& u : units_) {
        if (u.isAlive() && u.position == pos) return &u;
    }
    return nullptr;
}

const Unit* BattleState::unitAt(GridPos pos) const {
    for (const auto& u : units_) {
        if (u.isAlive() && u.position == pos) return &u;
    }
    return nullptr;
}

Unit* BattleState::findUnit(const std::string& id) {
    auto it = std::find_if(units_.begin(), units_.end(),
                            [&](const Unit& u) { return u.id == id; });
    return it == units_.end() ? nullptr : &(*it);
}

bool BattleState::moveUnit(Unit& unit, GridPos destination) {
    if (!isInBounds(destination)) return false;
    if (!isPassable(terrainAt(destination))) return false;
    Unit* occupant = unitAt(destination);
    if (occupant != nullptr && occupant != &unit) return false;
    unit.tilesMovedThisAction = manhattanDistance(unit.position, destination);
    unit.position = destination;
    return true;
}

bool BattleState::isTeamDone(Team team) const {
    for (const auto& u : units_) {
        if (u.team == team && u.isAlive() && !u.hasActed) return false;
    }
    return true;
}

void BattleState::beginPlayerPhase() {
    phase_ = Phase::PlayerPhase;
    for (auto& u : units_) {
        if (u.team == Team::Player) {
            u.hasActed = false;
            u.tilesMovedThisAction = 0;
        }
    }
}

void BattleState::beginEnemyPhase() {
    phase_ = Phase::EnemyPhase;
    for (auto& u : units_) {
        if (u.team == Team::Enemy) {
            u.hasActed = false;
            u.tilesMovedThisAction = 0;
        }
    }
}

bool BattleState::allEnemiesDefeated() const {
    return std::none_of(units_.begin(), units_.end(),
                         [](const Unit& u) { return u.team == Team::Enemy && u.isAlive(); });
}

bool BattleState::allPlayersDefeated() const {
    return std::none_of(units_.begin(), units_.end(),
                         [](const Unit& u) { return u.team == Team::Player && u.isAlive(); });
}

} // namespace jf
