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

} // namespace jf
