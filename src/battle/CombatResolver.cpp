#include "jf/battle/CombatResolver.hpp"

#include <algorithm>

namespace jf {

int computeDamage(const Unit& attacker, const Unit& target, int terrainDefense) {
    int defenseStat = attacker.weapon.damageType == DamageType::Physical
                           ? target.stats.defense
                           : target.stats.resistance;
    int raw = attacker.attackPower() + attacker.weapon.might - defenseStat - terrainDefense;
    return std::max(raw, 1);
}

CombatPreview previewAttack(const Unit& attacker, const Unit& target, int terrainDefense) {
    CombatPreview preview;
    preview.attackerName = attacker.name;
    preview.weaponName = attacker.weapon.name;
    preview.damage = computeDamage(attacker, target, terrainDefense);
    preview.targetName = target.name;
    preview.targetHpBefore = target.currentHp;
    preview.targetHpAfter = std::max(target.currentHp - preview.damage, 0);
    return preview;
}

void resolveAttack(const Unit& attacker, Unit& target, int terrainDefense) {
    int damage = computeDamage(attacker, target, terrainDefense);
    target.currentHp = std::max(target.currentHp - damage, 0);
}

} // namespace jf
