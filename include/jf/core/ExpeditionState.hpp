#pragma once

#include <string>
#include <vector>

namespace jf {

// Loot found mid-expedition is only "pending" — it becomes permanent
// inventory only after a safe return to base. A defeat clears it outright.
struct ExpeditionState {
    std::vector<std::string> pendingLoot;
    int battlesWon = 0;

    void clearOnDefeat() { pendingLoot.clear(); }
};

} // namespace jf
