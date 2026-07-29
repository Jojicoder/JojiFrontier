// Debug tool: writes a fully-unlocked save into imports/, so a real play
// session (not jf_forest_balance's simulator) can be used to sanity-check
// balance without grinding through the whole campaign first. Not part of
// the shipped game - build target only, same role as forest_balance.cpp /
// class_duel.cpp.
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

#include "jf/core/ArmorLeveling.hpp"
#include "jf/core/BaseState.hpp"
#include "jf/core/Facilities.hpp"
#include "jf/core/Region.hpp"
#include "jf/core/SaveSystem.hpp"
#include "jf/data/GameData.hpp"

using namespace jf;

namespace {

constexpr RegionId kAllRegions[] = {
    RegionId::CinderwatchGate,     RegionId::AshboughForest,      RegionId::AshironQuarry,
    RegionId::BlackwaterLowlands,  RegionId::WindscarPlateau,     RegionId::OldFrontierSettlement,
    RegionId::EmberRavine,         RegionId::BuriedDawnSanctum,   RegionId::ShatteredMarchFort,
    RegionId::MappedEdge,
};

} // namespace

int main() {
    auto data = loadGameData("data");
    if (!data) data = loadGameData("../data");
    if (!data) {
        std::cerr << "Failed to load game data (tried data/ and ../data/)." << std::endl;
        return 1;
    }

    SaveData save;
    save.language = "en";
    BaseState& base = save.base;

    base.outpostLevel = BaseState::kMaxImplementedOutpostLevel;

    // Every facility node unlocked; the 4 optional slot facilities also
    // marked "constructed" (occupiesFacilitySlot) since that's the separate
    // flag the Facilities screen checks to render them as built.
    for (const FacilityNode& node : facilityNodeRegistry()) {
        base.unlockedNodeIds.insert(node.id);
        if (node.occupiesFacilitySlot) base.constructedFacilityIds.insert(node.id);
    }

    // Every region completed and every one of its sites Secured, so all
    // route/exploration content is reachable and the Base screen offers
    // every region as a destination.
    for (RegionId region : kAllRegions) {
        base.completedRegionIds.insert(region);
        RegionDescriptor descriptor = regionDescriptor(region, *data);
        for (const StageDescriptor& stage : descriptor.stages) {
            base.siteAccess[siteAccessKey(region, stage.id)] = SiteAccessState::Secured;
        }
    }

    // Every recruit-able unit already joined; party seeded with the first 4
    // (Import/applySaveData falls back to the current selection anyway if
    // this ever isn't exactly 4 valid ids).
    for (const auto& [id, unitTemplate] : data->recruitDefinitionsById) {
        base.joinedRecruitIds.insert(id);
        if (save.selectedPartyIds.size() < 4) save.selectedPartyIds.push_back(id);
    }

    // Every weapon/armor maxed out.
    for (const auto& [id, weapon] : data->weaponsById) base.weaponLevels[id] = BaseState::kMaxWeaponLevel;
    for (const ArmorDefinition& armor : armorRegistry()) base.armorLevels[armor.id] = BaseState::kMaxArmorLevel;

    // Every consumable item stacked to cap.
    for (const ItemDefinition& item : kItemCatalog) base.itemStorage[item.type] = BaseState::kItemStorageCap;

    // Generous stock of every material any facility node/weapon-or-armor
    // recipe ever costs, so nothing in the Facilities/Forge UI reads as
    // material-blocked either.
    for (const FacilityNode& node : facilityNodeRegistry()) {
        for (const LootStack& cost : node.materialCosts) {
            int& quantity = [&]() -> int& {
                for (LootStack& stack : base.storage) {
                    if (stack.id == cost.id) return stack.quantity;
                }
                base.storage.push_back({cost.id, 0});
                return base.storage.back().quantity;
            }();
            quantity = 999;
        }
    }

    std::string json = serializeSave(save);
    std::string outPath = "imports/full_unlock_debug.json";
    std::error_code mkdirError;
    std::filesystem::create_directories("imports", mkdirError);
    std::ofstream out(outPath, std::ios::trunc);
    if (!out) {
        std::cerr << "Failed to open " << outPath << " for writing." << std::endl;
        return 1;
    }
    out << json;
    out.close();

    std::cout << "Wrote " << outPath << " (" << json.size() << " bytes).\n"
              << "In-game: Settings -> Import, then pick full_unlock_debug.json." << std::endl;
    return 0;
}
