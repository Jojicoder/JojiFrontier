#include "jf/core/Region.hpp"

#include <algorithm>

namespace jf {

namespace {

// Matches docs/base_development.md's "最初の縦切り実装" table exactly - this
// is a byte-for-byte migration of the old kVictoryLoot/kMissionNames/
// kStageDiscoveries/fieldTypeForStage/stage==0/stage==2 special cases into
// data, so Cinderwatch's behavior is unchanged by this refactor.
RegionDescriptor cinderwatchGateRegion() {
    RegionDescriptor region;
    region.id = RegionId::CinderwatchGate;
    // docs/regions/cinderwatch_gate.md: "# 第2地域 沈黙した監視所群" / "日本語名
    // 「沈黙した監視所群」を表示する" - the region's own doc, not "Cinderwatch
    // Gate" (that's stage0's mission name, cinderwatch_outpost, not the
    // region's name).
    region.displayNameEn = "Silenced Watchpost Cluster";
    region.displayNameJa = "沈黙した監視所群";

    StageDescriptor stage0;
    stage0.id = "cinderwatch_outpost";
    stage0.fieldType = FieldType::CinderwatchOutpost;
    stage0.enemyCountOverride = 3; // only 3 of the 4-unit shared roster appear
    stage0.baseVictoryLoot = {{"gate_tools", 1}, {"ash_road_map", 1}, {"hide", 2}, {"wood", 4}};
    stage0.discoveries = {kCinderwatchReconDiscovery};
    stage0.missionNameEn = "Cinderwatch Gate";
    stage0.missionNameJa = "シンダーウォッチ関門";
    region.stages.push_back(stage0);

    StageDescriptor stage1;
    stage1.id = "ironwatch_stores";
    stage1.fieldType = FieldType::AshRoad;
    stage1.baseVictoryLoot = {{"field_medicine", 1}, {"watch_ledger", 1}, {"wood", 3}, {"herb", 2}};
    stage1.discoveries = {kFieldMedicineDiscovery, kHerbThicketDiscovery};
    stage1.missionNameEn = "Ironwatch Stores";
    stage1.missionNameJa = "アイアンウォッチ物資庫";
    region.stages.push_back(stage1);

    StageDescriptor stage2;
    stage2.id = "signal_tower";
    stage2.fieldType = FieldType::SignalTower;
    stage2.boostedFirstEnemy = StageDescriptor::BoostedEnemy{"Former Captain", 10, 2};
    stage2.baseVictoryLoot = {
        {"signal_lens", 1}, {"captains_seal", 1}, {kAshveilFangMaterial, 1}, {"wood", 3}, {"hide", 3}};
    stage2.discoveries = {kReturnSignalDiscovery};
    stage2.missionNameEn = "The Last Signal";
    stage2.missionNameJa = "最後の信号塔";
    region.stages.push_back(stage2);

    return region;
}

// docs/regions/ashbough_forest.md "1. 灰枝の林縁" - the only location
// implemented so far (docs/implementation_roadmap.md Phase 2 scope). Its 4
// wolves are a self-contained roster, not part of GameData::enemyRoster.
RegionDescriptor ashboughForestRegion() {
    RegionDescriptor region;
    region.id = RegionId::AshboughForest;
    region.displayNameEn = "Ashbough Forest";
    region.displayNameJa = "灰枝の森";

    StageDescriptor verge;
    verge.id = "ashbough_verge";
    verge.fieldType = FieldType::AshboughVerge;
    verge.enemyRoster = {
        {"ashbough_wolf1", "Wolf", UnitClass::Wolf},
        {"ashbough_wolf2", "Wolf", UnitClass::Wolf},
        {"ashbough_wolf3", "Wolf", UnitClass::Wolf},
        {"ashbough_wolf4", "Wolf", UnitClass::Wolf},
    };
    // 通常勝利: 木材2、獣皮1
    verge.baseVictoryLoot = {{"wood", 2}, {"hide", 1}};
    // 急行ルート: 木材-2 (通常勝利の木材なし) / 斥候ルート: 獣皮+1
    verge.routeVictoryLootDelta = {
        {ExplorationChoice::CollapsedSidePath, {{"wood", -2}}},
        {ExplorationChoice::ScoutRoute, {{"hide", 1}}},
    };
    // 副目標「踏査地点を確保」: 木材+1
    verge.surveyObjectiveId = "ashbough_verge_surveyed";
    verge.surveyBonusLoot = {{"wood", 1}};
    verge.missionNameEn = "Ashbough Verge";
    verge.missionNameJa = "灰枝の林縁";
    region.stages.push_back(verge);

    // docs/regions/ashbough_forest.md "2. 薬草の沢". Reinforcement (a 4th
    // wolf arriving turn 2 on the harvest route), the Dawn Chirurgeon-only
    // dedicated survey tile (`herbwater_hollow_surveyed` RegionProgress
    // record), and the post-harvest one-time +2 HP on continue are not yet
    // implemented (no reinforcement-wave infrastructure exists; the
    // Chirurgeon-only tile needs a per-battle-instance required-unit-id
    // that this data-only StageDescriptor can't express). The main
    // objective, 3 exploration choices, terrain (Shallows + 2 HerbPatch),
    // and the common "薬草地点確保" Any-of-2-tiles bonus are implemented.
    StageDescriptor herbwater;
    herbwater.id = "herbwater_hollow";
    herbwater.fieldType = FieldType::HerbwaterHollow;
    herbwater.enemyRoster = {
        {"herbwater_wolf1", "Wolf", UnitClass::Wolf},
        {"herbwater_wolf2", "Wolf", UnitClass::Wolf},
        {"herbwater_wolf3", "Wolf", UnitClass::Wolf},
        {"herbwater_wolf4", "Wolf", UnitClass::Wolf},
        {"herbwater_wolf5", "Wolf", UnitClass::Wolf},
    };
    // 通常勝利: 木材1
    herbwater.baseVictoryLoot = {{"wood", 1}};
    // 採取ルート: 薬草2 / 衛生兵ルート: 高品質薬草1 (reinforcement on the
    // harvest route is deferred - see note above)
    herbwater.routeVictoryLootDelta = {
        {ExplorationChoice::CollapsedSidePath, {{"herb", 2}}},
        {ExplorationChoice::ScoutRoute, {{"quality_herb", 1}}},
    };
    // 共通副目標「薬草地点確保」: 薬草+1 (either of the 2 HerbPatch tiles)
    herbwater.surveyObjectiveId = "herbwater_hollow_herb_secured";
    herbwater.surveyBonusLoot = {{"herb", 1}};
    // 衛生兵ルート: 増援なし・味方全員を左2列のランダム候補へ制限。まだ辺境斥候の
    // 「自由配置」ではなく「乱数配置を2列に絞る」効果である点に注意。
    herbwater.routeOutcomes = {
        {ExplorationChoice::FrontalAdvance, {}},
        {ExplorationChoice::CollapsedSidePath, {}},
        {ExplorationChoice::ScoutRoute, {.restrictedAutoSpawnMaxColumn = 1}},
    };
    herbwater.scoutRouteRequiredClass = UnitClass::DawnChirurgeon;
    herbwater.missionNameEn = "Herbwater Hollow";
    herbwater.missionNameJa = "薬草の沢";
    region.stages.push_back(herbwater);

    // docs/regions/ashbough_forest.md "3. 折れ木の縄張り"/"灰角大猪". Route C
    // ("[辺境猟兵]獣の痕跡を追う") is out of scope per the doc's own text - it
    // needs 辺境猟兵, a post-clear recruit-only class that doesn't exist yet,
    // and the doc explicitly frames C as "初回攻略用ではなく再訪・再挑戦用の
    // 選択肢". The primary objective is the default EliminateTeam mission
    // The escort wolf remains active while the boar loses its own turn to a
    // fallen-log collision, preventing the stun window from becoming a fully
    // uncontested Enemy Phase.
    StageDescriptor brokenwood;
    brokenwood.id = "brokenwood_territory";
    brokenwood.fieldType = FieldType::BrokenwoodTerritory;
    brokenwood.enemyRoster = {
        {"ashenhorn_boar", "Ashenhorn Boar", UnitClass::AshenhornBoar},
        {"brokenwood_guard_wolf", "Wolf", UnitClass::Wolf},
        {"brokenwood_guard_wolf2", "Wolf", UnitClass::Wolf},
        {"brokenwood_guard_wolf3", "Wolf", UnitClass::Wolf},
        {"brokenwood_guard_wolf4", "Wolf", UnitClass::Wolf},
    };
    // ボス撃破保証: 灰角の大牙1、木材2、獣皮2
    brokenwood.baseVictoryLoot = {{kAshenhornFangMaterial, 1}, {"wood", 2}, {"hide", 2}};
    // A(慎重に): 通常倒木を回収して木材+1。B(誘導): 倒木を罠として破損させるため
    // 追加木材なし。
    brokenwood.routeVictoryLootDelta = {
        {ExplorationChoice::FrontalAdvance, {{"wood", 1}}},
    };
    // 両ルートとも「味方4人は左2列のランダム候補」。Bだけ倒木をもう1本追加。
    brokenwood.routeOutcomes = {
        {ExplorationChoice::FrontalAdvance, {.restrictedAutoSpawnMaxColumn = 1}},
        {ExplorationChoice::CollapsedSidePath, {.restrictedAutoSpawnMaxColumn = 1, .extraBarrierCount = 1}},
    };
    brokenwood.scoutRouteDisabled = true;
    // 副目標「倒木衝突」: 灰角の欠片1 / 副目標「無傷」: 獣皮1 (both routes)
    brokenwood.logCollisionBonusLoot = {{"ashenhorn_fragment", 1}};
    brokenwood.noCasualtiesBonusLoot = {{"hide", 1}};
    brokenwood.missionNameEn = "Brokenwood Territory";
    brokenwood.missionNameJa = "折れ木の縄張り";
    region.stages.push_back(brokenwood);

    return region;
}

} // namespace

RegionDescriptor regionDescriptor(RegionId id, const GameData& /*data*/) {
    switch (id) {
        case RegionId::CinderwatchGate: return cinderwatchGateRegion();
        case RegionId::AshboughForest: return ashboughForestRegion();
    }
    return cinderwatchGateRegion();
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
