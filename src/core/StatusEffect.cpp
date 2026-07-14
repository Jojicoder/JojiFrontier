#include "jf/core/StatusEffect.hpp"

namespace jf {

int statusPoisonDamage(bool isBoss) { return isBoss ? 1 : 2; }
int statusPoisonMaxProcs(bool isBoss) { return isBoss ? 2 : 3; }
int statusBurnDamage(bool isBoss) { return isBoss ? 2 : 3; }
int statusBurnMaxProcs(bool /*isBoss*/) { return 2; } // docs/status_effects.md: unchanged for bosses
int statusMoveDownAmount(bool isBoss) { return isBoss ? 1 : 2; }
int statusDefenseDownAmount(bool isBoss) { return isBoss ? 2 : 3; }

} // namespace jf
