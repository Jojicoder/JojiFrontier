#pragma once

namespace jf {

enum class ExplorationChoice { FrontalAdvance, CollapsedSidePath, ScoutRoute };

struct ExplorationOutcome {
    int partyDamage = 0;
    int enemiesRemoved = 0;
    bool enableFreeDeployment = false;
    int deploymentMaxColumn = 0;
};

inline ExplorationOutcome cinderwatchOutcome(ExplorationChoice choice) {
    if (choice == ExplorationChoice::CollapsedSidePath) return {.partyDamage = 2, .enemiesRemoved = 1};
    // Scouting from high ground costs no attrition, but the party goes in
    // via a side approach: the player freely places all 4 units anywhere
    // passable in the leftmost 3 columns (col 0-2) instead of the usual
    // fixed formation.
    if (choice == ExplorationChoice::ScoutRoute) return {.enableFreeDeployment = true, .deploymentMaxColumn = 2};
    return {};
}

} // namespace jf
