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
    Shallows
};

int movementCost(TerrainType terrain);
int defenseBonus(TerrainType terrain);
int evasionBonus(TerrainType terrain);
bool isPassable(TerrainType terrain);
std::string toString(TerrainType terrain);

} // namespace jf
