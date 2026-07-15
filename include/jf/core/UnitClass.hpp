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
    // docs/regions/ashbough_forest.md 灰枝の林縁: an ordinary large predator,
    // not a monster (per World Bible flora_fauna.md framing) - no special
    // combat rule of its own, just a distinct enemy roster entry.
    Wolf,
    // docs/regions/ashbough_forest.md "灰角大猪" (折れ木の縄張り's boss,
    // individual name "折れ木の主"). Own AI (jf/battle/EnemyAI.hpp's
    // takeBoarBossTurn()) and boss-only transient Unit fields.
    AshenhornBoar,
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
int passiveEvasionBonus(UnitClass unitClass);

} // namespace jf
