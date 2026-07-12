#pragma once

#include <memory>
#include <string>
#include <vector>

#include "jf/battle/BattleController.hpp"
#include "jf/core/ExpeditionState.hpp"
#include "jf/data/GameData.hpp"

namespace jf {

enum class Screen {
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

    void update(float dt);

    // Victory screen -> Camp, banking this battle's placeholder loot as "pending".
    void proceedToCamp();

    // Defeat screen -> fresh expedition. All pending loot is lost.
    void acknowledgeDefeat();

    // Camp -> new battle, keeping surviving party HP.
    void continueExpedition();

    // Camp -> shows "Loot Secured" (call acknowledgeLootSecured() to continue).
    void returnToBase();

    // Loot Secured screen -> brand new expedition.
    void acknowledgeLootSecured();

    bool justSecuredLoot() const { return justSecuredLoot_; }
    const std::vector<std::string>& lastSecuredLoot() const { return lastSecuredLoot_; }

private:
    void startFreshExpedition();

    GameData data_;
    std::unique_ptr<BattleController> battleController_;
    ExpeditionState expedition_;
    Screen screen_ = Screen::Battle;
    bool justSecuredLoot_ = false;
    std::vector<std::string> lastSecuredLoot_;
};

} // namespace jf
