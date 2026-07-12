#include "jf/core/GameApp.hpp"

#include "jf/battle/BattleFactory.hpp"

namespace jf {

namespace {
const std::vector<std::string> kVictoryLoot = {"Iron Ore", "Old Sword"};
} // namespace

GameApp::GameApp(GameData data) : data_(std::move(data)) {
    battleController_ = std::make_unique<BattleController>(createFreshBattle(data_));
}

void GameApp::update(float dt) {
    if (screen_ == Screen::Battle) {
        battleController_->update(dt);
    }
}

void GameApp::proceedToCamp() {
    if (battleController_->inputState() != BattleInputState::Victory) return;
    expedition_.pendingLoot.insert(expedition_.pendingLoot.end(), kVictoryLoot.begin(), kVictoryLoot.end());
    expedition_.battlesWon += 1;
    justSecuredLoot_ = false;
    screen_ = Screen::Camp;
}

void GameApp::acknowledgeDefeat() {
    if (battleController_->inputState() != BattleInputState::Defeat) return;
    startFreshExpedition();
}

void GameApp::continueExpedition() {
    if (screen_ != Screen::Camp) return;
    std::vector<Unit> survivors = battleController_->battle().units();
    battleController_ = std::make_unique<BattleController>(createContinuationBattle(data_, survivors));
    screen_ = Screen::Battle;
}

void GameApp::returnToBase() {
    if (screen_ != Screen::Camp) return;
    justSecuredLoot_ = true;
    lastSecuredLoot_ = expedition_.pendingLoot;
}

void GameApp::acknowledgeLootSecured() {
    justSecuredLoot_ = false;
    startFreshExpedition();
}

void GameApp::startFreshExpedition() {
    expedition_ = ExpeditionState{};
    battleController_ = std::make_unique<BattleController>(createFreshBattle(data_));
    screen_ = Screen::Battle;
}

} // namespace jf
