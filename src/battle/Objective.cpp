#include "jf/battle/Objective.hpp"

namespace jf {

BattleMissionState defaultEliminateEnemiesMission() {
    BattleMissionState mission;
    mission.groups.push_back({"primary", ObjectiveGroupRule::All});
    ObjectiveDefinition eliminateEnemies;
    eliminateEnemies.id = "eliminate_enemies";
    eliminateEnemies.kind = ObjectiveKind::EliminateTeam;
    eliminateEnemies.primary = true;
    eliminateEnemies.groupId = "primary";
    eliminateEnemies.target.team = Team::Enemy;
    mission.definitions.push_back(eliminateEnemies);
    mission.progress[eliminateEnemies.id] = ObjectiveProgress{eliminateEnemies.id};
    return mission;
}

} // namespace jf
