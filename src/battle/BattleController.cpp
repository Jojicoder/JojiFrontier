#include "jf/battle/BattleController.hpp"

#include "jf/battle/EnemyAI.hpp"
#include "jf/battle/Movement.hpp"

#include <algorithm>

namespace jf {

namespace {
constexpr float kEnemyActionDelay = 0.6f;
}

BattleController::BattleController(BattleState battle) : battle_(std::move(battle)) {}

std::optional<CombatPreview> BattleController::pendingPreview() const {
    if (inputState_ != BattleInputState::ConfirmAttack || !selectedUnit_ || !pendingTarget_) {
        return std::nullopt;
    }
    return jf::previewAttack(*selectedUnit_, *pendingTarget_,
                             battle_.combatDefenseBonus(*pendingTarget_, *selectedUnit_));
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
    battle_.markActed(*selectedUnit_);
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
    battle_.markActed(*selectedUnit_);
    selectedUnit_ = nullptr;
    reachableTiles_.clear();
    targetableTiles_.clear();
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
    battle_.markActed(*selectedUnit_);
    selectedUnit_ = nullptr;
    boardTargetTiles_.clear();
    attackRangeTiles_.clear();
    inputState_ = BattleInputState::SelectUnit;
    evaluateOutcome();
    return true;
}

void BattleController::chooseWait() {
    if (inputState_ != BattleInputState::SelectAction || !selectedUnit_) return;

    battle_.markActed(*selectedUnit_);
    selectedUnit_ = nullptr;
    reachableTiles_.clear();
    attackRangeTiles_.clear();
    inputState_ = BattleInputState::SelectUnit;
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
    if ((inputState_ != BattleInputState::SelectTarget &&
         inputState_ != BattleInputState::SelectHealTarget &&
         inputState_ != BattleInputState::SelectBoardTarget &&
         inputState_ != BattleInputState::ConfirmAttack) ||
        !selectedUnit_) {
        return;
    }

    pendingTarget_ = nullptr;
    targetableTiles_.clear();
    healableTiles_.clear();
    boardTargetTiles_.clear();
    inputState_ = BattleInputState::SelectAction;
}

void BattleController::confirmAttack() {
    if (inputState_ != BattleInputState::ConfirmAttack || !selectedUnit_ || !pendingTarget_) return;

    resolveAttack(*selectedUnit_, *pendingTarget_,
                  battle_.combatDefenseBonus(*pendingTarget_, *selectedUnit_));
    battle_.markActed(*selectedUnit_);

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
    inputState_ = BattleInputState::SelectUnit;
}

Unit* BattleController::nextUnactedEnemy() {
    for (auto& u : battle_.units()) {
        if (u.team == Team::Enemy && u.isAlive() && !u.hasActed) return &u;
    }
    return nullptr;
}

void BattleController::evaluateOutcome() {
    if (battle_.allEnemiesDefeated()) {
        inputState_ = BattleInputState::Victory;
        return;
    }
    if (battle_.allPlayersDefeated()) {
        inputState_ = BattleInputState::Defeat;
        return;
    }
    if (inputState_ == BattleInputState::SelectUnit && battle_.isTeamDone(Team::Player)) {
        battle_.beginEnemyPhase();
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
        takeEnemyTurn(battle_, *next);
        enemyActionTimer_ = kEnemyActionDelay;

        if (battle_.allPlayersDefeated()) {
            inputState_ = BattleInputState::Defeat;
            return;
        }
        if (battle_.allEnemiesDefeated()) {
            inputState_ = BattleInputState::Victory;
            return;
        }
        return;
    }

    battle_.beginPlayerPhase();
    selectedUnit_ = nullptr;
    inputState_ = BattleInputState::SelectUnit;
}

} // namespace jf
