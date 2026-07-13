#include "jf/battle/Movement.hpp"

#include <array>
#include <cstdlib>
#include <queue>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace jf {

namespace {

int tileKey(GridPos pos) { return pos.row * kGridCols + pos.col; }

const Unit* unitAtTile(const std::vector<Unit>& units, GridPos pos, const Unit* ignore) {
    for (const auto& u : units) {
        if (&u == ignore) continue;
        if (u.isAlive() && u.position == pos) return &u;
    }
    return nullptr;
}

bool isStoppedByZoneOfControl(const std::vector<Unit>& units, const Unit& mover, GridPos pos) {
    for (const Unit& unit : units) {
        if (!unit.isAlive() || unit.team == mover.team || !hasZoneOfControl(unit.unitClass)) continue;
        if (manhattanDistance(unit.position, pos) == 1) return true;
    }
    return false;
}

} // namespace

namespace {

std::vector<GridPos> computeReachableTilesImpl(const std::vector<Unit>& units, const Unit& mover,
                                                const std::function<TerrainType(GridPos)>& terrainAt) {
    std::unordered_map<int, int> bestCost;
    using Node = std::pair<int, GridPos>;
    auto greaterCost = [](const Node& a, const Node& b) { return a.first > b.first; };
    std::priority_queue<Node, std::vector<Node>, decltype(greaterCost)> frontier(greaterCost);

    bestCost[tileKey(mover.position)] = 0;
    frontier.push({0, mover.position});

    static const std::array<GridPos, 4> kDirections = {
        GridPos{-1, 0}, GridPos{1, 0}, GridPos{0, -1}, GridPos{0, 1}
    };

    while (!frontier.empty()) {
        auto [queuedCost, current] = frontier.top();
        frontier.pop();
        int currentCost = bestCost[tileKey(current)];
        if (queuedCost != currentCost) continue;
        if (currentCost >= mover.stats.move) continue;
        if (current != mover.position && isStoppedByZoneOfControl(units, mover, current)) continue;

        for (const auto& dir : kDirections) {
            GridPos next{current.row + dir.row, current.col + dir.col};
            if (!isInBounds(next)) continue;
            TerrainType terrain = terrainAt(next);
            if (!isPassable(terrain)) continue;
            const Unit* occupant = unitAtTile(units, next, &mover);
            // Allies can be crossed but never used as a destination. Enemies
            // block path expansion entirely. Future Zone of Control belongs
            // here as an additional expansion rule, not an occupancy rule.
            if (occupant && occupant->team != mover.team) continue;

            int stepCost = ignoresAshPenalty(mover.unitClass) && terrain == TerrainType::Ash
                               ? 1
                               : movementCost(terrain);
            int nextCost = currentCost + stepCost;
            if (nextCost > mover.stats.move) continue;
            auto it = bestCost.find(tileKey(next));
            if (it != bestCost.end() && it->second <= nextCost) continue;

            bestCost[tileKey(next)] = nextCost;
            frontier.push({nextCost, next});
        }
    }

    std::vector<GridPos> reachable;
    reachable.reserve(bestCost.size());
    for (const auto& [key, cost] : bestCost) {
        GridPos pos{key / kGridCols, key % kGridCols};
        if (unitAtTile(units, pos, &mover) != nullptr) continue;
        reachable.push_back(pos);
    }
    return reachable;
}

} // namespace

std::vector<GridPos> computeReachableTiles(const std::vector<Unit>& units, const Unit& mover) {
    return computeReachableTilesImpl(units, mover, [](GridPos) { return TerrainType::Floor; });
}

std::vector<GridPos> computeReachableTiles(const BattleState& battle, const Unit& mover) {
    return computeReachableTilesImpl(battle.units(), mover,
                                     [&](GridPos pos) { return battle.terrainAt(pos); });
}

std::vector<GridPos> computeTargetableTiles(const std::vector<Unit>& units,
                                             const Unit& attacker,
                                             GridPos origin) {
    std::vector<GridPos> targets;
    for (const auto& u : units) {
        if (!u.isAlive() || u.team == attacker.team) continue;
        int dist = manhattanDistance(origin, u.position);
        if (dist >= attacker.minimumAttackRange() && dist <= attacker.weapon.maxRange) {
            targets.push_back(u.position);
        }
    }
    return targets;
}

std::vector<GridPos> computeAttackRangeTiles(const Unit& attacker, const std::vector<GridPos>& fromTiles) {
    std::unordered_set<int> seen;
    std::vector<GridPos> result;

    for (const GridPos& from : fromTiles) {
        for (int dRow = -attacker.weapon.maxRange; dRow <= attacker.weapon.maxRange; ++dRow) {
            int remaining = attacker.weapon.maxRange - std::abs(dRow);
            for (int dCol = -remaining; dCol <= remaining; ++dCol) {
                GridPos candidate{from.row + dRow, from.col + dCol};
                if (!isInBounds(candidate)) continue;

                int dist = manhattanDistance(from, candidate);
                if (dist < attacker.minimumAttackRange() || dist > attacker.weapon.maxRange) continue;

                int key = tileKey(candidate);
                if (seen.insert(key).second) result.push_back(candidate);
            }
        }
    }
    return result;
}

} // namespace jf
