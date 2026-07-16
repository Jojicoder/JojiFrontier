#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "jf/battle/BattleObject.hpp"
#include "jf/battle/Phase.hpp"
#include "jf/core/BaseState.hpp"
#include "jf/core/Exploration.hpp"
#include "jf/core/Grid.hpp"
#include "jf/core/Stats.hpp"
#include "jf/core/TerrainProfile.hpp"
#include "jf/core/UnitClass.hpp"
#include "jf/core/Weapon.hpp"

namespace jf {

struct ClassDefinition {
    UnitClass id{};
    Stats baseStats;
    std::string weaponId;
    // docs/implementation_roadmap.md M1-E slice5: display Locale Keys, so
    // main.cpp's classNameFor()/classRoleFor() can look these up from data
    // instead of a UnitClass switch that needs a new case every time a
    // class is added.
    std::string nameKey;
    std::string roleKey;
};

struct UnitTemplate {
    std::string id;
    std::string name;
    UnitClass classId{};
};

// docs/implementation_roadmap.md M1-E slice1/2: the JSON-loadable subset of
// `jf::StageDescriptor` (jf/core/Region.hpp). Deliberately NOT the full
// StageDescriptor - that type lives in Region.hpp, which already includes
// this header for UnitTemplate/GameData, so GameData.hpp including it back
// would cycle (same reasoning as RegionId living in BaseState.hpp instead of
// Region.hpp - see that comment). Region.cpp's regionDescriptor() converts
// each StageContentData it needs into a full StageDescriptor, filling in
// whatever fields this Schema doesn't cover yet (still authored directly in
// C++ there) - "段階的にJSON化" per implementation_roadmap.md's M1-E slice1
// discussion: only the fields simple stages actually use are covered so far
// (proven end-to-end by Ashbough Verge, then extended to Herbwater Hollow -
// see `routeOutcomes`/`scoutRouteRequiredClass`/`timedReinforcement`/
// `herbPatchGeneration` below); richer stages (Brokenwood Territory, the
// Cinderwatch trio) keep their remaining fields (`objectPlacementRules`,
// `understaffedReinforcement`, `boostedFirstEnemy`, `enemyCountOverride`,
// `logCollisionBonusLoot`/`noCasualtiesBonusLoot`, `scoutRouteDisabled`
// combined with a Boss roster, etc.) authored in Region.cpp until a later
// slice extends this Schema to cover them too.
// Mirrors StageDescriptor::TimedReinforcement field-for-field (can't reuse
// that type directly - see StageContentData's comment on the include
// cycle). Region.cpp's stageDescriptorFromContent() converts this 1:1.
struct TimedReinforcementData {
    std::string id;
    int spawnRound = 2;
    Phase spawnPhase = Phase::EnemyPhase;
    int announceRoundsBefore = 1;
    bool requiredForElimination = true;
    std::vector<UnitTemplate> units;
    std::vector<GridPos> orderedSpawnCandidates;
};

struct StageContentData {
    std::string id;
    std::string terrainProfileId;
    std::vector<UnitTemplate> enemyRoster;
    std::vector<LootStack> baseVictoryLoot;
    std::vector<std::pair<ExplorationChoice, std::vector<LootStack>>> routeVictoryLootDelta;
    std::optional<std::string> surveyObjectiveId;
    std::vector<LootStack> surveyBonusLoot;
    std::vector<DiscoveryId> discoveries;
    std::string missionNameEn;
    std::string missionNameJa;

    // docs/implementation_roadmap.md M1-E slice1続き: extends the covered
    // Schema past Ashbough Verge's simple case to Herbwater Hollow's.
    std::vector<std::pair<ExplorationChoice, ExplorationOutcome>> routeOutcomes;
    std::optional<UnitClass> scoutRouteRequiredClass;
    bool scoutRouteDisabled = false;
    std::optional<TimedReinforcementData> timedReinforcement;
    // Mirrors StageDescriptor::HerbPatchGenerationRule.
    struct HerbPatchGenerationData {
        int count = 2;
        int zoneMinCol = 0;
        int zoneMaxCol = kGridCols - 1;
    };
    std::optional<HerbPatchGenerationData> herbPatchGeneration;

    // docs/implementation_roadmap.md M1-E slice1続き: extends the covered
    // Schema to Brokenwood Territory's and the Cinderwatch trio's remaining
    // fields. `BattleObjectDefinition` itself is safe to embed directly
    // (unlike StageDescriptor, it doesn't include GameData.hpp back - no
    // cycle), so this mirrors StageDescriptor::ObjectPlacementRule exactly.
    struct ObjectPlacementRuleData {
        BattleObjectDefinition definition;
        std::string idPrefix;
        int count = 1;
        bool scalesWithExtraBarrierOutcome = false;
        int zoneMinCol = 0;
        int zoneMaxCol = kGridCols - 1;
        bool avoidFirstEnemyRow = false;
    };
    std::vector<ObjectPlacementRuleData> objectPlacementRules;
    std::optional<std::size_t> enemyCountOverride;
    std::optional<int> enemyZoneWidth;
    struct BoostedEnemyData {
        std::string displayName;
        int maxHpBonus = 0;
        int defenseBonus = 0;
    };
    std::optional<BoostedEnemyData> boostedFirstEnemy;
    std::optional<UnitTemplate> understaffedReinforcement;
    int understaffedThreshold = 4;
    std::vector<LootStack> logCollisionBonusLoot;
    std::vector<LootStack> noCasualtiesBonusLoot;
};

struct GameData {
    std::unordered_map<std::string, Weapon> weaponsById;
    std::unordered_map<std::string, TerrainProfile> terrainProfilesById;
    std::unordered_map<UnitClass, ClassDefinition> classesById;
    std::unordered_map<std::string, StageContentData> stageContentById;
    std::vector<UnitTemplate> playerParty;
    std::vector<UnitTemplate> reserveRoster;
    std::vector<UnitTemplate> enemyRoster;

    const Weapon& weaponFor(UnitClass unitClass) const;
    const ClassDefinition& classDefinition(UnitClass unitClass) const;
    const TerrainProfile& terrainProfile(const std::string& id) const;
    const StageContentData& stageContent(const std::string& id) const;
};

std::optional<UnitClass> unitClassFromString(const std::string& name);
std::optional<ExplorationChoice> explorationChoiceFromString(const std::string& name);
std::optional<BattleObjectKind> battleObjectKindFromString(const std::string& name);

// Loads terrain_profiles.json, classes.json, units.json, weapons.json, and
// regions.json from dataDir. Returns std::nullopt (and prints diagnostics)
// on failure.
std::optional<GameData> loadGameData(const std::string& dataDir);

} // namespace jf
