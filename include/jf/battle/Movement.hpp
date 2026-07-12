#pragma once

#include <vector>

#include "jf/core/Grid.hpp"
#include "jf/core/Unit.hpp"

namespace jf {

// Breadth-first search over the grid, respecting `move` range and blocking
// on occupied tiles (other than the mover's own starting tile). Reusable by
// future terrain/class movement rules since occupancy/cost are the only
// inputs that would need to change.
std::vector<GridPos> computeReachableTiles(const std::vector<Unit>& units,
                                            const Unit& mover);

// Tiles within [minRange, maxRange] (Manhattan distance) of `origin` that
// currently contain a living unit belonging to the opposing team.
std::vector<GridPos> computeTargetableTiles(const std::vector<Unit>& units,
                                             const Unit& attacker,
                                             GridPos origin);

} // namespace jf
