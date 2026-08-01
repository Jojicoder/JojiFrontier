#include "jf/core/Skill.hpp"

#include <fstream>
#include <iostream>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace jf {

namespace {

// データ/ロジック分離方針(docs/implementation_status.md「データ/ロジック
// 分離方針」): スキルのデータ本体は`data/skills.json`へ切り出し、この
// ファイルにはロード処理と参照ロジックだけを残す。呼び出し側API
// (skillRegistry()/findSkill()等、20箇所超)は一切変更しない -
// GameData/BattleStateはこのレジストリに依存させず(戦闘層は今もGameData
// なしで完結できるままにする)、既存の「static localな不変レジストリを
// 返す自己完結フリー関数」という形だけ保ったまま、内部実装だけを
// ハードコードされたC++リテラルからJSON読み込みへ差し替えている。
UnitClass unitClassFromSkillJsonString(const std::string& name) {
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

SkillCategory skillCategoryFromString(const std::string& name) {
    if (name == "Passive") return SkillCategory::Passive;
    if (name == "Reactive") return SkillCategory::Reactive;
    return SkillCategory::Active;
}

SkillUsageType skillUsageTypeFromString(const std::string& name) {
    if (name == "PerTurn") return SkillUsageType::PerTurn;
    if (name == "OncePerBattle") return SkillUsageType::OncePerBattle;
    if (name == "Cooldown2") return SkillUsageType::Cooldown2;
    if (name == "OncePerPhase") return SkillUsageType::OncePerPhase;
    return SkillUsageType::Always;
}

std::vector<SkillDefinition> loadSkillsFromJson() {
    std::vector<SkillDefinition> skills;
    // cwd is always the repo root at runtime (see project convention noted
    // for tools/gen_full_unlock_save.cpp) - same relative path convention
    // every other data/*.json load uses (jf::loadGameData()'s dataDir).
    std::ifstream file("data/skills.json");
    if (!file.is_open()) {
        std::cerr << "Failed to open data file: data/skills.json" << std::endl;
        return skills;
    }
    nlohmann::json parsed;
    try {
        file >> parsed;
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse JSON file data/skills.json: " << e.what() << std::endl;
        return skills;
    }
    for (const auto& s : parsed.at("skills")) {
        SkillDefinition def;
        def.id = s.at("id").get<std::string>();
        def.unitClass = unitClassFromSkillJsonString(s.at("classId").get<std::string>());
        def.nameEn = s.at("nameEn").get<std::string>();
        def.nameJa = s.at("nameJa").get<std::string>();
        def.category = skillCategoryFromString(s.at("category").get<std::string>());
        def.usageType = skillUsageTypeFromString(s.at("usageType").get<std::string>());
        def.effectEn = s.at("effectEn").get<std::string>();
        def.effectJa = s.at("effectJa").get<std::string>();
        def.unlockTier = s.at("unlockTier").get<int>();
        skills.push_back(std::move(def));
    }
    return skills;
}

} // namespace

const std::vector<SkillDefinition>& skillRegistry() {
    static const std::vector<SkillDefinition> skills = loadSkillsFromJson();
    return skills;
}

const SkillDefinition* findSkill(const std::string& id) {
    for (const SkillDefinition& skill : skillRegistry()) {
        if (skill.id == id) return &skill;
    }
    return nullptr;
}

std::vector<const SkillDefinition*> skillsForClass(UnitClass unitClass) {
    std::vector<const SkillDefinition*> result;
    for (const SkillDefinition& skill : skillRegistry()) {
        if (skill.unitClass == unitClass) result.push_back(&skill);
    }
    return result;
}

std::string requiredTrainingNodeIdFor(UnitClass unitClass) {
    switch (unitClass) {
        case UnitClass::VeteranGuard:
        case UnitClass::Spearman:
        case UnitClass::HeavyInfantry:
            return "vanguard_training";
        case UnitClass::WatchArcher:
        case UnitClass::FrontierScout:
        case UnitClass::MessengerCavalry:
        case UnitClass::FrontierRanger:
            return "mobility_training";
        case UnitClass::MarchCaptain:
        case UnitClass::DawnChirurgeon:
        case UnitClass::FrontierEngineer:
        case UnitClass::BannerBearer:
            return "specialist_training";
        case UnitClass::Bandit:
        case UnitClass::Wolf:
        case UnitClass::AshenhornBoar:
        case UnitClass::AshironGrubworm:
        case UnitClass::MarshFangSerpent:
        case UnitClass::PlateauCourierCaptain:
        case UnitClass::RaidLeader:
        case UnitClass::RedbackLizard:
        case UnitClass::FrontierBeast:
            return "";
        // docs/implementation_status.md「装備スキル解放レビュー」#3: previously
        // returned "" like the enemy-only classes below, which made
        // equipSkillForUnit() reject Battle Mage unconditionally - the class
        // could never equip a skill at all. Now uses its own "magic_training"
        // node (Facilities.hpp), same shape as the other 3 branches.
        case UnitClass::BattleMage:
            return "magic_training";
    }
    return "";
}

} // namespace jf
