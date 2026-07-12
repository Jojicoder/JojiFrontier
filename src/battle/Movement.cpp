#include "jf/battle/Movement.hpp"

#include <array>
#include <queue>
#include <unordered_map>

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

} // namespace

std::vector<GridPos> computeReachableTiles(const std::vector<Unit>& units, const Unit& mover) {
    std::unordered_map<int, int> bestCost;
    std::queue<GridPos> frontier;

    bestCost[tileKey(mover.position)] = 0;
    frontier.push(mover.position);

    static const std::array<GridPos, 4> kDirections = {
        GridPos{-1, 0}, GridPos{1, 0}, GridPos{0, -1}, GridPos{0, 1}
    };

    while (!frontier.empty()) {
        GridPos current = frontier.front();
        frontier.pop();
        int currentCost = bestCost[tileKey(current)];
        if (currentCost >= mover.stats.move) continue;

        for (const auto& dir : kDirections) {
            GridPos next{current.row + dir.row, current.col + dir.col};
            if (!isInBounds(next)) continue;
            if (unitAtTile(units, next, &mover) != nullptr) continue;

            int nextCost = currentCost + 1;
            auto it = bestCost.find(tileKey(next));
            if (it != bestCost.end() && it->second <= nextCost) continue;

            bestCost[tileKey(next)] = nextCost;
            frontier.push(next);
        }
    }

    std::vector<GridPos> reachable;
    reachable.reserve(bestCost.size());
    for (const auto& [key, cost] : bestCost) {
        reachable.push_back(GridPos{key / kGridCols, key % kGridCols});
    }
    return reachable;
}

std::vector<GridPos> computeTargetableTiles(const std::vector<Unit>& units,
                                             const Unit& attacker,
                                             GridPos origin) {
    std::vector<GridPos> targets;
    for (const auto& u : units) {
        if (!u.isAlive() || u.team == attacker.team) continue;
        int dist = manhattanDistance(origin, u.position);
        if (dist >= attacker.weapon.minRange && dist <= attacker.weapon.maxRange) {
            targets.push_back(u.position);
        }
    }
    return targets;
}

} // namespace jf
