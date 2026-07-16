#pragma once

#include "jf/battle/BattleState.hpp"
#include "jf/battle/AiSystem.hpp"

namespace jf {

// docs/enemy_ai_rules.md: every non-Boss enemy (Wolves included) generates
// AiCandidates via its AiProfile (jf/battle/AiSystem.hpp - profileFor()/
// generateAiCandidates()/chooseBestAiCandidate()), scored and deterministic-
// tie-broken, then acts on the winner. `reservations`, when provided, holds
// one Enemy Phase's worth of squad-level state (reserved destinations/
// damage/support targets) so multiple enemies don't pile onto the same
// target or tile. AshenhornBoar (and any future Boss) still uses its own
// dedicated turn function instead. Returns the unit that was attacked this
// turn (nullptr if none) so the
// caller (BattleController) can report an attack event for UI purposes
// (e.g. driving a front-end attack animation) without EnemyAI knowing
// anything about rendering.
Unit* takeEnemyTurn(BattleState& battle, Unit& enemy, AiSquadReservations* reservations = nullptr);

} // namespace jf
