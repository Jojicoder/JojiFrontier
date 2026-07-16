#pragma once

#include <string>
#include <vector>

#include "jf/core/Grid.hpp"

namespace jf {

enum class TelegraphShape { Line, Area, TargetUnit };
enum class TelegraphState { None, Announced, Executed, Cancelled };

struct BossTelegraph {
    std::string actionId;
    TelegraphShape shape = TelegraphShape::Line;
    TelegraphState state = TelegraphState::None;
    int announcedRound = 0;
    int executeRound = 0;
    std::string targetUnitId;
    std::vector<GridPos> lockedTiles;
    int direction = 0;

    bool pending() const { return state == TelegraphState::Announced; }
    void clear() {
        actionId.clear();
        state = TelegraphState::None;
        announcedRound = executeRound = 0;
        targetUnitId.clear();
        lockedTiles.clear();
        direction = 0;
    }
};

struct BossRuntimeState {
    int stageIndex = 0;
    BossTelegraph telegraph;
};

} // namespace jf
