#pragma once

#include <array>
#include <algorithm>
#include <memory>
#include <cstdint>
#include <optional>
#include <string>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "jf/battle/BattleController.hpp"
#include "jf/battle/BattleFactory.hpp"
#include "jf/core/ExpeditionService.hpp"
#include "jf/core/ExpeditionState.hpp"
#include "jf/core/BaseState.hpp"
#include "jf/core/Exploration.hpp"
#include "jf/core/Facilities.hpp"
#include "jf/core/Region.hpp"
#include "jf/core/SaveSystem.hpp"
#include "jf/core/Skill.hpp"
#include "jf/core/Terrain.hpp"
#include "jf/data/GameData.hpp"

namespace jf {

enum class Screen {
    Base,
    Exploration,
    PreBattleDeployment,
    Battle,
    Camp
};

// Top-level, raylib-free screen flow: Battle -> (Victory|Defeat) -> Camp ->
// Continue/Return, tying BattleController and ExpeditionState together the
// way the design doc's "gameplay loop" describes.
class GameApp {
public:
    explicit GameApp(GameData data);

    Screen screen() const { return screen_; }
    BattleController& battle() { return *battleController_; }
    const BattleController& battle() const { return *battleController_; }
    const ExpeditionState& expedition() const { return expedition_; }
    ExpeditionState& expedition() { return expedition_; }
    const std::vector<UnitTemplate>& roster() const { return roster_; }
    const std::vector<std::string>& selectedPartyIds() const { return selectedPartyIds_; }
    const std::vector<ItemType>& preparedBag() const { return preparedBag_; }
    const BaseState& baseState() const { return baseState_; }
    // Read-only: lets UI code look up class base stats/weapons for tooltips
    // without GameApp needing to know anything about how they're displayed.
    const GameData& gameData() const { return data_; }

    void update(float dt);

    // Victory screen -> Camp, banking this battle's placeholder loot as "pending".
    void proceedToCamp();

    // Defeat screen -> fresh expedition. All pending loot is lost.
    void acknowledgeDefeat();

    // Camp -> new battle, keeping surviving party HP.
    void continueExpedition();
    bool useBattleHealingItem(ItemType item);
    bool chooseNeutralBattleHealingItem(ItemType item);
    bool selectNeutralBattleHealingTarget(GridPos pos);
    bool chooseProtectiveBoard();
    bool selectBoardTarget(GridPos pos);
    bool useCampItem(ItemType item, const std::string& unitId = {});

    bool togglePartyMember(const std::string& unitId);
    // docs/item_system.md: crafts one unit of `type` from base storage
    // materials (see itemCraftCost()) into baseState_.itemStorage, capped at
    // BaseState::kItemStorageCap. All-or-nothing: fails without consuming
    // anything if any single material is short.
    bool craftItem(ItemType type);
    // Moves one owned (baseState_.itemStorage) unit of `item` into the
    // prepared bag - fails if none are owned or the bag is already full.
    bool addPreparedItem(ItemType item);
    // Returns the removed item to baseState_.itemStorage (it was never
    // consumed, just packed).
    void removePreparedItem(std::size_t index);
    // `regionId` defaults to Ashbough Forest, the only region guaranteed
    // unlocked on a new game (docs/region_unlocks.md: "新規ゲームで解放される
    // 遠征地は灰枝の森だけ") - defaulting to CinderwatchGate would silently
    // fail for every caller that doesn't pass an explicit region, since it's
    // locked until Ashbough Forest's real completion condition exists
    // (Phase 4). Fails (returns false) if `regionId` isn't unlocked; the
    // real region-select screen only ever offers unlocked regions, but this
    // is enforced here too so the rule can't be bypassed from any caller.
    bool startExpedition(RegionId regionId = RegionId::AshboughForest);

    // docs/implementation_roadmap.md "Phase 3.5": Base screen's region list.
    // Ashbough Forest is always unlocked; every subsequent region requires
    // clearing the one before it (regionCleared() in jf/core/Region.hpp).
    bool isRegionUnlocked(RegionId regionId) const { return regionUnlocked(regionId, baseState_, data_); }
    // jf::RegionSummary (jf/core/ExpeditionService.hpp) - kept as an alias
    // here rather than a redeclared nested struct, since GameApp.hpp already
    // depends on ExpeditionService.hpp and callers spell this as
    // GameApp::RegionSummary.
    using RegionSummary = jf::RegionSummary;
    std::vector<RegionSummary> regionSummaries() const;

    // Base screen: advances outpostStage by one step once BaseState's
    // storage satisfies eligibleForOutpostStage() for the next stage.
    bool advanceOutpostStage();

    // Facility nodes (docs/base_development.md). Only usable from the Base
    // screen; validates via facilityNodeEligible() and consumes materials
    // from baseState_.storage. Once unlocked (and, for the 4 optional
    // facilities, built), a node is never dismantled or re-buildable -
    // docs/base_development.md: "解体、素材返却、再建費は採用しない".
    bool unlockFacilityNode(const std::string& nodeId);

    // Forge: overrides which weapon a class's units are equipped with
    // (docs/base_development.md "武器の装備変更"). `weaponId` must be a
    // known base weapon or one unlocked via a "craft_*" facility node;
    // pass an empty id to revert to the class's default weapon.
    bool equipWeaponForUnit(const std::string& unitId, const std::string& weaponId);
    const std::unordered_map<std::string, std::string>& weaponOverrides() const { return weaponOverrides_; }

    // Forge: equips/unequips the Hide-Wrapped Grip tuning trait for a class
    // (requires "trait_hide_wrapped_grip" to be unlocked and simple_forge to
    // be currently built). TuningTraitId::None unequips.
    bool equipTuningTraitForUnit(const std::string& unitId, TuningTraitId traitId);
    const std::unordered_map<std::string, TuningTraitId>& equippedTraits() const { return equippedTraits_; }

    // Training Ground: equips a skill into one of a unit's 2 skill slots
    // (docs/skill_system.md). Requires the skill's class-appropriate
    // training branch (requiredTrainingNodeIdFor()) to be unlocked. An empty
    // `skillId` unequips that slot. See jf/core/Skill.hpp for the registry.
    bool equipSkillForUnit(const std::string& unitId, int slotIndex, const std::string& skillId);
    const std::unordered_map<std::string, UnitSkillLoadout>& equippedSkills() const { return equippedSkills_; }

    // True once the party locked in for this expedition includes a
    // Frontier Scout - gates Exploration option C (the free-deployment
    // scout route) both for the button and for chooseExplorationRoute().
    bool partyHasFrontierScout() const;
    // Generic form: some stages gate option C on a different class (docs/
    // regions/ashbough_forest.md "薬草の沢"'s Dawn Chirurgeon route) via
    // StageDescriptor::scoutRouteRequiredClass.
    bool partyHasClass(UnitClass unitClass) const;

    // Exploration -> either straight to Battle (Frontal/Side routes) or to
    // PreBattleDeployment (Scout route, which requires a Frontier Scout).
    // Works for whichever region/stage is active (docs/implementation_
    // roadmap.md "Phase 1.5") rather than being Cinderwatch-specific. Only
    // available while the current site isn't Secured yet - once it is,
    // chooseSafePassage()/chooseReconnaissance() replace it.
    bool chooseExplorationRoute(ExplorationChoice choice);

    // docs/exploration_system.md "確保済み地点の通過": current site's
    // persistent access tier (Unknown by default for a never-visited site).
    SiteAccessState currentSiteAccess() const;

    // "安全路を進む": only available once currentSiteAccess() == Secured.
    // Skips the battle entirely - no exploration choice, no enemies fought,
    // no loot, no discoveries, and no healing. Goes straight to Camp with
    // the current expedition party snapshot unchanged.
    bool chooseSafePassage();

    // "危険区域を再調査": only available once currentSiteAccess() == Secured.
    // Fights a freshly-generated battle for ordinary-material rewards only -
    // no survey bonus, no discoveries, no further site-access promotion
    // (the site is already at its maximum tier).
    bool chooseReconnaissance();

    // docs/implementation_roadmap.md M2-D "周回短縮": repeatedly safe-passes
    // (docs/exploration_system.md's "安全路を進む" rules - no battle, no
    // reward, no HP/Bag/Pending change) through every CONSECUTIVE Secured
    // site starting at the current node, stopping at the first non-Secured
    // site (screen() stays Exploration there for a real choice) or the
    // region's Exit (screen() becomes Camp, same as reaching the end via a
    // single chooseSafePassage()). A no-op (returns 0) if the current site
    // isn't Secured - use chooseExplorationRoute()/chooseSafePassage() for
    // that site instead. Returns how many sites were passed.
    int bulkPassSecuredSites();

    // Command Post "Scout Network" node effect: once unlocked, the
    // Exploration screen can show the enemy composition ahead of time,
    // regardless of which route the player ends up choosing.
    bool scoutNetworkUnlocked() const { return baseState_.unlockedNodeIds.count("scout_network") > 0; }
    std::vector<Unit> explorationEnemyPreview() const {
        return previewEnemies(activeExpeditionData_, currentStage(), expeditionSeed_, {});
    }

    // PreBattleDeployment: place party member `partyIndex` (matching
    // deploymentPlayers() order) at `pos`. Rejects out-of-zone columns,
    // and tiles already taken by another placed ally. Terrain does not block
    // deployment; an impassable chosen tile is opened at battle start.
    bool placeDeploymentUnit(std::size_t partyIndex, GridPos pos);
    bool allDeploymentUnitsPlaced() const;
    // Only succeeds once all 4 are placed; hands the chosen positions to
    // BattleFactory and transitions to Battle.
    bool confirmDeployment();
    // Back out to the Exploration choice screen, discarding placement.
    void cancelDeployment();

    const std::vector<Unit>& deploymentPlayers() const { return deploymentPlayers_; }
    bool isDeploymentUnitPlaced(std::size_t partyIndex) const;
    const std::vector<Unit>& deploymentEnemyPreview() const { return deploymentEnemyPreview_; }
    const std::array<TerrainType, kGridRows * kGridCols>& deploymentTerrain() const { return deploymentTerrain_; }
    int deploymentMaxColumn() const { return deploymentOutcome_.deploymentMaxColumn; }

    // Abandons the current expedition from any non-Base screen (Exploration,
    // PreBattleDeployment, Battle, or Camp) and returns to Base, forfeiting
    // all of this run's pending (unsecured) loot/discoveries - the same
    // forfeiture rule as a Defeat. No-op (returns false) if already at Base.
    bool retireExpedition();

    // Camp -> shows "Loot Secured" (call acknowledgeLootSecured() to continue).
    // docs/inventory_overflow.md「帰還処理」: returns false (and changes
    // nothing) if committing would push RewardOverflowState past its 200-Stack
    // ceiling - the caller should route the player to the warehouse cleanup
    // screen instead of retrying blindly.
    bool returnToBase();

    // docs/inventory_overflow.md「保留品の放棄」/「倉庫整理画面」: discards
    // owned storage/consumables and pending overflow Stacks. Both return false
    // (no state change) if the requested quantity isn't available, or if the
    // id is a region key material (never discardable, matches
    // eligibleForOutpostStage()'s reliance on it).
    bool discardStorage(const LootId& id, int quantity);
    bool discardItemStorage(ItemType type, int quantity);
    bool discardOverflowStack(std::size_t index, int quantity);
    const RewardOverflowState& rewardOverflow() const { return baseState_.rewardOverflow; }

    // Loot Secured screen -> brand new expedition.
    void acknowledgeLootSecured();

    bool justSecuredLoot() const { return justSecuredLoot_; }
    const std::vector<std::string>& lastSecuredLoot() const { return lastSecuredLoot_; }
    // Tracked by battlesWon (incremented once per real proceedToCamp()
    // victory), not stageIndex: stageIndex only advances via
    // continueExpedition(), which is never called after the final stage, so
    // it never actually reaches stages.size() - battlesWon does.
    bool expeditionComplete() const;
    std::string currentMissionName() const;
    std::string currentMissionNameJa() const;
    bool currentSiteContentImplemented() const { return currentStage().contentImplemented; }
    std::optional<std::string> nextMissionNameJa() const;
    // Camp screen decision-support (docs/campaign_balance.md "初見で途中帰還
    // することを失敗扱いにせず、情報と安全路を持ち帰る正規の進行にする"): the
    // next site's enemy roster names, gated the same way as
    // explorationEnemyPreview() - only meaningful to show once
    // scoutNetworkUnlocked(), so this doesn't hand out free intel beyond
    // what that facility node already grants. std::nullopt at the region's
    // last site (nothing ahead to preview) or if there's no next site yet.
    std::optional<std::vector<std::string>> nextSiteEnemyRosterNames() const;
    std::uint32_t expeditionSeed() const { return expeditionSeed_; }
    SaveData createSaveData(const std::string& language) const;
    bool applySaveData(const SaveData& save);
    std::uint64_t persistentRevision() const { return persistentRevision_; }
    // Bumps whenever the mid-expedition checkpoint (docs/save_system.md
    // "遠征中断セーブ") changes, independent of persistentRevision() - lets the
    // autosave loop notice expedition progress even between base-state saves.
    std::uint64_t expeditionRevision() const { return expeditionRevision_; }

private:
    void resetToBase();
    // Snapshots the current expedition into expeditionCheckpoint_. Only
    // called at the two resumable points (Exploration entry, Camp
    // entry/updates) - never mid-battle or mid-deployment.
    void updateExpeditionCheckpoint(ExpeditionCheckpoint::Stage stage);
    void syncPartySnapshotFromBattle();
    // Thin wrapper: mutates expedition_ via ExpeditionService's
    // advanceExpeditionRouteToNextSite() (docs/implementation_roadmap.md M6
    // 「コンポーネント分離メモ」- see jf/core/ExpeditionService.hpp).
    bool advanceRouteToNextSite();

    // docs/implementation_roadmap.md "Phase 1.5": the single place that
    // resolves which region/stage is currently active. Never cached as a
    // member - region data is small and static, so recomputing it is cheap
    // and avoids a second source of truth to keep in sync with expedition_.
    RegionDescriptor currentRegion() const { return regionDescriptor(expedition_.regionId, data_); }
    // By value, not reference: currentRegion() returns a fresh temporary
    // each call, so a reference into it would dangle immediately. Thin
    // wrapper over ExpeditionService's computeCurrentStage().
    StageDescriptor currentStage() const { return computeCurrentStage(expedition_, data_); }

    GameData data_;
    std::uint32_t expeditionSeed_ = 0;
    std::unique_ptr<BattleController> battleController_;
    ExpeditionState expedition_;
    Screen screen_ = Screen::Base;
    std::vector<UnitTemplate> roster_;
    std::vector<std::string> selectedPartyIds_;
    std::vector<ItemType> preparedBag_;
    BaseState baseState_;
    GameData activeExpeditionData_;
    std::vector<Unit> expeditionPartyUnits_;
    // Per-stage guard so each stage's mission discoveries are only queued
    // once per expedition run (indices match ExpeditionState::stageIndex;
    // sized to the current region's stage count, not fixed at 3 anymore).
    std::vector<bool> stageDiscoveryAwarded_;
    // Which route the player picked at this stage's Exploration screen -
    // needed at proceedToCamp() time to compute the route-adjusted reward
    // (docs/regions/ashbough_forest.md's per-route loot table).
    ExplorationChoice lastExplorationChoice_ = ExplorationChoice::FrontalAdvance;
    // Set by chooseReconnaissance(): this stage's win only grants ordinary-
    // material rewards (no survey bonus, no discoveries, no further
    // site-access promotion), since the site is already Secured.
    bool isReconnaissanceRun_ = false;
    bool justSecuredLoot_ = false;
    std::vector<std::string> lastSecuredLoot_;
    // docs/inventory_overflow.md「受取保留」: source for OverflowStack::grantId,
    // one new value per returnToBase() call that actually produces overflow.
    std::uint64_t returnGrantSequence_ = 0;

    // PreBattleDeployment state. deploymentPlayers_[i]'s position is only
    // meaningful once deploymentPlaced_[i] is true.
    std::vector<Unit> deploymentPlayers_;
    std::vector<bool> deploymentPlaced_;
    std::vector<Unit> deploymentEnemyPreview_;
    std::array<TerrainType, kGridRows * kGridCols> deploymentTerrain_{};
    ExplorationOutcome deploymentOutcome_;

    // Forge equipment state (persists at the base, applied to every battle).
    std::unordered_map<std::string, std::string> weaponOverrides_;
    std::unordered_map<std::string, TuningTraitId> equippedTraits_;
    // Training Ground equip state (persists at the base, applied to every
    // battle - see applyEquippedSkills()).
    std::unordered_map<std::string, UnitSkillLoadout> equippedSkills_;
    std::uint64_t persistentRevision_ = 0;
    std::optional<ItemType> pendingBattleItem_;
    std::optional<ExpeditionCheckpoint> expeditionCheckpoint_;
    std::uint64_t expeditionRevision_ = 0;

    void markPersistentStateChanged() { ++persistentRevision_; }

    // Stamps each freshly-built battle's player units with their equipped
    // trait's effects (currently just knockback negation).
    void applyEquipmentTraits(BattleController& controller);
    // Stamps each freshly-built battle's player units with their equipped
    // skill ids and resets their charges/cooldowns for the new battle.
    void applyEquippedSkills(BattleController& controller);
};

} // namespace jf
