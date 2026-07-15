#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "jf/core/BaseState.hpp"
#include "jf/core/Exploration.hpp"
#include "jf/core/UnitClass.hpp"
#include "jf/data/GameData.hpp"

namespace jf {

// RegionId itself lives in jf/core/BaseState.hpp (see that header for why -
// BaseState::completedRegionIds needs it as a complete type, and BaseState.hpp
// can't include this header back).

// One battle's worth of region data. `enemyRoster` empty means "use
// GameData::enemyRoster" (Cinderwatch's existing shared roster); a non-empty
// roster (e.g. Ashbough Verge's wolves) is self-contained region data and
// never touches the shared roster.
struct StageDescriptor {
    std::string id;
    std::string terrainProfileId;

    std::vector<UnitTemplate> enemyRoster;
    // Cinderwatch stage 0's "only 3 of the 4-unit roster appear" rule.
    std::optional<std::size_t> enemyCountOverride;
    // Cinderwatch stage 2's "Former Captain" reskin/buff of enemyRoster[0].
    struct BoostedEnemy {
        std::string displayName;
        int maxHpBonus = 0;
        int defenseBonus = 0;
    };
    std::optional<BoostedEnemy> boostedFirstEnemy;

    std::vector<LootStack> baseVictoryLoot;
    // Additive (possibly negative) deltas on top of baseVictoryLoot for a
    // specific exploration route (docs/regions/ashbough_forest.md's
    // per-route reward table). Empty for stages whose reward doesn't vary
    // by route - true today of all 3 existing Cinderwatch stages.
    std::vector<std::pair<ExplorationChoice, std::vector<LootStack>>> routeVictoryLootDelta;
    // If set, this stage has a SecureTile objective with this id; on
    // success, surveyBonusLoot is added on top of the route-adjusted total.
    // BattleFactory decides how many tiles/objectives to generate for it:
    // one random tile in the enemy zone by default, or one per HerbPatch
    // tile the stage's terrain generation placed (docs/regions/
    // ashbough_forest.md "薬草の沢"'s 2-tile "薬草地点確保"), grouped as Any
    // either way - see BattleFactory.cpp's assembleScenario().
    std::optional<std::string> surveyObjectiveId;
    std::vector<LootStack> surveyBonusLoot;

    // Per-route outcome override (docs/regions/ashbough_forest.md: each
    // site's 3 exploration choices can have genuinely different effects, not
    // just different numbers plugged into the same shared shape). Empty
    // means "use cinderwatchOutcome(choice)" for every choice, preserving
    // every existing stage's behavior exactly.
    std::vector<std::pair<ExplorationChoice, ExplorationOutcome>> routeOutcomes;
    // ScoutRoute is gated on the party having a Frontier Scout by default;
    // a stage can require a different class instead (docs/regions/
    // ashbough_forest.md "薬草の沢"'s 衛生兵 route needs a Dawn Chirurgeon).
    std::optional<UnitClass> scoutRouteRequiredClass;
    // docs/regions/ashbough_forest.md "折れ木の縄張り"'s route C needs 辺境猟兵
    // (Frontier Hunter), a recruit-only class that doesn't exist yet ("Cは
    // 初回攻略用ではなく再訪・再挑戦用の選択肢とする"); rejects ScoutRoute
    // outright regardless of party composition, rather than mis-gating it on
    // scoutRouteRequiredClass's default (Frontier Scout).
    bool scoutRouteDisabled = false;

    // Ad-hoc, reward-only secondary bonuses that don't fit EliminateTeam/
    // DefeatUnit/SecureTile (docs/regions/ashbough_forest.md "折れ木の縄張り"):
    // colliding the boss into a fallen log at least once, and winning with
    // zero party members incapacitated. Checked directly against
    // BattleState/battle.units() at proceedToCamp() time rather than
    // through the Objective system.
    std::vector<LootStack> logCollisionBonusLoot;
    std::vector<LootStack> noCasualtiesBonusLoot;

    std::vector<DiscoveryId> discoveries;
    std::string missionNameEn;
    std::string missionNameJa;
    // False for a route node whose place/name/transition is registered but
    // whose exploration choices and battle content belong to a later phase.
    bool contentImplemented = true;
};

struct RegionDescriptor {
    RegionId id;
    std::string displayNameEn;
    std::string displayNameJa;
    std::vector<StageDescriptor> stages;
};

// Builds the descriptor for `id`. Needs `data` only to source Cinderwatch's
// shared `enemyRoster`/`playerParty` sizing; Ashbough Verge's wolves are
// fully self-contained in the returned StageDescriptor and don't read from
// `data` at all.
RegionDescriptor regionDescriptor(RegionId id, const GameData& data);

// Stable ASCII ids for save data (docs/save_system.md's ID-not-string-name
// convention). An unrecognized string falls back to CinderwatchGate, same
// as a save file predating this field entirely.
std::string toString(RegionId id);
RegionId regionIdFromString(const std::string& id);
// Strict variant for save data that must not silently misinterpret an
// unknown id as a real region (docs/region_unlocks.md: "未知のRegion IDは
// 無視して保存を上書きせず、復旧画面へ送る"). std::nullopt for any string
// that isn't a recognized id, so the caller can fail the whole deserialize
// instead of quietly substituting a region the save never actually meant.
std::optional<RegionId> regionIdFromStringStrict(const std::string& id);

// Looks up `stage.routeOutcomes` for `choice`; falls back to
// cinderwatchOutcome(choice) if the stage doesn't override it. The single
// place GameApp/BattleFactory resolve "what does picking this route do"
// so a stage with bespoke route effects doesn't need a parallel GameApp
// method.
ExplorationOutcome stageRouteOutcome(const StageDescriptor& stage, ExplorationChoice choice);

// Additive reward computation (docs/regions/ashbough_forest.md): base loot,
// plus this route's delta (if any), plus the survey bonus if that stage
// defines a survey objective and it succeeded. Never returns a negative or
// zero-quantity stack.
std::vector<LootStack> computeStageVictoryLoot(const StageDescriptor& stage, ExplorationChoice choice,
                                               bool surveyObjectiveSucceeded);

// Stable key for BaseState::siteAccess / save data: a site is identified by
// which region it's in plus its StageDescriptor::id, since ids are only
// unique within a region.
std::string siteAccessKey(RegionId regionId, const std::string& stageId);

// docs/region_unlocks.md: migration-only heuristic, NOT the live unlock
// gate (see regionUnlocked() below). Checks whether every currently-defined
// stage of `regionId` has been Surveyed (or better). This existed as the
// live gate in an earlier draft of Phase 3.5 and was wrong: it let a single
// implemented location (灰枝の林縁) stand in for the region's real,
// multi-location completion condition. Kept only for a future one-time
// save migration (regionCleared() at the moment a save predating
// completedRegionIds is loaded), per that doc's "Schema 2移行時だけ...".
bool regionCleared(RegionId regionId, const BaseState& base, const GameData& data);

// docs/implementation_roadmap.md "Phase 3.5" / docs/region_unlocks.md: which
// regions are currently selectable from the Base screen. Ashbough Forest
// starts unlocked; every other region requires the previous one's id in
// BaseState::completedRegionIds (today: just CinderwatchGate requiring
// AshboughForest). Nothing sets AshboughForest's completion yet - its real
// completion condition (regions/ashbough_forest.md: defeat 灰角大猪, secure
// all 3 locations) needs Phase 4 - so CinderwatchGate stays locked in the
// current build. This is intentional, not a bug: a single cleared location
// must not stand in for the whole region.
bool regionUnlocked(RegionId regionId, const BaseState& base, const GameData& data);

} // namespace jf
