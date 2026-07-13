#pragma once

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

namespace jf {

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

struct BaseState {
    std::vector<LootStack> storage;
    std::unordered_set<DiscoveryId> discoveryRegistry;
    OutpostStage outpostStage = OutpostStage::Encampment;

    int storageCount(const LootId& id) const {
        auto it = std::find_if(storage.begin(), storage.end(), [&](const LootStack& s) { return s.id == id; });
        return it == storage.end() ? 0 : it->quantity;
    }
};

// Command Tent's "Scout Network" branch discovery — secured by returning
// safely from the Cinderwatch Gate encounter (expedition stage 0).
inline constexpr const char* kCinderwatchReconDiscovery = "cinderwatch_recon_records";
// Infirmary's "Field Medicine" branch discovery — the Ironwatch Stores' records.
inline constexpr const char* kFieldMedicineDiscovery = "ironwatch_field_medicine_records";
// Workshop's "Return Signal" branch discovery — Signal Tower restoration.
inline constexpr const char* kReturnSignalDiscovery = "signal_tower_return_signal_records";

// Region key material: bringing this home is what makes the outpost
// eligible to advance from Encampment to Pioneer Outpost (stage 1). Per the
// design doc, this material is reserved for outpost advancement and is not
// spent on weapon crafting.
inline constexpr const char* kAshveilFangMaterial = "ashveil_fang";

// Data-driven eligibility check (docs/base_development.md: "解放条件はUIに
// ハードコードせずデータから評価する") — the key material sitting in storage
// is what grants eligibility, independent of any UI action.
inline bool eligibleForOutpostStage(const BaseState& base, OutpostStage next) {
    if (next == OutpostStage::PioneerOutpost) return base.storageCount(kAshveilFangMaterial) > 0;
    return false; // Later stages' requirements are not defined yet.
}

} // namespace jf
