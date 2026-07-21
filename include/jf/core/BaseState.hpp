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
    // pending candidates). Only the first 2 stages (共同テント6人/宿舎増築I 8人)
    // are wired so far - 専門区画(11人)/遠征別棟(12人) need Discovery/region-
    // completion signals that don't exist in code yet; extend this switch
    // when the next recruit's Slice adds them, same "add one more class at a
    // time" pattern as M7項目1.
    int recruitCapacity() const {
        constexpr int kCommunalTentCapacity = 6;
        constexpr int kBarracksExtensionICapacity = 8;
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
// Infirmary's Field Infirmary build requirement (a herb-thicket location).
inline constexpr const char* kHerbThicketDiscovery = "herb_thicket_grounds";
// docs/region_unlocks.md: Ashbough Forest's region-level completion mark.
// Registered as a Discovery (for UI/Discovery-registry visibility) in the
// same safe-return Transaction that adds RegionId::AshboughForest to
// completedRegionIds - the two are deliberately separate fields with
// separate responsibilities per that doc, even though they're set together.
// Nothing sets this yet: Ashbough Forest's real completion condition (per
// regions/ashbough_forest.md) requires defeating 灰角大猪 and securing all
// 3 locations, which don't exist in code until Phase 4.
inline constexpr const char* kAshboughForestSurveyCompleteDiscovery = "ashbough_forest_survey_complete";

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
