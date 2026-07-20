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

const StageContentData& GameData::stageContent(const std::string& id) const {
    return stageContentById.at(id);
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

std::optional<ExplorationChoice> explorationChoiceFromString(const std::string& name) {
    if (name == "FrontalAdvance") return ExplorationChoice::FrontalAdvance;
    if (name == "CollapsedSidePath") return ExplorationChoice::CollapsedSidePath;
    if (name == "ScoutRoute") return ExplorationChoice::ScoutRoute;
    return std::nullopt;
}

std::optional<BattleObjectKind> battleObjectKindFromString(const std::string& name) {
    if (name == "Marker") return BattleObjectKind::Marker;
    if (name == "Barrier") return BattleObjectKind::Barrier;
    if (name == "Device") return BattleObjectKind::Device;
    if (name == "Container") return BattleObjectKind::Container;
    if (name == "SpawnPoint") return BattleObjectKind::SpawnPoint;
    if (name == "ExitPoint") return BattleObjectKind::ExitPoint;
    return std::nullopt;
}

namespace {

std::optional<BattleObjectStateKind> battleObjectStateKindFromString(const std::string& name) {
    if (name == "Active") return BattleObjectStateKind::Active;
    if (name == "Disabled") return BattleObjectStateKind::Disabled;
    if (name == "Opened") return BattleObjectStateKind::Opened;
    if (name == "Destroyed") return BattleObjectStateKind::Destroyed;
    return std::nullopt;
}

} // namespace

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
        def.nameKey = c.at("nameKey").get<std::string>();
        def.roleKey = c.at("roleKey").get<std::string>();
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

    auto readLootStacks = [](const json& arr) {
        std::vector<LootStack> result;
        for (const auto& l : arr) result.push_back({l.at("id").get<std::string>(), l.at("quantity").get<int>()});
        return result;
    };

    auto regionsJson = readJsonFile(dataDir + "/regions.json");
    if (!regionsJson || !regionsJson->contains("stages") || !(*regionsJson)["stages"].is_array()) {
        std::cerr << "regions.json must contain a stages array" << std::endl;
        return std::nullopt;
    }
    for (const auto& s : (*regionsJson)["stages"]) {
        StageContentData stage;
        try {
            stage.id = s.at("id").get<std::string>();
            stage.terrainProfileId = s.at("terrainProfileId").get<std::string>();
            if (!data.terrainProfilesById.contains(stage.terrainProfileId)) {
                std::cerr << "Stage " << stage.id << " references unknown terrain profile "
                          << stage.terrainProfileId << std::endl;
                return std::nullopt;
            }
            if (s.contains("enemyRoster")) stage.enemyRoster = readTemplates(s.at("enemyRoster"));
            if (s.contains("baseVictoryLoot")) stage.baseVictoryLoot = readLootStacks(s.at("baseVictoryLoot"));
            if (s.contains("routeVictoryLootDelta")) {
                for (const auto& d : s.at("routeVictoryLootDelta")) {
                    auto choice = explorationChoiceFromString(d.at("choice").get<std::string>());
                    if (!choice) {
                        std::cerr << "Stage " << stage.id << " has an unknown ExplorationChoice" << std::endl;
                        return std::nullopt;
                    }
                    stage.routeVictoryLootDelta.emplace_back(*choice, readLootStacks(d.at("loot")));
                }
            }
            if (s.contains("routeDiscoveries")) {
                for (const auto& d : s.at("routeDiscoveries")) {
                    auto choice = explorationChoiceFromString(d.at("choice").get<std::string>());
                    if (!choice) {
                        std::cerr << "Stage " << stage.id << " has an unknown ExplorationChoice in routeDiscoveries"
                                  << std::endl;
                        return std::nullopt;
                    }
                    stage.routeDiscoveries.emplace_back(*choice, d.at("discoveries").get<std::vector<std::string>>());
                }
            }
            if (s.contains("surveyObjectiveId")) stage.surveyObjectiveId = s.at("surveyObjectiveId").get<std::string>();
            if (s.contains("surveyBonusLoot")) stage.surveyBonusLoot = readLootStacks(s.at("surveyBonusLoot"));
            if (s.contains("surveyTileCount")) stage.surveyTileCount = s.at("surveyTileCount").get<int>();
            if (s.contains("surveyTileObjectDefinitionId"))
                stage.surveyTileObjectDefinitionId = s.at("surveyTileObjectDefinitionId").get<std::string>();
            if (s.contains("discoveries")) stage.discoveries = s.at("discoveries").get<std::vector<std::string>>();
            stage.missionNameEn = s.at("missionNameEn").get<std::string>();
            stage.missionNameJa = s.at("missionNameJa").get<std::string>();

            if (s.contains("routeOutcomes")) {
                for (const auto& o : s.at("routeOutcomes")) {
                    auto choice = explorationChoiceFromString(o.at("choice").get<std::string>());
                    if (!choice) {
                        std::cerr << "Stage " << stage.id << " has an unknown ExplorationChoice in routeOutcomes"
                                  << std::endl;
                        return std::nullopt;
                    }
                    ExplorationOutcome outcome;
                    outcome.partyDamage = o.value("partyDamage", 0);
                    outcome.enemiesRemoved = o.value("enemiesRemoved", 0);
                    outcome.enableFreeDeployment = o.value("enableFreeDeployment", false);
                    outcome.deploymentMaxColumn = o.value("deploymentMaxColumn", 0);
                    if (o.contains("restrictedAutoSpawnMaxColumn"))
                        outcome.restrictedAutoSpawnMaxColumn = o.at("restrictedAutoSpawnMaxColumn").get<int>();
                    outcome.extraBarrierCount = o.value("extraBarrierCount", 0);
                    outcome.enableReinforcementWave = o.value("enableReinforcementWave", false);
                    stage.routeOutcomes.emplace_back(*choice, outcome);
                }
            }
            if (s.contains("scoutRouteRequiredClass")) {
                auto classId = unitClassFromString(s.at("scoutRouteRequiredClass").get<std::string>());
                if (!classId) {
                    std::cerr << "Stage " << stage.id << " has an unknown scoutRouteRequiredClass" << std::endl;
                    return std::nullopt;
                }
                stage.scoutRouteRequiredClass = *classId;
            }
            stage.scoutRouteDisabled = s.value("scoutRouteDisabled", false);
            if (s.contains("timedReinforcement")) {
                const auto& r = s.at("timedReinforcement");
                TimedReinforcementData reinforcement;
                reinforcement.id = r.at("id").get<std::string>();
                reinforcement.spawnRound = r.value("spawnRound", 2);
                reinforcement.spawnPhase =
                    r.value("spawnPhase", std::string("EnemyPhase")) == "PlayerPhase" ? Phase::PlayerPhase
                                                                                       : Phase::EnemyPhase;
                reinforcement.announceRoundsBefore = r.value("announceRoundsBefore", 1);
                reinforcement.requiredForElimination = r.value("requiredForElimination", true);
                reinforcement.units = readTemplates(r.at("units"));
                for (const auto& p : r.at("orderedSpawnCandidates")) {
                    reinforcement.orderedSpawnCandidates.push_back({p.at("row").get<int>(), p.at("col").get<int>()});
                }
                stage.timedReinforcement = std::move(reinforcement);
            }
            if (s.contains("herbPatchGeneration")) {
                const auto& h = s.at("herbPatchGeneration");
                stage.herbPatchGeneration = StageContentData::HerbPatchGenerationData{
                    h.value("count", 2), h.value("zoneMinCol", 0), h.value("zoneMaxCol", kGridCols - 1)};
            }
            if (s.contains("objectPlacementRules")) {
                for (const auto& r : s.at("objectPlacementRules")) {
                    const auto& d = r.at("definition");
                    auto kind = battleObjectKindFromString(d.at("kind").get<std::string>());
                    if (!kind) {
                        std::cerr << "Stage " << stage.id << " has an unknown BattleObjectKind" << std::endl;
                        return std::nullopt;
                    }
                    BattleObjectDefinition definition;
                    definition.definitionId = d.at("definitionId").get<std::string>();
                    definition.kind = *kind;
                    definition.maxDurability = d.value("maxDurability", 0);
                    definition.defense = d.value("defense", 0);
                    definition.resistance = d.value("resistance", 0);
                    definition.canOccupy = d.value("canOccupy", false);
                    definition.blocksMovement = d.value("blocksMovement", false);
                    definition.blocksStopping = d.value("blocksStopping", false);
                    definition.blocksDeployment = d.value("blocksDeployment", false);
                    definition.blocksProjectiles = d.value("blocksProjectiles", false);
                    definition.canBeAttacked = d.value("canBeAttacked", false);
                    definition.canBeRepaired = d.value("canBeRepaired", false);
                    if (d.contains("tags")) {
                        for (const std::string& tag : d.at("tags").get<std::vector<std::string>>())
                            definition.tags.insert(tag);
                    }
                    if (d.contains("interaction")) {
                        const auto& i = d.at("interaction");
                        ObjectInteractionDefinition interaction;
                        interaction.interactionId = i.at("interactionId").get<std::string>();
                        interaction.range = i.value("range", 1);
                        if (i.contains("allowedClasses")) {
                            for (const std::string& className : i.at("allowedClasses").get<std::vector<std::string>>()) {
                                auto classId = unitClassFromString(className);
                                if (!classId) {
                                    std::cerr << "Stage " << stage.id << " has an unknown interaction allowedClass"
                                              << std::endl;
                                    return std::nullopt;
                                }
                                interaction.allowedClasses.insert(*classId);
                            }
                        }
                        if (i.contains("requiredState")) {
                            auto requiredState = battleObjectStateKindFromString(i.at("requiredState").get<std::string>());
                            if (!requiredState) {
                                std::cerr << "Stage " << stage.id << " has an unknown interaction requiredState"
                                          << std::endl;
                                return std::nullopt;
                            }
                            interaction.requiredState = *requiredState;
                        }
                        interaction.maxUses = i.value("maxUses", 1);
                        definition.interaction = interaction;
                    }
                    if (d.contains("interactionResultState")) {
                        auto resultState = battleObjectStateKindFromString(d.at("interactionResultState").get<std::string>());
                        if (!resultState) {
                            std::cerr << "Stage " << stage.id << " has an unknown interactionResultState" << std::endl;
                            return std::nullopt;
                        }
                        definition.interactionResultState = *resultState;
                    }
                    std::vector<std::string> validationErrors;
                    if (!validateObjectDefinition(definition, &validationErrors)) {
                        std::cerr << "Stage " << stage.id << " has an invalid object definition "
                                  << definition.definitionId << ":";
                        for (const std::string& error : validationErrors) std::cerr << " " << error;
                        std::cerr << std::endl;
                        return std::nullopt;
                    }
                    std::optional<std::string> operateObjectiveId;
                    if (r.contains("operateObjectiveId")) {
                        operateObjectiveId = r.at("operateObjectiveId").get<std::string>();
                        if (definition.kind != BattleObjectKind::Device || !definition.interaction) {
                            std::cerr << "Stage " << stage.id
                                      << " has operateObjectiveId on a rule whose definition isn't an interactable "
                                         "Device"
                                      << std::endl;
                            return std::nullopt;
                        }
                    }
                    stage.objectPlacementRules.push_back(StageContentData::ObjectPlacementRuleData{
                        definition, r.at("idPrefix").get<std::string>(), r.value("count", 1),
                        r.value("scalesWithExtraBarrierOutcome", false), r.value("zoneMinCol", 0),
                        r.value("zoneMaxCol", kGridCols - 1), r.value("avoidFirstEnemyRow", false), operateObjectiveId});
                }
            }
            if (s.contains("enemyCountOverride")) stage.enemyCountOverride = s.at("enemyCountOverride").get<std::size_t>();
            if (s.contains("enemyZoneWidth")) stage.enemyZoneWidth = s.at("enemyZoneWidth").get<int>();
            if (s.contains("boostedFirstEnemy")) {
                const auto& b = s.at("boostedFirstEnemy");
                stage.boostedFirstEnemy = StageContentData::BoostedEnemyData{
                    b.at("displayName").get<std::string>(), b.value("maxHpBonus", 0), b.value("defenseBonus", 0)};
            }
            if (s.contains("understaffedReinforcement")) {
                const auto& u = s.at("understaffedReinforcement");
                auto classId = unitClassFromString(u.at("classId").get<std::string>());
                if (!classId) {
                    std::cerr << "Stage " << stage.id << " has an unknown understaffedReinforcement classId"
                              << std::endl;
                    return std::nullopt;
                }
                stage.understaffedReinforcement =
                    UnitTemplate{u.at("id").get<std::string>(), u.at("name").get<std::string>(), *classId};
                stage.understaffedThreshold = s.value("understaffedThreshold", 4);
            }
            if (s.contains("logCollisionBonusLoot"))
                stage.logCollisionBonusLoot = readLootStacks(s.at("logCollisionBonusLoot"));
            if (s.contains("noCasualtiesBonusLoot"))
                stage.noCasualtiesBonusLoot = readLootStacks(s.at("noCasualtiesBonusLoot"));
            if (s.contains("primaryHoldTileAlternative")) {
                const auto& h = s.at("primaryHoldTileAlternative");
                stage.primaryHoldTileAlternative = StageContentData::HoldTileMissionRuleData{
                    h.at("id").get<std::string>(), h.value("requiredHoldRounds", 2), h.value("zoneMinCol", 0),
                    h.value("zoneMaxCol", kGridCols - 1)};
            }
            if (stage.surveyTileCount) {
                if (*stage.surveyTileCount < 1) {
                    std::cerr << "Stage " << stage.id << " has a non-positive surveyTileCount" << std::endl;
                    return std::nullopt;
                }
                if (!stage.surveyObjectiveId) {
                    std::cerr << "Stage " << stage.id << " has surveyTileCount without surveyObjectiveId"
                              << std::endl;
                    return std::nullopt;
                }
                if (stage.herbPatchGeneration) {
                    // BattleFactory.cpp's assembleScenario() prefers HerbPatch
                    // tiles over surveyTileCount whenever both are set, so a
                    // stage combining them would have surveyTileCount silently
                    // ignored - reject the data instead of shipping a dead field.
                    std::cerr << "Stage " << stage.id << " sets both surveyTileCount and herbPatchGeneration"
                              << std::endl;
                    return std::nullopt;
                }
            } else if (stage.surveyTileObjectDefinitionId) {
                std::cerr << "Stage " << stage.id << " has surveyTileObjectDefinitionId without surveyTileCount"
                          << std::endl;
                return std::nullopt;
            }
            {
                // Same-choice entries in routeOutcomes/routeVictoryLootDelta/
                // routeDiscoveries would silently pick "whichever the loop
                // sees last" (routeOutcomes) or double-apply (the two additive
                // lists) rather than the author's intent - almost certainly a
                // copy-paste mistake, so reject it outright.
                auto hasDuplicateChoice = [](const auto& entries) {
                    for (std::size_t i = 0; i < entries.size(); ++i) {
                        for (std::size_t j = i + 1; j < entries.size(); ++j) {
                            if (entries[i].first == entries[j].first) return true;
                        }
                    }
                    return false;
                };
                if (hasDuplicateChoice(stage.routeOutcomes) || hasDuplicateChoice(stage.routeVictoryLootDelta) ||
                    hasDuplicateChoice(stage.routeDiscoveries)) {
                    std::cerr << "Stage " << stage.id
                              << " has a duplicate ExplorationChoice in routeOutcomes/routeVictoryLootDelta/"
                                 "routeDiscoveries"
                              << std::endl;
                    return std::nullopt;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Invalid stage in regions.json: " << e.what() << std::endl;
            return std::nullopt;
        }
        if (!data.stageContentById.emplace(stage.id, std::move(stage)).second) {
            std::cerr << "Duplicate stage id in regions.json: " << s.at("id").get<std::string>() << std::endl;
            return std::nullopt;
        }
    }

    return data;
}

} // namespace jf
