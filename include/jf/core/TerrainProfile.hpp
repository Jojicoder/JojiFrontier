#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "jf/core/Terrain.hpp"

namespace jf {

inline constexpr const char* kCinderwatchOutpostTerrain = "cinderwatch_outpost";
inline constexpr const char* kAshRoadTerrain = "ash_road";
inline constexpr const char* kSignalTowerTerrain = "signal_tower";
inline constexpr const char* kAshboughVergeTerrain = "ashbough_verge";
inline constexpr const char* kHerbwaterHollowTerrain = "herbwater_hollow";
inline constexpr const char* kBrokenwoodTerritoryTerrain = "brokenwood_territory";

struct WeightedTerrain {
    TerrainType terrain = TerrainType::Floor;
    int weight = 0;
};

struct TerrainCountBounds {
    TerrainType terrain = TerrainType::Floor;
    int minimum = 0;
    int maximum = 24;
};

struct TerrainProfile {
    std::string id;
    int generationVersion = 1;
    std::uint32_t seedSalt = 0;
    std::vector<WeightedTerrain> weights;
    TerrainType signatureTerrain = TerrainType::Floor;
    int maxBarriersPerColumn = 0;
    bool ensureHorizontalRoute = true;
    std::optional<TerrainCountBounds> countBounds;
};

std::optional<TerrainType> terrainTypeFromString(const std::string& id);
bool validateTerrainProfile(const TerrainProfile& profile, std::string* error = nullptr);

} // namespace jf
