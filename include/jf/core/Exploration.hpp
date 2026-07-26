#pragma once

#include <optional>

namespace jf {

enum class ExplorationChoice { FrontalAdvance, CollapsedSidePath, ScoutRoute };

struct ExplorationOutcome {
    int partyDamage = 0;
    int enemiesRemoved = 0;
    bool enableFreeDeployment = false;
    int deploymentMaxColumn = 0;
    // docs/regions/ashbough_forest.md "薬草の沢"衛生兵ルート: "味方全員を左2列の
    // ランダム候補へ制限" - auto-random placement (not player-driven like
    // enableFreeDeployment) but confined to a narrower left-edge zone than
    // the usual 3 columns. nullopt means "use the normal zone width".
    std::optional<int> restrictedAutoSpawnMaxColumn;
    // docs/regions/ashbough_forest.md "折れ木の縄張り"'s route B ("倒木を戦場へ
    // 誘導する"): one additional fallen-log Barrier beyond the stage's
    // baseline count.
    int extraBarrierCount = 0;
    // docs/regions/ember_ravine.md 地点1「焼け石の入口」ルート1「火の切れ間を
    // 待つ」: this route's own "熱量1" (battle starts at heat level 1),
    // consumed by BattleFactory.cpp into BattleState::setHeatLevel(). 0 (the
    // default) matches every route that doesn't mention a starting 熱量.
    int startingHeatLevel = 0;
    bool enableReinforcementWave = false;
};

// Generic route-outcome shape for any region's first-battle 3-choice
// exploration (Cinderwatch's A/B/C and docs/regions/ashbough_forest.md's
// 灰枝の林縁 both use exactly this): rush = attrition + one fewer enemy,
// scout = free deployment in the left 3 columns. Kept as one function
// (rather than a per-region copy) since every region implemented so far
// wants the same numbers here; a future region that needs different ones
// gets its own function then, not before.
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
