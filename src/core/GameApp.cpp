#include "jf/core/GameApp.hpp"

#include "jf/battle/BattleFactory.hpp"

#include <algorithm>
#include <random>

namespace jf {

namespace {
const std::vector<std::vector<LootStack>> kVictoryLoot = {
    {{{"gate_tools", 1}, {"ash_road_map", 1}}},
    {{{"field_medicine", 1}, {"watch_ledger", 1}}},
    {{{"signal_lens", 1}, {"captains_seal", 1}, {kAshveilFangMaterial, 1}}},
};
const std::vector<std::string> kMissionNames = {
    "Cinderwatch Gate", "Ironwatch Stores", "The Last Signal"
};

// Matches docs/base_development.md's "最初の縦切り実装" table: each of the 3
// mission stages unlocks one facility branch's discovery on safe return.
const std::array<const char*, 3> kStageDiscoveries = {
    kCinderwatchReconDiscovery, kFieldMedicineDiscovery, kReturnSignalDiscovery
};

std::uint32_t makeExpeditionSeed() {
    std::random_device device;
    return (static_cast<std::uint32_t>(device()) << 1u) ^ static_cast<std::uint32_t>(device());
}
} // namespace

GameApp::GameApp(GameData data) : data_(std::move(data)) {
    roster_ = data_.playerParty;
    roster_.insert(roster_.end(), data_.reserveRoster.begin(), data_.reserveRoster.end());
    for (const auto& unit : data_.playerParty) selectedPartyIds_.push_back(unit.id);
    expeditionSeed_ = makeExpeditionSeed();
    battleController_ = std::make_unique<BattleController>(
        createScenarioBattle(data_, 0, expeditionSeed_));
}

void GameApp::update(float dt) {
    if (screen_ == Screen::Battle) {
        battleController_->update(dt);
    }
}

void GameApp::proceedToCamp() {
    if (battleController_->inputState() != BattleInputState::Victory) return;
    const auto& loot = kVictoryLoot[static_cast<std::size_t>(expedition_.stage)];
    expedition_.pendingLoot.insert(expedition_.pendingLoot.end(), loot.begin(), loot.end());
    std::size_t stageIndex = static_cast<std::size_t>(expedition_.stage);
    if (stageIndex < kStageDiscoveries.size() && !stageDiscoveryAwarded_[stageIndex]) {
        expedition_.pendingDiscoveries.push_back(kStageDiscoveries[stageIndex]);
        stageDiscoveryAwarded_[stageIndex] = true;
    }
    expedition_.battlesWon += 1;
    justSecuredLoot_ = false;
    screen_ = Screen::Camp;
}

void GameApp::acknowledgeDefeat() {
    if (battleController_->inputState() != BattleInputState::Defeat) return;
    lastSecuredLoot_.clear();
    resetToBase();
}

void GameApp::continueExpedition() {
    if (screen_ != Screen::Camp || expeditionComplete()) return;
    std::vector<Unit> survivors = battleController_->battle().units();
    ++expedition_.stage;
    battleController_ = std::make_unique<BattleController>(
        createScenarioContinuationBattle(data_, survivors, expedition_.stage, expeditionSeed_));
    screen_ = Screen::Battle;
}

bool GameApp::useBattleHealingItem(ItemType item) {
    if (screen_ != Screen::Battle || expedition_.count(item) <= 0) return false;
    if (!battleController_->useHealingItem(healingAmount(item))) return false;
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
        return changed && expedition_.consume(item);
    }
    if (item == ItemType::ReturnFlare) {
        returnToBase();
        return true;
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
    return expedition_.consume(item);
}

std::string GameApp::currentMissionName() const {
    return kMissionNames[static_cast<std::size_t>(std::clamp(expedition_.stage, 0, 2))];
}

void GameApp::returnToBase() {
    if (screen_ != Screen::Camp) return;
    justSecuredLoot_ = true;
    lastSecuredLoot_.clear();
    for (const LootStack& loot : expedition_.pendingLoot) {
        lastSecuredLoot_.push_back(loot.id);
        auto stored = std::find_if(baseState_.storage.begin(), baseState_.storage.end(),
                                   [&](const LootStack& entry) { return entry.id == loot.id; });
        if (stored == baseState_.storage.end()) baseState_.storage.push_back(loot);
        else stored->quantity += loot.quantity;
    }
    for (const DiscoveryId& discovery : expedition_.pendingDiscoveries)
        baseState_.discoveryRegistry.insert(discovery);
    resetToBase();
    justSecuredLoot_ = true;
}

void GameApp::acknowledgeLootSecured() {
    justSecuredLoot_ = false;
}

bool GameApp::togglePartyMember(const std::string& unitId) {
    if (screen_ != Screen::Base) return false;
    auto it = std::find(selectedPartyIds_.begin(), selectedPartyIds_.end(), unitId);
    if (it != selectedPartyIds_.end()) {
        selectedPartyIds_.erase(it);
        return true;
    }
    if (selectedPartyIds_.size() >= 4) return false;
    if (std::none_of(roster_.begin(), roster_.end(), [&](const auto& unit) { return unit.id == unitId; })) return false;
    selectedPartyIds_.push_back(unitId);
    return true;
}

bool GameApp::addPreparedItem(ItemType item) {
    if (screen_ != Screen::Base || preparedBag_.size() >= ExpeditionState::kBagCapacity) return false;
    preparedBag_.push_back(item);
    return true;
}

void GameApp::removePreparedItem(std::size_t index) {
    if (screen_ == Screen::Base && index < preparedBag_.size()) preparedBag_.erase(preparedBag_.begin() + index);
}

bool GameApp::startExpedition() {
    if (screen_ != Screen::Base || selectedPartyIds_.size() != 4) return false;
    activeExpeditionData_ = data_;
    activeExpeditionData_.playerParty.clear();
    for (const std::string& id : selectedPartyIds_) {
        auto it = std::find_if(roster_.begin(), roster_.end(), [&](const auto& unit) { return unit.id == id; });
        if (it != roster_.end()) activeExpeditionData_.playerParty.push_back(*it);
    }
    if (activeExpeditionData_.playerParty.size() != 4) return false;
    expedition_ = ExpeditionState{};
    expedition_.bag = preparedBag_;
    expeditionSeed_ = makeExpeditionSeed();
    stageDiscoveryAwarded_ = {};
    screen_ = Screen::Exploration;
    justSecuredLoot_ = false;
    return true;
}

bool GameApp::partyHasFrontierScout() const {
    for (const UnitTemplate& unit : activeExpeditionData_.playerParty) {
        if (unit.classId == UnitClass::FrontierScout) return true;
    }
    return false;
}

bool GameApp::chooseCinderwatchRoute(ExplorationChoice choice) {
    if (screen_ != Screen::Exploration || expedition_.stage != 0) return false;
    if (choice == ExplorationChoice::ScoutRoute && !partyHasFrontierScout()) return false;

    ExplorationOutcome outcome = cinderwatchOutcome(choice);
    if (outcome.enableFreeDeployment) {
        deploymentOutcome_ = outcome;
        deploymentTerrain_ = generateFieldTerrain(fieldTypeForStage(0), expeditionSeed_);
        deploymentPlayers_.clear();
        for (const UnitTemplate& unitTemplate : activeExpeditionData_.playerParty) {
            deploymentPlayers_.push_back(
                instantiateUnit(activeExpeditionData_, unitTemplate, Team::Player, GridPos{0, 0}));
        }
        deploymentPlaced_.assign(deploymentPlayers_.size(), false);
        deploymentEnemyPreview_ = previewEnemies(activeExpeditionData_, 0, outcome);
        screen_ = Screen::PreBattleDeployment;
        return true;
    }

    battleController_ = std::make_unique<BattleController>(
        createScenarioBattle(activeExpeditionData_, 0, expeditionSeed_, outcome));
    screen_ = Screen::Battle;
    return true;
}

bool GameApp::placeDeploymentUnit(std::size_t partyIndex, GridPos pos) {
    if (screen_ != Screen::PreBattleDeployment || partyIndex >= deploymentPlayers_.size()) return false;
    if (pos.row < 0 || pos.row >= kGridRows) return false;
    if (pos.col < 0 || pos.col > deploymentOutcome_.deploymentMaxColumn) return false;
    if (!isPassable(deploymentTerrain_[pos.row * kGridCols + pos.col])) return false;
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
    battleController_ = std::make_unique<BattleController>(
        createScenarioBattle(activeExpeditionData_, 0, expeditionSeed_, deploymentOutcome_, &positions));
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
    return true;
}

void GameApp::resetToBase() {
    expedition_ = ExpeditionState{};
    stageDiscoveryAwarded_ = {};
    preparedBag_.clear();
    expeditionSeed_ = makeExpeditionSeed();
    battleController_ = std::make_unique<BattleController>(createScenarioBattle(data_, 0, expeditionSeed_));
    screen_ = Screen::Base;
}

} // namespace jf
