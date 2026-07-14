#pragma once

#include <string>
#include <vector>

#include "jf/core/BaseState.hpp"

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
    OutpostStage requiredStage = OutpostStage::Encampment;
    std::vector<DiscoveryId> requiredDiscoveries;
    // Empty means no material cost. Multiple stacks are checked and consumed
    // atomically by GameApp before a node is unlocked.
    std::vector<LootStack> materialCosts;
    std::vector<std::string> prerequisiteNodeIds;
    // True only for the 4 optional stage-1 facilities (docs: "救護テント、訓練場、
    // 工作台、簡易鍛冶台から選んで建設する") that occupy a limited facility slot
    // and can be built/dismantled. Branch research nodes never occupy a slot.
    bool occupiesFacilitySlot = false;
    // One-line, player-facing summary of what unlocking this node actually
    // does - shown in the Facilities screen's hover tooltip alongside the
    // material cost, per docs/base_development.md: "施設効果は原則として新しい
    // 行動、ルート、アイテム、編成判断を最低1つ追加する。"
    std::string effectEn;
    std::string effectJa;
};

// Full node graph for all 6 facilities, matching docs/base_development.md
// section by section. Costs not given explicit numbers in the design doc use
// a single unit of a thematically-fitting material already collected via the
// 3 mission stages, since the doc frames exact balancing as data to tune
// later ("解放条件はUIにハードコードせずデータから評価する") rather than
// something fixed by the UI/code.
inline const std::vector<FacilityNode>& facilityNodeRegistry() {
    static const std::vector<FacilityNode> nodes = {
        // 1. 司令所 (Command Post)
        {"operations_tent", FacilityId::CommandPost, "Operations Tent", "作戦テント",
         OutpostStage::Encampment, {}, {}, {}, false,
         "Choose an expedition route and preview basic danger.",
         "遠征先を選択し、基本的な危険度を表示する。"},
        {"scout_network", FacilityId::CommandPost, "Scout Network", "偵察網",
         OutpostStage::PioneerOutpost, {kCinderwatchReconDiscovery}, {{"watch_ledger", 1}},
         {"operations_tent"}, false,
         "Shows enemy composition and starting positions before you choose a route.",
         "ルート選択前に敵の種類と初期配置を表示する。"},
        {"map_room", FacilityId::CommandPost, "Map Room", "地図室",
         OutpostStage::PioneerOutpost, {}, {{"ash_road_map", 1}}, {"operations_tent"}, false,
         "Reveals part of the route branches and the rough distance to the boss.",
         "ルート分岐の一部とボスまでのおおよその距離を表示する。"},
        {"expedition_planning_room", FacilityId::CommandPost, "Expedition Planning Room", "遠征計画室",
         OutpostStage::FrontierSettlement, {kReturnSignalDiscovery}, {{"captains_seal", 1}},
         {"operations_tent"}, false,
         "Shows deep-region intel and unlocks special expeditions.",
         "深層情報を表示し、特殊遠征を解放する。"},

        // 2. 訓練所 (Training Ground)
        {"training_field", FacilityId::TrainingGround, "Training Field", "訓練場",
         OutpostStage::PioneerOutpost, {}, {{"wood", 3}, {"hide", 2}}, {}, true,
         "Enables the basic classes and their base class techniques.",
         "基礎兵種と基本兵種技を使用可能にする。"},
        {"vanguard_training", FacilityId::TrainingGround, "Vanguard Training", "前衛訓練",
         OutpostStage::PioneerOutpost, {}, {}, {"training_field"}, false,
         "Unlocks Veteran Guard, Spearman, and other heavy-vanguard techniques.",
         "古参兵・槍兵・重装兵の技術を解放する。"},
        {"mobility_training", FacilityId::TrainingGround, "Mobility Training", "機動訓練",
         OutpostStage::PioneerOutpost, {kCinderwatchReconDiscovery}, {}, {"training_field"}, false,
         "Unlocks Frontier Scout and other light-cavalry techniques.",
         "斥候・猟兵・伝令騎兵の技術を解放する。"},
        {"specialist_training", FacilityId::TrainingGround, "Specialist Training", "専門訓練",
         OutpostStage::FrontierSettlement, {}, {}, {"training_field"}, false,
         "Unlocks engineer, standard-bearer, and other advanced class branches.",
         "工兵・旗手など上位兵種分岐を解放する。"},

        // 3. 鍛冶場 (Forge)
        {"simple_forge", FacilityId::Forge, "Simple Forge", "簡易鍛冶台",
         OutpostStage::PioneerOutpost, {}, {{"wood", 2}, {"hide", 1}}, {}, true,
         "Lets you change equipped weapons and view/craft weapon branch recipes.",
         "武器の装備変更と、武器分岐レシピの閲覧・製作を可能にする。"},
        {"weapon_forging", FacilityId::Forge, "Weapon Forging", "武器鍛造",
         OutpostStage::PioneerOutpost, {}, {}, {"simple_forge"}, false,
         "Unlocks Iron Spear's branch weapon recipes (Long/Heavy/Guard Spear).",
         "鉄の槍の分岐武器レシピ(長槍・重槍・迎撃槍)を解放する。"},
        {"heavy_reinforcement", FacilityId::Forge, "Heavy Reinforcement", "重装加工",
         OutpostStage::PioneerOutpost, {}, {}, {"simple_forge"}, false,
         "Unlocks armor reinforcement and knockback-resistant equipment.",
         "防具強化とノックバック耐性装備を解放する。"},
        {"special_material_crafting", FacilityId::Forge, "Special Material Crafting", "特殊素材加工",
         OutpostStage::FrontierSettlement, {kReturnSignalDiscovery}, {}, {"simple_forge"}, false,
         "Unlocks weapons crafted from monster and ruin materials.",
         "魔物素材武器・遺跡素材武器を解放する。"},
        {"craft_long_spear", FacilityId::Forge, "Craft: Long Spear", "製作: 長槍",
         OutpostStage::PioneerOutpost, {}, {{"ash_road_map", 1}}, {"weapon_forging"}, false,
         "Ranged branch: max range +1, might -2.",
         "射程型分岐: 最大射程+1、威力-2。"},
        {"craft_heavy_spear", FacilityId::Forge, "Craft: Heavy Spear", "製作: 重槍",
         OutpostStage::PioneerOutpost, {}, {{"gate_tools", 1}}, {"weapon_forging"}, false,
         "Power branch: might +2, MOV -1, attacks knock the defender back one tile.",
         "火力型分岐: 威力+2、MOV-1、攻撃命中時に相手を1マスノックバック。"},
        {"craft_guard_spear", FacilityId::Forge, "Craft: Guard Spear", "製作: 迎撃槍",
         OutpostStage::PioneerOutpost, {}, {{"watch_ledger", 1}}, {"weapon_forging"}, false,
         "Interception branch: stronger Brace bonus, might -1.",
         "迎撃型分岐: 迎撃姿勢強化、通常威力-1。"},
        {"trait_hide_wrapped_grip", FacilityId::Forge, "Tuning: Hide-Wrapped Grip", "調整: 獣皮の柄巻き",
         OutpostStage::PioneerOutpost, {}, {{"hide", 1}}, {"simple_forge"}, false,
         "Negates the first knockback the wearer receives each battle.",
         "戦闘ごとに最初に受けるノックバックを1回無効化する。"},

        // 4. 診療所 (Infirmary)
        {"field_infirmary", FacilityId::Infirmary, "Field Infirmary", "救護テント",
         OutpostStage::PioneerOutpost, {kHerbThicketDiscovery}, {{"wood", 2}, {"herb", 2}}, {}, true,
         "Unlocks the First Aid Kit and Rescue Pack.",
         "応急手当具と救命包を解放する。"},
        {"field_medicine_branch", FacilityId::Infirmary, "Field Medicine", "野戦医療",
         OutpostStage::PioneerOutpost, {kFieldMedicineDiscovery}, {}, {"field_infirmary"}, false,
         "Unlocks the Field Treatment Kit and more advanced treatment gear.",
         "野戦治療具・高度治療具を解放する。"},
        {"lifesaving_technique", FacilityId::Infirmary, "Lifesaving Technique", "救命技術",
         OutpostStage::PioneerOutpost, {}, {}, {"field_infirmary"}, false,
         "Unlocks the upgraded Rescue Pack and increases revive HP.",
         "高級救命包と、復帰時HPの増加を解放する。"},
        {"pharmacology", FacilityId::Infirmary, "Pharmacology", "薬学",
         OutpostStage::FrontierSettlement, {}, {}, {"field_infirmary"}, false,
         "Unlocks the Panacea and cures for special status ailments.",
         "万能薬と特殊状態異常の治療手段を解放する。"},

        // 5. 工房 (Workshop)
        {"workshop_bench", FacilityId::Workshop, "Workshop Bench", "工作台",
         OutpostStage::PioneerOutpost, {}, {{"wood", 3}, {"hide", 1}}, {}, true,
         "Unlocks basic exploration tools.",
         "基本探索道具を解放する。"},
        {"exploration_tools", FacilityId::Workshop, "Exploration Tools", "探索工作",
         OutpostStage::PioneerOutpost, {}, {}, {"workshop_bench"}, false,
         "Unlocks climbing gear, mining tools, and lanterns.",
         "登攀具・採掘具・照明具を解放する。"},
        {"combat_tools", FacilityId::Workshop, "Combat Tools", "戦闘工作",
         OutpostStage::PioneerOutpost, {}, {}, {"workshop_bench"}, false,
         "Unlocks Protective Boards, caltrops, and smoke tubes.",
         "防護板・鉄杭・煙幕筒を解放する。"},
        {"advanced_crafting", FacilityId::Workshop, "Advanced Crafting", "高度工作",
         OutpostStage::FrontierSettlement, {kReturnSignalDiscovery}, {}, {"workshop_bench"}, false,
         "Unlocks the Return Flare, Protective Case, and special ruin devices.",
         "帰還信号弾・保護箱・特殊遺跡装置を解放する。"},

        // 6. 宿舎 (Barracks)
        {"communal_tent", FacilityId::Barracks, "Communal Tent", "共同テント",
         OutpostStage::Encampment, {}, {}, {}, false,
         "Enables the starting 4 companions and the 4-person formation.",
         "初期仲間4人と4人編成を使用可能にする。"},
        {"barracks_expansion", FacilityId::Barracks, "Barracks Expansion", "宿舎増築",
         OutpostStage::PioneerOutpost, {}, {}, {"communal_tent"}, false,
         "Increases the number of companions you can recruit.",
         "新しい仲間を受け入れられる枠を増やす。"},
        {"specialist_quarters", FacilityId::Barracks, "Specialist Quarters", "専門区画",
         OutpostStage::FrontierSettlement, {}, {}, {"communal_tent"}, false,
         "Allows recruiting companions of specialist classes.",
         "特殊兵種の仲間を受け入れ可能にする。"},
        {"social_quarters", FacilityId::Barracks, "Social Quarters", "交流区画",
         OutpostStage::PioneerOutpost, {}, {}, {"communal_tent"}, false,
         "Unlocks companion events and link skills.",
         "仲間イベントと連携能力を解放する。"},
    };
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
// stock, prerequisites (a branch's prerequisite facility must be actively
// built, not merely historically unlocked), and remaining facility slots.
inline bool facilityNodeEligible(const BaseState& base, const FacilityNode& node) {
    if (base.unlockedNodeIds.count(node.id)) return false;
    if (static_cast<int>(base.outpostStage) < static_cast<int>(node.requiredStage)) return false;
    for (const DiscoveryId& discovery : node.requiredDiscoveries) {
        if (!base.discoveryRegistry.count(discovery)) return false;
    }
    for (const std::string& prereqId : node.prerequisiteNodeIds) {
        const FacilityNode* prereq = findFacilityNode(prereqId);
        bool satisfied = prereq && prereq->occupiesFacilitySlot ? base.builtNodeIds.count(prereqId) > 0
                                                                 : base.unlockedNodeIds.count(prereqId) > 0;
        if (!satisfied) return false;
    }
    for (const LootStack& cost : node.materialCosts) {
        if (base.storageCount(cost.id) < cost.quantity) return false;
    }
    if (node.occupiesFacilitySlot &&
        static_cast<int>(base.builtNodeIds.size()) >= facilitySlotCapacity(base.outpostStage)) {
        return false;
    }
    return true;
}

} // namespace jf
