#pragma once

// M10-B: armor Lv strengthening (docs/deep_layers.md「Lv制」「防具Lv1〜5」).
// Mirrors include/jf/core/WeaponLeveling.hpp's generator shape exactly, with
// one structural difference: a weapon's Lv1 recipe already exists
// (character_progression.md「初期6兵種の武器レシピ」), but armor has none -
// docs/deep_layers.md「防具Lv1〜5」explicitly calls for a NEW Tier1 Lv1 recipe
// ("防具はTier1のLv1製作費自体を新規に設計する必要がある...武器Tier1より
// 軽めに設定"). This file supplies that Lv1 recipe (armorTier1CraftCost())
// alongside the same Lv2〜5 growth-formula generator weapons use.
//
// Covers only Lv1〜5 (本編区間). Originally covered only the 18 armor pieces
// of the 6 initial classes registered in jf::armorRegistry() (Armor.hpp);
// M10-C extended armorLevelMaterialsByClass() to the remaining 6 classes'
// 18 armor pieces, same generator unchanged - full 36-of-36 coverage.

#include <string>
#include <unordered_map>
#include <vector>

#include "jf/core/Armor.hpp"
#include "jf/core/BaseState.hpp"
#include "jf/core/UnitClass.hpp"
#include "jf/core/WeaponLeveling.hpp" // kRareMaterialId

namespace jf {

// Per-class "2nd/3rd/4th-region" materials seasoning armor Lv2〜5 costs
// (docs/deep_layers.md「防具Lv1〜5」: "Lv2以降すべてに他地域素材を混ぜる...
// 4種類の地域素材+レア枠"). 2026-07 material-economy pass: prefer
// higher-risk site rewards and specialist materials over ubiquitous staples.
struct ArmorLevelMaterials {
    std::string otherA; // Lv2/Lv3 (matches weapon's otherA - see comment above)
    std::string otherB; // Lv3/Lv4 (matches weapon's otherB)
    std::string otherC; // Lv4 only - armor-specific 4th material
};

inline const std::unordered_map<UnitClass, ArmorLevelMaterials>& armorLevelMaterialsByClass() {
    static const std::unordered_map<UnitClass, ArmorLevelMaterials> table = {
        {UnitClass::MarchCaptain, {"quality_iron", "military_supplies", "cloth"}},
        {UnitClass::VeteranGuard, {"quality_iron", "riding_gear", "military_supplies"}},
        {UnitClass::WatchArcher, {"wetland_resin", "ash_crystal", "hardwood"}},
        {UnitClass::FrontierScout, {"riding_gear", "wetland_resin", "quality_herb"}},
        {UnitClass::Spearman, {"quality_iron", "heat_resistant_material", "stone"}},
        {UnitClass::DawnChirurgeon, {"quality_herb", "sanctum_equipment", "cloth"}},

        // M10-C: the remaining 6 classes. otherA/otherB reuse
        // WeaponLeveling.hpp's weaponLevelMaterialsByClass() picks for the
        // same class (same simplification the first 6 used); otherC is a 4th
        // material drawn from that class's own weapon-branch recipes.
        {UnitClass::HeavyInfantry, {"quality_iron", "ash_crystal", "heat_resistant_material"}},
        {UnitClass::FrontierEngineer, {"combustion_oil", "military_supplies", "quality_iron"}},
        {UnitClass::MessengerCavalry, {"riding_gear", "military_supplies", "wetland_resin"}},
        {UnitClass::FrontierRanger, {"poison_material", "wetland_resin", "quality_herb"}},
        {UnitClass::BannerBearer, {"military_supplies", "frontier_edge_material", "rare_material"}},
        {UnitClass::BattleMage, {"ash_crystal", "sanctum_equipment", "ruin_fragment"}},
    };
    return table;
}

// Armor id -> owning ArmorDefinition, restricted to the 18 armor pieces
// registered so far. Doubles as the "is this armor id Lv-eligible" check.
inline const ArmorDefinition* armorLevelEligibleArmor(const std::string& armorId) {
    return findArmorDefinition(armorId);
}

inline int armorLevelRoundedQuantity(double value) {
    return std::max(1, static_cast<int>(value + 0.5));
}

// docs/deep_layers.md「防具Lv1〜5」table: Tier1's own worked example is
// "加入地域の主要素材1種2個" for Lv1 (armorTier1CraftCost() below), so unlike
// weapons (which have a pre-existing primary+secondary recipe), armor Lv1
// has exactly ONE material line - `primaryQuantity` below is always that
// single line's quantity (2, per the doc), doubling as every tier's Lv1 unit
// count (Tier2/3 slide the same formula onto that tier's own primary
// material, per "Tier2/3...同じ成長式をそのTierの解放時点で使える地域素材へ
// スライドさせて適用する").
constexpr int kArmorTier1LvOneQuantity = 2;

// Lv1 (製作) craft cost: 2 units of `primaryMaterialId` (the tier's own
// join-region/unlock-region primary material - see GameApp::armorTier1Primary()
// picks, one per (class, tier) pair, reusing each class's own weapon-branch
// Tier1/2/3 primary material per docs/deep_layers.md「製作要求素材」: "Tier
// 2/3も武器分岐と同じ解放地域・拠点段階に合わせる").
inline std::vector<LootStack> armorTier1CraftCost(const std::string& primaryMaterialId) {
    if (primaryMaterialId.empty()) return {};
    return {{primaryMaterialId, kArmorTier1LvOneQuantity}};
}

// docs/deep_layers.md「防具Lv1〜5」table, generalized exactly like
// weaponLevelUpCost(): `primaryMaterialId` is the armor's own Lv1 recipe
// material (armorTier1CraftCost()'s argument, recovered from the
// "craft_<armorId>" FacilityNode's materialCosts[0] by the caller - GameApp
// resolves it, since only Facilities.hpp's registry knows it). Returns empty
// if `armorId` isn't registered in armorRegistry(), `primaryMaterialId` is
// empty, or `targetLevel` isn't 2-5 (Lv6+ needs deep-layer materials, out of
// scope for this Slice - same "別の倍率テーブル" split as weapons).
inline std::vector<LootStack> armorLevelUpCost(const std::string& armorId, const std::string& primaryMaterialId,
                                               int targetLevel) {
    if (targetLevel < 2 || targetLevel > 5) return {};
    const ArmorDefinition* armor = armorLevelEligibleArmor(armorId);
    if (!armor || primaryMaterialId.empty()) return {};
    auto matIt = armorLevelMaterialsByClass().find(armor->unitClass);
    if (matIt == armorLevelMaterialsByClass().end()) return {};
    const ArmorLevelMaterials& mats = matIt->second;
    const std::string primaryLevelMaterial = equipmentLevelMaterialId(primaryMaterialId, armor->unitClass);

    std::vector<LootStack> cost;
    auto addOrMerge = [&](const std::string& id, int quantity) {
        if (quantity <= 0 || id.empty()) return;
        for (LootStack& stack : cost) {
            if (stack.id == id) {
                stack.quantity += quantity;
                return;
            }
        }
        cost.push_back({id, quantity});
    };

    switch (targetLevel) {
        case 2:
            addOrMerge(primaryLevelMaterial, armorLevelRoundedQuantity(kArmorTier1LvOneQuantity * 0.5));
            addOrMerge(mats.otherA, 1);
            break;
        case 3:
            addOrMerge(primaryLevelMaterial, armorLevelRoundedQuantity(kArmorTier1LvOneQuantity * 1.0));
            addOrMerge(mats.otherA, 1);
            addOrMerge(mats.otherB, 1);
            break;
        case 4:
            addOrMerge(primaryLevelMaterial, armorLevelRoundedQuantity(kArmorTier1LvOneQuantity * 1.0));
            addOrMerge(mats.otherA, 2);
            addOrMerge(mats.otherB, 1);
            addOrMerge(mats.otherC, 1);
            break;
        case 5:
            addOrMerge(primaryLevelMaterial, armorLevelRoundedQuantity(kArmorTier1LvOneQuantity * 1.0));
            addOrMerge(mats.otherA, 2);
            addOrMerge(mats.otherB, 1);
            addOrMerge(kRareMaterialId, 2);
            break;
    }
    return cost;
}

} // namespace jf
