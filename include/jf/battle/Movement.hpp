#pragma once

#include <vector>

#include "jf/core/Grid.hpp"
#include "jf/core/Unit.hpp"
#include "jf/battle/BattleState.hpp"

namespace jf {

// Weighted grid search respecting movement range and terrain. Allies may be
// crossed but cannot be destinations; enemies block both crossing and stopping.
// Future Zone of Control rules can add selective path-expansion blocking.
std::vector<GridPos> computeReachableTiles(const std::vector<Unit>& units,
                                            const Unit& mover);
std::vector<GridPos> computeReachableTiles(const BattleState& battle,
                                            const Unit& mover);

// Tiles within [minRange, maxRange] (Manhattan distance) of `origin` that
// currently contain a living unit belonging to the opposing team.
std::vector<GridPos> computeTargetableTiles(const std::vector<Unit>& units,
                                             const Unit& attacker,
                                             GridPos origin);

// Every in-bounds tile within `attacker`'s weapon range of any tile in
// `fromTiles`, regardless of what (if anything) occupies it. Used to show
// the attacker's threat range as a preview - e.g. the union over every
// reachable move tile before the player commits to a move, or just the
// unit's current tile once it has already moved.
std::vector<GridPos> computeAttackRangeTiles(const Unit& attacker,
                                              const std::vector<GridPos>& fromTiles);

} // namespace jf
