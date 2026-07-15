#include "jf/core/Item.hpp"

namespace jf {

const ItemDefinition& itemDefinition(ItemType type) {
    for (const auto& item : kItemCatalog) {
        if (item.type == type) return item;
    }
    return kItemCatalog.front();
}

int healingAmount(ItemType type) {
    if (type == ItemType::FirstAidKit) return 20;
    if (type == ItemType::FieldTreatmentKit) return 10;
    return 0;
}

std::vector<ItemCraftCost> itemCraftCost(ItemType type) {
    switch (type) {
        case ItemType::FirstAidKit: return {{"herb", 2}, {"hide", 1}};
        case ItemType::FieldTreatmentKit: return {{"herb", 1}, {"wood", 1}};
        case ItemType::RescuePack: return {{"herb", 2}, {"hide", 2}};
        case ItemType::CampRations: return {{"wood", 2}, {"herb", 1}};
        case ItemType::ProtectiveBoard: return {{"wood", 3}};
        case ItemType::ReturnFlare: return {{"wood", 2}, {"hide", 2}};
    }
    return {};
}

} // namespace jf
