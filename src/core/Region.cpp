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
            rule.zoneMaxCol, rule.avoidFirstEnemyRow, rule.operateObjectiveId});
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

// docs/implementation_roadmap.md M6-D: minimal placeholder for the 3rd
// region, unlocked once CinderwatchGate completes (docs/regions/
// cinderwatch_gate.md「地域攻略結果」/「安全帰還時」の「灰鉄採石場を遠征先へ
// 追加」). Same role M6-A's original `cinderwatch_outpost` played before
// M6-A/B/C fleshed Cinderwatch out - a single stage just enough to make the
// region selectable and completable. The real 5-site content is M9's scope
// (docs/implementation_status.md「次地域(灰鉄採石場、5地点)」).
RegionDescriptor ashironQuarryRegion(const GameData& data) {
    RegionDescriptor region;
    region.id = RegionId::AshironQuarry;
    region.displayNameEn = "Ashiron Quarry";
    region.displayNameJa = "灰鉄採石場";

    region.stages.push_back(stageDescriptorFromContent(data.stageContent("quarry_entrance")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("quarry_terrace")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("quarry_old_mine")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("quarry_hoist_works")));
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("ashiron_vein")));
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
        {RewardRule::Condition::Always, {}, {{"herb", 1}, {"wetland_resin", 1}}},
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
        {RewardRule::Condition::Always, {}, {{"hide", 2}, {"hardwood", 1}}},
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
        {RewardRule::Condition::Always, {}, {{"hardwood", 2}, {"hide", 1}, {"riding_gear", 1}}},
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

// docs/regions/windscar_plateau.md「地域攻略と拠点接続」の「旧辺境集落を次の
// 遠征先へ追加」: 6th region, added as a minimal 1-site placeholder (Bandit x2)
// - the exact same role Ashiron Quarry's `ashiron_quarry_outpost` (M6-D) and
// WindscarPlateau's own pre-M9-L `windswept_highland_outpost` stub played
// before their regions had real content. Does not use RouteGraph.cpp (see
// usesRouteGraph()) - a single-stage placeholder region has no branches or
// camps to route through yet, same as WindscarPlateau's own stub stage
// before M9-L added the graph.
RegionDescriptor oldFrontierSettlementRegion(const GameData& data) {
    RegionDescriptor region;
    region.id = RegionId::OldFrontierSettlement;
    region.displayNameEn = "Old Frontier Settlement";
    region.displayNameJa = "旧辺境集落";
    region.stages.push_back(stageDescriptorFromContent(data.stageContent("old_frontier_settlement_outpost")));
    return region;
}

} // namespace

RegionDescriptor regionDescriptor(RegionId id, const GameData& data) {
    switch (id) {
        case RegionId::CinderwatchGate: return cinderwatchGateRegion(data);
        case RegionId::AshboughForest: return ashboughForestRegion(data);
        case RegionId::AshironQuarry: return ashironQuarryRegion(data);
        case RegionId::BlackwaterLowlands: return blackwaterLowlandsRegion(data);
        case RegionId::WindscarPlateau: return windscarPlateauRegion(data);
        case RegionId::OldFrontierSettlement: return oldFrontierSettlementRegion(data);
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
    }
    return "cinderwatch_gate";
}

RegionId regionIdFromString(const std::string& id) {
    if (id == "ashbough_forest") return RegionId::AshboughForest;
    if (id == "ashiron_quarry") return RegionId::AshironQuarry;
    if (id == "blackwater_lowlands") return RegionId::BlackwaterLowlands;
    if (id == "windscar_plateau") return RegionId::WindscarPlateau;
    if (id == "old_frontier_settlement") return RegionId::OldFrontierSettlement;
    return RegionId::CinderwatchGate;
}

std::optional<RegionId> regionIdFromStringStrict(const std::string& id) {
    if (id == "ashbough_forest") return RegionId::AshboughForest;
    if (id == "cinderwatch_gate") return RegionId::CinderwatchGate;
    if (id == "ashiron_quarry") return RegionId::AshironQuarry;
    if (id == "blackwater_lowlands") return RegionId::BlackwaterLowlands;
    if (id == "windscar_plateau") return RegionId::WindscarPlateau;
    if (id == "old_frontier_settlement") return RegionId::OldFrontierSettlement;
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
    }
    return true;
}

} // namespace jf
