#pragma once

// M10-B (docs/deep_layers.md「防具システム(新設)」): the armor equipment
// slot's static data model. Mirrors jf::Weapon's role (jf/core/Weapon.hpp)
// but deliberately smaller - per the design doc, armor's only combat effect
// is a DEF/RES addition ("武器分岐のような固有戦闘効果...は防具には持たせ
// ない"), so there is no branch-effect payload to carry the way Weapon does.
//
// Scope of this Slice: only the 6 initial classes' 3 tiers each (18 of the
// eventual 36 armor pieces - docs/deep_layers.md「兵種専用3防具」12兵種×3Tier)
// are registered below, mirroring M10-A's own "18 of 33 weapons first" split
// (include/jf/core/WeaponLeveling.hpp's own doc comment). The remaining 6
// classes are deferred to a follow-up Slice using the same pattern.

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

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

// docs/deep_layers.md「兵種専用3防具」: the 18 armor pieces for the 6 initial
// classes (docs/character_progression.md「初期6兵種の武器レシピ」's own 6
// classes), 3 tiers each. Base DEF/RES: Tier1 balanced (1/1), Tier2 DEF-
// specialized (2/0, "引き換えにRES低め"), Tier3 same balanced shape as Tier1
// (see ArmorTier::Tier3's own comment on the deferred passive).
inline const std::vector<ArmorDefinition>& armorRegistry() {
    static const std::vector<ArmorDefinition> armors = {
        {"armor_march_captain_tier1", "March Captain Guard", "行軍隊長の護具", UnitClass::MarchCaptain,
         ArmorTier::Tier1, 1, 1},
        {"armor_march_captain_tier2", "March Captain Plate", "行軍隊長の重甲", UnitClass::MarchCaptain,
         ArmorTier::Tier2, 2, 0},
        {"armor_march_captain_tier3", "March Captain Ward", "行軍隊長の守甲", UnitClass::MarchCaptain,
         ArmorTier::Tier3, 1, 1},

        {"armor_veteran_guard_tier1", "Veteran Guard Mail", "古参守備兵の帷子", UnitClass::VeteranGuard,
         ArmorTier::Tier1, 1, 1},
        {"armor_veteran_guard_tier2", "Veteran Guard Bastion Plate", "古参守備兵の防塁甲", UnitClass::VeteranGuard,
         ArmorTier::Tier2, 2, 0},
        {"armor_veteran_guard_tier3", "Veteran Guard Sentinel Ward", "古参守備兵の哨戒守甲", UnitClass::VeteranGuard,
         ArmorTier::Tier3, 1, 1},

        {"armor_watch_archer_tier1", "Watch Archer Vest", "監視弓兵の胴衣", UnitClass::WatchArcher,
         ArmorTier::Tier1, 1, 1},
        {"armor_watch_archer_tier2", "Watch Archer Brigandine", "監視弓兵の鎖胴衣", UnitClass::WatchArcher,
         ArmorTier::Tier2, 2, 0},
        {"armor_watch_archer_tier3", "Watch Archer Marsh Ward", "監視弓兵の湿地守衣", UnitClass::WatchArcher,
         ArmorTier::Tier3, 1, 1},

        {"armor_frontier_scout_tier1", "Frontier Scout Wrap", "辺境斥候の巻き衣", UnitClass::FrontierScout,
         ArmorTier::Tier1, 1, 1},
        {"armor_frontier_scout_tier2", "Frontier Scout Hardleather", "辺境斥候の硬革衣", UnitClass::FrontierScout,
         ArmorTier::Tier2, 2, 0},
        {"armor_frontier_scout_tier3", "Frontier Scout Trailward", "辺境斥候の道守衣", UnitClass::FrontierScout,
         ArmorTier::Tier3, 1, 1},

        {"armor_spearman_tier1", "Spearman Cuirass", "槍兵の胸甲", UnitClass::Spearman, ArmorTier::Tier1, 1, 1},
        {"armor_spearman_tier2", "Spearman Bulwark Plate", "槍兵の防壁甲", UnitClass::Spearman, ArmorTier::Tier2,
         2, 0},
        {"armor_spearman_tier3", "Spearman Wall Ward", "槍兵の防壁守甲", UnitClass::Spearman, ArmorTier::Tier3,
         1, 1},

        {"armor_dawn_chirurgeon_tier1", "Dawn Chirurgeon Robe", "暁の衛生兵の外衣", UnitClass::DawnChirurgeon,
         ArmorTier::Tier1, 1, 1},
        {"armor_dawn_chirurgeon_tier2", "Dawn Chirurgeon Padded Robe", "暁の衛生兵の綿入り外衣",
         UnitClass::DawnChirurgeon, ArmorTier::Tier2, 2, 0},
        {"armor_dawn_chirurgeon_tier3", "Dawn Chirurgeon Sanctum Ward", "暁の衛生兵の聖堂守衣",
         UnitClass::DawnChirurgeon, ArmorTier::Tier3, 1, 1},
    };
    return armors;
}

inline const ArmorDefinition* findArmorDefinition(const std::string& armorId) {
    for (const ArmorDefinition& armor : armorRegistry()) {
        if (armor.id == armorId) return &armor;
    }
    return nullptr;
}

} // namespace jf
