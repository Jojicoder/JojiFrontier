#pragma once

#include <algorithm>
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

struct BaseState {
    std::vector<LootStack> storage;
    std::unordered_set<DiscoveryId> discoveryRegistry;
    OutpostStage outpostStage = OutpostStage::Encampment;

    // Permanent research record - once a node is here it stays even if a
    // physical facility housing it is later dismantled.
    std::unordered_set<std::string> unlockedNodeIds = initialUnlockedFacilityNodes();
    // Which facility-slot nodes are currently physically built (subset of
    // unlockedNodeIds restricted to FacilityNode::occupiesFacilitySlot nodes).
    std::unordered_set<std::string> builtNodeIds;

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
};

// Facility-slot capacity by outpost stage (docs/base_development.md: "開拓拠点
// の有効施設枠は2... 次の拠点段階で有効枠を増やす"). Only the stage-1 value (2)
// is given explicitly; later stages extrapolate the same growth rate.
inline int facilitySlotCapacity(OutpostStage stage) {
    switch (stage) {
        case OutpostStage::Encampment: return 0;
        case OutpostStage::PioneerOutpost: return 2;
        case OutpostStage::FrontierSettlement: return 4;
        case OutpostStage::PioneerCity: return 6;
    }
    return 0;
}

// Command Tent's "Scout Network" branch discovery — secured by returning
// safely from the Cinderwatch Gate encounter (expedition stage 0).
inline constexpr const char* kCinderwatchReconDiscovery = "cinderwatch_recon_records";
// Infirmary's "Field Medicine" branch discovery — the Ironwatch Stores' records.
inline constexpr const char* kFieldMedicineDiscovery = "ironwatch_field_medicine_records";
// Workshop's "Return Signal" branch discovery — Signal Tower restoration.
inline constexpr const char* kReturnSignalDiscovery = "signal_tower_return_signal_records";
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

// Region key material: bringing this home is what makes the outpost
// eligible to advance from Encampment to Pioneer Outpost (stage 1). Per the
// design doc, this material is reserved for outpost advancement and is not
// spent on weapon crafting.
inline constexpr const char* kAshveilFangMaterial = "ashveil_fang";
// docs/regions/ashbough_forest.md "折れ木の縄張り"'s 灰角の大牙 - a distinct key
// material from Cinderwatch's kAshveilFangMaterial (different creature,
// different region).
inline constexpr const char* kAshenhornFangMaterial = "ashenhorn_fang";

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
