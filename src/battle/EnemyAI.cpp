#include "jf/battle/EnemyAI.hpp"

#include <algorithm>
#include <climits>

#include "jf/battle/CombatResolver.hpp"
#include "jf/battle/Movement.hpp"
#include "jf/battle/ObjectiveTracker.hpp"
#include "jf/battle/StatusEffects.hpp"

namespace jf {

namespace {

// Mirrors BattleController::finishPlayerAction()'s ordering (docs/
// status_effects.md 地形効果の処理順) for enemy units: burn/stagger
// action-end processing runs right before the unit is marked acted. Also
// feeds an ActionResolvedEvent to Objective tracking (docs/
// mission_objectives.md), same as the player path. Combat-caused defeats
// must already be captured/emitted by the caller (see takeEnemyTurn) before
// this runs; this only catches a self-defeat from burn.
void finishEnemyAction(BattleState& battle, Unit& enemy, ActionKind actionKind) {
    const AliveSnapshot aliveBefore = captureAliveSnapshot(battle);
    processActionEndStatusEffects(battle, enemy);
    battle.markActed(enemy);
    emitUnitDefeatedEvents(battle, aliveBefore);

    const BattleEventId eventId = battle.issueEventId();
    BattleEvent event{eventId, static_cast<ActionId>(eventId),
                      ActionResolvedEvent{static_cast<ActionId>(eventId), enemy.id, enemy.team, actionKind,
                                         enemy.position}};
    handleObjectiveEvent(battle.missionState(), event);
}

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
// Returns the unit actually attacked, or nullptr if nothing was in range.
Unit* attackIfPossible(BattleState& battle, Unit& enemy, Unit* preferredTarget) {
    auto inRange = [&](const Unit& target) {
        int dist = manhattanDistance(enemy.position, target.position);
        return dist >= enemy.minimumAttackRange() && dist <= enemy.weapon.maxRange;
    };

    if (preferredTarget && preferredTarget->isAlive() && inRange(*preferredTarget)) {
        const bool hit = battle.rollAttackHit(*preferredTarget);
        resolveAttack(enemy, *preferredTarget,
                      battle.combatDefenseBonus(*preferredTarget, enemy), hit);
        if (hit && enemy.weapon.causesKnockback && preferredTarget->isAlive())
            battle.applyKnockback(enemy, *preferredTarget);
        return preferredTarget;
    }
    for (auto& u : battle.units()) {
        if (u.team == Team::Player && u.isAlive() && inRange(u)) {
            const bool hit = battle.rollAttackHit(u);
            resolveAttack(enemy, u, battle.combatDefenseBonus(u, enemy), hit);
            if (hit && enemy.weapon.causesKnockback && u.isAlive()) battle.applyKnockback(enemy, u);
            return &u;
        }
    }
    return nullptr;
}

// docs/regions/ashbough_forest.md "狼AI". Wolves share one target priority
// (isolated ally > lowest HP > nearest > UnitId order) and only commit to an
// attack once 2+ of them can converge on it - a lone wolf holds back instead
// of walking into a player's threat range. All comparisons are positional/
// stat-based with a stable UnitId tiebreak, so the same board state always
// produces the same decision.
int adjacentAllyCount(const BattleState& battle, const Unit& player) {
    int count = 0;
    for (const Unit& u : battle.units()) {
        if (&u == &player || u.team != Team::Player || !u.isAlive()) continue;
        if (manhattanDistance(u.position, player.position) == 1) ++count;
    }
    return count;
}

Unit* chooseWolfTarget(BattleState& battle, const Unit& wolf) {
    std::vector<Unit*> players;
    for (Unit& u : battle.units()) {
        if (u.team == Team::Player && u.isAlive()) players.push_back(&u);
    }
    if (players.empty()) return nullptr;
    std::stable_sort(players.begin(), players.end(), [&](Unit* a, Unit* b) {
        int adjA = adjacentAllyCount(battle, *a);
        int adjB = adjacentAllyCount(battle, *b);
        if (adjA != adjB) return adjA < adjB; // fewer nearby allies = more isolated = higher priority
        if (a->currentHp != b->currentHp) return a->currentHp < b->currentHp;
        int distA = manhattanDistance(wolf.position, a->position);
        int distB = manhattanDistance(wolf.position, b->position);
        if (distA != distB) return distA < distB;
        return a->id < b->id;
    });
    return players.front();
}

bool canReachAttackRange(BattleState& battle, Unit& wolf, const Unit& target) {
    int distNow = manhattanDistance(wolf.position, target.position);
    if (distNow >= wolf.minimumAttackRange() && distNow <= wolf.weapon.maxRange) return true;
    if (wolf.hasActed) return false; // already committed to its final position this phase
    for (GridPos tile : computeReachableTiles(battle, wolf)) {
        int dist = manhattanDistance(tile, target.position);
        if (dist >= wolf.minimumAttackRange() && dist <= wolf.weapon.maxRange) return true;
    }
    return false;
}

int wolvesAbleToReach(BattleState& battle, const Unit& target) {
    int count = 0;
    for (Unit& u : battle.units()) {
        if (u.team == Team::Enemy && u.unitClass == UnitClass::Wolf && u.isAlive() &&
            canReachAttackRange(battle, u, target)) {
            ++count;
        }
    }
    return count;
}

bool tileThreatenedByAnyPlayer(const BattleState& battle, GridPos tile) {
    for (const Unit& u : battle.units()) {
        if (u.team != Team::Player || !u.isAlive()) continue;
        int dist = manhattanDistance(u.position, tile);
        if (dist >= u.minimumAttackRange() && dist <= u.weapon.maxRange) return true;
    }
    return false;
}

Unit* takeWolfPackTurn(BattleState& battle, Unit& wolf) {
    Unit* target = chooseWolfTarget(battle, wolf);
    if (!target) {
        finishEnemyAction(battle, wolf, ActionKind::Wait);
        return nullptr;
    }

    if (wolvesAbleToReach(battle, *target) >= 2) {
        // Pack is ready: behave like a normal attacker (move-then-attack).
        const AliveSnapshot aliveBeforeAttack = captureAliveSnapshot(battle);
        if (Unit* attacked = attackIfPossible(battle, wolf, target)) {
            emitUnitDefeatedEvents(battle, aliveBeforeAttack);
            finishEnemyAction(battle, wolf, ActionKind::Attack);
            return attacked;
        }
        std::vector<GridPos> reachable = computeReachableTiles(battle, wolf);
        GridPos bestTile = wolf.position;
        int bestDist = manhattanDistance(wolf.position, target->position);
        for (GridPos tile : reachable) {
            int dist = manhattanDistance(tile, target->position);
            if (dist < bestDist) {
                bestDist = dist;
                bestTile = tile;
            }
        }
        battle.moveUnit(wolf, bestTile);
        Unit* attacked = attackIfPossible(battle, wolf, target);
        if (attacked) emitUnitDefeatedEvents(battle, aliveBeforeAttack);
        finishEnemyAction(battle, wolf, attacked ? ActionKind::Attack : ActionKind::Move);
        return attacked;
    }

    // A lone wolf avoids entering a new threat area, but it does not become
    // harmless once a player is already adjacent. In that case it bites and
    // then remains exposed, preserving the cautious identity without giving
    // the player a free adjacent enemy.
    const AliveSnapshot aliveBeforeAttack = captureAliveSnapshot(battle);
    if (Unit* attacked = attackIfPossible(battle, wolf, target)) {
        emitUnitDefeatedEvents(battle, aliveBeforeAttack);
        finishEnemyAction(battle, wolf, ActionKind::Attack);
        return attacked;
    }

    // Alone and out of range: advance without stepping into a player's
    // current threat range.
    std::vector<GridPos> reachable = computeReachableTiles(battle, wolf);
    GridPos bestTile = wolf.position;
    int bestDist = manhattanDistance(wolf.position, target->position);
    for (GridPos tile : reachable) {
        if (tileThreatenedByAnyPlayer(battle, tile)) continue;
        int dist = manhattanDistance(tile, target->position);
        if (dist < bestDist) {
            bestDist = dist;
            bestTile = tile;
        }
    }
    battle.moveUnit(wolf, bestTile);
    finishEnemyAction(battle, wolf, ActionKind::Move);
    return nullptr;
}

// docs/regions/ashbough_forest.md "灰角大猪"/"行動優先順位". Values from the
// doc's stat table; DEF/RES-while-stunned are hardcoded here rather than
// going through the generic defenseDownActive mechanic, since that one
// never touches RES and this boss's collision stun needs both.
constexpr int kBoarChargeRangeNormal = 3;
constexpr int kBoarChargeRangeEnraged = 4;
constexpr int kBoarChargePowerBonus = 50;
constexpr int kBoarSweepPowerBonus = 2;
constexpr int kBoarEnragedStrength = 11;
constexpr int kBoarBaseDefense = 5;
constexpr int kBoarBaseResistance = 1;
constexpr int kBoarStunnedDefense = 2;
constexpr int kBoarStunnedResistance = 0;

// The 3-tile sweep pattern: the column immediately toward the player side
// (one lower than the boar's), spanning boar.row-1..boar.row+1. Off-board
// tiles are simply skipped (docs: "盤外マスは無視する").
std::vector<Unit*> boarSweepTargets(BattleState& battle, const Unit& boar) {
    std::vector<Unit*> targets;
    int col = boar.position.col - 1;
    if (col < 0) return targets;
    for (int row = boar.position.row - 1; row <= boar.position.row + 1; ++row) {
        GridPos pos{row, col};
        if (!isInBounds(pos)) continue;
        Unit* occupant = battle.unitAt(pos);
        if (occupant && occupant->team == Team::Player && occupant->isAlive()) targets.push_back(occupant);
    }
    return targets;
}

// A charge target exists if a living ally sits on the boar's own row,
// toward the player side (lower column), within `range` tiles.
bool boarChargeTargetAvailable(BattleState& battle, const Unit& boar, int range) {
    for (const Unit& u : battle.units()) {
        if (u.team != Team::Player || !u.isAlive() || u.position.row != boar.position.row) continue;
        int dist = boar.position.col - u.position.col;
        if (dist > 0 && dist <= range) return true;
    }
    return false;
}

// Executes a telegraphed charge along the boar's current row: advances up
// to `range` tiles toward decreasing column, damaging (STR+4-DEF, doesn't
// stop for) every player unit it passes over, and stopping the instant it
// reaches a movement-blocking Battle Object (a fallen log) or the board
// edge. A log collision destroys the log, applies the DEF2/RES0 stun (one
// skipped Enemy Phase), and records it for the "倒木衝突" secondary reward.
void executeBoarCharge(BattleState& battle, Unit& boar) {
    const int range = boar.bossEnraged ? kBoarChargeRangeEnraged : kBoarChargeRangeNormal;
    const int power = boar.stats.strength + kBoarChargePowerBonus;
    const int row = boar.position.row;
    int endCol = boar.position.col;
    bool collided = false;

    for (int step = 1; step <= range; ++step) {
        int col = boar.position.col - step;
        if (col < 0) break; // board edge: stop at the last valid tile from the previous iteration
        GridPos pos{row, col};
        endCol = col;

        Unit* occupant = battle.unitAt(pos);
        if (occupant && occupant->team == Team::Player && occupant->isAlive()) {
            int damage = std::max(power - occupant->effectiveDefense(), 1);
            occupant->currentHp = std::max(occupant->currentHp - damage, 0);
        }

        BattleObjectState* object = battle.objectAt(pos);
        if (object && object->state != BattleObjectStateKind::Destroyed) {
            const BattleObjectDefinition* def = battle.objectDefinition(object->definitionId);
            if (def && def->blocksMovement) {
                object->state = BattleObjectStateKind::Destroyed;
                collided = true;
                break;
            }
        }
    }

    boar.position = GridPos{row, endCol};
    boar.chargeTelegraphed = false;
    if (collided) {
        boar.bossStunnedNextEnemyPhase = true;
        boar.bossWeakenedFromStun = true;
        boar.stats.defense = kBoarStunnedDefense;
        boar.stats.resistance = kBoarStunnedResistance;
        battle.markBossCollidedWithBarrier();
    }
}

Unit* takeBoarBossTurn(BattleState& battle, Unit& boar) {
    // 1. Skip this Enemy Phase entirely if still stunned from a log
    // collision; the DEF/RES penalty stays in effect through this skip.
    if (boar.bossStunnedNextEnemyPhase) {
        boar.bossStunnedNextEnemyPhase = false;
        finishEnemyAction(battle, boar, ActionKind::Wait);
        return nullptr;
    }
    // Right before it can act again, the stun's stat penalty lifts.
    if (boar.bossWeakenedFromStun) {
        boar.bossWeakenedFromStun = false;
        boar.stats.defense = kBoarBaseDefense;
        boar.stats.resistance = kBoarBaseResistance;
    }

    // 2. Enrage is an instant, non-turn-consuming state update, checked
    // first so it can influence this same turn's decision.
    if (!boar.bossEnraged && boar.currentHp * 2 <= boar.stats.maxHp) {
        boar.bossEnraged = true;
        boar.stats.strength = kBoarEnragedStrength;
    }
    const int range = boar.bossEnraged ? kBoarChargeRangeEnraged : kBoarChargeRangeNormal;

    // 3. A telegraphed charge always executes now, before anything else.
    if (boar.chargeTelegraphed) {
        executeBoarCharge(battle, boar);
        finishEnemyAction(battle, boar, ActionKind::Attack);
        return nullptr;
    }

    // 4. Sweep whenever at least one ally is in the 3-tile pattern. Clumping
    // still makes this substantially worse because every occupant is hit,
    // but leaving exactly one unit in front of the boss is no longer a safe
    // state where the boar silently gives up its attack.
    std::vector<Unit*> sweepTargets = boarSweepTargets(battle, boar);
    if (!sweepTargets.empty()) {
        const int power = boar.stats.strength + kBoarSweepPowerBonus;
        const AliveSnapshot aliveBefore = captureAliveSnapshot(battle);
        for (Unit* target : sweepTargets) {
            int damage = std::max(power - target->effectiveDefense(), 1);
            target->currentHp = std::max(target->currentHp - damage, 0);
        }
        emitUnitDefeatedEvents(battle, aliveBefore);
        finishEnemyAction(battle, boar, ActionKind::Attack);
        return sweepTargets.front();
    }

    // 5. Telegraph a charge if a target is reachable along the current row.
    if (boarChargeTargetAvailable(battle, boar, range)) {
        boar.chargeTelegraphed = true;
        finishEnemyAction(battle, boar, ActionKind::Wait);
        return nullptr;
    }

    // Enraged behavior never falls back to an ordinary move. Reposition to
    // a reachable tile on a living player's row, then lock that row for the
    // next charge. This move+telegraph still consumes only this one action.
    if (boar.bossEnraged) {
        std::vector<GridPos> reachable = computeReachableTiles(battle, boar);
        GridPos bestTile = boar.position;
        int bestScore = std::numeric_limits<int>::max();
        for (const GridPos& tile : reachable) {
            for (const Unit& target : battle.units()) {
                if (target.team != Team::Player || !target.isAlive() || tile.row != target.position.row) continue;
                const int score = manhattanDistance(tile, target.position);
                const bool stableTieBreak = tile.row < bestTile.row ||
                                            (tile.row == bestTile.row && tile.col < bestTile.col);
                if (score < bestScore || (score == bestScore && stableTieBreak)) {
                    bestScore = score;
                    bestTile = tile;
                }
            }
        }
        if (bestTile != boar.position) battle.moveUnit(boar, bestTile);
        boar.chargeTelegraphed = true;
        finishEnemyAction(battle, boar, ActionKind::Wait);
        return nullptr;
    }

    // 6. Otherwise, close the distance with ordinary movement.
    Unit* target = findNearestPlayer(battle, boar);
    if (target) {
        std::vector<GridPos> reachable = computeReachableTiles(battle, boar);
        GridPos bestTile = boar.position;
        int bestDist = manhattanDistance(boar.position, target->position);
        for (const GridPos& tile : reachable) {
            int dist = manhattanDistance(tile, target->position);
            if (dist < bestDist) {
                bestDist = dist;
                bestTile = tile;
            }
        }
        battle.moveUnit(boar, bestTile);
    }
    finishEnemyAction(battle, boar, ActionKind::Move);
    return nullptr;
}

} // namespace

Unit* takeEnemyTurn(BattleState& battle, Unit& enemy) {
    if (!enemy.isAlive() || enemy.hasActed) return nullptr;
    if (enemy.unitClass == UnitClass::Wolf) return takeWolfPackTurn(battle, enemy);
    if (enemy.unitClass == UnitClass::AshenhornBoar) return takeBoarBossTurn(battle, enemy);

    Unit* target = findNearestPlayer(battle, enemy);
    if (!target) {
        finishEnemyAction(battle, enemy, ActionKind::Wait);
        return nullptr;
    }

    // Captured once, before either attack attempt below, so a defeat from
    // either one fires UnitDefeatedEvent exactly once.
    const AliveSnapshot aliveBeforeAttack = captureAliveSnapshot(battle);

    if (Unit* attacked = attackIfPossible(battle, enemy, target)) {
        emitUnitDefeatedEvents(battle, aliveBeforeAttack);
        finishEnemyAction(battle, enemy, ActionKind::Attack);
        return attacked;
    }

    std::vector<GridPos> reachable = computeReachableTiles(battle, enemy);
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

    Unit* attacked = attackIfPossible(battle, enemy, target);
    if (attacked) emitUnitDefeatedEvents(battle, aliveBeforeAttack);
    finishEnemyAction(battle, enemy, attacked ? ActionKind::Attack : ActionKind::Move);
    return attacked;
}

} // namespace jf
