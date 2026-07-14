#pragma once

#include <array>
#include <string_view>

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

} // namespace jf
