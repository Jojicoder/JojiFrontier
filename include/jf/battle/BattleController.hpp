#pragma once

#include <optional>
#include <vector>

#include "jf/battle/BattleState.hpp"
#include "jf/battle/CombatResolver.hpp"
#include "jf/core/Grid.hpp"

namespace jf {

enum class BattleInputState {
    SelectUnit,
    SelectMove,
    SelectAction,
    SelectTarget,
    ConfirmAttack,
    EnemyTurn,
    Victory,
    Defeat
};

// Drives the SelectUnit -> SelectMove -> SelectAction -> SelectTarget ->
// ResolveAction -> SelectUnit input flow described in the design doc, plus
// the enemy phase and win/loss detection. Owns a BattleState and contains
// no raylib calls, so it can be unit-tested or reused by a future web
// front end untouched.
class BattleController {
public:
    explicit BattleController(BattleState battle);

    BattleInputState inputState() const { return inputState_; }
    BattleState& battle() { return battle_; }
    const BattleState& battle() const { return battle_; }

    Unit* selectedUnit() const { return selectedUnit_; }
    Unit* pendingTarget() const { return pendingTarget_; }

    // Cached for the currently selected unit; empty outside the relevant states.
    const std::vector<GridPos>& reachableTiles() const { return reachableTiles_; }
    const std::vector<GridPos>& targetableTiles() const { return targetableTiles_; }

    std::optional<CombatPreview> pendingPreview() const;

    // UI events. Each is a no-op if called outside its expected state.
    void selectUnit(Unit& unit);
    void selectMoveTile(GridPos pos);
    void chooseAttack();
    void chooseWait();
    void selectTargetTile(GridPos pos);
    void confirmAttack();
    void cancelToUnitSelect();

    // Advances the enemy phase by dt seconds (paced so the player can follow
    // along); call once per frame regardless of input state.
    void update(float dt);

private:
    void evaluateOutcome();
    Unit* nextUnactedEnemy();

    BattleState battle_;
    BattleInputState inputState_ = BattleInputState::SelectUnit;

    Unit* selectedUnit_ = nullptr;
    GridPos moveOrigin_{};
    Unit* pendingTarget_ = nullptr;

    std::vector<GridPos> reachableTiles_;
    std::vector<GridPos> targetableTiles_;

    float enemyActionTimer_ = 0.0f;
};

} // namespace jf
