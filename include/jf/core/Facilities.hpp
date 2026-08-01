#pragma once

#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "jf/core/BaseState.hpp"
#include "jf/core/UnitClass.hpp"

namespace jf {

// The 6 facility categories from docs/base_development.md.
enum class FacilityId {
    CommandPost,     // 司令所
    TrainingGround,  // 訓練所
    Forge,           // 鍛冶場
    Infirmary,       // 診療所
    Workshop,        // 工房
    Barracks         // 宿舎
};

struct FacilityNode {
    std::string id;
    FacilityId facility;
    std::string nameEn;
    std::string nameJa;
    // M10-A (docs/deep_layers.md「分岐/Tier解放を『拠点段階ゲート』から
    // 『素材ゲート』へ変更」): every `craft_*` weapon-branch node and its
    // `*_forging` branch-line-unlock prerequisite is pinned to
    // OutpostStage::Encampment here (see facilityNodeRegistry() below) -
    // recipes are visible/attemptable from the moment a unit joins, and
    // what actually gates crafting them is materialCosts/requiredDiscoveries
    // (Tier2/3 materials still only exist in mid/late-game regions, so the
    // real-world unlock timing is close to unchanged - only the artificial
    // stage check is gone). Every other node (training tiers, facility
    // construction, non-weapon research) keeps its real requiredStage.
    OutpostStage requiredStage = OutpostStage::Encampment;
    std::vector<DiscoveryId> requiredDiscoveries;
    // Empty means no material cost. Multiple stacks are checked and consumed
    // atomically by GameApp before a node is unlocked.
    std::vector<LootStack> materialCosts;
    std::vector<std::string> prerequisiteNodeIds;
    // True only for the 4 optional stage-1 facilities (docs: "救護テント、訓練場、
    // 工作台、簡易鍛冶台から選んで建設する") - once built, permanently usable
    // (docs/base_development.md: "解体、素材返却、再建費は採用しない"). Branch
    // research nodes are never a facility themselves.
    bool occupiesFacilitySlot = false;
    // One-line, player-facing summary of what unlocking this node actually
    // does - shown in the Facilities screen's hover tooltip alongside the
    // material cost, per docs/base_development.md: "施設効果は原則として新しい
    // 行動、ルート、アイテム、編成判断を最低1つ追加する。"
    std::string effectEn;
    std::string effectJa;
    // Which class a `craft_*` weapon-branch recipe belongs to (docs/
    // base_development.md's per-class 武器分岐 tables). Meaningful only for
    // nodes whose id starts with "craft_" - unused/default on every other
    // node. Lets UI route any class with registered recipes through the
    // real Forge crafting panel instead of hardcoding one class
    // (docs/implementation_roadmap.md "M7項目3(残り) ...特性・武器分岐の
    // 他兵種一般化").
    UnitClass weaponBranchClass = UnitClass::MarchCaptain;
};

inline UnitClass weaponBranchClassFromFacilityJsonString(const std::string& name) {
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

inline FacilityId facilityIdFromJsonString(const std::string& name) {
    static const std::unordered_map<std::string, FacilityId> table = {
        {"CommandPost", FacilityId::CommandPost}, {"TrainingGround", FacilityId::TrainingGround},
        {"Forge", FacilityId::Forge},             {"Infirmary", FacilityId::Infirmary},
        {"Workshop", FacilityId::Workshop},       {"Barracks", FacilityId::Barracks},
    };
    auto it = table.find(name);
    return it != table.end() ? it->second : FacilityId::CommandPost;
}

inline OutpostStage outpostStageFromFacilityJsonString(const std::string& name) {
    if (name == "PioneerOutpost") return OutpostStage::PioneerOutpost;
    if (name == "FrontierSettlement") return OutpostStage::FrontierSettlement;
    if (name == "PioneerCity") return OutpostStage::PioneerCity;
    return OutpostStage::Encampment;
}

inline std::vector<FacilityNode> loadFacilityNodesFromJson() {
    std::vector<FacilityNode> nodes;
    // cwd is always the repo root at runtime, same convention as every other
    // data/*.json load (jf::loadGameData()'s dataDir, skillRegistry()'s own
    // loader) - see docs/implementation_status.md「データ/ロジック分離方針」.
    std::ifstream file("data/facilities.json");
    if (!file.is_open()) {
        std::cerr << "Failed to open data file: data/facilities.json" << std::endl;
        return nodes;
    }
    nlohmann::json parsed;
    try {
        file >> parsed;
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse JSON file data/facilities.json: " << e.what() << std::endl;
        return nodes;
    }
    for (const auto& n : parsed.at("facilityNodes")) {
        FacilityNode node;
        node.id = n.at("id").get<std::string>();
        node.facility = facilityIdFromJsonString(n.at("facility").get<std::string>());
        node.nameEn = n.at("nameEn").get<std::string>();
        node.nameJa = n.at("nameJa").get<std::string>();
        node.requiredStage = outpostStageFromFacilityJsonString(n.at("requiredStage").get<std::string>());
        for (const auto& d : n.at("requiredDiscoveries")) node.requiredDiscoveries.push_back(d.get<std::string>());
        for (const auto& m : n.at("materialCosts"))
            node.materialCosts.push_back({m.at("id").get<std::string>(), m.at("quantity").get<int>()});
        for (const auto& p : n.at("prerequisiteNodeIds")) node.prerequisiteNodeIds.push_back(p.get<std::string>());
        node.occupiesFacilitySlot = n.at("occupiesFacilitySlot").get<bool>();
        node.effectEn = n.at("effectEn").get<std::string>();
        node.effectJa = n.at("effectJa").get<std::string>();
        if (n.contains("weaponBranchClass"))
            node.weaponBranchClass = weaponBranchClassFromFacilityJsonString(n.at("weaponBranchClass").get<std::string>());
        nodes.push_back(std::move(node));
    }
    return nodes;
}

// Full node graph for all 6 facilities (docs/base_development.md section by
// section), data-driven from data/facilities.json (docs/implementation_
// status.md「データ/ロジック分離方針」) - this header keeps only the struct
// definition, the loader above, and the eligibility/lookup logic below.
inline const std::vector<FacilityNode>& facilityNodeRegistry() {
    static const std::vector<FacilityNode> nodes = loadFacilityNodesFromJson();
    return nodes;
}

inline const FacilityNode* findFacilityNode(const std::string& id) {
    for (const FacilityNode& node : facilityNodeRegistry()) {
        if (node.id == id) return &node;
    }
    return nullptr;
}

// Data-driven eligibility evaluation (docs/base_development.md: "解放条件はUIに
// ハードコードせずデータから評価する"). Covers stage, discoveries, material
// stock, and prerequisites (a branch's prerequisite facility must be actively
// built, not merely historically unlocked). No facility-slot cap - any number
// of the 4 optional facilities can be built in parallel once each affords its
// own materials (docs/base_development.md: "素材が足りれば4施設すべてを順次
// 建設できる").
inline bool facilityNodeEligible(const BaseState& base, const FacilityNode& node) {
    if (base.unlockedNodeIds.count(node.id)) return false;
    if (static_cast<int>(base.outpostStage()) < static_cast<int>(node.requiredStage)) return false;
    for (const DiscoveryId& discovery : node.requiredDiscoveries) {
        if (!base.discoveryRegistry.count(discovery)) return false;
    }
    for (const std::string& prereqId : node.prerequisiteNodeIds) {
        const FacilityNode* prereq = findFacilityNode(prereqId);
        bool satisfied = prereq && prereq->occupiesFacilitySlot ? base.constructedFacilityIds.count(prereqId) > 0
                                                                 : base.unlockedNodeIds.count(prereqId) > 0;
        if (!satisfied) return false;
    }
    for (const LootStack& cost : node.materialCosts) {
        if (base.storageCount(cost.id) < cost.quantity) return false;
    }
    return true;
}

} // namespace jf
