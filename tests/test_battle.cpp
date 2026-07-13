#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>

#include "jf/battle/BattleController.hpp"
#include "jf/battle/BattleFactory.hpp"
#include "jf/battle/CombatResolver.hpp"
#include "jf/battle/Movement.hpp"
#include "jf/core/BaseState.hpp"
#include "jf/core/Exploration.hpp"
#include "jf/core/GameApp.hpp"

namespace {

jf::Unit makeUnit(std::string id, jf::Team team, jf::GridPos pos, int move = 4,
                  jf::UnitClass unitClass = jf::UnitClass::MarchCaptain) {
    jf::Unit unit;
    unit.id = id;
    unit.name = id;
    unit.team = team;
    unit.position = pos;
    unit.unitClass = unitClass;
    unit.stats = {.maxHp = 20, .strength = 6, .magic = 0, .speed = 5,
                  .defense = 2, .resistance = 1, .move = move};
    unit.currentHp = unit.stats.maxHp;
    unit.weapon = {.id = "sword", .name = "Sword", .might = 5,
                   .minRange = 1, .maxRange = 1, .damageType = jf::DamageType::Physical};
    return unit;
}

bool contains(const std::vector<jf::GridPos>& tiles, jf::GridPos pos) {
    return std::find(tiles.begin(), tiles.end(), pos) != tiles.end();
}

jf::GameData makeFactoryData() {
    jf::GameData data;
    jf::Stats stats{.maxHp = 20, .strength = 6, .magic = 0, .speed = 5,
                    .defense = 2, .resistance = 1, .move = 4};
    jf::Weapon sword{.id = "sword", .name = "Sword", .might = 5, .minRange = 1,
                     .maxRange = 1, .damageType = jf::DamageType::Physical};
    data.weaponsById.emplace("sword", sword);
    data.classesById.emplace(jf::UnitClass::MarchCaptain,
                             jf::ClassDefinition{jf::UnitClass::MarchCaptain, stats, "sword"});
    data.classesById.emplace(jf::UnitClass::FrontierScout,
                             jf::ClassDefinition{jf::UnitClass::FrontierScout, stats, "sword"});
    for (int i = 0; i < 4; ++i)
        data.playerParty.push_back({"player" + std::to_string(i), "Player", jf::UnitClass::MarchCaptain});
    for (int i = 0; i < 4; ++i)
        data.enemyRoster.push_back({"enemy" + std::to_string(i), "Enemy", jf::UnitClass::MarchCaptain});
    return data;
}

jf::GameData makeScoutFactoryData() {
    jf::GameData data = makeFactoryData();
    data.playerParty[0].classId = jf::UnitClass::FrontierScout;
    return data;
}

// Forces a live GameApp battle to Victory by zeroing every enemy's HP and
// then running one no-op player action (select -> stay in place -> wait) so
// BattleController::evaluateOutcome() notices allEnemiesDefeated().
void winCurrentBattle(jf::GameApp& app) {
    for (jf::Unit& unit : app.battle().battle().units()) {
        if (unit.team == jf::Team::Enemy) unit.currentHp = 0;
    }
    jf::Unit* actor = nullptr;
    for (jf::Unit& unit : app.battle().battle().units()) {
        if (unit.team == jf::Team::Player && unit.isAlive() && !unit.hasActed) {
            actor = &unit;
            break;
        }
    }
    app.battle().selectUnit(*actor);
    app.battle().selectMoveTile(actor->position);
    app.battle().chooseWait();
}

} // namespace

int main() {
    {
        const auto a = jf::cinderwatchOutcome(jf::ExplorationChoice::FrontalAdvance);
        const auto b = jf::cinderwatchOutcome(jf::ExplorationChoice::CollapsedSidePath);
        assert(a.partyDamage == 0 && a.enemiesRemoved == 0);
        assert(b.partyDamage == 2 && b.enemiesRemoved == 1);

        const jf::GameData data = makeFactoryData();
        jf::BattleState standard = jf::createScenarioBattle(data, 0, 42, a);
        jf::BattleState sidePath = jf::createScenarioBattle(data, 0, 42, b);
        int standardEnemies = 0;
        int sidePathEnemies = 0;
        for (const jf::Unit& unit : standard.units()) {
            if (unit.team == jf::Team::Player) assert(unit.currentHp == unit.stats.maxHp);
            else ++standardEnemies;
        }
        for (const jf::Unit& unit : sidePath.units()) {
            if (unit.team == jf::Team::Player) assert(unit.currentHp == unit.stats.maxHp - 2);
            else ++sidePathEnemies;
        }
        assert(standardEnemies == 3);
        assert(sidePathEnemies == 2);

        jf::BattleState clamped = jf::createScenarioBattle(data, 0, 42, {.partyDamage = 999, .enemiesRemoved = 99});
        for (const jf::Unit& unit : clamped.units()) {
            if (unit.team == jf::Team::Player) assert(unit.currentHp == 1);
            else assert(false && "excess removal must leave no enemies");
        }

        jf::BaseState base;
        base.discoveryRegistry.insert(jf::kCinderwatchReconDiscovery);
        base.discoveryRegistry.insert(jf::kCinderwatchReconDiscovery);
        assert(base.discoveryRegistry.size() == 1);
    }
    {
        const std::vector<jf::GridPos> protectedSpawns = {
            {0, 0}, {1, 0}, {1, 1}, {2, 1}, {0, 6}, {0, 7}, {1, 5}, {2, 6}
        };
        for (jf::FieldType field : {jf::FieldType::CinderwatchOutpost,
                                    jf::FieldType::AshRoad,
                                    jf::FieldType::SignalTower}) {
            for (std::uint32_t seed = 0; seed < 100; ++seed) {
                const auto terrain = jf::generateFieldTerrain(field, seed);
                assert(terrain == jf::generateFieldTerrain(field, seed));
                for (jf::GridPos spawn : protectedSpawns)
                    assert(terrain[spawn.row * jf::kGridCols + spawn.col] == jf::TerrainType::Floor);

                for (int col = 0; col < jf::kGridCols; ++col) {
                    int barriers = 0;
                    for (int row = 0; row < jf::kGridRows; ++row) {
                        if (terrain[row * jf::kGridCols + col] == jf::TerrainType::Barrier) ++barriers;
                    }
                    assert(barriers <= 1);
                    if (col < 2 || col > 5) assert(barriers == 0);
                }

                jf::Unit explorer = makeUnit("explorer", jf::Team::Player, {1, 0}, 99);
                jf::BattleState battle({explorer}, terrain);
                const auto reachable = jf::computeReachableTiles(battle, battle.units().front());
                bool reachesRightEdge = false;
                for (jf::GridPos pos : reachable) reachesRightEdge |= pos.col == jf::kGridCols - 1;
                assert(reachesRightEdge);
            }
        }
    }

    {
        jf::Unit mover = makeUnit("mover", jf::Team::Player, {1, 0}, 3);
        jf::BattleState battle({mover});
        battle.setTerrain({1, 1}, jf::TerrainType::Ash);
        battle.setTerrain({0, 0}, jf::TerrainType::Barrier);

        const auto reachable = jf::computeReachableTiles(battle, battle.units().front());
        assert(contains(reachable, {1, 1}));
        assert(contains(reachable, {1, 2}));
        assert(!contains(reachable, {1, 3}));
        assert(!contains(reachable, {0, 0}));
    }

    {
        jf::Unit captain = makeUnit("captain", jf::Team::Player, {1, 0}, 2);
        jf::Unit scout = makeUnit("scout", jf::Team::Player, {2, 0}, 2,
                                  jf::UnitClass::FrontierScout);
        jf::BattleState captainBattle({captain});
        jf::BattleState scoutBattle({scout});
        captainBattle.setTerrain({1, 1}, jf::TerrainType::Ash);
        scoutBattle.setTerrain({2, 1}, jf::TerrainType::Ash);
        const auto captainReach = jf::computeReachableTiles(captainBattle, captainBattle.units().front());
        const auto scoutReach = jf::computeReachableTiles(scoutBattle, scoutBattle.units().front());
        assert(!contains(captainReach, {1, 2}));
        assert(contains(scoutReach, {2, 2}));
    }

    {
        jf::Unit mover = makeUnit("mover", jf::Team::Player, {1, 0}, 6);
        jf::Unit guard = makeUnit("guard", jf::Team::Enemy, {0, 2}, 3,
                                  jf::UnitClass::VeteranGuard);
        jf::BattleState battle({mover, guard});
        for (int col = 0; col < jf::kGridCols; ++col) battle.setTerrain({2, col}, jf::TerrainType::Barrier);
        const auto reachable = jf::computeReachableTiles(battle, battle.units().front());
        assert(contains(reachable, {1, 2}));
        assert(!contains(reachable, {1, 3}));
    }

    {
        jf::Unit captain = makeUnit("captain", jf::Team::Player, {1, 0});
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {1, 1});
        jf::Unit attacker = makeUnit("attacker", jf::Team::Enemy, {1, 3});
        jf::BattleState battle({captain, ally, attacker});
        assert(battle.combatDefenseBonus(battle.units()[1], battle.units()[2]) == 1);

        battle.units()[1].unitClass = jf::UnitClass::Spearman;
        battle.units()[2].tilesMovedThisAction = 2;
        assert(battle.combatDefenseBonus(battle.units()[1], battle.units()[2]) == 3);
    }

    {
        jf::Unit mover = makeUnit("mover", jf::Team::Player, {1, 0}, 6);
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {1, 1});
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 3});
        jf::BattleState battle({mover, ally, enemy});
        for (int col = 0; col < jf::kGridCols; ++col) {
            battle.setTerrain({0, col}, jf::TerrainType::Barrier);
            battle.setTerrain({2, col}, jf::TerrainType::Barrier);
        }

        const auto reachable = jf::computeReachableTiles(battle, battle.units().front());
        assert(!contains(reachable, {1, 1})); // ally can be crossed, not occupied
        assert(contains(reachable, {1, 2}));  // reached through the ally
        assert(!contains(reachable, {1, 3})); // enemy blocks stopping
        assert(!contains(reachable, {1, 4})); // enemy blocks crossing
    }

    {
        jf::Unit archer = makeUnit("archer", jf::Team::Player, {1, 1});
        archer.unitClass = jf::UnitClass::WatchArcher;
        archer.weapon.minRange = 2;
        archer.weapon.maxRange = 3;
        const auto range = jf::computeAttackRangeTiles(archer, {{1, 1}, {1, 2}});
        assert(!contains(range, {1, 1}));
        assert(contains(range, {1, 4}));
        assert(contains(range, {0, 3}));

        archer.weapon.minRange = 1;
        const auto enforcedRange = jf::computeAttackRangeTiles(archer, {{1, 1}});
        assert(!contains(enforcedRange, {1, 2}));
    }

    {
        jf::Unit attacker = makeUnit("attacker", jf::Team::Player, {1, 1});
        jf::Unit defender = makeUnit("defender", jf::Team::Enemy, {1, 2});
        assert(jf::computeDamage(attacker, defender, 2) == jf::computeDamage(attacker, defender) - 2);
        defender.stats.defense = 99;
        assert(jf::computeDamage(attacker, defender, 2) == 1);
    }

    {
        jf::Unit player = makeUnit("player", jf::Team::Player, {1, 0}, 3);
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 7});
        jf::BattleController controller(jf::BattleState({player, enemy}));
        controller.selectUnit(controller.battle().units().front());
        assert(controller.inputState() == jf::BattleInputState::SelectMove);
        controller.selectMoveTile({1, 2});
        assert(controller.inputState() == jf::BattleInputState::SelectAction);
        assert(controller.reachableTiles().empty());
        controller.returnToMoveSelection();
        assert(controller.inputState() == jf::BattleInputState::SelectMove);
        assert((controller.selectedUnit()->position == jf::GridPos{1, 0}));
    }

    {
        jf::Unit player = makeUnit("player", jf::Team::Player, {1, 0});
        player.currentHp = 5;
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 7});
        jf::BattleController controller(jf::BattleState({player, enemy}));
        controller.selectUnit(controller.battle().units().front());
        controller.selectMoveTile({1, 0});
        assert(controller.useHealingItem(8));
        assert(controller.battle().units().front().currentHp == 13);
        assert(controller.battle().units().front().hasActed);
    }

    {
        jf::Unit healer = makeUnit("healer", jf::Team::Player, {1, 0}, 4,
                                   jf::UnitClass::DawnChirurgeon);
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {1, 1});
        ally.currentHp = 4;
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 7});
        jf::BattleController controller(jf::BattleState({healer, ally, enemy}));
        controller.selectUnit(controller.battle().units().front());
        controller.selectMoveTile({1, 0});
        controller.chooseHeal();
        assert(controller.inputState() == jf::BattleInputState::SelectHealTarget);
        controller.selectHealTarget({1, 1});
        assert(controller.battle().units()[1].currentHp == 12);
        assert(controller.battle().units().front().hasActed);
    }

    {
        // Option C (ScoutRoute) must stay locked out without a Frontier Scout,
        // while A/B remain available regardless.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        app.startExpedition();
        assert(!app.partyHasFrontierScout());
        assert(!app.chooseCinderwatchRoute(jf::ExplorationChoice::ScoutRoute));
        assert(app.screen() == jf::Screen::Exploration);
        assert(app.chooseCinderwatchRoute(jf::ExplorationChoice::FrontalAdvance));
        assert(app.screen() == jf::Screen::Battle);
    }

    {
        // With a Frontier Scout in the party, option C transitions to
        // PreBattleDeployment instead of straight to Battle, and A/B still work.
        jf::GameData scoutData = makeScoutFactoryData();
        {
            jf::GameApp app(scoutData);
            app.startExpedition();
            assert(app.partyHasFrontierScout());
            assert(app.chooseCinderwatchRoute(jf::ExplorationChoice::ScoutRoute));
            assert(app.screen() == jf::Screen::PreBattleDeployment);
            assert(app.deploymentPlayers().size() == 4);
            assert(app.deploymentEnemyPreview().size() == 3);
            assert(app.deploymentMaxColumn() == 2);
        }
        {
            jf::GameApp app(scoutData);
            app.startExpedition();
            assert(app.chooseCinderwatchRoute(jf::ExplorationChoice::CollapsedSidePath));
            assert(app.screen() == jf::Screen::Battle);
        }
    }

    {
        // Placement rules: reject out-of-zone columns, impassable terrain, and
        // duplicate tiles; battle only starts once all 4 units are placed, and
        // the chosen coordinates become the actual battle-start positions.
        jf::GameData scoutData = makeScoutFactoryData();
        jf::GameApp app(scoutData);
        app.startExpedition();
        assert(app.chooseCinderwatchRoute(jf::ExplorationChoice::ScoutRoute));

        assert(!app.placeDeploymentUnit(0, {0, 3})); // outside left-3-column zone
        assert(!app.placeDeploymentUnit(0, {-1, 0})); // out of bounds

        const auto& terrain = app.deploymentTerrain();
        jf::GridPos impassable{-1, -1};
        for (int row = 0; row < jf::kGridRows && impassable.row < 0; ++row) {
            for (int col = 0; col <= app.deploymentMaxColumn(); ++col) {
                if (!jf::isPassable(terrain[row * jf::kGridCols + col])) {
                    impassable = {row, col};
                    break;
                }
            }
        }
        if (impassable.row >= 0) assert(!app.placeDeploymentUnit(0, impassable));

        assert(app.placeDeploymentUnit(0, {0, 0}));
        assert(app.isDeploymentUnitPlaced(0));
        assert(!app.placeDeploymentUnit(1, {0, 0})); // tile already taken
        assert(!app.confirmDeployment()); // not all 4 placed yet

        assert(app.placeDeploymentUnit(1, {1, 0}));
        assert(app.placeDeploymentUnit(2, {1, 1}));
        assert(app.placeDeploymentUnit(3, {2, 1}));
        assert(app.allDeploymentUnitsPlaced());

        // Re-placing a unit onto its own current tile must still succeed.
        assert(app.placeDeploymentUnit(0, {0, 0}));

        assert(app.confirmDeployment());
        assert(app.screen() == jf::Screen::Battle);
        int playerCount = 0;
        assert((app.battle().battle().findUnit("player0")->position == jf::GridPos{0, 0}));
        assert((app.battle().battle().findUnit("player1")->position == jf::GridPos{1, 0}));
        assert((app.battle().battle().findUnit("player2")->position == jf::GridPos{1, 1}));
        assert((app.battle().battle().findUnit("player3")->position == jf::GridPos{2, 1}));
        for (const jf::Unit& unit : app.battle().battle().units())
            if (unit.team == jf::Team::Player) ++playerCount;
        assert(playerCount == 4);
    }

    {
        // Back returns to Exploration and discards the in-progress placement.
        jf::GameData scoutData = makeScoutFactoryData();
        jf::GameApp app(scoutData);
        app.startExpedition();
        assert(app.chooseCinderwatchRoute(jf::ExplorationChoice::ScoutRoute));
        assert(app.placeDeploymentUnit(0, {0, 0}));
        app.cancelDeployment();
        assert(app.screen() == jf::Screen::Exploration);
        assert(app.deploymentPlayers().empty());
    }

    {
        // Full expedition: base development vertical slice (docs/base_development.md).
        // Each of the 3 mission stages should unlock its facility discovery
        // on safe return, stage 2's victory loot includes the region key
        // material, and that material makes the outpost eligible to advance.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(app.startExpedition());
        assert(!jf::eligibleForOutpostStage(app.baseState(), jf::OutpostStage::PioneerOutpost));

        assert(app.chooseCinderwatchRoute(jf::ExplorationChoice::FrontalAdvance)); // stage 0 -> Battle
        for (int stage = 0; stage < 3; ++stage) {
            assert(app.screen() == jf::Screen::Battle);
            winCurrentBattle(app);
            assert(app.battle().inputState() == jf::BattleInputState::Victory);
            app.proceedToCamp();
            assert(app.screen() == jf::Screen::Camp);
            if (stage < 2) app.continueExpedition(); // -> Battle for the next stage
        }
        app.returnToBase();
        assert(app.screen() == jf::Screen::Base);

        const jf::BaseState& base = app.baseState();
        assert(base.discoveryRegistry.count(jf::kCinderwatchReconDiscovery) == 1);
        assert(base.discoveryRegistry.count(jf::kFieldMedicineDiscovery) == 1);
        assert(base.discoveryRegistry.count(jf::kReturnSignalDiscovery) == 1);
        assert(base.storageCount(jf::kAshveilFangMaterial) == 1);
        assert(base.outpostStage == jf::OutpostStage::Encampment);
        assert(jf::eligibleForOutpostStage(base, jf::OutpostStage::PioneerOutpost));

        assert(app.advanceOutpostStage());
        assert(app.baseState().outpostStage == jf::OutpostStage::PioneerOutpost);
        assert(!app.advanceOutpostStage()); // already past Encampment
    }

    {
        // Without the key material, the outpost cannot advance.
        jf::BaseState fresh;
        assert(!jf::eligibleForOutpostStage(fresh, jf::OutpostStage::PioneerOutpost));
        fresh.storage.push_back({jf::kAshveilFangMaterial, 1});
        assert(jf::eligibleForOutpostStage(fresh, jf::OutpostStage::PioneerOutpost));
    }

    std::cout << "Battle tests PASSED\n";
    return 0;
}
