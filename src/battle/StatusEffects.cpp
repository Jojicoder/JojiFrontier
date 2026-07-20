#include "jf/battle/StatusEffects.hpp"

#include <algorithm>

#include "jf/core/StatusEffect.hpp"

namespace jf {

namespace {
// docs/status_effects.md "地形・マス効果との関係": ending an action on a
// status-clearing tile (e.g. Ashbough Forest's Shallows) clears burn before
// its action-end damage is applied. No shipped TerrainType does this yet
// (Terrain.hpp has no Shallows-equivalent), so this always returns false
// today - it is the hook point for when that terrain ships.
bool terrainClearsBurn(TerrainType terrain) { return terrain == TerrainType::Shallows; }
} // namespace

void applyPoison(Unit& target) {
    target.poisonRemainingProcs = statusPoisonMaxProcs(target.isBoss);
}

void applyBurn(Unit& target) {
    target.burnRemainingProcs = statusBurnMaxProcs(target.isBoss);
}

void applyMoveDown(Unit& target) {
    target.moveDownActive = true;
}

void applyDefenseDown(Unit& target) {
    target.defenseDownActive = true;
}

void applyResistanceUp(Unit& target) {
    target.resistanceUpActive = true;
}

void applyDefenseUp(Unit& target) {
    target.defenseUpActive = true;
}

void applyZocRangeExtension(Unit& target) {
    target.zocRangeExtended = true;
}

void applyBraceBonus(Unit& target) {
    target.braceSkillActive = true;
}

void applyProvoke(Unit& target, const std::string& casterId) {
    target.provokedByUnitId = casterId;
}

void applyMoveUp(Unit& target) {
    target.moveUpActive = true;
}

void applyStagger(Unit& target) {
    if (target.staggerImmune) return;
    target.staggerActive = true;
}

void applyStatusEffect(Unit& target, StatusEffectType effect) {
    switch (effect) {
    case StatusEffectType::Poison: applyPoison(target); break;
    case StatusEffectType::Burn: applyBurn(target); break;
    case StatusEffectType::MoveDown: applyMoveDown(target); break;
    case StatusEffectType::DefenseDown: applyDefenseDown(target); break;
    case StatusEffectType::Stagger: applyStagger(target); break;
    }
}

void applyWeaponOnHitStatuses(const Unit& attacker, Unit& target) {
    if (!target.isAlive()) return;
    for (StatusEffectType effect : attacker.weapon.onHitStatuses) applyStatusEffect(target, effect);
}

void clearAllStatusEffects(Unit& target) {
    target.poisonRemainingProcs = 0;
    target.burnRemainingProcs = 0;
    target.moveDownActive = false;
    target.defenseDownActive = false;
    target.staggerActive = false;
}

void clearAllStatusEffects(BattleState& battle) {
    for (Unit& unit : battle.units()) {
        clearAllStatusEffects(unit);
        unit.staggerImmune = false;
        unit.resistanceUpActive = false;
        unit.defenseUpActive = false;
        unit.zocRangeExtended = false;
        unit.moveUpActive = false;
        unit.immovableStanceActive = false;
        unit.immovableStanceJustGranted = false;
        unit.braceForImpactActive = false;
        unit.braceForImpactJustGranted = false;
        unit.braceSkillActive = false;
        unit.provokedByUnitId.clear();
        unit.overwatchActive = false;
    }
    battle.clearTrailblazedTiles(); // 辺境斥候`trailblaze`: battle-scoped, never saved
}

void processActionEndStatusEffects(BattleState& battle, Unit& unit) {
    if (!unit.isAlive()) {
        if (unit.staggerActive) unit.staggerImmune = true;
        unit.staggerActive = false;
        return;
    }
    if (unit.burnRemainingProcs > 0 && terrainClearsBurn(battle.terrainAt(unit.position))) {
        unit.burnRemainingProcs = 0;
    }
    if (unit.burnRemainingProcs > 0) {
        unit.currentHp = std::max(unit.currentHp - statusBurnDamage(unit.isBoss), 0);
        --unit.burnRemainingProcs;
    }
    // 解除後、対象側の次Phase終了までよろめき無効 (docs/status_effects.md).
    if (unit.staggerActive) unit.staggerImmune = true;
    unit.staggerActive = false;
}

void processPhaseEndStatusEffects(BattleState& battle, Team team) {
    for (Unit& unit : battle.units()) {
        if (unit.team != team || !unit.isAlive()) continue;
        if (unit.poisonRemainingProcs > 0) {
            // 毒だけはHPを1未満にしない (docs/status_effects.md).
            unit.currentHp = std::max(unit.currentHp - statusPoisonDamage(unit.isBoss), 1);
            --unit.poisonRemainingProcs;
        }
        unit.moveDownActive = false;
        unit.defenseDownActive = false;
        unit.staggerImmune = false;
    }
}

void clearSkillBuffsAtEnemyPhaseEnd(BattleState& battle) {
    for (Unit& unit : battle.units()) {
        unit.resistanceUpActive = false;
        unit.defenseUpActive = false;
        unit.zocRangeExtended = false;
        unit.braceSkillActive = false;
        unit.provokedByUnitId.clear();
    }
}

void clearMoveUpAtPlayerPhaseEnd(BattleState& battle) {
    for (Unit& unit : battle.units()) unit.moveUpActive = false;
}

} // namespace jf
