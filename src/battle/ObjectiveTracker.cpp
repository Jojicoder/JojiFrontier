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
        case ObjectiveKind::EscapeUnits:
            // Both are credited by handleObjectiveEvent() below, which
            // already sets Completed the instant each one's own completion
            // condition (one credit for SecureTile, requiredEscapeCount
            // distinct credits for EscapeUnits) is met.
            return progress.status == ObjectiveStatus::Completed;
        case ObjectiveKind::DestroyObject: {
            // Same "unknown target is unsatisfiable, not an automatic win"
            // rule as DefeatUnit - a mission-authoring mistake must not
            // silently pass.
            const BattleObjectState* object = battle.findObject(def.target.objectId);
            return object != nullptr && object->state == BattleObjectStateKind::Destroyed;
        }
        case ObjectiveKind::SurviveRounds:
            return battle.round() > def.target.surviveUntilRound;
        case ObjectiveKind::ProtectUnit: {
            // Not actually reached through the generic primary-group loop
            // (ProtectUnit is always primary=false - see Objective.hpp's
            // comment on why it has its own dedicated pass in
            // syncObjectiveProgress() instead); kept here anyway so this
            // switch stays exhaustive and the definition lives in one place.
            const Unit* unit = battle.findUnit(def.target.unitId);
            return unit != nullptr && unit->isPresent();
        }
        case ObjectiveKind::OperateObject: {
            // Same "unknown target is unsatisfiable" rule as DestroyObject.
            const BattleObjectState* object = battle.findObject(def.target.objectId);
            return object != nullptr && object->interactionCount > 0;
        }
        case ObjectiveKind::HoldTile:
            // Credited by handleObjectiveEvent()'s RoundEndedEvent handling
            // below, the same "already Completed" read as SecureTile/
            // EscapeUnits - the consecutive-round counting itself can't be
            // Live-evaluated from current BattleState alone.
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
        if (def.kind != ObjectiveKind::SecureTile && def.kind != ObjectiveKind::EscapeUnits) continue;
        // Only the side the objective is written for can secure/escape it -
        // an enemy ending its action on a player objective's tile must not
        // credit it.
        if (resolved->actorTeam != def.target.securingTeam) continue;
        ObjectiveProgress& progress = mission.progress[def.id];
        if (progress.status == ObjectiveStatus::Completed) continue;
        if (resolved->endPosition.row != def.target.tile.row || resolved->endPosition.col != def.target.tile.col)
            continue;
        progress.creditedTargetIds.insert(resolved->actorUnitId);
        // SecureTile: any single credit completes it. EscapeUnits: needs
        // requiredEscapeCount DISTINCT units (creditedTargetIds is a set, so
        // the same unit ending multiple actions here only counts once).
        if (def.kind == ObjectiveKind::SecureTile ||
            progress.creditedTargetIds.size() >= static_cast<std::size_t>(def.target.requiredEscapeCount)) {
            progress.status = ObjectiveStatus::Completed;
        }
    }
}

void resolveHoldTileRoundEnd(BattleState& battle) {
    BattleMissionState& mission = battle.missionState();
    for (const ObjectiveDefinition& def : mission.definitions) {
        if (def.kind != ObjectiveKind::HoldTile) continue;
        ObjectiveProgress& progress = mission.progress[def.id];
        if (progress.status == ObjectiveStatus::Completed) continue;
        const Unit* holder = battle.unitAt(def.target.tile);
        const bool held = holder != nullptr && holder->team == def.target.securingTeam && holder->isPresent();
        progress.consecutiveRoundsHeld = held ? progress.consecutiveRoundsHeld + 1 : 0;
        if (progress.consecutiveRoundsHeld >= def.target.requiredHoldRounds) {
            progress.status = ObjectiveStatus::Completed;
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

    // docs/mission_objectives.md "対象保護": dedicated pass, deliberately
    // separate from the primary-group loop above (ProtectUnit is always
    // primary=false, so it never appears in `byGroup`, and its "satisfied"
    // check is a falling edge to punish rather than a rising edge to
    // celebrate - see Objective.hpp's comment on ObjectiveKind::ProtectUnit
    // for why routing it through the generic Completed-on-satisfied loop
    // would be wrong). Once Failed, stays Failed - never reverts.
    for (const ObjectiveDefinition& def : mission.definitions) {
        if (def.kind != ObjectiveKind::ProtectUnit) continue;
        ObjectiveProgress& progress = mission.progress.at(def.id);
        if (progress.status != ObjectiveStatus::Active) continue;
        if (!objectiveSatisfied(battle, def, progress)) progress.status = ObjectiveStatus::Failed;
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
        } else if (def.kind == ObjectiveKind::SurviveRounds) {
            // round_ starts at 1 (BattleState.hpp) - a target of 0 would be
            // trivially satisfied the instant the battle starts.
            if (def.target.surviveUntilRound < 1) {
                errors.push_back("SurviveRounds objective '" + def.id + "' needs surviveUntilRound >= 1");
            }
        } else if (def.kind == ObjectiveKind::DestroyObject) {
            const BattleObjectState* object = battle.findObject(def.target.objectId);
            if (!object) {
                errors.push_back("DestroyObject objective '" + def.id + "' targets unknown object '" +
                                 def.target.objectId + "'");
            } else {
                const BattleObjectDefinition* objectDef = battle.objectDefinition(object->definitionId);
                if (!objectDef || !objectDef->canBeAttacked) {
                    errors.push_back("DestroyObject objective '" + def.id +
                                     "' targets an object that can't be destroyed");
                }
            }
        } else if (def.kind == ObjectiveKind::EscapeUnits) {
            if (def.target.requiredEscapeCount < 1) {
                errors.push_back("EscapeUnits objective '" + def.id + "' needs requiredEscapeCount >= 1");
            }
            if (!isInBounds(def.target.tile)) {
                errors.push_back("EscapeUnits objective '" + def.id + "' targets an out-of-bounds tile");
            } else {
                if (!isPassable(battle.terrainAt(def.target.tile))) {
                    errors.push_back("EscapeUnits objective '" + def.id + "' targets an impassable tile");
                }
                if (battle.unitAt(def.target.tile)) {
                    errors.push_back("EscapeUnits objective '" + def.id +
                                     "' targets a tile occupied at battle start");
                }
            }
        } else if (def.kind == ObjectiveKind::ProtectUnit) {
            const Unit* unit = battle.findUnit(def.target.unitId);
            if (!unit) {
                errors.push_back("ProtectUnit objective '" + def.id + "' targets unknown unit '" +
                                 def.target.unitId + "'");
            }
            if (def.primary) {
                errors.push_back("ProtectUnit objective '" + def.id + "' must not be primary (secondary-only)");
            }
        } else if (def.kind == ObjectiveKind::HoldTile) {
            if (def.target.requiredHoldRounds < 1) {
                errors.push_back("HoldTile objective '" + def.id + "' needs requiredHoldRounds >= 1");
            }
            if (!isInBounds(def.target.tile)) {
                errors.push_back("HoldTile objective '" + def.id + "' targets an out-of-bounds tile");
            } else {
                if (!isPassable(battle.terrainAt(def.target.tile))) {
                    errors.push_back("HoldTile objective '" + def.id + "' targets an impassable tile");
                }
                if (battle.unitAt(def.target.tile)) {
                    errors.push_back("HoldTile objective '" + def.id + "' targets a tile occupied at battle start");
                }
            }
        } else if (def.kind == ObjectiveKind::OperateObject) {
            const BattleObjectState* object = battle.findObject(def.target.objectId);
            if (!object) {
                errors.push_back("OperateObject objective '" + def.id + "' targets unknown object '" +
                                 def.target.objectId + "'");
            } else {
                const BattleObjectDefinition* objectDef = battle.objectDefinition(object->definitionId);
                if (!objectDef || !objectDef->interaction) {
                    errors.push_back("OperateObject objective '" + def.id +
                                     "' targets an object with no Interact defined");
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
