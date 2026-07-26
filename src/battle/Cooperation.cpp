#include "jf/battle/Cooperation.hpp"

namespace jf {

const std::vector<CooperationDefinition>& cooperationDefinitions() {
    static const std::vector<CooperationDefinition> kDefinitions = {
        {"paired_fallback_line", "leon", "gareth", true},
        {"paired_cross_observation", "erin", "scout_reserve", false},
        {"paired_field_recovery", "mira", "ranger_recruit", true},
        {"paired_braced_breakthrough", "spear_reserve", "heavy_recruit", true},
        {"paired_rapid_works", "engineer_recruit", "cavalry_recruit", true},
        {"paired_signal_ward", "banner_recruit", "mage_recruit", true},
    };
    return kDefinitions;
}

const CooperationDefinition* findCooperationDefinition(const std::string& id) {
    for (const CooperationDefinition& def : cooperationDefinitions())
        if (def.id == id) return &def;
    return nullptr;
}

bool isCooperationUnlocked(const std::string& id, const BaseState& base) {
    if (id == "paired_fallback_line") return base.completedRegionIds.count(RegionId::CinderwatchGate) > 0;
    if (id == "paired_cross_observation") return base.completedRegionIds.count(RegionId::WindscarPlateau) > 0;
    if (id == "paired_field_recovery") return base.completedRegionIds.count(RegionId::BlackwaterLowlands) > 0;
    if (id == "paired_braced_breakthrough") return base.completedRegionIds.count(RegionId::AshironQuarry) > 0;
    if (id == "paired_rapid_works") return base.completedRegionIds.count(RegionId::WindscarPlateau) > 0;
    // docs/character_progression.md「連携作戦」表: 埋没聖堂攻略。EmberRavine's
    // own region-clear Slice added `RegionId::BuriedDawnSanctum` (still a
    // minimal 1-site outpost placeholder, same "add the stub, flesh out
    // later" state every prior region passed through), so this gate is now
    // wired the same way as every other region-gated pair above - it was
    // simply unreachable until that enum value/region implementation
    // existed, and clearing even the placeholder site satisfies
    // completedRegionIds the same way any other region's single-site stub
    // does.
    if (id == "paired_signal_ward") return base.completedRegionIds.count(RegionId::BuriedDawnSanctum) > 0;
    return false;
}

} // namespace jf
