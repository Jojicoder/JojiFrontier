#pragma once

// M10-A: weapon Lv strengthening (docs/deep_layers.md「Lv制(武器・防具共通の
// 強化軸)」「Lv1〜5 数値バランス設計(本編区間)」). Covers only Lv1〜5 (本編
//区間, no deep-layer materials/system exists yet). Originally covered only
// the 18 branch weapons of the 6 initial classes
// (docs/character_progression.md「初期6兵種の武器レシピ」); M10-C extended
// registration to the remaining 6 classes' 18 branches (33+3 crafted total,
// = full 33-of-33 branch-weapon coverage - the "15 branches" figure once
// assumed for the remaining classes was stale, the actual count is 18) using
// the same generator unchanged (weaponLevelUpCost() simply returns empty for
// any weapon id not registered below).

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include "jf/core/Facilities.hpp"
#include "jf/core/UnitClass.hpp"

namespace jf {

// docs/regions/*.md's own material tables - existing established scarce
// material (docs/deep_layers.md: "多くの地域で副目標報酬としてのみ低頻度で
// 入手できる既存の稀少枠"), already granted from `data/regions.json`.
inline constexpr const char* kRareMaterialId = "rare_material";

// Per-class "other-region" secondary materials used to season Lv2〜5 costs
// (docs/deep_layers.md: "毎Lvに他地域素材を混ぜ...武器分岐と同じく地理的・
// 進行的に近い1〜2地域を個別に割り当てる"). Reused across all branch weapons
// of one class, per the doc's own worked example doing the same thing (its
// 号令剣/決闘剣/護衛剣 example only varies the Lv1 recipe, not otherA/otherB).
// 2026-07 material-economy pass: avoid common "can get it anywhere" staples
// as recurring Lv gates. Other-region picks now favor higher-risk site rewards
// and specialist materials rather than wood/iron/hide.
struct WeaponLevelMaterials {
    std::string otherA; // used at Lv2/Lv3
    std::string otherB; // used at Lv4 (Lv5 always uses kRareMaterialId instead)
};

inline const std::unordered_map<UnitClass, WeaponLevelMaterials>& weaponLevelMaterialsByClass() {
    static const std::unordered_map<UnitClass, WeaponLevelMaterials> table = {
        {UnitClass::MarchCaptain, {"quality_iron", "military_supplies"}},
        {UnitClass::VeteranGuard, {"quality_iron", "riding_gear"}},
        {UnitClass::WatchArcher, {"wetland_resin", "ash_crystal"}},
        {UnitClass::FrontierScout, {"riding_gear", "wetland_resin"}},
        {UnitClass::Spearman, {"quality_iron", "heat_resistant_material"}},
        {UnitClass::DawnChirurgeon, {"quality_herb", "sanctum_equipment"}},

        // M10-C: the 6 remaining classes (docs/character_progression.md
        // 「スキル取得地点の完全対応」table gives each class's Tier2/Tier3
        // unlock location, used here as the "own join/unlock region" anchor
        // per the same judgment method M10-A's own 6 picks used).
        //
        {UnitClass::HeavyInfantry, {"quality_iron", "ash_crystal"}},
        {UnitClass::FrontierEngineer, {"combustion_oil", "military_supplies"}},
        {UnitClass::MessengerCavalry, {"riding_gear", "military_supplies"}},
        {UnitClass::FrontierRanger, {"poison_material", "wetland_resin"}},
        {UnitClass::BannerBearer, {"military_supplies", "frontier_edge_material"}},
        {UnitClass::BattleMage, {"ash_crystal", "sanctum_equipment"}},
    };
    return table;
}

// Weapon id -> owning class, for the 18 branch weapons wired for Lv strength-
// ening so far. Also doubles as the "is this weapon id Lv-eligible" check.
inline const std::unordered_map<std::string, UnitClass>& weaponLevelEligibleWeapons() {
    static const std::unordered_map<std::string, UnitClass> table = {
        {"command_sword", UnitClass::MarchCaptain}, {"duel_sword", UnitClass::MarchCaptain},
        {"guard_sword", UnitClass::MarchCaptain},
        {"hook_lance", UnitClass::VeteranGuard}, {"fortress_lance", UnitClass::VeteranGuard},
        {"patrol_lance", UnitClass::VeteranGuard},
        {"long_watch_bow", UnitClass::WatchArcher}, {"war_bow", UnitClass::WatchArcher},
        {"pinning_bow", UnitClass::WatchArcher},
        {"trail_blade", UnitClass::FrontierScout}, {"ambush_blade", UnitClass::FrontierScout},
        {"withdrawal_blade", UnitClass::FrontierScout},
        {"long_spear", UnitClass::Spearman}, {"heavy_spear", UnitClass::Spearman},
        {"guard_spear", UnitClass::Spearman},
        {"mercy_staff", UnitClass::DawnChirurgeon}, {"ward_staff", UnitClass::DawnChirurgeon},
        {"march_staff", UnitClass::DawnChirurgeon},
        // M10-C: the 6 remaining classes' 18 branch weapons (confirmed 3
        // branches each, 6x3=18 total, from the "craft_*" FacilityNode
        // entries in Facilities.hpp - not 15 as a stale count once assumed).
        {"bulwark_maul", UnitClass::HeavyInfantry}, {"breaker_maul", UnitClass::HeavyInfantry},
        {"driving_maul", UnitClass::HeavyInfantry},
        {"builder_hammer", UnitClass::FrontierEngineer}, {"demolition_hammer", UnitClass::FrontierEngineer},
        {"repair_hammer", UnitClass::FrontierEngineer},
        {"road_sabre", UnitClass::MessengerCavalry}, {"charge_lance", UnitClass::MessengerCavalry},
        {"escort_blade", UnitClass::MessengerCavalry},
        {"snare_bow", UnitClass::FrontierRanger}, {"quarry_bow", UnitClass::FrontierRanger},
        {"driving_bow", UnitClass::FrontierRanger},
        {"far_standard", UnitClass::BannerBearer}, {"valor_standard", UnitClass::BannerBearer},
        {"warding_standard", UnitClass::BannerBearer},
        {"resonant_focus", UnitClass::BattleMage}, {"war_focus", UnitClass::BattleMage},
        {"ember_focus", UnitClass::BattleMage},
    };
    return table;
}

// Rounds to nearest int, floor 1 (a cost stack is never 0/negative).
inline int weaponLevelRoundedQuantity(double value) {
    return std::max(1, static_cast<int>(value + 0.5));
}

// Lv strengthening should be gated by specialist site rewards, not by the
// same staples used for basic crafting. Recipe costs remain readable at Lv1;
// repeated Lv2〜5 costs promote common materials into rarer equivalents.
inline std::string equipmentLevelMaterialId(const std::string& materialId, UnitClass unitClass) {
    if (materialId == "iron") {
        switch (unitClass) {
            case UnitClass::FrontierScout:
            case UnitClass::MessengerCavalry: return "riding_gear";
            case UnitClass::HeavyInfantry: return "heat_resistant_material";
            case UnitClass::FrontierEngineer: return "combustion_oil";
            case UnitClass::BannerBearer: return "military_supplies";
            case UnitClass::BattleMage: return "ash_crystal";
            case UnitClass::DawnChirurgeon: return "sanctum_equipment";
            default: return "quality_iron";
        }
    }
    if (materialId == "wood") {
        switch (unitClass) {
            case UnitClass::DawnChirurgeon: return "sanctum_equipment";
            case UnitClass::FrontierEngineer:
            case UnitClass::BannerBearer: return "military_supplies";
            case UnitClass::BattleMage: return "ash_crystal";
            default: return "hardwood";
        }
    }
    if (materialId == "hide") {
        switch (unitClass) {
            case UnitClass::FrontierRanger:
            case UnitClass::WatchArcher: return "wetland_resin";
            default: return "riding_gear";
        }
    }
    if (materialId == "herb") {
        return unitClass == UnitClass::DawnChirurgeon ? "sanctum_equipment" : "quality_herb";
    }
    if (materialId == "stone") {
        return unitClass == UnitClass::BattleMage ? "ash_crystal" : "ruin_fragment";
    }
    return materialId;
}

// docs/deep_layers.md「武器Lv2〜5」table, generalized: the weapon's own Tier1
// recipe (`craft_<weaponId>` FacilityNode::materialCosts) is Lv1's cost;
// materialCosts[0] is "primary", materialCosts[1] (if present) is
// "secondary". Returns empty if `weaponId` isn't in
// weaponLevelEligibleWeapons(), has no registered Tier1 recipe, or
// `targetLevel` isn't 2-5 (Lv6+ needs deep-layer materials, out of scope for
// this Slice - docs/deep_layers.md「実装への落とし込み」's own note that Lv6+
// uses "別の倍率テーブル...本編区間の倍率テーブルとは独立").
inline std::vector<LootStack> weaponLevelUpCost(const std::string& weaponId, int targetLevel) {
    if (targetLevel < 2 || targetLevel > 5) return {};
    auto classIt = weaponLevelEligibleWeapons().find(weaponId);
    if (classIt == weaponLevelEligibleWeapons().end()) return {};
    const FacilityNode* recipe = findFacilityNode("craft_" + weaponId);
    if (!recipe || recipe->materialCosts.empty()) return {};
    const LootStack& primary = recipe->materialCosts[0];
    const LootStack* secondary = recipe->materialCosts.size() > 1 ? &recipe->materialCosts[1] : nullptr;
    const UnitClass unitClass = classIt->second;
    const std::string primaryLevelMaterial = equipmentLevelMaterialId(primary.id, unitClass);
    const std::string secondaryLevelMaterial = secondary ? equipmentLevelMaterialId(secondary->id, unitClass) : "";
    const WeaponLevelMaterials& mats = weaponLevelMaterialsByClass().at(classIt->second);

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
            addOrMerge(primaryLevelMaterial, weaponLevelRoundedQuantity(primary.quantity * 1.0));
            addOrMerge(mats.otherA, 1);
            break;
        case 3:
            addOrMerge(primaryLevelMaterial, weaponLevelRoundedQuantity(primary.quantity * 1.5));
            addOrMerge(mats.otherA, 2);
            break;
        case 4:
            addOrMerge(primaryLevelMaterial, weaponLevelRoundedQuantity(primary.quantity * 1.5));
            if (secondary) addOrMerge(secondaryLevelMaterial, weaponLevelRoundedQuantity(secondary->quantity * 1.0));
            addOrMerge(mats.otherB, 1);
            break;
        case 5:
            addOrMerge(primaryLevelMaterial, weaponLevelRoundedQuantity(primary.quantity * 2.0));
            if (secondary) addOrMerge(secondaryLevelMaterial, weaponLevelRoundedQuantity(secondary->quantity * 1.0));
            addOrMerge(kRareMaterialId, 2);
            break;
    }
    return cost;
}

} // namespace jf
