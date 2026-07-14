#pragma once

#include "jf/battle/BattleObject.hpp"
#include "jf/battle/BattleState.hpp"
#include "jf/core/Unit.hpp"

namespace jf {

// docs/battle_objects.md "耐久とダメージ": physical = max(STR + Weapon Might
// - defense, 1), magical = max(MAG + Weapon Might - resistance, 1). Poison,
// burn, heal, status effects, Critical, counterattack, and follow-up never
// apply to Objects (deliberately not modeled here).
int computeObjectDamage(const Unit& attacker, const BattleObjectDefinition& def);

// Applies computeObjectDamage() to `target`, floored at 0 durability,
// transitioning it to Destroyed exactly once when it first reaches 0. Only
// affects `canBeAttacked` objects with `maxDurability > 0` (others are
// left untouched, returns false). Returns true iff THIS call is what
// caused the Destroyed transition, so the caller (BattleController, once
// wired) knows to emit ObjectDestroyedEvent exactly once.
bool resolveObjectAttack(BattleState& battle, const Unit& attacker, BattleObjectState& target);

// docs/battle_objects.md "操作": validates range (manhattan distance from
// `actor`, 0 meaning "must end its move on the object's own tile"),
// allowedClasses (empty == any class), requiredState, and maxUses (a
// non-positive `remaining uses` blocks further interaction) before
// transitioning `target.state` to `newState` and incrementing
// interactionCount. Returns true iff a real transition happened (so the
// caller emits ObjectStateChangedEvent exactly once); a validation failure
// or an already-`newState` object is a silent no-op.
bool resolveObjectInteraction(const Unit& actor, BattleObjectState& target,
                              const ObjectInteractionDefinition& interaction, BattleObjectStateKind newState);

} // namespace jf
