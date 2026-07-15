#include "jf/battle/BattleController.hpp"

#include "jf/battle/EnemyAI.hpp"
#include "jf/battle/Movement.hpp"
#include "jf/battle/SkillCharges.hpp"
#include "jf/battle/StatusEffects.hpp"

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace jf {

namespace {
constexpr float kEnemyActionDelay = 0.6f;

// docs/implementation_roadmap.md M4-A: reusable equipped-skill effect shapes
// (docs/initial_skill_effects.md). A new skill that matches one of these
// shapes only needs a new table row here, not new branching logic in
// chooseSkill()/selectSkillTarget() - see the shared usage of each table
// below. Skills whose shape doesn't fit any of these yet (reactive skills,
// terrain-cost overrides, self-only movement, conditional triggers) still
// need dedicated code when they're implemented.

struct HealSkillShape {
    int amount = 0;
    int range = 1;
    bool requiresBelowHalfHp = false;
};
const std::unordered_map<std::string, HealSkillShape>& healSkillShapes() {
    static const std::unordered_map<std::string, HealSkillShape> table = {
        {"emergency_treatment", {.amount = 12, .range = 2, .requiresBelowHalfHp = true}},
    };
    return table;
}

// Self or one adjacent ally, clears every status effect - only 1 skill uses
// this shape today (no parameters to vary), so it's a plain id check rather
// than a table.
bool isCleanseShape(const std::string& skillId) { return skillId == "cleanse"; }

// 行軍隊長`advance_order`(前進命令): 隣接する未行動味方1人(自身除く)、MOV+1、
// このPlayer Phase終了まで。1つだけ既存の形状テーブルと食い違う点が多い(自身除外・
// 未行動限定という追加のターゲット条件、かつ次のEnemy Phase終了ではなく"この"Player
// Phase終了で切れる)ため、パラメータを増やしてbuffSkillShapesへ押し込むよりcleanse同様
// 専用分岐にした方が読みやすいと判断した。
bool isAdvanceOrderShape(const std::string& skillId) { return skillId == "advance_order"; }

struct AttackSkillShape {
    int bonusDamage = 0;
    bool appliesMoveDown = false;
    bool requiresUnacted = false; // 未行動の敵限定 (辺境斥候「奇襲」)
};
const std::unordered_map<std::string, AttackSkillShape>& attackSkillShapes() {
    static const std::unordered_map<std::string, AttackSkillShape> table = {
        {"suppressing_shot", {.appliesMoveDown = true}},
        {"halting_thrust", {.appliesMoveDown = true}},
        {"ambush", {.bonusDamage = 3, .requiresUnacted = true}},
    };
    return table;
}

// 監視弓兵`mark_target`(標的指定)/行軍隊長`support_order`(援護命令): neither
// attacks - both just set Unit::markedBonusDamage on a target (positive =
// vulnerability on an enemy, negative = a damage-reduction shield on an
// ally), consumed by CombatResolver.cpp the moment a real hit lands.
// `targetsAlly` picks which side chooseSkill()'s targeting rule uses (敵1体・
// 武器射程 vs 隣接味方1人).
struct MarkSkillShape {
    int bonusDamage = 2;
    bool targetsAlly = false;
};
const std::unordered_map<std::string, MarkSkillShape>& markSkillShapes() {
    static const std::unordered_map<std::string, MarkSkillShape> table = {
        {"mark_target", {.bonusDamage = 2}},
        {"support_order", {.bonusDamage = -3, .targetsAlly = true}},
    };
    return table;
}

enum class BuffKind { Resistance, Defense, ZocRange };
struct BuffSkillShape {
    BuffKind kind = BuffKind::Resistance;
    bool selfAndAllAdjacent = false; // true = self+every adjacent ally, resolves with no target selection
    bool selfOnly = false;           // true = self alone, also resolves with no target selection
};
const std::unordered_map<std::string, BuffSkillShape>& buffSkillShapes() {
    static const std::unordered_map<std::string, BuffSkillShape> table = {
        {"protective_treatment", {.kind = BuffKind::Resistance}},
        {"hold_formation", {.kind = BuffKind::Defense, .selfAndAllAdjacent = true}},
        {"extended_lockdown", {.kind = BuffKind::ZocRange, .selfOnly = true}},
    };
    return table;
}

void applyBuff(BuffKind kind, Unit& target) {
    switch (kind) {
        case BuffKind::Resistance: applyResistanceUp(target); return;
        case BuffKind::Defense: applyDefenseUp(target); return;
        case BuffKind::ZocRange: applyZocRangeExtension(target); return;
    }
}

} // namespace

BattleController::BattleController(BattleState battle) : battle_(std::move(battle)) {
    // Every unit starts a fresh battle with full skill charges (docs/
    // skill_system.md), regardless of which slots (if any) are equipped.
    for (Unit& unit : battle_.units()) initializeSkillCharges(unit);
}

std::optional<CombatPreview> BattleController::pendingPreview() const {
    if (inputState_ != BattleInputState::ConfirmAttack || !selectedUnit_ || !pendingTarget_) {
        return std::nullopt;
    }
    return jf::previewAttack(*selectedUnit_, *pendingTarget_,
                             battle_.combatDefenseBonus(*pendingTarget_, *selectedUnit_),
                             battle_.combatHitChance(*pendingTarget_));
}

void BattleController::finishPlayerAction(Unit& unit, ActionKind actionKind) {
    // Snapshot before the terrain/status resolution below so a self-defeat
    // from burn fires UnitDefeatedEvent exactly once (any combat-caused
    // defeat was already captured and emitted by the caller, e.g.
    // confirmAttack(), before finishPlayerAction() runs).
    const AliveSnapshot aliveBefore = captureAliveSnapshot(battle_);

    // docs/status_effects.md 地形効果の処理順: terrain heal before the
    // unit's own status-effect action-end damage.
    battle_.consumeHerbPatch(unit);
    processActionEndStatusEffects(battle_, unit);
    battle_.markActed(unit);
    emitUnitDefeatedEvents(battle_, aliveBefore);

    const ActionId actionId = nextActionId_++;
    BattleEvent event{battle_.issueEventId(), actionId,
                      ActionResolvedEvent{actionId, unit.id, unit.team, actionKind, unit.position}};
    handleObjectiveEvent(battle_.missionState(), event);
}

void BattleController::selectUnit(Unit& unit) {
    if (inputState_ != BattleInputState::SelectUnit) return;
    if (unit.team != Team::Player || !unit.isAlive() || unit.hasActed) return;

    selectedUnit_ = &unit;
    moveOrigin_ = unit.position;
    reachableTiles_ = computeReachableTiles(battle_, unit);
    attackRangeTiles_ = computeAttackRangeTiles(unit, reachableTiles_);
    inputState_ = BattleInputState::SelectMove;
}

void BattleController::selectMoveTile(GridPos pos) {
    if (inputState_ != BattleInputState::SelectMove || !selectedUnit_) return;

    bool isReachable = false;
    for (const GridPos& tile : reachableTiles_) {
        if (tile == pos) {
            isReachable = true;
            break;
        }
    }
    if (!isReachable) {
        // Shortcut: clicking an enemy directly (instead of a move tile)
        // attacks it immediately from the unit's current position, if it's
        // already in range, without requiring Move-in-place -> Attack ->
        // pick-target as separate steps.
        Unit* target = battle_.unitAt(pos);
        if (target && target->isAlive() && target->team != selectedUnit_->team) {
            int dist = manhattanDistance(selectedUnit_->position, pos);
            if (dist >= selectedUnit_->weapon.minRange && dist <= selectedUnit_->weapon.maxRange) {
                reachableTiles_.clear();
                attackRangeTiles_ = computeAttackRangeTiles(*selectedUnit_, {selectedUnit_->position});
                targetableTiles_ = computeTargetableTiles(battle_.units(), *selectedUnit_, selectedUnit_->position);
                pendingTarget_ = target;
                inputState_ = BattleInputState::ConfirmAttack;
            }
        }
        return;
    }

    battle_.moveUnit(*selectedUnit_, pos);
    reachableTiles_.clear();
    attackRangeTiles_ = computeAttackRangeTiles(*selectedUnit_, {selectedUnit_->position});
    inputState_ = BattleInputState::SelectAction;
}

void BattleController::returnToMoveSelection() {
    if (inputState_ != BattleInputState::SelectAction || !selectedUnit_) return;

    if (!battle_.moveUnit(*selectedUnit_, moveOrigin_)) return;
    reachableTiles_ = computeReachableTiles(battle_, *selectedUnit_);
    attackRangeTiles_ = computeAttackRangeTiles(*selectedUnit_, reachableTiles_);
    targetableTiles_.clear();
    pendingTarget_ = nullptr;
    inputState_ = BattleInputState::SelectMove;
}

void BattleController::chooseAttack() {
    if (inputState_ != BattleInputState::SelectAction || !selectedUnit_) return;

    targetableTiles_ = computeTargetableTiles(battle_.units(), *selectedUnit_, selectedUnit_->position);
    if (targetableTiles_.empty()) return;

    inputState_ = BattleInputState::SelectTarget;
}

void BattleController::chooseHeal() {
    if (inputState_ != BattleInputState::SelectAction || !selectedUnit_ ||
        !canHeal(selectedUnit_->unitClass)) return;

    healableTiles_.clear();
    for (const Unit& unit : battle_.units()) {
        if (!unit.isAlive() || unit.team != selectedUnit_->team ||
            unit.currentHp >= unit.stats.maxHp) continue;
        if (manhattanDistance(selectedUnit_->position, unit.position) <= 1)
            healableTiles_.push_back(unit.position);
    }
    if (!healableTiles_.empty()) inputState_ = BattleInputState::SelectHealTarget;
}

void BattleController::selectHealTarget(GridPos pos) {
    if (inputState_ != BattleInputState::SelectHealTarget || !selectedUnit_) return;
    if (std::find(healableTiles_.begin(), healableTiles_.end(), pos) == healableTiles_.end()) return;

    Unit* target = battle_.unitAt(pos);
    if (!target || target->team != selectedUnit_->team) return;
    target->currentHp = std::min(target->currentHp + 8, target->stats.maxHp);
    finishPlayerAction(*selectedUnit_, ActionKind::Skill);
    selectedUnit_ = nullptr;
    healableTiles_.clear();
    reachableTiles_.clear();
    attackRangeTiles_.clear();
    inputState_ = BattleInputState::SelectUnit;
    evaluateOutcome();
}

bool BattleController::useHealingItem(int amount) {
    if (inputState_ != BattleInputState::SelectAction || !selectedUnit_ || amount <= 0) return false;
    if (selectedUnit_->currentHp >= selectedUnit_->stats.maxHp) return false;

    selectedUnit_->currentHp = std::min(selectedUnit_->currentHp + amount, selectedUnit_->stats.maxHp);
    finishPlayerAction(*selectedUnit_, ActionKind::Item);
    selectedUnit_ = nullptr;
    reachableTiles_.clear();
    targetableTiles_.clear();
    attackRangeTiles_.clear();
    inputState_ = BattleInputState::SelectUnit;
    evaluateOutcome();
    return true;
}

bool BattleController::chooseHealingItemTarget(int amount) {
    if (inputState_ != BattleInputState::SelectUnit || amount <= 0) return false;
    itemTargetTiles_.clear();
    for (const Unit& unit : battle_.units()) {
        if (unit.team == Team::Player && unit.isAlive() && !unit.hasActed &&
            unit.currentHp < unit.stats.maxHp) {
            itemTargetTiles_.push_back(unit.position);
        }
    }
    if (itemTargetTiles_.empty()) return false;
    pendingHealingItemAmount_ = amount;
    inputState_ = BattleInputState::SelectItemTarget;
    return true;
}

bool BattleController::selectHealingItemTarget(GridPos pos) {
    if (inputState_ != BattleInputState::SelectItemTarget || pendingHealingItemAmount_ <= 0 ||
        std::find(itemTargetTiles_.begin(), itemTargetTiles_.end(), pos) == itemTargetTiles_.end()) return false;
    Unit* target = battle_.unitAt(pos);
    if (!target || target->team != Team::Player || target->hasActed) return false;
    target->currentHp = std::min(target->currentHp + pendingHealingItemAmount_, target->stats.maxHp);
    finishPlayerAction(*target, ActionKind::Item);
    itemTargetTiles_.clear();
    pendingHealingItemAmount_ = 0;
    inputState_ = BattleInputState::SelectUnit;
    evaluateOutcome();
    return true;
}

std::vector<SkillAvailability> BattleController::selectedUnitSkills() const {
    if (!selectedUnit_) return {};
    return availableSkills(*selectedUnit_);
}

void BattleController::chooseSkill(int slotIndex) {
    if (inputState_ != BattleInputState::SelectAction || !selectedUnit_) return;
    if (slotIndex < 0 || slotIndex >= static_cast<int>(selectedUnit_->skillSlots.size())) return;
    if (!skillSlotAvailable(*selectedUnit_, slotIndex)) return;
    const std::string& skillId = selectedUnit_->skillSlots[static_cast<std::size_t>(slotIndex)].skillId;

    // Self-cast buffs with no target to choose (e.g. 行軍隊長`hold_formation`'s
    // self+every adjacent ally, or 古参守備兵`extended_lockdown`'s self-only)
    // resolve immediately.
    if (auto buff = buffSkillShapes().find(skillId);
        buff != buffSkillShapes().end() && (buff->second.selfAndAllAdjacent || buff->second.selfOnly)) {
        if (buff->second.selfOnly) {
            applyBuff(buff->second.kind, *selectedUnit_);
        } else {
            for (Unit& unit : battle_.units()) {
                if (unit.team != selectedUnit_->team || !unit.isAlive()) continue;
                if (&unit == selectedUnit_ || manhattanDistance(selectedUnit_->position, unit.position) <= 1)
                    applyBuff(buff->second.kind, unit);
            }
        }
        consumeSkillCharge(*selectedUnit_, slotIndex);
        finishPlayerAction(*selectedUnit_, ActionKind::Skill);
        selectedUnit_ = nullptr;
        reachableTiles_.clear();
        attackRangeTiles_.clear();
        inputState_ = BattleInputState::SelectUnit;
        evaluateOutcome();
        return;
    }

    skillTargetTiles_.clear();
    // docs/initial_skill_effects.md: only the skills registered in the
    // shape tables above (plus cleanse) have an implemented effect so far
    // (M4-A's executor slice) - every other equipped skill id falls through
    // to "no targets", the same no-op chooseAttack()/chooseHeal() give when
    // nothing qualifies.
    if (auto heal = healSkillShapes().find(skillId); heal != healSkillShapes().end()) {
        for (Unit& unit : battle_.units()) {
            if (unit.team != selectedUnit_->team || !unit.isAlive()) continue;
            if (heal->second.requiresBelowHalfHp && unit.currentHp * 2 > unit.stats.maxHp) continue;
            if (manhattanDistance(selectedUnit_->position, unit.position) <= heal->second.range)
                skillTargetTiles_.push_back(unit.position);
        }
    } else if (isCleanseShape(skillId)) {
        // 自身または隣接味方1人の状態異常をすべて解除 - targetable regardless
        // of whether the target actually has an active status, same as a
        // heal item can target an ally already at full HP.
        for (Unit& unit : battle_.units()) {
            if (unit.team != selectedUnit_->team || !unit.isAlive()) continue;
            if (manhattanDistance(selectedUnit_->position, unit.position) <= 1)
                skillTargetTiles_.push_back(unit.position);
        }
    } else if (auto attack = attackSkillShapes().find(skillId); attack != attackSkillShapes().end()) {
        // Same targeting rule as a normal attack (computeTargetableTiles
        // already restricts to the opposing team), optionally narrowed to
        // units that haven't acted yet (辺境斥候「奇襲」's 未行動の敵限定).
        for (GridPos pos : computeTargetableTiles(battle_.units(), *selectedUnit_, selectedUnit_->position)) {
            const Unit* candidate = battle_.unitAt(pos);
            if (attack->second.requiresUnacted && candidate && candidate->hasActed) continue;
            skillTargetTiles_.push_back(pos);
        }
    } else if (auto buff = buffSkillShapes().find(skillId); buff != buffSkillShapes().end()) {
        // Single-target buffs (selfAndAllAdjacent == false; the AoE case
        // already returned above) - self or one adjacent ally.
        for (Unit& unit : battle_.units()) {
            if (unit.team != selectedUnit_->team || !unit.isAlive()) continue;
            if (manhattanDistance(selectedUnit_->position, unit.position) <= 1)
                skillTargetTiles_.push_back(unit.position);
        }
    } else if (auto mark = markSkillShapes().find(skillId); mark != markSkillShapes().end()) {
        if (mark->second.targetsAlly) {
            // 行軍隊長`support_order`: 隣接味方1人(自身は対象外) - unlike the
            // self-inclusive ally-targeting buffs above.
            for (Unit& unit : battle_.units()) {
                if (&unit == selectedUnit_ || unit.team != selectedUnit_->team || !unit.isAlive()) continue;
                if (manhattanDistance(selectedUnit_->position, unit.position) <= 1)
                    skillTargetTiles_.push_back(unit.position);
            }
        } else {
            // 監視弓兵`mark_target`: 敵1体・武器射程 - same target rule as an
            // attack skill, minus the 未行動限定 option no Mark skill needs.
            skillTargetTiles_ = computeTargetableTiles(battle_.units(), *selectedUnit_, selectedUnit_->position);
        }
    } else if (isAdvanceOrderShape(skillId)) {
        for (Unit& unit : battle_.units()) {
            if (&unit == selectedUnit_ || unit.team != selectedUnit_->team || !unit.isAlive() || unit.hasActed)
                continue;
            if (manhattanDistance(selectedUnit_->position, unit.position) <= 1)
                skillTargetTiles_.push_back(unit.position);
        }
    }
    if (skillTargetTiles_.empty()) return;
    pendingSkillSlot_ = slotIndex;
    inputState_ = BattleInputState::SelectSkillTarget;
}

bool BattleController::selectSkillTarget(GridPos pos) {
    if (inputState_ != BattleInputState::SelectSkillTarget || !selectedUnit_ || pendingSkillSlot_ < 0) return false;
    if (std::find(skillTargetTiles_.begin(), skillTargetTiles_.end(), pos) == skillTargetTiles_.end()) return false;
    Unit* target = battle_.unitAt(pos);
    if (!target) return false;

    const std::string& skillId = selectedUnit_->skillSlots[static_cast<std::size_t>(pendingSkillSlot_)].skillId;
    if (auto heal = healSkillShapes().find(skillId); heal != healSkillShapes().end()) {
        target->currentHp = std::min(target->currentHp + heal->second.amount, target->stats.maxHp);
    } else if (isCleanseShape(skillId)) {
        target->poisonRemainingProcs = 0;
        target->burnRemainingProcs = 0;
        target->moveDownActive = false;
        target->defenseDownActive = false;
        target->staggerActive = false;
    } else if (auto attack = attackSkillShapes().find(skillId); attack != attackSkillShapes().end()) {
        lastAttacker_ = selectedUnit_;
        lastAttackTarget_ = target;
        ++attackEventId_;
        const AliveSnapshot aliveBeforeAttack = captureAliveSnapshot(battle_);
        const int hpBeforeAttack = target->currentHp;
        const bool hit = battle_.rollAttackHit(*target);
        resolveAttack(*selectedUnit_, *target, battle_.combatDefenseBonus(*target, *selectedUnit_), hit);
        if (hit && attack->second.bonusDamage > 0 && target->isAlive())
            target->currentHp = std::max(0, target->currentHp - attack->second.bonusDamage);
        lastDamage_ = std::max(0, hpBeforeAttack - target->currentHp);
        lastAttackHit_ = lastDamage_ > 0;
        if (hit && attack->second.appliesMoveDown && target->isAlive()) applyMoveDown(*target);
        emitUnitDefeatedEvents(battle_, aliveBeforeAttack);
    } else if (auto buff = buffSkillShapes().find(skillId); buff != buffSkillShapes().end()) {
        applyBuff(buff->second.kind, *target);
    } else if (auto mark = markSkillShapes().find(skillId); mark != markSkillShapes().end()) {
        target->markedBonusDamage = mark->second.bonusDamage;
    } else if (isAdvanceOrderShape(skillId)) {
        applyMoveUp(*target);
    }
    consumeSkillCharge(*selectedUnit_, pendingSkillSlot_);
    finishPlayerAction(*selectedUnit_, ActionKind::Skill);
    selectedUnit_ = nullptr;
    pendingSkillSlot_ = -1;
    skillTargetTiles_.clear();
    reachableTiles_.clear();
    attackRangeTiles_.clear();
    inputState_ = BattleInputState::SelectUnit;
    evaluateOutcome();
    return true;
}

void BattleController::chooseProtectiveBoard() {
    if (inputState_ != BattleInputState::SelectAction || !selectedUnit_) return;
    boardTargetTiles_.clear();
    constexpr GridPos directions[] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (GridPos direction : directions) {
        GridPos pos{selectedUnit_->position.row + direction.row, selectedUnit_->position.col + direction.col};
        if (isInBounds(pos) && !battle_.unitAt(pos) && isPassable(battle_.terrainAt(pos)))
            boardTargetTiles_.push_back(pos);
    }
    if (!boardTargetTiles_.empty()) inputState_ = BattleInputState::SelectBoardTarget;
}

bool BattleController::selectBoardTarget(GridPos pos) {
    if (inputState_ != BattleInputState::SelectBoardTarget || !selectedUnit_ ||
        std::find(boardTargetTiles_.begin(), boardTargetTiles_.end(), pos) == boardTargetTiles_.end()) return false;
    battle_.setTerrain(pos, TerrainType::Barrier);
    finishPlayerAction(*selectedUnit_, ActionKind::Item);
    selectedUnit_ = nullptr;
    boardTargetTiles_.clear();
    attackRangeTiles_.clear();
    inputState_ = BattleInputState::SelectUnit;
    evaluateOutcome();
    return true;
}

void BattleController::chooseWait() {
    if (inputState_ != BattleInputState::SelectAction || !selectedUnit_) return;

    finishPlayerAction(*selectedUnit_, ActionKind::Wait);
    selectedUnit_ = nullptr;
    reachableTiles_.clear();
    attackRangeTiles_.clear();
    inputState_ = BattleInputState::SelectUnit;
    evaluateOutcome();
}

void BattleController::endPlayerTurn() {
    if (inputState_ != BattleInputState::SelectUnit) return;
    for (Unit& unit : battle_.units()) {
        if (unit.team == Team::Player && unit.isAlive()) battle_.markActed(unit);
    }
    evaluateOutcome();
}

void BattleController::selectTargetTile(GridPos pos) {
    if (inputState_ != BattleInputState::SelectTarget) return;

    bool isTargetable = false;
    for (const GridPos& tile : targetableTiles_) {
        if (tile == pos) {
            isTargetable = true;
            break;
        }
    }
    if (!isTargetable) return;

    pendingTarget_ = battle_.unitAt(pos);
    if (!pendingTarget_) return;
    inputState_ = BattleInputState::ConfirmAttack;
}

void BattleController::cancelAttackSelection() {
    if (inputState_ == BattleInputState::SelectItemTarget) {
        itemTargetTiles_.clear();
        pendingHealingItemAmount_ = 0;
        inputState_ = BattleInputState::SelectUnit;
        return;
    }
    if ((inputState_ != BattleInputState::SelectTarget &&
         inputState_ != BattleInputState::SelectHealTarget &&
         inputState_ != BattleInputState::SelectItemTarget &&
         inputState_ != BattleInputState::SelectBoardTarget &&
         inputState_ != BattleInputState::SelectSkillTarget &&
         inputState_ != BattleInputState::ConfirmAttack) ||
        !selectedUnit_) {
        return;
    }

    pendingTarget_ = nullptr;
    targetableTiles_.clear();
    healableTiles_.clear();
    itemTargetTiles_.clear();
    pendingHealingItemAmount_ = 0;
    boardTargetTiles_.clear();
    skillTargetTiles_.clear();
    pendingSkillSlot_ = -1;
    inputState_ = BattleInputState::SelectAction;
}

void BattleController::confirmAttack() {
    if (inputState_ != BattleInputState::ConfirmAttack || !selectedUnit_ || !pendingTarget_) return;

    lastAttacker_ = selectedUnit_;
    lastAttackTarget_ = pendingTarget_;
    ++attackEventId_;
    const AliveSnapshot aliveBeforeAttack = captureAliveSnapshot(battle_);
    const int hpBeforeAttack = pendingTarget_->currentHp;
    const bool hit = battle_.rollAttackHit(*pendingTarget_);
    resolveAttack(*selectedUnit_, *pendingTarget_,
                  battle_.combatDefenseBonus(*pendingTarget_, *selectedUnit_), hit);
    lastDamage_ = std::max(0, hpBeforeAttack - pendingTarget_->currentHp);
    lastAttackHit_ = lastDamage_ > 0;
    if (hit && selectedUnit_->weapon.causesKnockback && pendingTarget_->isAlive())
        battle_.applyKnockback(*selectedUnit_, *pendingTarget_);
    emitUnitDefeatedEvents(battle_, aliveBeforeAttack);
    finishPlayerAction(*selectedUnit_, ActionKind::Attack);

    selectedUnit_ = nullptr;
    pendingTarget_ = nullptr;
    reachableTiles_.clear();
    targetableTiles_.clear();
    attackRangeTiles_.clear();
    healableTiles_.clear();
    boardTargetTiles_.clear();
    inputState_ = BattleInputState::SelectUnit;
    evaluateOutcome();
}

void BattleController::cancelToUnitSelect() {
    if (inputState_ == BattleInputState::EnemyTurn ||
        inputState_ == BattleInputState::Victory ||
        inputState_ == BattleInputState::Defeat) {
        return;
    }
    selectedUnit_ = nullptr;
    pendingTarget_ = nullptr;
    reachableTiles_.clear();
    targetableTiles_.clear();
    attackRangeTiles_.clear();
    healableTiles_.clear();
    itemTargetTiles_.clear();
    pendingHealingItemAmount_ = 0;
    inputState_ = BattleInputState::SelectUnit;
}

Unit* BattleController::nextUnactedEnemy() {
    for (auto& u : battle_.units()) {
        if (u.team == Team::Enemy && u.isAlive() && !u.hasActed) return &u;
    }
    return nullptr;
}

void BattleController::evaluateOutcome() {
    // docs/mission_objectives.md 進捗同期: sync Completed/Superseded once per
    // Batch before evaluating, so evaluateBattleOutcome() itself never
    // mutates progress.
    syncObjectiveProgress(battle_);
    // docs/mission_objectives.md 判定順序: defeat takes priority over
    // victory even if both became true from the same batch of changes.
    const BattleOutcome outcome = evaluateBattleOutcome(battle_);
    if (outcome.kind == BattleOutcomeKind::Defeat) {
        // 状態異常は戦闘終了時にすべて解除する (docs/status_effects.md).
        if (inputState_ != BattleInputState::Defeat) clearAllStatusEffects(battle_);
        inputState_ = BattleInputState::Defeat;
        return;
    }
    if (outcome.kind == BattleOutcomeKind::Victory) {
        if (inputState_ != BattleInputState::Victory) clearAllStatusEffects(battle_);
        inputState_ = BattleInputState::Victory;
        return;
    }
    if (inputState_ == BattleInputState::SelectUnit && battle_.isTeamDone(Team::Player)) {
        // docs/mission_objectives.md Phase終了処理順: resolve Phase-end
        // status effects (and any resulting defeat) before PhaseEnded fires,
        // not after.
        const AliveSnapshot aliveBeforePhaseEnd = captureAliveSnapshot(battle_);
        processPhaseEndStatusEffects(battle_, Team::Player);
        clearMoveUpAtPlayerPhaseEnd(battle_);
        emitUnitDefeatedEvents(battle_, aliveBeforePhaseEnd);
        handleObjectiveEvent(battle_.missionState(),
                             {battle_.issueEventId(), 0, PhaseEndedEvent{Phase::PlayerPhase, battle_.round()}});
        battle_.beginEnemyPhase();
        // Enemy Phase is starting: refill/tick down enemy-side skill charges
        // (docs/skill_system.md "使用制限").
        refreshSkillChargesOnPhaseStart(battle_, Team::Enemy);
        handleObjectiveEvent(battle_.missionState(),
                             {battle_.issueEventId(), 0, PhaseStartedEvent{Phase::EnemyPhase, battle_.round()}});
        inputState_ = BattleInputState::EnemyTurn;
        enemyActionTimer_ = 0.0f;
    }
}

void BattleController::update(float dt) {
    if (inputState_ != BattleInputState::EnemyTurn) return;

    enemyActionTimer_ -= dt;
    if (enemyActionTimer_ > 0.0f) return;

    Unit* next = nextUnactedEnemy();
    if (next) {
        // EnemyAI doesn't report hit/damage directly, so snapshot every
        // player's HP first and diff afterward against whichever one (if
        // any) it says was attacked - avoids widening EnemyAI's interface
        // just to plumb this through.
        std::vector<std::pair<Unit*, int>> hpBefore;
        for (Unit& unit : battle_.units()) {
            if (unit.team == Team::Player) hpBefore.push_back({&unit, unit.currentHp});
        }
        if (Unit* attacked = takeEnemyTurn(battle_, *next)) {
            lastAttacker_ = next;
            lastAttackTarget_ = attacked;
            ++attackEventId_;
            int before = 0;
            for (const auto& [unit, hp] : hpBefore) {
                if (unit == attacked) {
                    before = hp;
                    break;
                }
            }
            lastDamage_ = std::max(0, before - attacked->currentHp);
            lastAttackHit_ = lastDamage_ > 0;
        }
        enemyActionTimer_ = kEnemyActionDelay;

        syncObjectiveProgress(battle_);
        const BattleOutcome outcome = evaluateBattleOutcome(battle_);
        if (outcome.kind == BattleOutcomeKind::Defeat) {
            clearAllStatusEffects(battle_);
            inputState_ = BattleInputState::Defeat;
            return;
        }
        if (outcome.kind == BattleOutcomeKind::Victory) {
            clearAllStatusEffects(battle_);
            inputState_ = BattleInputState::Victory;
            return;
        }
        return;
    }

    // Enemy Phase is ending: resolve status effects (and any resulting
    // defeat) before PhaseEnded/RoundEnded fire (docs/mission_objectives.md
    // Phase終了処理順), then control passes back to the player.
    const AliveSnapshot aliveBeforePhaseEnd = captureAliveSnapshot(battle_);
    processPhaseEndStatusEffects(battle_, Team::Enemy);
    clearSkillBuffsAtEnemyPhaseEnd(battle_);
    emitUnitDefeatedEvents(battle_, aliveBeforePhaseEnd);
    handleObjectiveEvent(battle_.missionState(),
                         {battle_.issueEventId(), 0, PhaseEndedEvent{Phase::EnemyPhase, battle_.round()}});
    // docs/mission_objectives.md: RoundEnded fires only when Enemy Phase ends.
    handleObjectiveEvent(battle_.missionState(), {battle_.issueEventId(), 0, RoundEndedEvent{battle_.round()}});
    battle_.beginPlayerPhase();
    // Player Phase is starting: refill/tick down player-side skill charges.
    refreshSkillChargesOnPhaseStart(battle_, Team::Player);
    handleObjectiveEvent(battle_.missionState(),
                         {battle_.issueEventId(), 0, PhaseStartedEvent{Phase::PlayerPhase, battle_.round()}});
    selectedUnit_ = nullptr;
    inputState_ = BattleInputState::SelectUnit;
}

} // namespace jf
