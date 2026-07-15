#include "jf/core/TerrainProfile.hpp"

#include <unordered_set>

namespace jf {

std::optional<TerrainType> terrainTypeFromString(const std::string& id) {
    if (id == "Floor") return TerrainType::Floor;
    if (id == "Ash") return TerrainType::Ash;
    if (id == "Rubble") return TerrainType::Rubble;
    if (id == "Barrier") return TerrainType::Barrier;
    if (id == "WatchPost") return TerrainType::WatchPost;
    if (id == "Brush") return TerrainType::Brush;
    if (id == "HerbPatch") return TerrainType::HerbPatch;
    if (id == "Shallows") return TerrainType::Shallows;
    return std::nullopt;
}

bool validateTerrainProfile(const TerrainProfile& profile, std::string* error) {
    auto fail = [&](const std::string& message) {
        if (error) *error = message;
        return false;
    };
    if (profile.id.empty()) return fail("terrain profile id is empty");
    if (profile.generationVersion < 1) return fail("generationVersion must be at least 1");
    if (profile.weights.empty()) return fail("terrain weights are empty");
    int totalWeight = 0;
    std::unordered_set<int> terrainIds;
    for (const WeightedTerrain& entry : profile.weights) {
        if (entry.weight <= 0) return fail("terrain weight must be positive");
        if (!terrainIds.insert(static_cast<int>(entry.terrain)).second)
            return fail("terrain weight contains a duplicate terrain type");
        totalWeight += entry.weight;
    }
    if (totalWeight != 100) return fail("terrain weights must total 100");
    if (profile.maxBarriersPerColumn < 0 || profile.maxBarriersPerColumn > 3)
        return fail("maxBarriersPerColumn must be between 0 and 3");
    if (profile.countBounds) {
        if (profile.countBounds->minimum < 0 ||
            profile.countBounds->maximum < profile.countBounds->minimum ||
            profile.countBounds->maximum > 24)
            return fail("terrain count bounds are invalid");
    }
    return true;
}

} // namespace jf
