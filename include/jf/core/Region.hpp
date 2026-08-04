#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "jf/battle/BattleObject.hpp"
#include "jf/core/BaseState.hpp"
#include "jf/core/Exploration.hpp"
#include "jf/core/UnitClass.hpp"
#include "jf/battle/Phase.hpp"
#include "jf/core/Grid.hpp"
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
    // docs/regions/ashbough_forest.md "折れ木の縄張り": "灰角大猪は右2列の候補
    // から生成する" - narrower than the usual 3-column enemy spawn zone.
    // nullopt means the usual width (kSpawnZoneWidth, BattleFactory.cpp).
    std::optional<int> enemyZoneWidth;
    // Cinderwatch site 6's (最後の信号) "Former Captain" reskin/buff of
    // enemyRoster[0] - docs/regions/cinderwatch_gate.md「地域ボス 元守備隊長」's
    // "現行試作の行軍隊長基礎値にHP+10、DEF+2、STR+2".
    struct BoostedEnemy {
        std::string displayName;
        int maxHpBonus = 0;
        int defenseBonus = 0;
        int strengthBonus = 0;
    };
    std::optional<BoostedEnemy> boostedFirstEnemy;
    // docs/regions/ashbough_forest.md "折れ木の縄張り": if the party arrives
    // with fewer living members than understaffedThreshold (casualties
    // earlier in the same expedition), one extra copy of this template
    // spawns as Team::Enemy - reinforcement that reacts to how depleted the
    // incoming party already is, rather than a flat per-region stat/
    // headcount bump (docs' "原則として上げないもの" keeps uniform stat
    // multipliers off the table; this only ever adds the one unit).
    std::optional<UnitTemplate> understaffedReinforcement;
    int understaffedThreshold = 4;

    struct TimedReinforcement {
        std::string id;
        int spawnRound = 2;
        Phase spawnPhase = Phase::EnemyPhase;
        int announceRoundsBefore = 1;
        bool requiredForElimination = true;
        std::vector<UnitTemplate> units;
        std::vector<GridPos> orderedSpawnCandidates;
    };
    std::optional<TimedReinforcement> timedReinforcement;

    // docs/regions/windscar_plateau.md "強風ルール": fixed per-battle wind
    // direction (`delta`, a 1-tile push offset) + trigger Round, consumed by
    // BattleFactory.cpp into BattleState::setWindGust(). Unset for every
    // stage without a WindGust-bearing terrain profile.
    struct WindGustRule {
        GridPos delta;
        int triggerRound = 0;
    };
    std::optional<WindGustRule> windGust;

    // docs/regions/blackwater_lowlands.md「5. 黒水渡し」's 荷運び役 escort
    // targets: Team::Player units (Unit::isGuest = true) spawned alongside
    // playerParty/enemyRoster, whose ids BattleFactory registers into
    // BattleState::missionState().guestUnitIds (see BattleState::
    // allGuestsLost()). Empty for every stage except blackwater_crossing.
    struct GuestUnitData {
        UnitTemplate unitTemplate;
        GridPos spawnPos;
    };
    std::vector<GuestUnitData> guestUnits;

    // docs/implementation_roadmap.md M1-E「M9前ブロッカー」項目2: unified reward
    // rule list (jf/data/GameData.hpp's RewardRule) - replaces the old parallel
    // baseVictoryLoot/routeVictoryLootDelta/surveyBonusLoot fields.
    // computeStageVictoryLoot() (Region.cpp) is still the sole consumer.
    std::vector<RewardRule> victoryRewardRules;
    // Same idea as victoryRewardRules' RouteChoice rules, but for stage.discoveries
    // (docs/regions/cinderwatch_gate.md "3. アイアンウォッチ物資庫"'s
    // per-route records: 医療区画確保→野戦医療記録, 工具庫確保→野戦工作記録).
    // Additive on top of the unconditional `discoveries` list, not a
    // replacement - see computeStageDiscoveries().
    std::vector<std::pair<ExplorationChoice, std::vector<DiscoveryId>>> routeDiscoveries;
    // If set, this stage has a SecureTile objective with this id; on
    // success, victoryRewardRules' SurveySuccess rule(s) are added on top of
    // the route-adjusted total. BattleFactory decides how many tiles/objectives
    // to generate for it:
    // one per HerbPatch tile the stage's terrain generation placed
    // (docs/regions/ashbough_forest.md "薬草の沢"'s 2-tile "薬草地点確保") if
    // any were placed; otherwise `surveyTileCount` plain (no-terrain-change)
    // tiles via chooseSurveyTiles() (docs/regions/cinderwatch_gate.md
    // "3. アイアンウォッチ物資庫"'s 2-crate "物資箱確保" - HerbPatch's own
    // healing-on-end-turn effect doesn't belong on a supply crate, so this
    // path never touches Terrain), or a single such tile if
    // `surveyTileCount` is unset - grouped as Any either way, see
    // BattleFactory.cpp's assembleScenario().
    std::optional<std::string> surveyObjectiveId;
    std::optional<int> surveyTileCount;
    // If set alongside surveyTileCount, a non-blocking BattleObjectKind::
    // Container is placed at each survey tile (docs/regions/
    // cinderwatch_gate.md "3. アイアンウォッチ物資庫"'s "物資箱2個" - without
    // this, the SecureTile targets are invisible, so a player can't tell
    // which tile is "the crate"). Ignored when herbPatchGeneration supplied
    // the tiles instead (already visibly marked by its own Terrain).
    std::optional<std::string> surveyTileObjectDefinitionId;

    // docs/implementation_roadmap.md M1-E slice3 "薬草地点...はBattle Object/
    // Placement Ruleとして配置し、地点名で判定しない": generalizes what used
    // to be `if (stage.terrainProfileId == kHerbwaterHollowTerrain)` in
    // BattleFactory.cpp into region data, same idea as
    // `ObjectPlacementRule` above for the fallen_log Barrier. When set,
    // `count` HerbPatch Terrain tiles are chosen (post Unit-placement, same
    // timing as `chooseSurveyTile()`) from `[zoneMinCol, zoneMaxCol]`; if
    // `surveyObjectiveId` is also set, one SecureTile Objective per chosen
    // tile replaces the single-random-tile fallback.
    struct HerbPatchGenerationRule {
        int count = 2;
        int zoneMinCol = 0;
        int zoneMaxCol = kGridCols - 1;
    };
    std::optional<HerbPatchGenerationRule> herbPatchGeneration;

    // docs/regions/cinderwatch_gate.md「2. 灰道の監視所」/
    // docs/mission_objectives.md「地点維持」: this stage's primary objective
    // is EliminateTeam OR holding a WatchPost tile for requiredHoldRounds
    // consecutive rounds - unlike every other stage (plain EliminateTeam via
    // BattleState's default mission ctor), so BattleFactory rewrites the
    // default "primary" group's rule to Any and adds the HoldTile member
    // when this is set. The tile itself is chosen at scenario-build time
    // (like chooseSurveyTile()) from whichever WatchPost terrain tiles the
    // stage's own TerrainProfile generated within [zoneMinCol, zoneMaxCol];
    // falls back to any WatchPost tile on the board if none generated in
    // that column range.
    struct HoldTileMissionRule {
        std::string id;
        int requiredHoldRounds = 2;
        int zoneMinCol = 0;
        int zoneMaxCol = kGridCols - 1;
    };
    std::optional<HoldTileMissionRule> primaryHoldTileAlternative;

    // docs/regions/ashiron_quarry.md「2. 砕石段丘」: primary objective is
    // EliminateTeam OR reaching+ending-turn-on a marker tile (a single touch,
    // not a multi-round hold) - same "widen the default primary group to Any"
    // pattern as primaryHoldTileAlternative above, just with ObjectiveKind::
    // SecureTile instead of HoldTile. Reuses HoldTileMissionRule's shape
    // (requiredHoldRounds is simply unused for this field) rather than adding
    // a near-identical struct.
    std::optional<HoldTileMissionRule> primarySecureTileAlternative;

    // docs/regions/cinderwatch_gate.md「地域ボス 元守備隊長」's "主目的: 元守備隊長を
    // 戦闘不能にして撤退させる": if set (to a `enemyRoster` unit's id),
    // BattleFactory replaces the default EliminateTeam primary member with a
    // single DefeatUnit targeting this unit - same "replace, don't widen"
    // reasoning as OperateObject above (only one primary group is allowed,
    // and killing every OTHER enemy while leaving the boss standing must
    // not win). Only covers "戦闘不能" (HP0) - this engine has no enemy
    // voluntary-retreat/surrender AI, so "撤退させる" isn't separately
    // modeled (same gap as the "元守備兵を2人以上撤退・降伏" secondary, left
    // unimplemented).
    std::optional<std::string> primaryDefeatUnitId;

    // docs/regions/blackwater_lowlands.md「5. 黒水渡し」's "主目的: 荷運び役2人の
    // うち1人以上を右端へ脱出": replaces the default EliminateTeam primary
    // member with a single EscapeUnits objective targeting a right-edge
    // tile, same "replace, not widen" reasoning as primaryDefeatUnitId
    // (defeating every enemy while both guests stay stranded must not win a
    // stage whose actual objective is a guest escaping). Any Team::Player
    // unit ending an action on the tile credits it (the existing generic
    // SecureTile/EscapeUnits ActionResolved mechanism, no guest-specific
    // wiring needed) - a guest crediting it is simply the expected case.
    struct PrimaryEscapeUnitsRule {
        std::string id;
        int requiredEscapeCount = 1;
        // Tile is chosen at scenario-build time (chooseHoldTile(), same as
        // primaryHoldTileAlternative/primarySecureTileAlternative) from this
        // column range, so it never lands on an impassable/occupied tile.
        int zoneMinCol = kGridCols - 1;
        int zoneMaxCol = kGridCols - 1;
    };
    std::optional<PrimaryEscapeUnitsRule> primaryEscapeUnitsAlternative;

    // docs/regions/blackwater_lowlands.md「3. 薬草洲」's "主目的: 3ラウンド防衛、
    // または敵全滅": same "widen the default primary group to Any" pattern as
    // primaryHoldTileAlternative/primarySecureTileAlternative above, just with
    // ObjectiveKind::SurviveRounds instead. Doesn't reuse HoldTileMissionRule's
    // shape since SurviveRounds has no tile/zone concept at all, just a round
    // threshold.
    struct SurviveRoundsMissionRule {
        std::string id;
        int surviveUntilRound = 3;
    };
    std::optional<SurviveRoundsMissionRule> primarySurviveRoundsAlternative;

    // docs/regions/old_frontier_settlement.md「2. 共同井戸」's "副目標: 中立住民を
    // 全員避難", alongside `primarySurviveRoundsAlternative` as this stage's
    // primary. Unlike primaryEscapeUnitsAlternative (which REPLACES the
    // default "primary" group's EliminateTeam member - see that field's own
    // comment), this adds a genuinely independent, non-primary
    // ObjectiveGroupDefinition/ObjectiveDefinition pair
    // (primary=false, its own groupId) that coexists with whichever primary
    // rule the stage also sets. Confirmed viable by reading Objective.hpp/
    // BattleFactory.cpp: ObjectiveKind::EscapeUnits itself carries no
    // primary-vs-secondary assumption (only its `target`/`kind`), and
    // groups.push_back() for a brand-new group id is already the established
    // pattern for `surveyObjectiveId`'s secondary SecureTile group above -
    // this is the same shape with EscapeUnits instead of SecureTile. Reads
    // back the same way `surveyObjectiveId` does (GameApp::proceedToCamp()
    // scans `mission.definitions` for this group id and checks its progress
    // is Completed), so this is a REAL secondary objective, not an
    // approximation.
    struct SecondaryEscapeUnitsRule {
        std::string id;
        int requiredEscapeCount = 1;
        int zoneMinCol = 0;
        int zoneMaxCol = kGridCols - 1;
    };
    std::optional<SecondaryEscapeUnitsRule> secondaryEscapeUnitsAlternative;

    // docs/regions/ember_ravine.md「3. 硫黄窪地」's "副目標: 採取者を撤退させ
    // ない" - the FIRST real use of ObjectiveKind::ProtectUnit (Objective.hpp's
    // own comment: it has a dedicated non-primary pass in
    // syncObjectiveProgress() already, but nothing ever generated a
    // ProtectUnit ObjectiveDefinition before this). Same "push a new
    // secondary group, one Objective under it" shape as
    // secondaryEscapeUnitsAlternative above, just targeting an existing
    // guestUnits id instead of a tile. Unlike SurviveRounds/EscapeUnits,
    // ProtectUnit's own satisfied-check is a falling edge (Active until the
    // guest is lost, then Failed - never Completed), so this does not by
    // itself drive Victory/Defeat; the stage's actual "採取者撤退→敗北" is
    // still `allGuestsLost()` via the same unitId being present in
    // `guestUnits` (ObjectiveTracker::evaluateBattleOutcome() only reads
    // allGuestsLost()/primary groups, never this Kind - confirmed by
    // reading it). This field only makes the secondary genuinely trackable
    // instead of an ad-hoc isPresent()-in-GameApp check.
    struct SecondaryProtectUnitRule {
        std::string id;
        std::string unitId;
    };
    std::optional<SecondaryProtectUnitRule> secondaryProtectUnitAlternative;

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

    // docs/battle_objects.md "ランダム生成": generalizes what used to be
    // Brokenwood Territory's fallen_log-only ad-hoc block in BattleFactory.cpp
    // into region data, so a future region declares its random Battle
    // Objects here instead of adding another `if (stage.terrainProfileId ==
    // ...)` branch to the factory. `definition` is registered as-is (a
    // region owns its own Object's stats, same as `enemyRoster` owns its own
    // Unit stats); `idPrefix` becomes `<idPrefix>_1`, `_2`, ... for however
    // many actually get placed.
    struct ObjectPlacementRule {
        BattleObjectDefinition definition;
        std::string idPrefix;
        int count = 1;
        // docs/regions/ashbough_forest.md "折れ木の縄張り" route B: +1 on top
        // of `count` when the chosen route's ExplorationOutcome carries a
        // positive extraBarrierCount. False for every rule that doesn't vary
        // by route.
        bool scalesWithExtraBarrierOutcome = false;
        int zoneMinCol = 0;
        int zoneMaxCol = kGridCols - 1;
        // "予告済み突進には適用しない" doesn't apply here (this is initial
        // placement, not projectile line-of-sight) - this instead mirrors
        // the fallen log's own placement rule: never share the first living
        // Enemy Unit's row, so a Boss (typically that first Enemy) can't
        // find its charge lane blocked before Round 1.
        bool avoidFirstEnemyRow = false;
        // docs/regions/cinderwatch_gate.md「5. 信号塔下層」's 2 control
        // panels: if set, BattleFactory generates one OperateObject
        // Objective per instance this rule actually placed (primary=true,
        // groupId="primary", target.objectId = the placed instance's id),
        // and - the first time any rule in the stage sets this - removes
        // the default EliminateTeam member from the "primary" group rather
        // than widening it to Any like primaryHoldTileAlternative does
        // (validateBattleMission() requires exactly one non-empty primary
        // group, and the doc's "敵全滅後、残った制御盤を操作" reads as
        // operating still being required even after a wipe, not an
        // independent win condition - so EliminateTeam alone shouldn't win).
        std::optional<std::string> operateObjectiveId;
        // docs/regions/old_frontier_settlement.md「5. 夜明けの共同防衛」's 主目的
        // sub-condition 3「警鐘を1回以上操作する」: unlike `operateObjectiveId`
        // above (which REPLACES the default "primary" group's EliminateTeam
        // member - the doc's primary is an AND of 3 sub-conditions this
        // engine has no AND/OR-Kind-mixing composition for, see
        // primarySurviveRoundsAlternative's use on this stage for how that's
        // approximated instead), this adds a genuinely independent,
        // non-primary ObjectiveGroupDefinition/ObjectiveDefinition pair (its
        // own groupId, primary=false) - same "push a new group, one
        // Objective per placed instance" shape as `operateObjectiveId`, just
        // without touching the default "primary" group at all. A real
        // secondary Objective that tracks bell-operated progress even though
        // it can't gate the primary win condition.
        std::optional<std::string> secondaryOperateObjectiveId;
    };
    std::vector<ObjectPlacementRule> objectPlacementRules;

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

// docs/deep_layers.md「全体構成」: 深層/最深層はRegionId/RouteGraphを持たない
// 単純な連続戦闘リスト(ルート構造なし)。RegionDescriptorへ包まず生の
// std::vector<StageDescriptor>を返すのはこのため - 呼び出し側は
// createScenarioBattle()/createScenarioContinuationBattle()へ直接渡し、
// 各戦闘後にDeepLayerScaling.hppの倍率をBattleStateへ後掛けする
// (jf_forest_balanceの--deep=<region>:<deep|deepest>モードが実装例)。
// 現状は「1地域だけ先に縦通しを作る」方針により灰枝の森(AshboughForest)のみ。
// 深層: ザコ戦2回+ボス2体(計4戦)、最深層: ザコ戦1回+ボス1体(計2戦)。
std::vector<StageDescriptor> ashboughForestDeepStages();
std::vector<StageDescriptor> ashboughForestDeepestStages();

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

// docs/prompts/exploration_system_improvement_prompt.md(2026-08-02、
// Phase 2): the class option C (ScoutRoute) is gated on. Returns
// `stage.scoutRouteRequiredClass` verbatim if the stage sets one (hand-
// authored stages keep their own choice); otherwise derives a sensible
// default from the same stage.id-based ExplorationTemplate heuristic
// stageRouteOutcome() uses, instead of hardcoding FrontierScout for every
// stage regardless of theme.
UnitClass scoutRouteClassForStage(const StageDescriptor& stage);

// docs/prompts/exploration_randomization_prompt.md(2026-08-02設計): public
// forwarding wrapper over Region.cpp's file-local explorationTemplateForStageId()
// (same reason scoutRouteClassForStage() exists as a wrapper) - GameApp.cpp
// needs the template itself, not just the class it maps to, to roll that
// template's ExplorationChoiceOption pool.
ExplorationTemplate explorationTemplateForStage(const StageDescriptor& stage);

// Additive reward computation (docs/regions/ashbough_forest.md): base loot,
// plus this route's delta (if any), plus the survey bonus if that stage
// defines a survey objective and it succeeded. Never returns a negative or
// zero-quantity stack.
std::vector<LootStack> computeStageVictoryLoot(const StageDescriptor& stage, ExplorationChoice choice,
                                               bool surveyObjectiveSucceeded);

// Same idea as computeStageVictoryLoot but for discoveries: the
// unconditional `stage.discoveries` plus whichever `routeDiscoveries` entry
// matches `choice` (docs/regions/cinderwatch_gate.md "3. アイアンウォッチ
// 物資庫"). No dedup beyond what a std::vector naturally gives - discovery
// ids are inserted into a set downstream (BaseState::discoveryRegistry), so
// a duplicate here is harmless.
std::vector<DiscoveryId> computeStageDiscoveries(const StageDescriptor& stage, ExplorationChoice choice);

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
// starts unlocked; every one of the other 9 regions requires the previous
// region's id in BaseState::completedRegionIds, chained in a straight line
// (AshboughForest -> CinderwatchGate -> AshironQuarry -> BlackwaterLowlands
// -> WindscarPlateau -> OldFrontierSettlement -> EmberRavine ->
// BuriedDawnSanctum -> ShatteredMarchFort -> MappedEdge - see Region.cpp's
// switch for the exact edges). A single cleared location within a region
// must never stand in for the whole region's completion - see
// regionCleared()'s own comment for why that migration-only heuristic isn't
// the live gate.
bool regionUnlocked(RegionId regionId, const BaseState& base, const GameData& data);

} // namespace jf
