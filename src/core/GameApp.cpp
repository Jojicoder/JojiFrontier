#include "jf/core/GameApp.hpp"

#include "jf/battle/BattleFactory.hpp"
#include "jf/battle/SkillCharges.hpp"
#include "jf/core/Skill.hpp"

#include <algorithm>
#include <random>

namespace jf {

namespace {
std::uint32_t makeExpeditionSeed() {
    std::random_device device;
    return (static_cast<std::uint32_t>(device()) << 1u) ^ static_cast<std::uint32_t>(device());
}

// The Base screen always keeps a BattleController alive even though nothing
// is actually being played there yet; which region/stage backs that idle
// placeholder is arbitrary. Kept as its own function so the constructor and
// resetToBase() can't drift apart.
StageDescriptor idlePlaceholderStage(const GameData& data) {
    return regionDescriptor(RegionId::CinderwatchGate, data).stages.at(0);
}
} // namespace

GameApp::GameApp(GameData data) : data_(std::move(data)) {
    roster_ = data_.playerParty;
    roster_.insert(roster_.end(), data_.reserveRoster.begin(), data_.reserveRoster.end());
    for (const auto& unit : data_.playerParty) selectedPartyIds_.push_back(unit.id);
    expeditionSeed_ = makeExpeditionSeed();
    battleController_ = std::make_unique<BattleController>(
        createScenarioBattle(data_, idlePlaceholderStage(data_), expeditionSeed_));
}

void GameApp::update(float dt) {
    if (screen_ == Screen::Battle) {
        battleController_->update(dt);
    }
}

void GameApp::proceedToCamp() {
    // docs/region_mission_data_contract.md "二重付与防止": without this
    // guard, calling proceedToCamp() again after screen_ already transitioned
    // to Camp (inputState() stays Victory forever - nothing resets it) would
    // re-run the whole reward grant, doubling pendingLoot and battlesWon
    // every time. Every other screen-transition method already guards on
    // the screen it's leaving; this one was missing it.
    if (screen_ != Screen::Battle || battleController_->inputState() != BattleInputState::Victory) return;
    const StageDescriptor stage = currentStage();

    bool surveySucceeded = false;
    if (stage.surveyObjectiveId && !isReconnaissanceRun_) {
        // BattleFactory groups every survey-tile objective (one or several,
        // e.g. Herbwater Hollow's 2 HerbPatch tiles) under a group id equal
        // to surveyObjectiveId, so this succeeds if any of them completed -
        // whether there's 1 tile or several.
        const BattleMissionState& mission = battleController_->battle().missionState();
        for (const ObjectiveDefinition& def : mission.definitions) {
            if (def.groupId == *stage.surveyObjectiveId &&
                mission.progress.at(def.id).status == ObjectiveStatus::Completed) {
                surveySucceeded = true;
                break;
            }
        }
    }
    std::vector<LootStack> loot = computeStageVictoryLoot(stage, lastExplorationChoice_, surveySucceeded);
    // docs/regions/ashbough_forest.md "折れ木の縄張り"'s ad-hoc secondary
    // bonuses - merged by id (not just appended) so a bonus sharing an id
    // with the base reward (e.g. 獣皮) becomes one combined stack rather
    // than two separate same-id entries.
    auto mergeLoot = [&](const std::vector<LootStack>& extra) {
        for (const LootStack& stack : extra) {
            auto it = std::find_if(loot.begin(), loot.end(),
                                   [&](const LootStack& entry) { return entry.id == stack.id; });
            if (it == loot.end()) loot.push_back(stack);
            else it->quantity += stack.quantity;
        }
    };
    if (!isReconnaissanceRun_ && battleController_->battle().bossHasCollidedWithBarrier())
        mergeLoot(stage.logCollisionBonusLoot);
    if (!isReconnaissanceRun_ &&
        std::none_of(battleController_->battle().units().begin(), battleController_->battle().units().end(),
                    [](const Unit& unit) { return unit.team == Team::Player && !unit.isAlive(); }))
        mergeLoot(stage.noCasualtiesBonusLoot);
    expedition_.pendingLoot.insert(expedition_.pendingLoot.end(), loot.begin(), loot.end());

    if (!isReconnaissanceRun_) {
        std::size_t stageIndex = static_cast<std::size_t>(expedition_.stageIndex);
        if (stageIndex < stageDiscoveryAwarded_.size() && !stageDiscoveryAwarded_[stageIndex]) {
            for (const DiscoveryId& discovery : computeStageDiscoveries(stage, lastExplorationChoice_))
                expedition_.pendingDiscoveries.push_back(discovery);
            stageDiscoveryAwarded_[stageIndex] = true;
        }
        SiteAccessState achieved = surveySucceeded ? SiteAccessState::Secured : SiteAccessState::Surveyed;
        queueExpeditionSiteAccessPromotion(expedition_, siteAccessKey(expedition_.regionId, stage.id), achieved,
                                           baseState_);

        // docs/regions/ashbough_forest.md "地域進行": once every site is
        // Surveyed+ (灰枝の林縁勝利、薬草の沢勝利、灰角大猪撃破), the win that
        // completes the last one queues the region's completion Discovery -
        // committed alongside completedRegionIds on the safe return that
        // follows (docs/region_mission_data_contract.md "地域完了判定").
        if (!baseState_.completedRegionIds.count(expedition_.regionId) &&
            !expedition_.pendingRegionCompletions.count(expedition_.regionId) &&
            computeWouldRegionBeCleared(expedition_.regionId, expedition_, baseState_, data_)) {
            expedition_.pendingRegionCompletions.insert(expedition_.regionId);
            if (expedition_.regionId == RegionId::AshboughForest)
                expedition_.pendingDiscoveries.push_back(kAshboughForestSurveyCompleteDiscovery);
        }
    }
    expedition_.battlesWon += 1;
    if (expedition_.routeProgress)
        expedition_.routeProgress->resolvedNodeIds.insert(expedition_.routeProgress->currentNodeId);
    syncPartySnapshotFromBattle();
    justSecuredLoot_ = false;
    screen_ = Screen::Camp;
    updateExpeditionCheckpoint(ExpeditionCheckpoint::Stage::Camp);
}

void GameApp::acknowledgeDefeat() {
    if (battleController_->inputState() != BattleInputState::Defeat) return;
    lastSecuredLoot_.clear();
    resetToBase();
}

bool GameApp::retireExpedition() {
    if (screen_ == Screen::Base) return false;
    lastSecuredLoot_.clear();
    resetToBase();
    return true;
}

void GameApp::continueExpedition() {
    if (screen_ != Screen::Camp || expeditionComplete()) return;
    syncPartySnapshotFromBattle();
    if (expedition_.routeProgress) {
        if (!advanceRouteToNextSite()) return;
        screen_ = Screen::Exploration;
        updateExpeditionCheckpoint(ExpeditionCheckpoint::Stage::Exploration);
        return;
    }
    std::vector<Unit> survivors = battleController_->battle().units();
    ++expedition_.stageIndex;
    battleController_ = std::make_unique<BattleController>(
        createScenarioContinuationBattle(data_, survivors, currentStage(), expeditionSeed_));
    applyEquipmentTraits(*battleController_);
    applyEquippedSkills(*battleController_);
    screen_ = Screen::Battle;
}

bool GameApp::useBattleHealingItem(ItemType item) {
    if (screen_ != Screen::Battle || item == ItemType::FirstAidKit || expedition_.count(item) <= 0) return false;
    if (!battleController_->useHealingItem(healingAmount(item))) return false;
    return expedition_.consume(item);
}

bool GameApp::chooseNeutralBattleHealingItem(ItemType item) {
    if (screen_ != Screen::Battle || item == ItemType::FirstAidKit || expedition_.count(item) <= 0 ||
        healingAmount(item) <= 0) return false;
    if (!battleController_->chooseHealingItemTarget(healingAmount(item))) return false;
    pendingBattleItem_ = item;
    return true;
}

bool GameApp::selectNeutralBattleHealingTarget(GridPos pos) {
    if (!pendingBattleItem_ || !battleController_->selectHealingItemTarget(pos)) return false;
    const ItemType item = *pendingBattleItem_;
    pendingBattleItem_.reset();
    return expedition_.consume(item);
}

bool GameApp::chooseProtectiveBoard() {
    if (screen_ != Screen::Battle || expedition_.count(ItemType::ProtectiveBoard) <= 0) return false;
    battleController_->chooseProtectiveBoard();
    return battleController_->inputState() == BattleInputState::SelectBoardTarget;
}

bool GameApp::selectBoardTarget(GridPos pos) {
    if (!battleController_->selectBoardTarget(pos)) return false;
    return expedition_.consume(ItemType::ProtectiveBoard);
}

bool GameApp::useCampItem(ItemType item, const std::string& unitId) {
    if (screen_ != Screen::Camp || expedition_.count(item) <= 0) return false;
    auto& units = battleController_->battle().units();
    if (item == ItemType::CampRations) {
        bool changed = false;
        for (Unit& unit : units) {
            if (unit.team == Team::Player && unit.isAlive() && unit.currentHp < unit.stats.maxHp) {
                unit.currentHp = std::min(unit.currentHp + 5, unit.stats.maxHp);
                changed = true;
            }
        }
        if (!changed || !expedition_.consume(item)) return false;
        updateExpeditionCheckpoint(ExpeditionCheckpoint::Stage::Camp);
        return true;
    }
    if (item == ItemType::ReturnFlare) {
        return returnToBase();
    }
    Unit* target = battleController_->battle().findUnit(unitId);
    if (!target || target->team != Team::Player) return false;
    if (item == ItemType::RescuePack) {
        if (target->isAlive()) return false;
        target->currentHp = std::max(1, target->stats.maxHp / 4);
    } else {
        int amount = healingAmount(item);
        if (!target->isAlive() || target->currentHp >= target->stats.maxHp || amount <= 0) return false;
        target->currentHp = std::min(target->currentHp + amount, target->stats.maxHp);
    }
    if (!expedition_.consume(item)) return false;
    updateExpeditionCheckpoint(ExpeditionCheckpoint::Stage::Camp);
    return true;
}

std::string GameApp::currentMissionName() const {
    return currentStage().missionNameEn;
}

std::string GameApp::currentMissionNameJa() const {
    return currentStage().missionNameJa;
}

bool GameApp::expeditionComplete() const { return computeExpeditionComplete(expedition_, data_); }

std::optional<std::string> GameApp::nextMissionNameJa() const {
    return computeNextMissionNameJa(expedition_, data_);
}

std::optional<std::vector<std::string>> GameApp::nextSiteEnemyRosterNames() const {
    return computeNextSiteEnemyRosterNames(expedition_, data_, expeditionPartyUnits_);
}

void GameApp::syncPartySnapshotFromBattle() {
    if ((screen_ != Screen::Battle && screen_ != Screen::Camp) || !battleController_) return;
    std::vector<Unit> party;
    for (const Unit& unit : battleController_->battle().units())
        if (unit.team == Team::Player) party.push_back(unit);
    if (!party.empty()) expeditionPartyUnits_ = std::move(party);
}

bool GameApp::advanceRouteToNextSite() {
    return advanceExpeditionRouteToNextSite(expedition_, baseState_, data_);
}

bool GameApp::returnToBase() {
    if (screen_ != Screen::Camp) return false;

    // docs/inventory_overflow.md「帰還処理」: compute what fits before
    // mutating anything, so a 200-Stack ceiling breach (checked below) leaves
    // storage/overflow untouched rather than partially applied.
    std::unordered_map<LootId, int> materialAdds;
    for (const LootStack& loot : expedition_.pendingLoot) materialAdds[loot.id] += loot.quantity;

    std::unordered_map<LootId, int> fitPlan;
    std::vector<std::pair<LootId, int>> overflowPlan;
    for (const auto& [id, quantity] : materialAdds) {
        const bool isKeyMaterial = baseState_.materialStorageCap(id) == BaseState::kKeyMaterialStorageCap;
        const int cap = baseState_.materialStorageCap(id);
        const int current = baseState_.storageCount(id);
        const int room = std::max(0, cap - current);
        const int fits = std::min(room, quantity);
        if (fits > 0) fitPlan[id] = fits;
        // docs/inventory_overflow.md「保留中のキー素材...は存在させない。これらは
        // 重複除去して直接恒久化する」: a key material's excess beyond its
        // 1-cap is deduplicated away here, never queued as overflow.
        if (!isKeyMaterial) {
            const int overflow = quantity - fits;
            if (overflow > 0) overflowPlan.push_back({id, overflow});
        }
    }

    // Unused expedition items are already owned by the player, but they still
    // need the same capacity-safe commit as newly secured materials. This also
    // makes imported/older saves safe when their storage and bag totals exceed
    // the current per-item cap.
    std::unordered_map<ItemType, int> returnedItems;
    for (ItemType item : expedition_.bag) ++returnedItems[item];
    std::unordered_map<ItemType, int> itemFitPlan;
    for (const auto& [item, quantity] : returnedItems) {
        const int room = std::max(0, BaseState::kItemStorageCap - baseState_.ownedItemCount(item));
        const int fits = std::min(room, quantity);
        if (fits > 0) itemFitPlan[item] = fits;
        const int overflow = quantity - fits;
        if (overflow > 0)
            overflowPlan.push_back({"item:" + std::to_string(static_cast<int>(item)), overflow});
    }

    if (baseState_.rewardOverflow.stacks.size() + overflowPlan.size() > RewardOverflowState::kMaxStacks)
        return false;

    justSecuredLoot_ = true;
    lastSecuredLoot_.clear();
    for (const LootStack& loot : expedition_.pendingLoot) lastSecuredLoot_.push_back(loot.id);
    for (const auto& [id, quantity] : fitPlan) baseState_.addStorage(id, quantity);
    for (const auto& [item, quantity] : itemFitPlan) baseState_.addItemStorage(item, quantity);
    if (!overflowPlan.empty()) {
        const std::string grantId = "return-" + std::to_string(++returnGrantSequence_);
        for (const auto& [id, quantity] : overflowPlan)
            baseState_.rewardOverflow.stacks.push_back({grantId, id, quantity});
    }

    for (const DiscoveryId& discovery : expedition_.pendingDiscoveries)
        baseState_.discoveryRegistry.insert(discovery);
    for (const auto& [key, achieved] : expedition_.pendingSiteAccessUpdates) {
        auto it = baseState_.siteAccess.find(key);
        if (it == baseState_.siteAccess.end() || it->second < achieved) baseState_.siteAccess[key] = achieved;
    }
    for (RegionId regionId : expedition_.pendingRegionCompletions) baseState_.completedRegionIds.insert(regionId);
    // The bag has been committed above; resetToBase() must not return it a
    // second time.
    expedition_.bag.clear();
    resetToBase();
    justSecuredLoot_ = true;
    markPersistentStateChanged();
    return true;
}

bool GameApp::discardStorage(const LootId& id, int quantity) {
    if (quantity <= 0) return false;
    if (baseState_.materialStorageCap(id) == BaseState::kKeyMaterialStorageCap) return false;
    if (!baseState_.consumeStorage(id, quantity)) return false;
    markPersistentStateChanged();
    return true;
}

bool GameApp::discardItemStorage(ItemType type, int quantity) {
    if (quantity <= 0) return false;
    if (!baseState_.consumeItemStorage(type, quantity)) return false;
    markPersistentStateChanged();
    return true;
}

bool GameApp::discardOverflowStack(std::size_t index, int quantity) {
    auto& stacks = baseState_.rewardOverflow.stacks;
    if (index >= stacks.size() || quantity <= 0 || stacks[index].quantity < quantity) return false;
    stacks[index].quantity -= quantity;
    if (stacks[index].quantity == 0) stacks.erase(stacks.begin() + static_cast<std::ptrdiff_t>(index));
    markPersistentStateChanged();
    return true;
}

void GameApp::acknowledgeLootSecured() {
    justSecuredLoot_ = false;
}

bool GameApp::togglePartyMember(const std::string& unitId) {
    if (screen_ != Screen::Base) return false;
    auto it = std::find(selectedPartyIds_.begin(), selectedPartyIds_.end(), unitId);
    if (it != selectedPartyIds_.end()) {
        selectedPartyIds_.erase(it);
        markPersistentStateChanged();
        return true;
    }
    if (selectedPartyIds_.size() >= 4) return false;
    if (std::none_of(roster_.begin(), roster_.end(), [&](const auto& unit) { return unit.id == unitId; })) return false;
    selectedPartyIds_.push_back(unitId);
    markPersistentStateChanged();
    return true;
}

bool GameApp::craftItem(ItemType type) {
    if (screen_ != Screen::Base) return false;
    if (baseState_.ownedItemCount(type) >= BaseState::kItemStorageCap) return false;
    const std::vector<ItemCraftCost> cost = itemCraftCost(type);
    for (const ItemCraftCost& line : cost)
        if (baseState_.storageCount(line.materialId) < line.quantity) return false;
    for (const ItemCraftCost& line : cost) baseState_.consumeStorage(line.materialId, line.quantity);
    baseState_.addItemStorage(type, 1);
    markPersistentStateChanged();
    return true;
}

bool GameApp::addPreparedItem(ItemType item) {
    if (screen_ != Screen::Base || preparedBag_.size() >= ExpeditionState::kBagCapacity) return false;
    if (!baseState_.consumeItemStorage(item, 1)) return false;
    preparedBag_.push_back(item);
    markPersistentStateChanged();
    return true;
}

void GameApp::removePreparedItem(std::size_t index) {
    if (screen_ != Screen::Base || index >= preparedBag_.size()) return;
    baseState_.addItemStorage(preparedBag_[index], 1);
    preparedBag_.erase(preparedBag_.begin() + index);
    markPersistentStateChanged();
}

std::vector<GameApp::RegionSummary> GameApp::regionSummaries() const {
    return computeRegionSummaries(data_, baseState_);
}

bool GameApp::startExpedition(RegionId regionId) {
    if (screen_ != Screen::Base || selectedPartyIds_.size() != 4 || !isRegionUnlocked(regionId)) return false;
    activeExpeditionData_ = data_;
    activeExpeditionData_.playerParty.clear();
    for (const std::string& id : selectedPartyIds_) {
        auto it = std::find_if(roster_.begin(), roster_.end(), [&](const auto& unit) { return unit.id == id; });
        if (it != roster_.end()) activeExpeditionData_.playerParty.push_back(*it);
    }
    if (activeExpeditionData_.playerParty.size() != 4) return false;
    expedition_ = ExpeditionState{};
    expedition_.regionId = regionId;
    if (usesRouteGraph(regionId)) expedition_.routeProgress = initialRouteProgress(regionId);
    expedition_.bag = preparedBag_;
    expeditionSeed_ = makeExpeditionSeed();
    isReconnaissanceRun_ = false;
    stageDiscoveryAwarded_.assign(regionDescriptor(regionId, data_).stages.size(), false);
    expeditionPartyUnits_.clear();
    for (const UnitTemplate& unitTemplate : activeExpeditionData_.playerParty)
        expeditionPartyUnits_.push_back(
            instantiateUnit(activeExpeditionData_, unitTemplate, Team::Player, GridPos{0, 0}, &weaponOverrides_));
    screen_ = Screen::Exploration;
    justSecuredLoot_ = false;
    updateExpeditionCheckpoint(ExpeditionCheckpoint::Stage::Exploration);
    return true;
}

bool GameApp::partyHasFrontierScout() const {
    for (const UnitTemplate& unit : activeExpeditionData_.playerParty) {
        if (unit.classId == UnitClass::FrontierScout) return true;
    }
    return false;
}

bool GameApp::partyHasClass(UnitClass unitClass) const {
    for (const UnitTemplate& unit : activeExpeditionData_.playerParty) {
        if (unit.classId == unitClass) return true;
    }
    return false;
}

SiteAccessState GameApp::currentSiteAccess() const {
    return computeCurrentSiteAccess(expedition_, baseState_, data_);
}

bool GameApp::chooseSafePassage() {
    if (screen_ != Screen::Exploration || currentSiteAccess() != SiteAccessState::Secured) return false;
    const StageDescriptor stage = currentStage();
    lastExplorationChoice_ = ExplorationChoice::FrontalAdvance;
    isReconnaissanceRun_ = false;
    if (expeditionPartyUnits_.empty()) {
        // No prior battle exists this run yet - a fresh, full-HP party is
        // correct here (this is the expedition's starting state, not a
        // free heal).
        battleController_ = std::make_unique<BattleController>(createScenarioBattle(
            activeExpeditionData_, stage, expeditionSeed_, cinderwatchOutcome(ExplorationChoice::FrontalAdvance),
            nullptr, &weaponOverrides_));
    } else {
        // Preserve the party's current HP/status exactly like
        // continueExpedition() does - safe passage must not silently heal
        // the party for free once a region has more than one stage.
        battleController_ = std::make_unique<BattleController>(
            createScenarioContinuationBattle(activeExpeditionData_, expeditionPartyUnits_, stage, expeditionSeed_));
    }
    applyEquipmentTraits(*battleController_);
    applyEquippedSkills(*battleController_);
    // No battle fought - straight to Camp with no loot/discoveries (docs/
    // regions/ashbough_forest.md: "狼戦と探索3択を省略。報酬なし").
    if (expedition_.routeProgress) {
        expedition_.routeProgress->resolvedNodeIds.insert(expedition_.routeProgress->currentNodeId);
        expedition_.routeProgress->safelyPassedNodeIds.insert(expedition_.routeProgress->currentNodeId);
    }
    justSecuredLoot_ = false;
    screen_ = Screen::Camp;
    updateExpeditionCheckpoint(ExpeditionCheckpoint::Stage::Camp);
    return true;
}

bool GameApp::chooseReconnaissance() {
    if (screen_ != Screen::Exploration || currentSiteAccess() != SiteAccessState::Secured) return false;
    const StageDescriptor stage = currentStage();
    lastExplorationChoice_ = ExplorationChoice::FrontalAdvance;
    isReconnaissanceRun_ = true;
    if (expeditionPartyUnits_.empty()) {
        battleController_ = std::make_unique<BattleController>(createScenarioBattle(
            activeExpeditionData_, stage, expeditionSeed_, cinderwatchOutcome(ExplorationChoice::FrontalAdvance),
            nullptr, &weaponOverrides_));
    } else {
        battleController_ = std::make_unique<BattleController>(
            createScenarioContinuationBattle(activeExpeditionData_, expeditionPartyUnits_, stage, expeditionSeed_));
    }
    applyEquipmentTraits(*battleController_);
    applyEquippedSkills(*battleController_);
    screen_ = Screen::Battle;
    return true;
}

int GameApp::bulkPassSecuredSites() {
    if (screen_ != Screen::Exploration || !expedition_.routeProgress) return 0;

    int passed = 0;
    // Mirrors continueExpedition()'s own guard ("don't advance once the
    // expedition is already complete"): stop the instant the site just
    // marked resolved is the last one before the Exit, WITHOUT calling
    // advanceRouteToNextSite() - that call would move currentNodeId to the
    // Exit node itself, which breaks expeditionComplete()'s invariant that
    // currentNodeId always names the last *resolved Site*, not the Exit.
    while (currentStage().contentImplemented && currentSiteAccess() == SiteAccessState::Secured) {
        expedition_.routeProgress->resolvedNodeIds.insert(expedition_.routeProgress->currentNodeId);
        expedition_.routeProgress->safelyPassedNodeIds.insert(expedition_.routeProgress->currentNodeId);
        expedition_.battlesWon += 1;
        ++passed;
        if (expeditionComplete()) break;
        if (!advanceRouteToNextSite()) break; // defensive: shouldn't happen given the check above
    }
    if (passed == 0) return 0;

    // Same "no battle fought, just need a party-state container for the
    // stopping screen" rebuild as chooseSafePassage(), done once at the end
    // rather than per intermediate site. currentNodeId is always a real
    // Site here (see above), so currentStage() is always valid.
    const StageDescriptor stage = currentStage();
    if (expeditionPartyUnits_.empty()) {
        battleController_ = std::make_unique<BattleController>(createScenarioBattle(
            activeExpeditionData_, stage, expeditionSeed_, cinderwatchOutcome(ExplorationChoice::FrontalAdvance),
            nullptr, &weaponOverrides_));
    } else {
        battleController_ = std::make_unique<BattleController>(
            createScenarioContinuationBattle(activeExpeditionData_, expeditionPartyUnits_, stage, expeditionSeed_));
    }
    applyEquipmentTraits(*battleController_);
    applyEquippedSkills(*battleController_);
    lastExplorationChoice_ = ExplorationChoice::FrontalAdvance;
    isReconnaissanceRun_ = false;
    justSecuredLoot_ = false;

    if (expeditionComplete()) {
        screen_ = Screen::Camp;
        updateExpeditionCheckpoint(ExpeditionCheckpoint::Stage::Camp);
    } else {
        screen_ = Screen::Exploration;
        updateExpeditionCheckpoint(ExpeditionCheckpoint::Stage::Exploration);
    }
    return passed;
}

bool GameApp::chooseExplorationRoute(ExplorationChoice choice) {
    if (screen_ != Screen::Exploration || !currentStage().contentImplemented) return false;
    if (!expedition_.routeProgress && expedition_.stageIndex != 0) return false;
    const StageDescriptor stage = currentStage();
    if (choice == ExplorationChoice::ScoutRoute && stage.scoutRouteDisabled) return false;
    if (choice == ExplorationChoice::ScoutRoute &&
        !partyHasClass(stage.scoutRouteRequiredClass.value_or(UnitClass::FrontierScout)))
        return false;
    if (currentSiteAccess() == SiteAccessState::Secured) return false;

    isReconnaissanceRun_ = false;
    lastExplorationChoice_ = choice;
    ExplorationOutcome outcome = stageRouteOutcome(stage, choice);
    if (outcome.enableFreeDeployment) {
        deploymentOutcome_ = outcome;
        deploymentTerrain_ = generateFieldTerrain(activeExpeditionData_.terrainProfile(stage.terrainProfileId),
                                                  expeditionSeed_);
        deploymentPlayers_.clear();
        for (const Unit& snapshot : expeditionPartyUnits_) {
            if (snapshot.team != Team::Player || !snapshot.isAlive()) continue;
            Unit unit = snapshot;
            unit.position = {0, 0};
            deploymentPlayers_.push_back(std::move(unit));
        }
        deploymentPlaced_.assign(deploymentPlayers_.size(), false);
        deploymentEnemyPreview_ = previewEnemies(activeExpeditionData_, stage, expeditionSeed_, outcome,
                                                 static_cast<int>(deploymentPlayers_.size()));
        for (const Unit& enemy : deploymentEnemyPreview_) {
            const int key = enemy.position.row * kGridCols + enemy.position.col;
            if (!isPassable(deploymentTerrain_[key])) deploymentTerrain_[key] = TerrainType::Floor;
        }
        screen_ = Screen::PreBattleDeployment;
        return true;
    }

    battleController_ = std::make_unique<BattleController>(createScenarioContinuationBattle(
        activeExpeditionData_, expeditionPartyUnits_, stage, expeditionSeed_, outcome));
    applyEquipmentTraits(*battleController_);
    applyEquippedSkills(*battleController_);
    screen_ = Screen::Battle;
    return true;
}

bool GameApp::placeDeploymentUnit(std::size_t partyIndex, GridPos pos) {
    if (screen_ != Screen::PreBattleDeployment || partyIndex >= deploymentPlayers_.size()) return false;
    if (pos.row < 0 || pos.row >= kGridRows) return false;
    if (pos.col < 0 || pos.col > deploymentOutcome_.deploymentMaxColumn) return false;
    for (std::size_t i = 0; i < deploymentPlayers_.size(); ++i) {
        if (i != partyIndex && deploymentPlaced_[i] && deploymentPlayers_[i].position == pos) return false;
    }
    deploymentPlayers_[partyIndex].position = pos;
    deploymentPlaced_[partyIndex] = true;
    return true;
}

bool GameApp::allDeploymentUnitsPlaced() const {
    if (deploymentPlaced_.empty()) return false;
    return std::all_of(deploymentPlaced_.begin(), deploymentPlaced_.end(), [](bool placed) { return placed; });
}

bool GameApp::isDeploymentUnitPlaced(std::size_t partyIndex) const {
    return partyIndex < deploymentPlaced_.size() && deploymentPlaced_[partyIndex];
}

bool GameApp::confirmDeployment() {
    if (screen_ != Screen::PreBattleDeployment || !allDeploymentUnitsPlaced()) return false;
    std::vector<GridPos> positions;
    for (const Unit& unit : deploymentPlayers_) positions.push_back(unit.position);
    battleController_ = std::make_unique<BattleController>(createScenarioContinuationBattle(
        activeExpeditionData_, expeditionPartyUnits_, currentStage(), expeditionSeed_, deploymentOutcome_, &positions));
    applyEquipmentTraits(*battleController_);
    applyEquippedSkills(*battleController_);
    screen_ = Screen::Battle;
    deploymentPlayers_.clear();
    deploymentPlaced_.clear();
    return true;
}

void GameApp::cancelDeployment() {
    if (screen_ != Screen::PreBattleDeployment) return;
    deploymentPlayers_.clear();
    deploymentPlaced_.clear();
    screen_ = Screen::Exploration;
}

bool GameApp::advanceOutpostStage() {
    if (screen_ != Screen::Base) return false;
    auto next = static_cast<OutpostStage>(static_cast<int>(baseState_.outpostStage) + 1);
    if (!eligibleForOutpostStage(baseState_, next)) return false;
    baseState_.outpostStage = next;
    markPersistentStateChanged();
    return true;
}

void GameApp::applyEquipmentTraits(BattleController& controller) {
    for (Unit& unit : controller.battle().units()) {
        if (unit.team != Team::Player) continue;
        auto it = equippedTraits_.find(unit.id);
        unit.knockbackNegatesRemaining =
            (it != equippedTraits_.end() && it->second == TuningTraitId::HideWrappedGrip) ? 1 : 0;
    }
}

void GameApp::applyEquippedSkills(BattleController& controller) {
    for (Unit& unit : controller.battle().units()) {
        if (unit.team != Team::Player) continue;
        auto it = equippedSkills_.find(unit.id);
        for (std::size_t slot = 0; slot < unit.skillSlots.size(); ++slot) {
            unit.skillSlots[slot].skillId =
                it != equippedSkills_.end() ? it->second.equippedSkillIds[slot] : std::string{};
        }
        // BattleController's constructor already ran initializeSkillCharges()
        // against these slots while they were still empty; re-run it now
        // that the real equipped ids are in place.
        initializeSkillCharges(unit);
    }
}

bool GameApp::equipSkillForUnit(const std::string& unitId, int slotIndex, const std::string& skillId) {
    if (screen_ != Screen::Base || slotIndex < 0 || slotIndex > 1) return false;
    auto unit = std::find_if(roster_.begin(), roster_.end(),
                              [&](const UnitTemplate& candidate) { return candidate.id == unitId; });
    if (unit == roster_.end()) return false;

    if (skillId.empty()) {
        equippedSkills_[unitId].equippedSkillIds[static_cast<std::size_t>(slotIndex)].clear();
        markPersistentStateChanged();
        return true;
    }

    const SkillDefinition* definition = findSkill(skillId);
    if (!definition || definition->unitClass != unit->classId) return false;
    std::string requiredNode = requiredTrainingNodeIdFor(unit->classId);
    if (requiredNode.empty() || !baseState_.unlockedNodeIds.count(requiredNode)) return false;

    equippedSkills_[unitId].equippedSkillIds[static_cast<std::size_t>(slotIndex)] = skillId;
    markPersistentStateChanged();
    return true;
}

bool GameApp::unlockFacilityNode(const std::string& nodeId) {
    if (screen_ != Screen::Base) return false;
    const FacilityNode* node = findFacilityNode(nodeId);
    if (!node || !facilityNodeEligible(baseState_, *node)) return false;

    // Eligibility checks every cost first, so this consumption cannot leave a
    // partially-paid node when one of several materials is missing.
    for (const LootStack& cost : node->materialCosts) {
        if (!baseState_.consumeStorage(cost.id, cost.quantity)) return false;
    }
    baseState_.unlockedNodeIds.insert(nodeId);
    if (node->occupiesFacilitySlot) baseState_.constructedFacilityIds.insert(nodeId);
    markPersistentStateChanged();
    return true;
}

bool GameApp::equipWeaponForUnit(const std::string& unitId, const std::string& weaponId) {
    if (screen_ != Screen::Base) return false;
    if (!baseState_.constructedFacilityIds.count("simple_forge")) return false;
    auto unit = std::find_if(roster_.begin(), roster_.end(), [&](const UnitTemplate& candidate) {
        return candidate.id == unitId;
    });
    if (unit == roster_.end() || unit->classId != UnitClass::Spearman) return false;
    if (weaponId.empty()) {
        weaponOverrides_.erase(unitId);
        markPersistentStateChanged();
        return true;
    }
    if (data_.weaponsById.find(weaponId) == data_.weaponsById.end()) return false;
    static const std::unordered_map<std::string, std::string> requiredRecipes = {
        {"long_spear", "craft_long_spear"},
        {"heavy_spear", "craft_heavy_spear"},
        {"guard_spear", "craft_guard_spear"},
    };
    if (weaponId != "iron_spear") {
        auto recipe = requiredRecipes.find(weaponId);
        if (recipe == requiredRecipes.end() || !baseState_.unlockedNodeIds.count(recipe->second)) return false;
    }
    weaponOverrides_[unitId] = weaponId;
    markPersistentStateChanged();
    return true;
}

bool GameApp::equipTuningTraitForUnit(const std::string& unitId, TuningTraitId traitId) {
    if (screen_ != Screen::Base) return false;
    if (!baseState_.constructedFacilityIds.count("simple_forge")) return false;
    auto unit = std::find_if(roster_.begin(), roster_.end(), [&](const UnitTemplate& candidate) {
        return candidate.id == unitId;
    });
    if (unit == roster_.end() || unit->classId != UnitClass::Spearman) return false;
    if (traitId == TuningTraitId::None) {
        equippedTraits_.erase(unitId);
        markPersistentStateChanged();
        return true;
    }
    if (traitId != TuningTraitId::HideWrappedGrip) return false;
    if (!baseState_.unlockedNodeIds.count("trait_hide_wrapped_grip")) return false;
    equippedTraits_[unitId] = traitId;
    markPersistentStateChanged();
    return true;
}

SaveData GameApp::createSaveData(const std::string& language) const {
    SaveData save;
    save.base = baseState_;
    save.selectedPartyIds = selectedPartyIds_;
    save.unitWeaponOverrides = weaponOverrides_;
    for (const auto& [unitId, traitId] : equippedTraits_) {
        std::string traitString = tuningTraitIdToString(traitId);
        if (!traitString.empty()) save.unitEquippedTraits[unitId] = traitString;
    }
    for (const auto& [unitId, loadout] : equippedSkills_) {
        if (!loadout.equippedSkillIds[0].empty()) save.unitEquippedSkillsSlot0[unitId] = loadout.equippedSkillIds[0];
        if (!loadout.equippedSkillIds[1].empty()) save.unitEquippedSkillsSlot1[unitId] = loadout.equippedSkillIds[1];
    }
    save.language = language;
    save.expedition = expeditionCheckpoint_;
    return save;
}

bool GameApp::applySaveData(const SaveData& save) {
    if (screen_ != Screen::Base || save.schemaVersion < 1 || save.schemaVersion > kCurrentSaveSchemaVersion) return false;

    BaseState loadedBase = save.base;
    // Defensive self-consistency only (docs/base_development.md: built
    // facilities never expire, so there's no capacity to violate) - a
    // constructedFacilityIds entry that isn't actually an occupiesFacilitySlot node, or
    // whose unlock record is missing, indicates corrupt/foreign save data.
    for (auto it = loadedBase.constructedFacilityIds.begin(); it != loadedBase.constructedFacilityIds.end();) {
        const FacilityNode* node = findFacilityNode(*it);
        if (!node || !node->occupiesFacilitySlot || !loadedBase.unlockedNodeIds.count(*it))
            it = loadedBase.constructedFacilityIds.erase(it);
        else ++it;
    }

    std::vector<std::string> loadedParty;
    for (const std::string& id : save.selectedPartyIds) {
        bool exists = std::any_of(roster_.begin(), roster_.end(), [&](const UnitTemplate& unit) { return unit.id == id; });
        if (exists && std::find(loadedParty.begin(), loadedParty.end(), id) == loadedParty.end()) loadedParty.push_back(id);
    }
    if (loadedParty.size() != 4) loadedParty = selectedPartyIds_;

    std::unordered_map<std::string, std::string> requestedWeapons = save.unitWeaponOverrides;
    if (save.schemaVersion == 1) {
        for (const auto& [unitClass, weaponId] : save.weaponOverrides)
            for (const UnitTemplate& unit : roster_)
                if (unit.classId == unitClass) requestedWeapons[unit.id] = weaponId;
    }
    std::unordered_map<std::string, std::string> loadedWeapons;
    for (const auto& [unitId, weaponId] : requestedWeapons) {
        auto unit = std::find_if(roster_.begin(), roster_.end(), [&](const UnitTemplate& candidate) {
            return candidate.id == unitId;
        });
        if (unit == roster_.end() || unit->classId != UnitClass::Spearman || !data_.weaponsById.count(weaponId)) continue;
        const std::unordered_map<std::string, std::string> recipes = {
            {"long_spear", "craft_long_spear"}, {"heavy_spear", "craft_heavy_spear"},
            {"guard_spear", "craft_guard_spear"},
        };
        if (weaponId == "iron_spear") loadedWeapons[unitId] = weaponId;
        else if (auto recipe = recipes.find(weaponId);
                 recipe != recipes.end() && loadedBase.unlockedNodeIds.count(recipe->second))
            loadedWeapons[unitId] = weaponId;
    }
    std::unordered_map<std::string, std::string> requestedTraits = save.unitEquippedTraits;
    if (save.schemaVersion == 1) {
        for (const auto& [unitClass, traitString] : save.equippedTraits)
            for (const UnitTemplate& unit : roster_)
                if (unit.classId == unitClass) requestedTraits[unit.id] = traitString;
    }
    std::unordered_map<std::string, TuningTraitId> loadedTraits;
    for (const auto& [unitId, traitString] : requestedTraits) {
        TuningTraitId traitId = tuningTraitIdFromString(traitString);
        auto unit = std::find_if(roster_.begin(), roster_.end(), [&](const UnitTemplate& candidate) {
            return candidate.id == unitId;
        });
        if (unit != roster_.end() && unit->classId == UnitClass::Spearman &&
            traitId == TuningTraitId::HideWrappedGrip &&
            loadedBase.unlockedNodeIds.count("trait_hide_wrapped_grip"))
            loadedTraits[unitId] = traitId;
    }

    std::unordered_map<std::string, UnitSkillLoadout> loadedSkills;
    auto loadSkillSlot = [&](const std::unordered_map<std::string, std::string>& requested, std::size_t slotIndex) {
        for (const auto& [unitId, skillId] : requested) {
            if (skillId.empty()) continue;
            auto unit = std::find_if(roster_.begin(), roster_.end(),
                                     [&](const UnitTemplate& candidate) { return candidate.id == unitId; });
            if (unit == roster_.end()) continue;
            const SkillDefinition* definition = findSkill(skillId);
            if (!definition || definition->unitClass != unit->classId) continue;
            std::string requiredNode = requiredTrainingNodeIdFor(unit->classId);
            if (requiredNode.empty() || !loadedBase.unlockedNodeIds.count(requiredNode)) continue;
            loadedSkills[unitId].equippedSkillIds[slotIndex] = skillId;
        }
    };
    loadSkillSlot(save.unitEquippedSkillsSlot0, 0);
    loadSkillSlot(save.unitEquippedSkillsSlot1, 1);

    baseState_ = std::move(loadedBase);
    selectedPartyIds_ = std::move(loadedParty);
    weaponOverrides_ = std::move(loadedWeapons);
    equippedTraits_ = std::move(loadedTraits);
    equippedSkills_ = std::move(loadedSkills);
    persistentRevision_ = 0;
    expeditionCheckpoint_.reset();
    expeditionRevision_ = 0;

    if (save.expedition && save.expedition->partyUnits.size() <= 4) {
        activeExpeditionData_ = data_;
        activeExpeditionData_.playerParty.clear();
        for (const std::string& id : selectedPartyIds_) {
            auto unit = std::find_if(roster_.begin(), roster_.end(), [&](const auto& candidate) { return candidate.id == id; });
            if (unit != roster_.end()) activeExpeditionData_.playerParty.push_back(*unit);
        }
        const ExpeditionCheckpoint& checkpoint = *save.expedition;
        RegionDescriptor checkpointRegion = regionDescriptor(checkpoint.regionId, data_);
        std::optional<RouteProgressSnapshot> restoredRoute = checkpoint.routeProgress;
        bool routeValid = true;
        if (usesRouteGraph(checkpoint.regionId)) {
            if (!restoredRoute) restoredRoute = initialRouteProgress(checkpoint.regionId);
            const RegionRouteGraph& graph = regionRouteGraph(checkpoint.regionId);
            const RouteNodeDefinition* node = findRouteNode(graph, restoredRoute->currentNodeId);
            routeValid = restoredRoute->routeId == graph.routeId && node && node->kind == RouteNodeKind::Site &&
                         node->stageId.has_value();
        } else if (restoredRoute) {
            routeValid = false;
        }
        const bool stageIndexValid =
            checkpoint.expeditionStage >= 0 &&
            static_cast<std::size_t>(checkpoint.expeditionStage) < checkpointRegion.stages.size();
        if (activeExpeditionData_.playerParty.size() == 4 && stageIndexValid && routeValid) {
            expedition_ = ExpeditionState{};
            expedition_.regionId = checkpoint.regionId;
            expedition_.pendingLoot = checkpoint.pendingLoot;
            expedition_.pendingDiscoveries = checkpoint.pendingDiscoveries;
            expedition_.bag = checkpoint.bag;
            expedition_.battlesWon = checkpoint.battlesWon;
            expedition_.stageIndex = checkpoint.expeditionStage;
            expedition_.routeProgress = std::move(restoredRoute);
            expedition_.pendingSiteAccessUpdates = checkpoint.pendingSiteAccessUpdates;
            expedition_.pendingRegionCompletions = checkpoint.pendingRegionCompletions;
            expeditionSeed_ = checkpoint.seed;
            stageDiscoveryAwarded_ = checkpoint.stageDiscoveryAwarded;
            stageDiscoveryAwarded_.resize(checkpointRegion.stages.size(), false);
            justSecuredLoot_ = false;
            isReconnaissanceRun_ = false;

            expeditionPartyUnits_.clear();
            for (const UnitTemplate& unitTemplate : activeExpeditionData_.playerParty) {
                Unit unit = instantiateUnit(activeExpeditionData_, unitTemplate, Team::Player, GridPos{0, 0},
                                            &weaponOverrides_);
                auto snapshot = std::find_if(checkpoint.partyUnits.begin(), checkpoint.partyUnits.end(),
                                             [&](const auto& entry) { return entry.id == unit.id; });
                unit.currentHp = snapshot != checkpoint.partyUnits.end() ? snapshot->currentHp : unit.stats.maxHp;
                expeditionPartyUnits_.push_back(unit);
            }

            if (checkpoint.stage == ExpeditionCheckpoint::Stage::Camp) {
                battleController_ = std::make_unique<BattleController>(createScenarioContinuationBattle(
                    activeExpeditionData_, expeditionPartyUnits_, currentStage(), expeditionSeed_));
                applyEquipmentTraits(*battleController_);
                applyEquippedSkills(*battleController_);
                screen_ = Screen::Camp;
            } else {
                screen_ = Screen::Exploration;
            }
            ExpeditionCheckpoint normalized = checkpoint;
            normalized.routeProgress = expedition_.routeProgress;
            normalized.partyUnits.clear();
            for (const Unit& unit : expeditionPartyUnits_)
                normalized.partyUnits.push_back({unit.id, unit.currentHp});
            expeditionCheckpoint_ = std::move(normalized);
        } else if (activeExpeditionData_.playerParty.size() == 4) {
            // docs/expedition_recovery.md "更新後の復旧" 優先順位4: the
            // region/party are fine, but the specific Node/Stage this
            // checkpoint pointed to isn't (e.g. after a content change) -
            // fall back to the region's entrance rather than silently
            // discarding the whole expedition. "入口退避ではHP、戦闘不能、
            // Bag、Pendingを巻き戻さない" - only the route/stage position
            // resets to the start; Pending rewards/discoveries/site-access/
            // bag and party HP all survive.
            expedition_ = ExpeditionState{};
            expedition_.regionId = checkpoint.regionId;
            expedition_.pendingLoot = checkpoint.pendingLoot;
            expedition_.pendingDiscoveries = checkpoint.pendingDiscoveries;
            expedition_.bag = checkpoint.bag;
            expedition_.pendingSiteAccessUpdates = checkpoint.pendingSiteAccessUpdates;
            expedition_.pendingRegionCompletions = checkpoint.pendingRegionCompletions;
            if (usesRouteGraph(checkpoint.regionId))
                expedition_.routeProgress = initialRouteProgress(checkpoint.regionId);
            expeditionSeed_ = checkpoint.seed;
            stageDiscoveryAwarded_.assign(checkpointRegion.stages.size(), false);
            justSecuredLoot_ = false;
            isReconnaissanceRun_ = false;

            expeditionPartyUnits_.clear();
            for (const UnitTemplate& unitTemplate : activeExpeditionData_.playerParty) {
                Unit unit = instantiateUnit(activeExpeditionData_, unitTemplate, Team::Player, GridPos{0, 0},
                                            &weaponOverrides_);
                auto snapshot = std::find_if(checkpoint.partyUnits.begin(), checkpoint.partyUnits.end(),
                                             [&](const auto& entry) { return entry.id == unit.id; });
                unit.currentHp = snapshot != checkpoint.partyUnits.end() ? snapshot->currentHp : unit.stats.maxHp;
                expeditionPartyUnits_.push_back(unit);
            }
            screen_ = Screen::Exploration;
            updateExpeditionCheckpoint(ExpeditionCheckpoint::Stage::Exploration);
        }
    }
    return true;
}

void GameApp::resetToBase() {
    // docs/item_system.md "未使用消耗品は帰還・敗北のどちらでも倉庫へ戻る。
    // 使用済み消耗品は敗北しても戻らない。" - expedition_.bag only ever holds
    // what hasn't been consumed yet (useCampItem/useBattleHealingItem/etc.
    // remove consumed items via ExpeditionState::consume()), so whatever
    // remains here at the end of ANY exit path (safe return, defeat, or a
    // harsh retireExpedition()) goes back to owned storage unconditionally.
    for (ItemType item : expedition_.bag) baseState_.addItemStorage(item, 1);
    expedition_ = ExpeditionState{};
    stageDiscoveryAwarded_ = {};
    expeditionPartyUnits_.clear();
    preparedBag_.clear();
    expeditionSeed_ = makeExpeditionSeed();
    battleController_ = std::make_unique<BattleController>(
        createScenarioBattle(data_, idlePlaceholderStage(data_), expeditionSeed_));
    screen_ = Screen::Base;
    expeditionCheckpoint_.reset();
    ++expeditionRevision_;
}

void GameApp::updateExpeditionCheckpoint(ExpeditionCheckpoint::Stage stage) {
    ExpeditionCheckpoint checkpoint;
    checkpoint.stage = stage;
    checkpoint.regionId = expedition_.regionId;
    checkpoint.expeditionStage = expedition_.stageIndex;
    checkpoint.seed = expeditionSeed_;
    checkpoint.pendingLoot = expedition_.pendingLoot;
    checkpoint.pendingDiscoveries = expedition_.pendingDiscoveries;
    checkpoint.bag = expedition_.bag;
    checkpoint.battlesWon = expedition_.battlesWon;
    checkpoint.routeProgress = expedition_.routeProgress;
    checkpoint.stageDiscoveryAwarded = stageDiscoveryAwarded_;
    checkpoint.pendingSiteAccessUpdates = expedition_.pendingSiteAccessUpdates;
    checkpoint.pendingRegionCompletions = expedition_.pendingRegionCompletions;
    if (stage == ExpeditionCheckpoint::Stage::Camp) syncPartySnapshotFromBattle();
    for (const Unit& unit : expeditionPartyUnits_)
        checkpoint.partyUnits.push_back({unit.id, unit.currentHp});
    expeditionCheckpoint_ = std::move(checkpoint);
    ++expeditionRevision_;
}

} // namespace jf
