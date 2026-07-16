#pragma once

#include <string>
#include <vector>

#include "jf/battle/Phase.hpp"
#include "jf/core/Grid.hpp"
#include "jf/core/Unit.hpp"

namespace jf {

enum class ReinforcementState { Scheduled, Announced, Spawned, Prevented, Cancelled };
enum class ReinforcementResult { Spawned, Prevented, Cancelled };

struct ReinforcementSpawn {
    Unit unit;
};

struct ReinforcementWave {
    std::string id;
    Team team = Team::Enemy;
    int spawnRound = 1;
    Phase spawnPhase = Phase::EnemyPhase;
    bool requiredForElimination = true;
    int announceRoundsBefore = 1;
    std::vector<ReinforcementSpawn> units;
    std::vector<GridPos> orderedSpawnCandidates;
    ReinforcementState state = ReinforcementState::Scheduled;
    bool announcementConsumed = false;
};

bool validateReinforcementWaves(const std::vector<ReinforcementWave>& waves,
                                bool defenseMission,
                                std::vector<std::string>* errors = nullptr);

} // namespace jf
