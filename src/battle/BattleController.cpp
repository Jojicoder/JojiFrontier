#include "jf/battle/BattleController.hpp"

#include "jf/battle/EnemyAI.hpp"
#include "jf/battle/Movement.hpp"

namespace jf {

namespace {
constexpr float kEnemyActionDelay = 0.6f;
}

BattleController::BattleController(BattleState battle) : battle_(std::move(battle)) {}

std::optional<CombatPreview> BattleController::pendingPreview() const {
    if (inputState_ != BattleInputState::ConfirmAttack || !selectedUnit_ || !pendingTarget_) {
        return std::nullopt;
    }
    return jf::previewAttack(*selectedUnit_, *pendingTarget_);
}

void BattleController::selectUnit(Unit& unit) {
    if (inputState_ != BattleInputState::SelectUnit) return;
    if (unit.team != Team::Player || !unit.isAlive() || unit.hasActed) return;

    selectedUnit_ = &unit;
    moveOrigin_ = unit.position;
    reachableTiles_ = computeReachableTiles(battle_.units(), unit);
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
    if (!isReachable) return;

    battle_.moveUnit(*selectedUnit_, pos);
    inputState_ = BattleInputState::SelectAction;
}

void BattleController::chooseAttack() {
    if (inputState_ != BattleInputState::SelectAction || !selectedUnit_) return;

    targetableTiles_ = computeTargetableTiles(battle_.units(), *selectedUnit_, selectedUnit_->position);
    if (targetableTiles_.empty()) return;

    inputState_ = BattleInputState::SelectTarget;
}

void BattleController::chooseWait() {
    if (inputState_ != BattleInputState::SelectAction || !selectedUnit_) return;

    battle_.markActed(*selectedUnit_);
    selectedUnit_ = nullptr;
    reachableTiles_.clear();
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

void BattleController::confirmAttack() {
    if (inputState_ != BattleInputState::ConfirmAttack || !selectedUnit_ || !pendingTarget_) return;

    resolveAttack(*selectedUnit_, *pendingTarget_);
    battle_.markActed(*selectedUnit_);

    selectedUnit_ = nullptr;
    pendingTarget_ = nullptr;
    reachableTiles_.clear();
    targetableTiles_.clear();
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
