#pragma once

#include <optional>

#include "jf/core/UnitClass.hpp"

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

// docs/prompts/exploration_system_improvement_prompt.md(2026-08-02設計・
// Phase 1「最初に実装すべき変更」): 本編62地点のうち`routeOutcomes`を個別
// 設定しているのは15地点だけで、残りは全て単一の`cinderwatchOutcome()`へ
// フォールバックしていた(地形や地域の違いが探索結果へ一切反映されない)。
// この単一フォールバックを、地形テーマ別の8種類のテンプレートへ置き換える。
// 既存の`ExplorationOutcome`の7フィールドだけを使い(スキーマ拡張はしない)、
// `stageRouteOutcome()`側でstage.idからテンプレートを推定して適用する
// (`explorationTemplateForStageId()`、Region.cpp)。
enum class ExplorationTemplate {
    Forest,        // 灰枝の森系: 木々・獣道
    Mountain,      // 風裂き高原/燼火峡谷系: 断崖・強風・熱気
    Mine,          // 灰鉄採石場系: 坑道・崩落・狭所
    Marsh,         // 黒水低湿地系: 泥濘・水路
    Ruins,         // 埋没聖堂系: 崩れた遺構
    Settlement,    // 旧辺境集落系: 建物・生活区画
    Fortification, // 沈黙した監視所群/破砕された前線砦系: 城壁・監視塔
    OpenField,     // 地図外縁系: 開けた荒野
};

inline ExplorationOutcome explorationTemplateOutcome(ExplorationTemplate tmpl, ExplorationChoice choice) {
    switch (tmpl) {
        case ExplorationTemplate::Forest:
            if (choice == ExplorationChoice::CollapsedSidePath) return {.partyDamage = 2, .enemiesRemoved = 1};
            if (choice == ExplorationChoice::ScoutRoute) return {.enableFreeDeployment = true, .deploymentMaxColumn = 2};
            return {};
        case ExplorationTemplate::Mountain:
            // Steep/exposed ground: the side detour costs more attrition,
            // and the scouted ridge line only leaves room for a narrow
            // formation (col 0-1) instead of the usual 3-wide zone.
            if (choice == ExplorationChoice::CollapsedSidePath) return {.partyDamage = 3, .enemiesRemoved = 1};
            if (choice == ExplorationChoice::ScoutRoute) return {.enableFreeDeployment = true, .deploymentMaxColumn = 1};
            return {};
        case ExplorationTemplate::Mine:
            // Tight tunnels: both the shortcut and the scouted route drag an
            // extra piece of rubble/support timber onto the field.
            if (choice == ExplorationChoice::CollapsedSidePath)
                return {.partyDamage = 1, .enemiesRemoved = 1, .extraBarrierCount = 1};
            if (choice == ExplorationChoice::ScoutRoute)
                return {.enableFreeDeployment = true, .deploymentMaxColumn = 2, .extraBarrierCount = 1};
            return {};
        case ExplorationTemplate::Marsh:
            // Wading straight through draws attention over time; the two
            // alternate routes avoid that but keep the usual tradeoffs.
            if (choice == ExplorationChoice::FrontalAdvance) return {.enableReinforcementWave = true};
            if (choice == ExplorationChoice::CollapsedSidePath) return {.partyDamage = 2, .enemiesRemoved = 1};
            if (choice == ExplorationChoice::ScoutRoute) return {.enableFreeDeployment = true, .deploymentMaxColumn = 2};
            return {};
        case ExplorationTemplate::Ruins:
            // Collapsed side passages leave debris behind as an extra
            // Barrier, same idea as Mine but without the tunnel penalty on
            // the scouted route.
            if (choice == ExplorationChoice::CollapsedSidePath)
                return {.partyDamage = 2, .enemiesRemoved = 1, .extraBarrierCount = 1};
            if (choice == ExplorationChoice::ScoutRoute) return {.enableFreeDeployment = true, .deploymentMaxColumn = 2};
            return {};
        case ExplorationTemplate::Settlement:
            if (choice == ExplorationChoice::CollapsedSidePath) return {.partyDamage = 2, .enemiesRemoved = 1};
            if (choice == ExplorationChoice::ScoutRoute) return {.enableFreeDeployment = true, .deploymentMaxColumn = 2};
            return {};
        case ExplorationTemplate::Fortification:
            // Assaulting the gate head-on draws garrison reinforcements;
            // breaching a side wall costs more attrition than usual, and the
            // scouted approach is cramped by the wall itself.
            if (choice == ExplorationChoice::FrontalAdvance) return {.enableReinforcementWave = true};
            if (choice == ExplorationChoice::CollapsedSidePath) return {.partyDamage = 3, .enemiesRemoved = 1};
            if (choice == ExplorationChoice::ScoutRoute) return {.enableFreeDeployment = true, .deploymentMaxColumn = 1};
            return {};
        case ExplorationTemplate::OpenField:
            // Exposed, few complications: the mildest detour cost of any
            // template.
            if (choice == ExplorationChoice::CollapsedSidePath) return {.partyDamage = 1, .enemiesRemoved = 1};
            if (choice == ExplorationChoice::ScoutRoute) return {.enableFreeDeployment = true, .deploymentMaxColumn = 2};
            return {};
    }
    return {};
}

// docs/prompts/exploration_system_improvement_prompt.md(2026-08-02、Phase 2):
// which class's route-C flavor best fits each template, per docs/
// exploration_system.md「兵種による探索能力」(辺境斥候=偵察・隠し道・高所観測、
// 辺境工兵=障害物・機械・遺跡装置、辺境猟兵=追跡・狩猟・野生生物、暁の衛生兵=
// 負傷者・医療記録、重装兵=瓦礫除去・強行突破・重量物運搬). Used only as the
// DEFAULT for stages that don't set StageDescriptor::scoutRouteRequiredClass
// explicitly - a hand-authored stage's own choice always wins.
inline UnitClass explorationTemplateDefaultClass(ExplorationTemplate tmpl) {
    switch (tmpl) {
        case ExplorationTemplate::Mine:
        case ExplorationTemplate::Ruins:
            return UnitClass::FrontierEngineer; // 障害物・機械・遺跡装置
        case ExplorationTemplate::Marsh:
            return UnitClass::FrontierRanger; // 追跡・狩猟・野生生物(黒水低湿地=辺境猟兵の所属地域と一致)
        case ExplorationTemplate::Settlement:
            return UnitClass::DawnChirurgeon; // 負傷者・医療記録
        case ExplorationTemplate::OpenField:
            return UnitClass::HeavyInfantry; // 瓦礫除去・強行突破
        case ExplorationTemplate::Forest:
            // 灰枝の森(辺境斥候の発見・所属地域)がこのテンプレートの唯一の
            // 実例のため、既定はFrontierScoutのまま変更しない。
        case ExplorationTemplate::Mountain:
        case ExplorationTemplate::Fortification:
            return UnitClass::FrontierScout; // 偵察・隠し道・高所観測(既定と同じ)
    }
    return UnitClass::FrontierScout;
}

} // namespace jf
