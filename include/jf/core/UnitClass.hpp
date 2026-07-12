#pragma once

#include <string>

namespace jf {

enum class UnitClass {
    Lord,
    ArmorKnight,
    Archer,
    Mage,
    Bandit,
    Soldier
};

enum class Team {
    Player,
    Enemy
};

std::string toString(UnitClass unitClass);

} // namespace jf
