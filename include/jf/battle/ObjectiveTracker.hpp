#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "jf/battle/BattleEvents.hpp"
#include "jf/battle/BattleState.hpp"
#include "jf/battle/Objective.hpp"

namespace jf {

enum class BattleOutcomeKind { Ongoing, Victory, Defeat };

struct BattleOutcome {
    BattleOutcomeKind kind = BattleOutcomeKind::Ongoing;
    std::vector<ObjectiveId> completedPrimaryObjectives;
};

// docs/mission_objectives.md "戦闘イベントの実装契約"/"勝敗評価API". These are
// free functions (matching this codebase's existing StatusEffects/
// SkillCharges convention) operating directly on BattleState::missionState()
// rather than a separate stateful tracker object, so there is only ever one
// copy of the mission's progress.

// Feeds one event into `mission`, ignoring it if its id was already consumed
// (docs: "イベントの重複防止"). Only SecureTile currently needs event-driven
// credit; EliminateTeam/DefeatUnit are evaluated live against BattleState.
// SecureTile only credits an ActionResolvedEvent whose actor's team matches
// the objective's `target.securingTeam`.
void handleObjectiveEvent(BattleMissionState& mission, const BattleEvent& event);

// docs/mission_objectives.md "進捗同期": mutates ObjectiveProgress::status for
// the live-evaluated kinds (EliminateTeam/DefeatUnit) and never reverts an
// already-Completed objective. All-group: once every member is satisfied,
// every (still-Active) member becomes Completed together. Any-group: only
// the FIRST satisfied member in `mission.definitions` order becomes
// Completed; every other still-Active member in that group becomes
// Superseded (once any member resolves the group, even later calls keep
// superseding stragglers rather than re-racing). Call this once per Batch,
// before evaluateBattleOutcome().
void syncObjectiveProgress(BattleState& battle);

// 判定順序 (mission_objectives.md): the fixed defeat rule (all players
// defeated) is evaluated before any primary objective group, matching the
// doc's explicit allowance to keep allPlayersDefeated() as an internal
// condition rather than data-driven. All primary groups must be satisfied
// (top-level combination beyond that - mixing multiple primary groups with
// their own AND/OR - is left for when a mission actually needs it). Pure:
// reads BattleMissionState::progress (already synced by
// syncObjectiveProgress()) rather than recomputing or mutating anything -
// docs/mission_objectives.md "進捗同期": "結果画面と報酬処理は再計算せず、
// 確定済みObjectiveProgress.statusを参照する".
BattleOutcome evaluateBattleOutcome(const BattleState& battle);

// docs/mission_objectives.md "戦闘開始時の検証". Returns one human-readable
// (English, developer-facing) message per problem found; empty means the
// mission is well-formed. Checked: objective/group id uniqueness, every
// objective references an existing group, exactly one non-empty primary
// group, DefeatUnit's target unit exists with the expected team, SecureTile
// targets an in-bounds/passable/unoccupied tile, and every definition has a
// matching ObjectiveProgress entry.
std::vector<std::string> validateBattleMission(const BattleMissionState& mission, const BattleState& battle);

// Per-unit id -> was-alive-at-capture-time snapshot, for detecting an
// alive->dead transition around a chunk of work (combat resolution, status
// effect damage) so UnitDefeatedEvent fires exactly once per real defeat.
using AliveSnapshot = std::unordered_map<std::string, bool>;
AliveSnapshot captureAliveSnapshot(const BattleState& battle);

// Emits a UnitDefeatedEvent for every unit that was alive in `before` but
// isn't anymore, using battle.issueEventId() for each. Call this after the
// work that might have killed someone, before anything that assumes
// defeats are already reflected in Objective state (e.g. ActionResolved).
void emitUnitDefeatedEvents(BattleState& battle, const AliveSnapshot& before);

} // namespace jf
