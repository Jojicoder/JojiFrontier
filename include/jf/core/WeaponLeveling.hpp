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
#include <array>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "jf/core/DeepLayers.hpp"
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

// データ/ロジック分離方針(docs/implementation_status.md): 両テーブルの本体は
// data/weapon_leveling.json へ切り出し済み。equipmentLevelMaterialId()の
// per-class switch文はテーブル的な素材配分ではなく分岐ロジックそのものなので
// JSON化の対象外(コードのまま)。
inline UnitClass unitClassFromWeaponLevelingJsonString(const std::string& name) {
    static const std::unordered_map<std::string, UnitClass> table = {
        {"MarchCaptain", UnitClass::MarchCaptain},   {"VeteranGuard", UnitClass::VeteranGuard},
        {"WatchArcher", UnitClass::WatchArcher},     {"FrontierScout", UnitClass::FrontierScout},
        {"Spearman", UnitClass::Spearman},           {"DawnChirurgeon", UnitClass::DawnChirurgeon},
        {"HeavyInfantry", UnitClass::HeavyInfantry}, {"FrontierEngineer", UnitClass::FrontierEngineer},
        {"MessengerCavalry", UnitClass::MessengerCavalry}, {"FrontierRanger", UnitClass::FrontierRanger},
        {"BannerBearer", UnitClass::BannerBearer},   {"BattleMage", UnitClass::BattleMage},
    };
    auto it = table.find(name);
    return it != table.end() ? it->second : UnitClass::MarchCaptain;
}

inline nlohmann::json loadWeaponLevelingJson() {
    // cwd is always the repo root at runtime - same convention as every
    // other data/*.json load (see skillRegistry()/facilityNodeRegistry()).
    std::ifstream file("data/weapon_leveling.json");
    if (!file.is_open()) {
        std::cerr << "Failed to open data file: data/weapon_leveling.json" << std::endl;
        return nlohmann::json::object();
    }
    nlohmann::json parsed;
    try {
        file >> parsed;
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse JSON file data/weapon_leveling.json: " << e.what() << std::endl;
        return nlohmann::json::object();
    }
    return parsed;
}

inline const std::unordered_map<UnitClass, WeaponLevelMaterials>& weaponLevelMaterialsByClass() {
    static const std::unordered_map<UnitClass, WeaponLevelMaterials> table = [] {
        std::unordered_map<UnitClass, WeaponLevelMaterials> t;
        nlohmann::json parsed = loadWeaponLevelingJson();
        if (parsed.contains("materialsByClass")) {
            for (const auto& m : parsed.at("materialsByClass")) {
                UnitClass uc = unitClassFromWeaponLevelingJsonString(m.at("unitClass").get<std::string>());
                t[uc] = {m.at("otherA").get<std::string>(), m.at("otherB").get<std::string>()};
            }
        }
        return t;
    }();
    return table;
}

// Weapon id -> owning class, for the 36 branch weapons wired for Lv strength-
// ening so far. Also doubles as the "is this weapon id Lv-eligible" check.
inline const std::unordered_map<std::string, UnitClass>& weaponLevelEligibleWeapons() {
    static const std::unordered_map<std::string, UnitClass> table = [] {
        std::unordered_map<std::string, UnitClass> t;
        nlohmann::json parsed = loadWeaponLevelingJson();
        if (parsed.contains("eligibleWeapons")) {
            for (const auto& e : parsed.at("eligibleWeapons")) {
                t[e.at("weaponId").get<std::string>()] =
                    unitClassFromWeaponLevelingJsonString(e.at("unitClass").get<std::string>());
            }
        }
        return t;
    }();
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
// weaponLevelEligibleWeapons(), has no registered Tier1 recipe, `targetLevel`
// is outside 2-20, or (for Lv6+) the weapon's class has no deep-layer region
// wired yet (see DeepLayers.hpp - only FrontierScout/AshboughForest exists so
// far, per docs/deep_layers.md「実装順序案」#5's "1地域だけ先に縦通し"
// scoping). Lv6+ uses a separate, deep-material-only formula per docs/
// deep_layers.md「実装への落とし込み」's own note that Lv6+ "別の倍率
// テーブル...本編区間の倍率テーブルとは独立" - see weaponDeepLevelUpCost()
// below.
inline std::vector<LootStack> weaponDeepLevelUpCost(UnitClass unitClass, int targetLevel) {
    std::optional<std::string> deepMaterial = deepMaterialIdForClass(unitClass);
    if (!deepMaterial || targetLevel < 6 || targetLevel > 20) return {};
    // Simple linear ramp (docs/deep_layers.md「深層限定素材の構成」2026-07-31
    // 設計レビュー: 最大LvをLv15→Lv25→Lv20で確定。ダンジョン構成も「本編/深層/
    // 最深層」の2段階遠征へ整理したが、素材のユニーク性は3種のまま維持する
    // (深層内の2体のボス+最深層1体、計3種のボス素材)) - provisional,
    // same "jf_forest_balance実測後に調整する前提の暫定値" caveat the rest of
    // this doc's own tables carry.
    std::vector<LootStack> cost = {{*deepMaterial, 2 + (targetLevel - 6)}};
    // 本来の設計(docs/deep_layers.md「Lv帯と対応コンテンツ」2026-08-01訂正)は
    // Lv9/13/20を「クリア済み深層地域数」基準のチェックポイントにする
    // (Lv9=深層前半5地域、Lv13=深層後半5地域+地図外苑攻略後、Lv20=最深層
    // 10地域+地図外苑攻略後)。ただし深層/最深層は灰枝の森(AshboughForest)
    // 1地域しか実装されておらず、地域数を数える仕組み自体がまだ無いため、
    // ここでは代わりに「灰枝の森自身のボス3体(Lv9/13=深層内2体、Lv20=最深層
    // 1体)を倒さないと手に入らないユニーク素材」を要求する暫定コードに
    // なっている。これは地域数ベース設計の実現ではなく仮の代替実装 - 他地域の
    // 深層を追加するタイミングで、実際の「クリア済み深層地域数」を数える形へ
    // このガード自体を書き換えること。
    if (targetLevel == 9 || targetLevel == 13 || targetLevel == 20) {
        if (std::optional<std::array<std::string, 3>> bossMats = layerBossMaterialIdsForClass(unitClass)) {
            const std::size_t layerIndex = targetLevel == 9 ? 0 : targetLevel == 13 ? 1 : 2;
            cost.push_back({(*bossMats)[layerIndex], 1});
        }
    }
    // TODO(他地域深層の横展開時): docs/deep_layers.md「他地域深層素材の混合
    // 要求」2026-08-01 - Lv2〜5が「毎Lvに他地域の本編素材を1種混ぜる」のと
    // 同じ考え方で、ここにも「別地域の深層素材を少量追加要求する」項を足す
    // 方針が決まっている(自地域の`deepMaterial`/ボス素材を代用するのでは
    // なく、追加でもう1種混ぜる形)。現状は灰枝の森(AshboughForest)しか
    // 深層が実装されておらず「混ぜる相手」が存在しないため未実装 - 2地域目
    // 以降の深層を追加するタイミングで、混ぜる節目Lv・相手地域の選び方・
    // 数量を決めてここへ追加すること。
    return cost;
}

inline std::vector<LootStack> weaponLevelUpCost(const std::string& weaponId, int targetLevel) {
    if (targetLevel < 2 || targetLevel > 20) return {};
    auto classIt = weaponLevelEligibleWeapons().find(weaponId);
    if (classIt == weaponLevelEligibleWeapons().end()) return {};
    if (targetLevel >= 6) return weaponDeepLevelUpCost(classIt->second, targetLevel);
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
