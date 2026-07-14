#pragma once

#include "jf/battle/BattleState.hpp"

namespace jf {

// Application (docs/status_effects.md "共通ルール"): call these only after
// an attack/skill/tile effect has already confirmed its hit - there is no
// internal chance gate. Each effect is independent of the others, and
// reapplying an already-active one resets it to its full count/duration
// rather than stacking.
void applyPoison(Unit& target);
void applyBurn(Unit& target);
void applyMoveDown(Unit& target);
void applyDefenseDown(Unit& target);
// No-op while the target is stagger-immune (docs/status_effects.md
// "よろめき" 再付与耐性).
void applyStagger(Unit& target);

// 万能薬・状態治療スキル: clears every status effect on one unit. Leaves
// staggerImmune untouched - that is a cooldown against reapplication, not a
// status to cure.
void clearAllStatusEffects(Unit& target);

// Battle end (Victory/Defeat): status effects never carry into the next
// battle (docs/status_effects.md).
void clearAllStatusEffects(BattleState& battle);

// Action-end pipeline (docs/status_effects.md "地形・マス効果との関係"
// processing order): clears burn if the unit ended its action on a
// status-clearing tile (hook point for future terrain, e.g. Ashbough
// Forest's Shallows - no shipped TerrainType clears burn yet), applies
// burn's action-end damage, then clears stagger. Call this once per unit
// right after it finishes acting (after any terrain heal, per the doc's
// step ordering), for both player and enemy units.
void processActionEndStatusEffects(BattleState& battle, Unit& unit);

// Phase-end pipeline: ticks poison (capped so it never brings HP below 1)
// for every living unit of `team`, then expires that team's move-down,
// defense-down, and stagger-immunity - all three are defined as lasting
// "until this side's own next phase ends". Call this right before the
// phase actually flips (i.e. for the team whose phase is ending).
void processPhaseEndStatusEffects(BattleState& battle, Team team);

} // namespace jf
