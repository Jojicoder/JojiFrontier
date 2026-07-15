#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace jf {

enum class ItemType {
    FirstAidKit,
    FieldTreatmentKit,
    RescuePack,
    CampRations,
    ProtectiveBoard,
    ReturnFlare
};

struct ItemDefinition {
    ItemType type;
    std::string_view name;
    std::string_view description;
};

inline constexpr std::array<ItemDefinition, 6> kItemCatalog{{
    {ItemType::FirstAidKit, "First Aid Kit", "Restore 20 HP at camp."},
    {ItemType::FieldTreatmentKit, "Field Treatment Kit", "Restore 10 HP in battle or camp."},
    {ItemType::RescuePack, "Rescue Pack", "Revive one ally at camp with 25% HP."},
    {ItemType::CampRations, "Camp Rations", "Restore 5 HP to every living ally at camp."},
    {ItemType::ProtectiveBoard, "Protective Board", "Place a barrier on an adjacent empty tile."},
    {ItemType::ReturnFlare, "Return Flare", "Secure loot and return from camp immediately."},
}};

const ItemDefinition& itemDefinition(ItemType type);
int healingAmount(ItemType type);

struct ItemCraftCost {
    std::string materialId;
    int quantity = 1;
};

// docs/item_system.md "製作単位と倉庫上限": crafting one consumable at a time,
// consuming materials into a permanent (per-ID, 99-cap) owned count separate
// from the expedition bag - see BaseState::itemStorage. Recipes use only
// Ashbough Forest's basic materials (wood/hide/herb) since these are the
// first items a new outpost can craft; no region-locked key material is
// spent on consumables.
std::vector<ItemCraftCost> itemCraftCost(ItemType type);

} // namespace jf
