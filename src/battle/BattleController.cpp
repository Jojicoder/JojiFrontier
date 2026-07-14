#include "jf/battle/BattleController.hpp"

#include "jf/battle/EnemyAI.hpp"
#include "jf/battle/Movement.hpp"
#include "jf/battle/SkillCharges.hpp"
#include "jf/battle/StatusEffects.hpp"

#include <algorithm>
#include <utility>

namespace jf {

namespace {
constexpr float kEnemyActionDelay = 0.6f;
}

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
