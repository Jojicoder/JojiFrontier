#pragma once

#include <string>

namespace jf {

enum class UnitClass {
    MarchCaptain,
    VeteranGuard,
    WatchArcher,
    FrontierScout,
    Spearman,
    DawnChirurgeon,
    Bandit,
};

enum class Team {
    Player,
    Enemy
};

std::string toString(UnitClass unitClass);
bool providesFormationBonus(UnitClass unitClass);
bool hasZoneOfControl(UnitClass unitClass);
bool ignoresAshPenalty(UnitClass unitClass);
bool hasBrace(UnitClass unitClass);
bool canHeal(UnitClass unitClass);

} // namespace jf
