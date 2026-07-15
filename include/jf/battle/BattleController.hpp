#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "jf/battle/BattleEvents.hpp"
#include "jf/battle/BattleState.hpp"
#include "jf/battle/CombatResolver.hpp"
#include "jf/battle/ObjectiveTracker.hpp"
#include "jf/battle/SkillCharges.hpp"
#include "jf/core/Grid.hpp"

namespace jf {

enum class BattleInputState {
    SelectUnit,
    SelectMove,
    SelectAction,
    SelectTarget,
    SelectHealTarget,
    SelectItemTarget,
    SelectBoardTarget,
    SelectSkillTarget,
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

    // Threat-range preview: every tile the selected unit could attack. While
    // still choosing where to move, this is the union over every reachable
    // move tile; once the unit has moved, it narrows to just its new tile.
    const std::vector<GridPos>& attackRangeTiles() const { return attackRangeTiles_; }
    const std::vector<GridPos>& healableTiles() const { return healableTiles_; }
    const std::vector<GridPos>& itemTargetTiles() const { return itemTargetTiles_; }
    const std::vector<GridPos>& boardTargetTiles() const { return boardTargetTiles_; }
    const std::vector<GridPos>& skillTargetTiles() const { return skillTargetTiles_; }
    // docs/skill_system.md "使用不能スキルは非表示にせず、理由付きで無効表示":
    // both equip slots' current availability, straight from SkillCharges'
    // charge/cooldown bookkeeping (doesn't know about "already acted this
    // turn" - callers combine that with inputState() themselves).
    std::vector<SkillAvailability> selectedUnitSkills() const;

    std::optional<CombatPreview> pendingPreview() const;

    // Reports the most recent attack (player or enemy) so the front end can
    // drive a purely-visual attack animation (e.g. a lunge toward the
    // target) without BattleController knowing anything about rendering.
    // `attackEventId()` increments every time an attack resolves; compare it
    // frame-to-frame to detect a new event before reading the other two.
    Unit* lastAttacker() const { return lastAttacker_; }
    Unit* lastAttackTarget() const { return lastAttackTarget_; }
    // Damage actually applied (0 on a miss) and whether it landed, for the
    // same event `attackEventId()` reports - lets the front end show a
    // hit/miss/damage message without duplicating combat math.
    int lastDamage() const { return lastDamage_; }
    bool lastAttackHit() const { return lastAttackHit_; }
    std::uint64_t attackEventId() const { return attackEventId_; }

    // UI events. Each is a no-op if called outside its expected state.
    void selectUnit(Unit& unit);
    void selectMoveTile(GridPos pos);
    void returnToMoveSelection();
    void chooseAttack();
    void chooseHeal();
    void selectHealTarget(GridPos pos);
    bool useHealingItem(int amount);
    bool chooseHealingItemTarget(int amount);
    bool selectHealingItemTarget(GridPos pos);
    void chooseProtectiveBoard();
    bool selectBoardTarget(GridPos pos);
    // docs/implementation_roadmap.md M4 item 1 "Skill Effect Executor": the
    // first equipped-skill executor slice. Only 暁の衛生兵's
    // `emergency_treatment` (docs/initial_skill_effects.md) actually has an
    // effect implemented so far - chooseSkill() no-ops for any other skill
    // id, same as chooseHeal()/chooseAttack() no-op when nothing's
    // targetable. The other 17 skills are deliberately not attempted in this
    // slice; see the roadmap for what's deferred and why.
    void chooseSkill(int slotIndex);
    bool selectSkillTarget(GridPos pos);
    void chooseWait();
    void endPlayerTurn();
    void selectTargetTile(GridPos pos);
    void cancelAttackSelection();
    void confirmAttack();
    void cancelToUnitSelect();

    // Advances the enemy phase by dt seconds (paced so the player can follow
    // along); call once per frame regardless of input state.
    void update(float dt);

private:
    void evaluateOutcome();
    // Runs the docs/status_effects.md action-end pipeline, marks `unit`
    // acted, and feeds an ActionResolvedEvent to the mission's Objective
    // tracking (docs/mission_objectives.md) so kinds like SecureTile can
    // credit it.
    void finishPlayerAction(Unit& unit, ActionKind actionKind);
    Unit* nextUnactedEnemy();

    BattleState battle_;
    BattleInputState inputState_ = BattleInputState::SelectUnit;
    ActionId nextActionId_ = 1;

    Unit* selectedUnit_ = nullptr;
    GridPos moveOrigin_{};
    Unit* pendingTarget_ = nullptr;

    std::vector<GridPos> reachableTiles_;
    std::vector<GridPos> targetableTiles_;
    std::vector<GridPos> attackRangeTiles_;
    std::vector<GridPos> healableTiles_;
    std::vector<GridPos> itemTargetTiles_;
    std::vector<GridPos> boardTargetTiles_;
    std::vector<GridPos> skillTargetTiles_;
    int pendingSkillSlot_ = -1;

    float enemyActionTimer_ = 0.0f;
    int pendingHealingItemAmount_ = 0;

    Unit* lastAttacker_ = nullptr;
    Unit* lastAttackTarget_ = nullptr;
    int lastDamage_ = 0;
    bool lastAttackHit_ = true;
    std::uint64_t attackEventId_ = 0;
};

} // namespace jf
