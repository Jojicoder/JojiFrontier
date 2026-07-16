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

const TerrainProfile& GameData::terrainProfile(const std::string& id) const {
    return terrainProfilesById.at(id);
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
        {"Wolf", UnitClass::Wolf},
        {"AshenhornBoar", UnitClass::AshenhornBoar},
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

    auto terrainJson = readJsonFile(dataDir + "/terrain_profiles.json");
    if (!terrainJson || !terrainJson->contains("terrainProfiles") ||
        !(*terrainJson)["terrainProfiles"].is_array()) {
        std::cerr << "terrain_profiles.json must contain a terrainProfiles array" << std::endl;
        return std::nullopt;
    }
    for (const auto& p : (*terrainJson)["terrainProfiles"]) {
        TerrainProfile profile;
        try {
            profile.id = p.at("id").get<std::string>();
            profile.generationVersion = p.at("generationVersion").get<int>();
            profile.seedSalt = p.at("seedSalt").get<std::uint32_t>();
            for (const auto& w : p.at("weights")) {
                auto terrain = terrainTypeFromString(w.at("terrain").get<std::string>());
                if (!terrain) {
                    std::cerr << "Unknown terrain in profile " << profile.id << std::endl;
                    return std::nullopt;
                }
                profile.weights.push_back({*terrain, w.at("weight").get<int>()});
            }
            auto signature = terrainTypeFromString(p.at("signatureTerrain").get<std::string>());
            if (!signature) {
                std::cerr << "Unknown signature terrain in profile " << profile.id << std::endl;
                return std::nullopt;
            }
            profile.signatureTerrain = *signature;
            profile.maxBarriersPerColumn = p.value("maxBarriersPerColumn", 0);
            profile.ensureHorizontalRoute = p.value("ensureHorizontalRoute", true);
            if (p.contains("countBounds")) {
                auto bounded = terrainTypeFromString(p.at("countBounds").at("terrain").get<std::string>());
                if (!bounded) {
                    std::cerr << "Unknown bounded terrain in profile " << profile.id << std::endl;
                    return std::nullopt;
                }
                profile.countBounds = TerrainCountBounds{
                    *bounded,
                    p.at("countBounds").at("minimum").get<int>(),
                    p.at("countBounds").at("maximum").get<int>()};
            }
        } catch (const std::exception& e) {
            std::cerr << "Invalid terrain profile: " << e.what() << std::endl;
            return std::nullopt;
        }
        std::string validationError;
        if (!validateTerrainProfile(profile, &validationError)) {
            std::cerr << "Invalid terrain profile " << profile.id << ": " << validationError << std::endl;
            return std::nullopt;
        }
        if (!data.terrainProfilesById.emplace(profile.id, std::move(profile)).second) {
            std::cerr << "Duplicate terrain profile id" << std::endl;
            return std::nullopt;
        }
    }
    for (const char* requiredId : {kCinderwatchOutpostTerrain, kAshRoadTerrain, kSignalTowerTerrain,
                                   kAshboughVergeTerrain, kHerbwaterHollowTerrain,
                                   kBrokenwoodTerritoryTerrain}) {
        if (!data.terrainProfilesById.contains(requiredId)) {
            std::cerr << "Missing terrain profile referenced by current content: " << requiredId << std::endl;
            return std::nullopt;
        }
    }

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
        if (w.contains("moveModifier")) weapon.moveModifier = w.at("moveModifier").get<int>();
        if (w.contains("braceBoost")) weapon.braceBoost = w.at("braceBoost").get<bool>();
        if (w.contains("causesKnockback")) weapon.causesKnockback = w.at("causesKnockback").get<bool>();
        if (w.contains("onHitStatuses")) {
            for (const std::string& status : w.at("onHitStatuses").get<std::vector<std::string>>()) {
                if (status == "poison") weapon.onHitStatuses.push_back(StatusEffectType::Poison);
                else if (status == "burn") weapon.onHitStatuses.push_back(StatusEffectType::Burn);
                else if (status == "move_down") weapon.onHitStatuses.push_back(StatusEffectType::MoveDown);
                else if (status == "defense_down") weapon.onHitStatuses.push_back(StatusEffectType::DefenseDown);
                else if (status == "stagger") weapon.onHitStatuses.push_back(StatusEffectType::Stagger);
            }
        }
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
