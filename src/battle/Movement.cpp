#include "jf/battle/Movement.hpp"

#include <algorithm>
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
        // isPresent() (not isAlive()): a retreated unit (docs/
        // enemy_ai_rules.md) no longer occupies its tile - it left the
        // field, even though isAlive() stays true (HP unaffected).
        if (u.isPresent() && u.position == pos) return &u;
    }
    return nullptr;
}

bool isStoppedByZoneOfControl(const std::vector<Unit>& units, const Unit& mover, GridPos pos) {
    for (const Unit& unit : units) {
        if (!unit.isPresent() || unit.team == mover.team || !hasZoneOfControl(unit.unitClass)) continue;
        // 古参守備兵`extended_lockdown` (docs/initial_skill_effects.md):
        // extends this unit's own ZoC from range 1 to range 2.
        const int range = unit.zocRangeExtended ? 2 : 1;
        if (manhattanDistance(unit.position, pos) <= range) return true;
    }
    return false;
}

} // namespace

namespace {

std::vector<GridPos> computeReachableTilesImpl(const std::vector<Unit>& units, const Unit& mover,
                                                const std::function<TerrainType(GridPos)>& terrainAt,
                                                const std::function<bool(GridPos)>& blocksMovementAt = {},
                                                const std::function<bool(GridPos)>& blocksStoppingAt = {},
                                                const std::function<bool(GridPos)>& costOverrideAt = {},
                                                std::unordered_map<int, int>* parentOut = nullptr) {
    std::unordered_map<int, int> bestCost;
    std::unordered_map<int, int> parent;
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
        if (currentCost >= mover.effectiveMove()) continue;
        if (current != mover.position && isStoppedByZoneOfControl(units, mover, current)) continue;

        for (const auto& dir : kDirections) {
            GridPos next{current.row + dir.row, current.col + dir.col};
            if (!isInBounds(next)) continue;
            TerrainType terrain = terrainAt(next);
            if (!isPassable(terrain)) continue;
            // docs/battle_objects.md "占有と通行": blocksMovement objects
            // block expansion entirely, like impassable terrain; a
            // blocksStopping-only object (e.g. an Exit) can still be passed
            // through, it's just excluded from the final stoppable set below.
            if (blocksMovementAt && blocksMovementAt(next)) continue;
            const Unit* occupant = unitAtTile(units, next, &mover);
            // Allies can be crossed but never used as a destination. Enemies
            // block path expansion entirely. Future Zone of Control belongs
            // here as an additional expansion rule, not an occupancy rule.
            if (occupant && occupant->team != mover.team) continue;

            // 辺境斥候`trailblaze`(道拓き): Ash/Shallows tiles the caster
            // passed through this Player Phase cost every ally 1 to cross,
            // regardless of the tile's normal cost or the mover's own class.
            int stepCost = (costOverrideAt && costOverrideAt(next)) ||
                                   (ignoresAshPenalty(mover.unitClass) && terrain == TerrainType::Ash)
                               ? 1
                               : movementCost(terrain);
            int nextCost = currentCost + stepCost;
            if (nextCost > mover.effectiveMove()) continue;
            auto it = bestCost.find(tileKey(next));
            if (it != bestCost.end() && it->second <= nextCost) continue;

            bestCost[tileKey(next)] = nextCost;
            parent[tileKey(next)] = tileKey(current);
            frontier.push({nextCost, next});
        }
    }

    if (parentOut) *parentOut = std::move(parent);

    std::vector<GridPos> reachable;
    reachable.reserve(bestCost.size());
    for (const auto& [key, cost] : bestCost) {
        GridPos pos{key / kGridCols, key % kGridCols};
        if (unitAtTile(units, pos, &mover) != nullptr) continue;
        // A blocksStopping-only object (blocksMovement objects were already
        // excluded from expansion above) can be passed through but never
        // used as a stop/destination tile.
        if (blocksStoppingAt && blocksStoppingAt(pos)) continue;
        reachable.push_back(pos);
    }
    return reachable;
}

} // namespace

std::vector<GridPos> computeReachableTiles(const std::vector<Unit>& units, const Unit& mover) {
    return computeReachableTilesImpl(units, mover, [](GridPos) { return TerrainType::Floor; });
}

std::vector<GridPos> computeReachableTiles(const BattleState& battle, const Unit& mover) {
    return computeReachableTilesImpl(
        battle.units(), mover, [&](GridPos pos) { return battle.terrainAt(pos); },
        [&](GridPos pos) { return battle.objectBlocksMovementAt(pos); },
        [&](GridPos pos) { return battle.objectBlocksStoppingAt(pos); },
        [&](GridPos pos) { return battle.isTrailblazed(pos); });
}

// 辺境斥候`trailblaze`(道拓き) (docs/initial_skill_effects.md "仮移動で通過し
// た灰地・浅瀬"): reconstructs the exact tile-by-tile shortest path `mover`
// takes to `destination`, using the same parent-tracking Dijkstra as
// computeReachableTilesImpl() above (identical terrain/occupancy/Battle
// Object/ZoC rules - the two must always agree on what's reachable).
// Returns tiles strictly between the origin and destination, inclusive of
// destination but excluding the origin itself (you don't "pass through"
// your own starting tile) - empty if destination equals the origin, or if
// it isn't actually reachable (shouldn't happen for a tile the caller
// already validated via computeReachableTiles()).
std::vector<GridPos> computeMovementPath(const BattleState& battle, const Unit& mover, GridPos destination) {
    std::vector<GridPos> path;
    const int originKey = tileKey(mover.position);
    const int destKey = tileKey(destination);
    if (destKey == originKey) return path;

    std::unordered_map<int, int> parent;
    computeReachableTilesImpl(
        battle.units(), mover, [&](GridPos pos) { return battle.terrainAt(pos); },
        [&](GridPos pos) { return battle.objectBlocksMovementAt(pos); },
        [&](GridPos pos) { return battle.objectBlocksStoppingAt(pos); },
        [&](GridPos pos) { return battle.isTrailblazed(pos); }, &parent);

    int key = destKey;
    while (key != originKey) {
        path.push_back(GridPos{key / kGridCols, key % kGridCols});
        auto it = parent.find(key);
        if (it == parent.end()) return {}; // not actually reachable
        key = it->second;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

std::vector<GridPos> computeTargetableTiles(const std::vector<Unit>& units,
                                             const Unit& attacker,
                                             GridPos origin) {
    std::vector<GridPos> targets;
    for (const auto& u : units) {
        // isPresent(): a retreated enemy (docs/enemy_ai_rules.md) is no
        // longer a valid attack target even though isAlive() stays true.
        if (!u.isPresent() || u.team == attacker.team) continue;
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
