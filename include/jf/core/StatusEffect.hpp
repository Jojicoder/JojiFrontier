#pragma once

namespace jf {

// 状態異常 (docs/status_effects.md). Confirmed-hit only (no RNG gate on
// application), each type is independent of the others, and every instance
// clears at battle end - Unit's status fields are battle-scoped transient
// state, never persisted (see docs/save_system.md).
enum class StatusEffectType {
    Poison,
    Burn,
    MoveDown,
    DefenseDown,
    Stagger
};

// Boss modifiers (docs/status_effects.md "ボス補正"): a boss takes reduced
// magnitude/count from every status effect rather than being immune to it.
// `isBoss` is the per-unit flag (Unit::isBoss) a boss encounter sets up
// front - no boss unit exists in the shipped data yet (docs/regions/
// ashbough_forest.md's Ash-Tusk Boar is still design-only), so these always
// return the normal-unit values in practice today.
int statusPoisonDamage(bool isBoss);
int statusPoisonMaxProcs(bool isBoss);
int statusBurnDamage(bool isBoss);
int statusBurnMaxProcs(bool isBoss);
int statusMoveDownAmount(bool isBoss);
int statusDefenseDownAmount(bool isBoss);

} // namespace jf
