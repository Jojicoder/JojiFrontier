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

// Battle Object統合(Interact配線): candidate Object Tiles for `actor`,
// filtered by the same checks resolveObjectInteraction() re-verifies at
// confirm time (requiredState/maxUses/allowedClasses/range) minus the
// mutation - so a Tile only shows here if selectInteractTarget() will
// actually accept it. Shared by chooseInteract() (populates the cached
// Tile list) and canInteract() (read-only "should the UI even show the
// button" query).
std::vector<GridPos> computeInteractableObjectTiles(const BattleState& battle, const Unit& actor) {
    std::vector<GridPos> tiles;
    for (const BattleObjectState& object : battle.objects()) {
        const BattleObjectDefinition* def = battle.objectDefinition(object.definitionId);
        if (!def || !def->interaction) continue;
        const ObjectInteractionDefinition& interaction = *def->interaction;
        if (object.state != interaction.requiredState) continue;
        if (object.interactionCount >= interaction.maxUses) continue;
        if (!interaction.allowedClasses.empty() && !interaction.allowedClasses.count(actor.unitClass)) continue;
        if (manhattanDistance(actor.position, object.position) > interaction.range) continue;
        tiles.push_back(object.position);
    }
    return tiles;
}

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

// 辺境斥候`emergency_withdrawal`(緊急離脱): self-only, no target unit at all -
// same "not worth a table for one instance" reasoning as isCleanseShape/
// isAdvanceOrderShape above.
bool isEmergencyWithdrawalShape(const std::string& skillId) { return skillId == "emergency_withdrawal"; }

// 重装兵`armor_advance`(装甲前進): same self-only movement shape as
// emergency_withdrawal above (no target unit, doesn't attack, ends the
// turn) - reuses that reasoning rather than adding another one-line
// comment.
bool isArmorAdvanceShape(const std::string& skillId) { return skillId == "armor_advance"; }

// 重装兵`break_obstacle`(障害物破砕): 隣接する破壊可能なObject1個を耐久に関わらず
// 即座に破壊する。通常のObject攻撃(chooseAttack()のobjectTargetableTiles_収集/
// confirmObjectAttack()のresolveObjectAttack())と土台は同じだが、ダメージ計算を
// 経由せず耐久を直接0にする点が異なるため専用分岐にした。`canBeAttacked`ゲートを
// 流用することで「任務上破壊不可」は自動的に満たされる。
bool isBreakObstacleShape(const std::string& skillId) { return skillId == "break_obstacle"; }
constexpr int kBreakObstacleRange = 1;

// 辺境工兵`field_repair`(野戦補修): 隣接する自チームの設置物1個の耐久を回復する。
// break_obstacle同様Object相手の専用分岐だが、破壊ではなく耐久を加算する点が異なる。
bool isFieldRepairShape(const std::string& skillId) { return skillId == "field_repair"; }
constexpr int kFieldRepairRange = 1;
constexpr int kFieldRepairAmount = 6;

// 辺境工兵`rubble_charge`(瓦礫爆破): break_obstacleと同じ即時破壊に加え、対象マスの
// 直上・直下(row±1、同col)にいる敵へ固定3ダメージを与える。反応攻撃は発生させない
// (通常の反撃トリガーを経由しないため)。
bool isRubbleChargeShape(const std::string& skillId) { return skillId == "rubble_charge"; }
constexpr int kRubbleChargeRange = 2;
constexpr int kRubbleChargeSplashDamage = 3;

// 辺境工兵`rapid_barricade`(即席防壁): 空きマスへ耐久6の設置物(rapid_barricade
// Definition)を配置する。次の自軍Phase開始時にBattleController::finishEnemyPhase()
// が自動的にDestroyed化する(固有能力`field_barricade`は消滅しない永続版)。
bool isRapidBarricadeShape(const std::string& skillId) { return skillId == "rapid_barricade"; }
constexpr int kRapidBarricadeRange = 2;

// 古参守備兵`provoke`(挑発): 敵1体・射程2、Damageなし、対象へUnit::
// provokedByUnitIdを設定するだけ - Mark形状に似るが、書き込む値がDamageの
// 符号付き整数ではなく「このスキルを使ったUnitのid」で、しかも設定先は
// (Mark形状のように味方/敵を選べる仕組みではなく)常に敵1体固定なので、
// 別の専用分岐にした。
bool isProvokeShape(const std::string& skillId) { return skillId == "provoke"; }
constexpr int kProvokeRange = 2;

// 伝令騎兵`urgent_dispatch`(緊急伝令): 行軍隊長`advance_order`と似た
// applyMoveUp()型のバフだが、(1)対象が隣接1ではなく射程2、(2)+1ではなく+2
// (`Unit::urgentDispatchActive`という別フィールド)、(3)自分自身は対象外だが
// 「未行動限定」ではない、という3点が異なるため専用分岐にした。
bool isUrgentDispatchShape(const std::string& skillId) { return skillId == "urgent_dispatch"; }
constexpr int kUrgentDispatchRange = 2;

// 伝令騎兵`ride_through`(駆け抜け): overwatch/trailblazeと同じself-only即時
// 解決型。実際の効果(このactionの再移動予算を4に)はUnit::
// rideThroughBudgetActiveを立てるだけで、消費・適用はfinishPlayerAction()側。
bool isRideThroughShape(const std::string& skillId) { return skillId == "ride_through"; }

// 伝令騎兵`rescue_transfer`(救援搬送): 隣接する味方1人を、使用者から見て
// 反対側の空きマスへ1マス移動させる(reflection)。対象はUnitだが移動させる
// 対象自体は行動状態を変えない(finishPlayerAction()を呼ばない)ため、
// break_obstacle等のObject専用分岐とも既存のUnit対象分岐とも別の専用形状。
bool isRescueTransferShape(const std::string& skillId) { return skillId == "rescue_transfer"; }
constexpr int kRescueTransferRange = 1;

// 監視弓兵`overwatch`(警戒射撃): self-only, no target to choose - resolves
// immediately like hold_formation/extended_lockdown's selfOnly buffs below,
// but arms Unit::overwatchActive rather than a BuffKind (this isn't a stat
// buff, it's ambush readiness consumed reactively by EnemyAI.cpp's
// triggerOverwatch(), not by anything in BattleController).
bool isOverwatchShape(const std::string& skillId) { return skillId == "overwatch"; }

// 辺境斥候`trailblaze`(道拓き): self-only, no target to choose - resolves
// immediately like overwatch above, but marks Ash/Shallows tiles from
// lastMovementPath_ as trailblazed (BattleState::markTrailblazed()) rather
// than arming a flag on the unit itself.
bool isTrailblazeShape(const std::string& skillId) { return skillId == "trailblaze"; }

struct AttackSkillShape {
    int bonusDamage = 0;
    bool appliesMoveDown = false;
    bool requiresUnacted = false; // 未行動の敵限定 (辺境斥候「奇襲」)
    std::vector<StatusEffectType> statuses;
};
const std::unordered_map<std::string, AttackSkillShape>& attackSkillShapes() {
    static const std::unordered_map<std::string, AttackSkillShape> table = {
        {"suppressing_shot", {.appliesMoveDown = true}},
        {"halting_thrust", {.appliesMoveDown = true}},
        {"ambush", {.bonusDamage = 3, .requiresUnacted = true,
                     .statuses = {StatusEffectType::DefenseDown}}},
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

enum class BuffKind { Resistance, Defense, ZocRange, Brace };
struct BuffSkillShape {
    BuffKind kind = BuffKind::Resistance;
    bool selfAndAllAdjacent = false; // true = self+every adjacent ally, resolves with no target selection
    bool selfOnly = false;           // true = self alone, also resolves with no target selection
    // 槍兵`spear_wall`(槍壁)「自身と隣接味方1人」: unlike the plain
    // single-target case (self OR one ally, whichever the player picks),
    // both self and the chosen ally receive the buff, and self isn't itself
    // a selectable target (it's automatic) - see chooseSkill()/
    // selectSkillTarget()'s buff branches below.
    bool alsoSelf = false;
};
const std::unordered_map<std::string, BuffSkillShape>& buffSkillShapes() {
    static const std::unordered_map<std::string, BuffSkillShape> table = {
        {"protective_treatment", {.kind = BuffKind::Resistance}},
        {"hold_formation", {.kind = BuffKind::Defense, .selfAndAllAdjacent = true}},
        {"extended_lockdown", {.kind = BuffKind::ZocRange, .selfOnly = true}},
        {"spear_wall", {.kind = BuffKind::Brace, .alsoSelf = true}},
    };
    return table;
}

void applyBuff(BuffKind kind, Unit& target) {
    switch (kind) {
        case BuffKind::Resistance: applyResistanceUp(target); return;
        case BuffKind::Defense: applyDefenseUp(target); return;
        case BuffKind::ZocRange: applyZocRangeExtension(target); return;
        case BuffKind::Brace: applyBraceBonus(target); return;
    }
}

// Shared by 辺境斥候`emergency_withdrawal`(緊急離脱, range 3) and 重装兵
// `armor_advance`(装甲前進, range 2) - both are "self-only movement, no
// attack" shapes (none of the 5 tables above fit a skill with no target
// unit at all). Deliberately simpler than the normal move
// computeReachableTiles(): fixed budget regardless of MOV/terrain cost, and
// ignores Zone of Control entirely (the whole point of both skills is
// bypassing it), while still respecting normal occupancy/passability/
// Battle Object blocking ("通常占有規則を守る").
std::vector<GridPos> computeSelfMovementTiles(const BattleState& battle, const Unit& mover, int range) {
    std::vector<GridPos> result;
    std::unordered_map<int, int> bestCost;
    const auto key = [](GridPos pos) { return pos.row * kGridCols + pos.col; };
    std::vector<GridPos> frontier{mover.position};
    bestCost[key(mover.position)] = 0;
    while (!frontier.empty()) {
        std::vector<GridPos> next;
        for (GridPos current : frontier) {
            int cost = bestCost[key(current)];
            if (cost >= range) continue;
            static const GridPos kDirections[] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
            for (GridPos dir : kDirections) {
                GridPos candidate{current.row + dir.row, current.col + dir.col};
                if (!isInBounds(candidate) || !isPassable(battle.terrainAt(candidate))) continue;
                if (battle.objectBlocksMovementAt(candidate)) continue;
                // Same occupancy-during-expansion rule as Movement.cpp's
                // computeReachableTilesImpl(): allies can be crossed but never
                // used as a destination, enemies block expansion entirely.
                const Unit* occupant = battle.unitAt(candidate);
                if (occupant && occupant->team != mover.team) continue;
                auto it = bestCost.find(key(candidate));
                if (it != bestCost.end() && it->second <= cost + 1) continue;
                bestCost[key(candidate)] = cost + 1;
                next.push_back(candidate);
            }
        }
        frontier = std::move(next);
    }
    for (const auto& [tileKey, cost] : bestCost) {
        GridPos pos{tileKey / kGridCols, tileKey % kGridCols};
        if (pos == mover.position) continue;
        const Unit* occupant = battle.unitAt(pos);
        if (occupant && occupant != &mover) continue;
        if (battle.objectBlocksStoppingAt(pos)) continue;
        result.push_back(pos);
    }
    return result;
}

std::vector<GridPos> computeEmergencyWithdrawalTiles(const BattleState& battle, const Unit& mover) {
    constexpr int kWithdrawalRange = 3;
    return computeSelfMovementTiles(battle, mover, kWithdrawalRange);
}

std::vector<GridPos> computeArmorAdvanceTiles(const BattleState& battle, const Unit& mover) {
    constexpr int kArmorAdvanceRange = 2;
    return computeSelfMovementTiles(battle, mover, kArmorAdvanceRange);
}

// 伝令騎兵「再移動」用のZone of Control判定。Movement.cpp's
// isStoppedByZoneOfControl()と同一ロジックだが、その関数はexportされていない
// ため(computeReachableTilesImpl()内の無名namespace専用)、ここへ複製した。
bool reMoveStoppedByZoneOfControl(const BattleState& battle, const Unit& mover, GridPos pos) {
    for (const Unit& unit : battle.units()) {
        if (!unit.isPresent() || unit.team == mover.team || !hasZoneOfControl(unit.unitClass)) continue;
        const int range = unit.zocRangeExtended ? 2 : 1;
        if (manhattanDistance(unit.position, pos) <= range) return true;
    }
    return false;
}

// 伝令騎兵「再移動」: computeSelfMovementTilesと似た固定予算BFSだが、(1)通常の
// 自己移動スキル(armor_advance等)と異なりZone of Controlを尊重する、(2)
// 「移動しない」を明示的な選択肢として常に含む(結果に現在地を必ず含める)点が
// 異なるため専用関数にした。
std::vector<GridPos> computeReMoveTiles(const BattleState& battle, const Unit& mover, int budget) {
    std::vector<GridPos> result{mover.position};
    std::unordered_map<int, int> bestCost;
    const auto key = [](GridPos pos) { return pos.row * kGridCols + pos.col; };
    std::vector<GridPos> frontier{mover.position};
    bestCost[key(mover.position)] = 0;
    while (!frontier.empty()) {
        std::vector<GridPos> next;
        for (GridPos current : frontier) {
            int cost = bestCost[key(current)];
            if (cost >= budget) continue;
            if (current != mover.position && reMoveStoppedByZoneOfControl(battle, mover, current)) continue;
            static const GridPos kDirections[] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
            for (GridPos dir : kDirections) {
                GridPos candidate{current.row + dir.row, current.col + dir.col};
                if (!isInBounds(candidate) || !isPassable(battle.terrainAt(candidate))) continue;
                if (battle.objectBlocksMovementAt(candidate)) continue;
                const Unit* occupant = battle.unitAt(candidate);
                if (occupant && occupant->team != mover.team) continue;
                auto it = bestCost.find(key(candidate));
                if (it != bestCost.end() && it->second <= cost + 1) continue;
                bestCost[key(candidate)] = cost + 1;
                next.push_back(candidate);
            }
        }
        frontier = std::move(next);
    }
    for (const auto& [tileKey, cost] : bestCost) {
        GridPos pos{tileKey / kGridCols, tileKey % kGridCols};
        if (pos == mover.position) continue; // already in result
        const Unit* occupant = battle.unitAt(pos);
        if (occupant && occupant != &mover) continue;
        if (battle.objectBlocksStoppingAt(pos)) continue;
        result.push_back(pos);
    }
    return result;
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

void BattleController::markActionResolved(Unit& unit, ActionKind actionKind) {
    battle_.markActed(unit);
    const ActionId actionId = nextActionId_++;
    BattleEvent event{battle_.issueEventId(), actionId,
                      ActionResolvedEvent{actionId, unit.id, unit.team, actionKind, unit.position}};
    handleObjectiveEvent(battle_.missionState(), event);
}

// Returns true once `unit`'s action is fully concluded (every existing call
// site's behavior, unchanged). Returns false when 伝令騎兵「再移動」defers the
// conclusion into BattleInputState::SelectReMoveTarget instead - the caller
// must skip its own post-action cleanup in that case (see
// selectReMoveTarget(), which runs the deferred tail once a re-move Tile is
// chosen) rather than tearing down selectedUnit_/caches out from under it.
bool BattleController::finishPlayerAction(Unit& unit, ActionKind actionKind) {
    // Snapshot before the terrain/status resolution below so a self-defeat
    // from burn fires UnitDefeatedEvent exactly once (any combat-caused
    // defeat was already captured and emitted by the caller, e.g.
    // confirmAttack(), before finishPlayerAction() runs).
    const AliveSnapshot aliveBefore = captureAliveSnapshot(battle_);

    // docs/status_effects.md 地形効果の処理順: terrain heal before the
    // unit's own status-effect action-end damage.
    battle_.consumeHerbPatch(unit);
    processActionEndStatusEffects(battle_, unit);
    // 古参守備兵`immovable_stance`: DEF+3/no-movement lasts through the very
    // next action this unit takes after the Wait that granted it - so the
    // Wait action itself (immovableStanceJustGranted) must not clear it,
    // only consume the "just granted" flag; the action after that does.
    if (unit.immovableStanceActive) {
        if (unit.immovableStanceJustGranted) unit.immovableStanceJustGranted = false;
        else unit.immovableStanceActive = false;
    }
    // 重装兵`brace_for_impact`: same two-flag "survives the granting Wait,
    // clears at the end of the next action" shape as immovable_stance above.
    if (unit.braceForImpactActive) {
        if (unit.braceForImpactJustGranted) unit.braceForImpactJustGranted = false;
        else unit.braceForImpactActive = false;
    }

    // 伝令騎兵「再移動」: Attack/Skill/Item行動後、生存していれば最大2マス
    // (`ride_through`使用時4マス)移動して行動終了。まだmarkActed/
    // ActionResolvedEventを発行せず、SelectReMoveTargetへ委譲する。
    if (canReMove(unit.unitClass) && unit.isAlive() &&
        (actionKind == ActionKind::Attack || actionKind == ActionKind::Skill || actionKind == ActionKind::Item)) {
        const int budget = unit.rideThroughBudgetActive ? 4 : 2;
        unit.rideThroughBudgetActive = false; // spent on this action either way
        reMoveTiles_ = computeReMoveTiles(battle_, unit, budget);
        if (!reMoveTiles_.empty()) {
            emitUnitDefeatedEvents(battle_, aliveBefore);
            selectedUnit_ = &unit;
            pendingReMoveActionKind_ = actionKind;
            reachableTiles_.clear();
            targetableTiles_.clear();
            objectTargetableTiles_.clear();
            objectInteractableTiles_.clear();
            attackRangeTiles_.clear();
            healableTiles_.clear();
            fieldFortificationTiles_.clear();
            itemTargetTiles_.clear();
            boardTargetTiles_.clear();
            skillTargetTiles_.clear();
            inputState_ = BattleInputState::SelectReMoveTarget;
            return false;
        }
    }

    emitUnitDefeatedEvents(battle_, aliveBefore);
    markActionResolved(unit, actionKind);
    return true;
}

void BattleController::selectReMoveTarget(GridPos pos) {
    if (inputState_ != BattleInputState::SelectReMoveTarget || !selectedUnit_) return;
    if (std::find(reMoveTiles_.begin(), reMoveTiles_.end(), pos) == reMoveTiles_.end()) return;

    if (pos != selectedUnit_->position) battle_.moveUnit(*selectedUnit_, pos);
    markActionResolved(*selectedUnit_, pendingReMoveActionKind_);

    selectedUnit_ = nullptr;
    reMoveTiles_.clear();
    reachableTiles_.clear();
    attackRangeTiles_.clear();
    inputState_ = BattleInputState::SelectUnit;
    evaluateOutcome();
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

    // 辺境斥候`trailblaze`: must be captured before moveUnit() below changes
    // selectedUnit_->position out from under computeMovementPath()'s origin.
    lastMovementPath_ = computeMovementPath(battle_, *selectedUnit_, pos);
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

    // Battle Object統合: Unit同様、射程内かつcanBeAttackedなObjectのTileも
    // 別Vectorとして収集する(computeTargetableTiles()自体はUnit専用のまま、
    // ここではUnitを一切見ないObject用の判定を独立して行う)。
    objectTargetableTiles_.clear();
    for (const BattleObjectState& object : battle_.objects()) {
        if (object.state == BattleObjectStateKind::Destroyed) continue;
        const BattleObjectDefinition* def = battle_.objectDefinition(object.definitionId);
        if (!def || !def->canBeAttacked) continue;
        int dist = manhattanDistance(selectedUnit_->position, object.position);
        if (dist >= selectedUnit_->minimumAttackRange() && dist <= selectedUnit_->weapon.maxRange)
            objectTargetableTiles_.push_back(object.position);
    }

    if (targetableTiles_.empty() && objectTargetableTiles_.empty()) return;

    inputState_ = BattleInputState::SelectTarget;
}

bool BattleController::canInteract() const {
    if (!selectedUnit_) return false;
    return !computeInteractableObjectTiles(battle_, *selectedUnit_).empty();
}

void BattleController::chooseInteract() {
    if (inputState_ != BattleInputState::SelectAction || !selectedUnit_) return;

    objectInteractableTiles_ = computeInteractableObjectTiles(battle_, *selectedUnit_);
    if (objectInteractableTiles_.empty()) return;

    inputState_ = BattleInputState::SelectInteractTarget;
}

void BattleController::selectInteractTarget(GridPos pos) {
    if (inputState_ != BattleInputState::SelectInteractTarget || !selectedUnit_) return;
    if (std::find(objectInteractableTiles_.begin(), objectInteractableTiles_.end(), pos) ==
        objectInteractableTiles_.end()) {
        return;
    }

    BattleObjectState* target = battle_.objectAt(pos);
    if (!target) return;
    const BattleObjectDefinition* def = battle_.objectDefinition(target->definitionId);
    if (!def || !def->interaction) return;

    if (resolveObjectInteraction(*selectedUnit_, *target, *def->interaction, def->interactionResultState)) {
        handleObjectiveEvent(battle_.missionState(),
                             BattleEvent{battle_.issueEventId(), 0,
                                        ObjectStateChangedEvent{target->id, def->interactionResultState}});
    }

    if (!finishPlayerAction(*selectedUnit_, ActionKind::Interact)) return;
    selectedUnit_ = nullptr;
    objectInteractableTiles_.clear();
    reachableTiles_.clear();
    attackRangeTiles_.clear();
    inputState_ = BattleInputState::SelectUnit;
    evaluateOutcome();
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
    if (!finishPlayerAction(*selectedUnit_, ActionKind::Skill)) return;
    selectedUnit_ = nullptr;
    healableTiles_.clear();
    reachableTiles_.clear();
    attackRangeTiles_.clear();
    inputState_ = BattleInputState::SelectUnit;
    evaluateOutcome();
}

void BattleController::chooseFieldFortification() {
    if (inputState_ != BattleInputState::SelectAction || !selectedUnit_ ||
        !canFieldFortify(selectedUnit_->unitClass) || selectedUnit_->fieldFortificationUsed) return;

    fieldFortificationTiles_.clear();
    constexpr GridPos directions[] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (GridPos direction : directions) {
        GridPos pos{selectedUnit_->position.row + direction.row, selectedUnit_->position.col + direction.col};
        if (isInBounds(pos) && isPassable(battle_.terrainAt(pos)) && !battle_.unitAt(pos) && !battle_.objectAt(pos))
            fieldFortificationTiles_.push_back(pos);
    }
    if (!fieldFortificationTiles_.empty()) inputState_ = BattleInputState::SelectFieldFortificationTarget;
}

void BattleController::selectFieldFortificationTarget(GridPos pos) {
    if (inputState_ != BattleInputState::SelectFieldFortificationTarget || !selectedUnit_) return;
    if (std::find(fieldFortificationTiles_.begin(), fieldFortificationTiles_.end(), pos) ==
        fieldFortificationTiles_.end()) return;

    BattleObjectTeam team =
        selectedUnit_->team == Team::Player ? BattleObjectTeam::Player : BattleObjectTeam::Enemy;
    battle_.placeObject({selectedUnit_->id + "_field_barricade", "field_barricade", pos, team,
                         BattleObjectStateKind::Active, 10, 0});
    selectedUnit_->fieldFortificationUsed = true;
    if (!finishPlayerAction(*selectedUnit_, ActionKind::Skill)) return;
    selectedUnit_ = nullptr;
    fieldFortificationTiles_.clear();
    reachableTiles_.clear();
    attackRangeTiles_.clear();
    inputState_ = BattleInputState::SelectUnit;
    evaluateOutcome();
}

bool BattleController::useHealingItem(int amount) {
    if (inputState_ != BattleInputState::SelectAction || !selectedUnit_ || amount <= 0) return false;
    if (selectedUnit_->currentHp >= selectedUnit_->stats.maxHp) return false;

    selectedUnit_->currentHp = std::min(selectedUnit_->currentHp + amount, selectedUnit_->stats.maxHp);
    if (!finishPlayerAction(*selectedUnit_, ActionKind::Item)) return true;
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
    if (!finishPlayerAction(*target, ActionKind::Item)) return true;
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
    // self+every adjacent ally, or 古参守備兵`extended_lockdown`'s self-only),
    // plus 監視弓兵`overwatch`/辺境斥候`trailblaze` (also self-only, but each
    // arms its own dedicated state rather than a BuffKind), resolve
    // immediately.
    const bool overwatch = isOverwatchShape(skillId);
    const bool trailblaze = isTrailblazeShape(skillId);
    const bool rideThrough = isRideThroughShape(skillId);
    if (auto buff = buffSkillShapes().find(skillId);
        overwatch || trailblaze || rideThrough ||
        (buff != buffSkillShapes().end() && (buff->second.selfAndAllAdjacent || buff->second.selfOnly))) {
        if (overwatch) {
            selectedUnit_->overwatchActive = true;
        } else if (trailblaze) {
            for (GridPos pos : lastMovementPath_) {
                TerrainType terrain = battle_.terrainAt(pos);
                if (terrain == TerrainType::Ash || terrain == TerrainType::Shallows) battle_.markTrailblazed(pos);
            }
        } else if (rideThrough) {
            selectedUnit_->rideThroughBudgetActive = true;
        } else if (buff->second.selfOnly) {
            applyBuff(buff->second.kind, *selectedUnit_);
        } else {
            for (Unit& unit : battle_.units()) {
                if (unit.team != selectedUnit_->team || !unit.isAlive()) continue;
                if (&unit == selectedUnit_ || manhattanDistance(selectedUnit_->position, unit.position) <= 1)
                    applyBuff(buff->second.kind, unit);
            }
        }
        consumeSkillCharge(*selectedUnit_, slotIndex);
        if (!finishPlayerAction(*selectedUnit_, ActionKind::Skill)) return;
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
        // already returned above) - self or one adjacent ally, except
        // alsoSelf shapes (spear_wall) where self isn't itself selectable
        // (it always receives the buff automatically) and only an adjacent
        // ally can be chosen.
        for (Unit& unit : battle_.units()) {
            if (unit.team != selectedUnit_->team || !unit.isAlive()) continue;
            if (buff->second.alsoSelf && &unit == selectedUnit_) continue;
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
    } else if (isEmergencyWithdrawalShape(skillId)) {
        skillTargetTiles_ = computeEmergencyWithdrawalTiles(battle_, *selectedUnit_);
    } else if (isArmorAdvanceShape(skillId)) {
        skillTargetTiles_ = computeArmorAdvanceTiles(battle_, *selectedUnit_);
    } else if (isBreakObstacleShape(skillId)) {
        // Same object-targeting rule as chooseAttack()'s objectTargetableTiles_
        // collection, restricted to range 1 instead of weapon range.
        for (const BattleObjectState& object : battle_.objects()) {
            if (object.state == BattleObjectStateKind::Destroyed) continue;
            const BattleObjectDefinition* def = battle_.objectDefinition(object.definitionId);
            if (!def || !def->canBeAttacked) continue;
            if (manhattanDistance(selectedUnit_->position, object.position) <= kBreakObstacleRange)
                skillTargetTiles_.push_back(object.position);
        }
    } else if (isProvokeShape(skillId)) {
        for (Unit& unit : battle_.units()) {
            if (unit.team == selectedUnit_->team || !unit.isAlive()) continue;
            if (manhattanDistance(selectedUnit_->position, unit.position) <= kProvokeRange)
                skillTargetTiles_.push_back(unit.position);
        }
    } else if (isFieldRepairShape(skillId)) {
        BattleObjectTeam ownTeam =
            selectedUnit_->team == Team::Player ? BattleObjectTeam::Player : BattleObjectTeam::Enemy;
        for (const BattleObjectState& object : battle_.objects()) {
            if (object.state == BattleObjectStateKind::Destroyed || object.team != ownTeam) continue;
            const BattleObjectDefinition* def = battle_.objectDefinition(object.definitionId);
            if (!def || !def->canBeRepaired || object.durability >= def->maxDurability) continue;
            if (manhattanDistance(selectedUnit_->position, object.position) <= kFieldRepairRange)
                skillTargetTiles_.push_back(object.position);
        }
    } else if (isRubbleChargeShape(skillId)) {
        for (const BattleObjectState& object : battle_.objects()) {
            if (object.state == BattleObjectStateKind::Destroyed) continue;
            const BattleObjectDefinition* def = battle_.objectDefinition(object.definitionId);
            if (!def || !def->canBeAttacked) continue;
            if (manhattanDistance(selectedUnit_->position, object.position) <= kRubbleChargeRange)
                skillTargetTiles_.push_back(object.position);
        }
    } else if (isRapidBarricadeShape(skillId)) {
        // 「即席防壁と固有能力の防護板は同時に合計2個まで存在できる」: if the
        // cap is already reached, no target tile is offered - the skill is
        // effectively unusable, same "no target -> skill doesn't activate"
        // pattern chooseSkill() uses everywhere else (see the empty-check
        // below).
        int barricadeCount = 0;
        for (const BattleObjectState& object : battle_.objects()) {
            if (object.state != BattleObjectStateKind::Destroyed &&
                (object.definitionId == "field_barricade" || object.definitionId == "rapid_barricade")) {
                ++barricadeCount;
            }
        }
        if (barricadeCount < 2) {
            for (int row = 0; row < kGridRows; ++row) {
                for (int col = 0; col < kGridCols; ++col) {
                    GridPos pos{row, col};
                    if (manhattanDistance(selectedUnit_->position, pos) > kRapidBarricadeRange ||
                        pos == selectedUnit_->position || !isPassable(battle_.terrainAt(pos)) ||
                        battle_.unitAt(pos) || battle_.objectAt(pos))
                        continue;
                    skillTargetTiles_.push_back(pos);
                }
            }
        }
    } else if (isUrgentDispatchShape(skillId)) {
        for (Unit& unit : battle_.units()) {
            if (&unit == selectedUnit_ || unit.team != selectedUnit_->team || !unit.isAlive()) continue;
            if (manhattanDistance(selectedUnit_->position, unit.position) <= kUrgentDispatchRange)
                skillTargetTiles_.push_back(unit.position);
        }
    } else if (isRescueTransferShape(skillId)) {
        // 隣接する味方1人を選ぶ - 実際の移動先(反対側のマス)はselectSkillTarget()
        // 側でreflectionにより計算するが、そのマスが空き・通行可能でなければ
        // ここで対象から除外する。
        for (Unit& unit : battle_.units()) {
            if (&unit == selectedUnit_ || unit.team != selectedUnit_->team || !unit.isAlive()) continue;
            if (hasHeavyArmor(unit.unitClass) || unit.isBoss) continue;
            if (manhattanDistance(selectedUnit_->position, unit.position) != 1) continue; // must be adjacent
            GridPos delta{unit.position.row - selectedUnit_->position.row,
                         unit.position.col - selectedUnit_->position.col};
            GridPos dest{selectedUnit_->position.row - delta.row, selectedUnit_->position.col - delta.col};
            if (!isInBounds(dest) || !isPassable(battle_.terrainAt(dest)) || battle_.unitAt(dest) ||
                battle_.objectBlocksMovementAt(dest) || battle_.objectBlocksStoppingAt(dest))
                continue;
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

    // 辺境斥候`emergency_withdrawal`/重装兵`armor_advance`: the destination is
    // an empty tile, not a Unit, so both are handled here rather than
    // falling into the target-lookup path every other skill (all of which
    // act on a Unit) shares below - identical resolution, only their tile
    // computation (chooseSkill() above) differs.
    const std::string& shapeSkillId = selectedUnit_->skillSlots[static_cast<std::size_t>(pendingSkillSlot_)].skillId;
    if (isEmergencyWithdrawalShape(shapeSkillId) || isArmorAdvanceShape(shapeSkillId)) {
        if (!battle_.moveUnit(*selectedUnit_, pos)) return false;
        consumeSkillCharge(*selectedUnit_, pendingSkillSlot_);
        if (!finishPlayerAction(*selectedUnit_, ActionKind::Skill)) return true;
        selectedUnit_ = nullptr;
        pendingSkillSlot_ = -1;
        skillTargetTiles_.clear();
        reachableTiles_.clear();
        attackRangeTiles_.clear();
        inputState_ = BattleInputState::SelectUnit;
        evaluateOutcome();
        return true;
    }

    // 重装兵`break_obstacle`: the destination is an Object, not a Unit -
    // instant-destroy regardless of durability, same ObjectDestroyedEvent
    // firing as confirmObjectAttack()'s resolveObjectAttack() path, minus
    // the damage roll.
    if (isBreakObstacleShape(shapeSkillId)) {
        BattleObjectState* object = battle_.objectAt(pos);
        if (!object || object->state == BattleObjectStateKind::Destroyed) return false;
        object->durability = 0;
        object->state = BattleObjectStateKind::Destroyed;
        handleObjectiveEvent(battle_.missionState(),
                             BattleEvent{battle_.issueEventId(), 0, ObjectDestroyedEvent{object->id}});
        consumeSkillCharge(*selectedUnit_, pendingSkillSlot_);
        if (!finishPlayerAction(*selectedUnit_, ActionKind::Skill)) return true;
        selectedUnit_ = nullptr;
        pendingSkillSlot_ = -1;
        skillTargetTiles_.clear();
        reachableTiles_.clear();
        attackRangeTiles_.clear();
        inputState_ = BattleInputState::SelectUnit;
        evaluateOutcome();
        return true;
    }

    // 辺境工兵`field_repair`(野戦補修): the destination is an Object, not a
    // Unit - increases its durability by kFieldRepairAmount, capped at the
    // Definition's maxDurability.
    if (isFieldRepairShape(shapeSkillId)) {
        BattleObjectState* object = battle_.objectAt(pos);
        const BattleObjectDefinition* def = object ? battle_.objectDefinition(object->definitionId) : nullptr;
        if (!object || !def) return false;
        object->durability = std::min(object->durability + kFieldRepairAmount, def->maxDurability);
        consumeSkillCharge(*selectedUnit_, pendingSkillSlot_);
        if (!finishPlayerAction(*selectedUnit_, ActionKind::Skill)) return true;
        selectedUnit_ = nullptr;
        pendingSkillSlot_ = -1;
        skillTargetTiles_.clear();
        reachableTiles_.clear();
        attackRangeTiles_.clear();
        inputState_ = BattleInputState::SelectUnit;
        evaluateOutcome();
        return true;
    }

    // 辺境工兵`rubble_charge`(瓦礫爆破): same instant-destroy as
    // break_obstacle, plus a fixed kRubbleChargeSplashDamage hit (no
    // DEF/RES, no counter) to any enemy directly above/below the destroyed
    // tile - mirrors the mark_target bonusDamage idiom below (AliveSnapshot
    // -> currentHp -= amount -> emitUnitDefeatedEvents).
    if (isRubbleChargeShape(shapeSkillId)) {
        BattleObjectState* object = battle_.objectAt(pos);
        if (!object || object->state == BattleObjectStateKind::Destroyed) return false;
        object->durability = 0;
        object->state = BattleObjectStateKind::Destroyed;
        handleObjectiveEvent(battle_.missionState(),
                             BattleEvent{battle_.issueEventId(), 0, ObjectDestroyedEvent{object->id}});

        const AliveSnapshot aliveBeforeSplash = captureAliveSnapshot(battle_);
        for (GridPos splashPos : {GridPos{pos.row - 1, pos.col}, GridPos{pos.row + 1, pos.col}}) {
            Unit* splashTarget = battle_.unitAt(splashPos);
            if (splashTarget && splashTarget->team != selectedUnit_->team && splashTarget->isAlive())
                splashTarget->currentHp = std::max(0, splashTarget->currentHp - kRubbleChargeSplashDamage);
        }
        emitUnitDefeatedEvents(battle_, aliveBeforeSplash);

        consumeSkillCharge(*selectedUnit_, pendingSkillSlot_);
        if (!finishPlayerAction(*selectedUnit_, ActionKind::Skill)) return true;
        selectedUnit_ = nullptr;
        pendingSkillSlot_ = -1;
        skillTargetTiles_.clear();
        reachableTiles_.clear();
        attackRangeTiles_.clear();
        inputState_ = BattleInputState::SelectUnit;
        evaluateOutcome();
        return true;
    }

    // 辺境工兵`rapid_barricade`(即席防壁): the destination is an empty tile -
    // places a temporary "rapid_barricade" Object (distinct definitionId from
    // the permanent 固有能力 barricade so finishEnemyPhase()'s Player-Phase-
    // start cleanup can target only this one).
    if (isRapidBarricadeShape(shapeSkillId)) {
        BattleObjectTeam team =
            selectedUnit_->team == Team::Player ? BattleObjectTeam::Player : BattleObjectTeam::Enemy;
        battle_.placeObject({"rapid_barricade_" + std::to_string(battle_.issueEventId()), "rapid_barricade", pos,
                             team, BattleObjectStateKind::Active, 6, 0});
        consumeSkillCharge(*selectedUnit_, pendingSkillSlot_);
        if (!finishPlayerAction(*selectedUnit_, ActionKind::Skill)) return true;
        selectedUnit_ = nullptr;
        pendingSkillSlot_ = -1;
        skillTargetTiles_.clear();
        reachableTiles_.clear();
        attackRangeTiles_.clear();
        inputState_ = BattleInputState::SelectUnit;
        evaluateOutcome();
        return true;
    }

    // 伝令騎兵`rescue_transfer`(救援搬送): moves the target ally itself
    // (reflection across the caster), not the caster - the ally's hasActed
    // must stay untouched, so this doesn't route through the generic Unit
    // fallback below (which always treats `pos` as an attack/buff/mark
    // target of the caster, never as "move this other unit").
    if (isRescueTransferShape(shapeSkillId)) {
        Unit* ally = battle_.unitAt(pos);
        if (!ally) return false;
        GridPos delta{ally->position.row - selectedUnit_->position.row,
                     ally->position.col - selectedUnit_->position.col};
        GridPos dest{selectedUnit_->position.row - delta.row, selectedUnit_->position.col - delta.col};
        if (!battle_.moveUnit(*ally, dest)) return false;
        consumeSkillCharge(*selectedUnit_, pendingSkillSlot_);
        if (!finishPlayerAction(*selectedUnit_, ActionKind::Skill)) return true;
        selectedUnit_ = nullptr;
        pendingSkillSlot_ = -1;
        skillTargetTiles_.clear();
        reachableTiles_.clear();
        attackRangeTiles_.clear();
        inputState_ = BattleInputState::SelectUnit;
        evaluateOutcome();
        return true;
    }

    Unit* target = battle_.unitAt(pos);
    if (!target) return false;

    const std::string& skillId = selectedUnit_->skillSlots[static_cast<std::size_t>(pendingSkillSlot_)].skillId;

    // M4 item 3 (Preview/Resolverの一致): 攻撃形状Skill(suppressing_shot/
    // halting_thrust/ambush)だけは通常攻撃と同じくConfirm前にPreviewを見せる
    // - 他の形状(Heal/バフ/Mark等)はDamageを予測する必要がなく、これまで通り
    // 即座に解決する。実際のCharge消費・resolveAttack()はconfirmSkillAttack()
    // 側で行う(pendingSkillPreview()と同じcomputeDamage()を使うため、
    // Previewと実結果は自動的に一致する)。
    if (attackSkillShapes().find(skillId) != attackSkillShapes().end()) {
        pendingTarget_ = target;
        inputState_ = BattleInputState::ConfirmSkillAttack;
        return true;
    }

    if (auto heal = healSkillShapes().find(skillId); heal != healSkillShapes().end()) {
        target->currentHp = std::min(target->currentHp + heal->second.amount, target->stats.maxHp);
    } else if (isCleanseShape(skillId)) {
        clearAllStatusEffects(*target);
    } else if (auto buff = buffSkillShapes().find(skillId); buff != buffSkillShapes().end()) {
        applyBuff(buff->second.kind, *target);
        if (buff->second.alsoSelf) applyBuff(buff->second.kind, *selectedUnit_);
    } else if (auto mark = markSkillShapes().find(skillId); mark != markSkillShapes().end()) {
        target->markedBonusDamage = mark->second.bonusDamage;
    } else if (isAdvanceOrderShape(skillId)) {
        applyMoveUp(*target);
    } else if (isUrgentDispatchShape(skillId)) {
        target->urgentDispatchActive = true;
    } else if (isProvokeShape(skillId)) {
        applyProvoke(*target, selectedUnit_->id);
    }
    consumeSkillCharge(*selectedUnit_, pendingSkillSlot_);
    if (!finishPlayerAction(*selectedUnit_, ActionKind::Skill)) return true;
    selectedUnit_ = nullptr;
    pendingSkillSlot_ = -1;
    skillTargetTiles_.clear();
    reachableTiles_.clear();
    attackRangeTiles_.clear();
    inputState_ = BattleInputState::SelectUnit;
    evaluateOutcome();
    return true;
}

std::optional<CombatPreview> BattleController::pendingSkillPreview() const {
    if (inputState_ != BattleInputState::ConfirmSkillAttack || !selectedUnit_ || !pendingTarget_ ||
        pendingSkillSlot_ < 0) {
        return std::nullopt;
    }
    const std::string& skillId = selectedUnit_->skillSlots[static_cast<std::size_t>(pendingSkillSlot_)].skillId;
    auto attack = attackSkillShapes().find(skillId);
    if (attack == attackSkillShapes().end()) return std::nullopt;
    // Same computeDamage() confirmSkillAttack() resolves with below, plus
    // the flat bonusDamage folded in so the Preview's number is the exact
    // total HP loss a hit will actually cause (M4 item 3's Gate: "18 Skill
    // すべてでPreviewと実結果が一致").
    CombatPreview preview = jf::previewAttack(*selectedUnit_, *pendingTarget_,
                                              battle_.combatDefenseBonus(*pendingTarget_, *selectedUnit_),
                                              battle_.combatHitChance(*pendingTarget_));
    if (attack->second.bonusDamage > 0) {
        preview.damage += attack->second.bonusDamage;
        preview.targetHpAfter = std::max(pendingTarget_->currentHp - preview.damage, 0);
    }
    return preview;
}

std::optional<ObjectAttackPreview> BattleController::pendingObjectPreview() const {
    if (inputState_ != BattleInputState::ConfirmObjectAttack || !selectedUnit_ || !pendingObjectTarget_) {
        return std::nullopt;
    }
    const BattleObjectDefinition* def = battle_.objectDefinition(pendingObjectTarget_->definitionId);
    if (!def) return std::nullopt;
    ObjectAttackPreview preview;
    preview.attackerName = selectedUnit_->name;
    preview.objectId = pendingObjectTarget_->id;
    preview.objectKind = def->kind;
    preview.damage = computeObjectDamage(*selectedUnit_, *def);
    preview.durabilityBefore = pendingObjectTarget_->durability;
    preview.durabilityAfter = std::max(preview.durabilityBefore - preview.damage, 0);
    return preview;
}

void BattleController::confirmSkillAttack() {
    if (inputState_ != BattleInputState::ConfirmSkillAttack || !selectedUnit_ || !pendingTarget_ ||
        pendingSkillSlot_ < 0) {
        return;
    }
    const std::string& skillId = selectedUnit_->skillSlots[static_cast<std::size_t>(pendingSkillSlot_)].skillId;
    auto attack = attackSkillShapes().find(skillId);
    if (attack == attackSkillShapes().end()) return;

    lastAttacker_ = selectedUnit_;
    lastAttackTarget_ = pendingTarget_;
    ++attackEventId_;
    const AliveSnapshot aliveBeforeAttack = captureAliveSnapshot(battle_);
    const int hpBeforeAttack = pendingTarget_->currentHp;
    const bool hit = battle_.rollAttackHit(*pendingTarget_);
    resolveAttack(*selectedUnit_, *pendingTarget_, battle_.combatDefenseBonus(*pendingTarget_, *selectedUnit_), hit);
    if (hit && selectedUnit_->weapon.causesKnockback && pendingTarget_->isAlive())
        battle_.applyKnockback(*selectedUnit_, *pendingTarget_);
    if (hit && attack->second.bonusDamage > 0 && pendingTarget_->isAlive())
        pendingTarget_->currentHp = std::max(0, pendingTarget_->currentHp - attack->second.bonusDamage);
    lastDamage_ = std::max(0, hpBeforeAttack - pendingTarget_->currentHp);
    lastAttackHit_ = lastDamage_ > 0;
    if (hit && attack->second.appliesMoveDown && pendingTarget_->isAlive()) applyMoveDown(*pendingTarget_);
    if (hit && pendingTarget_->isAlive())
        for (StatusEffectType effect : attack->second.statuses) applyStatusEffect(*pendingTarget_, effect);
    emitUnitDefeatedEvents(battle_, aliveBeforeAttack);

    consumeSkillCharge(*selectedUnit_, pendingSkillSlot_);
    if (!finishPlayerAction(*selectedUnit_, ActionKind::Skill)) return;
    selectedUnit_ = nullptr;
    pendingTarget_ = nullptr;
    pendingSkillSlot_ = -1;
    skillTargetTiles_.clear();
    reachableTiles_.clear();
    attackRangeTiles_.clear();
    inputState_ = BattleInputState::SelectUnit;
    evaluateOutcome();
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
    if (!finishPlayerAction(*selectedUnit_, ActionKind::Item)) return true;
    selectedUnit_ = nullptr;
    boardTargetTiles_.clear();
    attackRangeTiles_.clear();
    inputState_ = BattleInputState::SelectUnit;
    evaluateOutcome();
    return true;
}

void BattleController::chooseWait() {
    if (inputState_ != BattleInputState::SelectAction || !selectedUnit_) return;

    // 古参守備兵`immovable_stance`(不動の構え): a Passive skill with no
    // charge/target step - it just auto-triggers the instant Wait is
    // confirmed, if equipped in either slot.
    for (const SkillSlotState& slot : selectedUnit_->skillSlots) {
        if (slot.skillId == "immovable_stance") {
            selectedUnit_->immovableStanceActive = true;
            selectedUnit_->immovableStanceJustGranted = true;
            break;
        }
    }
    // 重装兵`brace_for_impact`(衝撃防御): same auto-trigger-on-Wait shape as
    // immovable_stance above.
    for (const SkillSlotState& slot : selectedUnit_->skillSlots) {
        if (slot.skillId == "brace_for_impact") {
            selectedUnit_->braceForImpactActive = true;
            selectedUnit_->braceForImpactJustGranted = true;
            break;
        }
    }
    if (!finishPlayerAction(*selectedUnit_, ActionKind::Wait)) return;
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
    if (isTargetable) {
        pendingTarget_ = battle_.unitAt(pos);
        if (!pendingTarget_) return;
        inputState_ = BattleInputState::ConfirmAttack;
        return;
    }

    bool isObjectTargetable = false;
    for (const GridPos& tile : objectTargetableTiles_) {
        if (tile == pos) {
            isObjectTargetable = true;
            break;
        }
    }
    if (!isObjectTargetable) return;

    pendingObjectTarget_ = battle_.objectAt(pos);
    if (!pendingObjectTarget_) return;
    inputState_ = BattleInputState::ConfirmObjectAttack;
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
         inputState_ != BattleInputState::SelectInteractTarget &&
         inputState_ != BattleInputState::ConfirmAttack &&
         inputState_ != BattleInputState::ConfirmSkillAttack &&
         inputState_ != BattleInputState::ConfirmObjectAttack) ||
        !selectedUnit_) {
        return;
    }

    pendingTarget_ = nullptr;
    pendingObjectTarget_ = nullptr;
    targetableTiles_.clear();
    objectTargetableTiles_.clear();
    objectInteractableTiles_.clear();
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
    pendingTarget_ = nullptr;
    if (!finishPlayerAction(*selectedUnit_, ActionKind::Attack)) return;

    selectedUnit_ = nullptr;
    reachableTiles_.clear();
    targetableTiles_.clear();
    objectTargetableTiles_.clear();
    attackRangeTiles_.clear();
    healableTiles_.clear();
    boardTargetTiles_.clear();
    inputState_ = BattleInputState::SelectUnit;
    evaluateOutcome();
}

void BattleController::confirmObjectAttack() {
    if (inputState_ != BattleInputState::ConfirmObjectAttack || !selectedUnit_ || !pendingObjectTarget_) return;

    // docs/battle_objects.md: ObjectDestroyedEvent fires exactly once, the
    // moment durability first reaches 0 - resolveObjectAttack()'s return
    // value already reports that instant. Partial (non-lethal) damage
    // leaves the object's BattleObjectStateKind unchanged, so no
    // ObjectStateChangedEvent applies here (that event is for
    // resolveObjectInteraction()'s Active/Disabled/Opened transitions).
    const bool destroyedNow = resolveObjectAttack(battle_, *selectedUnit_, *pendingObjectTarget_);
    if (destroyedNow) {
        handleObjectiveEvent(battle_.missionState(),
                             BattleEvent{battle_.issueEventId(), 0,
                                        ObjectDestroyedEvent{pendingObjectTarget_->id}});
    }
    pendingObjectTarget_ = nullptr;
    if (!finishPlayerAction(*selectedUnit_, ActionKind::Attack)) return;

    selectedUnit_ = nullptr;
    reachableTiles_.clear();
    targetableTiles_.clear();
    objectTargetableTiles_.clear();
    attackRangeTiles_.clear();
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
        // isPresent() (not isAlive()): a retreated enemy (docs/
        // enemy_ai_rules.md) has isAlive()==true but must never be picked
        // again - takeEnemyTurn() would no-op on it (see its own isPresent()
        // guard) without ever marking it acted, stalling this loop forever
        // once a later beginEnemyPhase() resets its hasActed flag.
        if (u.team == Team::Enemy && u.isPresent() && !u.hasActed) return &u;
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
        battle_.clearTrailblazedTiles(); // 辺境斥候`trailblaze`: "このPlayer Phase中だけ"
        emitUnitDefeatedEvents(battle_, aliveBeforePhaseEnd);
        handleObjectiveEvent(battle_.missionState(),
                             {battle_.issueEventId(), 0, PhaseEndedEvent{Phase::PlayerPhase, battle_.round()}});
        battle_.beginEnemyPhase();
        enemyReservations_.clear();
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
        if (Unit* attacked = takeEnemyTurn(battle_, *next, &enemyReservations_)) {
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
    // docs/mission_objectives.md「地点維持」: HoldTile's consecutive-round
    // counting needs live board occupancy, not just the RoundEndedEvent
    // payload, so it's a dedicated call at the same point rather than routed
    // through handleObjectiveEvent() (see resolveHoldTileRoundEnd()'s own doc
    // comment).
    resolveHoldTileRoundEnd(battle_);
    battle_.beginPlayerPhase();
    // 辺境工兵`rapid_barricade`(即席防壁)「次の自軍Phase開始時に消滅」: unlike
    // `field_barricade`(固有能力「野戦工作」、永続)、この専用definitionIdの
    // 設置物だけを自軍Phase開始のたびに破棄する。
    {
        std::vector<BattleObjectId> expiredBarricadeIds;
        for (const BattleObjectState& object : battle_.objects()) {
            if (object.definitionId == "rapid_barricade" && object.state != BattleObjectStateKind::Destroyed)
                expiredBarricadeIds.push_back(object.id);
        }
        for (const BattleObjectId& id : expiredBarricadeIds) {
            if (BattleObjectState* object = battle_.findObject(id)) {
                object->durability = 0;
                object->state = BattleObjectStateKind::Destroyed;
                handleObjectiveEvent(battle_.missionState(),
                                     BattleEvent{battle_.issueEventId(), 0, ObjectDestroyedEvent{object->id}});
            }
        }
    }
    // Player Phase is starting: refill/tick down player-side skill charges.
    refreshSkillChargesOnPhaseStart(battle_, Team::Player);
    handleObjectiveEvent(battle_.missionState(),
                         {battle_.issueEventId(), 0, PhaseStartedEvent{Phase::PlayerPhase, battle_.round()}});
    selectedUnit_ = nullptr;
    inputState_ = BattleInputState::SelectUnit;
}

} // namespace jf
