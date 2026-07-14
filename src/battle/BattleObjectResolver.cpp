#include "jf/battle/BattleObjectResolver.hpp"

#include <algorithm>

namespace jf {

int computeObjectDamage(const Unit& attacker, const BattleObjectDefinition& def) {
    const int power = attacker.attackPower() + attacker.weapon.might;
    const int mitigation = attacker.weapon.damageType == DamageType::Physical ? def.defense : def.resistance;
    return std::max(power - mitigation, 1);
}

bool resolveObjectAttack(BattleState& battle, const Unit& attacker, BattleObjectState& target) {
    if (target.state == BattleObjectStateKind::Destroyed) return false;
    const BattleObjectDefinition* def = battle.objectDefinition(target.definitionId);
    if (!def || !def->canBeAttacked || def->maxDurability <= 0) return false;

    const int damage = computeObjectDamage(attacker, *def);
    target.durability = std::max(target.durability - damage, 0);
    if (target.durability > 0) return false;
    target.state = BattleObjectStateKind::Destroyed; // fires exactly once: durability only reaches 0 once
    return true;
}

bool resolveObjectInteraction(const Unit& actor, BattleObjectState& target,
                              const ObjectInteractionDefinition& interaction, BattleObjectStateKind newState) {
    if (target.state != interaction.requiredState) return false;
    if (target.state == newState) return false;
    if (target.interactionCount >= interaction.maxUses) return false;
    if (!interaction.allowedClasses.empty() && !interaction.allowedClasses.count(actor.unitClass)) return false;
    const int distance = manhattanDistance(actor.position, target.position);
    if (distance > interaction.range) return false;
    target.state = newState;
    ++target.interactionCount;
    return true;
}

} // namespace jf
