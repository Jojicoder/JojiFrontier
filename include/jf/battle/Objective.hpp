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
// (matches docs/implementation_roadmap.md Phase 1 item 3): the remaining 7
// kinds (SecureTiles/SurviveRounds/EscapeUnits/OperateObject/DestroyObject/
// ProtectUnit/DefeatWithCondition) need the BattleObject/reinforcement model,
// which isn't built yet.
enum class ObjectiveKind {
    EliminateTeam,
    DefeatUnit,
    SecureTile
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

// Only the fields the 3 implemented kinds need.
struct ObjectiveTarget {
    Team team = Team::Enemy;         // EliminateTeam
    std::string unitId;               // DefeatUnit
    GridPos tile{};                   // SecureTile
    Team securingTeam = Team::Player; // SecureTile: only this team's actions credit it
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
    // SecureTile: which unit(s) already credited it, so re-ending an action
    // on the same tile doesn't re-fire (docs: "同じ対象を複数回調査してcurrent
    // を増やせないよう").
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
