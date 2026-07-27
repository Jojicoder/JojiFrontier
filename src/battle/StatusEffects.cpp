#include "jf/battle/StatusEffects.hpp"

#include <algorithm>

#include "jf/battle/SkillCharges.hpp"
#include "jf/core/StatusEffect.hpp"

namespace jf {

namespace {
// docs/status_effects.md "地形・マス効果との関係": ending an action on a
// status-clearing tile (e.g. Ashbough Forest's Shallows) clears burn before
// its action-end damage is applied. No shipped TerrainType does this yet
// (Terrain.hpp has no Shallows-equivalent), so this always returns false
// today - it is the hook point for when that terrain ships.
// docs/regions/ember_ravine.md "共通地形"「冷却床」: "行動終了時、炎上ダメージ
// 前に炎上解除" - same action-end-before-damage timing Shallows already used.
bool terrainClearsBurn(TerrainType terrain) {
    return terrain == TerrainType::Shallows || terrain == TerrainType::CoolFloor;
}
} // namespace

void applyPoison(Unit& target) {
    target.poisonRemainingProcs = statusPoisonMaxProcs(target.isBoss);
}

void applyBurn(Unit& target) {
    // docs/regions/ember_ravine.md 敵勢力「岩蜥蜴」: negate exactly the first
    // Burn application this battle, then behave normally afterward.
    if (target.firstBurnNegatesRemaining > 0) {
        --target.firstBurnNegatesRemaining;
        return;
    }
    target.burnRemainingProcs = statusBurnMaxProcs(target.isBoss);
}

bool consumeUnyieldingSignalIfAvailable(BattleState& battle, Unit& target) {
    for (Unit& unit : battle.units()) {
        if (&unit == &target || unit.team != target.team || !unit.isAlive() || !hasBannerAura(unit.unitClass))
            continue;
        if (manhattanDistance(unit.position, target.position) > 2) continue;
        for (int slot = 0; slot < static_cast<int>(unit.skillSlots.size()); ++slot) {
            if (unit.skillSlots[static_cast<std::size_t>(slot)].skillId == "unyielding_signal" &&
                skillSlotAvailable(unit, slot)) {
                consumeSkillCharge(unit, slot);
                return true;
            }
        }
    }
    return false;
}

void applyMoveDown(BattleState& battle, Unit& target) {
    if (consumeUnyieldingSignalIfAvailable(battle, target)) return;
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

void applyStagger(BattleState& battle, Unit& target) {
    if (target.staggerImmune) return;
    if (consumeUnyieldingSignalIfAvailable(battle, target)) return;
    target.staggerActive = true;
}

void applyStatusEffect(BattleState& battle, Unit& target, StatusEffectType effect) {
    // M10-B (docs/deep_layers.md「防具用調整特性」`ward_step`): negate exactly
    // the first status effect applied through this choke point each battle,
    // regardless of which StatusEffectType it is - see
    // Unit::firstStatusNegatesRemaining's own comment. Scoped to this
    // function (the entry point for weapon on-hit statuses via
    // applyWeaponOnHitStatuses() below) rather than every individual
    // apply*() helper - a terrain-sourced Burn via the direct applyBurn(unit)
    // call at battle-phase-tick time is unaffected, same documented scope
    // limit firstBurnNegatesRemaining itself already has for its own single
    // effect.
    if (target.firstStatusNegatesRemaining > 0) {
        --target.firstStatusNegatesRemaining;
        return;
    }
    switch (effect) {
    case StatusEffectType::Poison: applyPoison(target); break;
    case StatusEffectType::Burn: applyBurn(target); break;
    case StatusEffectType::MoveDown: applyMoveDown(battle, target); break;
    case StatusEffectType::DefenseDown: applyDefenseDown(target); break;
    case StatusEffectType::Stagger: applyStagger(battle, target); break;
    }
}

void applyWeaponOnHitStatuses(BattleState& battle, Unit& attacker, Unit& target) {
    if (!target.isAlive() || attacker.weapon.onHitStatuses.empty()) return;
    // Snare Bow (docs/base_development.md): gated to the first successful
    // hit each battle - see Unit::weaponFirstHitUsed/Weapon::firstHitOnly.
    if (attacker.weapon.firstHitOnly) {
        if (attacker.weaponFirstHitUsed) return;
        attacker.weaponFirstHitUsed = true;
    }
    for (StatusEffectType effect : attacker.weapon.onHitStatuses) applyStatusEffect(battle, target, effect);
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
    // docs/regions/ember_ravine.md "共通地形"「炎上床」: "行動終了時に炎上を
    // 確定付与" - guaranteed, unlike a weapon's on-hit Burn which is only
    // ever applied after an already-confirmed hit; the tile itself is the
    // confirmation. Checked before the CoolFloor-clears-Burn branch below
    // since a unit can only ever stand on one of the two tiles at once.
    if (battle.terrainAt(unit.position) == TerrainType::FireFloor) applyBurn(unit);
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
        unit.quarryRevealed = false; // 辺境猟兵`read_quarry`: "次のEnemy Phase終了まで"
        unit.rallyingBannerActive = false; // 旗手`rallying_banner`: 同じく"次のEnemy Phase終了まで"
        // 連携作戦(docs/character_progression.md「連携作戦」): all 3 active
        // battle-effect pairs use the same "次のEnemy Phase終了まで" lifecycle
        // as rallyingBannerActive above.
        unit.pairedFallbackLineActive = false;
        unit.pairedSignalWardActive = false;
        unit.pairedBracedBreakthroughActive = false;
    }
}

void clearMoveUpAtPlayerPhaseEnd(BattleState& battle) {
    for (Unit& unit : battle.units()) {
        unit.moveUpActive = false;
        unit.urgentDispatchActive = false; // 伝令騎兵`urgent_dispatch`: same lifecycle
    }
}

} // namespace jf
