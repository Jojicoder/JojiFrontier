#include "jf/battle/CombatResolver.hpp"
#include "jf/battle/StatusEffects.hpp"

#include <algorithm>

namespace jf {

int computeDamage(const Unit& attacker, const Unit& target, int terrainDefense) {
    int defenseStat = attacker.weapon.damageType == DamageType::Physical
                           ? target.effectiveDefense()
                           : target.effectiveResistance();
    // 監視弓兵`mark_target` (docs/initial_skill_effects.md): only reads the
    // mark here (stays pure for previewAttack()'s use) - resolveAttack()
    // clears it once a real hit actually consumes it.
    int raw = attacker.attackPower() + attacker.weapon.might - defenseStat - terrainDefense +
              target.markedBonusDamage;
    return std::max(raw, 1);
}

CombatPreview previewAttack(const Unit& attacker, const Unit& target, int terrainDefense, int hitChance) {
    CombatPreview preview;
    preview.attackerName = attacker.name;
    preview.weaponName = attacker.weapon.name;
    preview.weaponId = attacker.weapon.id;
    preview.damage = computeDamage(attacker, target, terrainDefense);
    preview.hitChance = std::clamp(hitChance, 0, 100);
    preview.targetName = target.name;
    preview.targetHpBefore = target.currentHp;
    preview.targetHpAfter = std::max(target.currentHp - preview.damage, 0);
    return preview;
}

void resolveAttack(const Unit& attacker, Unit& target, int terrainDefense, bool hit) {
    if (!hit) return;
    int damage = computeDamage(attacker, target, terrainDefense);
    target.currentHp = std::max(target.currentHp - damage, 0);
    target.markedBonusDamage = 0; // consumed by this real hit, if it was set
    applyWeaponOnHitStatuses(attacker, target);
}

} // namespace jf
