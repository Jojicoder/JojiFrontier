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
    // 戦闘魔導士`ward_break`(魔防破砕): same read-only-here/cleared-by-a-real-hit
    // pattern as markedBonusDamage above, but only for a Magical attacker
    // ("次に受ける魔法攻撃のダメージ+3" - a physical follow-up must not consume it).
    int magicBonus = attacker.weapon.damageType == DamageType::Magical ? target.magicMarkedBonusDamage : 0;
    int raw = attacker.attackPower() + attackerBonusPower + attacker.weapon.might - defenseStat - terrainDefense +
              target.markedBonusDamage + magicBonus;
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

void resolveAttack(BattleState& battle, Unit& attacker, Unit& target, int terrainDefense, bool hit,
                   int attackerBonusPower) {
    if (!hit) return;
    int damage = computeDamage(attacker, target, terrainDefense, attackerBonusPower);
    target.currentHp = std::max(target.currentHp - damage, 0);
    target.markedBonusDamage = 0; // consumed by this real hit, if it was set
    if (attacker.weapon.damageType == DamageType::Magical) target.magicMarkedBonusDamage = 0;
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

namespace {
// Duel Sword/Quarry Bow share "no other living unit of `target`'s own team
// is adjacent to `target`" (Duel Sword: no other enemy of the attacker near
// the target; Quarry Bow: no ally of the target near it - both mean the
// same adjacency check from target's team's point of view).
bool hasAdjacentAllyOfTarget(const std::vector<Unit>& units, const Unit& target) {
    for (const Unit& unit : units) {
        if (&unit == &target || !unit.isAlive() || unit.team != target.team) continue;
        if (manhattanDistance(unit.position, target.position) == 1) return true;
    }
    return false;
}
} // namespace

int weaponBranchBonusDamage(const std::vector<Unit>& units, const Unit& attacker, const Unit& target, int round) {
    const std::string& weaponId = attacker.weapon.id;
    if (weaponId == "charge_lance") {
        // 突撃騎槍: 攻撃前に3マス以上移動していれば+2.
        if (attacker.tilesMovedThisAction >= 3) return 2;
    } else if (weaponId == "ambush_blade") {
        // 奇襲刃: そのラウンドで未行動の敵へ+2. `hasActed`だけでは判定できない
        // (自チームの次のPhase開始までリセットされないため、Player Phase中は
        // 前RoundのEnemy Phase終了時点の値=trueのまま残り続ける - 2Round目
        // 以降ずっと不発になるバグの元)。`Unit::lastActedRound`
        // (`BattleState::markActed()`が記録)と現在のRoundを比較して
        // 「今Roundではまだ行動していない」を正しく判定する。
        if (target.lastActedRound != round) return 2;
    } else if (weaponId == "war_bow") {
        // 戦弓: 対象が最大HPなら+2.
        if (target.currentHp >= target.stats.maxHp) return 2;
    } else if (weaponId == "duel_sword" || weaponId == "quarry_bow") {
        // 決闘剣/追跡弓: 対象の隣接マスに(対象と同じチームの)他ユニットがいなければ+2.
        if (!hasAdjacentAllyOfTarget(units, target)) return 2;
    }
    return 0;
}

} // namespace jf
