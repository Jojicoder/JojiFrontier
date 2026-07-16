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
};

} // namespace jf
