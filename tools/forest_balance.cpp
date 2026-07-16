#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "jf/battle/BattleController.hpp"
#include "jf/battle/BattleFactory.hpp"
#include "jf/battle/CombatResolver.hpp"
#include "jf/battle/SkillCharges.hpp"
#include "jf/core/Region.hpp"
#include "jf/data/GameData.hpp"

namespace {

using namespace jf;

enum class Policy { Direct, Tactical };

struct BattleResult {
    bool victory = false;
    bool timeout = false;
    int rounds = 0;
    int incapacitated = 0;
    int hp = 0;
    int maxHp = 0;
    std::vector<Unit> players;
};

struct Aggregate {
    int attempts = 0;
    int victories = 0;
    int timeouts = 0;
    int totalRounds = 0;
    int totalIncapacitated = 0;
    int anyIncapacitation = 0;
    long long totalHp = 0;
    long long totalMaxHp = 0;

    void add(const BattleResult& result) {
        ++attempts;
        victories += result.victory;
        timeouts += result.timeout;
        totalRounds += result.rounds;
        totalIncapacitated += result.incapacitated;
        anyIncapacitation += result.incapacitated > 0;
        totalHp += result.hp;
        totalMaxHp += result.maxHp;
    }
};

bool inAttackRange(const Unit& unit, GridPos from, GridPos target) {
    const int distance = manhattanDistance(from, target);
    return distance >= unit.minimumAttackRange() && distance <= unit.weapon.maxRange;
}

// docs/initial_skill_effects.md: a standard 2-slot loadout per class, picked
// from that class's Tier-1 skills, so this simulation actually exercises
// M4-A's skill executor instead of running skill-free (the balance numbers
// recorded before M4-A were all measured with zero skill usage). Only the 4
// classes actually present in data/units.json's playerParty are covered -
// Spearman/FrontierScout never appear in this party today.
void equipStandardLoadout(Unit& unit) {
    switch (unit.unitClass) {
        case UnitClass::MarchCaptain:
            unit.skillSlots[0].skillId = "hold_formation";
            unit.skillSlots[1].skillId = "advance_order";
            break;
        case UnitClass::VeteranGuard:
            unit.skillSlots[0].skillId = "provoke";
            unit.skillSlots[1].skillId = "extended_lockdown";
            break;
        case UnitClass::WatchArcher:
            unit.skillSlots[0].skillId = "suppressing_shot";
            unit.skillSlots[1].skillId = "overwatch";
            break;
        case UnitClass::DawnChirurgeon:
            unit.skillSlots[0].skillId = "emergency_treatment";
            unit.skillSlots[1].skillId = "cleanse";
            break;
        default:
            break;
    }
}

int nearestEnemyDistance(const BattleState& battle, GridPos from) {
    int best = 1000;
    for (const Unit& unit : battle.units()) {
        if (unit.team == Team::Enemy && unit.isAlive())
            best = std::min(best, manhattanDistance(from, unit.position));
    }
    return best;
}

int threatenedDamage(const BattleState& battle, const Unit& mover, GridPos tile) {
    int damage = 0;
    for (const Unit& enemy : battle.units()) {
        if (enemy.team != Team::Enemy || !enemy.isAlive()) continue;
        const int reach = enemy.effectiveMove() + enemy.weapon.maxRange;
        if (manhattanDistance(tile, enemy.position) <= reach)
            damage += computeDamage(enemy, mover, battle.combatDefenseBonus(mover, enemy));
    }
    return damage;
}

int allySupport(const BattleState& battle, const Unit& mover, GridPos tile) {
    int support = 0;
    for (const Unit& ally : battle.units()) {
        if (&ally == &mover || ally.team != Team::Player || !ally.isAlive()) continue;
        if (manhattanDistance(tile, ally.position) <= 2) ++support;
    }
    return support;
}

struct MoveChoice {
    GridPos tile{};
    int score = std::numeric_limits<int>::min();
};

MoveChoice chooseMove(const BattleState& battle, const Unit& unit,
                      const std::vector<GridPos>& reachable, Policy policy) {
    MoveChoice best{unit.position, std::numeric_limits<int>::min()};
    for (GridPos tile : reachable) {
        int score = -nearestEnemyDistance(battle, tile) * 8;
        bool canAttackFromTile = false;
        for (const Unit& enemy : battle.units()) {
            if (enemy.team != Team::Enemy || !enemy.isAlive() || !inAttackRange(unit, tile, enemy.position))
                continue;
            canAttackFromTile = true;
            const int damage = computeDamage(unit, enemy, battle.combatDefenseBonus(enemy, unit));
            score += damage * 12;
            if (damage >= enemy.currentHp) score += 500;
            if (enemy.isBoss) score += 25;
        }
        if (policy == Policy::Tactical) {
            score -= threatenedDamage(battle, unit, tile) * 5;
            score += allySupport(battle, unit, tile) * 12;

            // docs/regions/ashbough_forest.md "操作水準" names 4 specific
            // tactics skilled play is expected to use; the plain
            // threatenedDamage/allySupport terms above don't reproduce any
            // of them per-class. Approximate each:
            if (unit.unitClass == UnitClass::VeteranGuard) {
                // 古参守備兵の進路封鎖: screen the party by engaging first,
                // on top of the base pull-toward-enemies term above.
                score -= nearestEnemyDistance(battle, tile) * 40;
            } else if (unit.unitClass == UnitClass::DawnChirurgeon) {
                // 衛生兵保護: her own safety matters more than closing
                // distance - stacks with the base threatenedDamage weight.
                score -= threatenedDamage(battle, unit, tile) * 15;
            } else if (unit.unitClass == UnitClass::WatchArcher && canAttackFromTile) {
                // 弓の先制: once she can already hit something from here,
                // closing further only adds risk for no benefit - prefer
                // the farthest tile that still lets her attack.
                score += nearestEnemyDistance(battle, tile) * 6;
            }
        }
        if (score > best.score || (score == best.score && tile.col > best.tile.col)) best = {tile, score};
    }
    return best;
}

bool healIfUseful(BattleController& controller, Policy policy) {
    if (policy != Policy::Tactical || !controller.selectedUnit() ||
        controller.selectedUnit()->unitClass != UnitClass::DawnChirurgeon) return false;
    controller.chooseHeal();
    if (controller.inputState() != BattleInputState::SelectHealTarget) return false;
    Unit* best = nullptr;
    int missing = 0;
    for (GridPos tile : controller.healableTiles()) {
        Unit* candidate = controller.battle().unitAt(tile);
        if (!candidate) continue;
        const int candidateMissing = candidate->stats.maxHp - candidate->currentHp;
        if (candidateMissing > missing) {
            best = candidate;
            missing = candidateMissing;
        }
    }
    if (!best || missing < 6) {
        controller.cancelAttackSelection();
        return false;
    }
    controller.selectHealTarget(best->position);
    return true;
}

// Picks a target tile from controller.skillTargetTiles() for `skillId`,
// or nullopt if none of the candidates are actually worth spending the
// skill's charge on (e.g. cleanse when nobody has an active ailment).
std::optional<GridPos> pickSkillTarget(BattleController& controller, const std::string& skillId) {
    const std::vector<GridPos>& tiles = controller.skillTargetTiles();
    if (tiles.empty()) return std::nullopt;
    Unit* self = controller.selectedUnit();
    BattleState& battle = controller.battle();

    if (skillId == "emergency_treatment") {
        GridPos best = tiles.front();
        double worstRatio = 2.0;
        for (GridPos tile : tiles) {
            Unit* u = battle.unitAt(tile);
            if (!u) continue;
            double ratio = static_cast<double>(u->currentHp) / u->stats.maxHp;
            if (ratio < worstRatio) {
                worstRatio = ratio;
                best = tile;
            }
        }
        return best;
    }
    if (skillId == "cleanse") {
        for (GridPos tile : tiles) {
            Unit* u = battle.unitAt(tile);
            if (!u) continue;
            if (u->poisonRemainingProcs > 0 || u->burnRemainingProcs > 0 || u->moveDownActive ||
                u->defenseDownActive || u->staggerActive) {
                return tile;
            }
        }
        return std::nullopt; // nobody actually needs it - don't burn the charge
    }
    if (skillId == "provoke") {
        GridPos best = tiles.front();
        int bestDist = std::numeric_limits<int>::max();
        for (GridPos tile : tiles) {
            int dist = manhattanDistance(self->position, tile);
            if (dist < bestDist) {
                bestDist = dist;
                best = tile;
            }
        }
        return best;
    }
    if (skillId == "suppressing_shot" || skillId == "halting_thrust" || skillId == "ambush") {
        GridPos best = tiles.front();
        int bestScore = std::numeric_limits<int>::min();
        for (GridPos tile : tiles) {
            Unit* target = battle.unitAt(tile);
            if (!target) continue;
            const int damage = computeDamage(*self, *target, battle.combatDefenseBonus(*target, *self));
            int score = damage * 10 - target->currentHp;
            if (damage >= target->currentHp) score += 500;
            if (target->isBoss) score += 20;
            if (score > bestScore) {
                bestScore = score;
                best = tile;
            }
        }
        return best;
    }
    // advance_order/hold_formation-adjacent single-target skills, or
    // anything else that just needs *a* valid tile: the first candidate.
    return tiles.front();
}

// Tries every equipped slot whose skillId is in `kindIds`, in slot order,
// and uses the first one that both has a charge available and (per
// pickSkillTarget()) has a worthwhile target. Self-only skills (
// hold_formation/extended_lockdown/overwatch) resolve immediately inside
// chooseSkill() itself - no target step needed.
bool trySkillsOfKind(BattleController& controller, const std::vector<std::string>& kindIds) {
    Unit* unit = controller.selectedUnit();
    if (!unit) return false;
    for (int i = 0; i < static_cast<int>(unit->skillSlots.size()); ++i) {
        const std::string skillId = unit->skillSlots[static_cast<std::size_t>(i)].skillId;
        if (skillId.empty()) continue;
        if (std::find(kindIds.begin(), kindIds.end(), skillId) == kindIds.end()) continue;
        if (!skillSlotAvailable(*unit, i)) continue;

        controller.chooseSkill(i);
        if (controller.inputState() == BattleInputState::SelectUnit) return true; // self-resolved
        if (controller.inputState() != BattleInputState::SelectSkillTarget) continue; // no valid targets

        std::optional<GridPos> target = pickSkillTarget(controller, skillId);
        if (!target) {
            controller.cancelAttackSelection();
            continue;
        }
        controller.selectSkillTarget(*target);
        if (controller.inputState() == BattleInputState::ConfirmSkillAttack) controller.confirmSkillAttack();
        return true;
    }
    return false;
}

// Heal-family skills take the same priority as the innate heal ability
// (healIfUseful()) - a support unit's job is to heal before it fights.
bool healSkillIfUseful(BattleController& controller, Policy policy) {
    if (policy != Policy::Tactical) return false;
    return trySkillsOfKind(controller, {"emergency_treatment", "cleanse"});
}

// Attack-shape skills (suppressing_shot/halting_thrust/ambush) deal at
// least as much damage as a plain attack plus a bonus effect, so they're
// tried before the plain attack, not after.
bool attackShapeSkillIfPossible(BattleController& controller, Policy policy) {
    if (policy != Policy::Tactical) return false;
    return trySkillsOfKind(controller, {"suppressing_shot", "halting_thrust", "ambush"});
}

// Everything else (hold_formation/advance_order/provoke/extended_lockdown/
// overwatch) is a situational buff/utility action, not a damage source -
// only worth it as a fallback once nothing could be attacked this turn
// (otherwise a VeteranGuard would spend every other turn on provoke/
// extended_lockdown instead of ever actually attacking).
bool utilitySkillIfSensible(BattleController& controller, Policy policy) {
    if (policy != Policy::Tactical) return false;
    return trySkillsOfKind(controller, {"hold_formation", "advance_order", "provoke", "extended_lockdown", "overwatch"});
}

bool attackIfPossible(BattleController& controller) {
    controller.chooseAttack();
    if (controller.inputState() != BattleInputState::SelectTarget) return false;
    Unit* best = nullptr;
    int bestScore = std::numeric_limits<int>::min();
    Unit* attacker = controller.selectedUnit();
    for (GridPos tile : controller.targetableTiles()) {
        Unit* target = controller.battle().unitAt(tile);
        if (!target) continue;
        const int damage = computeDamage(*attacker, *target,
            controller.battle().combatDefenseBonus(*target, *attacker));
        int score = damage * 10 - target->currentHp;
        if (damage >= target->currentHp) score += 500;
        if (target->isBoss) score += 20;
        if (score > bestScore) {
            best = target;
            bestScore = score;
        }
    }
    if (!best) return false;
    controller.selectTargetTile(best->position);
    controller.confirmAttack();
    return true;
}

BattleResult runBattle(BattleState state, Policy policy, int roundLimit = 40) {
    // Idempotent if already equipped (continuation stages carry survivors'
    // Unit copies forward) - re-assigning the same skillId is a no-op.
    // Charges are reset fresh every battle regardless, by BattleController's
    // constructor below (initializeSkillCharges()), matching how the real
    // game starts each battle.
    for (Unit& unit : state.units()) {
        if (unit.team == Team::Player) equipStandardLoadout(unit);
    }
    BattleController controller(std::move(state));
    int guard = 0;
    while (controller.inputState() != BattleInputState::Victory &&
           controller.inputState() != BattleInputState::Defeat && guard++ < 10000) {
        if (controller.battle().round() > roundLimit) break;
        if (controller.inputState() == BattleInputState::EnemyTurn) {
            controller.update(10.0f);
            continue;
        }
        if (controller.inputState() != BattleInputState::SelectUnit) break;

        Unit* unit = nullptr;
        for (Unit& candidate : controller.battle().units()) {
            if (candidate.team == Team::Player && candidate.isAlive() && !candidate.hasActed) {
                unit = &candidate;
                break;
            }
        }
        if (!unit) {
            controller.endPlayerTurn();
            continue;
        }
        controller.selectUnit(*unit);
        const MoveChoice move = chooseMove(controller.battle(), *unit, controller.reachableTiles(), policy);
        controller.selectMoveTile(move.tile);
        if (healSkillIfUseful(controller, policy)) continue;
        if (healIfUseful(controller, policy)) continue;
        if (attackShapeSkillIfPossible(controller, policy)) continue;
        if (attackIfPossible(controller)) continue;
        if (utilitySkillIfSensible(controller, policy)) continue;
        controller.chooseWait();
    }

    BattleResult result;
    result.victory = controller.inputState() == BattleInputState::Victory;
    result.timeout = !result.victory && controller.inputState() != BattleInputState::Defeat;
    result.rounds = controller.battle().round();
    for (const Unit& unit : controller.battle().units()) {
        if (unit.team != Team::Player) continue;
        result.players.push_back(unit);
        result.maxHp += unit.stats.maxHp;
        result.hp += unit.currentHp;
        result.incapacitated += !unit.isAlive();
    }
    if (result.timeout && getenv("JF_DEBUG_TIMEOUT")) {
        int livingPlayers = 0, livingEnemies = 0;
        for (const Unit& unit : controller.battle().units()) {
            if (unit.team == Team::Player) livingPlayers += unit.isAlive();
            if (unit.team == Team::Enemy) livingEnemies += unit.isAlive();
            if (unit.isBoss || unit.unitClass == UnitClass::AshenhornBoar) {
                std::cerr << "[timeout] boar HP " << unit.currentHp << "/" << unit.stats.maxHp
                          << " (" << (100 * unit.currentHp / unit.stats.maxHp) << "%)"
                          << " livingPlayers=" << livingPlayers << " livingEnemies=" << livingEnemies
                          << " round=" << result.rounds << "\n";
            }
        }
    }
    return result;
}

void printAggregate(const std::string& label, const Aggregate& aggregate) {
    auto pct = [](double value) { return value * 100.0; };
    std::cout << std::left << std::setw(26) << label
              << " win " << std::setw(6) << std::fixed << std::setprecision(1)
              << pct(static_cast<double>(aggregate.victories) / aggregate.attempts) << "%"
              << "  any KO " << std::setw(6)
              << pct(static_cast<double>(aggregate.anyIncapacitation) / aggregate.attempts) << "%"
              << "  avg KO " << std::setw(5) << std::setprecision(2)
              << static_cast<double>(aggregate.totalIncapacitated) / aggregate.attempts
              << "  HP " << std::setw(6) << std::setprecision(1)
              << pct(static_cast<double>(aggregate.totalHp) / aggregate.totalMaxHp) << "%"
              << "  rounds " << std::setprecision(2)
              << static_cast<double>(aggregate.totalRounds) / aggregate.attempts;
    if (aggregate.timeouts) std::cout << "  timeouts " << aggregate.timeouts;
    std::cout << '\n';
}

} // namespace

// One-off party-composition experiments (e.g. "how much does bringing/not
// bringing a specific class swing difficulty"): argv[2], a UnitClass's bare
// enum name (e.g. "VeteranGuard"), drops that class from the simulated
// party before any battle is built. Left as a permanent, opt-in CLI arg
// rather than a throwaway edit, since this kind of question recurs.
UnitClass classFromArg(const std::string& name) {
    if (name == "MarchCaptain") return UnitClass::MarchCaptain;
    if (name == "VeteranGuard") return UnitClass::VeteranGuard;
    if (name == "WatchArcher") return UnitClass::WatchArcher;
    if (name == "FrontierScout") return UnitClass::FrontierScout;
    if (name == "Spearman") return UnitClass::Spearman;
    if (name == "DawnChirurgeon") return UnitClass::DawnChirurgeon;
    std::cerr << "Unknown class '" << name << "', ignoring --exclude.\n";
    return UnitClass::MarchCaptain;
}

int main(int argc, char** argv) {
    int runs = 500;
    if (argc > 1) runs = std::max(std::stoi(argv[1]), 1);
    auto data = loadGameData("data");
    if (!data) data = loadGameData("../data");
    if (!data) {
        std::cerr << "Could not load game data.\n";
        return 1;
    }

    if (argc > 2) {
        const UnitClass excluded = classFromArg(argv[2]);
        auto& party = data->playerParty;
        party.erase(std::remove_if(party.begin(), party.end(),
                                   [&](const UnitTemplate& u) { return u.classId == excluded; }),
                   party.end());
        std::cout << "Excluding " << argv[2] << " from the party (" << party.size() << " remain).\n";
    }

    const RegionDescriptor forest = regionDescriptor(RegionId::AshboughForest, *data);
    std::cout << "Ashbough Forest headless balance simulation (" << runs << " seeds)\n"
              << "No consumables, no manual deployment, no log-lure planning.\n\n";

    for (Policy policy : {Policy::Direct, Policy::Tactical}) {
        const std::string policyName = policy == Policy::Direct ? "Direct" : "Tactical";
        std::cout << '[' << policyName << "] fresh party per site\n";
        for (std::size_t stageIndex = 0; stageIndex < forest.stages.size(); ++stageIndex) {
            Aggregate aggregate;
            for (int seed = 1; seed <= runs; ++seed) {
                BattleState battle = createScenarioBattle(*data, forest.stages[stageIndex],
                    static_cast<std::uint32_t>(seed), stageRouteOutcome(forest.stages[stageIndex],
                    ExplorationChoice::FrontalAdvance));
                aggregate.add(runBattle(std::move(battle), policy));
            }
            printAggregate(forest.stages[stageIndex].missionNameEn, aggregate);
        }

        Aggregate expedition;
        std::array<int, 3> reached{};
        // Snapshot of the party's condition the instant before Territory
        // (Brokenwood) starts - the survivors/HP carried over from Hollow,
        // to check whether "fewer allies/lower HP arriving" is actually as
        // severe as the Territory-alone win-rate drop (96.8% fresh vs the
        // much lower chained win rate) would suggest.
        long long territoryEntrySurvivors = 0;
        long long territoryEntryHpSum = 0;
        long long territoryEntryMaxHpSum = 0;
        int territoryEntrySamples = 0;
        for (int seed = 1; seed <= runs; ++seed) {
            std::vector<Unit> survivors;
            BattleResult finalResult;
            bool cleared = true;
            int expeditionRounds = 0;
            int expeditionMaxHp = 0;
            for (std::size_t stageIndex = 0; stageIndex < forest.stages.size(); ++stageIndex) {
                ++reached[stageIndex];
                if (stageIndex == 2) {
                    ++territoryEntrySamples;
                    for (const Unit& player : survivors) {
                        if (!player.isAlive()) continue;
                        ++territoryEntrySurvivors;
                        territoryEntryHpSum += player.currentHp;
                        territoryEntryMaxHpSum += player.stats.maxHp;
                    }
                }
                BattleState battle = stageIndex == 0
                    ? createScenarioBattle(*data, forest.stages[stageIndex], static_cast<std::uint32_t>(seed),
                        stageRouteOutcome(forest.stages[stageIndex], ExplorationChoice::FrontalAdvance))
                    : createScenarioContinuationBattle(*data, survivors, forest.stages[stageIndex],
                        static_cast<std::uint32_t>(seed * 17 + static_cast<int>(stageIndex)));
                finalResult = runBattle(std::move(battle), policy);
                expeditionRounds += finalResult.rounds;
                if (stageIndex == 0) expeditionMaxHp = finalResult.maxHp;
                survivors = finalResult.players;
                if (!finalResult.victory) {
                    cleared = false;
                    break;
                }
            }
            finalResult.victory = cleared;
            finalResult.rounds = expeditionRounds;
            finalResult.maxHp = expeditionMaxHp;
            finalResult.hp = 0;
            int livingPlayers = 0;
            for (const Unit& player : survivors) {
                finalResult.hp += player.currentHp;
                livingPlayers += player.isAlive();
            }
            finalResult.incapacitated = static_cast<int>(data->playerParty.size()) - livingPlayers;
            expedition.add(finalResult);
        }
        std::cout << '[' << policyName << "] three-site expedition\n";
        printAggregate("Forest clear", expedition);
        std::cout << "Reach: verge " << reached[0] << "/" << runs
                  << ", hollow " << reached[1] << "/" << runs
                  << ", territory " << reached[2] << "/" << runs << "\n";
        std::cout << std::fixed << std::setprecision(2)
                  << "Territory entry: avg survivors "
                  << static_cast<double>(territoryEntrySurvivors) / territoryEntrySamples << "/4"
                  << "  avg HP% "
                  << (territoryEntryMaxHpSum > 0
                          ? 100.0 * static_cast<double>(territoryEntryHpSum) / territoryEntryMaxHpSum
                          : 0.0)
                  << "\n\n";
    }
}
