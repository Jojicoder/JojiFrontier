// A lightweight, deterministic 1v1 matchup table across the roster's base
// classes (docs/class_reference.md). Combat in this game is fully
// deterministic (no crit, no counterattack except Spearman's Tier-1 skill,
// which this tool intentionally ignores to isolate raw base-class numbers),
// so "how many hits does A need to kill B" fully characterizes a matchup
// without needing repeated random trials - unlike jf_forest_balance, which
// simulates full multi-unit battles with movement/positioning/terrain and
// therefore does need many seeds.
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "jf/battle/BattleFactory.hpp"
#include "jf/battle/CombatResolver.hpp"
#include "jf/data/GameData.hpp"

using namespace jf;

namespace {

struct RosterEntry {
    std::string label;
    UnitClass classId;
    Team team;
};

int hitsToKill(const Unit& attacker, const Unit& defender) {
    const int damage = computeDamage(attacker, defender, 0);
    return (defender.stats.maxHp + damage - 1) / damage; // ceil
}

struct TradeResult {
    bool firstWins = false;
    int rounds = 0;
};

// Alternating trade with no positioning/range restrictions: `first` attacks,
// then `second` attacks if still alive, repeating until someone's HP reaches
// 0. Deterministic damage means this only needs to run once, not averaged
// over trials.
TradeResult simulateTrade(const Unit& first, const Unit& second) {
    int firstHp = first.stats.maxHp;
    int secondHp = second.stats.maxHp;
    const int firstDamage = computeDamage(first, second, 0);
    const int secondDamage = computeDamage(second, first, 0);
    int rounds = 0;
    while (true) {
        ++rounds;
        secondHp = std::max(0, secondHp - firstDamage);
        if (secondHp == 0) return {true, rounds};
        firstHp = std::max(0, firstHp - secondDamage);
        if (firstHp == 0) return {false, rounds};
    }
}

} // namespace

int main() {
    auto data = loadGameData("data");
    if (!data) data = loadGameData("../data");
    if (!data) {
        std::cerr << "Could not load game data.\n";
        return 1;
    }

    const std::vector<RosterEntry> roster = {
        {"MarchCaptain", UnitClass::MarchCaptain, Team::Player},
        {"VeteranGuard", UnitClass::VeteranGuard, Team::Player},
        {"WatchArcher", UnitClass::WatchArcher, Team::Player},
        {"FrontierScout", UnitClass::FrontierScout, Team::Player},
        {"Spearman", UnitClass::Spearman, Team::Player},
        {"DawnChirurgeon", UnitClass::DawnChirurgeon, Team::Player},
        {"Bandit", UnitClass::Bandit, Team::Enemy},
        {"Wolf", UnitClass::Wolf, Team::Enemy},
        {"AshenhornBoar", UnitClass::AshenhornBoar, Team::Enemy},
    };

    std::vector<Unit> units;
    for (const RosterEntry& entry : roster) {
        UnitTemplate tmpl{entry.label, entry.label, entry.classId};
        units.push_back(instantiateUnit(*data, tmpl, entry.team, GridPos{0, 0}));
    }

    std::cout << "Base stats (no equipment, no terrain, no traits)\n";
    std::cout << std::left << std::setw(16) << "Class" << std::right << std::setw(6) << "HP" << std::setw(6)
              << "STR" << std::setw(6) << "MAG" << std::setw(6) << "DEF" << std::setw(6) << "RES" << std::setw(6)
              << "SPD" << std::setw(6) << "MOV" << "   Weapon(Might/Range)\n";
    for (const Unit& unit : units) {
        std::cout << std::left << std::setw(16) << unit.name << std::right << std::setw(6) << unit.stats.maxHp
                  << std::setw(6) << unit.stats.strength << std::setw(6) << unit.stats.magic << std::setw(6)
                  << unit.stats.defense << std::setw(6) << unit.stats.resistance << std::setw(6) << unit.stats.speed
                  << std::setw(6) << unit.stats.move << "   " << unit.weapon.name << " (" << unit.weapon.might
                  << "/" << unit.weapon.minRange << "-" << unit.weapon.maxRange << ")\n";
    }

    std::cout << "\nHits-to-kill matrix (row attacks column; range/positioning ignored)\n";
    std::cout << std::left << std::setw(16) << "";
    for (const Unit& col : units) std::cout << std::right << std::setw(14) << col.name;
    std::cout << "\n";
    for (const Unit& row : units) {
        std::cout << std::left << std::setw(16) << row.name;
        for (const Unit& col : units) {
            if (&row == &col) {
                std::cout << std::right << std::setw(14) << "-";
                continue;
            }
            std::cout << std::right << std::setw(14) << hitsToKill(row, col);
        }
        std::cout << "\n";
    }

    std::cout << "\nStraight trade, adjacent from round 1, first-named side attacks first\n";
    for (std::size_t i = 0; i < units.size(); ++i) {
        for (std::size_t j = i + 1; j < units.size(); ++j) {
            const TradeResult result = simulateTrade(units[i], units[j]);
            const Unit& winner = result.firstWins ? units[i] : units[j];
            std::cout << std::left << std::setw(16) << units[i].name << " vs " << std::setw(16) << units[j].name
                      << "-> " << std::setw(16) << winner.name << " wins in " << result.rounds << " round(s)\n";
        }
    }
    return 0;
}
