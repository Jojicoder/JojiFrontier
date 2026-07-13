#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "jf/core/Stats.hpp"
#include "jf/core/UnitClass.hpp"
#include "jf/core/Weapon.hpp"

namespace jf {

struct ClassDefinition {
    UnitClass id{};
    Stats baseStats;
    std::string weaponId;
};

struct UnitTemplate {
    std::string id;
    std::string name;
    UnitClass classId{};
};

struct GameData {
    std::unordered_map<std::string, Weapon> weaponsById;
    std::unordered_map<UnitClass, ClassDefinition> classesById;
    std::vector<UnitTemplate> playerParty;
    std::vector<UnitTemplate> reserveRoster;
    std::vector<UnitTemplate> enemyRoster;

    const Weapon& weaponFor(UnitClass unitClass) const;
    const ClassDefinition& classDefinition(UnitClass unitClass) const;
};

std::optional<UnitClass> unitClassFromString(const std::string& name);

// Loads classes.json, units.json and weapons.json from dataDir.
// Returns std::nullopt (and prints diagnostics) on failure.
std::optional<GameData> loadGameData(const std::string& dataDir);

} // namespace jf
