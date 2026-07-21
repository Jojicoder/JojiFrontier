#pragma once

#include <string>
#include <vector>

#include "jf/battle/BattleState.hpp"
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
// `attackerBonusPower` (default 0): flat addition to the attacker's
// STR/MAG-equivalent before Might, e.g. 旗手「戦旗」's Aura bonus
// (BattleController.cpp computes it via bannerAuraBonus() since this
// function has no battle-wide unit list to query positions itself).
int computeDamage(const Unit& attacker, const Unit& target, int terrainDefense = 0,
                  int attackerBonusPower = 0);

CombatPreview previewAttack(const Unit& attacker, const Unit& target, int terrainDefense = 0,
                            int hitChance = 100, int attackerBonusPower = 0);

// Applies damage to `target.currentHp`, clamped at 0. `battle` is needed to
// apply any weapon on-hit status (Move Down/Stagger) - those consult 旗手
// `unyielding_signal` via applyMoveDown()/applyStagger() (StatusEffects.hpp).
void resolveAttack(BattleState& battle, const Unit& attacker, Unit& target, int terrainDefense = 0, bool hit = true,
                   int attackerBonusPower = 0);

// 旗手「戦旗」(docs/class_reference.md「後半6兵種」): `target`のマンハッタン
// 距離2以内に、`target`自身とは異なる生存中の味方BannerBearerが1体でもいれば+1、
// いなければ0(複数いても重複せず+1のまま)。
int bannerAuraBonus(const std::vector<Unit>& units, const Unit& target);

} // namespace jf
