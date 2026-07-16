#include "jf/battle/ObjectiveTracker.hpp"

#include <unordered_map>
#include <unordered_set>

namespace jf {

namespace {
bool objectiveSatisfied(const BattleState& battle, const ObjectiveDefinition& def,
                         const ObjectiveProgress& progress) {
    switch (def.kind) {
        case ObjectiveKind::EliminateTeam:
            // docs/enemy_ai_rules.md "撤退と降伏": a unit that retreated off
            // the field is no longer a threat, so it satisfies "eliminate"
            // the same as a defeated one - isPresent() covers both (unlike
            // DefeatUnit below, which targets a specific unit and must not
            // treat a mere retreat as satisfying it).
            for (const Unit& unit : battle.units()) {
                if (unit.team == def.target.team && unit.isPresent()) return false;
            }
            return true;
        case ObjectiveKind::DefeatUnit: {
            // A target id that doesn't exist is a mission-authoring error,
            // not an automatic win: treat it as unsatisfiable rather than
            // trivially true.
            const Unit* unit = battle.findUnit(def.target.unitId);
            return unit != nullptr && !unit->isAlive();
        }
        case ObjectiveKind::SecureTile:
            return progress.status == ObjectiveStatus::Completed;
    }
    return false;
}

std::unordered_map<ObjectiveGroupId, std::vector<const ObjectiveDefinition*>> primaryGroups(
    const BattleMissionState& mission) {
    std::unordered_map<ObjectiveGroupId, std::vector<const ObjectiveDefinition*>> byGroup;
    for (const ObjectiveDefinition& def : mission.definitions) {
        if (def.primary) byGroup[def.groupId].push_back(&def);
    }
    return byGroup;
}
} // namespace

void handleObjectiveEvent(BattleMissionState& mission, const BattleEvent& event) {
    if (mission.consumedEventIds.count(event.id)) return;
    mission.consumedEventIds.insert(event.id);

    const auto* resolved = std::get_if<ActionResolvedEvent>(&event.payload);
    if (!resolved) return;

    for (const ObjectiveDefinition& def : mission.definitions) {
        if (def.kind != ObjectiveKind::SecureTile) continue;
        // Only the side the objective is written for can secure it - an
        // enemy ending its action on a player objective's tile must not
        // credit it.
        if (resolved->actorTeam != def.target.securingTeam) continue;
        ObjectiveProgress& progress = mission.progress[def.id];
        if (progress.status == ObjectiveStatus::Completed) continue;
        if (resolved->endPosition.row == def.target.tile.row && resolved->endPosition.col == def.target.tile.col) {
            progress.status = ObjectiveStatus::Completed;
            progress.creditedTargetIds.insert(resolved->actorUnitId);
        }
    }
}

void syncObjectiveProgress(BattleState& battle) {
    BattleMissionState& mission = battle.missionState();
    auto byGroup = primaryGroups(mission);

    for (const ObjectiveGroupDefinition& group : mission.groups) {
        auto it = byGroup.find(group.id);
        if (it == byGroup.end()) continue;

        if (group.rule == ObjectiveGroupRule::All) {
            bool allSatisfied = true;
            for (const ObjectiveDefinition* def : it->second) {
                const ObjectiveProgress& progress = mission.progress.at(def->id);
                if (progress.status == ObjectiveStatus::Completed) continue; // locked in already
                if (!objectiveSatisfied(battle, *def, progress)) {
                    allSatisfied = false;
                    break;
                }
            }
            if (allSatisfied) {
                for (const ObjectiveDefinition* def : it->second) {
                    ObjectiveProgress& progress = mission.progress.at(def->id);
                    if (progress.status == ObjectiveStatus::Active) progress.status = ObjectiveStatus::Completed;
                }
            }
            continue;
        }

        // Any: the race is over once any member is already Completed - just
        // supersede stragglers. Otherwise look for the first (in Definition
        // order) still-Active member that's now satisfied and crown it.
        bool alreadyResolved = false;
        for (const ObjectiveDefinition* def : it->second) {
            if (mission.progress.at(def->id).status == ObjectiveStatus::Completed) {
                alreadyResolved = true;
                break;
            }
        }
        const ObjectiveDefinition* winner = nullptr;
        if (!alreadyResolved) {
            for (const ObjectiveDefinition* def : it->second) {
                const ObjectiveProgress& progress = mission.progress.at(def->id);
                if (progress.status != ObjectiveStatus::Active) continue;
                if (objectiveSatisfied(battle, *def, progress)) {
                    winner = def;
                    break;
                }
            }
            if (winner) mission.progress.at(winner->id).status = ObjectiveStatus::Completed;
        }
        if (winner || alreadyResolved) {
            for (const ObjectiveDefinition* def : it->second) {
                ObjectiveProgress& progress = mission.progress.at(def->id);
                if (progress.status == ObjectiveStatus::Active) progress.status = ObjectiveStatus::Superseded;
            }
        }
    }
}

BattleOutcome evaluateBattleOutcome(const BattleState& battle) {
    BattleOutcome outcome;
    if (battle.allPlayersDefeated()) {
        outcome.kind = BattleOutcomeKind::Defeat;
        return outcome;
    }

    const BattleMissionState& mission = battle.missionState();
    auto byGroup = primaryGroups(mission);

    bool anyPrimaryGroup = false;
    bool allGroupsSatisfied = true;
    for (const ObjectiveGroupDefinition& group : mission.groups) {
        auto it = byGroup.find(group.id);
        if (it == byGroup.end()) continue;
        anyPrimaryGroup = true;

        bool groupSatisfied = group.rule == ObjectiveGroupRule::All;
        for (const ObjectiveDefinition* def : it->second) {
            bool completed = mission.progress.at(def->id).status == ObjectiveStatus::Completed;
            if (completed) outcome.completedPrimaryObjectives.push_back(def->id);
            if (group.rule == ObjectiveGroupRule::All) groupSatisfied = groupSatisfied && completed;
            else groupSatisfied = groupSatisfied || completed;
        }
        if (!groupSatisfied) allGroupsSatisfied = false;
    }

    if (anyPrimaryGroup && allGroupsSatisfied && !battle.hasPendingRequiredEnemyReinforcements())
        outcome.kind = BattleOutcomeKind::Victory;
    return outcome;
}

std::vector<std::string> validateBattleMission(const BattleMissionState& mission, const BattleState& battle) {
    std::vector<std::string> errors;

    std::unordered_set<ObjectiveId> seenObjectiveIds;
    for (const ObjectiveDefinition& def : mission.definitions) {
        if (!seenObjectiveIds.insert(def.id).second) errors.push_back("duplicate objective id: " + def.id);
    }
    std::unordered_set<ObjectiveGroupId> groupIds;
    for (const ObjectiveGroupDefinition& group : mission.groups) {
        if (!groupIds.insert(group.id).second) errors.push_back("duplicate group id: " + group.id);
    }
    for (const ObjectiveDefinition& def : mission.definitions) {
        if (!groupIds.count(def.groupId)) {
            errors.push_back("objective '" + def.id + "' references unknown group '" + def.groupId + "'");
        }
    }

    int primaryGroupCount = 0;
    for (const auto& [groupId, defs] : primaryGroups(mission)) {
        if (!defs.empty()) ++primaryGroupCount;
    }
    if (primaryGroupCount != 1) {
        errors.push_back("expected exactly one non-empty primary objective group, found " +
                         std::to_string(primaryGroupCount));
    }

    for (const ObjectiveDefinition& def : mission.definitions) {
        if (def.kind == ObjectiveKind::DefeatUnit) {
            const Unit* unit = battle.findUnit(def.target.unitId);
            if (!unit) {
                errors.push_back("DefeatUnit objective '" + def.id + "' targets unknown unit '" +
                                 def.target.unitId + "'");
            } else if (unit->team != def.target.team) {
                errors.push_back("DefeatUnit objective '" + def.id + "' targets a unit on the wrong team");
            }
        } else if (def.kind == ObjectiveKind::SecureTile) {
            if (!isInBounds(def.target.tile)) {
                errors.push_back("SecureTile objective '" + def.id + "' targets an out-of-bounds tile");
            } else {
                if (!isPassable(battle.terrainAt(def.target.tile))) {
                    errors.push_back("SecureTile objective '" + def.id + "' targets an impassable tile");
                }
                if (battle.unitAt(def.target.tile)) {
                    errors.push_back("SecureTile objective '" + def.id +
                                     "' targets a tile occupied at battle start");
                }
            }
        }
    }

    for (const ObjectiveDefinition& def : mission.definitions) {
        if (!mission.progress.count(def.id)) {
            errors.push_back("objective '" + def.id + "' has no ObjectiveProgress entry");
        }
    }

    return errors;
}

AliveSnapshot captureAliveSnapshot(const BattleState& battle) {
    AliveSnapshot snapshot;
    for (const Unit& unit : battle.units()) snapshot[unit.id] = unit.isAlive();
    return snapshot;
}

void emitUnitDefeatedEvents(BattleState& battle, const AliveSnapshot& before) {
    // docs/battle_resolution_contract.md "同時発生": "同時撃破はUnit ID順に
    // Event化する" - iterate battle.units() (a fixed, deterministic vector)
    // rather than `before` itself, which is an unordered_map and would give
    // a different, hash-dependent event order on every run for simultaneous
    // defeats (breaking "同じRoot ActionとSeedでEvent順が一致").
    for (Unit& unit : battle.units()) {
        auto it = before.find(unit.id);
        if (it == before.end() || !it->second) continue; // wasn't alive before, nothing to report
        if (!unit.isAlive()) {
            // docs/boss_common_rules.md "Bossの退場理由": 灰角大猪はHP0後を
            // ScriptedWithdrawalとして扱う(撃破相当) - every other unit's
            // defeat is the plain Defeated case.
            unit.exitReason =
                unit.unitClass == UnitClass::AshenhornBoar ? UnitExitReason::ScriptedWithdrawal : UnitExitReason::Defeated;
            BattleEvent event{battle.issueEventId(), 0,
                              UnitDefeatedEvent{unit.id, unit.team, unit.exitReason}};
            handleObjectiveEvent(battle.missionState(), event);
        }
    }
}

} // namespace jf
