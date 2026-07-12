#include "jf/battle/EnemyAI.hpp"

#include <climits>

#include "jf/battle/CombatResolver.hpp"
#include "jf/battle/Movement.hpp"

namespace jf {

namespace {

Unit* findNearestPlayer(BattleState& battle, const Unit& enemy) {
    Unit* nearest = nullptr;
    int bestDist = INT_MAX;
    for (auto& u : battle.units()) {
        if (u.team != Team::Player || !u.isAlive()) continue;
        int dist = manhattanDistance(enemy.position, u.position);
        if (dist < bestDist) {
            bestDist = dist;
            nearest = &u;
        }
    }
    return nearest;
}

// Attacks the preferred target if in range, otherwise any in-range target.
bool attackIfPossible(BattleState& battle, Unit& enemy, Unit* preferredTarget) {
    auto inRange = [&](const Unit& target) {
        int dist = manhattanDistance(enemy.position, target.position);
        return dist >= enemy.weapon.minRange && dist <= enemy.weapon.maxRange;
    };

    if (preferredTarget && preferredTarget->isAlive() && inRange(*preferredTarget)) {
        resolveAttack(enemy, *preferredTarget);
        return true;
    }
    for (auto& u : battle.units()) {
        if (u.team == Team::Player && u.isAlive() && inRange(u)) {
            resolveAttack(enemy, u);
            return true;
        }
    }
    return false;
}

} // namespace

void takeEnemyTurn(BattleState& battle, Unit& enemy) {
    if (!enemy.isAlive() || enemy.hasActed) return;

    Unit* target = findNearestPlayer(battle, enemy);
    if (!target) {
        battle.markActed(enemy);
        return;
    }

    if (attackIfPossible(battle, enemy, target)) {
        battle.markActed(enemy);
        return;
    }

    std::vector<GridPos> reachable = computeReachableTiles(battle.units(), enemy);
    GridPos bestTile = enemy.position;
    int bestDist = manhattanDistance(enemy.position, target->position);
    for (const GridPos& tile : reachable) {
        int dist = manhattanDistance(tile, target->position);
        if (dist < bestDist) {
            bestDist = dist;
            bestTile = tile;
        }
    }
    battle.moveUnit(enemy, bestTile);

    attackIfPossible(battle, enemy, target);
    battle.markActed(enemy);
}

} // namespace jf
