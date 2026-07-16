#include "jf/battle/EnemyAI.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>

#include "jf/battle/CombatResolver.hpp"
#include "jf/battle/Movement.hpp"
#include "jf/battle/ObjectiveTracker.hpp"
#include "jf/battle/SkillCharges.hpp"
#include "jf/battle/StatusEffects.hpp"
#include "jf/battle/AiSystem.hpp"

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

// 槍兵`counterthrust`(反撃準備、SkillCategory::Reactive): unlike every
// other skill implemented so far, this has no chooseSkill()/
// selectSkillTarget() step at all - docs/initial_skill_effects.md "単体武器
// 攻撃を受け生存時、攻撃者へ通常攻撃1回" auto-triggers whenever `defender`
// is attacked (hit or missed - it's about being engaged, not about being
// hit) and survives, provided the attacker ends up within `defender`'s own
// weapon range ("その敵が武器射程内なら", Skill.cpp's effect text). Hooked
// only here (both call sites in attackIfPossible() below, shared by every
// non-Boss enemy's turn via takeEnemyTurn()) rather than in the Boar boss's
// sweep/charge, which apply damage directly and never call resolveAttack()
// - matching "単体武器攻撃" (a normal single-target weapon attack).
void tryCounterthrust(BattleState& battle, Unit& defender, Unit& attacker) {
    if (!defender.isAlive() || !attacker.isAlive()) return;
    for (std::size_t i = 0; i < defender.skillSlots.size(); ++i) {
        if (defender.skillSlots[i].skillId != "counterthrust") continue;
        if (!skillSlotAvailable(defender, static_cast<int>(i))) continue;
        int dist = manhattanDistance(defender.position, attacker.position);
        if (dist < defender.minimumAttackRange() || dist > defender.weapon.maxRange) return;
        const bool hit = battle.rollAttackHit(attacker);
        resolveAttack(defender, attacker, battle.combatDefenseBonus(attacker, defender), hit);
        if (hit && defender.weapon.causesKnockback && attacker.isAlive())
            battle.applyKnockback(defender, attacker);
        consumeSkillCharge(defender, static_cast<int>(i));
        return;
    }
}

// 監視弓兵`overwatch`(警戒射撃): each overwatching player unit (Unit::
// overwatchActive, armed by BattleController::chooseSkill()'s self-resolve
// branch) ambushes the first enemy that is or comes within ITS OWN weapon
// range - checked from takeEnemyTurn() both before `enemy` acts (catches an
// enemy already in range going into its turn) and again after it moves
// (catches movement carrying it into range). Different watchers are
// independent (each has its own readiness/charge, already consumed at cast
// time), so more than one may fire in the same call if `enemy` enters
// multiple ranges at once. Wired for every non-Boss enemy (Wolves included,
// now that they share the generic takeEnemyTurn() path too) - Boar boss AI
// is the only one still exempt, not something docs/initial_skill_effects.md
// requires explicitly the way `provoke`'s "Boss予告は変更しない" does.
void triggerOverwatch(BattleState& battle, Unit& enemy) {
    for (Unit& watcher : battle.units()) {
        if (!enemy.isAlive()) return;
        if (watcher.team != Team::Player || !watcher.isAlive() || !watcher.overwatchActive) continue;
        int dist = manhattanDistance(watcher.position, enemy.position);
        if (dist < watcher.minimumAttackRange() || dist > watcher.weapon.maxRange) continue;
        const bool hit = battle.rollAttackHit(enemy);
        resolveAttack(watcher, enemy, battle.combatDefenseBonus(enemy, watcher), hit);
        if (hit && watcher.weapon.causesKnockback && enemy.isAlive()) battle.applyKnockback(watcher, enemy);
        watcher.overwatchActive = false;
    }
}

// Attacks the preferred target if in range, otherwise any in-range target
// (unless `onlyPreferred` is set - 古参守備兵`provoke`'s "使用者を攻撃可能
// なら対象評価で最優先" means a provoked enemy passes up an opportunistic
// attack on anyone else rather than falling back, see takeEnemyTurn()).
// Returns the unit actually attacked, or nullptr if nothing was in range.
Unit* attackIfPossible(BattleState& battle, Unit& enemy, Unit* preferredTarget, bool onlyPreferred = false) {
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
        tryCounterthrust(battle, *preferredTarget, enemy);
        return preferredTarget;
    }
    if (onlyPreferred) return nullptr;
    for (auto& u : battle.units()) {
        if (u.team == Team::Player && u.isAlive() && inRange(u)) {
            const bool hit = battle.rollAttackHit(u);
            resolveAttack(enemy, u, battle.combatDefenseBonus(u, enemy), hit);
            if (hit && enemy.weapon.causesKnockback && u.isAlive()) battle.applyKnockback(enemy, u);
            tryCounterthrust(battle, u, enemy);
            return &u;
        }
    }
    return nullptr;
}

// docs/regions/ashbough_forest.md "灰角大猪"/"行動優先順位". Values from the
// doc's stat table; DEF/RES-while-stunned are hardcoded here rather than
// going through the generic defenseDownActive mechanic, since that one
// never touches RES and this boss's collision stun needs both.
constexpr int kBoarChargeRangeNormal = 3;
constexpr int kBoarChargeRangeEnraged = 4;
constexpr int kBoarChargePowerBonus = 10;
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

Unit* performBoarSweep(BattleState& battle, Unit& boar) {
    std::vector<Unit*> targets = boarSweepTargets(battle, boar);
    if (targets.empty()) return nullptr;
    const int power = boar.stats.strength + kBoarSweepPowerBonus;
    const AliveSnapshot aliveBefore = captureAliveSnapshot(battle);
    for (Unit* target : targets) {
        const int damage = std::max(power - target->effectiveDefense(), 1);
        target->currentHp = std::max(target->currentHp - damage, 0);
    }
    emitUnitDefeatedEvents(battle, aliveBefore);
    return targets.front();
}

// Returns the direction of the best charge target on the boar's row. A
// charge can travel either left or right; lowest HP, then nearest, then ID
// keeps the choice aggressive and deterministic.
int boarChargeDirectionForTarget(BattleState& battle, const Unit& boar, int range) {
    const Unit* best = nullptr;
    int bestDistance = range + 1;
    for (const Unit& unit : battle.units()) {
        if (unit.team != Team::Player || !unit.isAlive() || unit.position.row != boar.position.row) continue;
        const int distance = std::abs(unit.position.col - boar.position.col);
        if (distance == 0 || distance > range) continue;
        if (!best || unit.currentHp < best->currentHp ||
            (unit.currentHp == best->currentHp && distance < bestDistance) ||
            (unit.currentHp == best->currentHp && distance == bestDistance && unit.id < best->id)) {
            best = &unit;
            bestDistance = distance;
        }
    }
    if (!best) return 0;
    return best->position.col < boar.position.col ? -1 : 1;
}

// Side-effect-free preview of executeBoarCharge()'s walk, for populating
// BossTelegraph::lockedTiles at telegraph time (docs/boss_common_rules.md's
// "攻撃列" - the UI's danger-zone highlight). Since the boar doesn't move
// between telegraph and execution and nothing else changes the board mid-
// Enemy-Phase, this predicts the exact tiles executeBoarCharge() will later
// walk: stops at the board edge (tile not included) or at a movement-
// blocking Battle Object (that tile IS included, since the log itself gets
// hit/destroyed there).
std::vector<GridPos> computeBoarChargeTiles(const BattleState& battle, const Unit& boar, int direction, int range) {
    std::vector<GridPos> tiles;
    const int row = boar.position.row;
    for (int step = 1; step <= range; ++step) {
        int col = boar.position.col + direction * step;
        if (col < 0 || col >= kGridCols) break;
        GridPos pos{row, col};
        tiles.push_back(pos);

        const BattleObjectState* object = battle.objectAt(pos);
        if (object && object->state != BattleObjectStateKind::Destroyed) {
            const BattleObjectDefinition* def = battle.objectDefinition(object->definitionId);
            if (def && def->blocksMovement) break;
        }
    }
    return tiles;
}

// Executes a telegraphed charge along the boar's current row: advances up
// to `range` tiles in the direction locked during telegraphing, damaging
// stop for) every player unit it passes over, and stopping the instant it
// reaches a movement-blocking Battle Object (a fallen log) or the board
// edge. A log collision destroys the log, applies the DEF2/RES0 stun (one
// skipped Enemy Phase), and records it for the "倒木衝突" secondary reward.
// (Mirrors computeBoarChargeTiles()'s walk above, which is what populates
// BossTelegraph::lockedTiles at telegraph time - kept as a separate loop
// here since this one also applies damage/destroys the log.)
void executeBoarCharge(BattleState& battle, Unit& boar) {
    const int range = boar.bossEnraged ? kBoarChargeRangeEnraged : kBoarChargeRangeNormal;
    const int power = boar.stats.strength + kBoarChargePowerBonus;
    const int row = boar.position.row;
    const int direction = boar.bossRuntime.telegraph.direction < 0 ? -1 : 1;
    int endCol = boar.position.col;
    bool collided = false;

    for (int step = 1; step <= range; ++step) {
        int col = boar.position.col + direction * step;
        if (col < 0 || col >= kGridCols) break;
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
    boar.chargeDirection = -1;
    boar.bossRuntime.telegraph.state = TelegraphState::Executed;
    handleObjectiveEvent(battle.missionState(),
                         {battle.issueEventId(), 0,
                          BossTelegraphChangedEvent{boar.id, boar.bossRuntime.telegraph.actionId, false}});
    boar.bossRuntime.telegraph.clear();
    boar.chargeCooldownActions = 1;
    ++boar.chargesExecuted;
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

    // A bidirectional charge could otherwise bounce across the board every
    // other action forever. One intervening action must use sweep, a normal
    // attack, or movement; it never becomes a free wait.
    const bool chargeOnCooldown = boar.chargeCooldownActions > 0 || boar.chargesExecuted >= 2;
    if (chargeOnCooldown) --boar.chargeCooldownActions;

    // 2. Enrage is an instant, non-turn-consuming state update, checked
    // first so it can influence this same turn's decision.
    if (!boar.bossEnraged && boar.currentHp * 2 <= boar.stats.maxHp) {
        boar.bossEnraged = true;
        boar.stats.strength = kBoarEnragedStrength;
        // docs/boss_common_rules.md "Phase移行": fired exactly once for this
        // transition, even though the doc's own numbered steps (resolve
        // Root Action Batch, evaluate defeat, THEN check thresholds) are a
        // full turn-boundary sequence this single-threshold boar doesn't
        // need in full - there's only one stage to move to.
        boar.bossRuntime.stageIndex = 1;
        handleObjectiveEvent(battle.missionState(),
                             BattleEvent{battle.issueEventId(), 0,
                                         BossStageChangedEvent{boar.id, boar.bossRuntime.stageIndex}});
    }
    const int range = boar.bossEnraged ? kBoarChargeRangeEnraged : kBoarChargeRangeNormal;

    // 3. A telegraphed charge always executes now, before anything else.
    if (boar.bossRuntime.telegraph.pending()) {
        executeBoarCharge(battle, boar);
        finishEnemyAction(battle, boar, ActionKind::Attack);
        return nullptr;
    }

    // 4. Sweep whenever at least one ally is in the 3-tile pattern. Clumping
    // still makes this substantially worse because every occupant is hit,
    // but leaving exactly one unit in front of the boss is no longer a safe
    // state where the boar silently gives up its attack.
    if (Unit* swept = performBoarSweep(battle, boar)) {
        finishEnemyAction(battle, boar, ActionKind::Attack);
        return swept;
    }

    // 5. Telegraph a charge if a target is reachable along the current row.
    if (!chargeOnCooldown) {
        const int direction = boarChargeDirectionForTarget(battle, boar, range);
        if (direction != 0) {
        boar.chargeTelegraphed = true;
        boar.chargeDirection = direction;
        boar.bossRuntime.telegraph = {"ashenhorn_charge", TelegraphShape::Line,
                                      TelegraphState::Announced, battle.round(), battle.round() + 1,
                                      {}, computeBoarChargeTiles(battle, boar, direction, range), direction};
        handleObjectiveEvent(battle.missionState(),
                             {battle.issueEventId(), 0,
                              BossTelegraphChangedEvent{boar.id, "ashenhorn_charge", true}});
        finishEnemyAction(battle, boar, ActionKind::Skill);
        return nullptr;
        }
    }

    // Enraged behavior never falls back to an ordinary move. Reposition to
    // a reachable tile on a living player's row, then lock that row for the
    // next charge. This move+telegraph still consumes only this one action.
    if (boar.bossEnraged && !chargeOnCooldown) {
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
        boar.chargeDirection = boarChargeDirectionForTarget(battle, boar, range);
        if (boar.chargeDirection == 0) boar.chargeDirection = -1;
        boar.bossRuntime.telegraph = {"ashenhorn_charge", TelegraphShape::Line,
                                      TelegraphState::Announced, battle.round(), battle.round() + 1,
                                      {}, computeBoarChargeTiles(battle, boar, boar.chargeDirection, range),
                                      boar.chargeDirection};
        handleObjectiveEvent(battle.missionState(),
                             {battle.issueEventId(), 0,
                              BossTelegraphChangedEvent{boar.id, "ashenhorn_charge", true}});
        finishEnemyAction(battle, boar, ActionKind::Skill);
        return nullptr;
    }

    // 6. Otherwise, close the distance, then immediately re-evaluate every
    // offensive option. A normal turn never ends after movement alone when
    // an attack or charge telegraph is available from the new tile.
    Unit* target = findNearestPlayer(battle, boar);
    bool moved = false;
    if (target) {
        std::vector<GridPos> reachable = computeReachableTiles(battle, boar);
        GridPos bestTile = boar.position;
        int bestDist = manhattanDistance(boar.position, target->position);
        bool bestEnablesCharge = !chargeOnCooldown &&
                                 boarChargeDirectionForTarget(battle, boar, range) != 0;
        for (const GridPos& tile : reachable) {
            int dist = manhattanDistance(tile, target->position);
            const int chargeDistance = std::abs(tile.col - target->position.col);
            const bool enablesCharge = !chargeOnCooldown && tile.row == target->position.row &&
                                       chargeDistance > 0 && chargeDistance <= range;
            if ((enablesCharge && !bestEnablesCharge) ||
                (enablesCharge == bestEnablesCharge && dist < bestDist)) {
                bestEnablesCharge = enablesCharge;
                bestDist = dist;
                bestTile = tile;
            }
        }
        if (bestTile != boar.position) moved = battle.moveUnit(boar, bestTile);
    }

    if (Unit* swept = performBoarSweep(battle, boar)) {
        finishEnemyAction(battle, boar, ActionKind::Attack);
        return swept;
    }
    if (!chargeOnCooldown) {
        const int direction = boarChargeDirectionForTarget(battle, boar, range);
        if (direction != 0) {
        boar.chargeTelegraphed = true;
        boar.chargeDirection = direction;
        boar.bossRuntime.telegraph = {"ashenhorn_charge", TelegraphShape::Line,
                                      TelegraphState::Announced, battle.round(), battle.round() + 1,
                                      {}, computeBoarChargeTiles(battle, boar, direction, range), direction};
        handleObjectiveEvent(battle.missionState(),
                             {battle.issueEventId(), 0,
                              BossTelegraphChangedEvent{boar.id, "ashenhorn_charge", true}});
        finishEnemyAction(battle, boar, ActionKind::Skill);
        return nullptr;
        }
    }
    const AliveSnapshot aliveBeforeAttack = captureAliveSnapshot(battle);
    if (Unit* attacked = attackIfPossible(battle, boar, target)) {
        emitUnitDefeatedEvents(battle, aliveBeforeAttack);
        finishEnemyAction(battle, boar, ActionKind::Attack);
        return attacked;
    }
    finishEnemyAction(battle, boar, moved ? ActionKind::Move : ActionKind::Wait);
    return nullptr;
}

} // namespace

Unit* takeEnemyTurn(BattleState& battle, Unit& enemy, AiSquadReservations* reservations) {
    if (!enemy.isPresent() || enemy.hasActed) return nullptr;
    if (enemy.unitClass == UnitClass::AshenhornBoar) return takeBoarBossTurn(battle, enemy);

    // Captured once, before anything below (including 監視弓兵`overwatch`'s
    // ambush), so a defeat from any of it fires UnitDefeatedEvent exactly
    // once.
    const AliveSnapshot aliveBeforeAttack = captureAliveSnapshot(battle);

    // 監視弓兵`overwatch`(警戒射撃): ambushes `enemy` here, before it gets to
    // act at all, if it's already within an armed watcher's weapon range
    // going into this turn (see triggerOverwatch()).
    triggerOverwatch(battle, enemy);
    if (!enemy.isAlive()) {
        emitUnitDefeatedEvents(battle, aliveBeforeAttack);
        finishEnemyAction(battle, enemy, ActionKind::Wait);
        return nullptr;
    }

    // 古参守備兵`provoke`(挑発): if provoked, prioritize the provoking unit
    // over the normal nearest-player targeting for this Enemy Phase only
    // (docs/initial_skill_effects.md "次Enemy Phase...対象評価で最優先"). A
    // provoked enemy also passes up an opportunistic attack on anyone else
    // (attackIfPossible's `onlyPreferred`) - "最優先" means the provoker
    // wins even over a free in-range hit on a different unit. Wolf/Boar
    // boss AI never reach here (handled above), matching "Boss予告は
    // 変更しない".
    Unit* target = nullptr;
    bool provoked = false;
    if (!enemy.provokedByUnitId.empty()) {
        Unit* provoker = battle.findUnit(enemy.provokedByUnitId);
        if (provoker && provoker->isAlive()) {
            target = provoker;
            provoked = true;
        }
    }
    if (!target) {
        const AiSquadReservations emptyReservations;
        const AiProfile profile = profileFor(enemy);
        AiCandidate candidate = chooseBestAiCandidate(
            generateAiCandidates(battle, enemy, profile, reservations ? *reservations : emptyReservations));
        target = candidate.targetUnitId.empty() ? nullptr : battle.findUnit(candidate.targetUnitId);
        if (candidate.destination != enemy.position) battle.moveUnit(enemy, candidate.destination);
        if (reservations) {
            reservations->reserve(candidate);
        }
        if (candidate.type == AiActionType::Support && target && target->team == enemy.team) {
            target->currentHp = std::min(target->currentHp + 8, target->stats.maxHp);
            finishEnemyAction(battle, enemy, ActionKind::Skill);
            return nullptr;
        }
        if (candidate.type == AiActionType::Attack && target) {
            triggerOverwatch(battle, enemy);
            if (!enemy.isAlive()) {
                emitUnitDefeatedEvents(battle, aliveBeforeAttack);
                finishEnemyAction(battle, enemy, ActionKind::Move);
                return nullptr;
            }
            Unit* attacked = attackIfPossible(battle, enemy, target, true);
            if (attacked) emitUnitDefeatedEvents(battle, aliveBeforeAttack);
            finishEnemyAction(battle, enemy, attacked ? ActionKind::Attack : ActionKind::Move);
            return attacked;
        }
        if (candidate.type == AiActionType::Move) {
            triggerOverwatch(battle, enemy);
            if (!enemy.isAlive()) emitUnitDefeatedEvents(battle, aliveBeforeAttack);
            finishEnemyAction(battle, enemy, ActionKind::Move);
            return nullptr;
        }
        if (candidate.type == AiActionType::Retreat) {
            // docs/enemy_ai_rules.md "撤退と降伏": left the field alive -
            // Unit::isAlive() stays true (HP unaffected), so isPresent()
            // (not isAlive()) is what the rest of the codebase must check
            // to treat this unit as no longer a threat/target.
            enemy.hasExited = true;
            enemy.exitReason = UnitExitReason::Retreated;
            finishEnemyAction(battle, enemy, ActionKind::Move);
            return nullptr;
        }
    }
    if (!target) {
        finishEnemyAction(battle, enemy, ActionKind::Wait);
        return nullptr;
    }

    if (Unit* attacked = attackIfPossible(battle, enemy, target, provoked)) {
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

    // 監視弓兵`overwatch`: also check after movement, since moving is what
    // most often carries `enemy` newly into an armed watcher's range.
    triggerOverwatch(battle, enemy);
    if (!enemy.isAlive()) {
        emitUnitDefeatedEvents(battle, aliveBeforeAttack);
        finishEnemyAction(battle, enemy, ActionKind::Move);
        return nullptr;
    }

    Unit* attacked = attackIfPossible(battle, enemy, target, provoked);
    if (attacked) emitUnitDefeatedEvents(battle, aliveBeforeAttack);
    finishEnemyAction(battle, enemy, attacked ? ActionKind::Attack : ActionKind::Move);
    return attacked;
}

} // namespace jf
