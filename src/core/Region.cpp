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
    stage.baseVictoryLoot = content.baseVictoryLoot;
    stage.routeVictoryLootDelta = content.routeVictoryLootDelta;
    stage.routeDiscoveries = content.routeDiscoveries;
    stage.surveyObjectiveId = content.surveyObjectiveId;
    stage.surveyBonusLoot = content.surveyBonusLoot;
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
            content.boostedFirstEnemy->defenseBonus};
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

} // namespace

RegionDescriptor regionDescriptor(RegionId id, const GameData& data) {
    switch (id) {
        case RegionId::CinderwatchGate: return cinderwatchGateRegion(data);
        case RegionId::AshboughForest: return ashboughForestRegion(data);
    }
    return cinderwatchGateRegion(data);
}

std::string toString(RegionId id) {
    switch (id) {
        case RegionId::CinderwatchGate: return "cinderwatch_gate";
        case RegionId::AshboughForest: return "ashbough_forest";
    }
    return "cinderwatch_gate";
}

RegionId regionIdFromString(const std::string& id) {
    if (id == "ashbough_forest") return RegionId::AshboughForest;
    return RegionId::CinderwatchGate;
}

std::optional<RegionId> regionIdFromStringStrict(const std::string& id) {
    if (id == "ashbough_forest") return RegionId::AshboughForest;
    if (id == "cinderwatch_gate") return RegionId::CinderwatchGate;
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

    add(stage.baseVictoryLoot);
    for (const auto& [routeChoice, delta] : stage.routeVictoryLootDelta) {
        if (routeChoice == choice) add(delta);
    }
    if (surveyObjectiveSucceeded && stage.surveyObjectiveId) add(stage.surveyBonusLoot);

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
    }
    return true;
}

} // namespace jf
