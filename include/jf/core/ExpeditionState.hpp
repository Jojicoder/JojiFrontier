#pragma once

#include <string>
#include <vector>

#include "jf/core/Item.hpp"
#include "jf/core/BaseState.hpp"

namespace jf {

// Loot found mid-expedition is only "pending" — it becomes permanent
// inventory only after a safe return to base. A defeat clears it outright.
struct ExpeditionState {
    static constexpr std::size_t kBagCapacity = 6;

    std::vector<LootStack> pendingLoot;
    std::vector<DiscoveryId> pendingDiscoveries;
    std::vector<ItemType> bag;
    int battlesWon = 0;
    int stage = 0;

    void clearOnDefeat() { pendingLoot.clear(); pendingDiscoveries.clear(); }
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
