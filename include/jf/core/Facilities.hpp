#pragma once

#include <string>
#include <vector>

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
         OutpostStage::PioneerOutpost, {kCinderwatchReconDiscovery}, {{"watch_ledger", 1}, {"wood", 2}, {"iron", 1}},
         {"operations_tent"}, false,
         "Shows enemy composition and starting positions before you choose a route.",
         "ルート選択前に敵の種類と初期配置を表示する。"},
        {"map_room", FacilityId::CommandPost, "Map Room", "地図室",
         OutpostStage::PioneerOutpost, {}, {{"ash_road_map", 1}, {"wood", 2}, {"stone", 2}}, {"operations_tent"}, false,
         "Reveals part of the route branches and the rough distance to the boss.",
         "ルート分岐の一部とボスまでのおおよその距離を表示する。"},
        {"expedition_planning_room", FacilityId::CommandPost, "Expedition Planning Room", "遠征計画室",
         OutpostStage::FrontierSettlement, {kReturnSignalDiscovery},
         {{"captains_seal", 1}, {"building_material", 2}, {"quality_iron", 1}, {"military_supplies", 1}},
         {"operations_tent"}, false,
         "Shows deep-region intel and unlocks special expeditions.",
         "深層情報を表示し、特殊遠征を解放する。"},

        // 2. 訓練所 (Training Ground)
        {"training_field", FacilityId::TrainingGround, "Training Field", "訓練場",
         OutpostStage::PioneerOutpost, {}, {{"wood", 3}, {"hide", 2}}, {}, true,
         "Enables the basic classes and their base class techniques.",
         "基礎兵種と基本兵種技を使用可能にする。"},
        {"vanguard_training", FacilityId::TrainingGround, "Vanguard Training", "前衛訓練",
         OutpostStage::PioneerOutpost, {}, {{"wood", 2}, {"hide", 2}, {"quality_iron", 1}}, {"training_field"}, false,
         "Unlocks Veteran Guard, Spearman, and other heavy-vanguard techniques.",
         "古参兵・槍兵・重装兵の技術を解放する。"},
        {"mobility_training", FacilityId::TrainingGround, "Mobility Training", "機動訓練",
         OutpostStage::PioneerOutpost, {kCinderwatchReconDiscovery}, {{"wood", 2}, {"hide", 2}, {"cloth", 1}, {"riding_gear", 1}}, {"training_field"}, false,
         "Unlocks Frontier Scout and other light-cavalry techniques.",
         "斥候・猟兵・伝令騎兵の技術を解放する。"},
        {"specialist_training", FacilityId::TrainingGround, "Specialist Training", "専門訓練",
         OutpostStage::FrontierSettlement, {}, {{"wood", 2}, {"iron", 2}, {"quality_herb", 1}, {"military_supplies", 1}}, {"training_field"}, false,
         "Unlocks engineer, standard-bearer, and other advanced class branches.",
         "工兵・旗手など上位兵種分岐を解放する。"},
        // docs/implementation_status.md「装備スキル解放レビュー」#3: Battle Mage
        // had no mapped training node at all (requiredTrainingNodeIdFor()
        // returned "" for it, same as the enemy-only classes), so
        // equipSkillForUnit() rejected every attempt unconditionally - the
        // class could never equip a skill, not even its Tier 1. Added as a
        // 4th training branch, same shape/cost tier as the other 3, rather
        // than inventing a new gating mechanism just for this one class.
        {"magic_training", FacilityId::TrainingGround, "Magic Training", "魔導訓練",
         OutpostStage::FrontierSettlement, {}, {{"wood", 2}, {"iron", 2}, {"quality_herb", 1}, {"military_supplies", 1}}, {"training_field"}, false,
         "Unlocks Battle Mage's equippable skills.",
         "戦闘魔導士の装備スキルを解放する。"},

        // 3. 鍛冶場 (Forge)
        {"simple_forge", FacilityId::Forge, "Simple Forge", "簡易鍛冶台",
         OutpostStage::PioneerOutpost, {}, {{"wood", 2}, {"hide", 1}}, {}, true,
         "Lets you change equipped weapons and view/craft weapon branch recipes.",
         "武器の装備変更と、武器分岐レシピの閲覧・製作を可能にする。"},
        {"weapon_forging", FacilityId::Forge, "Weapon Forging", "武器鍛造",
         OutpostStage::Encampment, {}, {{"iron", 2}, {"wood", 2}, {"quality_iron", 1}}, {"simple_forge"}, false,
         "Unlocks Iron Spear's branch weapon recipes (Long/Heavy/Guard Spear).",
         "鉄の槍の分岐武器レシピ(長槍・重槍・迎撃槍)を解放する。"},
        {"heavy_reinforcement", FacilityId::Forge, "Heavy Reinforcement", "重装加工",
         OutpostStage::PioneerOutpost, {}, {{"iron", 3}, {"stone", 2}, {"quality_iron", 1}}, {"simple_forge"}, false,
         "Unlocks armor reinforcement and knockback-resistant equipment.",
         "防具強化とノックバック耐性装備を解放する。"},
        {"special_material_crafting", FacilityId::Forge, "Special Material Crafting", "特殊素材加工",
         OutpostStage::FrontierSettlement, {kReturnSignalDiscovery}, {{"heat_resistant_material", 2}, {"ash_crystal", 2}},
         {"simple_forge"}, false,
         "Unlocks weapons crafted from monster and ruin materials.",
         "魔物素材武器・遺跡素材武器を解放する。"},
        {"craft_long_spear", FacilityId::Forge, "Craft: Long Spear", "製作: 長槍",
         OutpostStage::Encampment, {}, {{"ash_road_map", 1}}, {"weapon_forging"}, false,
         "Ranged branch: max range +1, might -2.",
         "射程型分岐: 最大射程+1、威力-2。", UnitClass::Spearman},
        {"craft_heavy_spear", FacilityId::Forge, "Craft: Heavy Spear", "製作: 重槍",
         OutpostStage::Encampment, {}, {{"gate_tools", 1}}, {"weapon_forging"}, false,
         "Power branch: might +2, MOV -1, attacks knock the defender back one tile.",
         "火力型分岐: 威力+2、MOV-1、攻撃命中時に相手を1マスノックバック。", UnitClass::Spearman},
        {"craft_guard_spear", FacilityId::Forge, "Craft: Guard Spear", "製作: 迎撃槍",
         OutpostStage::Encampment, {}, {{"watch_ledger", 1}}, {"weapon_forging"}, false,
         "Interception branch: stronger Brace bonus, might -1.",
         "迎撃型分岐: 迎撃姿勢強化、通常威力-1。", UnitClass::Spearman},
        // Weapon-branch generalization to the other 11 classes (docs/
        // base_development.md「初期6兵種の武器分岐仕様」「後半6兵種の武器分岐
        // 仕様」, docs/character_progression.md「初期6兵種の武器レシピ」).
        // Stage per branch follows docs/base_development.md's own staged-
        // unlock tables: front-5 (non-Spearman) get 1st branch at Pioneer
        // Outpost / 2nd at Frontier Settlement / 3rd at Pioneer City; back-6
        // get 1st branch at Frontier Settlement / 2nd+3rd at Pioneer City
        // (Battle Mage's "魔導工房解放後" has no dedicated facility node yet,
        // so it is approximated as Pioneer City, its latest tier).
        // Several required-Discovery IDs below are not granted by any
        // implemented region content yet - see docs/implementation_status.md
        // for the current list (same "implement the correctly-gated recipe
        // even though the gate is temporarily unreachable" pattern used
        // elsewhere in this project).

        // March Captain / Iron Sword
        {"march_captain_forging", FacilityId::Forge, "March Captain Forging", "行軍隊長武器鍛造",
         OutpostStage::Encampment, {}, {}, {"simple_forge"}, false,
         "Unlocks Iron Sword's branch weapon recipes (Command/Duel/Guard Sword).",
         "鉄の剣の分岐武器レシピ(号令剣・決闘剣・護衛剣)を解放する。"},
        {"craft_command_sword", FacilityId::Forge, "Craft: Command Sword", "製作: 号令剣",
         OutpostStage::Encampment, {"cinderwatch_command_drills"}, {{"iron", 2}, {"wood", 1}},
         {"march_captain_forging"}, false,
         "Formation branch: Formation Bonus range extended to distance 2, might -1.",
         "隊形支援型分岐: Formation Bonus範囲を距離2へ拡張、威力-1。", UnitClass::MarchCaptain},
        {"craft_duel_sword", FacilityId::Forge, "Craft: Duel Sword", "製作: 決闘剣",
         OutpostStage::Encampment, {"quarry_combat_records"}, {{"iron", 3}, {"hide", 1}},
         {"march_captain_forging"}, false,
         "Duelist branch: might +2, +2 damage vs an isolated target, loses Formation Bonus while equipped.",
         "単独撃破型分岐: 威力+2、孤立した敵への与ダメージ+2、装備中はFormation Bonusを失う。", UnitClass::MarchCaptain},
        {"craft_guard_sword", FacilityId::Forge, "Craft: Guard Sword", "製作: 護衛剣",
         OutpostStage::Encampment, {"settlement_command_ledger"}, {{"iron", 2}, {"cloth", 1}},
         {"march_captain_forging"}, false,
         "Guard branch: reduces the first hit an adjacent ally takes each battle by 3, might -1.",
         "護衛型分岐: 隣接味方が受ける最初の攻撃ダメージを戦闘ごとに3軽減、威力-1。", UnitClass::MarchCaptain},

        // Veteran Guard / Iron Lance
        {"veteran_guard_forging", FacilityId::Forge, "Veteran Guard Forging", "古参守備兵武器鍛造",
         OutpostStage::Encampment, {}, {}, {"simple_forge"}, false,
         "Unlocks Iron Lance's branch weapon recipes (Hook/Fortress/Patrol Lance).",
         "鉄の槍の分岐武器レシピ(鉤槍・城塞槍・巡回槍)を解放する。"},
        {"craft_hook_lance", FacilityId::Forge, "Craft: Hook Lance", "製作: 鉤槍",
         OutpostStage::Encampment, {"cinderwatch_defense_manual"}, {{"iron", 2}, {"hide", 1}},
         {"veteran_guard_forging"}, false,
         "Disruption branch: pulls a range-2 hit target one tile closer, might -1.",
         "敵の隊形を崩す型分岐: 射程2から命中させた敵を1マス引き寄せる、威力-1。", UnitClass::VeteranGuard},
        {"craft_fortress_lance", FacilityId::Forge, "Craft: Fortress Lance", "製作: 城塞槍",
         OutpostStage::Encampment, {"quarry_brace_records"}, {{"iron", 3}, {"wood", 2}},
         {"veteran_guard_forging"}, false,
         "Lockdown branch: Zone of Control also cuts the intruder's damage by 2 until their next action, might -2, MOV -1.",
         "完全封鎖型分岐: Zone of Controlに入った敵の与ダメージ-2、威力-2、MOV-1。", UnitClass::VeteranGuard},
        {"craft_patrol_lance", FacilityId::Forge, "Craft: Patrol Lance", "製作: 巡回槍",
         OutpostStage::Encampment, {"plateau_patrol_records"}, {{"iron", 2}, {"riding_gear", 1}},
         {"veteran_guard_forging"}, false,
         "Mobile-defense branch: waiting grants DEF +2 until the next Player Phase, might -1, MOV +1.",
         "先回りして守る機動防衛型分岐: 待機でDEF+2を次のPlayer Phaseまで付与、威力-1、MOV+1。", UnitClass::VeteranGuard},

        // Watch Archer / Watch Bow
        {"watch_archer_forging", FacilityId::Forge, "Watch Archer Forging", "監視弓兵武器鍛造",
         OutpostStage::Encampment, {}, {}, {"simple_forge"}, false,
         "Unlocks Watch Bow's branch weapon recipes (Long Watch/War/Pinning Bow).",
         "監視弓の分岐武器レシピ(遠見弓・戦弓・制圧弓)を解放する。"},
        {"craft_long_watch_bow", FacilityId::Forge, "Craft: Long Watch Bow", "製作: 遠見弓",
         OutpostStage::Encampment, {"cinderwatch_watch_records"}, {{"wood", 2}, {"hide", 1}},
         {"watch_archer_forging"}, false,
         "Ultra-range branch: range 3-4 (loses range 2), might -2.",
         "超長距離型分岐: 射程3-4(射程2を失う)、威力-2。", UnitClass::WatchArcher},
        {"craft_war_bow", FacilityId::Forge, "Craft: War Bow", "製作: 戦弓",
         OutpostStage::Encampment, {"quarry_combat_records"}, {{"wood", 2}, {"iron", 2}},
         {"watch_archer_forging"}, false,
         "Close-power branch: might +3, range 2 only (loses range 3), +2 damage vs a full-HP target.",
         "近距離高火力型分岐: 威力+3、射程2のみ(射程3を失う)、最大HPの敵への与ダメージ+2。", UnitClass::WatchArcher},
        {"craft_pinning_bow", FacilityId::Forge, "Craft: Pinning Bow", "製作: 制圧弓",
         OutpostStage::Encampment, {"plateau_targeting_records"}, {{"hardwood", 2}, {"wetland_resin", 1}},
         {"watch_archer_forging"}, false,
         "Pin-down branch: on hit, the target's MOV is reduced next Enemy Phase, might -1.",
         "足止め型分岐: 命中した敵の次のEnemy PhaseのMOVを低下させる、威力-1。", UnitClass::WatchArcher},

        // Frontier Scout / Scout Blade
        {"frontier_scout_forging", FacilityId::Forge, "Frontier Scout Forging", "辺境斥候武器鍛造",
         OutpostStage::Encampment, {}, {}, {"simple_forge"}, false,
         "Unlocks Scout Blade's branch weapon recipes (Trail/Ambush/Withdrawal Blade).",
         "斥候刃の分岐武器レシピ(道拓きの刃・奇襲刃・離脱刃)を解放する。"},
        {"craft_trail_blade", FacilityId::Forge, "Craft: Trail Blade", "製作: 道拓きの刃",
         OutpostStage::Encampment, {"ashbough_forest_survey_complete"}, {{"iron", 1}, {"hide", 2}},
         {"frontier_scout_forging"}, false,
         "Route-support branch: eases ash-ground/shallow-water movement cost for the Player Phase, might -1, MOV +1.",
         "経路支援型分岐: 灰地・浅瀬の移動コストをそのPlayer Phase中緩和、威力-1、MOV+1。", UnitClass::FrontierScout},
        {"craft_ambush_blade", FacilityId::Forge, "Craft: Ambush Blade", "製作: 奇襲刃",
         OutpostStage::Encampment, {"quarry_ambush_records"}, {{"iron", 2}, {"hide", 2}},
         {"frontier_scout_forging"}, false,
         "First-strike branch: might +2, +2 damage vs an enemy that hasn't acted this round, DEF -2 after attacking.",
         "先制攻撃型分岐: 威力+2、未行動の敵への与ダメージ+2、攻撃後DEF-2。", UnitClass::FrontierScout},
        {"craft_withdrawal_blade", FacilityId::Forge, "Craft: Withdrawal Blade", "製作: 離脱刃",
         OutpostStage::Encampment, {"courier_route_chart"}, {{"iron", 1}, {"riding_gear", 2}},
         {"frontier_scout_forging"}, false,
         "Disruption branch: after attacking, can re-move one tile away from the enemy if it survives, might -1.",
         "攪乱型分岐: 攻撃後、生存していれば敵から離れる方向へ1マス再移動できる、威力-1。", UnitClass::FrontierScout},

        // Dawn Chirurgeon / Dawn Staff
        {"dawn_chirurgeon_forging", FacilityId::Forge, "Dawn Chirurgeon Forging", "暁の衛生兵武器鍛造",
         OutpostStage::Encampment, {}, {}, {"simple_forge"}, false,
         "Unlocks Dawn Staff's branch weapon recipes (Mercy/Ward/March Staff).",
         "暁杖の分岐武器レシピ(慈恵杖・守護杖・行軍杖)を解放する。"},
        {"craft_mercy_staff", FacilityId::Forge, "Craft: Mercy Staff", "製作: 慈恵杖",
         OutpostStage::Encampment, {kHerbThicketDiscovery}, {{"wood", 2}, {"herb", 2}},
         {"dawn_chirurgeon_forging"}, false,
         "Single-target healing branch: Heal amount 8 -> 12, attack might -2.",
         "単体治療型分岐: Healの回復量を8から12へ増加、攻撃威力-2。", UnitClass::DawnChirurgeon},
        {"craft_ward_staff", FacilityId::Forge, "Craft: Ward Staff", "製作: 守護杖",
         OutpostStage::Encampment, {kMarshEmergencyMedicineDiscovery}, {{"wood", 2}, {"quality_herb", 1}},
         {"dawn_chirurgeon_forging"}, false,
         "Magic-ward branch: Heal target gains RES +3 until the next Enemy Phase ends, attack might -1, Heal amount 6.",
         "魔法防護型分岐: Heal対象へRES+3を次のEnemy Phase終了まで付与、攻撃威力-1、Heal回復量は6。", UnitClass::DawnChirurgeon},
        {"craft_march_staff", FacilityId::Forge, "Craft: March Staff", "製作: 行軍杖",
         OutpostStage::Encampment, {"sanctum_field_medicine"}, {{"hardwood", 2}, {"cloth", 1}, {"herb", 1}},
         {"dawn_chirurgeon_forging"}, false,
         "March-support branch: an unacted Heal target gains MOV +1 for the Player Phase, attack might -1, Heal amount 6.",
         "進軍支援型分岐: Heal対象が未行動ならそのPlayer Phase中MOV+1、攻撃威力-1、Heal回復量は6。", UnitClass::DawnChirurgeon},

        // Heavy Infantry / Iron Maul
        {"heavy_infantry_forging", FacilityId::Forge, "Heavy Infantry Forging", "重装歩兵武器鍛造",
         OutpostStage::Encampment, {}, {}, {"simple_forge"}, false,
         "Unlocks Iron Maul's branch weapon recipes (Bulwark/Breaker/Driving Maul).",
         "鉄の大槌の分岐武器レシピ(防壁槌・破砕槌・圧進槌)を解放する。"},
        {"craft_bulwark_maul", FacilityId::Forge, "Craft: Bulwark Maul", "製作: 防壁槌",
         OutpostStage::Encampment, {"heavy_armor_method"}, {{"iron", 2}, {"wood", 1}},
         {"heavy_infantry_forging"}, false,
         "Braced branch: waiting grants DEF +2 until your next action, might -2.",
         "受け止め型分岐: 待機時、次の自分の行動開始までDEF+2、威力-2。", UnitClass::HeavyInfantry},
        {"craft_breaker_maul", FacilityId::Forge, "Craft: Breaker Maul", "製作: 破砕槌",
         OutpostStage::Encampment, {"demolition_forging"}, {{"iron", 3}, {"wood", 1}},
         {"heavy_infantry_forging"}, false,
         "Breach branch: double damage vs obstacles (no bonus vs units), might +2, MOV -1.",
         "障害物突破型分岐: 障害物へ与えるダメージ2倍(ユニットへの追加効果なし)、威力+2、MOV-1。", UnitClass::HeavyInfantry},
        {"craft_driving_maul", FacilityId::Forge, "Craft: Driving Maul", "製作: 圧進槌",
         OutpostStage::Encampment, {"impact_balance_record"}, {{"iron", 2}, {"hide", 1}},
         {"heavy_infantry_forging"}, false,
         "Knockback branch: on hit, pushes the defender back one tile, might -1.",
         "位置操作型分岐: 命中した敵を1マス押し出す、威力-1。", UnitClass::HeavyInfantry},

        // Frontier Engineer / Tool Hammer
        {"frontier_engineer_forging", FacilityId::Forge, "Frontier Engineer Forging", "辺境工兵武器鍛造",
         OutpostStage::Encampment, {}, {}, {"simple_forge"}, false,
         "Unlocks Tool Hammer's branch weapon recipes (Builder/Demolition/Repair Hammer).",
         "工作槌の分岐武器レシピ(築造槌・解体槌・補修槌)を解放する。"},
        {"craft_builder_hammer", FacilityId::Forge, "Craft: Builder Hammer", "製作: 築造槌",
         OutpostStage::Encampment, {"field_construction_manual"}, {{"iron", 1}, {"wood", 3}},
         {"frontier_engineer_forging"}, false,
         "Emplacement branch: Protective Boards placed by the class skill gain +4 durability, might -2.",
         "設置型分岐: 固有能力で設置する防護板の耐久+4、威力-2。", UnitClass::FrontierEngineer},
        {"craft_demolition_hammer", FacilityId::Forge, "Craft: Demolition Hammer", "製作: 解体槌",
         OutpostStage::Encampment, {"controlled_demolition_notes"}, {{"iron", 3}, {"wood", 1}},
         {"frontier_engineer_forging"}, false,
         "Assault branch: +2 damage vs enemies adjacent to an obstacle, might +2, can't reinforce emplacements.",
         "攻撃型分岐: 障害物に隣接する敵への与ダメージ+2、威力+2、設置物の耐久を補強できない。", UnitClass::FrontierEngineer},
        {"craft_repair_hammer", FacilityId::Forge, "Craft: Repair Hammer", "製作: 補修槌",
         OutpostStage::Encampment, {"structural_repair_guide"}, {{"iron", 1}, {"wood", 2}, {"herb", 1}},
         {"frontier_engineer_forging"}, false,
         "Sustain branch: Field Repair heal amount 6 -> 9, might -1.",
         "維持型分岐: 野戦補修の回復量を6から9へ増加、威力-1。", UnitClass::FrontierEngineer},

        // Messenger Cavalry / Courier Sabre
        {"messenger_cavalry_forging", FacilityId::Forge, "Messenger Cavalry Forging", "伝令騎兵武器鍛造",
         OutpostStage::Encampment, {}, {}, {"simple_forge"}, false,
         "Unlocks Courier Sabre's branch weapon recipes (Road Sabre/Charge Lance/Escort Blade).",
         "伝令剣の分岐武器レシピ(街道剣・突撃騎槍・護送剣)を解放する。"},
        {"craft_road_sabre", FacilityId::Forge, "Craft: Road Sabre", "製作: 街道剣",
         OutpostStage::Encampment, {"courier_route_chart"}, {{"iron", 1}, {"hide", 2}},
         {"messenger_cavalry_forging"}, false,
         "Reach branch: MOV +1 (re-move distance unchanged), might -2.",
         "到達範囲型分岐: MOV+1(再移動距離は増えない)、威力-2。", UnitClass::MessengerCavalry},
        {"craft_charge_lance", FacilityId::Forge, "Craft: Charge Lance", "製作: 突撃騎槍",
         OutpostStage::Encampment, {"mounted_charge_drill"}, {{"iron", 3}, {"hide", 1}},
         {"messenger_cavalry_forging"}, false,
         "Charge branch: might +2, +2 damage if the unit moved 3+ tiles before attacking, DEF -2 until your next action.",
         "突撃型分岐: 威力+2、攻撃前に3マス以上移動していれば与ダメージ+2、攻撃後DEF-2。", UnitClass::MessengerCavalry},
        {"craft_escort_blade", FacilityId::Forge, "Craft: Escort Blade", "製作: 護送剣",
         OutpostStage::Encampment, {"escort_signal_code"}, {{"iron", 2}, {"hide", 2}},
         {"messenger_cavalry_forging"}, false,
         "Rescue branch: after re-moving, an adjacent ally gains DEF +2 until Enemy Phase ends, might -1.",
         "救援型分岐: 再移動終了時、隣接味方へDEF+2を次のEnemy Phase終了まで付与、威力-1。", UnitClass::MessengerCavalry},

        // Frontier Ranger / Hunting Bow
        {"frontier_ranger_forging", FacilityId::Forge, "Frontier Ranger Forging", "辺境猟兵武器鍛造",
         OutpostStage::Encampment, {}, {}, {"simple_forge"}, false,
         "Unlocks Hunting Bow's branch weapon recipes (Snare/Quarry/Driving Bow).",
         "狩猟弓の分岐武器レシピ(拘束弓・追跡弓・追込弓)を解放する。"},
        {"craft_snare_bow", FacilityId::Forge, "Craft: Snare Bow", "製作: 拘束弓",
         OutpostStage::Encampment, {"snare_pattern_record"}, {{"wood", 2}, {"hide", 2}},
         {"frontier_ranger_forging"}, false,
         "Trap-synergy branch: the first hit each battle applies a movement-reducing effect, might -1.",
         "罠連携型分岐: 戦闘ごとに最初に命中させた敵へ移動低下を付与、威力-1。", UnitClass::FrontierRanger},
        {"craft_quarry_bow", FacilityId::Forge, "Craft: Quarry Bow", "製作: 追跡弓",
         OutpostStage::Encampment, {"quarry_tracking_notes"}, {{"wood", 2}, {"iron", 1}, {"hide", 1}},
         {"frontier_ranger_forging"}, false,
         "Isolation-hunt branch: +2 damage vs an enemy with no adjacent ally, might +1.",
         "孤立追跡型分岐: 隣接する味方がいない敵への与ダメージ+2、威力+1。", UnitClass::FrontierRanger},
        {"craft_driving_bow", FacilityId::Forge, "Craft: Driving Bow", "製作: 追込弓",
         OutpostStage::Encampment, {"herding_shot_method"}, {{"wood", 2}, {"iron", 1}, {"hide", 2}},
         {"frontier_ranger_forging"}, false,
         "Herding branch: the first hit each battle pushes the target back one tile, might -1.",
         "誘導型分岐: 戦闘ごとに最初に命中させた敵を1マス押し出す、威力-1。", UnitClass::FrontierRanger},

        // Banner Bearer / Standard Spear
        {"banner_bearer_forging", FacilityId::Forge, "Banner Bearer Forging", "旗手武器鍛造",
         OutpostStage::Encampment, {}, {}, {"simple_forge"}, false,
         "Unlocks Standard Spear's branch weapon recipes (Far/Valor/Warding Standard).",
         "戦旗槍の分岐武器レシピ(遠征旗槍・勇壮旗槍・守護旗槍)を解放する。"},
        {"craft_far_standard", FacilityId::Forge, "Craft: Far Standard", "製作: 遠征旗槍",
         OutpostStage::Encampment, {"long_range_signal_code"}, {{"iron", 1}, {"wood", 2}, {"hide", 1}},
         {"banner_bearer_forging"}, false,
         "Wide-support branch: banner range 2 -> 3, might -2.",
         "広域支援型分岐: 戦旗の範囲を距離2から3へ拡大、威力-2。", UnitClass::BannerBearer},
        {"craft_valor_standard", FacilityId::Forge, "Craft: Valor Standard", "製作: 勇壮旗槍",
         OutpostStage::Encampment, {"vanguard_banner_record"}, {{"iron", 2}, {"wood", 1}, {"hide", 2}},
         {"banner_bearer_forging"}, false,
         "Physical-offense branch: banner grants STR +2 only (no MAG), might +1, no defensive banner support.",
         "物理攻勢型分岐: 戦旗はSTR+2だけを与えMAGは上昇させない、威力+1、防御支援なし。", UnitClass::BannerBearer},
        {"craft_warding_standard", FacilityId::Forge, "Craft: Warding Standard", "製作: 守護旗槍",
         OutpostStage::Encampment, {"protective_banner_rite"}, {{"iron", 1}, {"wood", 2}, {"herb", 2}},
         {"banner_bearer_forging"}, false,
         "Defensive-support branch: banner grants DEF +1/RES +1 instead of STR/MAG, might -1.",
         "防衛支援型分岐: 戦旗をDEF+1、RES+1へ変更しSTRとMAGは上昇させない、威力-1。", UnitClass::BannerBearer},

        // Battle Mage / Arcane Focus
        {"battle_mage_forging", FacilityId::Forge, "Battle Mage Forging", "戦闘魔導士武器鍛造",
         OutpostStage::Encampment, {}, {}, {"simple_forge"}, false,
         "Unlocks Arcane Focus's branch weapon recipes (Resonant/War/Ember Focus).",
         "魔導焦点具の分岐武器レシピ(共鳴焦点具・戦式焦点具・残火焦点具)を解放する。"},
        {"craft_resonant_focus", FacilityId::Forge, "Craft: Resonant Focus", "製作: 共鳴焦点具",
         OutpostStage::Encampment, {"arcane_resonance_record"}, {{"iron", 1}, {"ruin_fragment", 2}},
         {"battle_mage_forging"}, false,
         "Splash branch: the first standard attack each battle also deals 2 fixed damage to the target's vertical neighbors, might -2.",
         "範囲型分岐: 戦闘ごとに最初の通常攻撃で対象の上下隣接敵へ固定2ダメージ、威力-2。", UnitClass::BattleMage},
        {"craft_war_focus", FacilityId::Forge, "Craft: War Focus", "製作: 戦式焦点具",
         OutpostStage::Encampment, {"battle_focus_formula"}, {{"iron", 2}, {"ruin_fragment", 3}},
         {"battle_mage_forging"}, false,
         "Single-target power branch: might +3, MOV -1.",
         "単体火力型分岐: 威力+3、MOV-1。", UnitClass::BattleMage},
        {"craft_ember_focus", FacilityId::Forge, "Craft: Ember Focus", "製作: 残火焦点具",
         OutpostStage::Encampment, {"controlled_ember_formula"}, {{"iron", 1}, {"ruin_fragment", 2}, {"herb", 1}},
         {"battle_mage_forging"}, false,
         "Sustained-pressure branch: the first hit each battle inflicts Burn, might -1.",
         "継続圧力型分岐: 戦闘ごとに最初に命中させた敵へ炎上を付与、威力-1。", UnitClass::BattleMage},

        {"trait_hide_wrapped_grip", FacilityId::Forge, "Tuning: Hide-Wrapped Grip", "調整: 獣皮の柄巻き",
         OutpostStage::PioneerOutpost, {}, {{"hide", 1}}, {"simple_forge"}, false,
         "Negates the first knockback the wearer receives each battle.",
         "戦闘ごとに最初に受けるノックバックを1回無効化する。"},

        // M10-B (docs/deep_layers.md「防具システム(新設)」): armor Tier1〜3
        // craft recipes for the 6 initial classes (18 of the eventual 36 -
        // same 6-classes-first split as the "craft_*" weapon-branch nodes
        // above). Same "requiredStage pinned to Encampment, material-only
        // gate" treatment as weapon branches from the start (docs/
        // deep_layers.md「分岐/Tier解放を...素材ゲートへ変更」) - no
        // stage-gated-then-ungated detour. Lv1 cost is 2 units of that Tier's
        // own weapon-branch primary material (docs/deep_layers.md「防具
        // Lv1〜5」worked example: 行軍隊長Tier1 = 鉄材2, matching
        // craft_command_sword's own primary "iron"), reusing
        // ArmorLeveling.hpp's kArmorTier1LvOneQuantity via the literal 2
        // below (that header can't be included here - it depends on this one
        // for LootStack/BaseState). `weaponBranchClass` is reused (not
        // renamed) to route these through the same per-class Forge UI
        // filtering as weapon branches - see ui_facilities.cpp's own
        // "craft_armor_" prefix check for how it tells the two apart.
        {"craft_armor_march_captain_tier1", FacilityId::Forge, "Craft: March Captain Guard", "製作: 行軍隊長の護具",
         OutpostStage::Encampment, {"cinderwatch_command_drills"}, {{"iron", 2}}, {"simple_forge"}, false,
         "Balanced armor: DEF/RES +1 each at Lv1.",
         "汎用防具: Lv1でDEF/RES各+1。", UnitClass::MarchCaptain},
        {"craft_armor_march_captain_tier2", FacilityId::Forge, "Craft: March Captain Plate", "製作: 行軍隊長の重甲",
         OutpostStage::Encampment, {"quarry_combat_records"}, {{"iron", 2}}, {"simple_forge"}, false,
         "DEF-specialized armor: DEF +2 at Lv1, no RES.",
         "DEF特化防具: Lv1でDEF+2、RESは付与しない。", UnitClass::MarchCaptain},
        {"craft_armor_march_captain_tier3", FacilityId::Forge, "Craft: March Captain Ward", "製作: 行軍隊長の守甲",
         OutpostStage::Encampment, {"settlement_command_ledger"}, {{"cloth", 2}}, {"simple_forge"}, false,
         "Balanced armor: DEF/RES +1 each at Lv1.",
         "汎用防具: Lv1でDEF/RES各+1。", UnitClass::MarchCaptain},

        {"craft_armor_veteran_guard_tier1", FacilityId::Forge, "Craft: Veteran Guard Mail", "製作: 古参守備兵の帷子",
         OutpostStage::Encampment, {"cinderwatch_defense_manual"}, {{"iron", 2}}, {"simple_forge"}, false,
         "Balanced armor: DEF/RES +1 each at Lv1.",
         "汎用防具: Lv1でDEF/RES各+1。", UnitClass::VeteranGuard},
        {"craft_armor_veteran_guard_tier2", FacilityId::Forge, "Craft: Veteran Guard Bastion Plate",
         "製作: 古参守備兵の防塁甲", OutpostStage::Encampment, {"quarry_brace_records"}, {{"iron", 2}},
         {"simple_forge"}, false, "DEF-specialized armor: DEF +2 at Lv1, no RES.",
         "DEF特化防具: Lv1でDEF+2、RESは付与しない。", UnitClass::VeteranGuard},
        {"craft_armor_veteran_guard_tier3", FacilityId::Forge, "Craft: Veteran Guard Sentinel Ward",
         "製作: 古参守備兵の哨戒守甲", OutpostStage::Encampment, {"plateau_patrol_records"}, {{"riding_gear", 2}},
         {"simple_forge"}, false, "Balanced armor: DEF/RES +1 each at Lv1.",
         "汎用防具: Lv1でDEF/RES各+1。", UnitClass::VeteranGuard},

        {"craft_armor_watch_archer_tier1", FacilityId::Forge, "Craft: Watch Archer Vest", "製作: 監視弓兵の胴衣",
         OutpostStage::Encampment, {"cinderwatch_watch_records"}, {{"wood", 2}}, {"simple_forge"}, false,
         "Balanced armor: DEF/RES +1 each at Lv1.",
         "汎用防具: Lv1でDEF/RES各+1。", UnitClass::WatchArcher},
        {"craft_armor_watch_archer_tier2", FacilityId::Forge, "Craft: Watch Archer Brigandine",
         "製作: 監視弓兵の鎖胴衣", OutpostStage::Encampment, {"quarry_combat_records"}, {{"iron", 2}},
         {"simple_forge"}, false, "DEF-specialized armor: DEF +2 at Lv1, no RES.",
         "DEF特化防具: Lv1でDEF+2、RESは付与しない。", UnitClass::WatchArcher},
        {"craft_armor_watch_archer_tier3", FacilityId::Forge, "Craft: Watch Archer Marsh Ward",
         "製作: 監視弓兵の湿地守衣", OutpostStage::Encampment, {"plateau_targeting_records"}, {{"wetland_resin", 2}},
         {"simple_forge"}, false, "Balanced armor: DEF/RES +1 each at Lv1.",
         "汎用防具: Lv1でDEF/RES各+1。", UnitClass::WatchArcher},

        {"craft_armor_frontier_scout_tier1", FacilityId::Forge, "Craft: Frontier Scout Wrap", "製作: 辺境斥候の巻き衣",
         OutpostStage::Encampment, {"ashbough_forest_survey_complete"}, {{"iron", 2}}, {"simple_forge"}, false,
         "Balanced armor: DEF/RES +1 each at Lv1.",
         "汎用防具: Lv1でDEF/RES各+1。", UnitClass::FrontierScout},
        {"craft_armor_frontier_scout_tier2", FacilityId::Forge, "Craft: Frontier Scout Hardleather",
         "製作: 辺境斥候の硬革衣", OutpostStage::Encampment, {"quarry_ambush_records"}, {{"iron", 2}},
         {"simple_forge"}, false, "DEF-specialized armor: DEF +2 at Lv1, no RES.",
         "DEF特化防具: Lv1でDEF+2、RESは付与しない。", UnitClass::FrontierScout},
        {"craft_armor_frontier_scout_tier3", FacilityId::Forge, "Craft: Frontier Scout Trailward",
         "製作: 辺境斥候の道守衣", OutpostStage::Encampment, {"courier_route_chart"}, {{"riding_gear", 2}},
         {"simple_forge"}, false, "Balanced armor: DEF/RES +1 each at Lv1.",
         "汎用防具: Lv1でDEF/RES各+1。", UnitClass::FrontierScout},

        {"craft_armor_spearman_tier1", FacilityId::Forge, "Craft: Spearman Cuirass", "製作: 槍兵の胸甲",
         OutpostStage::Encampment, {"cinderwatch_spear_drills"}, {{"iron", 2}}, {"simple_forge"}, false,
         "Balanced armor: DEF/RES +1 each at Lv1.",
         "汎用防具: Lv1でDEF/RES各+1。", UnitClass::Spearman},
        {"craft_armor_spearman_tier2", FacilityId::Forge, "Craft: Spearman Bulwark Plate", "製作: 槍兵の防壁甲",
         OutpostStage::Encampment, {"quarry_impact_records"}, {{"iron", 2}}, {"simple_forge"}, false,
         "DEF-specialized armor: DEF +2 at Lv1, no RES.",
         "DEF特化防具: Lv1でDEF+2、RESは付与しない。", UnitClass::Spearman},
        {"craft_armor_spearman_tier3", FacilityId::Forge, "Craft: Spearman Wall Ward", "製作: 槍兵の防壁守甲",
         OutpostStage::Encampment, {"collective_defense_records"}, {{"iron", 2}}, {"simple_forge"}, false,
         "Balanced armor: DEF/RES +1 each at Lv1.",
         "汎用防具: Lv1でDEF/RES各+1。", UnitClass::Spearman},

        {"craft_armor_dawn_chirurgeon_tier1", FacilityId::Forge, "Craft: Dawn Chirurgeon Robe",
         "製作: 暁の衛生兵の外衣", OutpostStage::Encampment, {kHerbThicketDiscovery}, {{"wood", 2}},
         {"simple_forge"}, false, "Balanced armor: DEF/RES +1 each at Lv1.",
         "汎用防具: Lv1でDEF/RES各+1。", UnitClass::DawnChirurgeon},
        {"craft_armor_dawn_chirurgeon_tier2", FacilityId::Forge, "Craft: Dawn Chirurgeon Padded Robe",
         "製作: 暁の衛生兵の綿入り外衣", OutpostStage::Encampment, {kMarshEmergencyMedicineDiscovery},
         {{"quality_herb", 2}}, {"simple_forge"}, false, "DEF-specialized armor: DEF +2 at Lv1, no RES.",
         "DEF特化防具: Lv1でDEF+2、RESは付与しない。", UnitClass::DawnChirurgeon},
        {"craft_armor_dawn_chirurgeon_tier3", FacilityId::Forge, "Craft: Dawn Chirurgeon Sanctum Ward",
         "製作: 暁の衛生兵の聖堂守衣", OutpostStage::Encampment, {"sanctum_field_medicine"}, {{"hardwood", 2}},
         {"simple_forge"}, false, "Balanced armor: DEF/RES +1 each at Lv1.",
         "汎用防具: Lv1でDEF/RES各+1。", UnitClass::DawnChirurgeon},

        // M10-C: armor Tier1〜3 craft recipes for the remaining 6 classes,
        // same pattern as the 18 above (requiredStage pinned to Encampment,
        // Discovery reused from that class's own weapon-branch Tier of the
        // same number, materialCosts is 2 units of a single material picked
        // per-tier from that same weapon branch's own recipe - see
        // ArmorLeveling.hpp「armorLevelMaterialsByClass()」comment for the
        // full citation of each pick).
        {"craft_armor_heavy_infantry_tier1", FacilityId::Forge, "Craft: Heavy Infantry Guard", "製作: 重装兵の護具",
         OutpostStage::Encampment, {"heavy_armor_method"}, {{"iron", 2}}, {"simple_forge"}, false,
         "Balanced armor: DEF/RES +1 each at Lv1.",
         "汎用防具: Lv1でDEF/RES各+1。", UnitClass::HeavyInfantry},
        {"craft_armor_heavy_infantry_tier2", FacilityId::Forge, "Craft: Heavy Infantry Plate", "製作: 重装兵の重甲",
         OutpostStage::Encampment, {"demolition_forging"}, {{"iron", 2}}, {"simple_forge"}, false,
         "DEF-specialized armor: DEF +2 at Lv1, no RES.",
         "DEF特化防具: Lv1でDEF+2、RESは付与しない。", UnitClass::HeavyInfantry},
        {"craft_armor_heavy_infantry_tier3", FacilityId::Forge, "Craft: Heavy Infantry Ward", "製作: 重装兵の守甲",
         OutpostStage::Encampment, {"impact_balance_record"}, {{"hide", 2}}, {"simple_forge"}, false,
         "Balanced armor: DEF/RES +1 each at Lv1.",
         "汎用防具: Lv1でDEF/RES各+1。", UnitClass::HeavyInfantry},

        {"craft_armor_frontier_engineer_tier1", FacilityId::Forge, "Craft: Frontier Engineer Vest",
         "製作: 辺境工兵の作業衣", OutpostStage::Encampment, {"field_construction_manual"}, {{"wood", 2}},
         {"simple_forge"}, false, "Balanced armor: DEF/RES +1 each at Lv1.",
         "汎用防具: Lv1でDEF/RES各+1。", UnitClass::FrontierEngineer},
        {"craft_armor_frontier_engineer_tier2", FacilityId::Forge, "Craft: Frontier Engineer Hardplate",
         "製作: 辺境工兵の硬甲", OutpostStage::Encampment, {"controlled_demolition_notes"}, {{"iron", 2}},
         {"simple_forge"}, false, "DEF-specialized armor: DEF +2 at Lv1, no RES.",
         "DEF特化防具: Lv1でDEF+2、RESは付与しない。", UnitClass::FrontierEngineer},
        {"craft_armor_frontier_engineer_tier3", FacilityId::Forge, "Craft: Frontier Engineer Ward",
         "製作: 辺境工兵の守衣", OutpostStage::Encampment, {"structural_repair_guide"}, {{"herb", 2}},
         {"simple_forge"}, false, "Balanced armor: DEF/RES +1 each at Lv1.",
         "汎用防具: Lv1でDEF/RES各+1。", UnitClass::FrontierEngineer},

        {"craft_armor_messenger_cavalry_tier1", FacilityId::Forge, "Craft: Messenger Cavalry Coat",
         "製作: 伝令騎兵の外套", OutpostStage::Encampment, {"courier_route_chart"}, {{"hide", 2}},
         {"simple_forge"}, false, "Balanced armor: DEF/RES +1 each at Lv1.",
         "汎用防具: Lv1でDEF/RES各+1。", UnitClass::MessengerCavalry},
        {"craft_armor_messenger_cavalry_tier2", FacilityId::Forge, "Craft: Messenger Cavalry Barding",
         "製作: 伝令騎兵の馬鎧", OutpostStage::Encampment, {"mounted_charge_drill"}, {{"iron", 2}},
         {"simple_forge"}, false, "DEF-specialized armor: DEF +2 at Lv1, no RES.",
         "DEF特化防具: Lv1でDEF+2、RESは付与しない。", UnitClass::MessengerCavalry},
        {"craft_armor_messenger_cavalry_tier3", FacilityId::Forge, "Craft: Messenger Cavalry Ward",
         "製作: 伝令騎兵の守衣", OutpostStage::Encampment, {"escort_signal_code"}, {{"hide", 2}},
         {"simple_forge"}, false, "Balanced armor: DEF/RES +1 each at Lv1.",
         "汎用防具: Lv1でDEF/RES各+1。", UnitClass::MessengerCavalry},

        {"craft_armor_frontier_ranger_tier1", FacilityId::Forge, "Craft: Frontier Ranger Wrap",
         "製作: 辺境猟兵の巻き衣", OutpostStage::Encampment, {"snare_pattern_record"}, {{"wood", 2}},
         {"simple_forge"}, false, "Balanced armor: DEF/RES +1 each at Lv1.",
         "汎用防具: Lv1でDEF/RES各+1。", UnitClass::FrontierRanger},
        {"craft_armor_frontier_ranger_tier2", FacilityId::Forge, "Craft: Frontier Ranger Hardleather",
         "製作: 辺境猟兵の硬革衣", OutpostStage::Encampment, {"quarry_tracking_notes"}, {{"hide", 2}},
         {"simple_forge"}, false, "DEF-specialized armor: DEF +2 at Lv1, no RES.",
         "DEF特化防具: Lv1でDEF+2、RESは付与しない。", UnitClass::FrontierRanger},
        {"craft_armor_frontier_ranger_tier3", FacilityId::Forge, "Craft: Frontier Ranger Ward",
         "製作: 辺境猟兵の守衣", OutpostStage::Encampment, {"herding_shot_method"}, {{"wood", 2}},
         {"simple_forge"}, false, "Balanced armor: DEF/RES +1 each at Lv1.",
         "汎用防具: Lv1でDEF/RES各+1。", UnitClass::FrontierRanger},

        {"craft_armor_banner_bearer_tier1", FacilityId::Forge, "Craft: Banner Bearer Vest", "製作: 旗手の胴衣",
         OutpostStage::Encampment, {"long_range_signal_code"}, {{"wood", 2}}, {"simple_forge"}, false,
         "Balanced armor: DEF/RES +1 each at Lv1.",
         "汎用防具: Lv1でDEF/RES各+1。", UnitClass::BannerBearer},
        {"craft_armor_banner_bearer_tier2", FacilityId::Forge, "Craft: Banner Bearer Bastion Plate",
         "製作: 旗手の防塁甲", OutpostStage::Encampment, {"vanguard_banner_record"}, {{"iron", 2}},
         {"simple_forge"}, false, "DEF-specialized armor: DEF +2 at Lv1, no RES.",
         "DEF特化防具: Lv1でDEF+2、RESは付与しない。", UnitClass::BannerBearer},
        {"craft_armor_banner_bearer_tier3", FacilityId::Forge, "Craft: Banner Bearer Ward", "製作: 旗手の守衣",
         OutpostStage::Encampment, {"protective_banner_rite"}, {{"herb", 2}}, {"simple_forge"}, false,
         "Balanced armor: DEF/RES +1 each at Lv1.",
         "汎用防具: Lv1でDEF/RES各+1。", UnitClass::BannerBearer},

        {"craft_armor_battle_mage_tier1", FacilityId::Forge, "Craft: Battle Mage Robe", "製作: 戦闘魔導士の外衣",
         OutpostStage::Encampment, {"arcane_resonance_record"}, {{"iron", 2}}, {"simple_forge"}, false,
         "Balanced armor: DEF/RES +1 each at Lv1.",
         "汎用防具: Lv1でDEF/RES各+1。", UnitClass::BattleMage},
        {"craft_armor_battle_mage_tier2", FacilityId::Forge, "Craft: Battle Mage Plated Robe",
         "製作: 戦闘魔導士の重衣", OutpostStage::Encampment, {"battle_focus_formula"}, {{"ruin_fragment", 2}},
         {"simple_forge"}, false, "DEF-specialized armor: DEF +2 at Lv1, no RES.",
         "DEF特化防具: Lv1でDEF+2、RESは付与しない。", UnitClass::BattleMage},
        {"craft_armor_battle_mage_tier3", FacilityId::Forge, "Craft: Battle Mage Sanctum Ward",
         "製作: 戦闘魔導士の聖堂守衣", OutpostStage::Encampment, {"controlled_ember_formula"}, {{"herb", 2}},
         {"simple_forge"}, false, "Balanced armor: DEF/RES +1 each at Lv1.",
         "汎用防具: Lv1でDEF/RES各+1。", UnitClass::BattleMage},

        // M10-B (docs/deep_layers.md「防具用調整特性」): "戦闘ごとに最初の
        // 状態異常を1回無効" - the doc's own worked example, single
        // proof-of-mechanism entry for the new armor trait pool (separate
        // from trait_hide_wrapped_grip above).
        {"armor_trait_ward_step", FacilityId::Forge, "Armor Tuning: Ward Step", "防具調整: 一歩の護り",
         OutpostStage::PioneerOutpost, {}, {{"herb", 1}, {"cloth", 1}}, {"simple_forge"}, false,
         "Negates the first status effect the wearer receives each battle.",
         "戦闘ごとに最初に受ける状態異常を1回無効化する。"},

        // docs/regions/ember_ravine.md「鍛冶場「特殊鍛造」「耐熱加工」を研究
        // 可能にする」: same "flavor-only research node gated on a region's own
        // Discovery" shape as Infirmary's pharmacology/Workshop's trapcraft
        // below (neither special forging nor heat-resistant processing has a
        // concrete ItemType/recipe of its own in this project yet, so these
        // nodes are effectJa-only, same known scope limit).
        {"special_forging", FacilityId::Forge, "Special Forging", "特殊鍛造",
         OutpostStage::Encampment, {kSpecialForgingRecordsDiscovery}, {}, {"simple_forge"}, false,
         "Unlocks specialized forging techniques for heat-worked equipment.",
         "耐熱装備向けの特殊鍛造技術を解放する。"},
        {"heat_resistant_processing", FacilityId::Forge, "Heat-Resistant Processing", "耐熱加工",
         OutpostStage::FrontierSettlement, {kHeatResistantProcessingRecordsDiscovery},
         {{"heat_resistant_material", 3}, {"wetland_resin", 1}, {"ash_crystal", 1}}, {"simple_forge"}, false,
         "Unlocks heat-resistant material processing for equipment.",
         "装備への耐熱素材加工を解放する。"},

        // 4. 診療所 (Infirmary)
        {"field_infirmary", FacilityId::Infirmary, "Field Infirmary", "救護テント",
         OutpostStage::PioneerOutpost, {kHerbThicketDiscovery}, {{"wood", 2}, {"herb", 2}}, {}, true,
         "Unlocks the First Aid Kit and Rescue Pack.",
         "応急手当具と救命包を解放する。"},
        {"field_medicine_branch", FacilityId::Infirmary, "Field Medicine", "野戦医療",
         OutpostStage::PioneerOutpost, {kFieldMedicineDiscovery}, {{"wood", 1}, {"herb", 3}, {"quality_herb", 1}},
         {"field_infirmary"}, false,
         "Unlocks the Field Treatment Kit and more advanced treatment gear.",
         "野戦治療具・高度治療具を解放する。"},
        {"lifesaving_technique", FacilityId::Infirmary, "Lifesaving Technique", "救命技術",
         OutpostStage::PioneerOutpost, {}, {{"herb", 3}, {"cloth", 2}, {"military_supplies", 1}}, {"field_infirmary"}, false,
         "Unlocks the upgraded Rescue Pack and increases revive HP.",
         "高級救命包と、復帰時HPの増加を解放する。"},
        {"pharmacology", FacilityId::Infirmary, "Pharmacology", "薬学",
         OutpostStage::FrontierSettlement, {kMarshPharmacologyDiscovery},
         {{"quality_herb", 1}, {"poison_material", 2}, {"wetland_resin", 1}}, {"field_infirmary"}, false,
         "Unlocks the Panacea and cures for special status ailments.",
         "万能薬と特殊状態異常の治療手段を解放する。"},

        // 5. 工房 (Workshop)
        {"workshop_bench", FacilityId::Workshop, "Workshop Bench", "工作台",
         OutpostStage::PioneerOutpost, {}, {{"wood", 3}, {"hide", 1}}, {}, true,
         "Unlocks basic exploration tools.",
         "基本探索道具を解放する。"},
        {"exploration_tools", FacilityId::Workshop, "Exploration Tools", "探索工作",
         OutpostStage::PioneerOutpost, {}, {{"wood", 2}, {"stone", 1}, {"quality_iron", 1}}, {"workshop_bench"}, false,
         "Unlocks climbing gear, mining tools, and lanterns.",
         "登攀具・採掘具・照明具を解放する。"},
        {"combat_tools", FacilityId::Workshop, "Combat Tools", "戦闘工作",
         OutpostStage::PioneerOutpost, {}, {{"wood", 2}, {"iron", 2}, {"military_supplies", 1}}, {"workshop_bench"}, false,
         "Unlocks Protective Boards, caltrops, and smoke tubes.",
         "防護板・鉄杭・煙幕筒を解放する。"},
        {"trapcraft", FacilityId::Workshop, "Trapcraft", "罠技術",
         OutpostStage::FrontierSettlement, {kMarshTrapcraftDiscovery},
         {{"wetland_resin", 2}, {"hardwood", 1}, {"poison_material", 1}}, {"workshop_bench"}, false,
         "Unlocks iron stakes and poison-trap disposal gear.",
         "鉄杭と毒罠処理道具を解放する。"},
        // docs/regions/ember_ravine.md「工房「上位戦闘工作」を研究可能にする」:
        // same flavor-only research node shape as trapcraft above.
        {"advanced_fieldwork", FacilityId::Workshop, "Advanced Fieldwork", "上位戦闘工作",
         OutpostStage::FrontierSettlement, {kAdvancedFieldworkRecordsDiscovery},
         {{"heat_resistant_material", 2}, {"sulfur", 1}, {"ash_crystal", 1}}, {"workshop_bench"}, false,
         "Unlocks advanced fieldwork tools for heat-hazard terrain.",
         "熱害地形向けの上位戦闘工作道具を解放する。"},
        {"advanced_crafting", FacilityId::Workshop, "Advanced Crafting", "高度工作",
         OutpostStage::FrontierSettlement, {kReturnSignalDiscovery},
         {{"iron", 2}, {"signal_core", 1}, {"sulfur", 1}, {"cloth", 1}}, {"workshop_bench"}, false,
         "Unlocks the Return Flare, Protective Case, and special ruin devices.",
         "帰還信号弾・保護箱・特殊遺跡装置を解放する。"},

        // 6. 宿舎 (Barracks)
        {"communal_tent", FacilityId::Barracks, "Communal Tent", "共同テント",
         OutpostStage::Encampment, {}, {}, {}, false,
         "Enables the starting 4 companions and the 4-person formation.",
         "初期仲間4人と4人編成を使用可能にする。"},
        {"barracks_expansion", FacilityId::Barracks, "Barracks Expansion", "宿舎増築",
         OutpostStage::PioneerOutpost, {}, {{"building_material", 2}, {"food", 1}}, {"communal_tent"}, false,
         "Increases the number of companions you can recruit.",
         "新しい仲間を受け入れられる枠を増やす。"},
        {"specialist_quarters", FacilityId::Barracks, "Specialist Quarters", "専門区画",
         OutpostStage::FrontierSettlement, {}, {{"building_material", 3}, {"cloth", 1}, {"military_supplies", 1}},
         {"communal_tent"}, false,
         "Allows recruiting companions of specialist classes.",
         "特殊兵種の仲間を受け入れ可能にする。"},
        {"social_quarters", FacilityId::Barracks, "Social Quarters", "交流区画",
         OutpostStage::PioneerOutpost, {}, {{"building_material", 2}, {"cloth", 2}}, {"communal_tent"}, false,
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
