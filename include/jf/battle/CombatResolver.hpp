#pragma once

#include <string>

#include "jf/core/Unit.hpp"

namespace jf {

struct CombatPreview {
    std::string attackerName;
    std::string weaponName;
    std::string weaponId;
    int damage = 0;
    int hitChance = 100;
    std::string targetName;
    int targetHpBefore = 0;
    int targetHpAfter = 0;
};

// Deterministic damage: STR/MAG + weapon Might - target DEF/RES, floor of 1.
int computeDamage(const Unit& attacker, const Unit& target, int terrainDefense = 0);

CombatPreview previewAttack(const Unit& attacker, const Unit& target, int terrainDefense = 0,
                            int hitChance = 100);

// Applies damage to `target.currentHp`, clamped at 0.
void resolveAttack(const Unit& attacker, Unit& target, int terrainDefense = 0, bool hit = true);

} // namespace jf
