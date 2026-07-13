#pragma once

#include <string>

namespace jf {

enum class TerrainType {
    Floor,
    Ash,
    Rubble,
    Barrier,
    WatchPost
};

int movementCost(TerrainType terrain);
int defenseBonus(TerrainType terrain);
bool isPassable(TerrainType terrain);
std::string toString(TerrainType terrain);

} // namespace jf
