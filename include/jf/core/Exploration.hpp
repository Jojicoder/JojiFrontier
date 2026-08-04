#pragma once

#include <algorithm>
#include <array>
#include <optional>
#include <random>
#include <string_view>
#include <vector>

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

// docs/prompts/exploration_randomization_prompt.md(2026-08-02設計): one
// candidate for a template-fallback stage's 3-choice screen. `id` is a
// stable identifier (test/debug use only, not shown to the player);
// `labelKey` is the Locale Key for the button's label text.
struct ExplorationChoiceOption {
    std::string_view id;
    std::string_view labelKey;
    ExplorationOutcome outcome;
    std::optional<UnitClass> requiredClass;
};

// The 4 candidates for one template: the template's existing 3 (frontal/
// side-path/scout, unchanged) plus 1 new option with a different tradeoff.
// Kept at 4 (not the prompt's suggested 5-6) so the added candidates and
// their Locale Keys stay a one-per-template, reviewable amount of new
// content rather than ~40 new strings; more can be appended later without
// changing rollExplorationChoices()'s logic.
inline std::array<ExplorationChoiceOption, 4> explorationOptionPool(ExplorationTemplate tmpl) {
    const ExplorationChoiceOption frontal{"frontal", "exploration.frontal_advance",
                                          explorationTemplateOutcome(tmpl, ExplorationChoice::FrontalAdvance),
                                          std::nullopt};
    const ExplorationChoiceOption side{"side_path", "exploration.side_path",
                                       explorationTemplateOutcome(tmpl, ExplorationChoice::CollapsedSidePath),
                                       std::nullopt};
    const ExplorationChoiceOption scout{"scout_route", "exploration.scout_route",
                                        explorationTemplateOutcome(tmpl, ExplorationChoice::ScoutRoute),
                                        explorationTemplateDefaultClass(tmpl)};
    switch (tmpl) {
        case ExplorationTemplate::Forest:
            return {frontal, side, scout,
                    ExplorationChoiceOption{"forest_brush_cover", "exploration.option.forest_brush_cover",
                                            ExplorationOutcome{.extraBarrierCount = 2}, std::nullopt}};
        case ExplorationTemplate::Mountain:
            return {frontal, side, scout,
                    ExplorationChoiceOption{"mountain_leeward_hollow", "exploration.option.mountain_leeward_hollow",
                                            ExplorationOutcome{.restrictedAutoSpawnMaxColumn = 2}, std::nullopt}};
        case ExplorationTemplate::Mine:
            return {frontal, side, scout,
                    ExplorationChoiceOption{"mine_engineer_breach", "exploration.option.mine_engineer_breach",
                                            ExplorationOutcome{.enemiesRemoved = 1, .extraBarrierCount = 1},
                                            UnitClass::FrontierEngineer}};
        case ExplorationTemplate::Marsh:
            return {frontal, side, scout,
                    ExplorationChoiceOption{"marsh_ranger_ford", "exploration.option.marsh_ranger_ford",
                                            ExplorationOutcome{.restrictedAutoSpawnMaxColumn = 2},
                                            UnitClass::FrontierRanger}};
        case ExplorationTemplate::Ruins:
            // Same requiredClass as `scout` (both FrontierEngineer, per
            // explorationTemplateDefaultClass(Ruins)) so rollExplorationChoices()
            // swapping this in for slot C never changes which class the
            // stage's gated slot requires - only its effect.
            return {frontal, side, scout,
                    ExplorationChoiceOption{"ruins_seal_reading", "exploration.option.ruins_seal_reading",
                                            ExplorationOutcome{.restrictedAutoSpawnMaxColumn = 2},
                                            UnitClass::FrontierEngineer}};
        case ExplorationTemplate::Settlement:
            // Same requiredClass as `scout` (DawnChirurgeon), same reason.
            return {frontal, side, scout,
                    ExplorationChoiceOption{"settlement_rally", "exploration.option.settlement_rally",
                                            ExplorationOutcome{.restrictedAutoSpawnMaxColumn = 2},
                                            UnitClass::DawnChirurgeon}};
        case ExplorationTemplate::Fortification:
            return {frontal, side, scout,
                    ExplorationChoiceOption{"fort_signal_tower", "exploration.option.fort_signal_tower",
                                            ExplorationOutcome{.enemiesRemoved = 1, .enableReinforcementWave = true},
                                            std::nullopt}};
        case ExplorationTemplate::OpenField:
            return {frontal, side, scout,
                    ExplorationChoiceOption{"open_dust_rush", "exploration.option.open_dust_rush",
                                            ExplorationOutcome{.enableFreeDeployment = true,
                                                               .deploymentMaxColumn = 2, .startingHeatLevel = 1},
                                            std::nullopt}};
    }
    return {frontal, side, scout, frontal};
}

// Rolls which 2 of the pool's 4 candidates are actually shown, keeping the
// 3 UI slots' ROLES fixed: slot A (index 0, FrontalAdvance) is always
// `pool[0]` (never class-gated, matching every existing assumption that at
// least one route is always pickable regardless of party composition);
// exactly one of slot B (CollapsedSidePath) or slot C (ScoutRoute) becomes a
// coin flip against `pool[3]`, depending on whether `pool[3]` is class-gated
// - an ungated 4th candidate only ever replaces the ungated slot B, and a
// gated one only ever replaces the gated slot C (see explorationOptionPool()
// comments: every gated 4th candidate requires the SAME class as
// `pool[2]`/scout_route for its template). This means `requiredClassForChoice
// (ExplorationChoice::ScoutRoute)` never changes across a reroll - only the
// concrete effect behind it might - so `stage.scoutRouteDisabled` and every
// existing "does this stage's ScoutRoute need a Scout?" check keeps working
// unmodified. Deterministic given a seeded RandomEngine, so this is directly
// unit-testable without raylib/GameApp.
template <class RandomEngine>
std::array<ExplorationChoiceOption, 3> rollExplorationChoices(ExplorationTemplate tmpl, RandomEngine& rng) {
    const std::array<ExplorationChoiceOption, 4> pool = explorationOptionPool(tmpl);
    std::array<ExplorationChoiceOption, 3> result{pool[0], pool[1], pool[2]};
    std::uniform_int_distribution<int> coinFlip(0, 1);
    if (coinFlip(rng) == 1) {
        if (pool[3].requiredClass) {
            result[2] = pool[3];
        } else {
            result[1] = pool[3];
        }
    }
    return result;
}

// docs/prompts/exploration_effect_summary_improvement_prompt.md(2026-08-02
// 設計、項目4「テスト可能性の向上」): buildExplorationEffectSummary()
// (src/ui_shared.cpp, raylib依存のUI層でヘッドレステスト不可)から、
// 「どのフィールドが非デフォルトか」「危険度(tone)」を判定する部分だけを
// jf_lib側の純粋ロジックとして切り出したもの。実際の文言・配色はUI層が
// kind/toneから作る - このstruct自体は文字列もColorも持たない。
enum class ExplorationEffectTone { Neutral, Benefit, Caution, Danger };

enum class ExplorationEffectKind {
    PartyDamage,
    EnemiesRemoved,
    FreeDeployment,
    RestrictedDeployment,
    ExtraBarrier,
    StartingHeat,
    ReinforcementWave,
};

struct ExplorationEffectToken {
    ExplorationEffectKind kind;
    ExplorationEffectTone tone;
    // {n}-substitution amount for kinds that carry a count; unused (0) for
    // boolean-only kinds like ReinforcementWave.
    int amount = 0;
};

struct ExplorationEffectSummary {
    bool hasEffect = false;
    std::vector<ExplorationEffectToken> tokens;
};

// Tone assignment reflects docs/prompts/exploration_effect_summary_improvement_prompt.md's
// item 2 rules: attrition scales Caution->Danger with severity, anything
// that helps the party is Benefit, a placement constraint or an early heat
// handicap is Caution, and an enemy reinforcement wave is always Danger.
inline ExplorationEffectSummary summarizeExplorationOutcome(const ExplorationOutcome& outcome) {
    ExplorationEffectSummary summary;
    if (outcome.partyDamage > 0) {
        const ExplorationEffectTone tone =
            outcome.partyDamage >= 3 ? ExplorationEffectTone::Danger : ExplorationEffectTone::Caution;
        summary.tokens.push_back({ExplorationEffectKind::PartyDamage, tone, outcome.partyDamage});
    }
    if (outcome.enemiesRemoved > 0) {
        summary.tokens.push_back(
            {ExplorationEffectKind::EnemiesRemoved, ExplorationEffectTone::Benefit, outcome.enemiesRemoved});
    }
    if (outcome.enableFreeDeployment) {
        summary.tokens.push_back(
            {ExplorationEffectKind::FreeDeployment, ExplorationEffectTone::Benefit, outcome.deploymentMaxColumn + 1});
    }
    if (outcome.restrictedAutoSpawnMaxColumn) {
        summary.tokens.push_back({ExplorationEffectKind::RestrictedDeployment, ExplorationEffectTone::Caution,
                                  *outcome.restrictedAutoSpawnMaxColumn + 1});
    }
    if (outcome.extraBarrierCount > 0) {
        summary.tokens.push_back(
            {ExplorationEffectKind::ExtraBarrier, ExplorationEffectTone::Neutral, outcome.extraBarrierCount});
    }
    if (outcome.startingHeatLevel > 0) {
        summary.tokens.push_back(
            {ExplorationEffectKind::StartingHeat, ExplorationEffectTone::Caution, outcome.startingHeatLevel});
    }
    if (outcome.enableReinforcementWave) {
        summary.tokens.push_back({ExplorationEffectKind::ReinforcementWave, ExplorationEffectTone::Danger, 0});
    }
    summary.hasEffect = !summary.tokens.empty();
    return summary;
}

} // namespace jf
