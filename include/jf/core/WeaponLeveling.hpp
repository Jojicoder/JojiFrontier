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
// otherA = the doc's own MarchCaptain worked example (hide/marsh_resin, from
// Ashbough Forest / Blackwater Lowlands respectively - the two regions
// nearest Cinderwatch Gate where March Captain joins). The other 5 classes'
// picks follow the same "1-2 regions near this class's own join/unlock path"
// judgment call, documented per-class below.
struct WeaponLevelMaterials {
    std::string otherA; // used at Lv2/Lv3
    std::string otherB; // used at Lv4 (Lv5 always uses kRareMaterialId instead)
};

inline const std::unordered_map<UnitClass, WeaponLevelMaterials>& weaponLevelMaterialsByClass() {
    static const std::unordered_map<UnitClass, WeaponLevelMaterials> table = {
        // MarchCaptain: doc's own worked example verbatim (Ashbough hide,
        // then Blackwater marsh_resin).
        {UnitClass::MarchCaptain, {"hide", "marsh_resin"}},
        // VeteranGuard: Windscar Plateau's tack_material (already used by its
        // own Patrol Lance Tier3 recipe), then Old Frontier Settlement's cloth.
        {UnitClass::VeteranGuard, {"tack_material", "cloth"}},
        // WatchArcher: Blackwater's marsh_resin (already used by its own
        // Pinning Bow Tier3 recipe), then Ember Ravine's ruin_fragment.
        {UnitClass::WatchArcher, {"marsh_resin", "ruin_fragment"}},
        // FrontierScout: Windscar's tack_material (already used by its own
        // Withdrawal Blade Tier3 recipe), then Blackwater's quality_herb.
        {UnitClass::FrontierScout, {"tack_material", "quality_herb"}},
        // Spearman: Ashbough's hide, then Ashiron Quarry's stone (already used
        // by its own Heavy Spear Tier2 recipe).
        {UnitClass::Spearman, {"hide", "stone"}},
        // DawnChirurgeon: Blackwater's quality_herb (already used by its own
        // Ward Staff Tier2 recipe), then Old Frontier Settlement's cloth.
        {UnitClass::DawnChirurgeon, {"quality_herb", "cloth"}},

        // M10-C: the 6 remaining classes (docs/character_progression.md
        // 「スキル取得地点の完全対応」table gives each class's Tier2/Tier3
        // unlock location, used here as the "own join/unlock region" anchor
        // per the same judgment method M10-A's own 6 picks used).
        //
        // HeavyInfantry: Ashiron Quarry's stone (its Tier2 unlock 灰鉄鉱脈 is
        // in this region; stone is already the established Quarry material
        // used by Spearman's own Heavy Spear Tier3), then Ember Ravine's
        // ruin_fragment (its Tier3 unlock 破砕外郭 sits at the Quarry/Ember
        // Ravine collapse boundary).
        {UnitClass::HeavyInfantry, {"stone", "ruin_fragment"}},
        // FrontierEngineer: Windscar Plateau's tack_material (its Tier2
        // unlock 信号塔下層 is Windscar's own signal-tower substructure),
        // then Old Frontier Settlement's cloth (its Tier3 unlock 兵站庫 is a
        // Settlement supply depot).
        {UnitClass::FrontierEngineer, {"tack_material", "cloth"}},
        // MessengerCavalry: Windscar Plateau's tack_material (its own Tier1
        // Road Sabre recipe already leans on mount gear; its Tier2 unlock
        // 風見台 IS Windscar Plateau), then Blackwater's marsh_resin (its
        // Tier3 unlock 信号庭 sits toward Blackwater).
        {UnitClass::MessengerCavalry, {"tack_material", "marsh_resin"}},
        // FrontierRanger: Blackwater's marsh_resin (its Tier2 unlock 樹脂林
        // IS Blackwater's own resin forest), then Ashiron Quarry's stone
        // (its Tier3 unlock 石盆地 is a Quarry stone basin).
        {UnitClass::FrontierRanger, {"marsh_resin", "stone"}},
        // BannerBearer: Ember Ravine's ruin_fragment (its Tier3 unlock 最後の
        // 信号 sits in the Ember Ravine late-game reaches), then Blackwater's
        // quality_herb (its Tier3 unlock 予備壁 is Blackwater-adjacent).
        {UnitClass::BannerBearer, {"ruin_fragment", "quality_herb"}},
        // BattleMage: Ashiron Quarry's stone (its Tier2 unlock 灰鉄鉱脈 is
        // the same Quarry iron vein HeavyInfantry unlocks at), then Old
        // Frontier Settlement's cloth (its Tier3 unlock 写本庫 is a
        // Settlement archive/scriptorium).
        {UnitClass::BattleMage, {"stone", "cloth"}},
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
            addOrMerge(primary.id, weaponLevelRoundedQuantity(primary.quantity * 1.0));
            addOrMerge(mats.otherA, 1);
            break;
        case 3:
            addOrMerge(primary.id, weaponLevelRoundedQuantity(primary.quantity * 1.5));
            addOrMerge(mats.otherA, 2);
            break;
        case 4:
            addOrMerge(primary.id, weaponLevelRoundedQuantity(primary.quantity * 1.5));
            if (secondary) addOrMerge(secondary->id, weaponLevelRoundedQuantity(secondary->quantity * 1.0));
            addOrMerge(mats.otherB, 1);
            break;
        case 5:
            addOrMerge(primary.id, weaponLevelRoundedQuantity(primary.quantity * 2.0));
            if (secondary) addOrMerge(secondary->id, weaponLevelRoundedQuantity(secondary->quantity * 1.0));
            addOrMerge(kRareMaterialId, 2);
            break;
    }
    return cost;
}

} // namespace jf
