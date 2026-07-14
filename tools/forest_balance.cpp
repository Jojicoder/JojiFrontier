#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "jf/battle/BattleController.hpp"
#include "jf/battle/BattleFactory.hpp"
#include "jf/battle/CombatResolver.hpp"
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
        for (const Unit& enemy : battle.units()) {
            if (enemy.team != Team::Enemy || !enemy.isAlive() || !inAttackRange(unit, tile, enemy.position))
                continue;
            const int damage = computeDamage(unit, enemy, battle.combatDefenseBonus(enemy, unit));
            score += damage * 12;
            if (damage >= enemy.currentHp) score += 500;
            if (enemy.isBoss) score += 25;
        }
        if (policy == Policy::Tactical) {
            score -= threatenedDamage(battle, unit, tile) * 5;
            score += allySupport(battle, unit, tile) * 12;
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
        if (healIfUseful(controller, policy)) continue;
        if (attackIfPossible(controller)) continue;
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

int main(int argc, char** argv) {
    int runs = 500;
    if (argc > 1) runs = std::max(std::stoi(argv[1]), 1);
    auto data = loadGameData("data");
    if (!data) data = loadGameData("../data");
    if (!data) {
        std::cerr << "Could not load game data.\n";
        return 1;
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
        for (int seed = 1; seed <= runs; ++seed) {
            std::vector<Unit> survivors;
            BattleResult finalResult;
            bool cleared = true;
            int expeditionRounds = 0;
            int expeditionMaxHp = 0;
            for (std::size_t stageIndex = 0; stageIndex < forest.stages.size(); ++stageIndex) {
                ++reached[stageIndex];
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
                  << ", territory " << reached[2] << "/" << runs << "\n\n";
    }
}
