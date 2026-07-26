#pragma once

#include <string>

namespace jf {

enum class TerrainType {
    Floor,
    Ash,
    Rubble,
    Barrier,
    WatchPost,
    Brush,
    HerbPatch,
    // docs/regions/ashbough_forest.md "薬草の沢": movement cost 2, passable,
    // no defense/evasion bonus. No current unit class ignores this penalty
    // (only a future flying class would).
    Shallows,
    // docs/regions/windscar_plateau.md "強風帯"/"強風ルール": movement cost 1,
    // passable, no defense/evasion bonus by itself - its effect is the
    // forced Round-end push (BattleState::resolveWindGustRoundEnd()), not a
    // per-tile stat modifier.
    WindGust,
    // docs/regions/ember_ravine.md "共通地形"/"戦場熱量" (6 new terrain kinds
    // for the new region's headline mechanic):
    // 焼け石床: movement cost 1, passable, no defense/evasion bonus.
    EmberFloor,
    // 熱砂: movement cost 2, passable, no defense/evasion bonus.
    HotSand,
    // 炎上床: movement cost 1, passable. Guaranteed Burn on action end
    // (StatusEffects.cpp's processActionEndStatusEffects()) - not a chance
    // gate like a weapon's on-hit Burn, this tile always applies it.
    FireFloor,
    // 冷却床: movement cost 1, passable. Clears Burn on action end, before
    // Burn's own damage tick (StatusEffects.cpp's terrainClearsBurn()) -
    // and exempts the standing unit from 戦場熱量 level 3's fixed Phase-end
    // damage (resolveEmberHeatPhaseEnd()).
    CoolFloor,
    // 噴気予告床: movement cost 1, passable. Telegraphs for exactly 1 Round,
    // then converts to FireFloor at the next Round End
    // (BattleState::resolveEmberFumeRoundEnd()) - same "telegraph 1 Round
    // ahead" shape docs/regions/ashiron_quarry.md's (never implemented)
    // 崩落予告床 described, built for real here per this region's own
    // 実装順 item 1.
    FumeWarning,
    // 灰煙床: movement cost 2, passable, no defense/evasion bonus (doc is
    // explicit this is unlike Brush's evasion bonus and unrelated to the
    // smoke-canister tile effect).
    AshSmoke
};

int movementCost(TerrainType terrain);
int defenseBonus(TerrainType terrain);
int evasionBonus(TerrainType terrain);
bool isPassable(TerrainType terrain);
std::string toString(TerrainType terrain);

} // namespace jf
