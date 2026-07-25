#pragma once

#include <string>
#include <vector>

#include "jf/core/StatusEffect.hpp"

namespace jf {

enum class DamageType {
    Physical,
    Magical
};

struct Weapon {
    std::string id;
    std::string name;
    int might = 0;
    int minRange = 1;
    int maxRange = 1;
    DamageType damageType = DamageType::Physical;

    // Forge weapon-branch modifiers (docs/base_development.md "鍛冶場").
    int moveModifier = 0;      // Heavy Spear: -1
    bool braceBoost = false;   // Guard Spear: strengthens the Brace bonus
    bool causesKnockback = false; // Heavy Spear: pushes the defender back on hit
    std::vector<StatusEffectType> onHitStatuses;

    // M7項目3続き(基礎〜中程度Tier) weapon-branch unique effects
    // (docs/base_development.md). Kept as plain data on the weapon itself,
    // consulted by CombatResolver.cpp/BattleController.cpp, rather than
    // switching on `id` string at every call site.

    // Mercy/Ward/March Staff: overrides the Dawn Chirurgeon's default Heal
    // amount (8) when > 0 - Mercy Staff uses 12, Ward/March Staff use 6
    // (both trade might for a side effect below, hence the lower heal too).
    int healAmountOverride = 0;
    // Ward Staff: Heal target also gets RES+3 until the next Enemy Phase
    // ends - same shape/lifecycle as 暁の衛生兵`protective_treatment`'s own
    // resistanceUpActive buff (Unit.hpp), just granted by a weapon instead.
    bool healGrantsResistanceUp = false;
    // March Staff: if the Heal target hasn't acted yet, it also gets MOV+1
    // until THIS Player Phase ends - same shape/lifecycle as 行軍隊長
    // `advance_order`'s moveUpActive (Unit.hpp).
    bool healGrantsMoveUp = false;

    // Repair Hammer: overrides 辺境工兵`field_repair`'s default durability
    // restore amount (6) when > 0 (9 for Repair Hammer).
    int fieldRepairAmountOverride = 0;

    // Snare Bow/Driving Bow/Ember Focus/Resonant Focus: gates whichever of
    // onHitStatuses/causesKnockback/splashDamage this weapon sets to only
    // the first successful hit each battle (Unit::weaponFirstHitUsed tracks
    // the "already procced" state, mirroring arcaneOverflowUsed's exact
    // lifecycle - never reset mid-battle). A weapon only ever uses ONE of
    // those three effects at a time, so a single per-unit flag is enough.
    bool firstHitOnly = false;
    // Resonant Focus: fixed damage (ignores DEF/RES, no reaction attack -
    // same jf::applyAdjacentSplashDamage() helper as 戦闘魔導士「魔力波及」)
    // applied to enemies directly above/below the primary target on a hit,
    // gated by firstHitOnly above like everything else on this weapon.
    int splashDamage = 0;
    // Hook Lance: on a hit made from exactly range 2, pulls the target 1
    // tile toward the attacker instead of Heavy Spear's push-away
    // (BattleState::applyPull(), the inverse of applyKnockback()).
    bool pullsAtRangeTwo = false;
    // Trail Blade: moving with this weapon equipped marks the Ash/Shallows
    // tiles passed through as trailblazed (BattleState::markTrailblazed()) -
    // same underlying mechanic/tile set as 辺境斥候`trailblaze`'s own skill,
    // just triggered by any move action instead of spending a skill charge.
    bool trailblazeOnMove = false;
};

} // namespace jf
