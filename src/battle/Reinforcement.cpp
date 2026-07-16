#include "jf/battle/Reinforcement.hpp"

#include <unordered_set>

namespace jf {

bool validateReinforcementWaves(const std::vector<ReinforcementWave>& waves,
                                bool defenseMission,
                                std::vector<std::string>* errors) {
    bool valid = true;
    int total = 0;
    std::unordered_set<std::string> ids;
    auto fail = [&](const std::string& message) {
        valid = false;
        if (errors) errors->push_back(message);
    };
    for (const ReinforcementWave& wave : waves) {
        if (wave.id.empty() || !ids.insert(wave.id).second) fail("duplicate or empty reinforcement wave id");
        if (wave.spawnRound < 1 || wave.announceRoundsBefore < 0 || wave.announceRoundsBefore > 2)
            fail("invalid reinforcement timing: " + wave.id);
        if (wave.units.empty() || wave.units.size() > 4) fail("reinforcement wave must contain 1-4 units: " + wave.id);
        if (wave.orderedSpawnCandidates.empty()) fail("reinforcement wave has no spawn candidates: " + wave.id);
        total += static_cast<int>(wave.units.size());
    }
    if (total > (defenseMission ? 12 : 8)) fail("reinforcement unit limit exceeded");
    return valid;
}

} // namespace jf
