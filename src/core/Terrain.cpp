#include "jf/core/Terrain.hpp"

namespace jf {

int movementCost(TerrainType terrain) {
    switch (terrain) {
        case TerrainType::Ash: return 2;
        case TerrainType::Rubble: return 2;
        case TerrainType::Barrier: return 999;
        case TerrainType::Floor:
        case TerrainType::WatchPost: return 1;
    }
    return 1;
}

int defenseBonus(TerrainType terrain) {
    return terrain == TerrainType::WatchPost ? 2 : 0;
}

bool isPassable(TerrainType terrain) {
    return terrain != TerrainType::Barrier;
}

std::string toString(TerrainType terrain) {
    switch (terrain) {
        case TerrainType::Floor: return "Floor";
        case TerrainType::Ash: return "Ash";
        case TerrainType::Rubble: return "Rubble";
        case TerrainType::Barrier: return "Barrier";
        case TerrainType::WatchPost: return "Watch Post";
    }
    return "Floor";
}

} // namespace jf
