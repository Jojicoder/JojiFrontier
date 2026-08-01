#pragma once

// docs/deep_layers.md「深層限定素材」: 深層(本編クリア後の任意周回コンテンツ、
// 「深層」「最深層」の2遠征構成)は地域ごとに専用素材を用意し、武器/防具
// Lv6〜20の専用材料として使う(Lv2〜5は本編素材だけで完結するため、この材料が
// 要る時点でLv6以降だと分かる)。
//
// ユーザー方針(2026-07-31、設計レビュー):
// - 深層素材は地域(ダンジョン)ごとに別物で、他地域の素材で代用できない
//   (でなければ「最初に着手した1地域だけ周回すればいい」になってしまい、
//   他ダンジョンへ行く意味が無くなる)。
// - 各地域は2階建ての素材を持つ: 通常戦闘で入手でき、進むほど入手量が増える
//   「共通深層素材」1種と、ボスを倒さないと手に入らない「ボス素材」3種
//   (深層内2体+最深層1体、ユニーク)。
//
// 現状は「1地域だけ先に縦通しを作る」というユーザー方針に基づき、灰枝の森
// (AshboughForest/FrontierScout)の1エントリのみ登録している。他地域を
// 横展開する際は、data/deep_layers.jsonへエントリを追加するだけでよい
// (weaponLevelUpCost()/armorLevelUpCost()側のLv6〜20ロジックは兵種を問わず
// 共通 - 対応するエントリが無ければそのままLv6+のコストが空になる=
// 従来通り「このSliceでは未実装」という扱いのまま)。

#include <array>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "jf/core/UnitClass.hpp"

namespace jf {

// docs/deep_layers.md「他地域深層素材の混合要求」/
// docs/prompts/equipment_other_region_materials_prompt.md(2026-08-01設計):
// Lv6〜20のうちボス撃破チェックポイントではない3つのLvで、自地域の深層素材とは
// 別に他地域の深層共通素材を少量追加要求する。
struct OtherDeepMaterialRequirement {
    int targetLevel = 0;
    std::string materialId;
    int quantity = 1;
};

struct DeepLayerRegionMaterials {
    std::string deepMaterialId;
    std::array<std::string, 3> layerBossMaterialIds; // index 0/1 = 深層内2体, 2 = 最深層
    std::vector<OtherDeepMaterialRequirement> otherDeepMaterials;
};

namespace deep_layers_detail {

inline UnitClass unitClassFromDeepLayersJsonString(const std::string& name) {
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

} // namespace deep_layers_detail

inline const std::unordered_map<UnitClass, DeepLayerRegionMaterials>& deepLayerMaterialsByClass() {
    static const std::unordered_map<UnitClass, DeepLayerRegionMaterials> table = [] {
        std::unordered_map<UnitClass, DeepLayerRegionMaterials> t;
        // cwd is always the repo root at runtime - same convention as every
        // other data/*.json load.
        std::ifstream file("data/deep_layers.json");
        if (!file.is_open()) {
            std::cerr << "Failed to open data file: data/deep_layers.json" << std::endl;
            return t;
        }
        nlohmann::json parsed;
        try {
            file >> parsed;
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse JSON file data/deep_layers.json: " << e.what() << std::endl;
            return t;
        }
        for (const auto& r : parsed.at("regions")) {
            UnitClass uc = deep_layers_detail::unitClassFromDeepLayersJsonString(r.at("classId").get<std::string>());
            DeepLayerRegionMaterials mats;
            mats.deepMaterialId = r.at("deepMaterialId").get<std::string>();
            const auto& bossIds = r.at("layerBossMaterialIds");
            for (std::size_t i = 0; i < 3 && i < bossIds.size(); ++i)
                mats.layerBossMaterialIds[i] = bossIds[i].get<std::string>();
            if (r.contains("otherDeepMaterials")) {
                for (const auto& req : r.at("otherDeepMaterials")) {
                    mats.otherDeepMaterials.push_back(
                        {req.at("targetLevel").get<int>(), req.at("materialId").get<std::string>(),
                         req.at("quantity").get<int>()});
                }
            }
            t[uc] = std::move(mats);
        }
        return t;
    }();
    return table;
}

// Empty when `unitClass` has no deep-layer region wired yet (every class
// except FrontierScout, until later regions are horizontally expanded per
// docs/deep_layers.md「実装順序案」#6) - callers treat this the same as the
// pre-existing "Lv6+ not implemented for this weapon/armor" empty-cost case.
inline std::optional<std::string> deepMaterialIdForClass(UnitClass unitClass) {
    auto it = deepLayerMaterialsByClass().find(unitClass);
    if (it == deepLayerMaterialsByClass().end()) return std::nullopt;
    return it->second.deepMaterialId;
}

// The class's region's 3 unique mid-boss materials (index 0/1 = 深層内2体,
// 2 = 最深層), or nullopt if no deep-layer region is wired for this class yet.
inline std::optional<std::array<std::string, 3>> layerBossMaterialIdsForClass(UnitClass unitClass) {
    auto it = deepLayerMaterialsByClass().find(unitClass);
    if (it == deepLayerMaterialsByClass().end()) return std::nullopt;
    return it->second.layerBossMaterialIds;
}

// docs/deep_layers.md「他地域深層素材の混合要求」: the class's region's Lv6〜20
// "other region" mix-in requirements (non-boss-checkpoint Lv only). Empty
// vector (not nullopt) when the class has a deep-layer region wired but no
// entries were authored for it yet.
inline const std::vector<OtherDeepMaterialRequirement>& otherDeepMaterialsForClass(UnitClass unitClass) {
    static const std::vector<OtherDeepMaterialRequirement> kEmpty;
    auto it = deepLayerMaterialsByClass().find(unitClass);
    if (it == deepLayerMaterialsByClass().end()) return kEmpty;
    return it->second.otherDeepMaterials;
}

} // namespace jf
