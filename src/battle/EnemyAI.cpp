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

// 辺境猟兵「簡易罠」/`snare_trap`(docs/skill_system.md「辺境猟兵」): 罠は
// 敵味方とも通過可能だが、最初に踏んだ敵だけを停止させて消滅する(ダメージなし)。
// プレイヤー側のmoveUnit()呼び出し(通常移動・再移動・救援搬送等)では発動せず、
// 敵ユニット自身の自発移動(このファイル内の4箇所のbattle.moveUnit(enemy/boar,
// ...)呼び出し)の直後にだけ判定する - 強制移動(ノックバック等)でのトリガーは
// 仕様に明記が無いため対象外。
void triggerRangerTrapIfPresent(BattleState& battle, Unit& mover) {
    if (mover.team != Team::Enemy) return;
    BattleObjectState* trap = battle.objectAt(mover.position);
    if (!trap || trap->definitionId != "ranger_trap" || trap->state == BattleObjectStateKind::Destroyed) return;
    applyMoveDown(battle, mover);
    trap->state = BattleObjectStateKind::Destroyed;
    handleObjectiveEvent(battle.missionState(),
                         BattleEvent{battle.issueEventId(), 0, ObjectDestroyedEvent{trap->id}});
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
        resolveAttack(battle, defender, attacker, battle.combatDefenseBonus(attacker, defender), hit);
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
        resolveAttack(battle, watcher, enemy, battle.combatDefenseBonus(enemy, watcher), hit);
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
        resolveAttack(battle, enemy, *preferredTarget,
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
            resolveAttack(battle, enemy, u, battle.combatDefenseBonus(u, enemy), hit);
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
        if (bestTile != boar.position) {
            battle.moveUnit(boar, bestTile);
            triggerRangerTrapIfPresent(battle, boar);
        }
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
        if (bestTile != boar.position) {
            moved = battle.moveUnit(boar, bestTile);
            if (moved) triggerRangerTrapIfPresent(battle, boar);
        }
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

// docs/regions/ashiron_quarry.md "灰殻穿岩虫"/"行動". Scoped-down relative to
// the doc's full 6-step priority list (see M9-D plan's approximation notes):
// no scripted retreat-from-stake-operator targeting, no telegraphed warning
// for 崩落誘発 (it's a one-time board-state change, not a repeating attack
// telegraph like 潜行突進, so it fires immediately instead of being
// announced a round ahead).
constexpr int kGrubwormChargeRange = 3;
constexpr int kGrubwormChargePowerBonus = 3;
constexpr int kGrubwormBaseDefense = 8;
constexpr int kGrubwormShellDefenseBonus = 2;

// Mirrors computeBoarChargeTiles(): side-effect-free preview of
// executeGrubwormCharge()'s walk, for BossTelegraph::lockedTiles.
std::vector<GridPos> computeGrubwormChargeTiles(const BattleState& battle, const Unit& grubworm, int direction,
                                                int range) {
    std::vector<GridPos> tiles;
    const int row = grubworm.position.row;
    for (int step = 1; step <= range; ++step) {
        int col = grubworm.position.col + direction * step;
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

// Mirrors boarChargeDirectionForTarget(): lowest HP, then nearest, then ID.
int grubwormChargeDirectionForTarget(BattleState& battle, const Unit& grubworm, int range) {
    const Unit* best = nullptr;
    int bestDistance = range + 1;
    for (const Unit& unit : battle.units()) {
        if (unit.team != Team::Player || !unit.isAlive() || unit.position.row != grubworm.position.row) continue;
        const int distance = std::abs(unit.position.col - grubworm.position.col);
        if (distance == 0 || distance > range) continue;
        if (!best || unit.currentHp < best->currentHp ||
            (unit.currentHp == best->currentHp && distance < bestDistance) ||
            (unit.currentHp == best->currentHp && distance == bestDistance && unit.id < best->id)) {
            best = &unit;
            bestDistance = distance;
        }
    }
    if (!best) return 0;
    return best->position.col < grubworm.position.col ? -1 : 1;
}

// 潜行突進: advances up to kGrubwormChargeRange tiles along the current row,
// damaging (STR+3, per the doc) every player unit passed over, stopping at
// the board edge or a movement-blocking Battle Object. Sets
// bossChargeRecoveryPending so 岩殻防御's DEF+2 drops for exactly the next
// action (docs: "潜行から出た直後以外はDEF+2").
void executeGrubwormCharge(BattleState& battle, Unit& grubworm) {
    const int power = grubworm.stats.strength + kGrubwormChargePowerBonus;
    const int row = grubworm.position.row;
    const int direction = grubworm.bossRuntime.telegraph.direction < 0 ? -1 : 1;
    int endCol = grubworm.position.col;

    for (int step = 1; step <= kGrubwormChargeRange; ++step) {
        int col = grubworm.position.col + direction * step;
        if (col < 0 || col >= kGridCols) break;
        GridPos pos{row, col};
        endCol = col;

        Unit* occupant = battle.unitAt(pos);
        if (occupant && occupant->team == Team::Player && occupant->isAlive()) {
            int damage = std::max(power - occupant->effectiveDefense(), 1);
            occupant->currentHp = std::max(occupant->currentHp - damage, 0);
        }

        const BattleObjectState* object = battle.objectAt(pos);
        if (object && object->state != BattleObjectStateKind::Destroyed) {
            const BattleObjectDefinition* def = battle.objectDefinition(object->definitionId);
            if (def && def->blocksMovement) break;
        }
    }

    grubworm.position = GridPos{row, endCol};
    grubworm.chargeTelegraphed = false;
    grubworm.bossRuntime.telegraph.state = TelegraphState::Executed;
    handleObjectiveEvent(battle.missionState(),
                         {battle.issueEventId(), 0,
                          BossTelegraphChangedEvent{grubworm.id, grubworm.bossRuntime.telegraph.actionId, false}});
    grubworm.bossRuntime.telegraph.clear();
    grubworm.chargeCooldownActions = 1;
    grubworm.chargesExecuted += 1;
    grubworm.bossChargeRecoveryPending = true;
}

// 崩落誘発: HP<=50%, once, immediately converts up to 2 currently-empty,
// unoccupied Floor tiles (excluding the boss's own tile) to Rubble
// (impassable - docs/regions/ashiron_quarry.md "崩落した床"). Scan order is
// fixed (row-major) for determinism; no RNG plumbing is threaded into
// EnemyAI.cpp elsewhere, so this doesn't introduce one just for 2 tiles.
void triggerGrubwormCollapse(BattleState& battle, Unit& grubworm) {
    int converted = 0;
    for (int row = 0; row < kGridRows && converted < 2; ++row) {
        for (int col = 0; col < kGridCols && converted < 2; ++col) {
            GridPos pos{row, col};
            if (pos == grubworm.position) continue;
            if (battle.terrainAt(pos) != TerrainType::Floor) continue;
            if (battle.unitAt(pos)) continue;
            if (battle.objectAt(pos)) continue;
            battle.setTerrain(pos, TerrainType::Rubble);
            ++converted;
        }
    }
}

Unit* takeGrubwormBossTurn(BattleState& battle, Unit& grubworm) {
    // 岩殻防御: DEF+2 by default, drops for exactly the action right after
    // emerging from a charge.
    if (grubworm.bossChargeRecoveryPending) {
        grubworm.bossChargeRecoveryPending = false;
        grubworm.stats.defense = kGrubwormBaseDefense;
    } else {
        grubworm.stats.defense = kGrubwormBaseDefense + kGrubwormShellDefenseBonus;
    }

    // 崩落誘発: instant, non-turn-consuming state update, checked before this
    // turn's action decision (mirrors the boar's enrage check).
    if (!grubworm.bossCollapseUsed && grubworm.currentHp * 2 <= grubworm.stats.maxHp) {
        grubworm.bossCollapseUsed = true;
        triggerGrubwormCollapse(battle, grubworm);
        grubworm.bossRuntime.stageIndex = 1;
        handleObjectiveEvent(battle.missionState(),
                             BattleEvent{battle.issueEventId(), 0,
                                         BossStageChangedEvent{grubworm.id, grubworm.bossRuntime.stageIndex}});
    }

    // A telegraphed charge always executes now, before anything else.
    if (grubworm.bossRuntime.telegraph.pending()) {
        executeGrubwormCharge(battle, grubworm);
        finishEnemyAction(battle, grubworm, ActionKind::Attack);
        return nullptr;
    }

    const bool chargeOnCooldown = grubworm.chargeCooldownActions > 0;
    if (chargeOnCooldown) --grubworm.chargeCooldownActions;

    // Telegraph a charge if a target is reachable along the current row.
    if (!chargeOnCooldown) {
        const int direction = grubwormChargeDirectionForTarget(battle, grubworm, kGrubwormChargeRange);
        if (direction != 0) {
            grubworm.chargeTelegraphed = true;
            grubworm.chargeDirection = direction;
            grubworm.bossRuntime.telegraph = {"grubworm_dash", TelegraphShape::Line, TelegraphState::Announced,
                                              battle.round(), battle.round() + 1, {},
                                              computeGrubwormChargeTiles(battle, grubworm, direction,
                                                                        kGrubwormChargeRange),
                                              direction};
            handleObjectiveEvent(battle.missionState(),
                                 {battle.issueEventId(), 0,
                                  BossTelegraphChangedEvent{grubworm.id, "grubworm_dash", true}});
            finishEnemyAction(battle, grubworm, ActionKind::Skill);
            return nullptr;
        }
    }

    // Otherwise, close the distance, then attack normally (generic fallback,
    // no bespoke logic beyond what every other enemy already uses).
    Unit* target = findNearestPlayer(battle, grubworm);
    bool moved = false;
    if (target) {
        std::vector<GridPos> reachable = computeReachableTiles(battle, grubworm);
        GridPos bestTile = grubworm.position;
        int bestDist = manhattanDistance(grubworm.position, target->position);
        for (const GridPos& tile : reachable) {
            int dist = manhattanDistance(tile, target->position);
            if (dist < bestDist) {
                bestDist = dist;
                bestTile = tile;
            }
        }
        if (bestTile != grubworm.position) moved = battle.moveUnit(grubworm, bestTile);
    }

    const AliveSnapshot aliveBeforeAttack = captureAliveSnapshot(battle);
    if (Unit* attacked = attackIfPossible(battle, grubworm, target)) {
        emitUnitDefeatedEvents(battle, aliveBeforeAttack);
        finishEnemyAction(battle, grubworm, ActionKind::Attack);
        return attacked;
    }
    finishEnemyAction(battle, grubworm, moved ? ActionKind::Move : ActionKind::Wait);
    return nullptr;
}

// docs/regions/blackwater_lowlands.md "沼牙の大蛇"/"行動"/"行動優先順位".
// Approximated relative to the doc the same way M9-D scoped down the
// grubworm (see this function's own comments below for each spot):
// - 水中潜行's "浅瀬が経路にない場合は使用しない" is approximated to "no
//   Shallows tile anywhere on the board" (no per-path terrain-walk exists
//   for a teleport-style move, unlike the boar/grubworm's straight-line
//   charges which do have one) rather than a true path check.
// - The `[辺境猟兵]` route's "実際の移動先1マスだけを表示" is deferred: no
//   route-conditional boss-AI wiring exists anywhere in the engine (grep
//   0 hits), so every route gets the same 2-tile telegraph.
// - "水源標識は押し出さず、固定2ダメージを受ける" is deferred: Objects have
//   no HP/durability field anywhere (same known gap as M6-C/M9-C/M9-D/-J),
//   so markers simply aren't pushed and take no damage either.
// - "毒溜まりへ押し出された場合は毒付与" is deferred: no poison-pool terrain
//   flavor exists (only Shallows), so a push never poisons.
constexpr int kSerpentVenomBonus = 3;
constexpr int kSerpentConstrictBonus = 1;

// 締め付け's front-3 pattern: identical shape to boarSweepTargets() (the
// column immediately toward the player side), reused for a different boss.
std::vector<Unit*> serpentConstrictTargets(BattleState& battle, const Unit& serpent) {
    std::vector<Unit*> targets;
    int col = serpent.position.col - 1;
    if (col < 0) return targets;
    for (int row = serpent.position.row - 1; row <= serpent.position.row + 1; ++row) {
        GridPos pos{row, col};
        if (!isInBounds(pos)) continue;
        Unit* occupant = battle.unitAt(pos);
        if (occupant && occupant->team == Team::Player && occupant->isAlive()) targets.push_back(occupant);
    }
    return targets;
}

int countAdjacentPlayers(BattleState& battle, const Unit& serpent) {
    int count = 0;
    for (const GridPos& delta : {GridPos{-1, 0}, GridPos{1, 0}, GridPos{0, -1}, GridPos{0, 1}}) {
        GridPos pos{serpent.position.row + delta.row, serpent.position.col + delta.col};
        if (!isInBounds(pos)) continue;
        Unit* occupant = battle.unitAt(pos);
        if (occupant && occupant->team == Team::Player && occupant->isAlive()) ++count;
    }
    return count;
}

Unit* adjacentPlayer(BattleState& battle, const Unit& serpent) {
    for (const GridPos& delta : {GridPos{-1, 0}, GridPos{1, 0}, GridPos{0, -1}, GridPos{0, 1}}) {
        GridPos pos{serpent.position.row + delta.row, serpent.position.col + delta.col};
        if (!isInBounds(pos)) continue;
        Unit* occupant = battle.unitAt(pos);
        if (occupant && occupant->team == Team::Player && occupant->isAlive()) return occupant;
    }
    return nullptr;
}

// True if any tile on the board is Shallows (see this function's own header
// comment above on the approximation this makes for "経路に浅瀬がない").
bool anyShallowsOnBoard(const BattleState& battle) {
    for (int row = 0; row < kGridRows; ++row)
        for (int col = 0; col < kGridCols; ++col)
            if (battle.terrainAt({row, col}) == TerrainType::Shallows) return true;
    return false;
}

// 毒牙: range-1 STR+3 physical attack on `target`, poisoning it unless
// already poisoned (docs: "すでに毒状態なら追加ダメージを増やさない" - no
// stacking, just skip the re-application).
Unit* performSerpentVenomBite(BattleState& battle, Unit& serpent, Unit& target) {
    const bool hit = battle.rollAttackHit(target);
    resolveAttack(battle, serpent, target, battle.combatDefenseBonus(target, serpent), hit, kSerpentVenomBonus);
    if (hit && target.isAlive() && target.poisonRemainingProcs <= 0) applyPoison(target);
    return &target;
}

// 締め付け: front-3 pattern, STR+1, Move Down with no stacking on a target
// that already has it (docs: "同じ対象へ効果量を重複させない").
Unit* performSerpentConstrict(BattleState& battle, Unit& serpent) {
    std::vector<Unit*> targets = serpentConstrictTargets(battle, serpent);
    if (targets.empty()) return nullptr;
    const AliveSnapshot aliveBefore = captureAliveSnapshot(battle);
    for (Unit* target : targets) {
        const bool hit = battle.rollAttackHit(*target);
        resolveAttack(battle, serpent, *target, battle.combatDefenseBonus(*target, serpent), hit,
                      kSerpentConstrictBonus);
        if (hit && target->isAlive() && !target->moveDownActive) applyMoveDown(battle, *target);
    }
    emitUnitDefeatedEvents(battle, aliveBefore);
    return targets.front();
}

// 激しい身震い: HP<=50%, once, knocks back all 4 orthogonally-adjacent
// units 1 tile (BattleState::applyKnockback(), same mechanic every other
// knockback source already uses). Markers/poison-pool tie-ins are deferred
// (see this function's own header comment above).
void triggerSerpentShudder(BattleState& battle, Unit& serpent) {
    std::vector<Unit*> adjacent;
    for (const GridPos& delta : {GridPos{-1, 0}, GridPos{1, 0}, GridPos{0, -1}, GridPos{0, 1}}) {
        GridPos pos{serpent.position.row + delta.row, serpent.position.col + delta.col};
        if (!isInBounds(pos)) continue;
        if (Unit* occupant = battle.unitAt(pos)) adjacent.push_back(occupant);
    }
    for (Unit* unit : adjacent) battle.applyKnockback(serpent, *unit);
}

// Picks up to 2 reachable tiles adjacent to the nearest living player as
// 水中潜行's telegraphed candidate destinations (row-major tie-break for
// determinism, mirroring the boar/grubworm's own deterministic scans).
std::vector<GridPos> computeSerpentSubmergeCandidates(BattleState& battle, const Unit& serpent) {
    Unit* target = findNearestPlayer(battle, serpent);
    if (!target) return {};
    std::vector<GridPos> reachable = computeReachableTiles(battle, serpent);
    std::vector<GridPos> candidates;
    for (const GridPos& delta : {GridPos{-1, 0}, GridPos{1, 0}, GridPos{0, -1}, GridPos{0, 1}}) {
        GridPos pos{target->position.row + delta.row, target->position.col + delta.col};
        if (!isInBounds(pos) || pos == serpent.position) continue;
        if (battle.unitAt(pos)) continue;
        if (std::find(reachable.begin(), reachable.end(), pos) == reachable.end()) continue;
        candidates.push_back(pos);
        if (candidates.size() >= 2) break;
    }
    return candidates;
}

Unit* takeSerpentBossTurn(BattleState& battle, Unit& serpent) {
    // 1. A telegraphed submerge always executes now: move to the first
    // candidate tile (deterministic - the doc's own player-visible "予告"
    // is a UI concern the AI doesn't need a live choice for), then attack
    // one adjacent unit if any is in range.
    if (serpent.bossRuntime.telegraph.pending()) {
        const std::vector<GridPos>& tiles = serpent.bossRuntime.telegraph.lockedTiles;
        if (!tiles.empty()) battle.moveUnit(serpent, tiles.front());
        serpent.chargeTelegraphed = false;
        serpent.bossRuntime.telegraph.state = TelegraphState::Executed;
        handleObjectiveEvent(battle.missionState(),
                             {battle.issueEventId(), 0,
                              BossTelegraphChangedEvent{serpent.id, serpent.bossRuntime.telegraph.actionId, false}});
        serpent.bossRuntime.telegraph.clear();
        serpent.chargeCooldownActions = 1;
        const AliveSnapshot aliveBefore = captureAliveSnapshot(battle);
        if (Unit* target = adjacentPlayer(battle, serpent)) performSerpentVenomBite(battle, serpent, *target);
        emitUnitDefeatedEvents(battle, aliveBefore);
        finishEnemyAction(battle, serpent, ActionKind::Attack);
        return nullptr;
    }

    const bool submergeOnCooldown = serpent.chargeCooldownActions > 0;
    if (submergeOnCooldown) --serpent.chargeCooldownActions;

    // 2. HP<=50%: 激しい身震い, once, instant (mirrors bossEnraged/
    // bossCollapseUsed's "checked before this turn's decision" pattern).
    if (!serpent.bossShudderUsed && serpent.currentHp * 2 <= serpent.stats.maxHp) {
        serpent.bossShudderUsed = true;
        triggerSerpentShudder(battle, serpent);
        serpent.bossRuntime.stageIndex = 1;
        handleObjectiveEvent(battle.missionState(),
                             BattleEvent{battle.issueEventId(), 0,
                                         BossStageChangedEvent{serpent.id, serpent.bossRuntime.stageIndex}});
    }

    // 3. Attack an ally currently occupying a marker tile (approximated as
    // "any adjacent player standing on a Container-kind Object", the shape
    // every surveyTileObjectDefinitionId marker takes - see BattleFactory.
    // cpp's survey-tile placement).
    for (const GridPos& delta : {GridPos{-1, 0}, GridPos{1, 0}, GridPos{0, -1}, GridPos{0, 1}}) {
        GridPos pos{serpent.position.row + delta.row, serpent.position.col + delta.col};
        if (!isInBounds(pos)) continue;
        Unit* occupant = battle.unitAt(pos);
        if (!occupant || occupant->team != Team::Player || !occupant->isAlive()) continue;
        if (!battle.objectAt(pos)) continue;
        const AliveSnapshot aliveBefore = captureAliveSnapshot(battle);
        performSerpentVenomBite(battle, serpent, *occupant);
        emitUnitDefeatedEvents(battle, aliveBefore);
        finishEnemyAction(battle, serpent, ActionKind::Attack);
        return occupant;
    }

    // 4. Constrict whenever 2+ players are adjacent.
    if (countAdjacentPlayers(battle, serpent) >= 2) {
        if (Unit* hit = performSerpentConstrict(battle, serpent)) {
            finishEnemyAction(battle, serpent, ActionKind::Attack);
            return hit;
        }
    }

    // 5. Venom bite an adjacent, not-yet-poisoned target.
    for (const GridPos& delta : {GridPos{-1, 0}, GridPos{1, 0}, GridPos{0, -1}, GridPos{0, 1}}) {
        GridPos pos{serpent.position.row + delta.row, serpent.position.col + delta.col};
        if (!isInBounds(pos)) continue;
        Unit* occupant = battle.unitAt(pos);
        if (!occupant || occupant->team != Team::Player || !occupant->isAlive()) continue;
        if (occupant->poisonRemainingProcs > 0) continue;
        const AliveSnapshot aliveBefore = captureAliveSnapshot(battle);
        performSerpentVenomBite(battle, serpent, *occupant);
        emitUnitDefeatedEvents(battle, aliveBefore);
        finishEnemyAction(battle, serpent, ActionKind::Attack);
        return occupant;
    }

    // 6. Telegraph a submerge toward an isolated target if Shallows terrain
    // exists anywhere on the board (see this function's header comment).
    if (!submergeOnCooldown && anyShallowsOnBoard(battle)) {
        std::vector<GridPos> candidates = computeSerpentSubmergeCandidates(battle, serpent);
        if (!candidates.empty()) {
            serpent.chargeTelegraphed = true;
            serpent.bossRuntime.telegraph = {"deep_mire_submerge", TelegraphShape::Area, TelegraphState::Announced,
                                             battle.round(), battle.round() + 1, {}, candidates, 0};
            handleObjectiveEvent(battle.missionState(),
                                 {battle.issueEventId(), 0,
                                  BossTelegraphChangedEvent{serpent.id, "deep_mire_submerge", true}});
            finishEnemyAction(battle, serpent, ActionKind::Skill);
            return nullptr;
        }
    }

    // 7. Otherwise, close the distance toward the nearest player (the doc's
    // "水源から3マス以内を維持" leash is deferred - no persistent "水源"
    // marker position is threaded into EnemyAI.cpp anywhere, the same
    // category of gap as the boar/grubworm not tracking a home tile either).
    Unit* target = findNearestPlayer(battle, serpent);
    bool moved = false;
    if (target) {
        std::vector<GridPos> reachable = computeReachableTiles(battle, serpent);
        GridPos bestTile = serpent.position;
        int bestDist = manhattanDistance(serpent.position, target->position);
        for (const GridPos& tile : reachable) {
            int dist = manhattanDistance(tile, target->position);
            if (dist < bestDist) {
                bestDist = dist;
                bestTile = tile;
            }
        }
        if (bestTile != serpent.position) moved = battle.moveUnit(serpent, bestTile);
    }

    const AliveSnapshot aliveBeforeAttack = captureAliveSnapshot(battle);
    if (Unit* attacked = attackIfPossible(battle, serpent, target)) {
        emitUnitDefeatedEvents(battle, aliveBeforeAttack);
        finishEnemyAction(battle, serpent, ActionKind::Attack);
        return attacked;
    }
    finishEnemyAction(battle, serpent, moved ? ActionKind::Move : ActionKind::Wait);
    return nullptr;
}

// docs/regions/windscar_plateau.md「地域ボス 高原運び手の隊長」.
constexpr int kCourierEscapeMoveBonus = 1;
constexpr int kCourierEscapeDefensePenalty = 2;
constexpr int kCourierLeashRadius = 4;
constexpr int kCourierPositioningRadius = 3;
// 行動優先順位6「伝令所から3マス以内で側面位置を取る」/退路確保「伝令所から
// 4マスを超えて離れない」: plateau_relay is authored directly in
// data/regions.json with no per-battle marker Object threaded into
// EnemyAI.cpp for "伝令所"'s own board position (M9-K's own 水源3マス leash
// note already recorded this exact "no persistent marker position" gap for
// a prior boss), so this hardcodes the fixed reference tile this stage's
// layout places it at, rather than adding new generic Object-position
// plumbing for a single boss ability.
constexpr GridPos kPlateauRelayStationTile{2, kGridCols - 1};

// 通り抜け攻撃「敵から離れる方向へ最大2マス再移動」: BattleState::moveUnit()
// already checks exactly the doc's "実際の通行可否は見るがZoC・Unit・通行不能
// 地形回避ルールは無視する" shape (destination terrain/Object/occupant only,
// no path or ZoC check) - the same direct-teleport call the serpent's
// submerge and grubworm's tunnel already use for their own ZoC-ignoring
// repositions, so this needs no new movement primitive.
void performCourierRepositionAwayFromTarget(BattleState& battle, Unit& captain, const Unit& target) {
    int dirRow = captain.position.row - target.position.row;
    int dirCol = captain.position.col - target.position.col;
    dirRow = dirRow > 0 ? 1 : (dirRow < 0 ? -1 : 0);
    dirCol = dirCol > 0 ? 1 : (dirCol < 0 ? -1 : 0);
    if (dirRow == 0 && dirCol == 0) return;
    for (int distance = 2; distance >= 1; --distance) {
        GridPos candidate{captain.position.row + dirRow * distance, captain.position.col + dirCol * distance};
        if (battle.moveUnit(captain, candidate)) return;
    }
}

// 通り抜け攻撃「直線で2マス以上移動した後だけ使用可能」: scans reachable
// tiles (leash-filtered while 退路確保 is active this same action, per its
// own "伝令所から4マスを超えて離れない") for one that is (a) melee range of a
// living player and (b) reached by a straight (same row or same column) move
// of at least 2 tiles from the captain's current position, preferring the
// lowest-HP such target (mirrors 行動優先順位4's "低HP対象を攻撃"). Falls
// back to nullptr (no reposition happens, caller keeps trying lower-priority
// steps) if no such tile/target pair exists - "再移動先がなければ通常攻撃
// だけ行う" only applies once a target is already committed to, which this
// selection guarantees by construction.
Unit* performCourierPassThroughStrike(BattleState& battle, Unit& captain, bool leashActive) {
    std::vector<GridPos> reachable = computeReachableTiles(battle, captain);
    Unit* best = nullptr;
    GridPos bestTile{};
    int bestHp = INT_MAX;
    for (Unit& target : battle.units()) {
        if (target.team != Team::Player || !target.isAlive()) continue;
        for (const GridPos& tile : reachable) {
            if (leashActive && manhattanDistance(tile, kPlateauRelayStationTile) > kCourierLeashRadius) continue;
            if (manhattanDistance(tile, target.position) != 1) continue;
            const bool sameRow = tile.row == captain.position.row;
            const bool sameCol = tile.col == captain.position.col;
            if (!sameRow && !sameCol) continue;
            if (manhattanDistance(captain.position, tile) < 2) continue;
            if (target.currentHp < bestHp) {
                bestHp = target.currentHp;
                best = &target;
                bestTile = tile;
            }
        }
    }
    if (!best) return nullptr;
    battle.moveUnit(captain, bestTile);
    triggerRangerTrapIfPresent(battle, captain);
    const AliveSnapshot aliveBefore = captureAliveSnapshot(battle);
    attackIfPossible(battle, captain, best, /*onlyPreferred=*/true);
    emitUnitDefeatedEvents(battle, aliveBefore);
    if (best->isAlive()) performCourierRepositionAwayFromTarget(battle, captain, *best);
    return best;
}

// 迂回命令 resolution: 行動優先順位1「予告済み迂回命令を解決」. Reduced-scope
// approximation (see this function's header comment on takeCourierCaptainBossTurn) -
// this project has no cross-unit squad-AI influence mechanism (every other
// boss's telegraph only ever affects the boss's own next action), so "騎兵
// 1体と弓兵1体が...優先" becomes "the captain itself prioritizes a player in
// the announced row". "対象不在なら通常AIへ戻り、無料の追加攻撃は発生しない"
// is honored literally: returns nullptr and lets the caller fall through to
// the rest of this same turn's priority list instead of ending the action.
Unit* resolveCourierFlankingOrder(BattleState& battle, Unit& captain) {
    const std::vector<GridPos> tiles = captain.bossRuntime.telegraph.lockedTiles;
    captain.chargeTelegraphed = false;
    captain.bossRuntime.telegraph.state = TelegraphState::Executed;
    handleObjectiveEvent(battle.missionState(),
                         {battle.issueEventId(), 0,
                          BossTelegraphChangedEvent{captain.id, captain.bossRuntime.telegraph.actionId, false}});
    captain.bossRuntime.telegraph.clear();

    Unit* target = nullptr;
    int bestDist = INT_MAX;
    for (Unit& unit : battle.units()) {
        if (unit.team != Team::Player || !unit.isAlive()) continue;
        bool inRow = false;
        for (const GridPos& tile : tiles) inRow = inRow || unit.position.row == tile.row;
        if (!inRow) continue;
        const int dist = manhattanDistance(captain.position, unit.position);
        if (dist < bestDist) {
            bestDist = dist;
            target = &unit;
        }
    }
    if (!target) return nullptr;

    std::vector<GridPos> reachable = computeReachableTiles(battle, captain);
    GridPos bestTile = captain.position;
    int bestScore = manhattanDistance(captain.position, target->position);
    for (const GridPos& tile : reachable) {
        const int score = manhattanDistance(tile, target->position);
        if (score < bestScore) {
            bestScore = score;
            bestTile = tile;
        }
    }
    if (bestTile != captain.position) {
        battle.moveUnit(captain, bestTile);
        triggerRangerTrapIfPresent(battle, captain);
    }
    const AliveSnapshot aliveBefore = captureAliveSnapshot(battle);
    Unit* hit = attackIfPossible(battle, captain, target);
    emitUnitDefeatedEvents(battle, aliveBefore);
    return hit;
}

// docs/regions/windscar_plateau.md「地域ボス 高原運び手の隊長」「行動優先
// 順位」. 主目的は本Sliceの近似(標準EliminateTeam、地点データのコメント参照)
// によりコースが存在しないため、doc手順3「伝令箱へ到達可能な味方伝令を妨害」
// は今回スコープ外(伝令ゲスト自体が未配線)。それ以外の5手順を、
// takeGrubwormBossTurn()/takeSerpentBossTurn()と同じ早期return連鎖で実装。
Unit* takeCourierCaptainBossTurn(BattleState& battle, Unit& captain) {
    // 1. Resolve a pending 迂回命令 telegraph first, always.
    if (captain.bossRuntime.telegraph.pending()) {
        if (Unit* hit = resolveCourierFlankingOrder(battle, captain)) {
            finishEnemyAction(battle, captain, ActionKind::Attack);
            return hit;
        }
        // No valid target in the announced row: falls back to normal AI
        // this same turn, no free extra attack (falls through below).
    }

    // 2. HP<=50%, one-time: 退路確保. "MOV+1/DEF-2まで行動終了" is applied for
    // exactly this action's own decisions below and reverted before this
    // function returns (see escapeRouteActive's two revert sites) rather
    // than persisted across turns, matching "次の行動終了まで" literally.
    bool escapeRouteActive = false;
    if (!captain.bossEscapeRouteUsed && captain.currentHp * 2 <= captain.stats.maxHp) {
        captain.bossEscapeRouteUsed = true;
        escapeRouteActive = true;
        captain.stats.move += kCourierEscapeMoveBonus;
        captain.stats.defense = std::max(captain.stats.defense - kCourierEscapeDefensePenalty, 0);
        captain.bossRuntime.stageIndex = 1;
        handleObjectiveEvent(battle.missionState(),
                             BattleEvent{battle.issueEventId(), 0,
                                         BossStageChangedEvent{captain.id, captain.bossRuntime.stageIndex}});
    }
    auto revertEscapeRoute = [&]() {
        if (!escapeRouteActive) return;
        captain.stats.move -= kCourierEscapeMoveBonus;
        captain.stats.defense += kCourierEscapeDefensePenalty;
    };

    // 4. 通り抜け攻撃 on the lowest-HP target it can reach with a straight
    // 2+-tile move and safely reposition away from afterward.
    // [M9-S balance tune] Reuses the generic chargeCooldownActions field
    // (same field the boar/grubworm/serpent bosses use for their own
    // telegraphed dash/submerge cooldowns) to put this action on a 1-turn
    // cooldown after each use. Without this, the captain could reposition to
    // a counter-safe tile and land a free unanswered hit every single turn
    // for the whole fight - a structural AI advantage the other 2 working
    // region bosses don't have (their analogous burst actions are each
    // gated by their own telegraph/cooldown already). This is the smallest
    // available fix once enemy-roster trimming alone proved insufficient
    // (see docs/implementation_status.md M9-S) - it does not touch the
    // action's damage, targeting, or reposition logic, only how often it
    // can fire.
    const bool passThroughOnCooldown = captain.chargeCooldownActions > 0;
    if (passThroughOnCooldown) --captain.chargeCooldownActions;
    if (!passThroughOnCooldown) {
        if (Unit* hit = performCourierPassThroughStrike(battle, captain, escapeRouteActive)) {
            captain.chargeCooldownActions = 1;
            revertEscapeRoute();
            finishEnemyAction(battle, captain, ActionKind::Attack);
            return hit;
        }
    }

    // 5. Telegraph 迂回命令 if not yet used this battle: announce whichever
    // half (top rows 0..kGridRows/2-1, bottom the rest) currently holds more
    // living players (ties favor top, a stable deterministic choice).
    if (!captain.bossFlankUsed) {
        int topCount = 0, bottomCount = 0;
        for (const Unit& unit : battle.units()) {
            if (unit.team != Team::Player || !unit.isAlive()) continue;
            if (unit.position.row < kGridRows / 2) ++topCount; else ++bottomCount;
        }
        const bool announceTop = topCount >= bottomCount;
        std::vector<GridPos> tiles;
        const int rowStart = announceTop ? 0 : kGridRows / 2;
        const int rowEnd = announceTop ? kGridRows / 2 : kGridRows;
        for (int row = rowStart; row < rowEnd; ++row)
            for (int col = 0; col < kGridCols; ++col) tiles.push_back(GridPos{row, col});

        captain.bossFlankUsed = true;
        captain.chargeTelegraphed = true;
        captain.bossRuntime.telegraph = {"plateau_relay_flanking_order", TelegraphShape::Area,
                                         TelegraphState::Announced, battle.round(), battle.round() + 1,
                                         {}, tiles, 0};
        handleObjectiveEvent(battle.missionState(),
                             {battle.issueEventId(), 0,
                              BossTelegraphChangedEvent{captain.id, "plateau_relay_flanking_order", true}});
        revertEscapeRoute();
        finishEnemyAction(battle, captain, ActionKind::Skill);
        return nullptr;
    }

    // 6. Otherwise, take a flanking position within kCourierPositioningRadius
    // of the relay station (leash-filtered to kCourierLeashRadius while
    // 退路確保 is active), attacking anyone already in range along the way.
    std::vector<GridPos> reachable = computeReachableTiles(battle, captain);
    GridPos bestTile = captain.position;
    int bestScore = manhattanDistance(captain.position, kPlateauRelayStationTile);
    for (const GridPos& tile : reachable) {
        if (escapeRouteActive && manhattanDistance(tile, kPlateauRelayStationTile) > kCourierLeashRadius) continue;
        const int score = manhattanDistance(tile, kPlateauRelayStationTile);
        if (score < bestScore) {
            bestScore = score;
            bestTile = tile;
        }
    }
    bool moved = false;
    if (bestTile != captain.position) {
        moved = battle.moveUnit(captain, bestTile);
        if (moved) triggerRangerTrapIfPresent(battle, captain);
    }
    const AliveSnapshot aliveBeforeAttack = captureAliveSnapshot(battle);
    if (Unit* attacked = attackIfPossible(battle, captain, findNearestPlayer(battle, captain))) {
        emitUnitDefeatedEvents(battle, aliveBeforeAttack);
        revertEscapeRoute();
        finishEnemyAction(battle, captain, ActionKind::Attack);
        return attacked;
    }
    revertEscapeRoute();
    finishEnemyAction(battle, captain, moved ? ActionKind::Move : ActionKind::Wait);
    return nullptr;
}

// docs/regions/ember_ravine.md「地域ボス 赤背の大蜥蜴」/「行動優先順位」.
// Structurally closest to takeGrubwormBossTurn() (telegraphed straight-line
// charge + one-time HP<=50% terrain-mutation ability), per this Slice's own
// plan. Approximated relative to the doc the same way M9-D/K/Q scoped down
// their own bosses (see this function's own comments below for each spot):
// - 冷却回避 (AI pathing preference to avoid CoolFloor tiles unless no other
//   path exists) is a pathing-weight nuance, not a discrete ability -
//   AiSystem.cpp's move-candidate scoring has no terrain-type-aware
//   preference hook for any unit anywhere in the engine, and this function's
//   own fallback movement (mirroring every other boss's) doesn't add one
//   just for this boss. Deferred as a no-op, documented per the plan's own
//   guidance that this is a minor flavor nuance, not core to the fight.
// - 行動優先順位 step 3「冷却弁の操作者を攻撃」is approximated the same way
//   takeSerpentBossTurn()'s own step 3 approximates "水源標識の操作者" -
//   "any adjacent player currently standing on ANY Battle Object", since
//   this engine has no per-Object "is this specifically a cooling valve"
//   query independent of definitionId string comparison, and the existing
//   precedent already treats "standing on an Object" as the generic marker-
//   operator signal.
// - 行動優先順位 step 5「突進可能な孤立対象へ予告」reuses
//   grubwormChargeDirectionForTarget()'s own lowest-HP-then-nearest scan
//   without an additional "isolated" (no allies adjacent) filter, the same
//   simplification the grubworm/serpent's own charge-target selection makes
//   (neither models isolation either).
// - 行動優先順位 step 6「封鎖扉から3マス以内を維持」reuses the same hardcoded
//   fixed-reference-tile leash `kRedheatFissureGateTile` that
//   kPlateauRelayStationTile established for 高原運び手の隊長's own leash -
//   no persistent Object-position tracking exists for "封鎖扉" as placed by
//   this stage's data-authored layout, so a fixed tile stands in for it.
constexpr int kLizardChargeRange = 3;
constexpr int kLizardChargePowerBonus = 3;
constexpr int kLizardSweepPowerBonus = 1;
constexpr GridPos kRedheatFissureGateTile{2, kGridCols - 1};
constexpr int kLizardLeashRadius = 3;

// Mirrors computeGrubwormChargeTiles(): side-effect-free preview of
// executeLizardCharge()'s walk, for BossTelegraph::lockedTiles.
std::vector<GridPos> computeLizardChargeTiles(const BattleState& battle, const Unit& lizard, int direction,
                                              int range) {
    std::vector<GridPos> tiles;
    const int row = lizard.position.row;
    for (int step = 1; step <= range; ++step) {
        int col = lizard.position.col + direction * step;
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

// Mirrors grubwormChargeDirectionForTarget(): lowest HP, then nearest, then ID.
int lizardChargeDirectionForTarget(BattleState& battle, const Unit& lizard, int range) {
    const Unit* best = nullptr;
    int bestDistance = range + 1;
    for (const Unit& unit : battle.units()) {
        if (unit.team != Team::Player || !unit.isAlive() || unit.position.row != lizard.position.row) continue;
        const int distance = std::abs(unit.position.col - lizard.position.col);
        if (distance == 0 || distance > range) continue;
        if (!best || unit.currentHp < best->currentHp ||
            (unit.currentHp == best->currentHp && distance < bestDistance) ||
            (unit.currentHp == best->currentHp && distance == bestDistance && unit.id < best->id)) {
            best = &unit;
            bestDistance = distance;
        }
    }
    if (!best) return 0;
    return best->position.col < lizard.position.col ? -1 : 1;
}

// 熱砂突進: advances up to kLizardChargeRange tiles along the current row,
// damaging (STR+3, per the doc) every player unit passed over, stopping at
// the board edge or a movement-blocking Battle Object.
void executeLizardCharge(BattleState& battle, Unit& lizard) {
    const int power = lizard.stats.strength + kLizardChargePowerBonus;
    const int row = lizard.position.row;
    const int direction = lizard.bossRuntime.telegraph.direction < 0 ? -1 : 1;
    int endCol = lizard.position.col;

    for (int step = 1; step <= kLizardChargeRange; ++step) {
        int col = lizard.position.col + direction * step;
        if (col < 0 || col >= kGridCols) break;
        GridPos pos{row, col};
        endCol = col;

        Unit* occupant = battle.unitAt(pos);
        if (occupant && occupant->team == Team::Player && occupant->isAlive()) {
            int damage = std::max(power - occupant->effectiveDefense(), 1);
            occupant->currentHp = std::max(occupant->currentHp - damage, 0);
        }

        const BattleObjectState* object = battle.objectAt(pos);
        if (object && object->state != BattleObjectStateKind::Destroyed) {
            const BattleObjectDefinition* def = battle.objectDefinition(object->definitionId);
            if (def && def->blocksMovement) break;
        }
    }

    lizard.position = GridPos{row, endCol};
    lizard.chargeTelegraphed = false;
    lizard.bossRuntime.telegraph.state = TelegraphState::Executed;
    handleObjectiveEvent(battle.missionState(),
                         {battle.issueEventId(), 0,
                          BossTelegraphChangedEvent{lizard.id, lizard.bossRuntime.telegraph.actionId, false}});
    lizard.bossRuntime.telegraph.clear();
    lizard.chargeCooldownActions = 1;
    lizard.chargesExecuted += 1;
}

// 尾払い's front-3 pattern: identical shape to boarSweepTargets()/
// serpentConstrictTargets() (the column immediately toward the player side).
std::vector<Unit*> lizardTailSweepTargets(BattleState& battle, const Unit& lizard) {
    std::vector<Unit*> targets;
    int col = lizard.position.col - 1;
    if (col < 0) return targets;
    for (int row = lizard.position.row - 1; row <= lizard.position.row + 1; ++row) {
        GridPos pos{row, col};
        if (!isInBounds(pos)) continue;
        Unit* occupant = battle.unitAt(pos);
        if (occupant && occupant->team == Team::Player && occupant->isAlive()) targets.push_back(occupant);
    }
    return targets;
}

// 尾払い: front-3 pattern, STR+1 physical (boar-sweep-style flat damage, no
// accuracy roll, mirroring performBoarSweep()'s own shape), then a 1-tile
// knockback (BattleState::applyKnockback(), the same mechanic every other
// knockback source in this engine already uses) on any target still alive.
Unit* performLizardTailSweep(BattleState& battle, Unit& lizard) {
    std::vector<Unit*> targets = lizardTailSweepTargets(battle, lizard);
    if (targets.empty()) return nullptr;
    const int power = lizard.stats.strength + kLizardSweepPowerBonus;
    const AliveSnapshot aliveBefore = captureAliveSnapshot(battle);
    for (Unit* target : targets) {
        const int damage = std::max(power - target->effectiveDefense(), 1);
        target->currentHp = std::max(target->currentHp - damage, 0);
        if (target->isAlive()) battle.applyKnockback(lizard, *target);
    }
    emitUnitDefeatedEvents(battle, aliveBefore);
    return targets.front();
}

// 噴気誘導: HP<=50%, once, immediately converts up to 2 currently-empty,
// unoccupied Ember/HotSand/Floor tiles (excluding the boss's own tile) to
// FumeWarning - reuses TerrainType::FumeWarning exactly as this region's
// own resolveEmberHeatRoundStart() (src/battle/BattleState.cpp) already
// does, so the existing resolveEmberFumeRoundEnd() will convert these to
// FireFloor at the next Round End with no further code needed here. Scan
// order is fixed (row-major) for determinism, mirroring
// triggerGrubwormCollapse()'s own choice. The doc's own "1ラウンド前に予告"
// framing is for 熱砂突進's telegraph, not this ability - 噴気誘導 itself is
// a one-time board-state change like 崩落誘発, so this fires immediately
// (same approximation M9-D's own grubworm collapse recorded).
void triggerLizardFumeLure(BattleState& battle, Unit& lizard) {
    int converted = 0;
    for (int row = 0; row < kGridRows && converted < 2; ++row) {
        for (int col = 0; col < kGridCols && converted < 2; ++col) {
            GridPos pos{row, col};
            if (pos == lizard.position) continue;
            TerrainType terrain = battle.terrainAt(pos);
            if (terrain != TerrainType::Floor && terrain != TerrainType::EmberFloor &&
                terrain != TerrainType::HotSand) {
                continue;
            }
            if (battle.unitAt(pos)) continue;
            if (battle.objectAt(pos)) continue;
            battle.setTerrain(pos, TerrainType::FumeWarning);
            ++converted;
        }
    }
}

Unit* takeRedbackLizardBossTurn(BattleState& battle, Unit& lizard) {
    // 1. A telegraphed charge always executes now, before anything else.
    if (lizard.bossRuntime.telegraph.pending()) {
        executeLizardCharge(battle, lizard);
        finishEnemyAction(battle, lizard, ActionKind::Attack);
        return nullptr;
    }

    const bool chargeOnCooldown = lizard.chargeCooldownActions > 0;
    if (chargeOnCooldown) --lizard.chargeCooldownActions;

    // 2. 噴気誘導: instant, non-turn-consuming state update, checked before
    // this turn's action decision (mirrors the grubworm/serpent's own
    // HP<=50% one-time checks).
    if (!lizard.bossFumeLureUsed && lizard.currentHp * 2 <= lizard.stats.maxHp) {
        lizard.bossFumeLureUsed = true;
        triggerLizardFumeLure(battle, lizard);
        lizard.bossRuntime.stageIndex = 1;
        handleObjectiveEvent(battle.missionState(),
                             BattleEvent{battle.issueEventId(), 0,
                                         BossStageChangedEvent{lizard.id, lizard.bossRuntime.stageIndex}});
    }

    // 3. Attack an ally currently occupying/adjacent to a Battle Object (see
    // this function's own header comment on the "冷却弁の操作者" approximation).
    for (const GridPos& delta : {GridPos{-1, 0}, GridPos{1, 0}, GridPos{0, -1}, GridPos{0, 1}}) {
        GridPos pos{lizard.position.row + delta.row, lizard.position.col + delta.col};
        if (!isInBounds(pos)) continue;
        Unit* occupant = battle.unitAt(pos);
        if (!occupant || occupant->team != Team::Player || !occupant->isAlive()) continue;
        if (!battle.objectAt(pos)) continue;
        const AliveSnapshot aliveBefore = captureAliveSnapshot(battle);
        attackIfPossible(battle, lizard, occupant);
        emitUnitDefeatedEvents(battle, aliveBefore);
        finishEnemyAction(battle, lizard, ActionKind::Attack);
        return occupant;
    }

    // 4. 尾払い whenever 2+ players are in the front-3 pattern.
    if (lizardTailSweepTargets(battle, lizard).size() >= 2) {
        if (Unit* hit = performLizardTailSweep(battle, lizard)) {
            finishEnemyAction(battle, lizard, ActionKind::Attack);
            return hit;
        }
    }

    // 5. Telegraph a charge if a target is reachable along the current row.
    if (!chargeOnCooldown) {
        const int direction = lizardChargeDirectionForTarget(battle, lizard, kLizardChargeRange);
        if (direction != 0) {
            lizard.chargeTelegraphed = true;
            lizard.chargeDirection = direction;
            lizard.bossRuntime.telegraph = {"redback_lizard_charge", TelegraphShape::Line, TelegraphState::Announced,
                                            battle.round(), battle.round() + 1, {},
                                            computeLizardChargeTiles(battle, lizard, direction, kLizardChargeRange),
                                            direction};
            handleObjectiveEvent(battle.missionState(),
                                 {battle.issueEventId(), 0,
                                  BossTelegraphChangedEvent{lizard.id, "redback_lizard_charge", true}});
            finishEnemyAction(battle, lizard, ActionKind::Skill);
            return nullptr;
        }
    }

    // 6. Otherwise, maintain position near 封鎖扉 (leash, see this function's
    // own header comment) while closing distance toward the nearest player,
    // then attack normally.
    Unit* target = findNearestPlayer(battle, lizard);
    bool moved = false;
    if (target) {
        std::vector<GridPos> reachable = computeReachableTiles(battle, lizard);
        GridPos bestTile = lizard.position;
        int bestDist = manhattanDistance(lizard.position, target->position);
        bool leashSatisfiedAtCurrent = manhattanDistance(lizard.position, kRedheatFissureGateTile) <= kLizardLeashRadius;
        for (const GridPos& tile : reachable) {
            const bool withinLeash = manhattanDistance(tile, kRedheatFissureGateTile) <= kLizardLeashRadius;
            if (!withinLeash && leashSatisfiedAtCurrent) continue;
            int dist = manhattanDistance(tile, target->position);
            if (dist < bestDist) {
                bestDist = dist;
                bestTile = tile;
            }
        }
        if (bestTile != lizard.position) moved = battle.moveUnit(lizard, bestTile);
    }

    const AliveSnapshot aliveBeforeAttack = captureAliveSnapshot(battle);
    if (Unit* attacked = attackIfPossible(battle, lizard, target)) {
        emitUnitDefeatedEvents(battle, aliveBeforeAttack);
        finishEnemyAction(battle, lizard, ActionKind::Attack);
        return attacked;
    }
    finishEnemyAction(battle, lizard, moved ? ActionKind::Move : ActionKind::Wait);
    return nullptr;
}

// docs/regions/mapped_edge.md「最終戦「地図外縁」」's 環境波「普通の大型獣1体」:
// "直線突進を1Round前に予告するが、撃破は不要". The simplest of this
// project's 6 telegraphed-charge enemies - no enrage stage (unlike
// takeBoarBossTurn), no secondary attack pattern (unlike the boar's sweep or
// the lizard's tail-sweep), no leash (unlike takeRedbackLizardBossTurn's
// kRedheatFissureGateTile), and no stun-on-collision side effect (the doc
// never mentions one for this beast, unlike the boar's log-collision stun) -
// "普通の大型獣" (an ordinary beast) reads as this single mechanic, not a
// scaled-down copy of an existing boss's full kit. "撃破は不要" is NOT
// modeled here at all (no retreatHpPercent tuning, no invulnerability) -
// this project already gets that property for free at the objective layer
// (mappedEdgeFinalStage()'s primary has no EliminateTeam member, so the
// beast can be fought, ignored, or killed with zero effect on Victory/
// Defeat either way).
constexpr int kFrontierBeastChargeRange = 3;
constexpr int kFrontierBeastChargePowerBonus = 3;

// Mirrors boarChargeDirectionForTarget(): lowest-HP, then nearest, then ID
// tie-break, along the beast's current row only.
int frontierBeastChargeDirectionForTarget(BattleState& battle, const Unit& beast, int range) {
    const Unit* best = nullptr;
    int bestDistance = range + 1;
    for (const Unit& unit : battle.units()) {
        if (unit.team != Team::Player || !unit.isAlive() || unit.position.row != beast.position.row) continue;
        const int distance = std::abs(unit.position.col - beast.position.col);
        if (distance == 0 || distance > range) continue;
        if (!best || unit.currentHp < best->currentHp ||
            (unit.currentHp == best->currentHp && distance < bestDistance) ||
            (unit.currentHp == best->currentHp && distance == bestDistance && unit.id < best->id)) {
            best = &unit;
            bestDistance = distance;
        }
    }
    if (!best) return 0;
    return best->position.col < beast.position.col ? -1 : 1;
}

// Mirrors computeBoarChargeTiles(): side-effect-free preview for
// BossTelegraph::lockedTiles, stopping at the board edge or a movement-
// blocking Battle Object (not destroying it - unlike the boar, this beast
// has no documented log-collision interaction).
std::vector<GridPos> computeFrontierBeastChargeTiles(const BattleState& battle, const Unit& beast, int direction,
                                                     int range) {
    std::vector<GridPos> tiles;
    const int row = beast.position.row;
    for (int step = 1; step <= range; ++step) {
        int col = beast.position.col + direction * step;
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

// Executes a telegraphed charge along the beast's row: damages every player
// unit passed over, stops at a movement-blocking Battle Object (without
// destroying it - no documented log-collision mechanic for this beast) or
// the board edge.
void executeFrontierBeastCharge(BattleState& battle, Unit& beast) {
    const int range = kFrontierBeastChargeRange;
    const int power = beast.stats.strength + kFrontierBeastChargePowerBonus;
    const int row = beast.position.row;
    const int direction = beast.bossRuntime.telegraph.direction < 0 ? -1 : 1;
    int endCol = beast.position.col;

    for (int step = 1; step <= range; ++step) {
        int col = beast.position.col + direction * step;
        if (col < 0 || col >= kGridCols) break;
        GridPos pos{row, col};
        endCol = col;

        Unit* occupant = battle.unitAt(pos);
        if (occupant && occupant->team == Team::Player && occupant->isAlive()) {
            int damage = std::max(power - occupant->effectiveDefense(), 1);
            occupant->currentHp = std::max(occupant->currentHp - damage, 0);
        }

        const BattleObjectState* object = battle.objectAt(pos);
        if (object && object->state != BattleObjectStateKind::Destroyed) {
            const BattleObjectDefinition* def = battle.objectDefinition(object->definitionId);
            if (def && def->blocksMovement) break;
        }
    }

    beast.position = GridPos{row, endCol};
    beast.chargeTelegraphed = false;
    beast.chargeDirection = -1;
    beast.bossRuntime.telegraph.state = TelegraphState::Executed;
    handleObjectiveEvent(battle.missionState(),
                         {battle.issueEventId(), 0,
                          BossTelegraphChangedEvent{beast.id, beast.bossRuntime.telegraph.actionId, false}});
    beast.bossRuntime.telegraph.clear();
    beast.chargeCooldownActions = 1;
    ++beast.chargesExecuted;
}

Unit* takeFrontierBeastBossTurn(BattleState& battle, Unit& beast) {
    // 1. A telegraphed charge always executes now, before anything else.
    if (beast.bossRuntime.telegraph.pending()) {
        executeFrontierBeastCharge(battle, beast);
        finishEnemyAction(battle, beast, ActionKind::Attack);
        return nullptr;
    }

    const bool chargeOnCooldown = beast.chargeCooldownActions > 0;
    if (chargeOnCooldown) --beast.chargeCooldownActions;

    // 2. Telegraph a charge if a target is reachable along the current row.
    if (!chargeOnCooldown) {
        const int direction = frontierBeastChargeDirectionForTarget(battle, beast, kFrontierBeastChargeRange);
        if (direction != 0) {
            beast.chargeTelegraphed = true;
            beast.chargeDirection = direction;
            beast.bossRuntime.telegraph = {
                "frontier_beast_charge", TelegraphShape::Line, TelegraphState::Announced, battle.round(),
                battle.round() + 1, {},
                computeFrontierBeastChargeTiles(battle, beast, direction, kFrontierBeastChargeRange), direction};
            handleObjectiveEvent(battle.missionState(),
                                 {battle.issueEventId(), 0,
                                  BossTelegraphChangedEvent{beast.id, "frontier_beast_charge", true}});
            finishEnemyAction(battle, beast, ActionKind::Skill);
            return nullptr;
        }
    }

    // 3. Otherwise, close the distance toward the nearest player and attack
    // normally - "人間のObjectiveは理解しない" reads as this beast having no
    // Objective-aware targeting logic (marker-placer priority etc.), just
    // the same generic nearest-target behavior as any plain enemy.
    Unit* target = findNearestPlayer(battle, beast);
    bool moved = false;
    if (target) {
        std::vector<GridPos> reachable = computeReachableTiles(battle, beast);
        GridPos bestTile = beast.position;
        int bestDist = manhattanDistance(beast.position, target->position);
        for (const GridPos& tile : reachable) {
            int dist = manhattanDistance(tile, target->position);
            if (dist < bestDist) {
                bestDist = dist;
                bestTile = tile;
            }
        }
        if (bestTile != beast.position) moved = battle.moveUnit(beast, bestTile);
    }

    const AliveSnapshot aliveBeforeAttack = captureAliveSnapshot(battle);
    if (Unit* attacked = attackIfPossible(battle, beast, target)) {
        emitUnitDefeatedEvents(battle, aliveBeforeAttack);
        finishEnemyAction(battle, beast, ActionKind::Attack);
        return attacked;
    }
    finishEnemyAction(battle, beast, moved ? ActionKind::Move : ActionKind::Wait);
    return nullptr;
}

} // namespace

Unit* takeEnemyTurn(BattleState& battle, Unit& enemy, AiSquadReservations* reservations) {
    if (!enemy.isPresent() || enemy.hasActed) return nullptr;
    if (enemy.unitClass == UnitClass::AshenhornBoar) return takeBoarBossTurn(battle, enemy);
    if (enemy.unitClass == UnitClass::AshironGrubworm) return takeGrubwormBossTurn(battle, enemy);
    if (enemy.unitClass == UnitClass::MarshFangSerpent) return takeSerpentBossTurn(battle, enemy);
    if (enemy.unitClass == UnitClass::PlateauCourierCaptain) return takeCourierCaptainBossTurn(battle, enemy);
    if (enemy.unitClass == UnitClass::RedbackLizard) return takeRedbackLizardBossTurn(battle, enemy);
    if (enemy.unitClass == UnitClass::FrontierBeast) return takeFrontierBeastBossTurn(battle, enemy);

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
        if (candidate.destination != enemy.position) {
            battle.moveUnit(enemy, candidate.destination);
            triggerRangerTrapIfPresent(battle, enemy);
        }
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
    triggerRangerTrapIfPresent(battle, enemy);

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
