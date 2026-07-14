#pragma once

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "jf/core/Item.hpp"
#include "jf/core/BaseState.hpp"
#include "jf/core/Region.hpp"
#include "jf/core/RouteGraph.hpp"

namespace jf {

// Loot found mid-expedition is only "pending" — it becomes permanent
// inventory only after a safe return to base. A defeat clears it outright.
struct ExpeditionState {
    static constexpr std::size_t kBagCapacity = 6;

    // docs/implementation_roadmap.md "Phase 1.5": which region/stage this
    // expedition is currently in - replaces the old bare stage-int-only
    // model now that more than one region exists.
    RegionId regionId = RegionId::CinderwatchGate;
    std::vector<LootStack> pendingLoot;
    std::vector<DiscoveryId> pendingDiscoveries;
    std::vector<ItemType> bag;
    int battlesWon = 0;
    int stageIndex = 0;
    // Route-driven regions use this as their only progression source.
    // stageIndex remains for legacy Cinderwatch checkpoint compatibility.
    std::optional<RouteProgressSnapshot> routeProgress;
    // Site-access promotions earned this run (keyed by Region::
    // siteAccessKey()), only committed into BaseState::siteAccess on a safe
    // return - discarded outright on defeat, same as pendingLoot/
    // pendingDiscoveries (docs/exploration_system.md: "敗北した遠征の踏査・
    // 工作は恒久化しない").
    std::vector<std::pair<std::string, SiteAccessState>> pendingSiteAccessUpdates;
    // docs/region_mission_data_contract.md "PendingRegionCompletion": region
    // ids whose completion condition (every site Surveyed+) was met this
    // run, only committed into BaseState::completedRegionIds on a safe
    // return - same Pending-until-safe-return rule as everything else here.
    std::unordered_set<RegionId> pendingRegionCompletions;

    void clearOnDefeat() {
        pendingLoot.clear();
        pendingDiscoveries.clear();
        pendingSiteAccessUpdates.clear();
        pendingRegionCompletions.clear();
    }
    bool addItem(ItemType item) {
        if (bag.size() >= kBagCapacity) return false;
        bag.push_back(item);
        return true;
    }
    bool consume(ItemType item) {
        for (auto it = bag.begin(); it != bag.end(); ++it) {
            if (*it == item) {
                bag.erase(it);
                return true;
            }
        }
        return false;
    }
    int count(ItemType item) const {
        int result = 0;
        for (ItemType held : bag) result += held == item;
        return result;
    }
};

} // namespace jf
