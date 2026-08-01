#pragma once

// M10-B (docs/deep_layers.md「防具システム(新設)」): the armor equipment
// slot's static data model. Mirrors jf::Weapon's role (jf/core/Weapon.hpp)
// but deliberately smaller - per the design doc, armor's only combat effect
// is a DEF/RES addition ("武器分岐のような固有戦闘効果...は防具には持たせ
// ない"), so there is no branch-effect payload to carry the way Weapon does.
//
// Originally scoped to only the 6 initial classes' 3 tiers each (18 of the
// eventual 36 armor pieces - docs/deep_layers.md「兵種専用3防具」12兵種×3Tier),
// mirroring M10-A's own "18 of 33 weapons first" split
// (include/jf/core/WeaponLeveling.hpp's own doc comment). M10-C extended
// registration to the remaining 6 classes' 18 pieces below, using the same
// pattern unchanged - full 36-of-36 coverage.

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "jf/core/UnitClass.hpp"

namespace jf {

enum class ArmorTier {
    Tier1 = 1, // 汎用バランス型 (docs/deep_layers.md「兵種専用3防具」)
    Tier2 = 2, // DEF特化(RES低め)
    // Tier3 "状態異常耐性1つ付与" per the design doc's own flavor text - this
    // Slice implements Tier3 with the SAME DEF/RES-only mechanism as Tier1/2
    // (see this file's own top comment / docs/implementation_status.md「M10-B」
    // for why: no generic "grant a standing status-resistance" combat
    // mechanism exists yet, and building one just for this one Tier's flavor
    // is out of scope per the task's own explicit effort-vs-scope guidance).
    // The passive status-resistance effect itself is a documented, deferred
    // gap, not silently dropped.
    Tier3 = 3,
};

struct ArmorDefinition {
    std::string id;
    std::string nameEn;
    std::string nameJa;
    UnitClass unitClass = UnitClass::MarchCaptain;
    ArmorTier tier = ArmorTier::Tier1;
    // Lv1 (freshly-crafted, before any strengthening) DEF/RES bonus - see
    // armorDefBonusAtLevel()/armorResBonusAtLevel() below for how Lv2-15
    // scale from this base.
    int baseDef = 0;
    int baseRes = 0;
};

// docs/deep_layers.md「1Lvあたりの数値」: "防具はLvごとに...DEF/RES合計+1〜2"
// - this Slice uses the doc's own lower bound (total +1/Lv) so the eventual
// Lv15 total (base + 14) lands in the doc's own "Lv15で軽装DEF+RES合計+15前後"
// ballpark for the Tier1/3 (balanced) split, while Tier2's DEF-only growth
// (its own "DEF特化(RES低め)" flavor) reaches a proportionally higher single-
// stat total by Lv15 - a deliberate, documented asymmetry, not an oversight.
inline int armorDefBonusAtLevel(const ArmorDefinition& armor, int level) {
    int extra = std::max(0, level - 1);
    switch (armor.tier) {
        case ArmorTier::Tier2: return armor.baseDef + extra; // all growth goes to DEF
        case ArmorTier::Tier1:
        case ArmorTier::Tier3:
        default: return armor.baseDef + (extra + 1) / 2; // alternate DEF/RES, DEF first
    }
}

inline int armorResBonusAtLevel(const ArmorDefinition& armor, int level) {
    int extra = std::max(0, level - 1);
    switch (armor.tier) {
        case ArmorTier::Tier2: return armor.baseRes; // RES stays at its (low) Lv1 value
        case ArmorTier::Tier1:
        case ArmorTier::Tier3:
        default: return armor.baseRes + extra / 2;
    }
}

// docs/implementation_status.md「データ/ロジック分離方針」: データ本体は
// data/armor.json(36件、docs/deep_layers.md「兵種専用3防具」の12兵種×3Tier)
// へ切り出し済み。ここにはstruct定義・JSONローダー・参照ロジックだけを残す。
// Base DEF/RES: Tier1 balanced (1/1)、Tier2 DEF特化(2/0、"引き換えにRES低め")、
// Tier3 RES寄り(1/2)がデフォルトだが、VeteranGuard/Spearman/HeavyInfantryは
// Tier2が(3/0)、DawnChirurgeon/BannerBearer/BattleMageはTier3が(1/3)という
// per-class tweak(docs/implementation_status.md「防具設定レビュー」#3)が
// data/armor.jsonの数値そのものに反映されている。
inline UnitClass unitClassFromArmorJsonString(const std::string& name) {
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

inline ArmorTier armorTierFromJsonString(const std::string& name) {
    if (name == "Tier2") return ArmorTier::Tier2;
    if (name == "Tier3") return ArmorTier::Tier3;
    return ArmorTier::Tier1;
}

inline std::vector<ArmorDefinition> loadArmorFromJson() {
    std::vector<ArmorDefinition> armors;
    // cwd is always the repo root at runtime - same convention as every
    // other data/*.json load (see skillRegistry()/facilityNodeRegistry()).
    std::ifstream file("data/armor.json");
    if (!file.is_open()) {
        std::cerr << "Failed to open data file: data/armor.json" << std::endl;
        return armors;
    }
    nlohmann::json parsed;
    try {
        file >> parsed;
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse JSON file data/armor.json: " << e.what() << std::endl;
        return armors;
    }
    for (const auto& a : parsed.at("armor")) {
        ArmorDefinition def;
        def.id = a.at("id").get<std::string>();
        def.nameEn = a.at("nameEn").get<std::string>();
        def.nameJa = a.at("nameJa").get<std::string>();
        def.unitClass = unitClassFromArmorJsonString(a.at("unitClass").get<std::string>());
        def.tier = armorTierFromJsonString(a.at("tier").get<std::string>());
        def.baseDef = a.at("baseDef").get<int>();
        def.baseRes = a.at("baseRes").get<int>();
        armors.push_back(std::move(def));
    }
    return armors;
}

inline const std::vector<ArmorDefinition>& armorRegistry() {
    static const std::vector<ArmorDefinition> armors = loadArmorFromJson();
    return armors;
}

inline const ArmorDefinition* findArmorDefinition(const std::string& armorId) {
    for (const ArmorDefinition& armor : armorRegistry()) {
        if (armor.id == armorId) return &armor;
    }
    return nullptr;
}

} // namespace jf
