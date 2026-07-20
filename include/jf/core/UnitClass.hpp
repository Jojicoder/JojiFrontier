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
    // docs/class_reference.md「後半6兵種」/M7項目1: 固有能力「重量装甲」
    // (hasHeavyArmor())。加入経路(M7項目2)は未実装のため、まだ
    // playerParty/reserveRosterには登場しない - Classとしてのみ完全に有効。
    HeavyInfantry,
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
// docs/class_reference.md「重量装甲」: this engine's knockback is always
// exactly 1 tile (BattleState::applyKnockback), so "reduce distance by 1,
// floor 0" collapses to "never gets knocked back" - consulted directly
// there, unconditionally (unlike the consumable knockbackNegatesRemaining).
bool hasHeavyArmor(UnitClass unitClass);

} // namespace jf
