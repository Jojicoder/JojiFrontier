#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "jf/battle/BattleEvents.hpp"
#include "jf/battle/BattleObject.hpp"
#include "jf/battle/BattleObjectResolver.hpp"
#include "jf/battle/BattleState.hpp"
#include "jf/battle/CombatResolver.hpp"
#include "jf/battle/ObjectiveTracker.hpp"
#include "jf/battle/SkillCharges.hpp"
#include "jf/battle/AiSystem.hpp"
#include "jf/core/Grid.hpp"

namespace jf {

enum class BattleInputState {
    SelectUnit,
    SelectMove,
    SelectAction,
    SelectTarget,
    SelectHealTarget,
    // 辺境工兵「野戦工作」(docs/class_reference.md「後半6兵種」): same
    // dedicated-command shape as SelectHealTarget above, for an固有能力
    // outside the 2 equip slots.
    SelectFieldFortificationTarget,
    SelectItemTarget,
    SelectBoardTarget,
    SelectSkillTarget,
    ConfirmAttack,
    // M4 item 3 (Preview/Resolverの一致): between SelectSkillTarget and
    // actual resolution for the 3 attack-shape skills (suppressing_shot/
    // halting_thrust/ambush) only - mirrors ConfirmAttack's preview/confirm
    // gate so their predicted damage is never a surprise. Every other skill
    // shape still resolves immediately on target selection (see
    // selectSkillTarget()).
    ConfirmSkillAttack,
    // Battle ObjectのBattleController統合: SelectTarget中にcanBeAttackedな
    // Objectの乗るTileを選んだ時だけ遷移する、ConfirmAttackと並行する状態
    // (Unit相手のConfirmAttackとは異なりObjectには命中率・反撃・状態異常が
    // 無いため、previewAttack()を再利用せず専用のPreviewを持つ)。
    ConfirmObjectAttack,
    // Battle Object統合(Interact配線): chooseInteract()がSelectActionから
    // 遷移する専用State。操作は攻撃と違いPreview/Confirmを挟まず、対象Tile
    // 選択(selectInteractTarget())の時点でresolveObjectInteraction()まで
    // 即座に解決する(chooseHeal()/selectHealTarget()と同じ即時解決パターン)。
    SelectInteractTarget,
    // 伝令騎兵「再移動」(docs/class_reference.md「後半6兵種」): Attack/Skill/
    // Item行動の直後、finishPlayerAction()がここへ委譲する(その行動自体は
    // まだmarkActed/ActionResolvedEvent未発行) - selectReMoveTarget()が
    // 移動先(現在地=移動しない、も含む)を確定させて初めて行動が終了する。
    SelectReMoveTarget,
    EnemyTurn,
    Victory,
    Defeat
};

// docs/battle_objects.md "耐久とダメージ": ConfirmObjectAttack用の簡易Preview。
// Unit相手のCombatPreviewと違い命中率・反撃・状態異常の概念が無いため専用の型にした。
struct ObjectAttackPreview {
    std::string attackerName;
    BattleObjectId objectId;
    BattleObjectKind objectKind;
    int damage = 0;
    int durabilityBefore = 0;
    int durabilityAfter = 0;
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
    BattleObjectState* pendingObjectTarget() const { return pendingObjectTarget_; }

    // Cached for the currently selected unit; empty outside the relevant states.
    const std::vector<GridPos>& reachableTiles() const { return reachableTiles_; }
    const std::vector<GridPos>& targetableTiles() const { return targetableTiles_; }
    // Battle Object統合: chooseAttack()が射程内のcanBeAttackedなObject Tileを
    // 別Vectorとして収集したもの(UnitとObjectが同一Tileに同時に存在することは
    // 無い前提のため、targetableTiles_とは重複しない)。
    const std::vector<GridPos>& objectTargetableTiles() const { return objectTargetableTiles_; }
    // Battle Object統合(Interact配線): chooseInteract()が射程内で
    // interactionを持ち、現在requiredStateにあるObjectのTileを収集したもの。
    const std::vector<GridPos>& objectInteractableTiles() const { return objectInteractableTiles_; }

    // Threat-range preview: every tile the selected unit could attack. While
    // still choosing where to move, this is the union over every reachable
    // move tile; once the unit has moved, it narrows to just its new tile.
    const std::vector<GridPos>& attackRangeTiles() const { return attackRangeTiles_; }
    const std::vector<GridPos>& healableTiles() const { return healableTiles_; }
    const std::vector<GridPos>& fieldFortificationTiles() const { return fieldFortificationTiles_; }
    const std::vector<GridPos>& reMoveTiles() const { return reMoveTiles_; }
    const std::vector<GridPos>& itemTargetTiles() const { return itemTargetTiles_; }
    const std::vector<GridPos>& boardTargetTiles() const { return boardTargetTiles_; }
    const std::vector<GridPos>& skillTargetTiles() const { return skillTargetTiles_; }
    // docs/skill_system.md "使用不能スキルは非表示にせず、理由付きで無効表示":
    // both equip slots' current availability, straight from SkillCharges'
    // charge/cooldown bookkeeping (doesn't know about "already acted this
    // turn" - callers combine that with inputState() themselves).
    std::vector<SkillAvailability> selectedUnitSkills() const;

    std::optional<CombatPreview> pendingPreview() const;
    // M4 item 3: same idea as pendingPreview(), for the ConfirmSkillAttack
    // state the 3 attack-shape skills enter instead of resolving instantly.
    // nullopt outside that state.
    std::optional<CombatPreview> pendingSkillPreview() const;
    // Battle Object統合: pendingPreview()のObject版。ConfirmObjectAttack以外ではnullopt。
    std::optional<ObjectAttackPreview> pendingObjectPreview() const;
    // Battle Object統合(Interact配線): read-only query for the front end to
    // decide whether to even show an Interact button, without mutating
    // input state the way chooseInteract() does. True iff the selected unit
    // currently has at least one Object in interact range.
    bool canInteract() const;

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
    // Battle Object統合(Interact配線): SelectAction -> SelectInteractTarget。
    // No-op (stays in SelectAction) if nothing is currently interactable.
    void chooseInteract();
    // Resolves resolveObjectInteraction() immediately on a valid target Tile,
    // same one-step pattern as selectHealTarget(). No-op outside
    // SelectInteractTarget or on an invalid/no-longer-valid target.
    void selectInteractTarget(GridPos pos);
    void chooseHeal();
    void selectHealTarget(GridPos pos);
    // 辺境工兵「野戦工作」: chooseHeal()/selectHealTarget()と同じ即時解決
    // パターン。既に使用済み(Unit::fieldFortificationUsed)ならno-op。
    void chooseFieldFortification();
    void selectFieldFortificationTarget(GridPos pos);
    // 伝令騎兵「再移動」: resolves the pending re-move deferred by
    // finishPlayerAction() (see BattleInputState::SelectReMoveTarget). No-op
    // outside that state or on an invalid target.
    void selectReMoveTarget(GridPos pos);
    bool useHealingItem(int amount);
    bool chooseHealingItemTarget(int amount);
    bool selectHealingItemTarget(GridPos pos);
    void chooseProtectiveBoard();
    bool selectBoardTarget(GridPos pos);
    // docs/implementation_roadmap.md M4-A "Skill Effect Executor": all 18
    // equipped skills across the 6 initial classes have real in-battle
    // effects (see the shape tables/dedicated branches in
    // BattleController.cpp's anonymous namespace). chooseSkill() still
    // no-ops for a skill id that matches none of them, same as
    // chooseHeal()/chooseAttack() no-op when nothing's targetable.
    void chooseSkill(int slotIndex);
    // For the 3 attack-shape skills, this only transitions to
    // ConfirmSkillAttack (see pendingSkillPreview()/confirmSkillAttack()) -
    // it does not resolve the attack itself. Every other skill shape
    // resolves immediately here.
    bool selectSkillTarget(GridPos pos);
    void chooseWait();
    void endPlayerTurn();
    void selectTargetTile(GridPos pos);
    void cancelAttackSelection();
    void confirmAttack();
    // M4 item 3: resolves the attack-shape skill selectSkillTarget() staged
    // into ConfirmSkillAttack. No-op outside that state.
    void confirmSkillAttack();
    // Battle Object統合: selectTargetTile()がObject Tileを選んだ時に遷移する
    // ConfirmObjectAttackを解決する。No-op outside that state.
    void confirmObjectAttack();
    void cancelToUnitSelect();

    // Advances the enemy phase by dt seconds (paced so the player can follow
    // along); call once per frame regardless of input state.
    void update(float dt);

private:
    void evaluateOutcome();
    // Runs the docs/status_effects.md action-end pipeline, then either
    // concludes `unit`'s action (markActed + ActionResolvedEvent, returning
    // true) or - for 伝令騎兵「再移動」only - defers that conclusion into
    // BattleInputState::SelectReMoveTarget and returns false. Every call
    // site must skip its own post-action cleanup when this returns false
    // (selectReMoveTarget() runs the deferred tail once resolved).
    bool finishPlayerAction(Unit& unit, ActionKind actionKind);
    // markActed + ActionResolvedEvent construction/dispatch, shared by
    // finishPlayerAction()'s immediate path and selectReMoveTarget()'s
    // deferred-tail path.
    void markActionResolved(Unit& unit, ActionKind actionKind);
    Unit* nextUnactedEnemy();

    BattleState battle_;
    BattleInputState inputState_ = BattleInputState::SelectUnit;
    ActionId nextActionId_ = 1;

    Unit* selectedUnit_ = nullptr;
    GridPos moveOrigin_{};
    Unit* pendingTarget_ = nullptr;
    BattleObjectState* pendingObjectTarget_ = nullptr;
    // 辺境斥候`trailblaze`(道拓き): the exact path the selected unit's most
    // recent move took, captured in selectMoveTile() before the move
    // actually happens (computeMovementPath() needs the mover's position to
    // still be the origin). Consumed if trailblaze resolves this action.
    std::vector<GridPos> lastMovementPath_;

    std::vector<GridPos> reachableTiles_;
    std::vector<GridPos> targetableTiles_;
    std::vector<GridPos> objectTargetableTiles_;
    std::vector<GridPos> objectInteractableTiles_;
    std::vector<GridPos> attackRangeTiles_;
    std::vector<GridPos> healableTiles_;
    std::vector<GridPos> fieldFortificationTiles_;
    std::vector<GridPos> itemTargetTiles_;
    std::vector<GridPos> boardTargetTiles_;
    std::vector<GridPos> skillTargetTiles_;
    int pendingSkillSlot_ = -1;
    // 伝令騎兵「再移動」: candidate destinations (always includes the mover's
    // current position, so "don't move" is a valid choice) and which
    // ActionKind finishPlayerAction() deferred - consumed by
    // selectReMoveTarget().
    std::vector<GridPos> reMoveTiles_;
    ActionKind pendingReMoveActionKind_ = ActionKind::Wait;

    float enemyActionTimer_ = 0.0f;
    AiSquadReservations enemyReservations_;
    int pendingHealingItemAmount_ = 0;

    Unit* lastAttacker_ = nullptr;
    Unit* lastAttackTarget_ = nullptr;
    int lastDamage_ = 0;
    bool lastAttackHit_ = true;
    std::uint64_t attackEventId_ = 0;
};

} // namespace jf
