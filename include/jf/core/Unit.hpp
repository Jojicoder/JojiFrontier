#pragma once

#include <algorithm>
#include <array>
#include <string>

#include "jf/core/Grid.hpp"
#include "jf/core/Stats.hpp"
#include "jf/core/StatusEffect.hpp"
#include "jf/core/UnitClass.hpp"
#include "jf/core/Weapon.hpp"

namespace jf {

// Battle-scoped runtime state for one of a unit's 2 equipped-skill slots
// (docs/skill_system.md). `skillId` is set from the unit's persistent
// UnitSkillLoadout when a battle is created; empty means nothing equipped
// there. See jf/battle/SkillCharges.hpp for how these are initialized,
// refreshed, and consumed.
struct SkillSlotState {
    std::string skillId;
    int usesRemaining = 0;
    int cooldownPhasesRemaining = 0;
};

struct Unit {
    std::string id;
    std::string name;
    UnitClass unitClass = UnitClass::MarchCaptain;
    Team team = Team::Player;

    Stats stats;
    int currentHp = 1;
    Weapon weapon;

    GridPos position{};
    bool hasActed = false;
    int tilesMovedThisAction = 0;

    // Hide-Wrapped Grip tuning trait: negates this many knockbacks for the
    // rest of the battle (set once at battle start, decremented on use).
    int knockbackNegatesRemaining = 0;

    // Scales down status-effect magnitude/duration instead of granting full
    // immunity (docs/status_effects.md "ボス補正"). Set once when a boss
    // encounter is instantiated; no shipped content sets this yet.
    bool isBoss = false;

    // 状態異常 (docs/status_effects.md). Applied/ticked/cleared through
    // jf::applyPoison()/applyBurn()/.../processActionEndStatusEffects()/
    // processPhaseEndStatusEffects() (jf/battle/StatusEffects.hpp) rather
    // than mutated directly, so the reapplication/immunity rules stay in
    // one place. All of it is battle-scoped: never saved, always cleared at
    // battle end.
    int poisonRemainingProcs = 0;   // 0 = not poisoned
    int burnRemainingProcs = 0;     // 0 = not burning
    bool moveDownActive = false;    // MOV penalty until this unit's side's next phase ends
    bool defenseDownActive = false; // DEF penalty until this unit's side's next phase ends
    bool staggerActive = false;     // no movement on this unit's next action
    bool staggerImmune = false;     // cannot be re-staggered until this unit's side's next phase ends

    // The 2 equipped-skill slots (docs/skill_system.md). See
    // jf/battle/SkillCharges.hpp for lifecycle management.
    std::array<SkillSlotState, 2> skillSlots{};

    // 灰角大猪 (docs/regions/ashbough_forest.md "灰角大猪") boss-only
    // transient state - meaningless for any other unit. Kept directly on
    // Unit rather than a parallel structure since mid-battle state is never
    // saved anyway (matches how the generic status-effect fields above are
    // already modeled). Charge always travels along the boar's own current
    // row (docs: "同じ行を...進む") and it can't move between telegraphing
    // and executing (execution is checked before any repositioning), so the
    // telegraph only needs to be a flag, not a stored row/target.
    bool bossEnraged = false;               // HP<=50%, triggers once
    bool chargeTelegraphed = false;         // executes next turn, then clears
    bool bossStunnedNextEnemyPhase = false; // set on log collision; skips one turn
    bool bossWeakenedFromStun = false;      // DEF/RES overridden low while true

    bool isAlive() const { return currentHp > 0; }

    int attackPower() const {
        return weapon.damageType == DamageType::Physical ? stats.strength : stats.magic;
    }

    int minimumAttackRange() const {
        return unitClass == UnitClass::WatchArcher && weapon.minRange < 2 ? 2 : weapon.minRange;
    }

    // Move budget after よろめき/移動低下 (docs/status_effects.md). Stagger on
    // a normal unit means "no movement" outright (0, not merely low); on a
    // boss it substitutes a MOV-1 penalty instead of a full lock.
    int effectiveMove() const {
        if (staggerActive && !isBoss) return 0;
        int mov = stats.move;
        if (staggerActive) mov = std::max(mov - 1, 0);
        if (moveDownActive) mov = std::max(mov - statusMoveDownAmount(isBoss), 1);
        return mov;
    }

    // Defense after 防御低下 (docs/status_effects.md - RES is never lowered
    // by it).
    int effectiveDefense() const {
        if (!defenseDownActive) return stats.defense;
        return std::max(stats.defense - statusDefenseDownAmount(isBoss), 0);
    }
};

} // namespace jf
