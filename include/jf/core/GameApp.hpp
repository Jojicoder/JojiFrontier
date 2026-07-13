#pragma once

#include <array>
#include <memory>
#include <cstdint>
#include <string>
#include <vector>

#include "jf/battle/BattleController.hpp"
#include "jf/core/ExpeditionState.hpp"
#include "jf/core/BaseState.hpp"
#include "jf/core/Exploration.hpp"
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

    void update(float dt);

    // Victory screen -> Camp, banking this battle's placeholder loot as "pending".
    void proceedToCamp();

    // Defeat screen -> fresh expedition. All pending loot is lost.
    void acknowledgeDefeat();

    // Camp -> new battle, keeping surviving party HP.
    void continueExpedition();
    bool useBattleHealingItem(ItemType item);
    bool chooseProtectiveBoard();
    bool selectBoardTarget(GridPos pos);
    bool useCampItem(ItemType item, const std::string& unitId = {});

    bool togglePartyMember(const std::string& unitId);
    bool addPreparedItem(ItemType item);
    void removePreparedItem(std::size_t index);
    bool startExpedition();

    // Base screen: advances outpostStage by one step once BaseState's
    // storage satisfies eligibleForOutpostStage() for the next stage.
    bool advanceOutpostStage();

    // True once the party locked in for this expedition includes a
    // Frontier Scout - gates Exploration option C (the free-deployment
    // scout route) both for the button and for chooseCinderwatchRoute().
    bool partyHasFrontierScout() const;

    // Exploration -> either straight to Battle (Frontal/Side routes) or to
    // PreBattleDeployment (Scout route, which requires a Frontier Scout).
    bool chooseCinderwatchRoute(ExplorationChoice choice);

    // PreBattleDeployment: place party member `partyIndex` (matching
    // deploymentPlayers() order) at `pos`. Rejects out-of-zone columns,
    // impassable terrain, and tiles already taken by another placed ally.
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

    // Camp -> shows "Loot Secured" (call acknowledgeLootSecured() to continue).
    void returnToBase();

    // Loot Secured screen -> brand new expedition.
    void acknowledgeLootSecured();

    bool justSecuredLoot() const { return justSecuredLoot_; }
    const std::vector<std::string>& lastSecuredLoot() const { return lastSecuredLoot_; }
    bool expeditionComplete() const { return expedition_.stage >= 2; }
    std::string currentMissionName() const;
    std::uint32_t expeditionSeed() const { return expeditionSeed_; }

private:
    void resetToBase();

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
    // Per-stage guard so each of the 3 mission discoveries is only queued
    // once per expedition run (indices match ExpeditionState::stage 0-2).
    std::array<bool, 3> stageDiscoveryAwarded_{};
    bool justSecuredLoot_ = false;
    std::vector<std::string> lastSecuredLoot_;

    // PreBattleDeployment state. deploymentPlayers_[i]'s position is only
    // meaningful once deploymentPlaced_[i] is true.
    std::vector<Unit> deploymentPlayers_;
    std::vector<bool> deploymentPlaced_;
    std::vector<Unit> deploymentEnemyPreview_;
    std::array<TerrainType, kGridRows * kGridCols> deploymentTerrain_{};
    ExplorationOutcome deploymentOutcome_;
};

} // namespace jf
