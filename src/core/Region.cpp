#include "jf/core/Region.hpp"

#include <algorithm>

namespace jf {

namespace {

// docs/implementation_roadmap.md M1-E slice1: builds the common fields a
// StageDescriptor shares with GameData::StageContentData (the JSON-loadable
// Schema, jf/data/GameData.hpp) from `data/regions.json`'s Loader output.
// Callers still set whatever richer fields this Schema doesn't cover yet
// directly on the returned StageDescriptor (this is the "段階的に" part of
// the migration - see StageContentData's own comment for what's covered).
StageDescriptor stageDescriptorFromContent(const StageContentData& content) {
    StageDescriptor stage;
    stage.id = content.id;
    stage.terrainProfileId = content.terrainProfileId;
    stage.enemyRoster = content.enemyRoster;
    stage.victoryRewardRules = content.victoryRewardRules;
    stage.routeDiscoveries = content.routeDiscoveries;
    stage.surveyObjectiveId = content.surveyObjectiveId;
    stage.surveyTileCount = content.surveyTileCount;
    stage.surveyTileObjectDefinitionId = content.surveyTileObjectDefinitionId;
    stage.discoveries = content.discoveries;
    stage.missionNameEn = content.missionNameEn;
    stage.missionNameJa = content.missionNameJa;
    stage.routeOutcomes = content.routeOutcomes;
    stage.scoutRouteRequiredClass = content.scoutRouteRequiredClass;
    stage.scoutRouteDisabled = content.scoutRouteDisabled;
    if (content.timedReinforcement) {
        const auto& r = *content.timedReinforcement;
        stage.timedReinforcement = StageDescriptor::TimedReinforcement{
            r.id, r.spawnRound, r.spawnPhase, r.announceRoundsBefore,
            r.requiredForElimination, r.units, r.orderedSpawnCandidates};
    }
    if (content.herbPatchGeneration) {
        stage.herbPatchGeneration = StageDescriptor::HerbPatchGenerationRule{
            content.herbPatchGeneration->count, content.herbPatchGeneration->zoneMinCol,
            content.herbPatchGeneration->zoneMaxCol};
    }
    for (const auto& rule : content.objectPlacementRules) {
        stage.objectPlacementRules.push_back(StageDescriptor::ObjectPlacementRule{
            rule.definition, rule.idPrefix, rule.count, rule.scalesWithExtraBarrierOutcome, rule.zoneMinCol,
            rule.zoneMaxCol, rule.avoidFirstEnemyRow, rule.operateObjectiveId, rule.secondaryOperateObjectiveId});
    }
    stage.enemyCountOverride = content.enemyCountOverride;
    stage.enemyZoneWidth = content.enemyZoneWidth;
    if (content.boostedFirstEnemy) {
        stage.boostedFirstEnemy = StageDescriptor::BoostedEnemy{
            content.boostedFirstEnemy->displayName, content.boostedFirstEnemy->maxHpBonus,
            content.boostedFirstEnemy->defenseBonus, content.boostedFirstEnemy->strengthBonus};
    }
    stage.understaffedReinforcement = content.understaffedReinforcement;
    stage.understaffedThreshold = content.understaffedThreshold;
    stage.logCollisionBonusLoot = content.logCollisionBonusLoot;
    stage.noCasualtiesBonusLoot = content.noCasualtiesBonusLoot;
    if (content.primaryHoldTileAlternative) {
        const auto& r = *content.primaryHoldTileAlternative;
        stage.primaryHoldTileAlternative =
            StageDescriptor::HoldTileMissionRule{r.id, r.requiredHoldRounds, r.zoneMinCol, r.zoneMaxCol};
    }
    if (content.primarySecureTileAlternative) {
        const auto& r = *content.primarySecureTileAlternative;
        stage.primarySecureTileAlternative =
            StageDescriptor::HoldTileMissionRule{r.id, r.requiredHoldRounds, r.zoneMinCol, r.zoneMaxCol};
    }
    stage.primaryDefeatUnitId = content.primaryDefeatUnitId;
    if (content.primarySurviveRoundsAlternative) {
        const auto& r = *content.primarySurviveRoundsAlternative;
        stage.primarySurviveRoundsAlternative = StageDescriptor::SurviveRoundsMissionRule{r.id, r.surviveUntilRound};
    }
    return stage;
}

// docs/implementation_roadmap.md M6-A/B/C: docs/regions/cinderwatch_gate.md's
// full 6-site region, being migrated in from the old 3-battle placeholder
// one Slice at a time. So far: site 1 (シンダーウォッチ外門,
// cinderwatch_outer_gate), site 2 (灰道の監視所, ashroad_watch), site 3A
// (アイアンウォッチ物資庫, ironwatch_stores), site 4 (旧兵舎, old_barracks), and
// site 5 (信号塔下層, signal_tower) are real; the RouteGraph (RouteGraph.cpp)
// branches site 3's slot between ironwatch_stores and old_barracks per the
// doc's 3A/3B. `last_signal` (site 6, 最後の信号) is the OLD pre-spec
// placeholder content that used to stand in for sites 5+6 combined - kept
// under a new id after the M6-C item2 split so the region stays completable
// end-to-end until the next Slice replaces it with the real boss fight
// (`boostedFirstEnemy` Former Captain, `captains_seal`/`ashveil_fang`
// rewards, and the wood/hide balance top-up all carried over unchanged from
// the pre-split `signal_tower`). ironwatch_stores' real content (M6-C
// item 1) deliberately stops short of the design doc's 工作兵護衛/加入候補
// (needs a controllable-NPC-unit subsystem that doesn't exist anywhere in
// the codebase, plus M7項目5's Pending加入候補基盤), its class-gated 3rd
// exploration choice (`辺境工兵` isn't a real UnitClass yet -
// scoutRouteDisabled like site 1's own `[重装兵]`), and the "both crates
// opened within 2 rounds" reinforcement trigger (no state-conditioned
// reinforcement trigger exists, only choice-conditioned via
// ExplorationOutcome.enableReinforcementWave). signal_tower's real content
// (M6-C item2) similarly stops short of: the "敵全滅後に操作" route's
// 6-round time limit (no round-limit defeat condition exists anywhere in
// the engine or docs/mission_objectives.md's own data model), the axeman
// reinforcement's exact "after the first panel is operated" trigger
// (approximated as a fixed round 2, same shape as herbwater_hollow's),
// the class-gated 3rd exploration choice (same `辺境工兵` gap as
// ironwatch_stores), and the 軍旗記録 discovery (same "no recruit-candidate
// system to register it against yet" reasoning as ironwatch_stores' 野戦
// 工作記録). `enemyRoster` deliberately absent from last_signal - empty
// means "use GameData::enemyRoster", the shared roster it still draws
// from, per StageDescriptor's own top-of-file comment.
RegionDescriptor cinderwatchGateRegion(const GameData& data) {
    RegionDescriptor region;
    region.id = RegionId::CinderwatchGate;
    // docs/regions/cinderwatch_gate.md: "# 第2地域 沈黙した監視所群" / "日本語名
    // 「沈黙した監視所群」を表示する" - the region's own doc, not "Cinderwatch
    // Gate" (that's stage0's mission name, cinderwatch_outpost, not the
    // region's name).
    region.displayNameEn = "Silenced Watchpost Cluster";
    region.displayNameJa = "沈黙した監視所群";

    region.stages.push_back(stageDescriptorFromContent(data.stageContent("cinderwatch_outer_gate")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("ashroad_watch")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("ironwatch_stores")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("old_barracks")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("signal_tower")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("last_signal")));

    return region;
}

// docs/regions/ashbough_forest.md "1. 灰枝の林縁" - the only location
// implemented so far (docs/implementation_roadmap.md Phase 2 scope). Its 4
// wolves are a self-contained roster, not part of GameData::enemyRoster.
RegionDescriptor ashboughForestRegion(const GameData& data) {
    RegionDescriptor region;
    region.id = RegionId::AshboughForest;
    region.displayNameEn = "Ashbough Forest";
    region.displayNameJa = "灰枝の森";

    // docs/implementation_roadmap.md M1-E slice1: the first stage fully
    // sourced from `data/regions.json` rather than authored inline here -
    // every field it uses (roster, victory/route loot, survey bonus,
    // mission names) fits StageContentData's Schema.
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("ashbough_verge")));

    // docs/regions/ashbough_forest.md "2. 薬草の沢". Reinforcement (a 4th
    // wolf arriving turn 2 on the harvest route), the Dawn Chirurgeon-only
    // dedicated survey tile (`herbwater_hollow_surveyed` RegionProgress
    // record), and the post-harvest one-time +2 HP on continue are not yet
    // implemented. The harvest-route round-2 wolf reinforcement is wired
    // through StageDescriptor::timedReinforcement. The Chirurgeon-only tile
    // still needs a per-battle-instance required-unit-id. The main
    // objective, 3 exploration choices, terrain (Shallows + 2 HerbPatch),
    // and the common "薬草地点確保" Any-of-2-tiles bonus are implemented.
    // docs/implementation_roadmap.md M1-E slice1続き: fully sourced from
    // `data/regions.json` - the second stage migrated after Ashbough Verge,
    // proving the Schema extension (routeOutcomes/scoutRouteRequiredClass/
    // timedReinforcement/herbPatchGeneration) covers a stage this much
    // richer than Verge's.
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("herbwater_hollow")));

    // docs/regions/ashbough_forest.md "3. 折れ木の縄張り"/"灰角大猪". Route C
    // ("[辺境猟兵]獣の痕跡を追う") is out of scope per the doc's own text - it
    // needs 辺境猟兵, a post-clear recruit-only class that doesn't exist yet,
    // and the doc explicitly frames C as "初回攻略用ではなく再訪・再挑戦用の
    // 選択肢". The primary objective is the default EliminateTeam mission
    // The escort wolf remains active while the boar loses its own turn to a
    // fallen-log collision, preventing the stun window from becoming a fully
    // uncontested Enemy Phase.
    // docs/implementation_roadmap.md M1-E slice1続き: fully sourced from
    // `data/regions.json` - the richest stage migrated so far (roster,
    // route loot/outcomes, disabled scout route, objectPlacementRules,
    // understaffedReinforcement, both Ad-hoc bonus loot fields).
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("brokenwood_territory")));

    return region;
}

std::vector<StageDescriptor> ashboughForestDeepStagesImpl() {
    StageDescriptor trash1;
    trash1.id = "ashbough_deep_1";
    trash1.terrainProfileId = "brokenwood_territory";
    trash1.enemyRoster = {
        {"ashbough_deep_wolf1", "Deepwood Wolf", UnitClass::Wolf},
        {"ashbough_deep_wolf2", "Deepwood Wolf", UnitClass::Wolf},
        {"ashbough_deep_wolf3", "Deepwood Wolf", UnitClass::Wolf},
        {"ashbough_deep_wolf4", "Deepwood Wolf", UnitClass::Wolf},
    };
    trash1.victoryRewardRules = {
        {RewardRule::Condition::Always, {}, {{"ashbough_deep_core", 2}}},
    };
    trash1.missionNameEn = "Deep Layer - Wolf Pack";
    trash1.missionNameJa = "深層・狼の群れ";

    // 深層ボス1体目: 「灰角大猪の眷属・片牙」(AshenhornBoarのリスキン)。
    StageDescriptor boss1;
    boss1.id = "ashbough_deep_boss1";
    boss1.terrainProfileId = "brokenwood_territory";
    boss1.enemyRoster = {
        {"ashbough_deep_boar1", "Ashenhorn Kin, Tuskscar", UnitClass::AshenhornBoar},
        {"ashbough_deep_boss1_wolf", "Deepwood Wolf", UnitClass::Wolf},
    };
    boss1.victoryRewardRules = {
        {RewardRule::Condition::Always, {}, {{"ashbough_deep_core", 3}, {"ashenhorn_deep_tusk", 1}}},
    };
    boss1.missionNameEn = "Deep Layer - Tuskscar";
    boss1.missionNameJa = "深層・灰角大猪の眷属「片牙」";

    StageDescriptor trash2;
    trash2.id = "ashbough_deep_2";
    trash2.terrainProfileId = "brokenwood_territory";
    trash2.enemyRoster = {
        {"ashbough_deep_wolf5", "Deepwood Wolf", UnitClass::Wolf},
        {"ashbough_deep_wolf6", "Deepwood Wolf", UnitClass::Wolf},
        {"ashbough_deep_wolf7", "Deepwood Wolf", UnitClass::Wolf},
        {"ashbough_deep_wolf8", "Deepwood Wolf", UnitClass::Wolf},
    };
    trash2.victoryRewardRules = {
        {RewardRule::Condition::Always, {}, {{"ashbough_deep_core", 3}}},
    };
    trash2.missionNameEn = "Deep Layer - Wolf Pack II";
    trash2.missionNameJa = "深層・狼の群れ(二陣)";

    // 深層ボス2体目(=深層完了): 「灰角大猪の頭目・森ノ主」(同じくリスキン)。
    StageDescriptor boss2;
    boss2.id = "ashbough_deep_boss2";
    boss2.terrainProfileId = "brokenwood_territory";
    boss2.enemyRoster = {
        {"ashbough_deep_boar2", "Ashenhorn Chief, Forestwarden", UnitClass::AshenhornBoar},
        {"ashbough_deep_boss2_wolf", "Deepwood Wolf", UnitClass::Wolf},
    };
    boss2.victoryRewardRules = {
        {RewardRule::Condition::Always, {}, {{"ashbough_deep_core", 4}, {"ashenhorn_deep_horn", 1}}},
    };
    boss2.missionNameEn = "Deep Layer - Forestwarden";
    boss2.missionNameJa = "深層・灰角大猪の頭目「森ノ主」";

    return {trash1, boss1, trash2, boss2};
}

std::vector<StageDescriptor> ashboughForestDeepestStagesImpl() {
    StageDescriptor trash;
    trash.id = "ashbough_deepest_1";
    trash.terrainProfileId = "brokenwood_territory";
    trash.enemyRoster = {
        {"ashbough_deepest_wolf1", "Abyssal Wolf", UnitClass::Wolf},
        {"ashbough_deepest_wolf2", "Abyssal Wolf", UnitClass::Wolf},
        {"ashbough_deepest_wolf3", "Abyssal Wolf", UnitClass::Wolf},
        {"ashbough_deepest_wolf4", "Abyssal Wolf", UnitClass::Wolf},
    };
    trash.victoryRewardRules = {
        {RewardRule::Condition::Always, {}, {{"ashbough_deep_core", 4}}},
    };
    trash.missionNameEn = "Deepest Layer - Wolf Pack";
    trash.missionNameJa = "最深層・狼の群れ";

    // 最深層ボス(計3体目、最終): 「灰角大猪の古老・朽木の王」。docs/deep_layers.md
    // は最深層ボスにだけ`chargeTelegraphed`/`bossRuntime`の固有ギミックを1つ
    // 追加してよいとしているが、既存AIは`UnitClass`単位のディスパッチ
    // (EnemyAI.cpp `takeBoarBossTurn`)で、同じクラスの個体ごとに行動を
    // 分岐させる仕組みが今のところ無い。このプロトタイプでは既存の
    // `takeBoarBossTurn`をそのまま再利用し(ステータスの強化のみ)、固有
    // ギミックの追加は個体差分の仕組みが要る別Sliceの課題として残す。
    StageDescriptor boss;
    boss.id = "ashbough_deepest_boss";
    boss.terrainProfileId = "brokenwood_territory";
    boss.enemyRoster = {
        {"ashbough_deepest_boar", "Ashenhorn Elder, Rotwood King", UnitClass::AshenhornBoar},
    };
    boss.victoryRewardRules = {
        {RewardRule::Condition::Always, {}, {{"ashbough_deep_core", 5}, {"ashenhorn_deep_relic", 1}}},
    };
    boss.missionNameEn = "Deepest Layer - Rotwood King";
    boss.missionNameJa = "最深層・灰角大猪の古老「朽木の王」";

    return {trash, boss};
}

// docs/implementation_roadmap.md M6-D: minimal placeholder for the 3rd
// region, unlocked once CinderwatchGate completes (docs/regions/
// cinderwatch_gate.md「地域攻略結果」/「安全帰還時」の「灰鉄採石場を遠征先へ
// 追加」). Same role M6-A's original `cinderwatch_outpost` played before
// M6-A/B/C fleshed Cinderwatch out - a single stage just enough to make the
// region selectable and completable. The real 5-site content is M9's scope
// (docs/implementation_status.md「次地域(灰鉄採石場、5地点)」).
// docs/regions/ashiron_quarry.md「3A. 旧採掘坑」: deferred all the way back
// at M9-A/M9-D because its primary objective ("作業員1人以上を脱出させる")
// needs the guest-escort subsystem (StageDescriptor::guestUnits/
// primaryEscapeUnitsAlternative), which M9-I built for Blackwater Crossing.
// Hand-built here exactly like blackwaterCrossingStage()/windscar's guest
// sites, for the same reason (guestUnits isn't exposed to
// data/regions.json's Schema) - the old `quarry_old_mine` JSON entry
// (labelled "旧採掘坑(仮実装)") is left in place as dead data, same
// precedent as blackwater_crossing's own dead JSON entry.
//
// Deliberately NOT implemented (documented gap, established convention):
// - Route 2「支柱を先に確認する」's 作業員1人(vs 2) and route 3
//   `[古参守備兵]`「退路を封鎖する」's 増援なし: guestUnits/timedReinforcement
//   are both fixed at scenario-build time, before the chosen
//   ExplorationChoice's outcome could vary them (same "guestUnits fixed
//   across all routes" limit M9-I/-O/-P already recorded) - all 3 routes
//   spawn the same 2 workers and the same round-2 reinforcement.
// - Route 2's「崩落予告を常時表示」: no CollapseWarning-style terrain kind
//   exists anywhere in the engine (M9-B's own "落石予告" gap, still open) -
//   a no-op regardless of route.
// - Route 3's「味方初期配置は左2列」: DOES have a real mechanism -
//   ExplorationOutcome::restrictedAutoSpawnMaxColumn (the exact field
//   Herbwater Hollow's 衛生兵 route already proved) - wired below.
// - 副目標「坑道支柱2本のうち1本以上を保全」and the`quarry_hoist`-style
//   `重装加工記録`-style pillar-durability reward: Object-durability
//   mechanics don't exist (M6-C/M9-C/M9-D's same known gap).
StageDescriptor quarryOldMineStage() {
    StageDescriptor stage;
    stage.id = "quarry_old_mine";
    stage.terrainProfileId = "ash_road";
    stage.enemyRoster = {
        // 穿岩獣 = Bandit reuse, "Rock Borer" display name - established
        // since M9-D's Collapse Core roster.
        {"quarry_old_mine_borer1", "Rock Borer", UnitClass::Bandit},
        {"quarry_old_mine_borer2", "Rock Borer", UnitClass::Bandit},
        {"quarry_old_mine_borer3", "Rock Borer", UnitClass::Bandit},
        {"quarry_old_mine_archer1", "Salvage Archer", UnitClass::WatchArcher},
    };
    stage.routeOutcomes = {
        // 「作業員の声を追う」: no condition, standard 4 enemies.
        {ExplorationChoice::FrontalAdvance, ExplorationOutcome{}},
        // 「支柱を先に確認する」: no condition. Both the guest-count
        // difference and the collapse-telegraph effect are the documented
        // no-ops above.
        {ExplorationChoice::CollapsedSidePath, ExplorationOutcome{}},
        // `[古参守備兵]`「退路を封鎖する」: restrictedAutoSpawnMaxColumn=1
        // confines the party's auto-placement to the left 2 columns
        // (col 0-1). 増援なし is the documented no-op above (reinforcement
        // still spawns on this route too).
        {ExplorationChoice::ScoutRoute, ExplorationOutcome{.restrictedAutoSpawnMaxColumn = 1}},
    };
    stage.scoutRouteRequiredClass = UnitClass::VeteranGuard;

    // 2ラウンド目に穿岩獣1体の予告増援 - same TimedReinforcement shape as
    // blackwater_crossing's snake wave.
    stage.timedReinforcement = StageDescriptor::TimedReinforcement{
        "quarry_old_mine_borer_wave",
        /*spawnRound=*/2,
        Phase::EnemyPhase,
        /*announceRoundsBefore=*/1,
        /*requiredForElimination=*/false,
        {{"quarry_old_mine_borer_reinforcement", "Rock Borer", UnitClass::Bandit}},
        {GridPos{0, kGridCols - 1}, GridPos{1, kGridCols - 1}, GridPos{2, kGridCols - 1}},
    };

    // 作業員2人 - non-combatant guest escort targets, same DawnChirurgeon
    // reuse (lowest STR of any class) as blackwater_crossing's porters.
    stage.guestUnits = {
        {{"quarry_old_mine_worker1", "Mine Worker", UnitClass::DawnChirurgeon}, GridPos{0, 3}},
        {{"quarry_old_mine_worker2", "Mine Worker", UnitClass::DawnChirurgeon}, GridPos{2, 3}},
    };

    // 主目的: 作業員1人以上を脱出させる - same shape as blackwater_crossing's
    // primary, right-edge exit (doc doesn't call out a side for this site,
    // unlike site 4's explicit "左側退路").
    stage.primaryEscapeUnitsAlternative =
        StageDescriptor::PrimaryEscapeUnitsRule{"quarry_old_mine_escape", /*requiredEscapeCount=*/1,
                                                /*zoneMinCol=*/kGridCols - 1, /*zoneMaxCol=*/kGridCols - 1};

    stage.victoryRewardRules = {
        {RewardRule::Condition::Always, {}, {{"wood", 2}, {"stone", 1}}},
    };

    // 副目標「作業員2人とも脱出」→ 採掘技術記録: creditedTargetIds.size()>=2,
    // same ad-hoc check GameApp::proceedToCamp() already uses for
    // blackwater_crossing's "2人とも脱出" bonus (RewardRule::Condition has
    // no shape for crediting-count).
    // 敗北条件「全作業員の撤退」: allGuestsLost(), zero new code (automatic
    // via BattleFactory registering guestUnits into missionState().guestUnitIds).

    stage.missionNameEn = "Old Mine Shaft";
    stage.missionNameJa = "旧採掘坑";
    return stage;
}

// docs/regions/ashiron_quarry.md「4. 灰鉄鉱脈」: deferred at M9-D for the
// same guest-escort reason as site 3A (イリエンを操作可能なゲストユニットと
// して扱う). Hand-built here for the same JSON-Schema-gap reason as
// quarryOldMineStage()/blackwaterCrossingStage() - the old `ashiron_vein`
// JSON entry ("灰鉄鉱脈(仮実装)") is left in place as dead data.
//
// **Primary objective approximation**: the doc's primary is an AND of
// OperateObject(N measurement points, N=1/2/3 by route) and
// EscapeUnits(Irien to the left exit) - the same "mixed-Kind AND
// composition has no generic infra" gap M9-D/-J/-M/-O/-Q have all
// deferred. Per the established precedent of picking ONE Kind as the
// actual primary (M9-Q's site 6), this stage uses
// primaryEscapeUnitsAlternative (Irien reaching a LEFT-column zone -
// PrimaryEscapeUnitsRule::zoneMinCol/zoneMaxCol already support any
// column range, just never used for the left edge before) as the real
// primary, since "イリエンを操作可能なゲストユニットとして扱う" is called
// out as this site's defining new mechanic. The OperateObject/
// measurement-count mechanic (routes needing 1/2/3 operated tiles) is
// entirely deferred - no objectPlacementRules are declared for it.
//
// **副目標「イリエンを撤退させない」(ObjectiveKind::ProtectUnit)**: NOT
// wired through the real ProtectUnit machinery. Generating a ProtectUnit
// Objective for a single guest would need new BattleFactory plumbing (no
// StageDescriptor field routes a guest into a ProtectUnit definition
// today), and here it would be redundant anyway - Irien is also the
// primaryEscapeUnitsAlternative target, so any Victory on this stage
// already implies she's alive and present. Approximated instead with the
// same ad-hoc isPresent()-in-GameApp::proceedToCamp() check every other
// Kind-mismatch secondary bonus already uses (blackwater_crossing's "2人
// とも脱出", deep_mire's "薬草地点未使用") - see GameApp.cpp. This means
// ObjectiveKind::ProtectUnit's reward-consumption gap (Objective.hpp's own
// comment) is still open; this Slice did not close it.
//
// **鉱石箱1個を確保**: reuses surveyObjectiveId + surveyTileCount:1 +
// surveyTileObjectDefinitionId, the exact same secondary-alongside-a-
// different-primary pattern blackwater_crossing already proved (that
// stage's crate secondary coexists with its own EscapeUnits primary too).
//
// **Route 3 `[戦闘魔導士候補]`**: gated on "地点3Aまたは3B確保", a
// stage-completion gate no mechanism in this codebase can express
// (scoutRouteRequiredClass only checks current party composition, and the
// class this route needs - 戦闘魔導士 - isn't recruitable before this very
// site grants it). Disabled via scoutRouteDisabled, the exact same
// resolution M9-D used for this site's own sibling site 5 route 3
// (`[戦闘魔導士]`, gated on "イリエン加入候補確定").
StageDescriptor ashironVeinStage() {
    StageDescriptor stage;
    stage.id = "ashiron_vein";
    stage.terrainProfileId = "ash_road";
    stage.enemyRoster = {
        {"ashiron_vein_borer1", "Rock Borer", UnitClass::Bandit},
        {"ashiron_vein_borer2", "Rock Borer", UnitClass::Bandit},
        {"ashiron_vein_borer3", "Rock Borer", UnitClass::Bandit},
        {"ashiron_vein_borer4", "Rock Borer", UnitClass::Bandit},
        // 大型穿岩獣 - reuses the base Rock Borer class/stats (no new stat
        // variant), name-only reskin, same "Rock Borer = Bandit" precedent.
        // Present by default; routes 1/3 subtract it via enemiesRemoved=1
        // (the established "roster includes the max, routes subtract" trick
        // M9-B/-O use), route 2 (異常反応) keeps all 5.
        {"ashiron_vein_borer_large", "Large Rock Borer", UnitClass::Bandit},
    };
    stage.routeOutcomes = {
        // 「通常鉱脈だけを採る」: measurement count (1) is part of the
        // deferred OperateObject mechanic above; enemies 4 (large borer
        // removed).
        {ExplorationChoice::FrontalAdvance, ExplorationOutcome{.enemiesRemoved = 1}},
        // 「異常反応を調べる」: measurement count (3, deferred) + 敵5体
        // (large borer stays) + 崩落予告 (no CollapseWarning terrain kind
        // exists - same no-op as quarryOldMineStage()).
        {ExplorationChoice::CollapsedSidePath, ExplorationOutcome{}},
        // `[戦闘魔導士候補]` route disabled below (scoutRouteDisabled) - kept
        // here only so routeOutcomes stays exhaustive over all 3 choices.
        {ExplorationChoice::ScoutRoute, ExplorationOutcome{.enemiesRemoved = 1}},
    };
    stage.scoutRouteDisabled = true;

    // イリエン・ヴェイルリーチ - the operable guest unit this site's doc
    // text calls its defining mechanic. BattleMage (her actual eventual
    // class, mage_recruit in data/units.json) rather than a stat-only
    // reskin, since she's also this site's recruit-candidate reward.
    stage.guestUnits = {
        {{"ashiron_vein_irien", "Irien", UnitClass::BattleMage}, GridPos{1, 3}},
    };

    // 主目的: 指定測定地点をすべて操作し、左側退路へ戻る - approximated as
    // Irien-escape-to-the-left-column alone (see comment above). Uses the
    // full left spawn zone (col 0-2, kLeftZoneMinCol/kLeftZoneMaxCol in
    // BattleFactory.cpp) rather than a single column: chooseHoldTile()
    // picks the tile AFTER the party/guest already occupy their spawn
      // tiles there, so a single-column zone (col 0 only) is often fully
    // saturated by spawned units and silently falls back to the opposite
    // (right) edge - widening to 3 columns leaves enough free tiles in
    // practice. Still genuinely the left side, unlike every prior guest
    // site's right-edge default.
    stage.primaryEscapeUnitsAlternative = StageDescriptor::PrimaryEscapeUnitsRule{
        "ashiron_vein_escape", /*requiredEscapeCount=*/1, /*zoneMinCol=*/0, /*zoneMaxCol=*/2};

    // 副目標「鉱石箱1個を確保」: same Any-of-N SecureTile mechanism as
    // blackwater_crossing's own crate secondary.
    stage.surveyObjectiveId = "ashiron_vein_crate";
    stage.surveyTileCount = 1;
    stage.surveyTileObjectDefinitionId = "ashiron_vein_crate_marker";

    stage.victoryRewardRules = {
        // docs/implementation_status.md「素材システム全面再設計」#1続き:
        // veinstone_powder added alongside the existing iron/stone.
        {RewardRule::Condition::Always, {}, {{"iron", 2}, {"stone", 1}, {"veinstone_powder", 1}}},
        // 通常鉱脈ルート: 鉄鉱石+1.
        {RewardRule::Condition::RouteChoice, ExplorationChoice::FrontalAdvance, {{"iron", 1}}},
        // 鉱石箱確保: 高品質鉄材1。灰鉄芯(レア素材)も同じ特殊採取条件に接続
        // (docs/implementation_status.md「素材システム全面再設計」次の方針メモ)。
        {RewardRule::Condition::SurveySuccess, {}, {{"quality_iron", 1}, {"grayiron_core", 1}}},
    };

    // 「測定完了かつイリエン生存: 異常鉱脈記録、mage_recruit」: measurement
    // completion is folded into Victory itself (see the deferred-
    // OperateObject note above); Irien's survival is checked ad-hoc in
    // GameApp::proceedToCamp() (see that file's own comment) rather than
    // through RewardRule, same pattern as quarry_old_mine's "2人とも脱出".
    // 敗北条件「全測定器破壊」: Object-durability gap, deferred (M6-C/M9-C/
    // M9-D's same known gap).

    stage.missionNameEn = "Ashiron Vein";
    stage.missionNameJa = "灰鉄鉱脈";
    return stage;
}

RegionDescriptor ashironQuarryRegion(const GameData& data) {
    RegionDescriptor region;
    region.id = RegionId::AshironQuarry;
    region.displayNameEn = "Ashiron Quarry";
    region.displayNameJa = "灰鉄採石場";

    region.stages.push_back(stageDescriptorFromContent(data.stageContent("quarry_entrance")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("quarry_terrace")));
    region.stages.push_back(quarryOldMineStage());
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("quarry_hoist_works")));
    region.stages.push_back(ashironVeinStage());
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("quarry_collapse_core")));

    return region;
}

// docs/regions/blackwater_lowlands.md「5. 黒水渡し」: unlike every other
// stage in this file, this one's primary objective genuinely needs the
// guest-escort subsystem (StageDescriptor::guestUnits /
// primaryEscapeUnitsAlternative), neither of which `StageContentData`/
// `data/regions.json` expose (docs' M9-I decision: extend the C++ Schema
// directly rather than growing the JSON Schema for a single stage - see
// StageDescriptor::guestUnits' own comment). So this stage is hand-built
// here instead of going through stageDescriptorFromContent() +
// data/regions.json like every sibling site. The old `blackwater_crossing`
// JSON entry (`data/regions.json`) is now dead data, left in place
// unreferenced rather than deleted (deleting isn't required for
// correctness and risks an unrelated Loader regression for zero benefit).
//
// Reuses established regional precedent throughout: 毒蜘蛛=Wolf and
// 沼蛇=Bandit (matching Marsh Viper's own Bandit reuse from sites 2-4) for
// enemy stats/name-only reskins; DawnChirurgeon for the two 荷運び役
// (non-combatant escort targets - lowest STR of any class, cf. "Rock
// Borer = Bandit reuse" naming-only precedent); surveyObjectiveId +
// surveyTileCount:1 for "荷物箱を保持" (same Any-of-N SecureTile mechanism
// reedway_fork/herb_islet already use, just N=1); a RewardRule with
// Condition::SurveySuccess for the crate's 毒素材1 (no new GameApp code
// needed for that one - see computeStageVictoryLoot()); scoutRouteRequiredClass
// = MessengerCavalry for the `[伝令騎兵]` 3rd route.
//
// Deliberately NOT implemented (documented gap, same convention M9-D/-G/-H
// used rather than half-building new infra for a single stage):
// - 「荷物を減らして渡る」's 持込品1個を一時封印 (no persistent per-battle
//   item-seal/unseal infrastructure exists anywhere in the codebase).
// - `[伝令騎兵]` route's 護衛対象MOV+1 and 増援位置公開 (guestUnits/
//   timedReinforcement are fixed at scenario-build time, before the chosen
//   ExplorationChoice's ExplorationOutcome could conditionally alter a
//   specific guest's stats or reveal a spawn tile; no such
//   outcome-to-guest-stat or spawn-tile-reveal wiring exists).
StageDescriptor blackwaterCrossingStage() {
    StageDescriptor stage;
    stage.id = "blackwater_crossing";
    stage.terrainProfileId = "ash_road";
    stage.enemyRoster = {
        {"blackwater_crossing_snake1", "Swamp Snake", UnitClass::Bandit},
        {"blackwater_crossing_snake2", "Swamp Snake", UnitClass::Bandit},
        {"blackwater_crossing_snake3", "Swamp Snake", UnitClass::Bandit},
        {"blackwater_crossing_spider1", "Marsh Poison Spider", UnitClass::Wolf},
        {"blackwater_crossing_spider2", "Marsh Poison Spider", UnitClass::Wolf},
    };
    stage.routeOutcomes = {
        // 「浅瀬を一列ずつ渡る」: no condition, standard escort, enemies 5 (base roster).
        {ExplorationChoice::FrontalAdvance, ExplorationOutcome{.enableReinforcementWave = true}},
        // 「荷物を減らして渡る」: no condition, one fewer 沼蛇 (敵4体), no MOV penalty.
        {ExplorationChoice::CollapsedSidePath,
         ExplorationOutcome{.enemiesRemoved = 1, .enableReinforcementWave = true}},
        // `[伝令騎兵]` 「対岸へ縄を渡す」: enemy count unchanged; MOV+1/増援位置公開 are
        // the documented gaps above.
        {ExplorationChoice::ScoutRoute, ExplorationOutcome{.enableReinforcementWave = true}},
    };
    stage.scoutRouteRequiredClass = UnitClass::MessengerCavalry;

    // docs' "敵: 沼蛇3、毒蜘蛛2。3ラウンド目に沼蛇1体増援".
    stage.timedReinforcement = StageDescriptor::TimedReinforcement{
        "blackwater_crossing_snake_wave",
        /*spawnRound=*/3,
        Phase::EnemyPhase,
        /*announceRoundsBefore=*/1,
        /*requiredForElimination=*/false,
        {{"blackwater_crossing_snake_reinforcement", "Swamp Snake", UnitClass::Bandit}},
        {GridPos{0, kGridCols - 1}, GridPos{1, kGridCols - 1}, GridPos{2, kGridCols - 1}},
    };

    // 荷運び役2人 - non-combatant escort targets. DawnChirurgeon has the
    // lowest STR (2) of any implemented class, reused purely for stats/
    // display-name (cf. Ashiron Quarry's "Rock Borer = Bandit reuse"
    // precedent), not its Skill kit.
    stage.guestUnits = {
        {{"blackwater_crossing_porter1", "Porter", UnitClass::DawnChirurgeon}, GridPos{0, 3}},
        {{"blackwater_crossing_porter2", "Porter", UnitClass::DawnChirurgeon}, GridPos{2, 3}},
    };

    // 主目的: 荷運び役2人のうち1人以上を右端へ脱出 (replaces the default
    // EliminateTeam primary member entirely - see
    // StageDescriptor::primaryEscapeUnitsAlternative's own comment).
    stage.primaryEscapeUnitsAlternative =
        StageDescriptor::PrimaryEscapeUnitsRule{"blackwater_crossing_escape", /*requiredEscapeCount=*/1,
                                                /*zoneMinCol=*/kGridCols - 1, /*zoneMaxCol=*/kGridCols - 1};

    // 副目標「荷物箱を保持」: same Any-of-N SecureTile mechanism as
    // reedway_fork/herb_islet's own surveyObjectiveId secondaries, N=1 here.
    stage.surveyObjectiveId = "blackwater_crossing_crate";
    stage.surveyTileCount = 1;
    stage.surveyTileObjectDefinitionId = "blackwater_crossing_crate_marker";

    // 勝利: 薬草1、湿地樹脂1. 荷物箱保持: 毒素材1 (SurveySuccess RewardRule -
    // no extra GameApp.cpp code needed for this one, unlike the "2人とも
    // 脱出" bonus below which needs creditedTargetIds.size()>=2, a check
    // RewardRule's Condition enum has no shape for).
    stage.victoryRewardRules = {
        // docs/implementation_status.md「素材システム全面再設計」#1続き:
        // rot_reed_fiber added alongside the existing herb/wetland_resin.
        {RewardRule::Condition::Always, {}, {{"herb", 1}, {"wetland_resin", 1}, {"rot_reed_fiber", 1}}},
        {RewardRule::Condition::SurveySuccess, {}, {{"poison_material", 1}}},
    };

    // 敗北条件「部隊全滅」は既存allPlayersDefeated()のまま。「荷運び役2人の撤退」は
    // BattleFactory.cpp がstage.guestUnitsのidをmissionState().guestUnitIdsへ登録
    // することで自動的にallGuestsLost()経由で配線される(ここでの追加配線は不要)。

    // 恒久成果`blackwater_crossing_secured`: 他の全地点と同じ既存の一般機構
    // (勝利+安全帰還でsiteAccess::Secured、docs/implementation_status.mdの
    // Signal Tower/Hoist Worksと同じ扱い)がそのまま処理する。「地点5を安全通過」
    // という戦闘スキップ効果自体は他の全地点(沈み道標識等)と同じく未実装の
    // ドキュメント済みギャップ。

    stage.missionNameEn = "Blackwater Crossing";
    stage.missionNameJa = "黒水渡し";
    return stage;
}

// docs/regions/blackwater_lowlands.md「地点構成」: region skeleton, same role
// M9-A's ashironQuarryRegion() played for Ashiron Quarry - sites 1-6
// (sunken_path 〜 sunken_sluice) are real content as of M9-J; site 7
// (deep_mire) remains a placeholder.
//
// docs/regions/blackwater_lowlands.md「6. 沈没水門」(M9-J): unlike site 5,
// this stage's primary objective fits entirely within the existing
// `objectPlacementRules`/`operateObjectiveId` Schema signal_tower already
// proved (`data/regions.json`'s "sunken_sluice" entry), so it goes through
// stageDescriptorFromContent() + JSON like every sibling site except site 5.
// A single "sluice_gate_wheel" Device Object replaces the default
// EliminateTeam primary member with a lone OperateObject objective, same
// "replace, not widen" pattern signal_tower's 2-panel version uses (here
// just 1 Object instead of 2 - see BattleFactory.cpp's own comment on that
// block). 罠師=Bandit・弓兵=WatchArcher・毒蜘蛛=Wolf reskins follow the same
// established regional reuse as every other Blackwater site's roster.
//
// Deliberately NOT implemented (documented gap, same convention as
// M9-D/-G/-H/-I):
// - The doc's per-route interaction counts (操作2回 vs 操作1回) can't be
//   distinguished: `ObjectiveKind::OperateObject`'s Live-evaluation is a
//   fixed `interactionCount > 0` check with no configurable threshold
//   (docs/implementation_status.md:63), so all 3 routes share this single
//   Object and the differing counts are flavor-only, not enforced.
// - Route 2's "次Roundに敵味方の浅瀬4マスが深泥化" (no mid-battle terrain
//   mutation mechanism exists anywhere, same category as site 2's own
//   deferred 地形上書き) and route 3's `[辺境工兵]` 工具部品1消費 (no
//   consumable-item-cost-for-route mechanism exists) - both no-op, the 3
//   routes end up functionally identical besides route 3's class gate.
// - The primary's AND-combination with "2ラウンド防衛": no existing
//   StageDescriptor field ANDs a second Kind into "primary" alongside an
//   OperateObject replacement - `primarySurviveRoundsAlternative` always
//   widens the group to Any (OR) and re-adds the default EliminateTeam
//   member (BattleFactory.cpp), which is the wrong shape here (operating
//   the gate alone would win instantly, ignoring the enemies still on
//   board). Approximated as OperateObject-only, same "approximate, document
//   the gap" convention as the round-limit gaps above.
// - Secondaries "制御輪2個を保全" and "毒罠3個を処理" (no Object-destruction/
//   trap-processing infra, same as M9-H site 4's identical-shape gaps - no
//   trap Objects placed, and their tied discoveries `薬学記録`/`罠技術記録`
//   are correspondingly not granted here).
// - Lose condition "水門本体の耐久0" (Object-destruction-driven defeat still
//   doesn't exist, the same known M6-C/M9-C/M9-D gap).
// - 恒久成果`sunken_sluice_restored`: same generic siteAccess::Secured
//   mechanism every other site uses (see blackwaterCrossingStage()'s own
//   comment on this) - no new plumbing needed.
RegionDescriptor blackwaterLowlandsRegion(const GameData& data) {
    RegionDescriptor region;
    region.id = RegionId::BlackwaterLowlands;
    region.displayNameEn = "Blackwater Lowlands";
    region.displayNameJa = "黒水低湿地";

    region.stages.push_back(stageDescriptorFromContent(data.stageContent("sunken_path")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("reedway_fork")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("herb_islet")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("resin_grove")));
    region.stages.push_back(blackwaterCrossingStage());
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("sunken_sluice")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("deep_mire")));

    return region;
}

// docs/regions/windscar_plateau.md「1. 風下の登り口」: hand-crafted (not
// stageDescriptorFromContent()+JSON) because it needs the new
// StageDescriptor::windGust field the generic JSON Schema doesn't cover yet
// - same reason blackwaterCrossingStage() above is hand-crafted for its own
// StageDescriptor-only fields (guestUnits/primaryEscapeUnitsAlternative).
//
// 高原運び手 (この地域の敵勢力) gets no new UnitClass for this minimal
// 4-enemy roster - same "reskin an existing class's stats under the
// faction's display name" precedent as Blackwater's 湿地の毒蜘蛛=Wolf/
// Ashiron's Rock Borer=Bandit. Bandit is the closest existing stat block to
// an unarmored raider-militia fighter; a real 騎兵/弓兵/槍兵 breakdown per
// docs' 敵勢力 section is deferred to whichever later site's objective
// actually depends on the distinction (site 1's primary is plain
// EliminateTeam against a same-stat foursome).
//
// Deliberately NOT implemented (documented gap, same convention as prior
// Slices):
// - Route 2's "織物-1" (a route-triggered consumable cost, not a reward
//   delta - base victory loot has no cloth to subtract from). No
//   consumable-item-cost-for-route mechanism exists anywhere in the engine;
//   M9-K's own comment on Blackwater's site 7 routes documents this exact
//   gap already, no-op here for the same reason.
// - Route 3's "地形全公開" - the engine has no fog-of-war/hidden-terrain
//   system at all (every battle's terrain is always fully visible), so this
//   is already true unconditionally; no-op.
// - Route 3's "強風帯2マス減少" - no per-route terrain-generation-override
//   mechanism exists (same shape as M9-F's deferred per-route terrain
//   overwrite); all 3 routes share one `windscar_ascent` TerrainProfile and
//   therefore the same WindGust tile count.
// - "標識確保: 高原踏査進行" - not in the doc's own 安定ID table (unlike
//   `windscar_ascent_marked` right below it), reads as a narrative label for
//   the secondary rather than a mechanical reward/Discovery; no loot/
//   Discovery attached to SurveySuccess here.
// - 恒久成果`windscar_ascent_marked`: same generic siteAccess::Secured
//   mechanism every other site's permanent outcome uses (see
//   blackwaterCrossingStage()'s own comment on this) - no new plumbing.
StageDescriptor windscarAscentStage() {
    StageDescriptor stage;
    stage.id = "windscar_ascent";
    stage.terrainProfileId = "windscar_ascent";
    stage.enemyRoster = {
        {"windscar_ascent_runner1", "Plateau Runner", UnitClass::Bandit},
        {"windscar_ascent_runner2", "Plateau Runner", UnitClass::Bandit},
        {"windscar_ascent_runner3", "Plateau Runner", UnitClass::Bandit},
        {"windscar_ascent_runner4", "Plateau Runner", UnitClass::Bandit},
    };
    stage.routeOutcomes = {
        // 「風が弱まるのを待つ」: no condition, standard 4-enemy roster.
        {ExplorationChoice::FrontalAdvance, ExplorationOutcome{}},
        // 「荷物を分けて登る」: 2組の分散配置 has no existing engine
        // mechanism (docs/regions/windscar_plateau.md「実装順」item2, a
        // future Slice) - approximated as the documented enemy-count
        // reduction only, same "approximate with what routeOutcomes can
        // express, document the rest" convention as every prior region.
        {ExplorationChoice::CollapsedSidePath, ExplorationOutcome{.enemiesRemoved = 1}},
        // `[辺境斥候]` 「風裏を先行する」: enemy count unchanged; both of this
        // route's documented effects are no-ops per the gaps above.
        {ExplorationChoice::ScoutRoute, ExplorationOutcome{}},
    };
    stage.scoutRouteRequiredClass = UnitClass::FrontierScout;

    // 主目的: 高原運び手4体を退ける - default EliminateTeam against this
    // stage's own enemyRoster (StageDescriptor's existing default primary),
    // matching the doc's explicit "この地点自身の4体" framing rather than
    // any region-wide shared roster.

    // 副目標「登り口標識で行動終了」: bare single-tile surveyObjectiveId,
    // same shape as sunken_path_marker/ashroad_watch_fixture (no
    // surveyTileCount/surveyTileObjectDefinitionId).
    stage.surveyObjectiveId = "windscar_ascent_marker";

    // 勝利: 獣皮2、硬木1. 斥候ルート: 織物1.
    stage.victoryRewardRules = {
        // docs/implementation_status.md「素材システム全面再設計」#1続き:
        // windcleft_feather added alongside the existing hide/hardwood.
        {RewardRule::Condition::Always, {}, {{"hide", 2}, {"hardwood", 1}, {"windcleft_feather", 1}}},
        {RewardRule::Condition::RouteChoice, ExplorationChoice::ScoutRoute, {{"cloth", 1}}},
    };

    // docs/regions/windscar_plateau.md「強風ルール」: downward push (row+1,
    // the board's short axis - 3 rows), triggering at Round 3 per route 1's
    // "強風は3Round目". Applied to all 3 routes (see the per-route terrain
    // gap noted above).
    stage.windGust = StageDescriptor::WindGustRule{GridPos{1, 0}, /*triggerRound=*/3};

    stage.missionNameEn = "Downwind Ascent";
    stage.missionNameJa = "風下の登り口";
    return stage;
}

// docs/regions/windscar_plateau.md「2. 崩れた中継路」: hand-crafted (not
// stageDescriptorFromContent()+JSON) for the same reason blackwaterCrossingStage()
// is - this stage needs StageDescriptor::guestUnits/primaryEscapeUnitsAlternative,
// which the generic JSON Schema doesn't expose (M9-I's decision, see that
// stage's own comment).
//
// 敵は「槍兵2、弓兵2」= this region's own 高原運び手 roster, but unlike site 1's
// 4-Bandit "Plateau Runner" reskin, this doc explicitly names two distinct
// combat roles ("槍兵は狭い橋と登り道を封鎖する"/"弓兵は稜線を優先し...") that
// already have real matching UnitClasses (Spearman/WatchArcher, both already in
// loadAppFont()'s UnitClass toString() list) - so this stage uses them directly,
// undisguised (no reskin display name), rather than reusing Bandit like site 1.
//
// 主目的「護衛対象を右端へ脱出、または敵全滅後に橋を操作」: an OR between two
// different Objective Kinds (EscapeUnits vs EliminateTeam+OperateObject), which
// no existing StageDescriptor field composes (same "no AND/OR-of-mixed-Kinds
// infra for one site" reasoning as M9-D's boss AND-combination and M9-J site 6's
// AND-with-SurviveRounds gap). Approximated as primaryEscapeUnitsAlternative
// (EscapeUnits) only, since route 1 ("吊り橋を一人ずつ渡る", the doc's only route
// that actually mentions 護衛対象) is the most explicit "protect a person" framing
// of the site's identity; the "敵全滅後に橋を操作" alternate path is deferred
// along with the rest of the bridge-Object mechanics below (same gap, not a new
// one - see the Object-durability note).
//
// Guest unit is spawned unconditionally across all 3 routes even though the doc's
// route 2/3 text doesn't mention 護衛対象ing (route 2 explicitly says
// 「橋防衛なし」) - StageDescriptor::guestUnits is fixed at scenario-build time,
// before the chosen ExplorationChoice is known (same documented limitation
// blackwaterCrossingStage()'s own `[伝令騎兵]` route note and M9-I's write-up
// already cite: "ルート別のユニット別ステータス修正...機構が無い"). All 3 routes
// therefore share the same escort target and EscapeUnits primary.
//
// Deliberately NOT implemented (documented gap, same M6-C/M9-C/M9-D/M9-H/M9-J
// convention - Object-durability tracking doesn't exist anywhere in the engine):
// - Route 3's「木橋耐久+5」(no durability field on BattleObject to add to).
// - 副目標「木橋耐久を1以上残す」(same gap - no durability to check ≥1 against).
// - 敗北条件「木橋破壊後に代替路なし」(depends on the same missing durability/
//   destruction system - can't detect "the bridge was destroyed" at all).
// - 主目的の代替経路「敵全滅後に橋を操作」(no bridge Object exists to operate;
//   see the OR-composition note above).
// - Route 2's「全員HP-2」: this one is NOT a gap - ExplorationOutcome::partyDamage
//   already exists exactly for this shape (windscar_ascent's sibling
//   CollapsedSidePath route uses enemiesRemoved, this stage's own route uses
//   partyDamage directly), reused as-is.
// - 恒久成果`windscar_relay_bridge_repaired`/キャンプIの安全通過・HP回復効果: same
//   generic siteAccess::Secured mechanism every prior site's permanent outcome
//   uses (see blackwaterCrossingStage()'s own comment) - no new plumbing needed;
//   キャンプI自体はRouteGraph.cpp側で既にM9-Lがsite2後のノードとして配線済み
//   (windscarPlateauGraph()の`windscar_camp1`)なので、本Sliceでの追加配線は不要。
StageDescriptor windscarRelayStage() {
    StageDescriptor stage;
    stage.id = "windscar_relay";
    stage.terrainProfileId = "windscar_ascent";
    stage.enemyRoster = {
        {"windscar_relay_spearman1", "Spearman", UnitClass::Spearman},
        {"windscar_relay_spearman2", "Spearman", UnitClass::Spearman},
        {"windscar_relay_archer1", "Watch Archer", UnitClass::WatchArcher},
        {"windscar_relay_archer2", "Watch Archer", UnitClass::WatchArcher},
    };
    stage.routeOutcomes = {
        // 「吊り橋を一人ずつ渡る」: no condition, 護衛対象1人, 敵4体 (base roster).
        {ExplorationChoice::FrontalAdvance, ExplorationOutcome{}},
        // 「下の砕石道を進む」: no condition, 全員HP-2, 敵3体, 橋防衛なし
        // (「橋防衛なし」は橋Object未実装のため元から関与しない=暗黙のno-op).
        {ExplorationChoice::CollapsedSidePath,
         ExplorationOutcome{.partyDamage = 2, .enemiesRemoved = 1}},
        // `[辺境工兵]` 「橋索を補強する」: 木橋耐久+5(未実装、上記コメント参照)、
        // 敵4体(base roster、増減なし)、硬木+1はvictoryRewardRulesのRouteChoice
        // ルールで表現。
        {ExplorationChoice::ScoutRoute, ExplorationOutcome{}},
    };
    stage.scoutRouteRequiredClass = UnitClass::FrontierEngineer;

    // 護衛対象1人 - blackwaterCrossingStage()の荷運び役2人と同じ非戦闘Escort
    // パターン、ここは1人。DawnChirurgeon再利用(既存最低STRクラス、同じ
    // 「ステータス/表示名だけ再利用」慣習)。
    stage.guestUnits = {
        {{"windscar_relay_porter", "Relay Courier", UnitClass::DawnChirurgeon}, GridPos{1, 3}},
    };

    // 主目的: 護衛対象を右端へ脱出 (EliminateTeam+OperateObjectの代替経路は上記
    // コメントのとおり見送り - EscapeUnitsのみで近似).
    stage.primaryEscapeUnitsAlternative =
        StageDescriptor::PrimaryEscapeUnitsRule{"windscar_relay_escape", /*requiredEscapeCount=*/1,
                                                /*zoneMinCol=*/kGridCols - 1, /*zoneMaxCol=*/kGridCols - 1};

    // 敗北条件「部隊全滅」は既存allPlayersDefeated()のまま。「護衛対象の撤退」は
    // BattleFactory.cppがstage.guestUnitsのidをmissionState().guestUnitIdsへ登録
    // することで自動的にallGuestsLost()経由で配線される(blackwaterCrossingStage()
    // と同じ、追加配線は不要)。「木橋破壊後に代替路なし」は上記コメントのとおり
    // 未実装。

    // 勝利: 織物2、騎具素材1. 工兵ルート: 硬木+1(橋保全ルート自体の耐久効果は
    // 未実装だが、素材報酬の硬木+1はvictoryRewardRulesで単純に表現できる).
    stage.victoryRewardRules = {
        {RewardRule::Condition::Always, {}, {{"cloth", 2}, {"riding_gear", 1}}},
        {RewardRule::Condition::RouteChoice, ExplorationChoice::ScoutRoute, {{"hardwood", 1}}},
    };

    stage.missionNameEn = "Collapsed Relay Path";
    stage.missionNameJa = "崩れた中継路";
    return stage;
}

// docs/regions/windscar_plateau.md「4. 分断された輸送隊」: hand-authored in
// Region.cpp rather than data/regions.json - like windscarRelayStage()/
// blackwaterCrossingStage() above, this stage needs StageDescriptor::
// guestUnits/primaryEscapeUnitsAlternative, neither of which is exposed by
// stageDescriptorFromContent()'s JSON Schema.
//
// **主目的のOR合成の近似**: 正本の主目的は「負傷者1人以上を脱出、または荷物箱1個
// 以上を確保」で、異なるObjective Kind同士のOR(EscapeUnits vs SecureTile)。
// この地点はルート1が明示的に負傷者2人を、ルート2が明示的に荷物箱2個を挙げており
// blackwaterCrossingStage()の荷運び役よりむしろ対称的だが、EscapeUnits(guest)を
// 主目的として採用する判断はM9-Mの前例をそのまま踏襲した - blackwaterCrossingStage()
// が証明済みの`primaryEscapeUnitsAlternative`+`guestUnits`はEnd-to-end動作する
// 一方、荷物箱側の`surveyObjectiveId`はこのプロジェクトで常に「勝利へのボーナス
// 報酬経路」としてのみ実証されており(sunken_path_marker/blackwater_crossing_crate
// 等)、主目的そのものとして機能させる配線(default primary groupの置換ではなく、
// 荷物箱確保単独で勝利させる)はまだどこにも存在しない。1地点のためにこの新しい
// primary化を新設するより、実証済みのEscapeUnits経路を再利用する方が
// 最小プラミングという本プロジェクトの一貫した判断に合う。荷物箱自体は
// Object耐久機構が丸ごと未実装(M6-C以来の既知ギャップ)のため、このSliceでは
// 一切モデル化しない - ルート2の「荷物箱2個」/副目標「荷物箱2個を保全」/
// 敗北条件の「荷物箱をすべて失う」側/全保全報酬`courier_route_chart`は
// すべて未配線のまま据え置く(M9-Hの「到達不能な報酬は未宣言のまま残す」前例と
// 同型 - 主目的が負傷者側の一択である以上、"両方保全"の副目標は到達不能)。
//
// **護衛ユニットは全3ルート共通**: `guestUnits`はシナリオ構築時点で固定され、
// 選択ルートで出し分けられない(M9-Iの既知の限界、windscarRelayStage()/
// blackwaterCrossingStageの`[伝令騎兵]`ルート自身のコメントが同じ制約を既に
// 記録済み)。ルート2「荷車を先に確保する」の正本テキストは負傷者を明示しないが、
// この実装では護衛ユニット自体は3ルート共通で出現する(近似)。
//
// **ルート2「防衛中に負傷者HP-3」**: `StageDescriptor::GuestUnitData`に
// 開始前HPペナルティ用のフィールドは無い(`partyDamage`はplayerParty専用、
// guestUnitsには適用されない)。護衛サブシステム自体に手を入れる新規フィールドを
// 1地点のためだけに追加するのは過剰実装と判断し、暗黙のno-opとして見送った
// (guestUnits関連の既知ギャップとして記録)。
//
// **敵は既存クラスの再利用のみ**: 正本の断崖の野盗(斧兵/弓兵/軽装剣士)には
// 対応する`UnitClass`(Axeman/LightSwordsman相当)が存在しない。弓兵は
// `UnitClass::WatchArcher`をそのまま使うが、斧兵・軽装剣士は新規Classを起こさず
// `UnitClass::Bandit`を「Raider」表示名で再利用した - この表示名は
// `data/regions.json`のsplit_convoyプレースホルダー自身が既に使っていたのと
// 同じ既存ロケールキー(`ui_shared.cpp`の`character.raider`)で、追加のJA
// グリフ登録は不要。「騎兵ルートでは軽装剣士1追加」はenemyRosterへ5体目として
// 常時含め、他2ルートで`enemiesRemoved=1`により差し引く形で表現した
// (windscarAscentStage()の「敵4体→ルート2で敵3体」と同じ加算後減算パターン)。
StageDescriptor windscarConvoyStage() {
    StageDescriptor stage;
    stage.id = "split_convoy";
    stage.terrainProfileId = "windscar_ascent";
    stage.enemyRoster = {
        {"split_convoy_axeman1", "Raider", UnitClass::Bandit},
        {"split_convoy_axeman2", "Raider", UnitClass::Bandit},
        {"split_convoy_archer1", "Watch Archer", UnitClass::WatchArcher},
        {"split_convoy_archer2", "Watch Archer", UnitClass::WatchArcher},
        // 騎兵ルート専用の5体目(軽装剣士相当、Bandit再利用) - 他2ルートは
        // enemiesRemoved=1で差し引く。
        {"split_convoy_swordsman1", "Raider", UnitClass::Bandit},
    };
    stage.routeOutcomes = {
        // 「負傷者を先に救う」: no condition, 負傷者2人を護衛(guestUnits, 全ルート
        // 共通)、荷物報酬-1(riding_gearをRouteChoiceルールで-1)、敵4体
        // (5体目のswordsmanを除く)。
        {ExplorationChoice::FrontalAdvance, ExplorationOutcome{.enemiesRemoved = 1}},
        // 「荷車を先に確保する」: no condition, 荷物箱2個(未モデル化、上記コメント
        // 参照)、防衛中に負傷者HP-3(no-op、上記コメント参照)、敵4体。
        {ExplorationChoice::CollapsedSidePath, ExplorationOutcome{.enemiesRemoved = 1}},
        // `[伝令騎兵]` 「両班へ合図する」: 2組分散配置(地点1/3と同じ「本格機構
        // 無し」判断により暗黙のno-op)、両目的を維持(主目的が単一Kindへ近似済みの
        // ため元々暗黙に真)、敵+1(5体フル、swordsmanを含む)。
        {ExplorationChoice::ScoutRoute, ExplorationOutcome{}},
    };
    stage.scoutRouteRequiredClass = UnitClass::MessengerCavalry;

    // 負傷者2人 - blackwaterCrossingStage()の荷運び役2人と同じ非戦闘Escortパターン。
    stage.guestUnits = {
        {{"split_convoy_evacuee1", "Injured Evacuee", UnitClass::DawnChirurgeon}, GridPos{0, 3}},
        {{"split_convoy_evacuee2", "Injured Evacuee", UnitClass::DawnChirurgeon}, GridPos{2, 3}},
    };

    // 主目的: 負傷者1人以上を脱出(荷物箱側のOR代替経路は上記コメントのとおり
    // 見送り - EscapeUnitsのみで近似)。
    stage.primaryEscapeUnitsAlternative =
        StageDescriptor::PrimaryEscapeUnitsRule{"split_convoy_escape", /*requiredEscapeCount=*/1,
                                                /*zoneMinCol=*/kGridCols - 1, /*zoneMaxCol=*/kGridCols - 1};

    // 敗北条件「部隊全滅」は既存allPlayersDefeated()のまま。「負傷者と荷物箱を
    // すべて失う」のうち負傷者側はBattleFactory.cppがguestUnitsのidを
    // missionState().guestUnitIdsへ登録することで自動的にallGuestsLost()経由で
    // 配線される(blackwaterCrossingStage()と同じ)。荷物箱側はObject耐久機構
    // 未実装のため上記のとおり見送り(複合条件の片側のみ実装、同型の既知ギャップ)。

    // 勝利: 織物2、騎具素材1. ルート1「荷物報酬-1」はriding_gearを-1する
    // RouteChoiceルール(computeStageVictoryLoot()が全ルールを合算し、結果が
    // 正の分だけ残す既存機構をそのまま利用 - 新規プラミング不要)。
    stage.victoryRewardRules = {
        {RewardRule::Condition::Always, {}, {{"cloth", 2}, {"riding_gear", 1}}},
        {RewardRule::Condition::RouteChoice, ExplorationChoice::FrontalAdvance, {{"riding_gear", -1}}},
    };

    stage.missionNameEn = "Split Convoy";
    stage.missionNameJa = "分断された輸送隊";
    return stage;
}

// docs/regions/windscar_plateau.md「5. 断崖荷車道」: hand-authored in
// Region.cpp rather than data/regions.json - like windscarRelayStage()/
// windscarConvoyStage() above, this stage needs StageDescriptor::
// guestUnits/primaryEscapeUnitsAlternative, neither of which is exposed by
// stageDescriptorFromContent()'s JSON Schema.
//
// **荷車=Guestユニットとしてモデル化**: 正本の「荷車耐久12」を、既存の非戦闘
// Escortユニット(GuestUnitData、blackwaterCrossingStage()以来のパターン)1体で
// 近似した - 荷車が右端タイルへ到達すること自体は既存の
// `primaryEscapeUnitsAlternative`/`guestUnits`の脱出判定と機能的に同一のため。
// ただしUnitTemplateにHP上書きフィールドは無い(統計はUnitClassから決まる)ため、
// 「耐久12」という具体的な数値そのものは表現できない - 護衛クラスを再利用した
// 際のベースHPで近似するのみで、正確な12という値は未モデル化(guestUnits関連の
// 既知の限界に付随する新しいギャップ)。
//
// **主目的はEscapeUnitsとして最も素直に一致する地点**: 正本の主目的
// 「荷車または護衛対象1人以上を右端へ脱出」はルートに関わらず「何か」が右端へ
// 到達すればよいため、`primaryEscapeUnitsAlternative`(requiredEscapeCount=1)が
// これまでのどの地点よりも近似ではなく正確に一致する。
//
// **guestUnitsは全3ルート共通(固定)の既知の限界**: シナリオ構築時点で固定され
// ルート別に出し分けられない(M9-Iの既知の限界、windscarRelayStage()/
// windscarConvoyStage()と同型)。正本はルート1/3が荷車1台、ルート2が護衛対象2人
// という異なる構成を明示するが、このSliceでは荷車1体(Guest 1体)を全3ルート
// 共通で採用した - requiredEscapeCount=1はどちらの構成でも変わらず、かつ
// ルート1/3の2ルートが荷車を明示する多数派のため。ルート2の「護衛対象2人」は
// 未モデル化(1体のまま)として記録する。
//
// **ルート3`[重装兵]`「荷車への強風移動無効」は見送り**: BattleState::
// resolveWindGustRoundEnd()のHeavyGuard免除判定(`hasHeavyArmor()`)は
// `unit.unitClass`のみを見る汎用チェックで、プレイヤーか護衛かを区別しない
// ため、荷車Guestの`UnitClass`を`HeavyInfantry`にすれば免除自体は「タダで」
// 発生する。しかしHeavyInfantryは実戦闘クラスであり、荷車という非戦闘物体に
// 本物の重装歩兵ステータス(高STR/高DEF)を与えるのは正本の意図と乖離する。
// guestUnitsはルート別の出し分けもできないため(上記の限界)、ルート3専用に
// 別クラスを割り当てることもできない。よってこの効果はwindscarConvoyStage()の
// 「防衛中に負傷者HP-3」同様、暗黙のno-opとして見送り、コメントにのみ記録する
// (guestUnits関連の既知ギャップ)。
//
// **敵は既存クラスの再利用のみ**: 正本の高原運び手(騎兵2/槍兵2/弓兵1)は
// すべて既存`UnitClass`(MessengerCavalry/Spearman/WatchArcher)に対応するため
// 表示名の再利用なしで直接使用する(windscarRelayStage()と同じ「実クラスその
// まま」パターン)。ルート2「敵4体」はbase 5体ロースターから
// `enemiesRemoved=1`で差し引く(windscarAscentStage()以来の加算後減算パターン)。
//
// Deliberately NOT implemented (documented gap, same M6-C以来の Object耐久
// 未実装 convention):
// - 副目標「すべての荷物を保持」(荷物箱Object耐久が丸ごと未実装)。
// - 副目標「木橋を破壊されない」(橋Object耐久、windscarRelayStage()と同じ
//   既知のギャップ)。
// - 敗北条件「全輸送対象の撤退」のうち荷物側は上記と同じ理由で対象外 - 護衛
//   ユニット側はBattleFactory.cppがguestUnitsのidをmissionState().
//   guestUnitIdsへ登録することで自動的にallGuestsLost()経由で配線される
//   (blackwaterCrossingStage()と同じ)。
// - 全荷物保持報酬`windscar_road_chart`: 副目標自体が未配線のため到達不能
//   (M9-Hの「到達不能な報酬は未宣言のまま残す」前例と同型)。
StageDescriptor windscarCartRoadStage() {
    StageDescriptor stage;
    stage.id = "cliff_cart_road";
    stage.terrainProfileId = "windscar_ascent";
    stage.enemyRoster = {
        {"cliff_cart_cavalry1", "Messenger Cavalry", UnitClass::MessengerCavalry},
        {"cliff_cart_cavalry2", "Messenger Cavalry", UnitClass::MessengerCavalry},
        {"cliff_cart_spearman1", "Spearman", UnitClass::Spearman},
        {"cliff_cart_spearman2", "Spearman", UnitClass::Spearman},
        {"cliff_cart_archer1", "Watch Archer", UnitClass::WatchArcher},
    };
    stage.routeOutcomes = {
        // 「荷車を中央から進める」: no condition, 荷車耐久12(guestUnitsで近似)、
        // 敵5体(base roster)。
        {ExplorationChoice::FrontalAdvance, ExplorationOutcome{}},
        // 「荷物を人手で分ける」: no condition, 護衛対象2人(未モデル化、上記
        // コメント参照 - guestUnitsは1体のまま)、敵4体、騎具素材-1
        // (victoryRewardRulesのRouteChoiceルールで表現)。
        {ExplorationChoice::CollapsedSidePath, ExplorationOutcome{.enemiesRemoved = 1}},
        // `[重装兵]` 「風上側を支える」: 荷車への強風移動無効(no-op、上記コメント
        // 参照)、敵5体(base roster、増減なし)。
        {ExplorationChoice::ScoutRoute, ExplorationOutcome{}},
    };
    stage.scoutRouteRequiredClass = UnitClass::HeavyInfantry;

    // 荷車1体 - DawnChirurgeon再利用(blackwaterCrossingStage()/
    // windscarRelayStage()以来の非戦闘Escortパターンの既存最低STRクラス)。
    stage.guestUnits = {
        {{"cliff_cart_wagon", "Cart", UnitClass::DawnChirurgeon}, GridPos{1, 3}},
    };

    // 主目的: 荷車または護衛対象1人以上を右端へ脱出(上記コメントのとおり、
    // このSliceのguestUnits近似では単一Guestの到達判定として厳密に一致)。
    stage.primaryEscapeUnitsAlternative =
        StageDescriptor::PrimaryEscapeUnitsRule{"cliff_cart_escape", /*requiredEscapeCount=*/1,
                                                /*zoneMinCol=*/kGridCols - 1, /*zoneMaxCol=*/kGridCols - 1};

    // 敗北条件「部隊全滅」は既存allPlayersDefeated()のまま。「全輸送対象の
    // 撤退」はguestUnitsのid登録経由でallGuestsLost()に自動配線される
    // (blackwaterCrossingStage()と同じ)。

    // 勝利: 硬木2、獣皮1、騎具素材1. ルート2「騎具素材-1」はRouteChoiceルール
    // (computeStageVictoryLoot()が全ルールを合算し結果が正の分だけ残す既存機構、
    // windscarConvoyStage()と同じ)。
    stage.victoryRewardRules = {
        // docs/implementation_status.md「素材システム全面再設計」#1続き:
        // highland_fleece added alongside the existing hardwood/hide/riding_gear.
        {RewardRule::Condition::Always, {}, {{"hardwood", 2}, {"hide", 1}, {"riding_gear", 1}, {"highland_fleece", 1}}},
        {RewardRule::Condition::RouteChoice, ExplorationChoice::CollapsedSidePath, {{"riding_gear", -1}}},
    };

    stage.missionNameEn = "Cliffside Cart Road";
    stage.missionNameJa = "断崖荷車道";
    return stage;
}

// docs/regions/windscar_plateau.md「地点構成」: 6-site skeleton + 2 camps,
// same M6/M9 "build the skeleton once, flesh out one site at a time" pattern
// as every prior region. Sites 1-4 (`windscar_ascent`/`windscar_relay`/
// `windwatch_station`/`split_convoy`, "風下の登り口"/"崩れた中継路"/"風見台"/
// "分断された輸送隊") are real content as of this Slice (see
// windscarAscentStage()/windscarRelayStage()/windscarConvoyStage() above and
// `data/regions.json`'s `windwatch_station` entry); site 5 (`cliff_cart_road`,
// "断崖荷車道") is real content as of this Slice too (see
// windscarCartRoadStage() above); site 6 (`plateau_relay`, "高原伝令所") is
// real content as of this Slice too - unlike sites 1/2/4/5 it needed no
// `Region.cpp` hand-written function, since the primary-objective
// approximation this Slice settled on (standard EliminateTeam, see
// `data/regions.json`'s `plateau_relay` entry's own comment) fits the
// existing JSON Schema directly, the same way `windwatch_station` (site 3)
// did.
// Site 3/4's "どちらを
// 先に攻略しても
// よい、両方必須" is wired in RouteGraph.cpp (windscarPlateauGraph()), not
// here - this function only builds the flat stage list RouteGraph indexes
// into by id.
RegionDescriptor windscarPlateauRegion(const GameData& data) {
    RegionDescriptor region;
    region.id = RegionId::WindscarPlateau;
    region.displayNameEn = "Windscar Plateau";
    region.displayNameJa = "風裂き高原";

    region.stages.push_back(windscarAscentStage());
    region.stages.push_back(windscarRelayStage());
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("windwatch_station")));
    region.stages.push_back(windscarConvoyStage());
    region.stages.push_back(windscarCartRoadStage());
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("plateau_relay")));

    return region;
}

// docs/regions/old_frontier_settlement.md「2. 共同井戸」(M9-V): hand-authored
// here rather than data/regions.json - like blackwaterCrossingStage()/
// windscarConvoyStage()/windscarCartRoadStage() before it, this stage needs
// `guestUnits`, neither exposed by stageDescriptorFromContent()'s JSON
// Schema. Replaces M9-U's `settlement_common_well` Bandit x2 placeholder
// (data/regions.json's own entry is left in place, dead/unreferenced - same
// precedent as blackwater_crossing's own dead JSON entry).
//
// **主目的**: 「共同井戸を3ラウンド防衛」は`primarySurviveRoundsAlternative`
// (`SurviveRoundsMissionRule`)の直接再利用 - blackwater_lowlands site3
// (herb_islet, M9-G)以来証明済みの「defaultのprimary groupをAnyへ拡張し
// SurviveRounds memberを足す」パターンをそのまま踏襲した。
//
// **副目標「中立住民を全員避難」は近似ではなく実際の独立Secondary Objective**:
// 新設した`StageDescriptor::secondaryEscapeUnitsAlternative`
// (`primaryEscapeUnitsAlternative`とは別の新規フィールド)を使い、
// `primarySurviveRoundsAlternative`が握る"primary"グループとは別の、
// 完全に独立したObjectiveGroupDefinition("secondary_evacuate"、
// primary=false)を追加した。`primaryEscapeUnitsAlternative`は
// defaultのEliminateTeam primary memberを「置換」する専用パターン
// (blackwater_crossing以来)であり、既にprimarySurviveRoundsAlternativeが
// primaryを握っているこの地点とは共存できない - しかし
// `ObjectiveKind::EscapeUnits`自体はprimary/secondaryのどちらの文脈にも
// 中立(Objective.hppの定義参照)で、`surveyObjectiveId`の副目標グループ
// 追加パターン(BattleFactory.cppの該当ブロック)と全く同型に「新規groupId +
// primary=false」で追加できることをコード読解で確認した。GameApp::
// proceedToCamp()もsurveyObjectiveIdと同じ「このgroupIdを持つdefinitionが
// Completedかどうかを走査する」形で読み戻すだけなので、これは近似ではなく
// 正しい独立Secondary Objectiveとして機能する。
//
// **guestUnitsは4人固定(全3ルート共通)、既知の限界**: シナリオ構築時点で
// 固定されルート別に出し分けられない(windscarConvoyStage()/
// windscarCartRoadStage()と同型の既知の限界、M9-Iの記録どおり)。ルート1
// 「両集団を退避」/ルート3`[旗手]`「共同の防衛位置を示す」がともに4人
// (両集団)を明示する多数派のため、4人固定を採用した。ルート2「井戸だけを
// 先に守る」の正本の2人は未モデル化(4人のまま近似) -
// `secondaryEscapeUnitsAlternative.requiredEscapeCount`も4人固定のため、
// ルート2でも(正本の2人条件より厳しい)4人全員避難が副目標達成条件となる。
//
// **ルート3`[旗手]`「中立Unitが最寄り避難所へ自動移動」は見送り**:
// AI制御された味方ユニットの自動経路探索機構自体がプロジェクトに存在しない
// (windscarConvoyStage()の「防衛中に負傷者HP-3」等と同型の既知のギャップ) -
// 暗黙のno-opとして記録するのみ。`scoutRouteRequiredClass`は
// `UnitClass::BannerBearer`(旗手)。
//
// **敵は既存クラスの再利用のみ**: 正本の斧兵1・弓兵2・軽装剣士1に対応する
// Axeman/LightSwordsman相当のUnitClassは存在しない(M9-U自身の確認と同じ
// 結論) - 弓兵は`WatchArcher`をそのまま使い、斧兵・軽装剣士は`Bandit`を
// 「Raider」表示名で再利用した(M9-Uと同じ再利用パターン、追加JAグリフ
// 登録不要)。「井戸優先ルート(ルート2)は斧兵1追加」は5体目としてベース
// ロースターへ常時含め、ルート1/3で`enemiesRemoved=1`により差し引く
// (windscarAscentStage()以来の加算後減算パターン)。
//
// **敗北条件**: 「部隊全滅」は既存`allPlayersDefeated()`のまま。「中立住民
// 全員の撤退」は`guestUnits`のidを`missionState().guestUnitIds`へ登録する
// ことで`allGuestsLost()`経由で自動配線される(blackwater_crossing以来)。
// 「井戸耐久0」はObject耐久機構が丸ごと未実装(M6-C以来の既知のギャップ)の
// ため見送り - 同じ理由で副目標「井戸耐久8以上」/報酬「食料+1」も未配線
// のまま(M9-Hの「到達不能な報酬は未宣言のまま残す」前例と同型)。
//
// **恒久成果`settlement_well_agreement_reached`**: M9-U自身が地点1の
// `settlement_outer_fence_opened`をまだ配線していない(地域単位の安全帰還
// 恒久化機構自体がこの地域にまだ無い)のと同じ理由で、このSliceでも見送る -
// 個別地点の恒久成果配線は地域全体の「5成果達成+安全帰還で恒久化」機構が
// 実装されるタイミングでまとめて対応する判断。
//
// **全員避難: 集落証言記録**: `kSettlementCommunalTestimonyDiscovery`
// (新規id、正本の「安定ID」表に個別記載が無いため`<region>_..._records`
// 命名規則(kMiningTechniqueRecordsDiscovery等)に倣って選定 - id自体の
// コメントはBaseState.hpp参照) - GameApp::proceedToCamp()から
// quarry_old_mine/ashiron_veinの「2人とも脱出」等と同じad-hoc
// creditedTargetIds/グループ完了チェックパターンで付与する(Region.cpp側は
// 配線しない、GameApp.cppのみ)。
StageDescriptor settlementCommonWellStage() {
    StageDescriptor stage;
    stage.id = "settlement_common_well";
    stage.terrainProfileId = "old_frontier_settlement";
    stage.enemyRoster = {
        {"settlement_well_axeman1", "Raider", UnitClass::Bandit},
        {"settlement_well_archer1", "Watch Archer", UnitClass::WatchArcher},
        {"settlement_well_archer2", "Watch Archer", UnitClass::WatchArcher},
        {"settlement_well_swordsman1", "Raider", UnitClass::Bandit},
        // 井戸優先ルート(CollapsedSidePath)専用の5体目(斧兵追加) - 他2ルートは
        // enemiesRemoved=1で差し引く。
        {"settlement_well_axeman2", "Raider", UnitClass::Bandit},
    };
    stage.routeOutcomes = {
        // 「両集団を井戸から退避」: no condition, 中立Unit4人(guestUnits,
        // 全ルート共通)、敵4体(5体目のaxeman2を除く)。
        {ExplorationChoice::FrontalAdvance, ExplorationOutcome{.enemiesRemoved = 1}},
        // 「井戸だけを先に守る」: no condition, 中立Unit2人(未モデル化、上記
        // コメント参照 - guestUnitsは4人のまま)、敵5体(base roster、フル)、
        // 食料+1。
        {ExplorationChoice::CollapsedSidePath, ExplorationOutcome{}},
        // `[旗手]`「共同の防衛位置を示す」: 中立Unitが最寄り避難所へ自動移動
        // (no-op、上記コメント参照)、敵4体(5体目を除く)。
        {ExplorationChoice::ScoutRoute, ExplorationOutcome{.enemiesRemoved = 1}},
    };
    stage.scoutRouteRequiredClass = UnitClass::BannerBearer;

    // 中立住民4人 - blackwaterCrossingStage()以来の非戦闘Escortパターン
    // (DawnChirurgeon再利用、既存最低STRクラス)。
    stage.guestUnits = {
        {{"settlement_well_resident1", "Settler", UnitClass::DawnChirurgeon}, GridPos{0, 3}},
        {{"settlement_well_resident2", "Settler", UnitClass::DawnChirurgeon}, GridPos{1, 3}},
        {{"settlement_well_resident3", "Settler", UnitClass::DawnChirurgeon}, GridPos{2, 3}},
        {{"settlement_well_resident4", "Settler", UnitClass::DawnChirurgeon}, GridPos{1, 4}},
    };

    // 主目的: 共同井戸を3ラウンド防衛、または敵全滅。
    stage.primarySurviveRoundsAlternative = StageDescriptor::SurviveRoundsMissionRule{"settlement_well_defense", 3};

    // 副目標: 中立住民を全員避難(独立Secondary Objective、上記コメント参照)。
    // 避難先は右端(既存EscapeUnits系地点と同じ列)。
    stage.secondaryEscapeUnitsAlternative = StageDescriptor::SecondaryEscapeUnitsRule{
        "settlement_well_evacuate_all", /*requiredEscapeCount=*/4,
        /*zoneMinCol=*/kGridCols - 1, /*zoneMaxCol=*/kGridCols - 1};

    // 敗北条件「部隊全滅」は既存allPlayersDefeated()のまま。「中立住民全員の
    // 撤退」はguestUnitsのid登録経由でallGuestsLost()に自動配線される
    // (blackwaterCrossingStage()と同じ)。「井戸耐久0」は上記コメントのとおり
    // Object耐久機構未実装のため見送り。

    // 勝利: 食料2、織物1. 井戸優先ルート: 食料+1(RouteChoiceルール)。
    stage.victoryRewardRules = {
        // docs/implementation_status.md「素材システム全面再設計」#1続き:
        // crest_nail added alongside the existing food/cloth.
        {RewardRule::Condition::Always, {}, {{"food", 2}, {"cloth", 1}, {"crest_nail", 1}}},
        {RewardRule::Condition::RouteChoice, ExplorationChoice::CollapsedSidePath, {{"food", 1}}},
    };

    stage.missionNameEn = "Communal Well";
    stage.missionNameJa = "共同井戸";
    return stage;
}

// docs/regions/old_frontier_settlement.md「地点構成」: 6th region, promoted
// from M9-Q's single-stage `old_frontier_settlement_outpost` placeholder to
// the full 5-site + 2-camp skeleton this Slice, mirroring M9-L's own
// "build the skeleton once" step for Windscar Plateau. Site 1
// (`settlement_outer_fence`, "風化した外柵") is real content as of this
// Slice (see `data/regions.json`'s own entry - it fits the JSON Schema
// directly, the same way windwatch_station/plateau_relay did, so no
// hand-written Region.cpp stage function was needed for it). Site 2
// (`settlement_common_well`, "共同井戸") is real content as of M9-V (see
// settlementCommonWellStage() above - hand-authored, like
// blackwaterCrossingStage(), since it needs `guestUnits`). Sites 3-5
// (`settlement_old_granary`/`settlement_gathering_hall`/
// `settlement_dawn_defense`) remain minimal Bandit x2 placeholders (same
// M6-B/C/M9-L "1 site at a time" pattern) to be fleshed out in later Slices.
// Site 3/4's "どちらを先に攻略してもよいが両方必須" branch is wired in
// RouteGraph.cpp (oldFrontierSettlementGraph()), not here - this function
// only builds the flat stage list RouteGraph indexes into by id.
// docs/regions/old_frontier_settlement.md「5. 夜明けの共同防衛」(M9-Y): this
// region's final site, region-boss-equivalent battle. Hand-authored here
// rather than `data/regions.json` - like settlementCommonWellStage() before
// it, needs `guestUnits` (中立住民), not exposed by
// stageDescriptorFromContent()'s JSON Schema. Replaces `data/regions.json`'s
// `settlement_dawn_defense` placeholder entry (left in place, dead/
// unreferenced - same precedent as blackwater_crossing/settlement_common_well's
// own dead JSON entries).
//
// **主目的のAND合成**: 正本は3副条件のAND(5ラウンド防衛 / 井戸か穀物庫どちらかを
// 耐久1以上残す / 警鐘を1回操作)。この形のAND合成(異なるObjectiveKind同士を
// 全部満たす)を表現する汎用機構はエンジンに存在しない(M9-D「封鎖杭2箇所」/
// M9-K「水源標識2個のうちどちらか」以来、1地点のためだけの新設は過剰実装という
// 判断が繰り返されてきた領域と同型)。
// 対応方針(タスク指示の「シンプルな方を採用」に従う):
//   1. 「5ラウンド終了まで避難所を守る」は`primarySurviveRoundsAlternative`
//      (`SurviveRoundsMissionRule`、blackwater_lowlands site3 M9-G以来証明済み)
//      をそのまま再利用した。BattleFactory.cppを実際に読み、この機構は
//      常に「defaultのprimary groupをAnyへ拡張しSurviveRounds memberを足す」
//      (EliminateTeamとのOR)動作であり、「SurviveRoundsのみ、OR拡張なし」の
//      設定は存在しないことを確認した - 新規フィールドを足すことも検討したが、
//      このORが実際の数値・勝敗判定を壊す具体的な理由が見当たらず(敵全滅でも
//      防衛成功でも「守り切った」という正本の趣旨に反しない)、この地点のためだけに
//      新しいAND限定バリアントを追加するのはタスク自身が推奨する「よりシンプルな
//      再利用」を上回る投資と判断し、既存のまま採用した。
//   2. 「井戸または穀物庫を耐久1以上」はObject耐久機構が丸ごと未実装(M6-C以来の
//      既知のギャップ)のため見送り。
//   3. 「警鐘を1回操作」は主目的からは完全に見送るのではなく、新設した
//      `ObjectPlacementRule::secondaryOperateObjectiveId`(このSliceの新規
//      フィールド、既存の`operateObjectiveId`と同型だが「置換」ではなく
//      「独立追加」)経由で実際に機能する非primary Secondary Objectiveとして
//      配線した - 進捗は追跡できるが、主目的を左右はしない。
//
// **副目標「井戸と穀物庫を両方保全」**: Object耐久機構が無いため見送り。
//
// **副目標「中立住民を全員避難」**: `secondaryEscapeUnitsAlternative`
// (settlementCommonWellStage()以来の独立Secondary Objectiveパターン)を
// そのまま再利用。guestUnitsは4人固定・全3ルート共通(既知の限界、下記参照)。
//
// **副目標「襲撃団頭領を撤退させる」**: 新設のUnitClass::RaidLeaderは
// bespoke boss AIを持たず(RaidLeader自身のコメント参照)、汎用
// takeEnemyTurn()のRetreat経路(AiProfile::retreatHpPercent)がそのまま使う。
// GameApp::proceedToCamp()にwindscar_plateau「高原運び手を2人以上撤退」
// (M9-Q)と同じ`exitReason==Retreated`走査のad-hocボーナスを追加した(この
// 地点は特定1体=`settlement_dawn_raid_leader`なので2人以上ではなく該当id
// そのものの撤退を見る)。
//
// **副目標「味方戦闘不能者0」**: 既存`noCasualtiesBonusLoot`をそのまま宣言。
//
// **敗北条件**: 「部隊全滅」は既存`allPlayersDefeated()`。「避難所耐久0」は
// Object耐久機構未実装のため見送り。「中立住民全員の撤退、かつ避難完了者も0」は
// `guestUnits`のid登録経由で`allGuestsLost()`にそのまま配線される
// (blackwater_crossing/settlement_common_well以来) -
// [[feedback_ja_glyph_coverage]]と同種の既存知見どおり、EscapeUnitsの
// crediting は`hasExited`/`isPresent()`を変更しないため、既に避難済みの
// 住民を「撤退で失った」と誤カウントすることはなく、追加配線は不要だった。
//
// **guestUnitsは4人固定・全3ルート共通、既知の限界**: シナリオ構築時点で
// 固定されルート別に出し分けられない(M9-I以来の限界、M9-V/M9-Xと同型)。
// ルート2「住民を先に避難させる」正本の「中立Unitなし」は表現できず、
// guestUnits自体はこのルートでも4人のまま出現する(近似、明記のみ)。
//
// **探索3択**:
//   - 「外柵を中心に守る」: `extraBarrierCount:2`(`settlement_reinforced_barrier`
//     Barrier定義を地点1/4に続き再利用、`scalesWithExtraBarrierOutcome`)。
//     「上段増援が多い」はルート限定の増援配分機構が無いため(M9-U自身の
//     「上段増援」の見送り記録どおり)no-op。
//   - 「住民を先に避難させる」: 「避難所耐久-3」はObject耐久機構未実装のため
//     no-op。「敵1体追加」は`enemyRoster`末尾のraid_axeman3を常時含め、
//     他2ルートで`enemiesRemoved:1`により差し引く(windscarAscentStage()以来の
//     加算後減算パターン)。
//   - `[旗手]`「防衛班を三つに分ける」: `scoutRouteRequiredClass:BannerBearer`。
//     「3組分散配置」は分散初期配置機構自体が無い(M9-L/M9-Fと同型の既知の
//     ギャップ)ためno-op。「警鐘を開始時に1個操作済み」も、Secondary
//     Objectiveを戦闘開始前からCompleted状態にする機構が無いため見送り
//     (この副目標は他2ルートでも通常操作で普通に達成可能なので、達成手段自体は
//     失われない)。`enemiesRemoved:1`で6体目(raid_axeman3)を差し引く。
//
// **敵編成と増援**: 頭領1(RaidLeader)、斧兵2・弓兵2(Raider/WatchArcher再利用、
// M9-U以来の既知の結論どおりAxeman/LightSwordsman相当のUnitClassは存在しない)。
// 2ラウンド目「軽装剣士2、1ラウンド前に予告」は`timedReinforcement`
// (`TimedReinforcementData`、spawnRound:2, announceRoundsBefore:1)で実装。
// **複数波のうち1波のみ実装、既知の新規制限**: `StageDescriptor::
// timedReinforcement`は`std::optional`の単一フィールドで、正本が要求する
// 4ラウンド目の2波目(斧兵1・弓兵1、1ラウンド前予告)を同時に表現できない -
// タスク指示どおり「より影響の大きい方を採用、他方は見送り明記」の判断で、
// 5ラウンド防衛という主目的の中間地点により近い2ラウンド目の波を実装し、
// 4ラウンド目の波は見送った(このプロジェクト全体で複数`timedReinforcement`
// が同時に必要になったのはこの地点が初めてで、既存の前例が無い新種の制約)。
StageDescriptor settlementDawnDefenseStage() {
    StageDescriptor stage;
    stage.id = "settlement_dawn_defense";
    stage.terrainProfileId = "old_frontier_settlement";
    stage.enemyRoster = {
        {"settlement_dawn_raid_leader", "Raid Leader", UnitClass::RaidLeader},
        {"settlement_dawn_axeman1", "Raider", UnitClass::Bandit},
        {"settlement_dawn_axeman2", "Raider", UnitClass::Bandit},
        {"settlement_dawn_archer1", "Watch Archer", UnitClass::WatchArcher},
        {"settlement_dawn_archer2", "Watch Archer", UnitClass::WatchArcher},
        // 「住民を先に避難させる」ルート専用の6体目(敵1体追加) - 他2ルートは
        // enemiesRemoved=1で差し引く。
        {"settlement_dawn_axeman3", "Raider", UnitClass::Bandit},
    };
    stage.routeOutcomes = {
        // 「外柵を中心に守る」: 防護柵2個、敵5体(axeman3を除く)。
        {ExplorationChoice::FrontalAdvance, ExplorationOutcome{.enemiesRemoved = 1, .extraBarrierCount = 2}},
        // 「住民を先に避難させる」: 敵6体(base roster、フル)。
        {ExplorationChoice::CollapsedSidePath, ExplorationOutcome{}},
        // `[旗手]`「防衛班を三つに分ける」: 敵5体(axeman3を除く)。
        {ExplorationChoice::ScoutRoute, ExplorationOutcome{.enemiesRemoved = 1}},
    };
    stage.scoutRouteRequiredClass = UnitClass::BannerBearer;

    // 中立住民4人 - settlementCommonWellStage()以来の非戦闘Escortパターン
    // (DawnChirurgeon再利用)。
    stage.guestUnits = {
        {{"settlement_dawn_resident1", "Settler", UnitClass::DawnChirurgeon}, GridPos{0, 3}},
        {{"settlement_dawn_resident2", "Settler", UnitClass::DawnChirurgeon}, GridPos{1, 3}},
        {{"settlement_dawn_resident3", "Settler", UnitClass::DawnChirurgeon}, GridPos{2, 3}},
        {{"settlement_dawn_resident4", "Settler", UnitClass::DawnChirurgeon}, GridPos{1, 4}},
    };

    // 主目的サブ条件1: 5ラウンド終了まで避難所を守る(または敵全滅) - 上記コメント
    // 参照。サブ条件2(井戸/穀物庫耐久)は見送り。
    stage.primarySurviveRoundsAlternative = StageDescriptor::SurviveRoundsMissionRule{"settlement_dawn_defense_survive", 5};

    // 主目的サブ条件3: 警鐘を1回操作(独立Secondary Objectiveとして配線、
    // 主目的は左右しない - 上記コメント参照)。
    stage.objectPlacementRules = {
        {.definition = BattleObjectDefinition{.definitionId = "settlement_reinforced_barrier",
                                              .kind = BattleObjectKind::Barrier,
                                              .maxDurability = 8,
                                              .defense = 3,
                                              .resistance = 3,
                                              .blocksMovement = true,
                                              .canBeAttacked = true},
         .idPrefix = "settlement_dawn_barrier",
         .count = 0,
         .scalesWithExtraBarrierOutcome = true,
         .zoneMinCol = 0,
         .zoneMaxCol = 2,
         .avoidFirstEnemyRow = true},
        {.definition = BattleObjectDefinition{
             .definitionId = "settlement_alarm_bell",
             .kind = BattleObjectKind::Device,
             .interaction = ObjectInteractionDefinition{.interactionId = "operate_bell", .range = 1, .maxUses = 1},
             .interactionResultState = BattleObjectStateKind::Opened},
         .idPrefix = "settlement_alarm_bell",
         .count = 1,
         .zoneMinCol = 3,
         .zoneMaxCol = 4,
         .secondaryOperateObjectiveId = "settlement_dawn_alarm_bell"},
    };

    // 副目標: 中立住民を全員避難(独立Secondary Objective)。
    stage.secondaryEscapeUnitsAlternative = StageDescriptor::SecondaryEscapeUnitsRule{
        "settlement_dawn_evacuate_all", /*requiredEscapeCount=*/4,
        /*zoneMinCol=*/kGridCols - 1, /*zoneMaxCol=*/kGridCols - 1};

    // 2ラウンド目増援: 軽装剣士2(Raider再利用)、1ラウンド前に予告。
    stage.timedReinforcement = StageDescriptor::TimedReinforcement{
        "settlement_dawn_reinforcement_wave1",
        /*spawnRound=*/2,
        Phase::EnemyPhase,
        /*announceRoundsBefore=*/1,
        /*requiredForElimination=*/true,
        {
            {"settlement_dawn_swordsman1", "Raider", UnitClass::Bandit},
            {"settlement_dawn_swordsman2", "Raider", UnitClass::Bandit},
        },
        {},
    };

    // 副目標「味方戦闘不能者0」: 建築材1。
    stage.noCasualtiesBonusLoot = {{"building_material", 1}};

    // 勝利: 建築材2、鉄材2、食料2(主目的達成時)。
    // docs/implementation_status.md「素材システム全面再設計」次の方針メモ:
    // first_settlement_tablet(キー素材) - この地域には固有Bossクラスが
    // いないため、地域最終防衛戦の達成を「地域制圧」相当として接続。
    stage.victoryRewardRules = {
        {RewardRule::Condition::Always,
        {},
        {{"building_material", 2}, {"iron", 2}, {"food", 2}, {"first_settlement_tablet", 1}}},
    };

    stage.missionNameEn = "Dawn's Joint Defense";
    stage.missionNameJa = "夜明けの共同防衛";
    return stage;
}

RegionDescriptor oldFrontierSettlementRegion(const GameData& data) {
    RegionDescriptor region;
    region.id = RegionId::OldFrontierSettlement;
    region.displayNameEn = "Old Frontier Settlement";
    region.displayNameJa = "旧辺境集落";
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("settlement_outer_fence")));
    region.stages.push_back(settlementCommonWellStage());
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("settlement_old_granary")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("settlement_gathering_hall")));
    region.stages.push_back(settlementDawnDefenseStage());
    return region;
}

// docs/regions/ember_ravine.md「2. 熱風の棚道」: hand-crafted in Region.cpp
// rather than data/regions.json's stageDescriptorFromContent() - same reason
// as windscarRelayStage()/blackwaterCrossingStage() above: this stage needs
// StageDescriptor::guestUnits/primaryEscapeUnitsAlternative, neither of which
// the generic JSON Schema exposes (M9-I's decision, see that stage's own
// comment). Replaces the M9-Z `ember_ravine_ledge` JSON placeholder (still
// present in data/regions.json but now dead/unreferenced, same
// "leave the superseded placeholder in place" precedent as every prior site
// upgrade in this project).
//
// 敵は「熱地採取団」(斧兵/弓兵/工兵型) - this region's own faction doc
// (「主な兵種: 斧兵、弓兵、工兵型」). No AxeInfantry/Engineer-combat UnitClass
// exists in this project (UnitClass.hpp), so all 4 are the same
// "reuse an existing class's numbers, reskin the display name only" pattern
// site 1's 岩蜥蜴=Bandit reskin used: 斧兵x2/工兵型x1 reuse Bandit, 弓兵x1
// reuses WatchArcher (closest existing ranged stat block, same choice
// windscarRelayStage() made for its own 弓兵). Unlike windscarRelayStage()'s
// undisguised Spearman/WatchArcher (which had a doc-confirmed exact class
// match), these get reskinned display names since 斧兵/工兵型 don't map to
// any real UnitClass - no new UnitClass added (regional invariant "新兵種...
// を追加しない").
//
// 探索3択: ルート1「退避所を順に使う」は無条件、護衛対象1人・敵4体(base
// roster)。ルート2「荷物を置いて進む」は`enemiesRemoved:1`(敵3体)+
// `heat_resistant_material`-1をvictoryRewardRulesのRouteChoiceルールで表現
// (負のquantityはM9-Z地点1のルート2前例と同型の「既存reward-ruleの数量を負に
// するだけ」)。「MOV低下なし」は、そもそもこのステージにMOV低下効果が
// 存在しないため(注記のみでロジック不要 - 打ち消す対象が無い)。ルート3
// `[辺境工兵]`「遮熱扉を直す」は`scoutRouteRequiredClass=FrontierEngineer`+
// 敵4体(base roster、増減なし)。
//
// 「冷却床2マス追加」(ルート3限定の地形上書き)は見送り - M9-F/M9-Zが記録済みの
// 「ルート単位・タイル種別単位の地形生成上書き機構がない」ことと同型のギャップ
// (`TerrainProfile::countBounds`は単一TerrainType×ステージ全体専用で、
// ルート条件付きでの追加ができない)。`extraBarrierCount`はBarrier Object用の
// 別機構であり、CoolFloorのような地形タイル種別そのものを差し替えるものではない
// ため転用できない。
//
// 主目的「護衛対象を右端へ脱出」: windscarRelayStage()と全く同じ形の直接再利用
// (`primaryEscapeUnitsAlternative`、requiredEscapeCount=1、護衛対象1人は
// `guestUnits`)。副目標「遮熱扉耐久1以上」は見送り(M6-C以来の既知ギャップ - Object
// 耐久を追跡する仕組みが未実装)。敗北条件「部隊全滅、護衛対象撤退」はどちらも
// 既存の仕組み(`allPlayersDefeated()`/`allGuestsLost()`)がそのまま配線される
// (BattleFactory.cppがguestUnitsのidをmissionState().guestUnitIdsへ登録)。
//
// 護衛ユニットは全3ルート共通(guestUnitsはシナリオ構築時点で固定 - 上記
// windscarRelayStage()の同型コメントどおりの既知の限界)。
//
// 新素材`sulfur`(硫黄): `heat_resistant_material`に続く本地域2つめの新素材、
// `materialNameFor()`のknownセット+`data/locales/{en,ja}.json`
// (`material.sulfur`)へ追加。既存`heat_resistant_material`同様、JA文字列は
// Locale Key経由(`tr()`)でloadAppFont()の`allJapaneseGlyphText()`(ja.json
// 全値を収集)が自動収集するため、charsetSourceへの手動追加は不要
// (`heat_resistant_material`自身も実際にはこの経路で既にカバーされていた -
// M9-Zの記録が示す「charset収集ループへ明示追加」は本コードベースの現状の
// materialNameFor()専用ループには反映されていない、確認の上そのまま踏襲)。
//
// 恒久成果`heated_ledge_shelter_secured`/キャンプIの安全通過効果: 他の全地点と
// 同じ汎用siteAccess::Securedメカニズム(新規配線不要)。キャンプI自体は
// RouteGraph.cpp側でM9-Zがsite2後のノードとして既に配線済み
// (emberRavineGraph()の`ember_ravine_camp1`)。
//
// キャンプIの「遮熱退避所を確保済みなら、キャンプ到達時に生存者の炎上を解除する」
// (siteAccess::Secured条件付きのキャンプ到着時ステータス解除)効果は見送り -
// このプロジェクトにキャンプ到達時にUnitのステータス効果を書き換えるフック自体が
// 存在しない(ExpeditionService.cppのキャンプ到着処理は施設アクセス/回復UIの
// 提示のみで、Burn等のStatusEffectを消費・解除する経路を持たない)。1地点の
// 単発演出のために新規インフラを組むより、M9-Zの熱量/噴気弁フック未接続と同じ
// 「後続Sliceが実際に必要になった時点で着手」判断に合わせ、ドキュメントのみに
// 留める新規ギャップとして記録する。
StageDescriptor emberRavineLedgeStage() {
    StageDescriptor stage;
    stage.id = "ember_ravine_ledge";
    stage.terrainProfileId = "ember_ravine_entrance";
    stage.enemyRoster = {
        {"ember_ravine_ledge_axe1", "Heat Gatherer Axeman", UnitClass::Bandit},
        {"ember_ravine_ledge_axe2", "Heat Gatherer Axeman", UnitClass::Bandit},
        {"ember_ravine_ledge_archer1", "Heat Gatherer Archer", UnitClass::WatchArcher},
        {"ember_ravine_ledge_engineer1", "Heat Gatherer Engineer", UnitClass::Bandit},
    };
    stage.routeOutcomes = {
        // 「退避所を順に使う」: no condition, 護衛対象1人, 敵4体 (base roster).
        {ExplorationChoice::FrontalAdvance, ExplorationOutcome{}},
        // 「荷物を置いて進む」: no condition, 敵3体, 耐熱素材-1
        // (victoryRewardRulesのRouteChoiceで表現), MOV低下なし(そもそも本ステージに
        // MOV低下効果が無いため打ち消す対象がない=暗黙のno-op).
        {ExplorationChoice::CollapsedSidePath, ExplorationOutcome{.enemiesRemoved = 1}},
        // `[辺境工兵]`「遮熱扉を直す」: 冷却床2マス追加(未実装、上記コメント参照)、
        // 敵4体(base roster、増減なし)。
        {ExplorationChoice::ScoutRoute, ExplorationOutcome{}},
    };
    stage.scoutRouteRequiredClass = UnitClass::FrontierEngineer;

    // 護衛対象1人 - windscarRelayStage()の護衛1人と同じ非戦闘Escortパターン。
    // DawnChirurgeon再利用(既存最低STRクラス、同じ「ステータス/表示名だけ再利用」
    // 慣習)。
    stage.guestUnits = {
        {{"ember_ravine_ledge_evacuee", "Ledge Evacuee", UnitClass::DawnChirurgeon}, GridPos{1, 3}},
    };

    // 主目的: 護衛対象を右端へ脱出。
    stage.primaryEscapeUnitsAlternative =
        StageDescriptor::PrimaryEscapeUnitsRule{"ember_ravine_ledge_escape", /*requiredEscapeCount=*/1,
                                                /*zoneMinCol=*/kGridCols - 1, /*zoneMaxCol=*/kGridCols - 1};

    // 敗北条件「部隊全滅」は既存allPlayersDefeated()のまま。「護衛対象の撤退」は
    // BattleFactory.cppがstage.guestUnitsのidをmissionState().guestUnitIdsへ登録
    // することで自動的にallGuestsLost()経由で配線される(追加配線は不要)。

    // 勝利: 耐熱素材1、硫黄1。ルート2は耐熱素材-1(荷物を置いて進む=先に消費する)。
    stage.victoryRewardRules = {
        // docs/implementation_status.md「素材システム全面再設計」#1続き:
        // firetrail_sand added alongside the existing heat_resistant_material/sulfur.
        {RewardRule::Condition::Always, {}, {{"heat_resistant_material", 1}, {"sulfur", 1}, {"firetrail_sand", 1}}},
        {RewardRule::Condition::RouteChoice, ExplorationChoice::CollapsedSidePath,
         {{"heat_resistant_material", -1}}},
    };

    stage.missionNameEn = "Heated Ledge Path";
    stage.missionNameJa = "熱風の棚道";
    return stage;
}

// docs/regions/ember_ravine.md「3. 硫黄窪地」: hand-authored for the same
// reason as emberRavineLedgeStage() - needs `guestUnits` (採取者1人) and, new
// to this Slice, `secondaryProtectUnitAlternative`, neither exposed by
// stageDescriptorFromContent()'s JSON Schema. Replaces the `sulfur_hollow`
// Bandit x2 placeholder (data/regions.json's own entry left in place, dead/
// unreferenced - same precedent as every other site's own dead JSON entry).
//
// **主目的**: 「3ラウンド防衛、または敵全滅」は`primarySurviveRoundsAlternative`
// (`SurviveRoundsMissionRule`)の直接再利用 - herb_islet(M9-G)/
// settlement_common_well(M9-U)以来証明済みのパターンをそのまま踏襲した。
//
// **副目標「採取者を撤退させない」は初のProtectUnit実配線**: ashiron_vein
// (M9-J、Region.cppコメント参照)がJSON Schema/BattleFactoryプラミング欠如を
// 理由に見送り、GameApp.cppのad-hoc isPresent()チェックで近似していた同じ
// 副目標を、今回`StageDescriptor::secondaryProtectUnitAlternative`(新規
// フィールド、上記コメント参照)を新設して本物のObjectiveKind::ProtectUnit
// Definitionとして生成するようにした。ObjectiveTracker.cppのProtectUnit専用
// パス(syncObjectiveProgress()、falling-edgeでActive→Failed)は既に存在して
// いたが、それを生成するStageDescriptorフィールド/BattleFactory配線が今まで
// 一つも無かった(Objective.hpp自身のコメントが明記する「reward-consumption
// gap」の一部) - このSliceでその生成側ギャップを閉じた。ただし正本にこの
// 副目標専用の追加報酬は無い(「勝利: 硫黄2、耐熱素材1」のみ)ため、
// GameApp.cpp側の追加配線(報酬付与)は不要 - 生成/トラッキングのみで完結する。
//
// **敗北条件「採取者撤退」は引き続きallGuestsLost()経由**:
// secondaryProtectUnitAlternative自体はVictory/Defeat判定に一切関与しない
// (StageDescriptor::secondaryProtectUnitAlternativeのコメント、
// ObjectiveTracker::evaluateBattleOutcome()の読解で確認済み) - 「採取者撤退→
// 敗北」は他の護衛地点と同じくguestUnitsのid登録によるallGuestsLost()が担う。
// ProtectUnitはあくまで副目標の可視化用。
//
// **岩蜥蜴3・熱地弓兵1(base roster、4体)、深部ルートで岩蜥蜴4体目追加(5体)**:
// 岩蜥蜴はM9-Zの`firstBurnNegated`Bandit reskinをそのまま再利用、熱地弓兵は
// emberRavineLedgeStage()の熱地採取団弓兵と同じWatchArcher reskin。
//
// **探索3択**: ルート1「必要量だけ採る」は`startingHeatLevel:1`のみ(敵4体は
// base rosterからenemiesRemoved:1で5体目を除く)。ルート2「深部まで採る」は
// `startingHeatLevel:2`(熱量2、M9-Zで実装済みだがこのSliceで初めて実戦闘へ
// 到達する)+敵5体(base roster、フル)+硫黄+2(victoryRewardRulesの
// RouteChoice、勝利報酬2→4)。ルート3`[暁の衛生兵]`「安全時間を測る」は
// `scoutRouteRequiredClass = DawnChirurgeon`+`startingHeatLevel:1`+敵4体
// (enemiesRemoved:1)+耐熱素材+1(勝利報酬1→2)。
StageDescriptor sulfurHollowStage() {
    StageDescriptor stage;
    stage.id = "sulfur_hollow";
    stage.terrainProfileId = "ember_ravine_entrance";
    stage.enemyRoster = {
        {"sulfur_hollow_lizard1", "Rock Lizard", UnitClass::Bandit, /*firstBurnNegated=*/true},
        {"sulfur_hollow_lizard2", "Rock Lizard", UnitClass::Bandit, /*firstBurnNegated=*/true},
        {"sulfur_hollow_lizard3", "Rock Lizard", UnitClass::Bandit, /*firstBurnNegated=*/true},
        {"sulfur_hollow_archer1", "Heat Archer", UnitClass::WatchArcher},
        // 「深部まで採る」ルート専用の5体目(岩蜥蜴追加) - 他2ルートは
        // enemiesRemoved=1で差し引く(settlementCommonWellStage()以来の
        // 加算後減算パターン)。
        {"sulfur_hollow_lizard4", "Rock Lizard", UnitClass::Bandit, /*firstBurnNegated=*/true},
    };
    stage.routeOutcomes = {
        // 「必要量だけ採る」: no condition, 熱量1, 敵4体(5体目を除く), 硫黄2.
        {ExplorationChoice::FrontalAdvance, ExplorationOutcome{.enemiesRemoved = 1, .startingHeatLevel = 1}},
        // 「深部まで採る」: no condition, 熱量2, 敵5体(base roster、フル), 硫黄4
        // (victoryRewardRulesのRouteChoiceで+2).
        {ExplorationChoice::CollapsedSidePath, ExplorationOutcome{.startingHeatLevel = 2}},
        // `[暁の衛生兵]`「安全時間を測る」: 熱量1, 敵4体(5体目を除く), 耐熱素材+1
        // (victoryRewardRulesのRouteChoiceで1→2).
        {ExplorationChoice::ScoutRoute, ExplorationOutcome{.enemiesRemoved = 1, .startingHeatLevel = 1}},
    };
    stage.scoutRouteRequiredClass = UnitClass::DawnChirurgeon;

    // 採取者1人 - windscarRelayStage()/emberRavineLedgeStage()と同じ非戦闘
    // Escortパターン(DawnChirurgeon再利用)。allGuestsLost()経由の敗北条件
    // 「採取者撤退」に加え、下のsecondaryProtectUnitAlternativeが同じidを
    // ProtectUnit Objectiveのtargetとして使う。
    stage.guestUnits = {
        {{"sulfur_hollow_gatherer", "Gatherer", UnitClass::DawnChirurgeon}, GridPos{1, 3}},
    };

    // 主目的: 3ラウンド防衛、または敵全滅。
    stage.primarySurviveRoundsAlternative = StageDescriptor::SurviveRoundsMissionRule{"sulfur_hollow_defense", 3};

    // 副目標: 採取者を撤退させない - 初のProtectUnit実配線(上記コメント参照)。
    stage.secondaryProtectUnitAlternative =
        StageDescriptor::SecondaryProtectUnitRule{"sulfur_hollow_protect_gatherer", "sulfur_hollow_gatherer"};

    // 敗北条件「部隊全滅」は既存allPlayersDefeated()のまま。「採取者撤退」は
    // guestUnitsのid登録経由でallGuestsLost()に自動配線される(上記コメント
    // 参照)。

    // 勝利: 硫黄2、耐熱素材1。深部ルート: 硫黄+2。衛生兵ルート: 耐熱素材+1。
    stage.victoryRewardRules = {
        // docs/implementation_status.md「素材システム全面再設計」#1続き:
        // kilnbone added alongside the existing sulfur/heat_resistant_material.
        {RewardRule::Condition::Always, {}, {{"sulfur", 2}, {"heat_resistant_material", 1}, {"kilnbone", 1}}},
        {RewardRule::Condition::RouteChoice, ExplorationChoice::CollapsedSidePath, {{"sulfur", 2}}},
        {RewardRule::Condition::RouteChoice, ExplorationChoice::ScoutRoute, {{"heat_resistant_material", 1}}},
    };

    stage.missionNameEn = "Sulfur Hollow";
    stage.missionNameJa = "硫黄窪地";
    return stage;
}

// docs/regions/ember_ravine.md「地点構成」: 8-site skeleton + 3 camps, same
// M6/M9 "build the skeleton once, flesh out one site at a time" pattern as
// every prior region. Site 1 (`ember_ravine_entrance`, "焼け石の入口") is
// real content as of M9-Z (see `data/regions.json`'s own entry - it
// fits the existing JSON Schema directly, no hand-written StageDescriptor
// fields needed, same shape as Windscar's `windwatch_station`/`plateau_relay`
// once those needed no Region.cpp function of their own). Site 2
// (`ember_ravine_ledge`, "熱風の棚道") is real content as of M9-AA, hand-
// authored via emberRavineLedgeStage() (guest-escort site, same as
// windscarRelayStage()/blackwaterCrossingStage()). Site 3
// (`sulfur_hollow`, "硫黄窪地") is real content as of this Slice, hand-
// authored via sulfurHollowStage() above (SurviveRounds primary + genuine
// ProtectUnit secondary, see that function's own comment). Sites 4
// (`ravine_cooling_channel`) and 5 (`ash_crystal_shelf`) are real content as
// of M9-AC/M9-AD, and site 6 (`heatwork_shop`, "旧耐熱工房") is real content
// as of this Slice, all three still JSON-authored directly in
// `data/regions.json` (no hand-written StageDescriptor function needed).
// Sites 7/8 remain minimal Bandit x2 placeholders (`data/regions.json`'s
// `ashsealed_observatory`/`redheat_fissure` entries) replacing the single-site
// `ember_ravine_outpost` M9-Y stub (left in place, dead/unreferenced - same
// precedent as `blackwater_crossing`'s own dead JSON entry). The M9-Z
// `ember_ravine_ledge`/`sulfur_hollow` JSON entries are likewise left in
// place, now dead/unreferenced since their Region.cpp functions replace
// them. Site 3/4's "どちらを先に攻略してもよい、両方必須" branch is wired in
// RouteGraph.cpp (emberRavineGraph()), not here.
RegionDescriptor emberRavineRegion(const GameData& data) {
    RegionDescriptor region;
    region.id = RegionId::EmberRavine;
    region.displayNameEn = "Ember Ravine";
    region.displayNameJa = "燼火峡谷";
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("ember_ravine_entrance")));
    region.stages.push_back(emberRavineLedgeStage());
    region.stages.push_back(sulfurHollowStage());
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("ravine_cooling_channel")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("ash_crystal_shelf")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("heatwork_shop")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("ashsealed_observatory")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("redheat_fissure")));
    return region;
}

// docs/regions/buried_dawn_sanctum.md「2. 崩れた礼拝堂」: hand-authored for the
// same reason as emberRavineLedgeStage()/sulfurHollowStage() - needs
// `guestUnits`/`primaryEscapeUnitsAlternative`, neither exposed by
// stageDescriptorFromContent()'s JSON Schema. Replaces the `collapsed_nave`
// Bandit x2 + Wolf placeholder (data/regions.json's own entry left in place,
// dead/unreferenced - same precedent as every other superseded placeholder).
//
// **主目的**: 「避難者1人以上脱出」は`primaryEscapeUnitsAlternative`
// (`PrimaryEscapeUnitsRule`) - blackwater_crossing/windscarRelayStage()/
// emberRavineLedgeStage()以来証明済みのguest-escort primaryをそのまま再利用。
// 敗北条件「全避難者撤退」はguestUnitsのid登録によるallGuestsLost()経由
// (追加配線不要、他の全guest-escort地点と同型)。
//
// **避難者2人**: 副目標「避難者全員脱出」が意味を持つには2人以上必要 -
// タスク側の指示どおり2人とした(DawnChirurgeon再利用、他のguest-escort地点と
// 同じ「ステータス/表示名だけ再利用」慣習)。
//
// **副目標「避難者全員脱出→野戦救護記録」**: RewardRule::ConditionにEscapeUnits
// のcreditedTargetIds件数を読む手段が無いため、blackwater_crossing「2人とも
// 脱出→高品質薬草1」/quarry_old_mine「2人とも脱出→採掘技術記録」と全く同じ
// ad-hoc creditedTargetIds.size()>=2チェックをGameApp.cppへ追加(下記参照)。
// `kFieldMedicalRecordsDiscovery`の命名根拠はBaseState.hppの当該コメント参照。
//
// **探索3択**: 正本の表セルは「避難者優先 / 器具優先 / [暁の衛生兵]負傷判定」
// のみで、sulfur_hollowの回のような数値差分(熱量・敵数増減等)の追加記述が
// 無い(表以外に地点2専用の追加テキストが存在しない箇所まで確認した)。
// sanctum_approach(M9-AH地点1)の「無条件ルートは数値差分なし」前例に倣い、
// ルート1「避難者優先」/ルート2「器具優先」はいずれも無条件・base roster
// (敵勢力節の「聖堂回収団はHP30%以下で降伏・撤退を評価」も地点1同様、この
// 地点専用のAiProfile新設は見送り - 標準雑魚敵への追加チューニング範囲外)。
// ルート3`[暁の衛生兵]`「負傷判定」はscoutRouteRequiredClass:DawnChirurgeonの
// クラス要件のみ(base roster、数値差分なし) - sanctum_approachのルート3
// `[重装兵]`「梁を支える」と同型。
//
// **回収団3・崩土の野生獣1(Wolf reskin)**: 「聖堂回収団」はsanctum_approachと
// 同じBandit reskin("Sanctum Retriever"表示名)。「崩土の野生獣」は正本の
// 敵勢力節「普通の蛇・蜘蛛」を、このプロジェクト長年の「毒蜘蛛=Wolf」前例
// (黒水低湿地以来)でWolf reskinへ近似(`collapsed_nave`placeholderが既に
// "Buried Beast"のWolf reskinとして用意していたためその表示名をそのまま
// 踏襲)。
//
// **主目的報酬 薬草2、聖堂器材1**: `herb`は既存material、`sanctum_equipment`
// (聖堂器材)は本Sliceで新規登録した新素材 - `materialNameFor()`のknownセット
// +`data/locales/{en,ja}.json`+JAグリフcharsetへ追加登録済み(他地点も今後
// この素材を報酬に使うため、地点2で先行登録する)。
//
// **恒久成果`collapsed_nave_sheltered`/キャンプIの状態異常解除効果**: 他の
// 全地点と同じ汎用siteAccess::Securedメカニズム(新規配線不要)。キャンプI
// 自体はRouteGraph.cpp側でM9-AHが既に地点2直後のノードとして配線済み
// (buriedDawnSanctumGraph()の`sanctum_camp1`)。「CAMP Iで状態異常を全解除、
// HP自動回復なし」効果自体はEmber Ravine地点2/CAMP Iのために既にM9-AAが
// 下した判断と同一理由で見送り: このプロジェクトにキャンプ到達時にUnitの
// ステータス効果を書き換えるフック自体が存在しない
// (ExpeditionService.cppのキャンプ到着処理は施設アクセス/回復UIの提示のみ)。
// M9-AAの既存ギャップ記録に合わせ、ドキュメントのみに留める。
StageDescriptor collapsedNaveStage() {
    StageDescriptor stage;
    stage.id = "collapsed_nave";
    stage.terrainProfileId = "buried_dawn_sanctum";
    stage.enemyRoster = {
        {"collapsed_nave_retriever1", "Sanctum Retriever", UnitClass::Bandit},
        {"collapsed_nave_retriever2", "Sanctum Retriever", UnitClass::Bandit},
        {"collapsed_nave_retriever3", "Sanctum Retriever", UnitClass::Bandit},
        {"collapsed_nave_beast1", "Buried Beast", UnitClass::Wolf},
    };
    stage.routeOutcomes = {
        // 「避難者優先」: no condition, base roster.
        {ExplorationChoice::FrontalAdvance, ExplorationOutcome{}},
        // 「器具優先」: no condition, base roster (正本に数値差分の記述なし).
        {ExplorationChoice::CollapsedSidePath, ExplorationOutcome{}},
        // `[暁の衛生兵]`「負傷判定」: base roster、クラス要件のみ.
        {ExplorationChoice::ScoutRoute, ExplorationOutcome{}},
    };
    stage.scoutRouteRequiredClass = UnitClass::DawnChirurgeon;

    // 避難者2人 - windscarRelayStage()/emberRavineLedgeStage()と同じ非戦闘
    // Escortパターン(DawnChirurgeon再利用)。2人にすることで副目標「避難者
    // 全員脱出」が単なる主目的の重複にならない。
    stage.guestUnits = {
        {{"collapsed_nave_evacuee1", "Nave Evacuee", UnitClass::DawnChirurgeon}, GridPos{1, 3}},
        {{"collapsed_nave_evacuee2", "Nave Evacuee", UnitClass::DawnChirurgeon}, GridPos{2, 3}},
    };

    // 主目的: 避難者1人以上を右端へ脱出。
    stage.primaryEscapeUnitsAlternative =
        StageDescriptor::PrimaryEscapeUnitsRule{"collapsed_nave_escape", /*requiredEscapeCount=*/1,
                                                /*zoneMinCol=*/kGridCols - 1, /*zoneMaxCol=*/kGridCols - 1};

    // 敗北条件「部隊全滅」は既存allPlayersDefeated()のまま。「全避難者撤退」は
    // BattleFactory.cppがstage.guestUnitsのidをmissionState().guestUnitIdsへ
    // 登録することで自動的にallGuestsLost()経由で配線される(追加配線不要)。
    // 副目標「避難者全員脱出→野戦救護記録」はcreditedTargetIds.size()>=2の
    // ad-hocチェック(GameApp.cpp、上記コメント参照)。

    // 勝利: 薬草2、聖堂器材1。
    stage.victoryRewardRules = {
        // docs/implementation_status.md「素材システム全面再設計」#1続き:
        // scripture_tile added alongside the existing herb/sanctum_equipment.
        {RewardRule::Condition::Always, {}, {{"herb", 2}, {"sanctum_equipment", 1}, {"scripture_tile", 1}}},
    };

    stage.missionNameEn = "Collapsed Nave";
    stage.missionNameJa = "崩れた礼拝堂";
    return stage;
}

// docs/regions/buried_dawn_sanctum.md「地点と周回」: 6-site skeleton + 2 camps
// + the site 3/4 "順序選択" (either order, both required) branch, same
// M9-U/M9-Z "build the skeleton once" pattern as every prior region. This
// region introduces no new terrain/battle mechanic of its own (per task
// scope), so the site3/4 order-choice is the same `BranchCompletion::
// AllMembers` structure Old Frontier Settlement's granary/hall and Ember
// Ravine's sulfur_hollow/cooling_channel branches already use - the doc's
// "順序選択" wording describes the exact same "either order, both required"
// shape those two branches already implement, not a new mechanism.
// Site 1 (`sanctum_approach`, "埋没参道") is real content as of M9-AH,
// JSON-authored directly (fits the existing Schema, no hand-written
// StageDescriptor function needed, same shape as `settlement_outer_fence`).
// Site 2 (`collapsed_nave`, "崩れた礼拝堂") is real content as of this Slice,
// hand-authored via collapsedNaveStage() above (guest-escort site, same as
// emberRavineLedgeStage()/sulfurHollowStage()). Sites 3-6 (`sanctum_infirmary`/
// `sanctum_archive`/`sealed_passage`/`dawn_altar`) remain minimal Bandit
// x2(-3) placeholders, replacing the single-site `buried_dawn_sanctum_outpost`
// M9-AG stub (left in place, dead/unreferenced - same precedent as
// `ember_ravine_ledge`'s own dead JSON entry after M9-AA superseded it). The
// M9-AH `collapsed_nave` JSON entry is likewise left in place, now
// dead/unreferenced since this Slice's Region.cpp function replaces it.
RegionDescriptor buriedDawnSanctumRegion(const GameData& data) {
    RegionDescriptor region;
    region.id = RegionId::BuriedDawnSanctum;
    region.displayNameEn = "Buried Dawn Sanctum";
    region.displayNameJa = "埋没聖堂";
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("sanctum_approach")));
    region.stages.push_back(collapsedNaveStage());
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("sanctum_infirmary")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("sanctum_archive")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("sealed_passage")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("dawn_altar")));
    return region;
}

// docs/regions/shattered_march_fort.md「7地点仕様」旧兵舎: hand-authored
// (like every prior guest-escort site - blackwaterCrossingStage()/
// emberRavineLedgeStage()/sulfurHollowStage() etc.) because it needs
// `guestUnits`/`primaryEscapeUnitsAlternative`, neither exposed by
// `stageDescriptorFromContent()`'s JSON Schema. Replaces the
// `fort_old_barracks` Bandit x2 placeholder from M9-AN (JSON entry left in
// place, dead/unreferenced - same precedent as every other site's own dead
// JSON entry).
//
// **主目的「負傷兵1人脱出」**: direct reuse of `primaryEscapeUnitsAlternative`
// (requiredEscapeCount=1), the fully-proven M9-I guest-escort subsystem, same
// as every prior guest-escort site (Blackwater Crossing/Windscar Relay/Ember
// Ravine Ledge/Buried Dawn Sanctum's collapsed nave).
//
// **敗北条件「全員撤退」**: `allGuestsLost()`, direct reuse via the same
// guestUnits id-registration mechanism every guest-escort site uses (no
// additional wiring needed).
//
// **負傷兵2人**: the doc doesn't give an explicit guest count for this site,
// so this follows the established "1人以上/全員" wording pattern of every
// prior guest-escort site (Blackwater Crossing's 2 porters, Buried Dawn
// Sanctum's collapsed nave 2 evacuees, Ashiron Quarry's old mine 2 workers) -
// 2 wounded soldiers, escape 1 for primary / both for the secondary bonus
// below.
//
// **敵「残留隊4」**: Bandit x2 + WatchArcher x2 reskinned "Fort Garrison",
// per M9-AO's own established convention for this region's own "残留砦隊"
// faction (distinct from site 1's "Fort Retriever"/軍需回収団 and the dead
// placeholder's own "Fort Retainer").
//
// **探索3択**: 「負傷兵避難」/「武具回収」(いずれも無条件) / `[衛生兵]`
// 「救護班」(`scoutRouteRequiredClass: DawnChirurgeon`) - the doc's table row
// carries no extra numeric deltas (no HP/MOV modifiers listed for this row,
// unlike fort_outer_wall's own route 2), so `routeOutcomes` is the plain
// three-choice enumeration, same shape as fort_broken_gate's (M9-AO) own
// routeOutcomes.
//
// **主目的報酬 軍需品1・織物2**: both already-registered materials
// (`military_supplies` from M9-AO, `cloth` from Windscar Plateau work) -
// confirmed via `data/locales/en.json`/`ja.json` before reuse, no new
// registration (this session's own repeated-duplicate-material-id
// discipline).
//
// **公開副目標「負傷兵全員避難」-> 集団救護記録**: new Discovery
// `kGroupTriageRecordsDiscovery` (id `fort_old_barracks_group_triage_records`,
// following the same `<region-site>_..._records` naming convention as
// `kFieldMedicalRecordsDiscovery`/`kMiningTechniqueRecordsDiscovery` since the
// doc's own 安定ID list doesn't carry an id for this record), granted via the
// same ad-hoc `creditedTargetIds.size()>=2` check in GameApp.cpp as every
// prior "EscapeUnits objective, count-based secondary tier" bonus
// (blackwater_crossing/collapsed_nave/quarry_old_mine all share this exact
// shape).
//
// **恒久成果「兵舎救護」(CAMP IIで最も低HPの生存者5回復) is deferred**: this
// project has no camp-arrival hook that rewrites a Unit's HP/status on camp
// entry - `ExpeditionService.cpp`'s camp-arrival handling is presentation
// (facility access/heal UI) only, the same documented gap as Buried Dawn
// Sanctum's own deferred CAMP I "状態異常を全解除" effect (M9-AI). No new
// infrastructure is built for this single-site effect; the permanent-outcome
// id itself isn't listed in the doc's own 安定ID table (only region/camp-
// level ids are), so nothing is wired here beyond this documented gap.
StageDescriptor fortOldBarracksStage() {
    StageDescriptor stage;
    stage.id = "fort_old_barracks";
    stage.terrainProfileId = "shattered_march_fort";
    stage.enemyRoster = {
        {"fort_old_barracks_bandit1", "Fort Garrison", UnitClass::Bandit},
        {"fort_old_barracks_bandit2", "Fort Garrison", UnitClass::Bandit},
        {"fort_old_barracks_archer1", "Fort Garrison", UnitClass::WatchArcher},
        {"fort_old_barracks_archer2", "Fort Garrison", UnitClass::WatchArcher},
    };
    stage.routeOutcomes = {
        // 「負傷兵避難」: no condition, base roster.
        {ExplorationChoice::FrontalAdvance, ExplorationOutcome{}},
        // 「武具回収」: no condition, base roster.
        {ExplorationChoice::CollapsedSidePath, ExplorationOutcome{}},
        // `[衛生兵]`「救護班」: no condition beyond the class requirement,
        // base roster.
        {ExplorationChoice::ScoutRoute, ExplorationOutcome{}},
    };
    stage.scoutRouteRequiredClass = UnitClass::DawnChirurgeon;

    // 負傷兵2人 - blackwaterCrossingStage()の荷運び役2人と同じ非戦闘Escort
    // パターン(DawnChirurgeon再利用、既存最低STRクラス、ステータス/表示名だけ
    // 再利用する慣習)。
    stage.guestUnits = {
        {{"fort_old_barracks_wounded1", "Wounded Soldier", UnitClass::DawnChirurgeon}, GridPos{0, 3}},
        {{"fort_old_barracks_wounded2", "Wounded Soldier", UnitClass::DawnChirurgeon}, GridPos{2, 3}},
    };

    // 主目的: 負傷兵2人のうち1人以上を右端へ脱出。
    stage.primaryEscapeUnitsAlternative =
        StageDescriptor::PrimaryEscapeUnitsRule{"fort_old_barracks_escape", /*requiredEscapeCount=*/1,
                                                /*zoneMinCol=*/kGridCols - 1, /*zoneMaxCol=*/kGridCols - 1};

    // 敗北条件「部隊全滅」は既存allPlayersDefeated()のまま。「負傷兵全員撤退」は
    // BattleFactory.cppがstage.guestUnitsのidをmissionState().guestUnitIdsへ登録
    // することで自動的にallGuestsLost()経由で配線される(blackwaterCrossingStage()
    // と同じ)。

    // 勝利: 軍需品1、織物2。
    stage.victoryRewardRules = {
        // docs/implementation_status.md「素材システム全面再設計」#1続き:
        // trench_plate added alongside the existing military_supplies/cloth.
        {RewardRule::Condition::Always, {}, {{"military_supplies", 1}, {"cloth", 2}, {"trench_plate", 1}}},
    };

    // 恒久成果「兵舎救護」(CAMP IIで最も低HPの生存者5回復)はカメラ到着時Unit書換
    // フック自体が存在しないため見送り(上記コメント参照)。

    stage.missionNameEn = "Old Barracks";
    stage.missionNameJa = "旧兵舎";
    return stage;
}

// docs/regions/shattered_march_fort.md「地点・周回」: 9th region skeleton
// expanded to the full 7-site/3-camp shape this Slice (same M9-AH precedent:
// no new terrain/battle mechanic introduced by the region's own 正本, so the
// region skeleton + site 1 content lands in a single Slice, sites 2-7
// remaining Bandit x2(-3) placeholders for future Slices (M9-AN); sites 2
// (M9-AO) and 3 (this Slice) have since become real content). Site 3/4
// (`fort_old_barracks`/`fort_logistics_depot`) is a "順序選択" pair wired via
// RouteGraph.cpp's `BranchCompletion::AllMembers`, identical in shape to
// BuriedDawnSanctum's `sanctum_infirmary_archive_branch`/EmberRavine's
// `ember_ravine_sulfur_channel_branch`. Site 1 (`fort_outer_wall`, "破砕外郭")
// is real content as of M9-AN, JSON-authored directly (fits the existing
// Schema, no hand-written StageDescriptor function needed, same shape as
// `sanctum_approach`/`settlement_outer_fence`). Site 2 (`fort_broken_gate`,
// "崩れ門") is real content as of M9-AO, also JSON-authored (single
// OperateObject Objective). Site 3 (`fort_old_barracks`, "旧兵舎") is real
// content as of M9-AP, hand-authored via fortOldBarracksStage() above
// (guest-escort site, see that function's own comment). Site 4
// (`fort_logistics_depot`, "兵站庫") is real content as of this Slice, also
// JSON-authored (EliminateTeam-primary + crate-secondary, same shape as
// `sanctum_archive`).
RegionDescriptor shatteredMarchFortRegion(const GameData& data) {
    RegionDescriptor region;
    region.id = RegionId::ShatteredMarchFort;
    region.displayNameEn = "Shattered March Fort";
    region.displayNameJa = "破砕された前線砦";
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("fort_outer_wall")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("fort_broken_gate")));
    region.stages.push_back(fortOldBarracksStage());
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("fort_logistics_depot")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("fort_signal_yard")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("fort_reserve_wall")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("fort_severance_order_archive")));
    return region;
}

// docs/regions/mapped_edge.md「9地点仕様」地点6「石盆地」: hand-authored in
// Region.cpp rather than data/regions.json - like blackwaterCrossingStage()/
// windscarRelayStage() above, this stage needs StageDescriptor::guestUnits/
// primaryEscapeUnitsAlternative, neither of which is exposed by
// stageDescriptorFromContent()'s JSON Schema (same precedent as
// blackwater_crossing's own dead JSON entry - `data/regions.json`'s
// `mapped_edge_stone_basin` placeholder entry is left in place unreferenced).
//
// **主目的「護衛2人中1人脱出」**: blackwaterCrossingStage()と全く同じ形の直接
// 再利用 - `primaryEscapeUnitsAlternative`(requiredEscapeCount=1)+`guestUnits`
// 2体、右端ゾーンへの脱出。敗北条件「全護衛撤退」はBattleFactory.cppが
// guestUnitsのidをmissionState().guestUnitIdsへ登録することで自動的に
// allGuestsLost()経由で配線される(blackwaterCrossingStage()以来、追加配線は
// 不要)。Porter/DawnChirurgeon再利用(既存最低STRクラス、同じ「ステータス/
// 表示名だけ再利用」慣習)。
//
// **敵「大型獣1、野生獣4」**: 正本の「地形と脅威」節が「既存の大型個体で構成
// する」と定め、この地点自体の表側にはテレグラフ/ボス演出の記載が無い(地点9
// 「地図外縁」最終戦の大型獣とは異なり、明示的な予告突進の物語付けが無い)。
// よってAshenhornBoarを**素のUnitClassとして**再利用する - EnemyAI.cpp's
// `takeEnemyTurn()`はunitClassだけでtakeBoarBossTurn()へ分岐するため、この
// 分岐自体は避けられないが、AshenhornBoarのボスAIは(RedbackLizardの
// `kRedheatFissureGateTile`のような)ステージ固有の固定座標leashを一切
// 参照しない自己完結した突進/暴走メカニクス(HP50%閾値のEnrage、電荷
// テレグラフ、丸太衝突スタン)であるため、他地点の地形へそのまま持ち込んでも
// 座標的な破綻がない。これが「大型獣1」の4候補
// (AshenhornBoar/AshironGrubworm/MarshFangSerpent/RedbackLizard)のうち
// AshenhornBoarを選んだ理由。新規UnitClass・新規AIプロファイルの追加は無し。
// 野生獣4はWolf再利用(このプロジェクト全体の野生動物の確立済み慣習)。
//
// **探索3択**: 「中央横断」「外周護衛」はwindscarRelayStage()の
// FrontalAdvance/CollapsedSidePathと同型のフレーバーのみの分岐(数値差分なし)。
// `[重装兵]`「落石受止め」はscoutRouteRequiredClass=HeavyInfantryで表現し、
// windscarAscentStage()以来のenemiesRemoved機構をそのまま再利用して「1体を
// 落石で足止め」を敵-1体として近似した(新規のhazard-mitigation機構は追加
// しない)。
//
// Deliberately NOT implemented (documented gap, same M6-C以来の convention):
// - guestUnitsは全3ルート共通(固定) - シナリオ構築時点で確定するため、ルート
//   別に護衛のステータス/人数を出し分けられない(blackwaterCrossingStage()の
//   `[伝令騎兵]`ルート注釈と同型の既知の限界)。
// - 「落石受止め」の落石そのもの(地形上の予告危険Object)は本Sliceでは生成
//   せず、敵-1体という結果面のみを近似(hazardタイル自体の生成/解除機構は
//   このSliceの範囲外)。
StageDescriptor mappedEdgeStoneBasinStage() {
    StageDescriptor stage;
    stage.id = "mapped_edge_stone_basin";
    stage.terrainProfileId = "mapped_edge";
    stage.enemyRoster = {
        {"mapped_edge_stone_basin_boar", "Stone Basin Beast", UnitClass::AshenhornBoar},
        {"mapped_edge_stone_basin_wolf1", "Wolf", UnitClass::Wolf},
        {"mapped_edge_stone_basin_wolf2", "Wolf", UnitClass::Wolf},
        {"mapped_edge_stone_basin_wolf3", "Wolf", UnitClass::Wolf},
        {"mapped_edge_stone_basin_wolf4", "Wolf", UnitClass::Wolf},
    };
    stage.routeOutcomes = {
        // 「中央横断」: no condition, 敵5体(base roster)。
        {ExplorationChoice::FrontalAdvance, ExplorationOutcome{}},
        // 「外周護衛」: no condition, flavor-only(数値差分なし、windscarRelay
        // Stage()の同型ペアと同じ慣習)。
        {ExplorationChoice::CollapsedSidePath, ExplorationOutcome{}},
        // `[重装兵]` 「落石受止め」: 野生獣1体を落石で足止め(敵4体)。
        {ExplorationChoice::ScoutRoute, ExplorationOutcome{.enemiesRemoved = 1}},
    };
    stage.scoutRouteRequiredClass = UnitClass::HeavyInfantry;

    // 護衛2人 - blackwaterCrossingStage()の荷運び役2人と同じ非戦闘Escort
    // パターン。
    stage.guestUnits = {
        {{"mapped_edge_stone_basin_escort1", "Escort", UnitClass::DawnChirurgeon}, GridPos{0, 3}},
        {{"mapped_edge_stone_basin_escort2", "Escort", UnitClass::DawnChirurgeon}, GridPos{2, 3}},
    };

    // 主目的: 護衛2人中1人以上を右端へ脱出。
    stage.primaryEscapeUnitsAlternative =
        StageDescriptor::PrimaryEscapeUnitsRule{"mapped_edge_stone_basin_escape", /*requiredEscapeCount=*/1,
                                                /*zoneMinCol=*/kGridCols - 1, /*zoneMaxCol=*/kGridCols - 1};

    // 敗北条件「部隊全滅」は既存allPlayersDefeated()のまま。「全護衛撤退」は
    // guestUnitsのid登録経由でallGuestsLost()に自動配線される
    // (blackwaterCrossingStage()以来)。

    // 主目的報酬: 希少素材2、地域固有素材1(frontier_edge_material -
    // mapped_edge_split_survey_routeで既出、再利用のみ、新規素材登録は無し)。
    stage.victoryRewardRules = {
        // docs/implementation_status.md「素材システム全面再設計」#1続き:
        // stareater_moss added alongside the existing rare_material/frontier_edge_material.
        {RewardRule::Condition::Always, {}, {{"rare_material", 2}, {"frontier_edge_material", 1}, {"stareater_moss", 1}}},
    };

    stage.missionNameEn = "Stone Basin";
    stage.missionNameJa = "石盆地";
    return stage;
}

// docs/regions/mapped_edge.md「最終戦「地図外縁」」/「9地点仕様」地点9「地図外縁」
// (the FINAL site of the FINAL region of the whole 10-region campaign).
//
// **主目的「標識3個設置後、4人中1人以上を帰還基点へ脱出」**: a two-phase primary
// (place-then-escape) this engine has no chained-AND-in-order composition
// for (same class of gap as every "true AND vs available infra" case this
// project has approximated ~10+ times before). Approximated as BOTH groups
// being required, WITHOUT the strict "escape only after placing" ordering:
// 3 `objectPlacementRules` entries, each with its own `operateObjectiveId`
// (the same true-multi-Object-AND pattern as windwatch_station/
// fort_signal_yard/mapped_edge_abandoned_relay's dual-panel AND, extended
// from 2 to 3 markers - verified by reading BattleFactory.cpp: every rule
// with `operateObjectiveId` set adds its OperateObject Objective(s) to the
// same "primary" group, default rule ObjectiveGroupRule::All, and the
// group's default EliminateTeam member is removed by the FIRST such rule
// only, not re-added by later ones) PLUS `primaryEscapeUnitsAlternative`
// (`PrimaryEscapeUnitsRule`) - confirmed by reading the same function that
// this ALSO removes "eliminate_enemies" (idempotent, already gone) and adds
// its own EscapeUnits Objective to the SAME "primary" group, which stays
// ObjectiveGroupRule::All (nothing here widens it to Any, unlike
// primaryHoldTileAlternative/primarySecureTileAlternative/
// primarySurviveRoundsAlternative's own "widen to Any" pattern) - so the
// final "primary" group ends up as a true AND of all 4 members (3 markers +
// 1 escape), exactly matching "敵全滅だけでは勝利しない...標識設置後の帰還を
// 必須にする" in spirit, minus the ordering constraint (a player can
// technically stand on the escape tile before placing all 3 markers without
// it mattering - Victory only evaluates once every member is Completed).
// `primaryEscapeUnitsAlternative` isn't exposed by stageDescriptorFromContent()'s
// JSON Schema (same as mappedEdgeStoneBasinStage()'s own reason for being
// hand-authored), so this whole stage is hand-authored in C++ rather than
// data/regions.json (whose `mapped_edge_outermost_marker` placeholder entry
// is left dead/unreferenced, same precedent as blackwater_crossing/
// mapped_edge_stone_basin's own dead JSON entries).
//
// **敗北「標識全損」**: Object耐久機構が存在しないため、地点4/5/7/8の「主盤0」/
// 「両方破壊」/「基点0」と全く同じ既知ギャップとして見送り(部隊全滅は既存
// Engineで常時有効)。
//
// **敗北「12Round超過」**: round-limit機構自体がこのEngineに存在しない
// (Cinderwatch 6周目・Ember Ravine地点7・mapped_edge_split_survey_route/M9-AX
// の「8Round超過」がいずれも同じ理由で見送り済み - 本Sliceでも同じ扱い)。
//
// **環境波の大型獣「撃破は不要」**: モデル化不要 - primary groupに
// EliminateTeamメンバーが一切無いため(上記の3標識+脱出のみ)、この大型獣を
// 倒しても倒さなくてもVictory/Defeatに一切影響しない。これはUnitClass::
// FrontierBeast側で意図的にretreatHpPercent等のチューニングを一切していない
// 理由でもある(不要だから)。
//
// **3波**: `StageDescriptor::timedReinforcement`が単一`std::optional`である
// 既知の制限(fort_reserve_wall/M9-Y/mapped_edge_return_base/M9-BB以来)の
// ため、3波を「初期ロースター(機動波2体+環境波の大型獣1体)+増援1波
// (制圧波4体、2ラウンド目、1ラウンド前予告、右端3マスへ帰還路を塞ぐ形で出現)」
// の2段階へ近似した。環境波の大型獣を「波」としてタイミングをずらすのではなく
// 初期ロースターへ含めたのは、正本が環境波を「予告地形」を伴う持続的な脅威として
// 描写しており(機動波/制圧波のような対人間の戦術的出現トリガーの記載が無い)、
// 開始直後から盤面に存在する自己完結した危険物という扱いの方が自然だと判断した
// ため。機動波はMessengerCavalry(騎兵型、windwatch_station/plateau_relay等
// 以来のenemy-flavor再利用慣習)+FrontierScout(斥候型、既存の12兵種のうち
// 敵flavorとしての再利用はこのSliceが初出 - 新規UnitClassではなく既存クラスの
// 転用)。制圧波はVeteranGuard(守備兵型)+WatchArcher(弓兵型)、どちらも
// 既に本地域で敵flavor再利用済み(old_barracks/signal_tower/last_signal等の
// VeteranGuard、abandoned_relay/return_base/broken_watchtowerのWatchArcher)。
//
// **探索3択**: 「最奥標識優先」「観測記録優先」はwindscarRelayStage()以来の
// フレーバーのみペア(数値差分なし、正本の表セルに追加の数値デルタ記載も無い)。
// `[行軍隊長]`「撤退順固定」はscoutRouteRequiredClass=MarchCaptain+
// enemiesRemoved=1(mappedEdgeStoneBasinStage()の「落石受止め」以来のenemy
// Countによる近似 - 実際の「撤退順」制御機構はこのEngineに存在しないため、
// 結果面のみを敵-1体として近似する)。
//
// Deliberately NOT implemented (documented gap, cited above inline):
// - 標識耐久/「標識全損」敗北条件(Object耐久機構が存在しない)
// - 「12Round超過」敗北条件(round-limit機構が存在しない)
// - 副目標「観測箱2個保全」(surveyObjectiveId等の副目標配線は本Sliceでは
//   追加しない - 主目的自体が既に4メンバーAND+guestUnits非使用という
//   このプロジェクトで最も複雑な組み合わせであり、これ以上の副目標追加は
//   後続Sliceへ持ち越す)
// - 副目標「Pending Loot保護箱を使わず完遂」(この概念自体に対応するフックが
//   Engineに存在しない)
// - 恒久成果id`outermost_markers_placed`/`mapped_edge_secured`/
//   `main_campaign_completed`(正本の「安定ID」節に記載があるが、地点1〜8と
//   同様、恒久成果配線・地域攻略(安全帰還)Slice自体が本Sliceの範囲外)
// - 副目標「4人全員帰還」「標識3個すべて耐久1以上」「10Round以内」(耐久・
//   round-limit機構が無い/複数人脱出のクレジット判定を副目標として追加する
//   ところまでは本Sliceの範囲に含めなかった - 主目的の4メンバーAND自体の
//   実装・検証を優先した)
StageDescriptor mappedEdgeFinalStage() {
    StageDescriptor stage;
    stage.id = "mapped_edge_outermost_marker";
    stage.terrainProfileId = "mapped_edge";

    // 初期ロースター: 機動波(騎兵型・斥候型、標識設置者を狙う)+環境波の大型獣1体
    // (人間のObjectiveは理解しない、持続的な脅威として開始直後から盤面に存在)。
    stage.enemyRoster = {
        {"mapped_edge_final_cavalry1", "Outrider Cavalry", UnitClass::MessengerCavalry},
        {"mapped_edge_final_scout1", "Outrider Scout", UnitClass::FrontierScout},
        {"mapped_edge_final_beast", "Frontier Beast", UnitClass::FrontierBeast},
    };

    stage.routeOutcomes = {
        // 「最奥標識優先」: no condition, flavor-only。
        {ExplorationChoice::FrontalAdvance, ExplorationOutcome{}},
        // 「観測記録優先」: no condition, flavor-only(数値差分なし)。
        {ExplorationChoice::CollapsedSidePath, ExplorationOutcome{}},
        // `[行軍隊長]` 「撤退順固定」: 敵1体を足止め(結果面のみの近似)。
        {ExplorationChoice::ScoutRoute, ExplorationOutcome{.enemiesRemoved = 1}},
    };
    stage.scoutRouteRequiredClass = UnitClass::MarchCaptain;

    // 増援1波: 制圧波(守備兵型・弓兵型、帰還路を塞ぐ) - 2ラウンド目、1ラウンド
    // 前予告、右端(帰還基点側)3マスへ出現。
    stage.timedReinforcement = StageDescriptor::TimedReinforcement{
        "mapped_edge_final_reinforcement_wave2",
        /*spawnRound=*/2,
        Phase::EnemyPhase,
        /*announceRoundsBefore=*/1,
        /*requiredForElimination=*/true,
        {
            {"mapped_edge_final_guard1", "Veteran Guard", UnitClass::VeteranGuard},
            {"mapped_edge_final_guard2", "Veteran Guard", UnitClass::VeteranGuard},
            {"mapped_edge_final_archer1", "Watch Archer", UnitClass::WatchArcher},
            {"mapped_edge_final_archer2", "Watch Archer", UnitClass::WatchArcher},
        },
        {GridPos{0, kGridCols - 1}, GridPos{1, kGridCols - 1}, GridPos{2, kGridCols - 1}},
    };

    // 主目的サブ条件1: 最奥標識3個を設置(操作)する - 3件のDevice、それぞれ
    // 独立のoperateObjectiveIdを持ち、いずれもデフォルトの"primary"グループ
    // (ObjectiveGroupRule::All)へ乗る(このファイル冒頭のコメントで検証済みの
    // 真の3-way AND)。
    for (int i = 1; i <= 3; ++i) {
        const std::string prefix = "mapped_edge_final_marker_" + std::to_string(i);
        BattleObjectDefinition markerDef;
        markerDef.definitionId = prefix;
        markerDef.kind = BattleObjectKind::Device;
        markerDef.interaction = ObjectInteractionDefinition{"operate_marker", /*range=*/1, {},
                                                             BattleObjectStateKind::Active, /*maxUses=*/1};
        markerDef.interactionResultState = BattleObjectStateKind::Opened;
        const int zoneMin = (i - 1) * (kGridCols / 3);
        const int zoneMax = i == 3 ? kGridCols - 1 : zoneMin + (kGridCols / 3) - 1;
        stage.objectPlacementRules.push_back(StageDescriptor::ObjectPlacementRule{
            markerDef, prefix, /*count=*/1, /*scalesWithExtraBarrierOutcome=*/false, zoneMin, zoneMax,
            /*avoidFirstEnemyRow=*/false, "operate_" + prefix, std::nullopt});
    }

    // 主目的サブ条件2: 4人中1人以上を帰還基点(右端)へ脱出。
    stage.primaryEscapeUnitsAlternative =
        StageDescriptor::PrimaryEscapeUnitsRule{"mapped_edge_final_escape", /*requiredEscapeCount=*/1,
                                                /*zoneMinCol=*/kGridCols - 1, /*zoneMaxCol=*/kGridCols - 1};

    // 主目的報酬: 最終キー素材1(新規`frontier_final_key`、本編最終発展/深層
    // 遠征候補の解放条件 - 正本「安定ID」節どおりの名前をそのまま採用)、
    // 希少素材3、遺跡片2(どちらも既存登録素材の再利用)。
    // docs/implementation_status.md「素材システム全面再設計」次の方針メモ:
    // edge_anchor(キー素材)・outerwild_core(レア素材、外縁固有の大型獣
    // ドロップ)も同じ主目的達成へ接続。
    stage.victoryRewardRules = {
        {RewardRule::Condition::Always,
        {},
        {{"frontier_final_key", 1},
         {"rare_material", 3},
         {"ruin_fragment", 2},
         {"edge_anchor", 1},
         {"outerwild_core", 1}}},
    };

    stage.missionNameEn = "Mapped Edge";
    stage.missionNameJa = "地図外縁";
    return stage;
}

// docs/regions/mapped_edge.md「地点と周回」: 10th and FINAL region of the
// whole campaign. Its own 正本 explicitly introduces no new terrain/battle
// mechanic (existing terrain types recombined 3-at-a-time per battle, no new
// UnitClass, no new elite/boss class for the eventual no-fixed-boss 3-wave
// finale) - the same shape BuriedDawnSanctum/ShatteredMarchFort followed, so
// this Slice expands the M9-AT 1-site placeholder stub straight to the full
// 9-site/4-camp skeleton (`1 最後の既知標識 -> 2 乾いた川床 -> CAMP I ->
// (3 無記録野営跡 / 4 二股の踏査路、順序選択) -> CAMP II -> 5 放棄中継所 ->
// 6 石盆地 -> CAMP III -> 7 折れた見張台 -> 8 帰還基点 -> CAMP IV ->
// 9 地図外縁`) in one Slice, with site 1 ("最後の既知標識") as real content
// and sites 2-9 left as Bandit x2 minimal placeholders for future Slices
// (identical convention to every prior region's own skeleton Slice).
RegionDescriptor mappedEdgeRegion(const GameData& data) {
    RegionDescriptor region;
    region.id = RegionId::MappedEdge;
    region.displayNameEn = "Mapped Edge";
    region.displayNameJa = "地図外縁";
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("mapped_edge_last_known_marker")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("mapped_edge_dry_riverbed")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("mapped_edge_unrecorded_camp")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("mapped_edge_split_survey_route")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("mapped_edge_abandoned_relay")));
    region.stages.push_back(mappedEdgeStoneBasinStage());
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("mapped_edge_broken_watchtower")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("mapped_edge_return_base")));
    region.stages.push_back(mappedEdgeFinalStage());
    return region;
}

} // namespace

// docs/deep_layers.md「全体構成」/「ボス(深層に2体、最深層に1体、計3体)」:
// 灰枝の森(AshboughForest)の深層縦通し用ステージ。RegionId/RouteGraphを持たず
// (regionDescriptor()のswitchにもRouteGraph.cppのroute一覧にも登録しない)、
// 呼び出し側が生の配列として直接BattleFactoryへ渡す(Region.hppのコメント参照)。
// 新規地形/新規敵UnitClassは追加しない方針のとおり、本編と同じ`brokenwood_
// territory`地形と、既存の`Wolf`/`AshenhornBoar`をそのまま再利用する
// (ボスは`MappedEdge`の"Stone Basin Beast"と同じ「再利用+リスキン」の前例)。
// 敵の強化倍率自体はここでは掛けない(StageDescriptorにその概念がないため) -
// 呼び出し側がDeepLayerScaling.hppのkDeepLayerScaleStage1/2/3をBattleState
// 構築後に後掛けする。実体(*Impl)は無名namespace内、これは公開ラッパー。
std::vector<StageDescriptor> ashboughForestDeepStages() { return ashboughForestDeepStagesImpl(); }
std::vector<StageDescriptor> ashboughForestDeepestStages() { return ashboughForestDeepestStagesImpl(); }

RegionDescriptor regionDescriptor(RegionId id, const GameData& data) {
    switch (id) {
        case RegionId::CinderwatchGate: return cinderwatchGateRegion(data);
        case RegionId::AshboughForest: return ashboughForestRegion(data);
        case RegionId::AshironQuarry: return ashironQuarryRegion(data);
        case RegionId::BlackwaterLowlands: return blackwaterLowlandsRegion(data);
        case RegionId::WindscarPlateau: return windscarPlateauRegion(data);
        case RegionId::OldFrontierSettlement: return oldFrontierSettlementRegion(data);
        case RegionId::EmberRavine: return emberRavineRegion(data);
        case RegionId::BuriedDawnSanctum: return buriedDawnSanctumRegion(data);
        case RegionId::ShatteredMarchFort: return shatteredMarchFortRegion(data);
        case RegionId::MappedEdge: return mappedEdgeRegion(data);
        case RegionId::AshboughForestDeep: {
            RegionDescriptor region;
            region.id = RegionId::AshboughForestDeep;
            region.displayNameEn = "Ashbough Forest - Deep Layer";
            region.displayNameJa = "灰枝の森・深層";
            region.stages = ashboughForestDeepStages();
            return region;
        }
        case RegionId::AshboughForestDeepest: {
            RegionDescriptor region;
            region.id = RegionId::AshboughForestDeepest;
            region.displayNameEn = "Ashbough Forest - Deepest Layer";
            region.displayNameJa = "灰枝の森・最深層";
            region.stages = ashboughForestDeepestStages();
            return region;
        }
    }
    return cinderwatchGateRegion(data);
}

std::string toString(RegionId id) {
    switch (id) {
        case RegionId::CinderwatchGate: return "cinderwatch_gate";
        case RegionId::AshboughForest: return "ashbough_forest";
        case RegionId::AshironQuarry: return "ashiron_quarry";
        case RegionId::BlackwaterLowlands: return "blackwater_lowlands";
        case RegionId::WindscarPlateau: return "windscar_plateau";
        case RegionId::OldFrontierSettlement: return "old_frontier_settlement";
        case RegionId::EmberRavine: return "ember_ravine";
        case RegionId::BuriedDawnSanctum: return "buried_dawn_sanctum";
        case RegionId::ShatteredMarchFort: return "shattered_march_fort";
        case RegionId::MappedEdge: return "mapped_edge";
        case RegionId::AshboughForestDeep: return "ashbough_forest_deep";
        case RegionId::AshboughForestDeepest: return "ashbough_forest_deepest";
    }
    return "cinderwatch_gate";
}

RegionId regionIdFromString(const std::string& id) {
    if (id == "ashbough_forest") return RegionId::AshboughForest;
    if (id == "ashiron_quarry") return RegionId::AshironQuarry;
    if (id == "blackwater_lowlands") return RegionId::BlackwaterLowlands;
    if (id == "windscar_plateau") return RegionId::WindscarPlateau;
    if (id == "old_frontier_settlement") return RegionId::OldFrontierSettlement;
    if (id == "ember_ravine") return RegionId::EmberRavine;
    if (id == "buried_dawn_sanctum") return RegionId::BuriedDawnSanctum;
    if (id == "shattered_march_fort") return RegionId::ShatteredMarchFort;
    if (id == "mapped_edge") return RegionId::MappedEdge;
    if (id == "ashbough_forest_deep") return RegionId::AshboughForestDeep;
    if (id == "ashbough_forest_deepest") return RegionId::AshboughForestDeepest;
    return RegionId::CinderwatchGate;
}

std::optional<RegionId> regionIdFromStringStrict(const std::string& id) {
    if (id == "ashbough_forest") return RegionId::AshboughForest;
    if (id == "cinderwatch_gate") return RegionId::CinderwatchGate;
    if (id == "ashiron_quarry") return RegionId::AshironQuarry;
    if (id == "blackwater_lowlands") return RegionId::BlackwaterLowlands;
    if (id == "windscar_plateau") return RegionId::WindscarPlateau;
    if (id == "old_frontier_settlement") return RegionId::OldFrontierSettlement;
    if (id == "ember_ravine") return RegionId::EmberRavine;
    if (id == "buried_dawn_sanctum") return RegionId::BuriedDawnSanctum;
    if (id == "shattered_march_fort") return RegionId::ShatteredMarchFort;
    if (id == "mapped_edge") return RegionId::MappedEdge;
    if (id == "ashbough_forest_deep") return RegionId::AshboughForestDeep;
    if (id == "ashbough_forest_deepest") return RegionId::AshboughForestDeepest;
    return std::nullopt;
}

std::vector<LootStack> computeStageVictoryLoot(const StageDescriptor& stage, ExplorationChoice choice,
                                               bool surveyObjectiveSucceeded) {
    std::vector<std::pair<LootId, int>> totals;
    auto add = [&](const std::vector<LootStack>& stacks) {
        for (const LootStack& stack : stacks) {
            auto it = std::find_if(totals.begin(), totals.end(),
                                   [&](const auto& entry) { return entry.first == stack.id; });
            if (it == totals.end()) totals.push_back({stack.id, stack.quantity});
            else it->second += stack.quantity;
        }
    };

    for (const RewardRule& rule : stage.victoryRewardRules) {
        bool applies = rule.condition == RewardRule::Condition::Always ||
                      (rule.condition == RewardRule::Condition::RouteChoice && rule.routeChoice == choice) ||
                      (rule.condition == RewardRule::Condition::SurveySuccess && surveyObjectiveSucceeded &&
                       stage.surveyObjectiveId);
        if (applies) add(rule.loot);
    }

    std::vector<LootStack> result;
    for (const auto& [id, quantity] : totals) {
        if (quantity > 0) result.push_back({id, quantity});
    }
    return result;
}

std::vector<DiscoveryId> computeStageDiscoveries(const StageDescriptor& stage, ExplorationChoice choice) {
    std::vector<DiscoveryId> result = stage.discoveries;
    for (const auto& [routeChoice, discoveries] : stage.routeDiscoveries) {
        if (routeChoice == choice) result.insert(result.end(), discoveries.begin(), discoveries.end());
    }
    return result;
}

ExplorationOutcome stageRouteOutcome(const StageDescriptor& stage, ExplorationChoice choice) {
    for (const auto& [routeChoice, outcome] : stage.routeOutcomes) {
        if (routeChoice == choice) return outcome;
    }
    return cinderwatchOutcome(choice);
}

std::string siteAccessKey(RegionId regionId, const std::string& stageId) {
    return toString(regionId) + ":" + stageId;
}

bool regionCleared(RegionId regionId, const BaseState& base, const GameData& data) {
    RegionDescriptor region = regionDescriptor(regionId, data);
    for (const StageDescriptor& stage : region.stages) {
        auto it = base.siteAccess.find(siteAccessKey(regionId, stage.id));
        if (it == base.siteAccess.end() || it->second < SiteAccessState::Surveyed) return false;
    }
    return true;
}

bool regionUnlocked(RegionId regionId, const BaseState& base, const GameData& /*data*/) {
    switch (regionId) {
        case RegionId::AshboughForest: return true;
        case RegionId::CinderwatchGate: return base.completedRegionIds.count(RegionId::AshboughForest) > 0;
        case RegionId::AshironQuarry: return base.completedRegionIds.count(RegionId::CinderwatchGate) > 0;
        case RegionId::BlackwaterLowlands: return base.completedRegionIds.count(RegionId::AshironQuarry) > 0;
        case RegionId::WindscarPlateau: return base.completedRegionIds.count(RegionId::BlackwaterLowlands) > 0;
        case RegionId::OldFrontierSettlement: return base.completedRegionIds.count(RegionId::WindscarPlateau) > 0;
        case RegionId::EmberRavine: return base.completedRegionIds.count(RegionId::OldFrontierSettlement) > 0;
        case RegionId::BuriedDawnSanctum: return base.completedRegionIds.count(RegionId::EmberRavine) > 0;
        case RegionId::ShatteredMarchFort: return base.completedRegionIds.count(RegionId::BuriedDawnSanctum) > 0;
        case RegionId::MappedEdge: return base.completedRegionIds.count(RegionId::ShatteredMarchFort) > 0;
        // docs/deep_layers.md「全体構成」: 本編10地域の直線チェーンとは別の支線
        // (AshboughForest安全帰還が前提、深層クリアで最深層が解放 - 段階的
        // アンロック)。他の9地域の解放判定には一切影響しない。
        case RegionId::AshboughForestDeep: return base.completedRegionIds.count(RegionId::AshboughForest) > 0;
        case RegionId::AshboughForestDeepest:
            return base.completedRegionIds.count(RegionId::AshboughForestDeep) > 0;
    }
    return true;
}

} // namespace jf
