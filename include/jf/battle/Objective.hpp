#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "jf/battle/BattleEvents.hpp"
#include "jf/core/Grid.hpp"
#include "jf/core/UnitClass.hpp"

namespace jf {

using ObjectiveId = std::string;
using ObjectiveGroupId = std::string;

// docs/mission_objectives.md "目標の種類". Deliberate subset for this pass
// (matches docs/implementation_roadmap.md Phase 1 item 3): the remaining 2
// kinds (SecureTiles/DefeatWithCondition) still need further Mechanic work
// (multi-tile grouping, condition-tracking) of their own.
enum class ObjectiveKind {
    EliminateTeam,
    DefeatUnit,
    SecureTile,
    // docs/mission_objectives.md "脱出": "条件を満たす必要人数が脱出マスで
    //行動終了" - same ActionResolved-credit mechanism as SecureTile (see
    // handleObjectiveEvent()), except completion requires
    // target.requiredEscapeCount DISTINCT units having credited it rather
    // than just one. Deliberately doesn't cover the doc's alternate
    // `UnitRetreated`-driven escape path (an AI-controlled unit fleeing off
    // the board via ExitPoint rather than a player ending an action there) -
    // that needs ExitPoint's real board behavior, not just this Event kind.
    EscapeUnits,
    // docs/battle_objects.md's "耐久とダメージ"/"破壊後の状態": satisfied once
    // the target Object's BattleObjectState reaches Destroyed. Live-evaluated
    // against BattleState (like DefeatUnit), no event-driven credit needed -
    // ObjectDestroyedEvent already fires exactly once per Object elsewhere
    // (BattleObjectResolver.hpp), this Objective just reads the resulting
    // state rather than consuming that Event itself.
    DestroyObject,
    // docs/mission_objectives.md "防衛": "指定ラウンド終了まで敗北条件を回避" -
    // satisfied once battle.round() has moved past target.surviveUntilRound
    // (i.e. that round actually ended). Needs no extra "avoided defeat"
    // check of its own: evaluateBattleOutcome() already checks
    // allPlayersDefeated() before any primary group, so a defeat never lets
    // this objective's satisfaction matter.
    SurviveRounds,
    // docs/mission_objectives.md "対象保護": "戦闘終了時に指定対象が撤退して
    //いない" - always primary=false (the doc's own table: primary No,
    // secondary Yes), unlike every other kind here. Structurally different
    // from the rest: "satisfied" isn't a rising edge to celebrate the
    // instant it's true (the target being present is the default state,
    // true from turn 1), it's a falling edge to punish - so it's NOT
    // evaluated through the generic primary-group Live-evaluation loop in
    // syncObjectiveProgress() (which would incorrectly mark it Completed on
    // the very first sync). Instead syncObjectiveProgress() has a dedicated
    // pass that only ever moves it Active -> Failed the moment
    // target.unitId stops being present (defeated or retreated - both make
    // Unit::isPresent() false), and otherwise leaves it Active indefinitely.
    // Whatever eventually reads this for reward-granting (not connected to
    // any shipped content yet - see docs/implementation_status.md) should
    // treat "still Active when the battle ends in Victory" as success, not
    // wait for a Completed transition that this Kind deliberately never
    // makes on its own.
    ProtectUnit,
    // docs/mission_objectives.md "装置操作": "指定ユニット・兵種が装置で専用
    //行動を完了" - Live-evaluated like DestroyObject (reads
    // target.objectId's BattleObjectState.interactionCount directly,
    // reusing the same field DestroyObject uses for its own object
    // reference). The unit/class restriction from the doc's wording is
    // already enforced by resolveObjectInteraction()'s
    // ObjectInteractionDefinition::allowedClasses at Interact-resolution
    // time (BattleController::selectInteractTarget()) - interactionCount
    // can only have advanced if an allowed class actually did it, so this
    // Objective doesn't need to separately re-check who performed it.
    OperateObject
};

enum class ObjectiveStatus {
    Hidden,
    Active,
    Completed,
    Failed,
    Superseded
};

enum class ObjectiveGroupRule {
    All, // AND
    Any  // OR
};

// Only the fields the implemented kinds need.
struct ObjectiveTarget {
    Team team = Team::Enemy;         // EliminateTeam
    std::string unitId;               // DefeatUnit
    GridPos tile{};                   // SecureTile
    Team securingTeam = Team::Player; // SecureTile: only this team's actions credit it
    std::string objectId;             // DestroyObject
    int surviveUntilRound = 0;        // SurviveRounds
    int requiredEscapeCount = 1;      // EscapeUnits: distinct units needed on `tile`
};

struct ObjectiveDefinition {
    ObjectiveId id;
    ObjectiveKind kind;
    bool primary = false;
    ObjectiveGroupId groupId;
    ObjectiveTarget target;
};

struct ObjectiveGroupDefinition {
    ObjectiveGroupId id;
    ObjectiveGroupRule rule = ObjectiveGroupRule::All;
};

struct ObjectiveProgress {
    ObjectiveId id;
    ObjectiveStatus status = ObjectiveStatus::Active;
    // SecureTile/EscapeUnits: which unit(s) already credited it, so
    // re-ending an action on the same tile doesn't re-fire (docs: "同じ対象を
    // 複数回調査してcurrentを増やせないよう") and, for EscapeUnits, so the
    // same unit ending multiple actions there doesn't count twice toward
    // requiredEscapeCount.
    std::unordered_set<std::string> creditedTargetIds;
};

// docs/mission_objectives.md "所有権と責務": BattleState owns this.
// Definitions never change after battle start; only progress/consumedEventIds
// mutate. EliminateTeam/DefeatUnit are evaluated live against BattleState
// (no stored progress needed); only SecureTile needs event-driven credit.
struct BattleMissionState {
    std::vector<ObjectiveDefinition> definitions;
    std::vector<ObjectiveGroupDefinition> groups;
    std::unordered_map<ObjectiveId, ObjectiveProgress> progress;
    std::unordered_set<BattleEventId> consumedEventIds;
    BattleEventId nextEventId = 1; // docs: "戦闘開始時に1へ戻し、発行ごとに単調増加"
};

// Matches the game's existing behavior exactly (defeat every enemy =
// victory) so any battle that doesn't define a custom mission behaves the
// same as before this system existed.
BattleMissionState defaultEliminateEnemiesMission();

} // namespace jf
