#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "jf/core/Item.hpp"

namespace jf {

// docs/implementation_roadmap.md Phase 3 "周回・地域経路の開拓" /
// docs/exploration_system.md "周回と地域経路の開拓". Per-site progression,
// promoted only on a safe return from an expedition (never mid-run, never on
// defeat - see ExpeditionState::pendingSiteAccessUpdates). Defined here
// (rather than in Region.hpp, which includes this header) so BaseState's
// siteAccess map below can hold it by value.
enum class SiteAccessState {
    Unknown = 0,   // 未踏
    Surveyed = 1,  // 踏査済み
    Secured = 2,   // 経路確保済み
};

// docs/implementation_roadmap.md "Phase 1.5 軽量地域データ基盤" /
// docs/region_unlocks.md. Defined here (rather than in Region.hpp, which
// includes this header) for the same reason as SiteAccessState above:
// BaseState::completedRegionIds below needs it as a complete type.
enum class RegionId {
    CinderwatchGate,
    AshboughForest,
    AshironQuarry,
    BlackwaterLowlands,
    // docs/regions/windscar_plateau.md「内部地域IDは`windscar_plateau`」: 5th
    // region. M9-K added this as a minimal placeholder (same role Ashiron
    // Quarry's own `ashiron_quarry_outpost` stub played before M9-A fleshed
    // it out); this Slice renamed the enum/string id from the pre-spec
    // "WindsweptHighland"/"windswept_highland" to match the doc's internal
    // id exactly (every other region's enum name already matches its
    // toString() 1:1 - see Region.cpp) and fleshed out the region skeleton +
    // site 1.
    WindscarPlateau,
    // docs/regions/windscar_plateau.md「旧辺境集落を次の遠征先へ追加」: 6th
    // region. This Slice added it as a minimal placeholder (same role
    // WindscarPlateau's own pre-M9-L `windswept_highland_outpost` stub
    // played), unlocked once WindscarPlateau completes. Content itself is
    // out of scope here - a later Slice fleshes it out, mirroring M9-K/L's
    // "add the stub now, flesh it out later" split.
    OldFrontierSettlement,
    // docs/regions/old_frontier_settlement.md「燼火峡谷を遠征先へ追加」: 7th
    // region. Added as a minimal placeholder (same role OldFrontierSettlement's
    // own pre-M9-U `old_frontier_settlement_outpost`-shaped stub played),
    // unlocked once OldFrontierSettlement completes. Content itself is out of
    // scope here - a later Slice fleshes it out, mirroring M9-K/L/Q/U's
    // established "add the stub now, flesh it out later" split.
    EmberRavine,
    // docs/regions/ember_ravine.md「埋没聖堂を次の遠征先へ追加」: 8th region.
    // Added as a minimal placeholder (same role EmberRavine's own pre-M9-Z/
    // pre-this-Slice `ember_ravine_outpost`-shaped stub played), unlocked
    // once EmberRavine completes. Content itself is out of scope here - a
    // later Slice fleshes it out, mirroring M9-K/L/Q/U/Y's established "add
    // the stub now, flesh it out later" split. This also resolves
    // Cooperation.cpp's `paired_signal_ward`'s own documented gap ("this
    // region has no enum value/region implementation anywhere in this
    // codebase") - see that file's own updated comment.
    BuriedDawnSanctum,
};

using LootId = std::string;
using DiscoveryId = std::string;

struct LootStack {
    LootId id;
    int quantity = 1;
};

// Outpost-wide development stages (docs/base_development.md "拠点全体の開拓段階").
// This is an access ceiling for facility nodes, not a technology itself.
enum class OutpostStage {
    Encampment = 0,      // 野営地
    PioneerOutpost = 1,  // 開拓拠点
    FrontierSettlement = 2, // 辺境集落
    PioneerCity = 3       // 開拓都市
};

// The 司令所 and 宿舎 initial nodes are core-loop facilities that never cost a
// facility slot and are available from the very start (docs/base_development.md
// "第1段階の施設枠"). Every other node - including their own branches - must
// be unlocked through GameApp::unlockFacilityNode().
inline std::unordered_set<std::string> initialUnlockedFacilityNodes() {
    return {"operations_tent", "communal_tent"};
}

// Region key material: bringing this home is what makes the outpost
// eligible to advance from Encampment to Pioneer Outpost (stage 1). Per the
// design doc, this material is reserved for outpost advancement and is not
// spent on weapon crafting. Declared here (ahead of BaseState) so
// BaseState::materialStorageCap() below can reference it.
inline constexpr const char* kAshveilFangMaterial = "ashveil_fang";
// docs/regions/ashbough_forest.md "折れ木の縄張り"'s 灰角の大牙 - a distinct key
// material from Cinderwatch's kAshveilFangMaterial (different creature,
// different region).
inline constexpr const char* kAshenhornFangMaterial = "ashenhorn_fang";

// docs/inventory_overflow.md「上限」: the only "探索道具・キー素材(1個上限)"
// ids that exist in code today are these two region key materials - listed
// here as a single place both BaseState::materialStorageCap() and any future
// key-material addition can extend.
inline std::array<const char*, 2> regionKeyMaterialIds() {
    return {kAshveilFangMaterial, kAshenhornFangMaterial};
}

// docs/inventory_overflow.md「受取保留」: what a safe return's reward grant
// couldn't fit into warehouse caps gets moved here instead of being silently
// discarded, converted, or auto-sold.
struct OverflowStack {
    // Lightweight per-return identifier (see GameApp::returnToBase()) - not
    // the full RewardGrantId/ExpeditionAttemptId ledger docs/expedition_rewards.md
    // specifies, since that ledger itself is still unimplemented
    // (docs/implementation_roadmap.md M1-D item 1, deferred as low-value while
    // only one region exists). Re-display safety instead comes from
    // returnToBase()'s existing `screen_ != Screen::Camp` guard, the same
    // pattern that already fixed a proceedToCamp() double-grant bug.
    std::string grantId;
    // LootId for materials, or "item:<ItemType int>" for consumables - a
    // namespace prefix keeps the two id spaces from ever colliding (ItemType
    // has no string form of its own; see SaveSystem.cpp's itemStorageToJson()
    // for the established "raw enum int" convention this mirrors).
    std::string itemId;
    int quantity = 0;
};

struct RewardOverflowState {
    std::vector<OverflowStack> stacks;
    // docs/inventory_overflow.md: "保留上限は合計200 Stack" - a safe return
    // that would push the total over this must not commit at all (see
    // GameApp::returnToBase()'s pre-check), so this is a hard ceiling read
    // before committing, not a truncation applied after.
    static constexpr int kMaxStacks = 200;
};

struct BaseState {
    std::vector<LootStack> storage;
    std::unordered_set<DiscoveryId> discoveryRegistry;
    OutpostStage outpostStage = OutpostStage::Encampment;

    // Permanent research record - once a node is here it stays forever
    // (docs/base_development.md: no dismantling exists).
    std::unordered_set<std::string> unlockedNodeIds = initialUnlockedFacilityNodes();
    // docs/facility_data_contract.md's `constructedFacilityIds`: the subset of
    // unlockedNodeIds restricted to FacilityNode::occupiesFacilitySlot nodes -
    // i.e. which of the 4 optional facilities have been built. Once a node is
    // in this set it never leaves (no dismantling); serialized under the
    // legacy JSON key "builtNodes" (SaveSystem.cpp), unaffected by this rename.
    std::unordered_set<std::string> constructedFacilityIds;

    // docs/exploration_system.md "周回と地域経路の開拓": permanent per-site
    // progression, keyed by Region::siteAccessKey(). Only ever raised via
    // GameApp::returnToBase() committing a safe return's earned promotion -
    // never mutated mid-expedition, never lowered.
    std::unordered_map<std::string, SiteAccessState> siteAccess;

    // docs/region_unlocks.md: the sole source of truth for which regions are
    // unlocked (regionUnlocked() in jf/core/Region.hpp). A region is added
    // here only when its own region-level completion is committed on a safe
    // return - never inferred from SiteAccessState, and never removed.
    std::unordered_set<RegionId> completedRegionIds;

    // docs/roster_design.md「加入処理の共通ルール」: a candidate recorded here
    // is "加入可能候補" - permanent once a safe return commits it (see
    // ExpeditionState::pendingRecruitCandidateIds), never removed even by a
    // later expedition's defeat. GameApp::confirmRecruitJoin() consumes an id
    // from this set (without removing it) to grant the actual join.
    std::unordered_set<std::string> joinReadyCandidateIds;
    // Permanent once GameApp::confirmRecruitJoin() commits the join - kept as
    // its own set (not inferred from roster_) per roster_design.md「加入可能
    // 候補IDと加入済みUnit IDを別の集合として保存する」.
    std::unordered_set<std::string> joinedRecruitIds;

    // docs/regions/cinderwatch_gate.md「地域の最低保証報酬」: a running tally of
    // how much of each material this region's sites have granted across every
    // expedition, independent of `storage` (which is spendable and can drop
    // below the floor via crafting). Only accumulated while CinderwatchGate
    // isn't in completedRegionIds yet; ExpeditionService.cpp's
    // applyExpeditionReturnToBase() reads this once, at the moment the region
    // completes, to top up any shortfall against the floor table.
    std::unordered_map<std::string, int> cinderwatchMaterialsEarned;
    // docs/regions/blackwater_lowlands.md「最低保証報酬」: same role/mechanism
    // as cinderwatchMaterialsEarned above, tracked independently per region.
    std::unordered_map<std::string, int> blackwaterMaterialsEarned;
    // docs/regions/windscar_plateau.md「最低保証報酬」: same role/mechanism as
    // cinderwatchMaterialsEarned/blackwaterMaterialsEarned above, tracked
    // independently per region.
    std::unordered_map<std::string, int> windscarMaterialsEarned;
    // docs/regions/old_frontier_settlement.md「最低保証報酬」: same mechanism as
    // cinderwatchMaterialsEarned/blackwaterMaterialsEarned/
    // windscarMaterialsEarned above, tracked independently per region.
    std::unordered_map<std::string, int> settlementMaterialsEarned;
    // docs/regions/ember_ravine.md「最低保証報酬」: same mechanism as
    // cinderwatchMaterialsEarned/blackwaterMaterialsEarned/
    // windscarMaterialsEarned/settlementMaterialsEarned above, tracked
    // independently per region.
    std::unordered_map<std::string, int> emberRavineMaterialsEarned;

    // docs/item_system.md "製作単位と倉庫上限": consumables owned but not
    // currently packed into an expedition bag - crafted via GameApp::
    // craftItem() (consumes storage materials), consumed into preparedBag_
    // via GameApp::addPreparedItem(), and refunded on removePreparedItem()
    // or whenever an expedition ends with the item still unused (see
    // GameApp::resetToBase()). Absent key = 0 owned, not "unknown".
    std::unordered_map<ItemType, int> itemStorage;

    int ownedItemCount(ItemType type) const {
        auto it = itemStorage.find(type);
        return it == itemStorage.end() ? 0 : it->second;
    }

    static constexpr int kItemStorageCap = 99;

    void addItemStorage(ItemType type, int quantity) {
        if (quantity <= 0) return;
        int& count = itemStorage[type];
        count = std::min(count + quantity, kItemStorageCap);
    }

    bool consumeItemStorage(ItemType type, int quantity) {
        if (quantity <= 0) return true;
        auto it = itemStorage.find(type);
        if (it == itemStorage.end() || it->second < quantity) return false;
        it->second -= quantity;
        return true;
    }

    int storageCount(const LootId& id) const {
        auto it = std::find_if(storage.begin(), storage.end(), [&](const LootStack& s) { return s.id == id; });
        return it == storage.end() ? 0 : it->quantity;
    }

    void addStorage(const LootId& id, int quantity) {
        if (quantity <= 0) return;
        auto it = std::find_if(storage.begin(), storage.end(), [&](const LootStack& s) { return s.id == id; });
        if (it == storage.end()) storage.push_back({id, quantity});
        else it->quantity += quantity;
    }

    bool consumeStorage(const LootId& id, int quantity) {
        if (quantity <= 0) return true;
        auto it = std::find_if(storage.begin(), storage.end(), [&](const LootStack& s) { return s.id == id; });
        if (it == storage.end() || it->quantity < quantity) return false;
        it->quantity -= quantity;
        if (it->quantity == 0) storage.erase(it);
        return true;
    }

    // Capacity-sensitive operations must preflight against these caps before
    // calling addStorage()/addItemStorage(). Safe return does that for both
    // secured materials and unused bag items; crafting refuses at the item cap.
    static constexpr int kMaterialStorageCap = 999;
    static constexpr int kKeyMaterialStorageCap = 1;

    RewardOverflowState rewardOverflow;

    // docs/inventory_overflow.md 「上限」: 探索道具・キー素材は1個上限。region-
    // specific key materials (brought home to unlock outpost stages) are the
    // only ids in that category that currently exist in code - see
    // eligibleForOutpostStage() below, which reads these same two ids.
    int materialStorageCap(const LootId& id) const {
        for (const char* keyId : regionKeyMaterialIds())
            if (id == keyId) return kKeyMaterialStorageCap;
        return kMaterialStorageCap;
    }

    // docs/roster_design.md「受け入れ枠」: joined-Roster capacity (not counting
    // pending candidates). First 3 stages (共同テント6人/宿舎増築I 8人/専門区画
    // 11人) are wired - 遠征別棟(12人)needs 灰鉄採石場の本格攻略+異常鉱脈記録
    // (a Discovery that doesn't exist in code yet, since M9's real Ashiron
    // Quarry content is still a placeholder); extend this switch when that
    // Slice adds them, same "add one more class at a time" pattern as M7項目1.
    // kFieldConstructionDiscovery's literal is duplicated here (rather than
    // referencing the named constant declared below) because that constant is
    // declared after this struct in the same header.
    int recruitCapacity() const {
        constexpr int kCommunalTentCapacity = 6;
        constexpr int kBarracksExtensionICapacity = 8;
        constexpr int kSpecialistWingCapacity = 11;
        if (discoveryRegistry.count("ironwatch_field_construction_records")) return kSpecialistWingCapacity;
        if (completedRegionIds.count(RegionId::AshboughForest)) return kBarracksExtensionICapacity;
        return kCommunalTentCapacity;
    }
};

// Command Tent's "Scout Network" branch discovery — secured by returning
// safely from the Cinderwatch Gate encounter (expedition stage 0).
inline constexpr const char* kCinderwatchReconDiscovery = "cinderwatch_recon_records";
// Infirmary's "Field Medicine" branch discovery — the Ironwatch Stores' records.
inline constexpr const char* kFieldMedicineDiscovery = "ironwatch_field_medicine_records";
// Workshop's "Return Signal" branch discovery — Signal Tower restoration.
inline constexpr const char* kReturnSignalDiscovery = "signal_tower_return_signal_records";
// docs/regions/cinderwatch_gate.md「地域の最低保証報酬」の軍旗記録: the "2人以上
// 撤退・降伏" trigger that would normally grant this is deferred (no roster-
// retreat-counting subsystem exists yet), so the region-completion floor
// top-up in ExpeditionService.cpp is currently its only grant path.
inline constexpr const char* kBannerRecordsDiscovery = "last_signal_banner_records";
// docs/roster_design.md「受け入れ枠」の専門区画(11人)条件、および
// docs/regions/cinderwatch_gate.md「3. アイアンウォッチ物資庫」工具庫確保の野戦工作記録
// (`CollapsedSidePath`ルート限定 - `ironwatch_stores`のroute Discoveries)。
inline constexpr const char* kFieldConstructionDiscovery = "ironwatch_field_construction_records";
// Infirmary's Field Infirmary build requirement (a herb-thicket location).
inline constexpr const char* kHerbThicketDiscovery = "herb_thicket_grounds";
// docs/regions/blackwater_lowlands.md「安定ID」: the region's 4 key records,
// same "region-completion floor top-up is the actual grant path" pattern as
// kBannerRecordsDiscovery above - none of the individual sites' own AND/
// trap/device infra exists to grant these precisely at the specific
// sub-objective the doc names (M9-F/-G/-H/-J's own documented gaps), so
// ExpeditionService.cpp's Blackwater floor top-up (mirroring Cinderwatch's)
// is where all 4 actually get granted, on the safe return that completes
// the region.
inline constexpr const char* kBlackwaterSurveyDiscovery = "blackwater_survey_records";
inline constexpr const char* kMarshPharmacologyDiscovery = "marsh_pharmacology_records";
inline constexpr const char* kMarshTrapcraftDiscovery = "marsh_trapcraft_records";
inline constexpr const char* kMarshEmergencyMedicineDiscovery = "marsh_emergency_medicine";
// docs/regions/windscar_plateau.md「安定ID」: same role as the Blackwater
// Discovery constants above - all 4 are actually granted through
// ExpeditionService.cpp's Windscar floor top-up on the region-completing
// safe return (per-site attainment of these isn't individually wired, same
// documented gap the Blackwater constants' own comment records).
inline constexpr const char* kWindscarRoadChartDiscovery = "windscar_road_chart";
inline constexpr const char* kCavalryOperationRecordsDiscovery = "cavalry_operation_records";
inline constexpr const char* kPlateauTargetingRecordsDiscovery = "plateau_targeting_records";
inline constexpr const char* kCourierRouteChartDiscovery = "courier_route_chart";
// docs/region_unlocks.md: Ashbough Forest's region-level completion mark.
// Registered as a Discovery (for UI/Discovery-registry visibility) in the
// same safe-return Transaction that adds RegionId::AshboughForest to
// completedRegionIds - the two are deliberately separate fields with
// separate responsibilities per that doc, even though they're set together.
// Nothing sets this yet: Ashbough Forest's real completion condition (per
// regions/ashbough_forest.md) requires defeating 灰角大猪 and securing all
// 3 locations, which don't exist in code until Phase 4.
inline constexpr const char* kAshboughForestSurveyCompleteDiscovery = "ashbough_forest_survey_complete";
// docs/regions/ashiron_quarry.md「3A. 旧採掘坑」/「4. 灰鉄鉱脈」(M9-T):
// 採掘技術記録/異常鉱脈記録, granted ad-hoc from GameApp::proceedToCamp()
// (same pattern as kWindscarRoadChartDiscovery above) rather than through a
// region-completion floor top-up - Ashiron Quarry doesn't have one yet
// (unlike Blackwater/Windscar's kBlackwaterSurveyDiscovery-style top-up),
// so the doc's "採掘技術記録は失敗しても地点5の作業台帳から代替取得できる"
// fallback is NOT modeled (documented gap - no such floor infra exists for
// this region).
inline constexpr const char* kMiningTechniqueRecordsDiscovery = "quarry_mining_technique_records";
inline constexpr const char* kAnomalousVeinRecordsDiscovery = "ashiron_anomalous_vein_records";
// docs/regions/old_frontier_settlement.md「2. 共同井戸」の「全員避難: 集落証言
// 記録」(M9-V): not listed in the doc's own「安定ID」table (that table only
// covers 恒久成果/地域攻略-scale ids), so this id was chosen to match the
// existing `<region>_..._records` Discovery naming convention (see
// kMiningTechniqueRecordsDiscovery above). Granted ad-hoc from
// GameApp::proceedToCamp() (same pattern as kMiningTechniqueRecordsDiscovery),
// same known display-name gap as kWindscarRoadChartDiscovery/
// kCourierRouteChartDiscovery/kMiningTechniqueRecordsDiscovery/
// kAnomalousVeinRecordsDiscovery above - none of these post-M9-T Discoveries
// are wired into ui_shared.cpp's discoveryNameFor(), so they display as their
// raw id rather than a localized name (pre-existing gap, not newly
// introduced here).
inline constexpr const char* kSettlementCommunalTestimonyDiscovery = "settlement_communal_testimony_records";
// docs/regions/old_frontier_settlement.md「安定ID」table: 集落台帳・援護命令 and
// 集団防衛・不動の構え respectively - each id is shared by 2 doc-listed reward
// rows (the doc's floor table lists them as 2 separate line items, but both
// map to the same underlying Discovery per the「安定ID」table, same "one id,
// dedup on re-grant" semantics every other Discovery constant already has).
// `settlement_command_ledger` itself is already granted directly as a raw
// string from `data/regions.json`'s `settlement_gathering_hall` entry
// (M9-X); this constant exists so the region-clear floor top-up (below) and
// GameApp's dawn-defense ad-hoc bonus block can reference the same id
// without retyping the literal.
inline constexpr const char* kSettlementCommandLedgerDiscovery = "settlement_command_ledger";
// docs/regions/old_frontier_settlement.md「5. 夜明けの共同防衛」の「井戸・穀物庫
// 保全: 集団防衛資料」: unreachable as ordinary Loot/Discovery victory reward
// (Object durability system doesn't exist - M6-C-era known gap, sub-condition
// 2 of the primary's 3-way AND is deferred entirely, see
// settlementDawnDefenseStage()'s own comment), but still granted via the
// region-clear floor top-up below (same "unreachable per-site, guaranteed at
// region-clear" shape as kMiningTechniqueRecordsDiscovery's own gap, just
// resolved by the floor instead of left fully stranded).
inline constexpr const char* kCollectiveDefenseRecordsDiscovery = "collective_defense_records";
// docs/regions/ember_ravine.md「6. 旧耐熱工房」の「記録箱2個: 特殊鍛造記録」
// (id from that doc's own「安定ID」table). Not expressible via a standard
// SurveySuccess RewardRule the way the primary's "1個以上" secondary
// (surveyObjectiveId/surveyTileCount) already is, because a single
// surveyObjectiveId group is always ObjectiveGroupRule::Any (satisfied by
// ANY 1 of N placed tiles - see M9-X's own investigation) and can't also
// express a stricter "ALL N" tier. Granted ad-hoc from GameApp's
// end-of-battle bonus block by checking every individual survey objective
// under `heatwork_shop_crate`'s group is Completed, the same
// ad-hoc-secondary-bonus pattern as kAnomalousVeinRecordsDiscovery above
// (just checking group-membership completion instead of
// creditedTargetIds.size()).
inline constexpr const char* kSpecialForgingRecordsDiscovery = "special_forging_records";
// docs/regions/ember_ravine.md「7. 灰封観測所」の「記録箱2個: 峡谷踏査記録、
// 灰嵐以前の監視記録」(id from that doc's own「安定ID」table - only
// 峡谷踏査 is listed there as `ember_ravine_survey_records`;「灰嵐以前の監視
// 記録」has no doc-listed id, so it follows the same `<region>_..._records`
// naming convention as kSpecialForgingRecordsDiscovery above). Same
// all-group-members-Completed ad-hoc check as kSpecialForgingRecordsDiscovery
// (「2個とも回収」bonus tier), just granting 2 Discoveries instead of 1 since
// the doc lists 2 distinct record names for the 2 crates. The primary's
// "1個以上を左端へ運ぶ" is NOT modeled as carrying to a zone - only
// "securing/touching" the crate is (surveyObjectiveId/surveyTileCount:2's
// ObjectiveGroupRule::Any, satisfied by any 1 of the 2 - same crate-primary
// simplification as ash_crystal_shelf/heatwork_shop above), with the actual
// primary objective approximated as standard EliminateTeam.
inline constexpr const char* kEmberRavineSurveyRecordsDiscovery = "ember_ravine_survey_records";
inline constexpr const char* kPreAshstormWatchRecordsDiscovery = "preashstorm_watch_records";
// docs/regions/ember_ravine.md「安定ID」table: 耐熱加工 ->
// `heat_resistant_processing_records`. Granted from 地点8「赤熱裂け目」's own
// "冷却弁2個: 耐熱加工記録を未取得なら追加" bonus tier (same
// creditedTargetIds.size()>=2 ad-hoc check as kSpecialForgingRecordsDiscovery
// above, this time over `redheat_fissure_valves`'s secondaryOperateObjectiveId
// group instead of a surveyObjectiveId group) and/or the region's own floor
// top-up if still missing at region-clear time.
inline constexpr const char* kHeatResistantProcessingRecordsDiscovery = "heat_resistant_processing_records";
// docs/regions/ember_ravine.md「安定ID」table: 上位戦闘工作 ->
// `advanced_fieldwork_records`. Per 地点6「旧耐熱工房」's own doc text
// ("炉扉保全: 上位戦闘工作記録"), this is gated on an Object-durability
// condition that this project doesn't implement (M6-C-era known gap) -
// individually unreachable, same as kAdvancedFieldworkRecordsDiscovery's own
// sibling `heat_resistant_processing_records`'s site-4 "水路保全" path. Both
// remain reachable only via this region's floor top-up at region-clear time.
inline constexpr const char* kAdvancedFieldworkRecordsDiscovery = "advanced_fieldwork_records";
// docs/regions/ember_ravine.md「安定ID」table: 制御燃焼式 ->
// `controlled_ember_formula`. Granted from 地点8「赤熱裂け目」's own副目標
// "味方の炎上状態0で終了" - same ad-hoc end-of-battle bonus pattern as
// deep_mire's own "毒状態の味方0" -> kMarshEmergencyMedicineDiscovery check
// (GameApp.cpp), scanning burnRemainingProcs instead of poisonRemainingProcs.
// Already referenced by Facilities.hpp's `craft_ember_focus` requiredDiscoveries
// (added by an earlier Slice), so granting this Discovery immediately
// unlocks that recipe - no Facilities.hpp change needed for this specific
// unlock.
inline constexpr const char* kControlledEmberFormulaDiscovery = "controlled_ember_formula";

// docs/regions/buried_dawn_sanctum.md「公開副目標」地点2「崩れた礼拝堂」の
// "避難者全員脱出 -> 野戦救護記録": the doc's own「安定ID」table doesn't list an
// id for this record (only the region's own permanent-outcome/altar-record
// ids are listed there), so this follows the same `<region-site>_..._records`
// naming convention as kEmberRavineSurveyRecordsDiscovery/
// kSpecialForgingRecordsDiscovery above. Granted via the same
// creditedTargetIds.size()>=2 ad-hoc check as blackwater_crossing's own "2人
// とも脱出" bonus (GameApp.cpp) - adapted here for 2 guests instead of 2
// workers, same "EscapeUnits objective, count-based secondary tier" shape.
inline constexpr const char* kFieldMedicalRecordsDiscovery = "collapsed_nave_field_medical_records";

// Forge tuning traits, tracked as an id rather than a free string so a typo
// can't silently create an unrecognized "equipped" trait.
enum class TuningTraitId {
    None,
    HideWrappedGrip
};

// String form is only for save-file/JSON stability; runtime code should
// pass TuningTraitId around, not strings.
inline std::string tuningTraitIdToString(TuningTraitId id) {
    return id == TuningTraitId::HideWrappedGrip ? "hide_wrapped_grip" : "";
}

inline TuningTraitId tuningTraitIdFromString(const std::string& value) {
    return value == "hide_wrapped_grip" ? TuningTraitId::HideWrappedGrip : TuningTraitId::None;
}

// Data-driven eligibility check (docs/base_development.md: "解放条件はUIに
// ハードコードせずデータから評価する") — the key material sitting in storage
// is what grants eligibility, independent of any UI action. Two independent
// regions can each unlock Pioneer Outpost: Cinderwatch Gate's Ashveil Fang,
// or Ashbough Forest's "灰角の大牙1、木材3" (docs/regions/ashbough_forest.md
// "開拓拠点への発展") - whichever the player reaches first.
inline bool eligibleForOutpostStage(const BaseState& base, OutpostStage next) {
    if (next == OutpostStage::PioneerOutpost) {
        return base.storageCount(kAshveilFangMaterial) > 0 ||
               (base.storageCount(kAshenhornFangMaterial) > 0 && base.storageCount("wood") >= 3);
    }
    return false; // Later stages' requirements are not defined yet.
}

} // namespace jf
