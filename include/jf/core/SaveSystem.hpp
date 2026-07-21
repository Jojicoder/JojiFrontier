#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "jf/core/BaseState.hpp"
#include "jf/core/Item.hpp"
#include "jf/core/Region.hpp"
#include "jf/core/RouteGraph.hpp"
#include "jf/core/UnitClass.hpp"

namespace jf {

inline constexpr int kCurrentSaveSchemaVersion = 2;

// Mid-expedition checkpoint (docs/save_system.md "遠征中断セーブ"). Only two
// resumable checkpoints are persisted - Exploration (party/bag locked in,
// stage 0, no route chosen yet) and Camp (battle just won, rewards banked).
// Mid-battle and mid-deployment state is never saved: quitting there just
// resumes from the last Exploration/Camp checkpoint and the player redoes
// the route pick or continue-expedition step, which regenerates the same
// battle deterministically from the saved seed.
struct ExpeditionCheckpoint {
    enum class Stage { Exploration, Camp };

    struct UnitSnapshot {
        std::string id;
        int currentHp = 0;
    };

    Stage stage = Stage::Exploration;
    // docs/implementation_roadmap.md "Phase 1.5": which region this
    // expedition belongs to. Defaults to CinderwatchGate so a save file
    // written before this field existed still resolves correctly.
    RegionId regionId = RegionId::CinderwatchGate;
    int expeditionStage = 0;
    std::uint32_t seed = 0;
    std::vector<LootStack> pendingLoot;
    std::vector<DiscoveryId> pendingDiscoveries;
    std::vector<ItemType> bag;
    int battlesWon = 0;
    std::optional<RouteProgressSnapshot> routeProgress;
    // Sized to the region's stage count - no longer a fixed 3.
    std::vector<bool> stageDiscoveryAwarded;
    // Phase 3 "周回・地域経路の開拓": site-access promotions earned so far
    // this run, still pending a safe return (see BaseState::siteAccess /
    // ExpeditionState::pendingSiteAccessUpdates).
    std::vector<std::pair<std::string, SiteAccessState>> pendingSiteAccessUpdates;
    // docs/region_mission_data_contract.md "PendingRegionCompletion": mirrors
    // ExpeditionState::pendingRegionCompletions.
    std::unordered_set<RegionId> pendingRegionCompletions;
    // docs/roster_design.md「加入処理の共通ルール」: mirrors ExpeditionState::
    // pendingRecruitCandidateIds, so a pending recruit candidate survives an
    // app restart mid-expedition the same way pendingDiscoveries does.
    std::unordered_set<std::string> pendingRecruitCandidateIds;
    // Populated at both resumable stages so HP/incapacitation survives a
    // site-to-site Exploration checkpoint as well as Camp.
    std::vector<UnitSnapshot> partyUnits;
};

struct SaveData {
    int schemaVersion = kCurrentSaveSchemaVersion;
    std::string gameVersion = "0.1.0";
    BaseState base;
    std::vector<std::string> selectedPartyIds;
    std::unordered_map<UnitClass, std::string> weaponOverrides;
    std::unordered_map<UnitClass, std::string> equippedTraits;
    std::unordered_map<std::string, std::string> unitWeaponOverrides;
    std::unordered_map<std::string, std::string> unitEquippedTraits;
    // docs/skill_system.md "保存データ": the equippedSkills half of that
    // doc's UnitLoadout - one flat map per slot index, mirroring
    // unitWeaponOverrides/unitEquippedTraits's shape. Empty string / no
    // entry means nothing equipped in that slot.
    std::unordered_map<std::string, std::string> unitEquippedSkillsSlot0;
    std::unordered_map<std::string, std::string> unitEquippedSkillsSlot1;
    std::string language = "en";
    std::optional<ExpeditionCheckpoint> expedition;
};

std::string serializeSave(const SaveData& save);
std::optional<SaveData> deserializeSave(const std::string& jsonText, std::string* error = nullptr);

// docs/save_system.md「Schema移行」: applies one `vN -> vN+1` step at a time
// up to kCurrentSaveSchemaVersion. deserializeSave() above already fills
// every new field with a safe default when reading an older file's JSON, so
// today's only real step (v1 -> v2) has no field-shape work left to do -
// this exists so the *version number* itself gets bumped, and so the
// one-step-at-a-time loop structure is already in place for when Schema 3
// (docs/save_system.md's planned facility rework) needs actual per-field
// transformation.
SaveData migrateSave(SaveData save);

class SaveStore {
public:
    explicit SaveStore(std::string path);
    std::optional<SaveData> load(std::string* error = nullptr) const;
    bool save(const SaveData& data, std::string* error = nullptr) const;
    // Import (docs/save_system.md "Export / Import"): preserves the
    // pre-import save as "<path>.preimport.bak" (distinct from the regular
    // ".bak" safety copy) before replacing it with `data`.
    bool importFrom(const SaveData& data, std::string* error = nullptr) const;
    const std::string& path() const { return path_; }

    // docs/save_system.md「破損復旧画面」: tries backup sources load() itself
    // doesn't automatically fall back to - "<path>.preimport.bak" and any
    // "<path>.schema-vN.bak" files (newest N first) - since by the time this
    // is worth calling, load()'s own silent ".bak" fallback has already been
    // tried and failed too. On success, writes the recovered (migrated) data
    // back as the new primary save and returns true.
    bool restoreFromBackup(std::string* error = nullptr) const;

    // docs/save_system.md「破損復旧画面」"Start New": moves the current
    // (corrupt) primary save file aside to "<path>.corrupt-YYYYMMDD-HHMMSS.json"
    // rather than deleting it. No-op (returns true) if no primary file exists.
    bool quarantineCorruptSave(std::string* error = nullptr) const;

private:
    std::string path_;
};

std::string defaultSavePath();
void flushWebSaveStorage();

// Export: writes `data` as a timestamped JSON file under `exports/` (next to
// the save file) and returns its path, or an empty string on failure.
std::string exportSaveData(const SaveData& data, std::string* error = nullptr);

struct ImportCandidate {
    std::string path;
    std::string filename;
};

// Import candidates: every `*.json` file under `imports/` (next to the save
// file), newest first. This is the desktop fallback described in
// save_system.md for platforms without a native file-open dialog.
std::vector<ImportCandidate> listImportCandidates();
std::optional<SaveData> loadImportCandidate(const std::string& path, std::string* error = nullptr);

} // namespace jf
