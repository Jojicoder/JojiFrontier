#include "jf/data/GameData.hpp"

#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

namespace jf {

using json = nlohmann::json;

const Weapon& GameData::weaponFor(UnitClass unitClass) const {
    return weaponsById.at(classDefinition(unitClass).weaponId);
}

const ClassDefinition& GameData::classDefinition(UnitClass unitClass) const {
    return classesById.at(unitClass);
}

std::optional<UnitClass> unitClassFromString(const std::string& name) {
    static const std::unordered_map<std::string, UnitClass> lookup = {
        {"MarchCaptain", UnitClass::MarchCaptain},
        {"VeteranGuard", UnitClass::VeteranGuard},
        {"WatchArcher", UnitClass::WatchArcher},
        {"FrontierScout", UnitClass::FrontierScout},
        {"Spearman", UnitClass::Spearman},
        {"DawnChirurgeon", UnitClass::DawnChirurgeon},
        {"Bandit", UnitClass::Bandit},
    };
    auto it = lookup.find(name);
    if (it == lookup.end()) return std::nullopt;
    return it->second;
}

namespace {

DamageType damageTypeFromString(const std::string& name) {
    return name == "Magical" ? DamageType::Magical : DamageType::Physical;
}

std::optional<json> readJsonFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open data file: " << path << std::endl;
        return std::nullopt;
    }
    try {
        json parsed;
        file >> parsed;
        return parsed;
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse JSON file " << path << ": " << e.what() << std::endl;
        return std::nullopt;
    }
}

} // namespace

std::optional<GameData> loadGameData(const std::string& dataDir) {
    GameData data;

    auto weaponsJson = readJsonFile(dataDir + "/weapons.json");
    if (!weaponsJson) return std::nullopt;
    for (const auto& w : (*weaponsJson)["weapons"]) {
        Weapon weapon;
        weapon.id = w.at("id").get<std::string>();
        weapon.name = w.at("name").get<std::string>();
        weapon.might = w.at("might").get<int>();
        weapon.minRange = w.at("minRange").get<int>();
        weapon.maxRange = w.at("maxRange").get<int>();
        weapon.damageType = damageTypeFromString(w.at("damageType").get<std::string>());
        data.weaponsById[weapon.id] = weapon;
    }

    auto classesJson = readJsonFile(dataDir + "/classes.json");
    if (!classesJson) return std::nullopt;
    for (const auto& c : (*classesJson)["classes"]) {
        auto classId = unitClassFromString(c.at("id").get<std::string>());
        if (!classId) continue;

        ClassDefinition def;
        def.id = *classId;
        const auto& s = c.at("baseStats");
        def.baseStats.maxHp = s.at("maxHp").get<int>();
        def.baseStats.strength = s.at("strength").get<int>();
        def.baseStats.magic = s.at("magic").get<int>();
        def.baseStats.speed = s.at("speed").get<int>();
        def.baseStats.defense = s.at("defense").get<int>();
        def.baseStats.resistance = s.at("resistance").get<int>();
        def.baseStats.move = s.at("move").get<int>();
        def.weaponId = c.at("weaponId").get<std::string>();
        data.classesById[def.id] = def;
    }

    auto unitsJson = readJsonFile(dataDir + "/units.json");
    if (!unitsJson) return std::nullopt;

    auto readTemplates = [](const json& arr) {
        std::vector<UnitTemplate> result;
        for (const auto& u : arr) {
            auto classId = unitClassFromString(u.at("classId").get<std::string>());
            if (!classId) continue;
            UnitTemplate t;
            t.id = u.at("id").get<std::string>();
            t.name = u.at("name").get<std::string>();
            t.classId = *classId;
            result.push_back(std::move(t));
        }
        return result;
    };

    data.playerParty = readTemplates((*unitsJson)["playerParty"]);
    if (unitsJson->contains("reserveRoster"))
        data.reserveRoster = readTemplates((*unitsJson)["reserveRoster"]);
    data.enemyRoster = readTemplates((*unitsJson)["enemyRoster"]);

    return data;
}

} // namespace jf
