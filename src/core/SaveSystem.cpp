#include "jf/core/SaveSystem.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

#include <nlohmann/json.hpp>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace jf {
namespace {
using json = nlohmann::json;

std::string classKey(UnitClass unitClass) {
    return std::to_string(static_cast<int>(unitClass));
}

std::optional<UnitClass> classFromKey(const std::string& key) {
    try {
        int value = std::stoi(key);
        if (value < static_cast<int>(UnitClass::MarchCaptain) || value > static_cast<int>(UnitClass::Bandit))
            return std::nullopt;
        return static_cast<UnitClass>(value);
    } catch (...) {
        return std::nullopt;
    }
}

json classMapToJson(const std::unordered_map<UnitClass, std::string>& values) {
    json result = json::object();
    for (const auto& [unitClass, value] : values) result[classKey(unitClass)] = value;
    return result;
}

std::unordered_map<UnitClass, std::string> classMapFromJson(const json& value) {
    std::unordered_map<UnitClass, std::string> result;
    if (!value.is_object()) return result;
    for (const auto& [key, entry] : value.items()) {
        auto unitClass = classFromKey(key);
        if (unitClass && entry.is_string()) result[*unitClass] = entry.get<std::string>();
    }
    return result;
}

void setError(std::string* error, const std::string& message) {
    if (error) *error = message;
}

// Export/Import (docs/save_system.md): both directories sit next to the
// save file itself, so a desktop build's exports/imports move together with
// wherever the save was configured to live.
std::filesystem::path siblingDir(const std::string& savePath, const char* name) {
    std::filesystem::path target(savePath);
    std::filesystem::path base = target.has_parent_path() ? target.parent_path() : std::filesystem::path(".");
    return base / name;
}

// Shared "YYYYMMDD-HHMMSS" stamp for both Export filenames and quarantined
// corrupt-save filenames (docs/save_system.md's Export section and its
// 破損復旧画面 "Start New" section use the same format independently).
std::string timestampSuffix() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif
    std::ostringstream stamp;
    stamp << std::put_time(&local, "%Y%m%d-%H%M%S");
    return stamp.str();
}

std::string timestampedExportName() { return "JOJIFrontier-save-" + timestampSuffix() + ".json"; }

json siteAccessMapToJson(const std::unordered_map<std::string, SiteAccessState>& siteAccess) {
    json result = json::object();
    for (const auto& [key, state] : siteAccess) result[key] = static_cast<int>(state);
    return result;
}

std::unordered_map<std::string, SiteAccessState> siteAccessMapFromJson(const json& value) {
    std::unordered_map<std::string, SiteAccessState> result;
    if (!value.is_object()) return result;
    for (const auto& [key, entry] : value.items()) {
        if (!entry.is_number_integer()) continue;
        int raw = entry.get<int>();
        if (raw < static_cast<int>(SiteAccessState::Unknown) || raw > static_cast<int>(SiteAccessState::Secured))
            continue;
        result[key] = static_cast<SiteAccessState>(raw);
    }
    return result;
}

// Same "raw enum int, same order as ItemType's declaration" convention
// already used by expeditionToJson()'s bag array below - ItemType has no
// string-id table of its own (unlike RegionId/SiteAccessState, which are
// permanent enough to need a strict-parse string form).
json itemStorageToJson(const std::unordered_map<ItemType, int>& itemStorage) {
    json result = json::array();
    for (const auto& [type, count] : itemStorage) {
        if (count <= 0) continue;
        result.push_back({{"type", static_cast<int>(type)}, {"count", count}});
    }
    return result;
}

std::unordered_map<ItemType, int> itemStorageFromJson(const json& value) {
    std::unordered_map<ItemType, int> result;
    if (!value.is_array()) return result;
    for (const json& entry : value) {
        if (!entry.is_object() || !entry.contains("type") || !entry.contains("count")) continue;
        int rawType = entry.at("type").get<int>();
        int count = entry.at("count").get<int>();
        if (rawType < static_cast<int>(ItemType::FirstAidKit) || rawType > static_cast<int>(ItemType::ReturnFlare) ||
            count <= 0)
            continue;
        result[static_cast<ItemType>(rawType)] = count;
    }
    return result;
}

json pendingSiteAccessToJson(const std::vector<std::pair<std::string, SiteAccessState>>& updates) {
    json result = json::array();
    for (const auto& [key, state] : updates) result.push_back({{"key", key}, {"state", static_cast<int>(state)}});
    return result;
}

std::vector<std::pair<std::string, SiteAccessState>> pendingSiteAccessFromJson(const json& value) {
    std::vector<std::pair<std::string, SiteAccessState>> result;
    if (!value.is_array()) return result;
    for (const json& entry : value) {
        if (!entry.is_object() || !entry.contains("key") || !entry.contains("state")) continue;
        int raw = entry.at("state").get<int>();
        if (raw < static_cast<int>(SiteAccessState::Unknown) || raw > static_cast<int>(SiteAccessState::Secured))
            continue;
        result.push_back({entry.at("key").get<std::string>(), static_cast<SiteAccessState>(raw)});
    }
    return result;
}

// docs/inventory_overflow.md「受取保留」: permanent (Base-level, not
// expedition-level) state, so it lives alongside `storage`/`itemStorage`
// rather than inside expeditionToJson()'s disposable checkpoint below.
json rewardOverflowToJson(const RewardOverflowState& overflow) {
    json result = json::array();
    for (const OverflowStack& stack : overflow.stacks)
        result.push_back({{"grantId", stack.grantId}, {"itemId", stack.itemId}, {"quantity", stack.quantity}});
    return result;
}

RewardOverflowState rewardOverflowFromJson(const json& value) {
    RewardOverflowState result;
    if (!value.is_array()) return result;
    for (const json& entry : value) {
        if (!entry.is_object() || !entry.contains("grantId") || !entry.contains("itemId") ||
            !entry.contains("quantity"))
            continue;
        int quantity = entry.at("quantity").get<int>();
        if (quantity <= 0) continue;
        result.stacks.push_back(
            {entry.at("grantId").get<std::string>(), entry.at("itemId").get<std::string>(), quantity});
    }
    return result;
}

json expeditionToJson(const ExpeditionCheckpoint& checkpoint) {
    json pendingLoot = json::array();
    for (const LootStack& stack : checkpoint.pendingLoot)
        pendingLoot.push_back({{"id", stack.id}, {"quantity", stack.quantity}});
    json bag = json::array();
    for (ItemType item : checkpoint.bag) bag.push_back(static_cast<int>(item));
    json partyUnits = json::array();
    for (const auto& unit : checkpoint.partyUnits)
        partyUnits.push_back({{"id", unit.id}, {"currentHp", unit.currentHp}});
    json result = {
        {"stage", checkpoint.stage == ExpeditionCheckpoint::Stage::Camp ? "camp" : "exploration"},
        {"regionId", toString(checkpoint.regionId)},
        {"expeditionStage", checkpoint.expeditionStage},
        {"seed", checkpoint.seed},
        {"pendingLoot", pendingLoot},
        {"pendingDiscoveries", checkpoint.pendingDiscoveries},
        {"bag", bag},
        {"battlesWon", checkpoint.battlesWon},
        {"stageDiscoveryAwarded", checkpoint.stageDiscoveryAwarded},
        {"partyUnits", partyUnits},
        {"pendingSiteAccessUpdates", pendingSiteAccessToJson(checkpoint.pendingSiteAccessUpdates)},
        {"pendingRegionCompletions", [&] {
            std::vector<std::string> ids;
            for (RegionId regionId : checkpoint.pendingRegionCompletions) ids.push_back(toString(regionId));
            std::sort(ids.begin(), ids.end());
            return ids;
        }()},
        {"pendingRecruitCandidateIds", checkpoint.pendingRecruitCandidateIds},
    };
    if (checkpoint.routeProgress) {
        std::vector<std::string> resolved(checkpoint.routeProgress->resolvedNodeIds.begin(),
                                          checkpoint.routeProgress->resolvedNodeIds.end());
        std::vector<std::string> safe(checkpoint.routeProgress->safelyPassedNodeIds.begin(),
                                      checkpoint.routeProgress->safelyPassedNodeIds.end());
        std::sort(resolved.begin(), resolved.end());
        std::sort(safe.begin(), safe.end());
        result["routeProgress"] = {
            {"routeId", checkpoint.routeProgress->routeId},
            {"currentNodeId", checkpoint.routeProgress->currentNodeId},
            {"lastCheckpointNodeId", checkpoint.routeProgress->lastCheckpointNodeId},
            {"resolvedNodeIds", resolved},
            {"safelyPassedNodeIds", safe},
            {"traversalHistory", checkpoint.routeProgress->traversalHistory},
        };
    }
    return result;
}

std::optional<ExpeditionCheckpoint> expeditionFromJson(const json& value) {
    if (!value.is_object()) return std::nullopt;
    ExpeditionCheckpoint checkpoint;
    checkpoint.stage = value.value("stage", std::string("exploration")) == "camp"
                           ? ExpeditionCheckpoint::Stage::Camp
                           : ExpeditionCheckpoint::Stage::Exploration;
    // Missing key (a save predating regionId entirely) means the single
    // battle sequence that existed back then - what's now CinderwatchGate.
    // A key that's present but unrecognized is different: that's corrupt or
    // from a future region this build doesn't know, and must fail outright
    // rather than silently substitute a real region (docs/region_unlocks.md).
    auto regionId = regionIdFromStringStrict(value.value("regionId", std::string("cinderwatch_gate")));
    if (!regionId) return std::nullopt;
    checkpoint.regionId = *regionId;
    checkpoint.expeditionStage = value.value("expeditionStage", 0);
    // No fixed upper bound anymore (regions can have any stage count); just
    // guard against corrupt/negative data.
    if (checkpoint.expeditionStage < 0 || checkpoint.expeditionStage > 100) return std::nullopt;
    checkpoint.seed = value.value("seed", 0u);
    if (value.contains("pendingLoot") && value["pendingLoot"].is_array()) {
        for (const json& entry : value["pendingLoot"]) {
            std::string id = entry.at("id").get<std::string>();
            int quantity = entry.at("quantity").get<int>();
            if (id.empty() || quantity <= 0) return std::nullopt;
            checkpoint.pendingLoot.push_back({id, quantity});
        }
    }
    if (value.contains("pendingDiscoveries") && value["pendingDiscoveries"].is_array())
        checkpoint.pendingDiscoveries = value["pendingDiscoveries"].get<std::vector<std::string>>();
    if (value.contains("bag") && value["bag"].is_array()) {
        for (const json& entry : value["bag"]) {
            int item = entry.get<int>();
            if (item < 0 || item > static_cast<int>(ItemType::ReturnFlare)) return std::nullopt;
            checkpoint.bag.push_back(static_cast<ItemType>(item));
        }
    }
    checkpoint.battlesWon = value.value("battlesWon", 0);
    if (value.contains("routeProgress")) {
        const json& route = value["routeProgress"];
        if (!route.is_object()) return std::nullopt;
        RouteProgressSnapshot progress;
        progress.routeId = route.value("routeId", std::string{});
        progress.currentNodeId = route.value("currentNodeId", std::string{});
        progress.lastCheckpointNodeId = route.value("lastCheckpointNodeId", std::string{});
        if (progress.routeId.empty() || progress.currentNodeId.empty()) return std::nullopt;
        if (route.contains("resolvedNodeIds"))
            for (const std::string& id : route["resolvedNodeIds"].get<std::vector<std::string>>())
                progress.resolvedNodeIds.insert(id);
        if (route.contains("safelyPassedNodeIds"))
            for (const std::string& id : route["safelyPassedNodeIds"].get<std::vector<std::string>>())
                progress.safelyPassedNodeIds.insert(id);
        if (route.contains("traversalHistory"))
            progress.traversalHistory = route["traversalHistory"].get<std::vector<std::string>>();
        checkpoint.routeProgress = std::move(progress);
    }
    if (value.contains("stageDiscoveryAwarded") && value["stageDiscoveryAwarded"].is_array()) {
        const json& flags = value["stageDiscoveryAwarded"];
        checkpoint.stageDiscoveryAwarded.resize(flags.size());
        for (std::size_t i = 0; i < flags.size(); ++i)
            checkpoint.stageDiscoveryAwarded[i] = flags[i].get<bool>();
    }
    if (value.contains("partyUnits") && value["partyUnits"].is_array()) {
        for (const json& entry : value["partyUnits"]) {
            ExpeditionCheckpoint::UnitSnapshot unit;
            unit.id = entry.at("id").get<std::string>();
            unit.currentHp = entry.at("currentHp").get<int>();
            if (unit.id.empty()) return std::nullopt;
            checkpoint.partyUnits.push_back(unit);
        }
    }
    if (value.contains("pendingSiteAccessUpdates"))
        checkpoint.pendingSiteAccessUpdates = pendingSiteAccessFromJson(value["pendingSiteAccessUpdates"]);
    if (value.contains("pendingRegionCompletions") && value["pendingRegionCompletions"].is_array()) {
        for (const json& entry : value["pendingRegionCompletions"]) {
            if (!entry.is_string()) continue;
            // Unlike BaseState::completedRegionIds (permanent), an unknown id
            // here just drops that one pending entry rather than failing the
            // whole (disposable, regenerable) checkpoint.
            if (auto regionId = regionIdFromStringStrict(entry.get<std::string>()))
                checkpoint.pendingRegionCompletions.insert(*regionId);
        }
    }
    if (value.contains("pendingRecruitCandidateIds") && value["pendingRecruitCandidateIds"].is_array()) {
        for (const json& entry : value["pendingRecruitCandidateIds"])
            if (entry.is_string()) checkpoint.pendingRecruitCandidateIds.insert(entry.get<std::string>());
    }
    return checkpoint;
}
} // namespace

std::string serializeSave(const SaveData& save) {
    json storage = json::array();
    for (const LootStack& stack : save.base.storage) storage.push_back({{"id", stack.id}, {"quantity", stack.quantity}});
    json completedRegions = json::array();
    for (RegionId id : save.base.completedRegionIds) completedRegions.push_back(toString(id));

    json root = {
        {"schemaVersion", save.schemaVersion},
        {"gameVersion", save.gameVersion},
        {"base", {
            {"storage", storage},
            {"discoveries", save.base.discoveryRegistry},
            {"outpostStage", static_cast<int>(save.base.outpostStage)},
            {"unlockedNodes", save.base.unlockedNodeIds},
            {"builtNodes", save.base.constructedFacilityIds},
            {"siteAccess", siteAccessMapToJson(save.base.siteAccess)},
            {"completedRegions", completedRegions},
            {"itemStorage", itemStorageToJson(save.base.itemStorage)},
            {"rewardOverflow", rewardOverflowToJson(save.base.rewardOverflow)},
            {"joinReadyCandidateIds", save.base.joinReadyCandidateIds},
            {"joinedRecruitIds", save.base.joinedRecruitIds},
            {"cinderwatchMaterialsEarned", save.base.cinderwatchMaterialsEarned},
        }},
        {"selectedPartyIds", save.selectedPartyIds},
        {"weaponOverrides", classMapToJson(save.weaponOverrides)},
        {"equippedTraits", classMapToJson(save.equippedTraits)},
        {"unitWeaponOverrides", save.unitWeaponOverrides},
        {"unitEquippedTraits", save.unitEquippedTraits},
        {"unitEquippedSkillsSlot0", save.unitEquippedSkillsSlot0},
        {"unitEquippedSkillsSlot1", save.unitEquippedSkillsSlot1},
        {"settings", {{"language", save.language}}},
        {"expedition", save.expedition ? expeditionToJson(*save.expedition) : json(nullptr)},
    };
    return root.dump(2);
}

std::optional<SaveData> deserializeSave(const std::string& jsonText, std::string* error) {
    try {
        json root = json::parse(jsonText);
        if (!root.is_object() || !root.contains("schemaVersion") || !root["schemaVersion"].is_number_integer()) {
            setError(error, "Missing save schema version");
            return std::nullopt;
        }
        const int version = root["schemaVersion"].get<int>();
        if (version < 1 || version > kCurrentSaveSchemaVersion) {
            setError(error, "Unsupported save schema version");
            return std::nullopt;
        }
        if (!root.contains("base") || !root["base"].is_object()) {
            setError(error, "Missing base state");
            return std::nullopt;
        }

        SaveData save;
        save.schemaVersion = version;
        save.gameVersion = root.value("gameVersion", "unknown");
        const json& base = root["base"];
        int stage = base.value("outpostStage", 0);
        if (stage < 0 || stage > static_cast<int>(OutpostStage::PioneerCity)) {
            setError(error, "Invalid outpost stage");
            return std::nullopt;
        }
        save.base.outpostStage = static_cast<OutpostStage>(stage);
        if (base.contains("storage")) {
            if (!base["storage"].is_array()) throw std::runtime_error("Invalid storage");
            for (const json& entry : base["storage"]) {
                std::string id = entry.at("id").get<std::string>();
                int quantity = entry.at("quantity").get<int>();
                if (id.empty() || quantity <= 0) throw std::runtime_error("Invalid loot stack");
                save.base.addStorage(id, quantity);
            }
        }
        if (base.contains("discoveries")) save.base.discoveryRegistry = base["discoveries"].get<std::unordered_set<std::string>>();
        if (base.contains("unlockedNodes")) save.base.unlockedNodeIds = base["unlockedNodes"].get<std::unordered_set<std::string>>();
        if (base.contains("builtNodes")) save.base.constructedFacilityIds = base["builtNodes"].get<std::unordered_set<std::string>>();
        if (base.contains("siteAccess")) save.base.siteAccess = siteAccessMapFromJson(base["siteAccess"]);
        if (base.contains("itemStorage")) save.base.itemStorage = itemStorageFromJson(base["itemStorage"]);
        if (base.contains("rewardOverflow")) save.base.rewardOverflow = rewardOverflowFromJson(base["rewardOverflow"]);
        if (base.contains("joinReadyCandidateIds"))
            save.base.joinReadyCandidateIds = base["joinReadyCandidateIds"].get<std::unordered_set<std::string>>();
        if (base.contains("joinedRecruitIds"))
            save.base.joinedRecruitIds = base["joinedRecruitIds"].get<std::unordered_set<std::string>>();
        if (base.contains("cinderwatchMaterialsEarned"))
            save.base.cinderwatchMaterialsEarned = base["cinderwatchMaterialsEarned"].get<std::unordered_map<std::string, int>>();
        if (base.contains("completedRegions")) {
            if (!base["completedRegions"].is_array()) throw std::runtime_error("Invalid completedRegions");
            for (const json& entry : base["completedRegions"]) {
                auto regionId = regionIdFromStringStrict(entry.get<std::string>());
                // docs/region_unlocks.md: an unrecognized region id here must
                // not be silently dropped - it could mean a future region's
                // completion got lost, which would wrongly re-lock content.
                if (!regionId) throw std::runtime_error("Unknown region id in completedRegions");
                save.base.completedRegionIds.insert(*regionId);
            }
        }
        save.base.unlockedNodeIds.insert("operations_tent");
        save.base.unlockedNodeIds.insert("communal_tent");

        save.selectedPartyIds = root.value("selectedPartyIds", std::vector<std::string>{});
        if (root.contains("weaponOverrides")) save.weaponOverrides = classMapFromJson(root["weaponOverrides"]);
        if (root.contains("equippedTraits")) save.equippedTraits = classMapFromJson(root["equippedTraits"]);
        if (root.contains("unitWeaponOverrides") && root["unitWeaponOverrides"].is_object())
            save.unitWeaponOverrides = root["unitWeaponOverrides"].get<std::unordered_map<std::string, std::string>>();
        if (root.contains("unitEquippedTraits") && root["unitEquippedTraits"].is_object())
            save.unitEquippedTraits = root["unitEquippedTraits"].get<std::unordered_map<std::string, std::string>>();
        if (root.contains("unitEquippedSkillsSlot0") && root["unitEquippedSkillsSlot0"].is_object())
            save.unitEquippedSkillsSlot0 =
                root["unitEquippedSkillsSlot0"].get<std::unordered_map<std::string, std::string>>();
        if (root.contains("unitEquippedSkillsSlot1") && root["unitEquippedSkillsSlot1"].is_object())
            save.unitEquippedSkillsSlot1 =
                root["unitEquippedSkillsSlot1"].get<std::unordered_map<std::string, std::string>>();
        if (root.contains("settings") && root["settings"].is_object())
            save.language = root["settings"].value("language", "en");
        if (save.language != "en" && save.language != "ja") save.language = "en";
        if (root.contains("expedition") && root["expedition"].is_object())
            save.expedition = expeditionFromJson(root["expedition"]);
        return save;
    } catch (const std::exception& exception) {
        setError(error, exception.what());
        return std::nullopt;
    }
}

SaveData migrateSave(SaveData save) {
    while (save.schemaVersion < kCurrentSaveSchemaVersion) {
        if (save.schemaVersion == 1) {
            // v1 -> v2: no field-shape change needed here - deserializeSave()
            // already defaulted every v2-only field (itemStorage,
            // rewardOverflow, unit-level equipment maps, etc.) while parsing,
            // since it reads with `.value()`/`.contains()` guards rather than
            // assuming the field exists. This step exists to advance the
            // version number itself and to give the migration loop a real
            // first iteration to run through.
            save.schemaVersion = 2;
            continue;
        }
        break;  // Unknown version below current: leave as-is rather than loop forever.
    }
    return save;
}

SaveStore::SaveStore(std::string path) : path_(std::move(path)) {}

std::optional<SaveData> SaveStore::load(std::string* error) const {
    auto read = [](const std::string& path) -> std::optional<std::string> {
        std::ifstream input(path);
        if (!input) return std::nullopt;
        std::ostringstream contents;
        contents << input.rdbuf();
        return contents.str();
    };
    // docs/save_system.md「Schema移行」: back up the pre-migration file
    // before applying migrateSave(), and only for an actual old-version read
    // (never touches an already-current-schema file).
    auto migrateAndBackup = [this](const std::string& rawContents, SaveData save) {
        if (save.schemaVersion < kCurrentSaveSchemaVersion) {
            std::ofstream backupFile(path_ + ".schema-v" + std::to_string(save.schemaVersion) + ".bak",
                                     std::ios::trunc);
            backupFile << rawContents;
            save = migrateSave(std::move(save));
        }
        return save;
    };
    if (auto contents = read(path_)) {
        std::string primaryError;
        if (auto save = deserializeSave(*contents, &primaryError)) {
            setError(error, "");
            return migrateAndBackup(*contents, std::move(*save));
        }
        setError(error, primaryError);
    }
    if (auto backup = read(path_ + ".bak")) {
        if (auto save = deserializeSave(*backup, error)) {
            setError(error, "");
            return migrateAndBackup(*backup, std::move(*save));
        }
    }
    return std::nullopt;
}

bool SaveStore::save(const SaveData& data, std::string* error) const {
    namespace fs = std::filesystem;
    try {
        fs::path target(path_);
        if (target.has_parent_path()) fs::create_directories(target.parent_path());
        const fs::path temporary = target.string() + ".tmp";
        const fs::path backup = target.string() + ".bak";
        {
            std::ofstream output(temporary, std::ios::trunc);
            if (!output) throw std::runtime_error("Could not open temporary save file");
            output << serializeSave(data);
            output.flush();
            if (!output) throw std::runtime_error("Could not write save file");
        }
        if (fs::exists(target)) {
            fs::copy_file(target, backup, fs::copy_options::overwrite_existing);
            fs::remove(target);
        }
        fs::rename(temporary, target);
        flushWebSaveStorage();
        return true;
    } catch (const std::exception& exception) {
        setError(error, exception.what());
        return false;
    }
}

bool SaveStore::importFrom(const SaveData& data, std::string* error) const {
    namespace fs = std::filesystem;
    try {
        fs::path target(path_);
        if (fs::exists(target)) fs::copy_file(target, target.string() + ".preimport.bak", fs::copy_options::overwrite_existing);
    } catch (const std::exception& exception) {
        setError(error, exception.what());
        return false;
    }
    return save(data, error);
}

bool SaveStore::restoreFromBackup(std::string* error) const {
    namespace fs = std::filesystem;
    std::vector<fs::path> candidates;
    const fs::path preimport(path_ + ".preimport.bak");
    if (fs::exists(preimport)) candidates.push_back(preimport);

    // "<path>.schema-vN.bak" candidates written by SaveStore::load()'s
    // migration path - collected newest-version-first since a higher N is a
    // more recently-superseded (and thus more complete) snapshot.
    const fs::path target(path_);
    const fs::path parent = target.has_parent_path() ? target.parent_path() : fs::path(".");
    const std::string prefix = target.filename().string() + ".schema-v";
    std::vector<fs::path> schemaBackups;
    if (fs::exists(parent)) {
        for (const auto& entry : fs::directory_iterator(parent)) {
            const std::string name = entry.path().filename().string();
            if (name.rfind(prefix, 0) == 0 && name.size() > 4 && name.compare(name.size() - 4, 4, ".bak") == 0)
                schemaBackups.push_back(entry.path());
        }
    }
    std::sort(schemaBackups.begin(), schemaBackups.end());
    for (auto it = schemaBackups.rbegin(); it != schemaBackups.rend(); ++it) candidates.push_back(*it);

    for (const fs::path& candidate : candidates) {
        std::ifstream input(candidate);
        if (!input) continue;
        std::ostringstream contents;
        contents << input.rdbuf();
        std::string parseError;
        if (auto restored = deserializeSave(contents.str(), &parseError)) {
            if (save(migrateSave(std::move(*restored)), error)) return true;
        }
    }
    setError(error, "No valid backup found");
    return false;
}

bool SaveStore::quarantineCorruptSave(std::string* error) const {
    namespace fs = std::filesystem;
    try {
        fs::path target(path_);
        if (!fs::exists(target)) return true;  // Nothing to quarantine.
        fs::rename(target, target.string() + ".corrupt-" + timestampSuffix() + ".json");
        return true;
    } catch (const std::exception& exception) {
        setError(error, exception.what());
        return false;
    }
}

std::string defaultSavePath() {
#ifdef __EMSCRIPTEN__
    return "/joji-save/save.json";
#else
    return "joji_frontier_save.json";
#endif
}

std::string exportSaveData(const SaveData& data, std::string* error) {
    namespace fs = std::filesystem;
    try {
        fs::path dir = siblingDir(defaultSavePath(), "exports");
        fs::create_directories(dir);
        fs::path target = dir / timestampedExportName();
        std::ofstream output(target, std::ios::trunc);
        if (!output) throw std::runtime_error("Could not open export file");
        output << serializeSave(data);
        output.flush();
        if (!output) throw std::runtime_error("Could not write export file");
        return target.string();
    } catch (const std::exception& exception) {
        setError(error, exception.what());
        return {};
    }
}

std::vector<ImportCandidate> listImportCandidates() {
    namespace fs = std::filesystem;
    std::vector<std::pair<fs::file_time_type, ImportCandidate>> found;
    std::error_code listError;
    fs::path dir = siblingDir(defaultSavePath(), "imports");
    for (const auto& entry : fs::directory_iterator(dir, listError)) {
        if (listError || !entry.is_regular_file() || entry.path().extension() != ".json") continue;
        std::error_code timeError;
        auto writeTime = fs::last_write_time(entry.path(), timeError);
        found.push_back({writeTime, ImportCandidate{entry.path().string(), entry.path().filename().string()}});
    }
    std::sort(found.begin(), found.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    std::vector<ImportCandidate> candidates;
    candidates.reserve(found.size());
    for (auto& [time, candidate] : found) candidates.push_back(std::move(candidate));
    return candidates;
}

std::optional<SaveData> loadImportCandidate(const std::string& path, std::string* error) {
    std::ifstream input(path);
    if (!input) {
        setError(error, "Could not open import file");
        return std::nullopt;
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    return deserializeSave(contents.str(), error);
}

void flushWebSaveStorage() {
#ifdef __EMSCRIPTEN__
    EM_ASM({ FS.syncfs(false, function(error) { if (error) console.error('JOJIFrontier save sync failed', error); }); });
#endif
}

} // namespace jf
