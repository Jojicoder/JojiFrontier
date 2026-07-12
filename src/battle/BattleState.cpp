#include "jf/battle/BattleState.hpp"

#include <algorithm>

namespace jf {

BattleState::BattleState(std::vector<Unit> units) : units_(std::move(units)) {}

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
    if (unitAt(destination) != nullptr) return false;
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
        if (u.team == Team::Player) u.hasActed = false;
    }
}

void BattleState::beginEnemyPhase() {
    phase_ = Phase::EnemyPhase;
    for (auto& u : units_) {
        if (u.team == Team::Enemy) u.hasActed = false;
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
