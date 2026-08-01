#pragma once

// docs/deep_layers.md「敵強化率」: 深層/最深層の敵ステータスへ一律倍率をかける
// ためのポストプロセス。StageDescriptor自体にはスケーリングの概念がなく
// (本編は素材/ルート差分のみ)、UnitTemplateも`data/classes.json`のベース値を
// そのまま使うため、BattleFactory::createScenarioBattle()/
// createScenarioContinuationBattle()で組み上がったBattleStateへ後掛けする。

#include <cmath>

#include "jf/battle/BattleState.hpp"
#include "jf/core/UnitClass.hpp"

namespace jf {

struct DeepLayerEnemyScaling {
    float hpMult = 1.0f;
    float atkMult = 1.0f;  // strength/magic
    float defMult = 1.0f;  // defense/resistance
};

// Applies to every Team::Enemy unit already in `battle` - call right after
// createScenarioBattle()/createScenarioContinuationBattle(), before wrapping
// in a BattleController. currentHp is reset to the new maxHp (enemies always
// start a stage at full health, same as every other stage in the game).
inline void applyDeepLayerEnemyScaling(BattleState& battle, const DeepLayerEnemyScaling& scale) {
    for (Unit& unit : battle.units()) {
        if (unit.team != Team::Enemy) continue;
        unit.stats.maxHp = std::max(1, static_cast<int>(std::lround(unit.stats.maxHp * scale.hpMult)));
        unit.currentHp = unit.stats.maxHp;
        unit.stats.strength = static_cast<int>(std::lround(unit.stats.strength * scale.atkMult));
        unit.stats.magic = static_cast<int>(std::lround(unit.stats.magic * scale.atkMult));
        unit.stats.defense = static_cast<int>(std::lround(unit.stats.defense * scale.defMult));
        unit.stats.resistance = static_cast<int>(std::lround(unit.stats.resistance * scale.defMult));
    }
}

// docs/deep_layers.md「敵強化率」表の3段階(深層・ボス1体目/深層・ボス2体目/
// 最深層・ボス) - 深層内の道中(ザコ戦)は次に控えるボスと同じ段階の倍率を使う
// (深層1戦目+ボス1体目=段階1、深層2戦目+ボス2体目=段階2、最深層道中+ボス=段階3)。
// 2026-08-01: ユーザー指摘「深層の敵を強くして」を受け、3段階とも引き上げ
// (旧: 1.3/1.15/1.1, 1.6/1.3/1.2, 3.0/2.1/1.6)。今後の再調整時の目安として、
// 各値の引き上げ幅(旧→新の比率)を記録しておく:
//   段階1: HP +23.1%, STR/MAG +17.4%, DEF/RES +13.6%
//   段階2: HP +31.2%, STR/MAG +23.1%, DEF/RES +16.7%
//   段階3: HP +26.7%, STR/MAG +23.8%, DEF/RES +18.7%
// jf_forest_balance実測記録はdocs/deep_layers.md「敵強化率」節参照。
inline constexpr DeepLayerEnemyScaling kDeepLayerScaleStage1{1.6f, 1.35f, 1.25f};
inline constexpr DeepLayerEnemyScaling kDeepLayerScaleStage2{2.1f, 1.6f, 1.4f};
inline constexpr DeepLayerEnemyScaling kDeepLayerScaleStage3{3.8f, 2.6f, 1.9f};

} // namespace jf
