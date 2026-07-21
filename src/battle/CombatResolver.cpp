#include "jf/battle/CombatResolver.hpp"
#include "jf/battle/StatusEffects.hpp"

#include <algorithm>

namespace jf {

int computeDamage(const Unit& attacker, const Unit& target, int terrainDefense, int attackerBonusPower) {
    int defenseStat = attacker.weapon.damageType == DamageType::Physical
                           ? target.effectiveDefense()
                           : target.effectiveResistance();
    // 監視弓兵`mark_target` (docs/initial_skill_effects.md): only reads the
    // mark here (stays pure for previewAttack()'s use) - resolveAttack()
    // clears it once a real hit actually consumes it.
    int raw = attacker.attackPower() + attackerBonusPower + attacker.weapon.might - defenseStat - terrainDefense +
              target.markedBonusDamage;
    return std::max(raw, 1);
}

CombatPreview previewAttack(const Unit& attacker, const Unit& target, int terrainDefense, int hitChance,
                            int attackerBonusPower) {
    CombatPreview preview;
    preview.attackerName = attacker.name;
    preview.weaponName = attacker.weapon.name;
    preview.weaponId = attacker.weapon.id;
    preview.damage = computeDamage(attacker, target, terrainDefense, attackerBonusPower);
    preview.hitChance = std::clamp(hitChance, 0, 100);
    preview.targetName = target.name;
    preview.targetHpBefore = target.currentHp;
    preview.targetHpAfter = std::max(target.currentHp - preview.damage, 0);
    return preview;
}

void resolveAttack(BattleState& battle, const Unit& attacker, Unit& target, int terrainDefense, bool hit,
                   int attackerBonusPower) {
    if (!hit) return;
    int damage = computeDamage(attacker, target, terrainDefense, attackerBonusPower);
    target.currentHp = std::max(target.currentHp - damage, 0);
    target.markedBonusDamage = 0; // consumed by this real hit, if it was set
    applyWeaponOnHitStatuses(battle, attacker, target);
}

int bannerAuraBonus(const std::vector<Unit>& units, const Unit& target) {
    for (const Unit& unit : units) {
        if (&unit == &target || unit.team != target.team || !unit.isAlive() || !hasBannerAura(unit.unitClass))
            continue;
        if (manhattanDistance(unit.position, target.position) <= 2) return 1;
    }
    return 0;
}

} // namespace jf
