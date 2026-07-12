#pragma once

#include <string>

#include "jf/core/Grid.hpp"
#include "jf/core/Stats.hpp"
#include "jf/core/UnitClass.hpp"
#include "jf/core/Weapon.hpp"

namespace jf {

struct Unit {
    std::string id;
    std::string name;
    UnitClass unitClass = UnitClass::Lord;
    Team team = Team::Player;

    Stats stats;
    int currentHp = 1;
    Weapon weapon;

    GridPos position{};
    bool hasActed = false;

    bool isAlive() const { return currentHp > 0; }

    int attackPower() const {
        return weapon.damageType == DamageType::Physical ? stats.strength : stats.magic;
    }
};

} // namespace jf
