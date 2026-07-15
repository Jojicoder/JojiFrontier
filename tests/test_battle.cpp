#include <algorithm>
#include <cassert>

#ifdef NDEBUG
#error "jf_battle_tests requires assertions; NDEBUG must not be defined"
#endif
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include "jf/battle/BattleController.hpp"
#include "jf/battle/BattleFactory.hpp"
#include "jf/battle/BattleObjectResolver.hpp"
#include "jf/battle/CombatResolver.hpp"
#include "jf/battle/EnemyAI.hpp"
#include "jf/battle/Movement.hpp"
#include "jf/battle/SkillCharges.hpp"
#include "jf/battle/StatusEffects.hpp"
#include "jf/core/BaseState.hpp"
#include "jf/core/Exploration.hpp"
#include "jf/core/GameApp.hpp"
#include "jf/core/Skill.hpp"

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
    static const auto terrainProfiles = [] {
        auto loaded = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(loaded);
        return loaded->terrainProfilesById;
    }();
    data.terrainProfilesById = terrainProfiles;
    jf::Stats stats{.maxHp = 20, .strength = 6, .magic = 0, .speed = 5,
                    .defense = 2, .resistance = 1, .move = 4};
    jf::Weapon sword{.id = "sword", .name = "Sword", .might = 5, .minRange = 1,
                     .maxRange = 1, .damageType = jf::DamageType::Physical};
    data.weaponsById.emplace("sword", sword);
    jf::Weapon ironSpear{.id = "iron_spear", .name = "Iron Spear", .might = 6, .minRange = 1,
                         .maxRange = 2, .damageType = jf::DamageType::Physical};
    jf::Weapon heavySpear{.id = "heavy_spear", .name = "Heavy Spear", .might = 8, .minRange = 1, .maxRange = 2,
                          .damageType = jf::DamageType::Physical, .moveModifier = -1, .causesKnockback = true};
    data.weaponsById.emplace("iron_spear", ironSpear);
    data.weaponsById.emplace("heavy_spear", heavySpear);
    data.classesById.emplace(jf::UnitClass::MarchCaptain,
                             jf::ClassDefinition{jf::UnitClass::MarchCaptain, stats, "sword"});
    data.classesById.emplace(jf::UnitClass::FrontierScout,
                             jf::ClassDefinition{jf::UnitClass::FrontierScout, stats, "sword"});
    data.classesById.emplace(jf::UnitClass::Spearman,
                             jf::ClassDefinition{jf::UnitClass::Spearman, stats, "iron_spear"});
    jf::Weapon dawnStaff{.id = "dawn_staff", .name = "Dawn Staff", .might = 3, .minRange = 1,
                        .maxRange = 2, .damageType = jf::DamageType::Magical};
    data.weaponsById.emplace("dawn_staff", dawnStaff);
    data.classesById.emplace(jf::UnitClass::DawnChirurgeon,
                             jf::ClassDefinition{jf::UnitClass::DawnChirurgeon, stats, "dawn_staff"});
    jf::Weapon wolfBite{.id = "wolf_bite", .name = "Bite", .might = 5, .minRange = 1,
                       .maxRange = 1, .damageType = jf::DamageType::Physical};
    data.weaponsById.emplace("wolf_bite", wolfBite);
    jf::Stats wolfStats{.maxHp = 16, .strength = 6, .magic = 0, .speed = 7,
                       .defense = 2, .resistance = 1, .move = 5};
    data.classesById.emplace(jf::UnitClass::Wolf, jf::ClassDefinition{jf::UnitClass::Wolf, wolfStats, "wolf_bite"});
    jf::Weapon boarTusks{.id = "boar_tusks", .name = "Tusks", .might = 0, .minRange = 1,
                        .maxRange = 1, .damageType = jf::DamageType::Physical};
    data.weaponsById.emplace("boar_tusks", boarTusks);
    jf::Stats boarStats{.maxHp = 56, .strength = 9, .magic = 0, .speed = 3,
                       .defense = 5, .resistance = 1, .move = 2};
    data.classesById.emplace(jf::UnitClass::AshenhornBoar,
                             jf::ClassDefinition{jf::UnitClass::AshenhornBoar, boarStats, "boar_tusks"});
    for (int i = 0; i < 4; ++i)
        data.playerParty.push_back({"player" + std::to_string(i), "Player", jf::UnitClass::MarchCaptain});
    for (int i = 0; i < 4; ++i)
        data.enemyRoster.push_back({"enemy" + std::to_string(i), "Enemy", jf::UnitClass::MarchCaptain});
    return data;
}

// Mirrors Cinderwatch stage 0's real StageDescriptor (regionDescriptor()
// in src/core/Region.cpp) closely enough for battle-generation tests: only
// 3 of the 4-unit roster spawn, matching the shipped game's behavior.
jf::StageDescriptor testStage0(std::string terrainProfileId = jf::kCinderwatchOutpostTerrain) {
    jf::StageDescriptor stage;
    stage.terrainProfileId = std::move(terrainProfileId);
    stage.enemyCountOverride = 3;
    return stage;
}

jf::GameData makeScoutFactoryData() {
    jf::GameData data = makeFactoryData();
    data.playerParty[0].classId = jf::UnitClass::FrontierScout;
    return data;
}

jf::GameData makeChirurgeonFactoryData() {
    jf::GameData data = makeFactoryData();
    data.playerParty[0].classId = jf::UnitClass::DawnChirurgeon;
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

// docs/region_unlocks.md: Cinderwatch Gate (沈黙した監視所群, 第2地域) is locked
// until Ashbough Forest (第1地域)'s region-level completion is committed to
// BaseState::completedRegionIds - never inferred from SiteAccessState (a
// single cleared location must not stand in for the whole region). Most
// existing tests only care about Cinderwatch Gate's own content and predate
// this rule, so they short-circuit the unlock via applySaveData() the same
// way a real (future, Phase 4) region-complete safe return would.
bool startCinderwatchExpedition(jf::GameApp& app) {
    jf::SaveData save = app.createSaveData("en");
    save.base.completedRegionIds.insert(jf::RegionId::AshboughForest);
    if (!app.applySaveData(save)) return false;
    return app.startExpedition(jf::RegionId::CinderwatchGate);
}

// docs/regions/ashbough_forest.md "2. 薬草の沢": wins Ashbough Verge (the
// route graph's first site) and continues to Herbwater Hollow's Exploration
// screen, the shared setup every Herbwater Hollow test needs.
void reachHerbwaterHollow(jf::GameApp& app) {
    assert(app.startExpedition(jf::RegionId::AshboughForest));
    assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
    winCurrentBattle(app);
    app.proceedToCamp();
    app.continueExpedition();
    assert(app.screen() == jf::Screen::Exploration);
    assert(app.currentMissionNameJa() == "薬草の沢");
}

// docs/regions/ashbough_forest.md "3. 折れ木の縄張り": wins Ashbough Verge and
// Herbwater Hollow, then continues (skipping the ashbough_camp Route Graph
// node automatically) to Brokenwood Territory's Exploration screen.
void reachBrokenwoodTerritory(jf::GameApp& app) {
    reachHerbwaterHollow(app);
    assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
    winCurrentBattle(app);
    app.proceedToCamp();
    app.continueExpedition();
    assert(app.screen() == jf::Screen::Exploration);
    assert(app.currentMissionNameJa() == "折れ木の縄張り");
}

} // namespace

int main() {
    {
        assert(jf::healingAmount(jf::ItemType::FirstAidKit) == 20);
        assert(jf::healingAmount(jf::ItemType::FieldTreatmentKit) == 10);
    }

    {
        const auto a = jf::cinderwatchOutcome(jf::ExplorationChoice::FrontalAdvance);
        const auto b = jf::cinderwatchOutcome(jf::ExplorationChoice::CollapsedSidePath);
        assert(a.partyDamage == 0 && a.enemiesRemoved == 0);
        assert(b.partyDamage == 2 && b.enemiesRemoved == 1);

        const jf::GameData data = makeFactoryData();
        jf::BattleState standard = jf::createScenarioBattle(data, testStage0(), 42, a);
        jf::BattleState sidePath = jf::createScenarioBattle(data, testStage0(), 42, b);
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

        jf::BattleState clamped =
            jf::createScenarioBattle(data, testStage0(), 42, {.partyDamage = 999, .enemiesRemoved = 99});
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
        // Every one of the 24 tiles, including both 3x3 deployment zones,
        // participates in terrain generation. Only actual occupied spawn
        // tiles are opened later by createScenarioBattle().
        bool sawGeneratedEdgeTerrain = false;
        const jf::GameData profileData = makeFactoryData();
        for (const std::string& profileId : {std::string(jf::kCinderwatchOutpostTerrain),
                                             std::string(jf::kAshRoadTerrain),
                                             std::string(jf::kSignalTowerTerrain)}) {
            for (std::uint32_t seed = 0; seed < 100; ++seed) {
                const auto& profile = profileData.terrainProfile(profileId);
                const auto terrain = jf::generateFieldTerrain(profile, seed);
                assert(terrain == jf::generateFieldTerrain(profile, seed));
                // Cinderwatch is a constructed and ruined frontier route.
                // Brush is reserved for wilderness regions such as Ashbough.
                for (jf::TerrainType tile : terrain) assert(tile != jf::TerrainType::Brush);
                for (int row = 0; row < jf::kGridRows; ++row) {
                    for (int col : {0, 1, 2, 5, 6, 7}) {
                        sawGeneratedEdgeTerrain |= terrain[row * jf::kGridCols + col] != jf::TerrainType::Floor;
                    }
                }

                for (int col = 0; col < jf::kGridCols; ++col) {
                    int barriers = 0;
                    for (int row = 0; row < jf::kGridRows; ++row) {
                        if (terrain[row * jf::kGridCols + col] == jf::TerrainType::Barrier) ++barriers;
                    }
                    assert(barriers <= 1);
                }

                jf::Unit explorer = makeUnit("explorer", jf::Team::Player, {1, 0}, 99);
                jf::BattleState battle({explorer}, terrain);
                const auto reachable = jf::computeReachableTiles(battle, battle.units().front());
                bool reachesRightEdge = false;
                for (jf::GridPos pos : reachable) reachesRightEdge |= pos.col == jf::kGridCols - 1;
                assert(reachesRightEdge);
            }
        }
        assert(sawGeneratedEdgeTerrain);

        for (std::uint32_t seed = 0; seed < 100; ++seed) {
            const auto& profile = profileData.terrainProfile(jf::kAshboughVergeTerrain);
            const auto terrain = jf::generateFieldTerrain(profile, seed);
            assert(terrain == jf::generateFieldTerrain(profile, seed));
            int brushTiles = 0;
            for (jf::TerrainType tile : terrain) {
                if (tile == jf::TerrainType::Brush) ++brushTiles;
            }
            assert(brushTiles >= 2);
            assert(brushTiles <= 4);
        }

        // A new map composition is data, not a new FieldType/switch branch.
        jf::TerrainProfile customProfile;
        customProfile.id = "test_mixed_ground";
        customProfile.seedSalt = 99;
        customProfile.weights = {{jf::TerrainType::Shallows, 50}, {jf::TerrainType::Floor, 50}};
        customProfile.signatureTerrain = jf::TerrainType::Shallows;
        std::string validationError;
        assert(jf::validateTerrainProfile(customProfile, &validationError));
        const auto customTerrain = jf::generateFieldTerrain(customProfile, 77);
        assert(std::find(customTerrain.begin(), customTerrain.end(), jf::TerrainType::Shallows) !=
               customTerrain.end());

        customProfile.weights.front().weight = 49;
        assert(!jf::validateTerrainProfile(customProfile, &validationError));
    }

    {
        // Player/enemy starting positions are randomized within each side's
        // edge 3x3 zone rather than a fixed formation: verify they land in
        // the right zone, never overlap a teammate, actually vary across
        // seeds, and that the pre-battle enemy preview matches the real
        // battle's spawns for that same seed.
        const jf::GameData data = makeFactoryData();
        auto inLeftZone = [](jf::GridPos p) { return p.col >= 0 && p.col <= 2; };
        auto inRightZone = [](jf::GridPos p) { return p.col >= 5 && p.col <= 7; };

        std::vector<jf::GridPos> firstPlayerPositions;
        bool sawDifferentArrangement = false;
        bool sawBlockedSpawnOpened = false;
        for (std::uint32_t seed = 0; seed < 200; ++seed) {
            jf::BattleState battle = jf::createScenarioBattle(data, testStage0(), seed);
            const auto rawTerrain = jf::generateFieldTerrain(
                data.terrainProfile(jf::kCinderwatchOutpostTerrain), seed);
            std::vector<jf::GridPos> playerPositions;
            std::vector<jf::GridPos> enemyPositions;
            for (const jf::Unit& unit : battle.units()) {
                assert(jf::isPassable(battle.terrainAt(unit.position)));
                const int key = unit.position.row * jf::kGridCols + unit.position.col;
                if (!jf::isPassable(rawTerrain[key])) {
                    sawBlockedSpawnOpened = true;
                    assert(battle.terrainAt(unit.position) == jf::TerrainType::Floor);
                }
                if (unit.team == jf::Team::Player) {
                    assert(inLeftZone(unit.position));
                    playerPositions.push_back(unit.position);
                } else {
                    assert(inRightZone(unit.position));
                    enemyPositions.push_back(unit.position);
                }
            }
            for (std::size_t i = 0; i < playerPositions.size(); ++i)
                for (std::size_t j = i + 1; j < playerPositions.size(); ++j)
                    assert(!(playerPositions[i] == playerPositions[j]));
            for (std::size_t i = 0; i < enemyPositions.size(); ++i)
                for (std::size_t j = i + 1; j < enemyPositions.size(); ++j)
                    assert(!(enemyPositions[i] == enemyPositions[j]));

            if (seed == 0) firstPlayerPositions = playerPositions;
            else if (playerPositions != firstPlayerPositions) sawDifferentArrangement = true;

            auto preview = jf::previewEnemies(data, testStage0(), seed);
            assert(preview.size() == enemyPositions.size());
            for (std::size_t i = 0; i < preview.size(); ++i)
                assert(preview[i].position == enemyPositions[i]);
        }
        assert(sawDifferentArrangement); // confirms genuine randomization, not a lucky fixed layout
        assert(sawBlockedSpawnOpened); // an actual generated barrier was cleared only under its unit
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
        assert(jf::movementCost(jf::TerrainType::Brush) == 1);
        assert(jf::movementCost(jf::TerrainType::HerbPatch) == 1);
        assert(jf::evasionBonus(jf::TerrainType::Brush) == 20);
        assert(jf::evasionBonus(jf::TerrainType::HerbPatch) == 0);

        jf::Unit attacker = makeUnit("attacker", jf::Team::Player, {1, 0});
        jf::Unit defender = makeUnit("defender", jf::Team::Enemy, {1, 1});
        jf::BattleState first({attacker, defender}, {}, 12345);
        jf::BattleState second({attacker, defender}, {}, 12345);
        assert(first.combatHitChance(first.units()[1]) == 100);
        first.setTerrain({1, 1}, jf::TerrainType::Brush);
        second.setTerrain({1, 1}, jf::TerrainType::Brush);
        assert(first.combatHitChance(first.units()[1]) == 80);
        assert(jf::previewAttack(first.units()[0], first.units()[1], 0,
                                 first.combatHitChance(first.units()[1])).hitChance == 80);
        for (int i = 0; i < 20; ++i)
            assert(first.rollAttackHit(first.units()[1]) == second.rollAttackHit(second.units()[1]));

        const int hpBefore = first.units()[1].currentHp;
        jf::resolveAttack(first.units()[0], first.units()[1], 0, false);
        assert(first.units()[1].currentHp == hpBefore);

        defender.unitClass = jf::UnitClass::FrontierScout;
        jf::BattleState scoutBattle({attacker, defender}, {}, 12345);
        assert(scoutBattle.combatHitChance(scoutBattle.units()[1]) == 90);
        scoutBattle.setTerrain({1, 1}, jf::TerrainType::Brush);
        assert(scoutBattle.combatHitChance(scoutBattle.units()[1]) == 70);
    }

    {
        jf::Unit player = makeUnit("player", jf::Team::Player, {1, 0});
        player.currentHp = 7;
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 7});
        jf::BattleController controller(jf::BattleState({player, enemy}));
        controller.battle().setTerrain({1, 0}, jf::TerrainType::HerbPatch);
        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile({1, 0});
        controller.chooseWait();
        assert(controller.battle().units()[0].currentHp == 12);
        assert(controller.battle().terrainAt({1, 0}) == jf::TerrainType::Floor);
        assert(controller.battle().collectedHerbPatches() == 1);
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
        // docs/initial_skill_effects.md 古参守備兵`extended_lockdown`(封鎖
        // 強化): extends this ZoC-having unit's own range from 1 to 2 via
        // Unit::zocRangeExtended, consulted directly by
        // isStoppedByZoneOfControl() in Movement.cpp.
        jf::Unit mover = makeUnit("mover", jf::Team::Player, {1, 0}, 6);
        jf::Unit guard = makeUnit("guard", jf::Team::Enemy, {0, 3}, 3, jf::UnitClass::VeteranGuard);
        guard.zocRangeExtended = true;
        jf::BattleState battle({mover, guard});
        for (int col = 0; col < jf::kGridCols; ++col) battle.setTerrain({2, col}, jf::TerrainType::Barrier);
        const auto reachable = jf::computeReachableTiles(battle, battle.units().front());
        assert(contains(reachable, {1, 2})); // distance 2 from guard: entering still allowed
        assert(!contains(reachable, {1, 3})); // but movement stops there now, one tile earlier than before
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
        jf::Unit wounded = makeUnit("wounded", jf::Team::Player, {1, 0});
        wounded.currentHp = 5;
        jf::Unit healthy = makeUnit("healthy", jf::Team::Player, {2, 0});
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 7});
        jf::BattleController controller(jf::BattleState({wounded, healthy, enemy}));
        assert(controller.chooseHealingItemTarget(8));
        assert(controller.inputState() == jf::BattleInputState::SelectItemTarget);
        assert(controller.itemTargetTiles().size() == 1);
        assert(controller.selectHealingItemTarget({1, 0}));
        assert(controller.battle().units()[0].currentHp == 13);
        assert(controller.battle().units()[0].hasActed);
        assert(controller.inputState() == jf::BattleInputState::SelectUnit);
    }

    {
        jf::Unit first = makeUnit("first", jf::Team::Player, {1, 0});
        jf::Unit second = makeUnit("second", jf::Team::Player, {2, 0});
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 7});
        jf::BattleController controller(jf::BattleState({first, second, enemy}));
        controller.endPlayerTurn();
        assert(controller.battle().units()[0].hasActed);
        assert(controller.battle().units()[1].hasActed);
        assert(controller.inputState() == jf::BattleInputState::EnemyTurn);
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
        startCinderwatchExpedition(app);
        assert(!app.partyHasFrontierScout());
        assert(!app.chooseExplorationRoute(jf::ExplorationChoice::ScoutRoute));
        assert(app.screen() == jf::Screen::Exploration);
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        assert(app.screen() == jf::Screen::Battle);
    }

    {
        // With a Frontier Scout in the party, option C transitions to
        // PreBattleDeployment instead of straight to Battle, and A/B still work.
        jf::GameData scoutData = makeScoutFactoryData();
        {
            jf::GameApp app(scoutData);
            startCinderwatchExpedition(app);
            assert(app.partyHasFrontierScout());
            assert(app.chooseExplorationRoute(jf::ExplorationChoice::ScoutRoute));
            assert(app.screen() == jf::Screen::PreBattleDeployment);
            assert(app.deploymentPlayers().size() == 4);
            assert(app.deploymentEnemyPreview().size() == 3);
            assert(app.deploymentMaxColumn() == 2);
        }
        {
            jf::GameApp app(scoutData);
            startCinderwatchExpedition(app);
            assert(app.chooseExplorationRoute(jf::ExplorationChoice::CollapsedSidePath));
            assert(app.screen() == jf::Screen::Battle);
        }
    }

    {
        // Placement rules: reject out-of-zone columns and duplicate tiles, but
        // allow generated impassable terrain; battle only starts once all 4 units are placed, and
        // the chosen coordinates become the actual battle-start positions.
        jf::GameData scoutData = makeScoutFactoryData();
        jf::GameApp app(scoutData);
        startCinderwatchExpedition(app);
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::ScoutRoute));

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
        if (impassable.row >= 0) assert(app.placeDeploymentUnit(0, impassable));

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
        startCinderwatchExpedition(app);
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::ScoutRoute));
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
        assert(startCinderwatchExpedition(app));
        assert(!jf::eligibleForOutpostStage(app.baseState(), jf::OutpostStage::PioneerOutpost));

        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)); // stage 0 -> Battle
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

    {
        // BaseState storage add/consume helpers.
        jf::BaseState state;
        state.addStorage("x", 2);
        assert(state.storageCount("x") == 2);
        assert(state.consumeStorage("x", 1));
        assert(state.storageCount("x") == 1);
        assert(!state.consumeStorage("x", 5));
        assert(state.consumeStorage("x", 1));
        assert(state.storageCount("x") == 0); // fully consumed entries are erased
    }

    {
        // Facility node eligibility ladder: stage -> discovery -> material.
        jf::BaseState fresh;
        assert(fresh.unlockedNodeIds.count("operations_tent") == 1);
        assert(fresh.unlockedNodeIds.count("communal_tent") == 1);
        const jf::FacilityNode* scoutNode = jf::findFacilityNode("scout_network");
        assert(scoutNode != nullptr);
        assert(!jf::facilityNodeEligible(fresh, *scoutNode)); // wrong stage
        fresh.outpostStage = jf::OutpostStage::PioneerOutpost;
        assert(!jf::facilityNodeEligible(fresh, *scoutNode)); // missing discovery + material
        fresh.discoveryRegistry.insert(jf::kCinderwatchReconDiscovery);
        assert(!jf::facilityNodeEligible(fresh, *scoutNode)); // still missing material
        fresh.addStorage("watch_ledger", 1);
        assert(jf::facilityNodeEligible(fresh, *scoutNode));
    }

    {
        // Full GameApp flow: earn materials via 3 real victories, advance the
        // outpost, then unlock/dismantle/rebuild facility nodes and confirm
        // branch nodes require their facility to be actively BUILT (not just
        // historically unlocked), and that material scarcity gates them too.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(startCinderwatchExpedition(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        for (int stage = 0; stage < 3; ++stage) {
            winCurrentBattle(app);
            app.proceedToCamp();
            if (stage < 2) app.continueExpedition();
        }
        app.returnToBase();
        assert(app.advanceOutpostStage()); // -> PioneerOutpost, 2 facility slots open
        // 3 real victories bank wood:10, hide:5 (docs/base_development.md:
        // exactly covers all 4 optional facilities + one tuning trait).
        assert(app.baseState().storageCount("wood") == 10);
        assert(app.baseState().storageCount("hide") == 5);

        jf::BaseState& testBase = const_cast<jf::BaseState&>(app.baseState());
        testBase.addStorage("wood", 5);
        testBase.addStorage("hide", 4);
        assert(!app.baseState().unlockedNodeIds.count("training_field"));
        assert(app.unlockFacilityNode("training_field")); // costs wood:3 + hide:2
        assert(app.baseState().unlockedNodeIds.count("training_field") == 1);
        assert(app.baseState().builtNodeIds.count("training_field") == 1);
        assert(app.baseState().storageCount("wood") == 12); // 10 + 5 - 3
        assert(app.baseState().storageCount("hide") == 7);  // 5 + 4 - 2

        assert(app.unlockFacilityNode("vanguard_training")); // branch: no cost, just needs the facility built
        assert(app.baseState().unlockedNodeIds.count("vanguard_training") == 1);

        assert(app.dismantleFacilityNode("training_field"));
        assert(app.baseState().unlockedNodeIds.count("training_field") == 1); // unlock record survives
        assert(!app.baseState().builtNodeIds.count("training_field"));        // but it's not active
        assert(app.baseState().storageCount("wood") == 13); // +floor(3 * 50%) == +1
        assert(app.baseState().storageCount("hide") == 8);  // +floor(2 * 50%) == +1
        // A branch of a dismantled facility is no longer buildable until rebuilt.
        assert(!jf::facilityNodeEligible(app.baseState(), *jf::findFacilityNode("mobility_training")));

        assert(app.rebuildFacilityNode("training_field")); // free, just needs an open slot
        assert(app.baseState().builtNodeIds.count("training_field") == 1);
        assert(!app.rebuildFacilityNode("training_field")); // already built

        assert(app.unlockFacilityNode("simple_forge")); // costs wood:2 + hide:1
        assert(app.baseState().builtNodeIds.size() == 2);
        assert(!app.unlockFacilityNode("field_infirmary")); // no open slot left

        assert(app.unlockFacilityNode("weapon_forging")); // branch, no cost
        assert(jf::facilityNodeEligible(app.baseState(), *jf::findFacilityNode("craft_heavy_spear")));
        assert(app.unlockFacilityNode("craft_heavy_spear"));

        assert(app.unlockFacilityNode("trait_hide_wrapped_grip"));
    }

    {
        // Regression: the 4 optional stage-1 facilities must be buildable
        // using only materials/discoveries actually earned from 3 real
        // victories - not just by hand-injecting BaseState in a test. Every
        // material id and the herb-thicket discovery a facility needs has to
        // actually appear in production loot.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(startCinderwatchExpedition(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        for (int stage = 0; stage < 3; ++stage) {
            winCurrentBattle(app);
            app.proceedToCamp();
            if (stage < 2) app.continueExpedition();
        }
        app.returnToBase();
        assert(app.advanceOutpostStage());
        assert(app.baseState().discoveryRegistry.count(jf::kHerbThicketDiscovery) == 1);

        assert(app.unlockFacilityNode("training_field"));
        assert(app.unlockFacilityNode("simple_forge"));
        assert(app.baseState().builtNodeIds.size() == 2);
        assert(app.dismantleFacilityNode("training_field"));
        assert(app.dismantleFacilityNode("simple_forge"));

        assert(app.unlockFacilityNode("workshop_bench"));
        assert(app.unlockFacilityNode("field_infirmary")); // needs the herb-thicket discovery too
        assert(app.baseState().builtNodeIds.count("workshop_bench") == 1);
        assert(app.baseState().builtNodeIds.count("field_infirmary") == 1);
    }

    {
        // Multi-material shortfall must not partially consume storage: when
        // one of several required materials is missing, unlockFacilityNode
        // fails and leaves every material stack exactly as it was.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        jf::BaseState& testBase = const_cast<jf::BaseState&>(app.baseState());
        testBase.outpostStage = jf::OutpostStage::PioneerOutpost;
        testBase.addStorage("wood", 3); // training_field needs wood:3 + hide:2, but hide is missing entirely
        assert(!jf::facilityNodeEligible(app.baseState(), *jf::findFacilityNode("training_field")));
        assert(!app.unlockFacilityNode("training_field"));
        assert(app.baseState().storageCount("wood") == 3); // untouched, not partially spent
        assert(app.baseState().storageCount("hide") == 0);
        assert(!app.baseState().unlockedNodeIds.count("training_field"));
    }

    {
        // Forge equipment: weapon overrides validate against known weapons,
        // and change the effective weapon (incl. its move penalty) that a
        // freshly-built battle instantiates for that class.
        jf::GameData data = makeFactoryData();
        data.playerParty[0].classId = jf::UnitClass::Spearman;
        jf::GameApp app(data);
        assert(!app.equipWeaponForUnit("player0", "no_such_weapon"));
        jf::BaseState& testBase = const_cast<jf::BaseState&>(app.baseState());
        testBase.outpostStage = jf::OutpostStage::PioneerOutpost;
        testBase.unlockedNodeIds.insert("simple_forge");
        testBase.builtNodeIds.insert("simple_forge");
        assert(!app.equipWeaponForUnit("player0", "heavy_spear"));
        testBase.unlockedNodeIds.insert("craft_heavy_spear");
        assert(app.equipWeaponForUnit("player0", "heavy_spear"));
        assert(app.weaponOverrides().at("player0") == "heavy_spear");
        assert(app.equipWeaponForUnit("player0", "")); // revert to default
        assert(!app.weaponOverrides().count("player0"));

        testBase.builtNodeIds.erase("simple_forge");
        assert(!app.equipWeaponForUnit("player0", "iron_spear"));
        assert(!app.equipTuningTraitForUnit("player0", jf::TuningTraitId::HideWrappedGrip));

        jf::UnitTemplate spearTemplate{"spear_test", "Spear Test", jf::UnitClass::Spearman};
        jf::WeaponOverrides overrides{{"spear_test", "heavy_spear"}};
        jf::Unit heavy = jf::instantiateUnit(data, spearTemplate, jf::Team::Player, {0, 0}, &overrides);
        assert(heavy.weapon.id == "heavy_spear");
        assert(heavy.stats.move == data.classDefinition(jf::UnitClass::Spearman).baseStats.move - 1);

        jf::Unit base = jf::instantiateUnit(data, spearTemplate, jf::Team::Player, {0, 0});
        assert(base.weapon.id == "iron_spear");
        assert(base.stats.move == data.classDefinition(jf::UnitClass::Spearman).baseStats.move);
    }

    {
        // Heavy Spear knockback: pushes the defender one tile straight back.
        jf::Unit attacker = makeUnit("attacker", jf::Team::Player, {1, 2});
        attacker.weapon.causesKnockback = true;
        jf::Unit defender = makeUnit("defender", jf::Team::Enemy, {1, 3});
        jf::BattleState battle({attacker, defender});
        battle.applyKnockback(battle.units()[0], battle.units()[1]);
        assert((battle.units()[1].position == jf::GridPos{1, 4}));
    }

    {
        // Hide-Wrapped Grip: negates the first knockback instead of moving.
        jf::Unit attacker = makeUnit("attacker", jf::Team::Player, {1, 2});
        jf::Unit defender = makeUnit("defender", jf::Team::Enemy, {1, 3});
        defender.knockbackNegatesRemaining = 1;
        jf::BattleState battle({attacker, defender});
        battle.applyKnockback(battle.units()[0], battle.units()[1]);
        assert((battle.units()[1].position == jf::GridPos{1, 3})); // unmoved
        assert(battle.units()[1].knockbackNegatesRemaining == 0);
    }

    {
        // Knockback silently no-ops when the destination is occupied.
        jf::Unit attacker = makeUnit("attacker", jf::Team::Player, {1, 2});
        jf::Unit defender = makeUnit("defender", jf::Team::Enemy, {1, 3});
        jf::Unit blocker = makeUnit("blocker", jf::Team::Enemy, {1, 4});
        jf::BattleState battle({attacker, defender, blocker});
        battle.applyKnockback(battle.units()[0], battle.units()[1]);
        assert((battle.units()[1].position == jf::GridPos{1, 3}));
    }

    {
        // A diagonal range attack still pushes exactly one orthogonal tile.
        jf::Unit attacker = makeUnit("attacker", jf::Team::Player, {0, 0});
        jf::Unit defender = makeUnit("defender", jf::Team::Enemy, {1, 1});
        jf::BattleState battle({attacker, defender});
        battle.applyKnockback(battle.units()[0], battle.units()[1]);
        assert((battle.units()[1].position == jf::GridPos{1, 2}));
    }

    {
        // Guard Spear: Brace bonus strengthens from +2 to +3.
        jf::Unit defender = makeUnit("defender", jf::Team::Player, {1, 1}, 4, jf::UnitClass::Spearman);
        defender.weapon.braceBoost = true;
        jf::Unit attacker = makeUnit("attacker", jf::Team::Enemy, {1, 3});
        attacker.tilesMovedThisAction = 2;
        jf::BattleState battle({defender, attacker});
        assert(battle.combatDefenseBonus(battle.units()[0], battle.units()[1]) == 3);
    }

    {
        // Scout Network exploration preview: available before the node is
        // unlocked (UI gates its visibility on scoutNetworkUnlocked()), and
        // reflects the same stage-0 enemy count as the real battle.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(startCinderwatchExpedition(app));
        assert(!app.scoutNetworkUnlocked());
        assert(app.explorationEnemyPreview().size() == 3);
    }

    {
        // Permanent save JSON round-trip preserves base, facilities,
        // equipment, party, and language without including expedition state.
        jf::SaveData source;
        source.base.addStorage("wood", 7);
        source.base.discoveryRegistry.insert("discovery_test");
        source.base.outpostStage = jf::OutpostStage::PioneerOutpost;
        source.base.unlockedNodeIds.insert("simple_forge");
        source.base.unlockedNodeIds.insert("craft_heavy_spear");
        source.base.builtNodeIds.insert("simple_forge");
        source.base.completedRegionIds.insert(jf::RegionId::AshboughForest);
        source.selectedPartyIds = {"a", "b", "c", "d"};
        source.unitWeaponOverrides["rowan"] = "heavy_spear";
        source.language = "ja";
        std::string error;
        auto restored = jf::deserializeSave(jf::serializeSave(source), &error);
        assert(restored.has_value());
        assert(error.empty());
        assert(restored->base.storageCount("wood") == 7);
        assert(restored->base.builtNodeIds.count("simple_forge") == 1);
        assert(restored->unitWeaponOverrides.at("rowan") == "heavy_spear");
        assert(restored->language == "ja");
        assert(restored->base.completedRegionIds.count(jf::RegionId::AshboughForest) == 1);
        assert(restored->base.completedRegionIds.count(jf::RegionId::CinderwatchGate) == 0);
    }

    {
        // docs/region_unlocks.md "失敗と例外": an unrecognized region id must
        // fail deserialization outright rather than being silently dropped
        // or substituted - a lost completedRegions entry would wrongly
        // re-lock content the player already unlocked.
        const std::string corruptCompletedRegion = R"({
            "schemaVersion": 2, "gameVersion": "0.1.0",
            "base": {"storage": [], "discoveries": [], "outpostStage": 0,
                     "unlockedNodes": [], "builtNodes": [],
                     "completedRegions": ["some_future_region"]},
            "selectedPartyIds": [], "weaponOverrides": {}, "equippedTraits": {},
            "unitWeaponOverrides": {}, "unitEquippedTraits": {},
            "unitEquippedSkillsSlot0": {}, "unitEquippedSkillsSlot1": {},
            "settings": {"language": "en"}, "expedition": null
        })";
        std::string error;
        assert(!jf::deserializeSave(corruptCompletedRegion, &error).has_value());
        assert(!error.empty());

        // A corrupt regionId inside the (disposable) expedition checkpoint,
        // by contrast, only drops that checkpoint - the rest of the save
        // (BaseState etc.) still loads, matching how checkpoints are always
        // treated as regeneratable/resumable rather than as valuable as
        // permanent BaseState (docs/save_system.md "遠征中断セーブ").
        jf::SaveData source;
        source.base.addStorage("wood", 3);
        jf::ExpeditionCheckpoint checkpoint;
        checkpoint.regionId = jf::RegionId::CinderwatchGate;
        source.expedition = checkpoint;
        std::string validJson = jf::serializeSave(source);
        std::string corruptJson = validJson;
        std::size_t pos = corruptJson.find("\"cinderwatch_gate\"");
        assert(pos != std::string::npos);
        corruptJson.replace(pos, std::string("\"cinderwatch_gate\"").size(), "\"some_future_region\"");
        auto restoredWithDroppedCheckpoint = jf::deserializeSave(corruptJson, &error);
        assert(restoredWithDroppedCheckpoint.has_value());
        assert(restoredWithDroppedCheckpoint->base.storageCount("wood") == 3);
        assert(!restoredWithDroppedCheckpoint->expedition.has_value());
    }

    {
        // Expedition checkpoint (docs/save_system.md "遠征中断セーブ") JSON
        // round-trip: Camp-stage checkpoint carries party HP, bag, pending
        // loot/discoveries, and the stage-discovery guard flags.
        jf::SaveData source;
        jf::ExpeditionCheckpoint checkpoint;
        checkpoint.stage = jf::ExpeditionCheckpoint::Stage::Camp;
        checkpoint.regionId = jf::RegionId::AshboughForest;
        checkpoint.expeditionStage = 1;
        checkpoint.seed = 42;
        checkpoint.pendingLoot = {{"wood", 3}, {"hide", 1}};
        checkpoint.pendingDiscoveries = {"discovery_test"};
        checkpoint.bag = {jf::ItemType::FirstAidKit, jf::ItemType::CampRations};
        checkpoint.battlesWon = 2;
        checkpoint.stageDiscoveryAwarded = {true, false, false};
        checkpoint.partyUnits = {{"player0", 18}, {"player1", 0}};
        source.expedition = checkpoint;

        std::string error;
        auto restored = jf::deserializeSave(jf::serializeSave(source), &error);
        assert(restored.has_value());
        assert(error.empty());
        assert(restored->expedition.has_value());
        assert(restored->expedition->stage == jf::ExpeditionCheckpoint::Stage::Camp);
        assert(restored->expedition->regionId == jf::RegionId::AshboughForest);
        assert(restored->expedition->expeditionStage == 1);
        assert(restored->expedition->seed == 42);
        assert(restored->expedition->pendingLoot.size() == 2);
        assert(restored->expedition->pendingDiscoveries == std::vector<std::string>{"discovery_test"});
        assert((restored->expedition->bag ==
                std::vector<jf::ItemType>{jf::ItemType::FirstAidKit, jf::ItemType::CampRations}));
        assert(restored->expedition->battlesWon == 2);
        assert(restored->expedition->stageDiscoveryAwarded[0] && !restored->expedition->stageDiscoveryAwarded[1]);
        assert(restored->expedition->partyUnits[0].currentHp == 18);
        assert(restored->expedition->partyUnits[1].currentHp == 0);
    }

    {
        // Phase 3 site-access JSON round-trip: BaseState::siteAccess (the
        // permanent record) and ExpeditionCheckpoint::pendingSiteAccessUpdates
        // (the mid-expedition-in-progress record) both survive a save/load.
        jf::SaveData source;
        source.base.siteAccess["ashbough_forest:ashbough_verge"] = jf::SiteAccessState::Secured;
        source.base.siteAccess["cinderwatch_gate:cinderwatch_outpost"] = jf::SiteAccessState::Surveyed;
        jf::ExpeditionCheckpoint checkpoint;
        checkpoint.regionId = jf::RegionId::AshboughForest;
        checkpoint.pendingSiteAccessUpdates = {{"ashbough_forest:ashbough_verge", jf::SiteAccessState::Secured}};
        checkpoint.pendingRegionCompletions = {jf::RegionId::AshboughForest};
        source.expedition = checkpoint;
        source.base.completedRegionIds.insert(jf::RegionId::AshboughForest);

        std::string error;
        auto restored = jf::deserializeSave(jf::serializeSave(source), &error);
        assert(restored.has_value());
        assert(error.empty());
        assert(restored->base.siteAccess.at("ashbough_forest:ashbough_verge") == jf::SiteAccessState::Secured);
        assert(restored->base.siteAccess.at("cinderwatch_gate:cinderwatch_outpost") == jf::SiteAccessState::Surveyed);
        assert(restored->expedition.has_value());
        assert(restored->expedition->pendingSiteAccessUpdates.size() == 1);
        assert(restored->expedition->pendingSiteAccessUpdates[0].first == "ashbough_forest:ashbough_verge");
        assert(restored->expedition->pendingSiteAccessUpdates[0].second == jf::SiteAccessState::Secured);
        assert(restored->expedition->pendingRegionCompletions.count(jf::RegionId::AshboughForest) == 1);
        assert(restored->base.completedRegionIds.count(jf::RegionId::AshboughForest) == 1);
    }

    {
        // Migration: a save JSON written before this field existed (no
        // "siteAccess" key in "base", no "pendingSiteAccessUpdates" in
        // "expedition") must still load, defaulting every site to Unknown -
        // this is the exact shape produced by serializeSave() prior to this
        // session's Phase 3 work.
        const std::string legacyJson = R"({
            "schemaVersion": 2,
            "gameVersion": "0.1.0",
            "base": {
                "storage": [],
                "discoveries": [],
                "outpostStage": 0,
                "unlockedNodes": ["operations_tent", "communal_tent"],
                "builtNodes": []
            },
            "selectedPartyIds": [],
            "weaponOverrides": {},
            "equippedTraits": {},
            "unitWeaponOverrides": {},
            "unitEquippedTraits": {},
            "unitEquippedSkillsSlot0": {},
            "unitEquippedSkillsSlot1": {},
            "settings": {"language": "en"},
            "expedition": null
        })";
        std::string error;
        auto restored = jf::deserializeSave(legacyJson, &error);
        assert(restored.has_value());
        assert(error.empty());
        assert(restored->base.siteAccess.empty());
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(app.applySaveData(*restored));
        assert(app.startExpedition(jf::RegionId::AshboughForest));
        assert(app.currentSiteAccess() == jf::SiteAccessState::Unknown);
    }

    {
        // GameApp <-> SaveData round-trip for a live Camp checkpoint: winning
        // a battle should let a brand new GameApp resume at Camp with the
        // same surviving party HP and pending rewards, without replaying the
        // battle itself.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(startCinderwatchExpedition(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        assert(app.screen() == jf::Screen::Camp);
        const std::uint64_t revisionAfterCamp = app.expeditionRevision();
        assert(revisionAfterCamp > 0);

        jf::SaveData saved = app.createSaveData("en");
        assert(saved.expedition.has_value());
        assert(saved.expedition->stage == jf::ExpeditionCheckpoint::Stage::Camp);

        jf::GameApp restoredApp(data);
        assert(restoredApp.applySaveData(saved));
        assert(restoredApp.screen() == jf::Screen::Camp);
        assert(restoredApp.expedition().pendingLoot.size() == app.expedition().pendingLoot.size());

        int restoredPlayerCount = 0;
        for (const jf::Unit& unit : restoredApp.battle().battle().units())
            if (unit.team == jf::Team::Player) ++restoredPlayerCount;
        assert(restoredPlayerCount == 4);

        // Returning to base afterwards clears the checkpoint from the save.
        restoredApp.returnToBase();
        jf::SaveData clearedSave = restoredApp.createSaveData("en");
        assert(!clearedSave.expedition.has_value());
    }

    {
        // GameApp <-> SaveData round-trip for the equipped tuning trait: the
        // enum must survive the string boundary used by the JSON save file.
        jf::GameData data = makeFactoryData();
        data.playerParty[0].classId = jf::UnitClass::Spearman;
        jf::GameApp app(data);
        jf::BaseState& testBase = const_cast<jf::BaseState&>(app.baseState());
        testBase.outpostStage = jf::OutpostStage::PioneerOutpost;
        testBase.unlockedNodeIds.insert("simple_forge");
        testBase.builtNodeIds.insert("simple_forge");
        testBase.unlockedNodeIds.insert("trait_hide_wrapped_grip");
        assert(app.equipTuningTraitForUnit("player0", jf::TuningTraitId::HideWrappedGrip));
        assert(app.equippedTraits().at("player0") == jf::TuningTraitId::HideWrappedGrip);

        jf::SaveData saved = app.createSaveData("en");
        assert(saved.unitEquippedTraits.at("player0") == "hide_wrapped_grip");

        // saved.base already carries the "trait_hide_wrapped_grip" unlock
        // (it's a full copy of app's baseState_ made above), so applying it
        // to a fresh GameApp reproduces the same tech-tree state.
        jf::GameApp restoredApp(data);
        assert(restoredApp.applySaveData(saved));
        assert(restoredApp.equippedTraits().at("player0") == jf::TuningTraitId::HideWrappedGrip);
    }

    {
        std::string error;
        assert(!jf::deserializeSave(R"({"schemaVersion":999,"base":{}})", &error));
        assert(!error.empty());
        assert(!jf::deserializeSave(R"({"schemaVersion":1,"base":{"outpostStage":99}})", &error));
    }

    {
        // Schema 1 stored equipment by class. Loading it into Schema 2
        // migrates that loadout to each matching roster member's unit id.
        jf::GameData data = makeFactoryData();
        data.playerParty[0].classId = jf::UnitClass::Spearman;
        jf::SaveData legacy;
        legacy.schemaVersion = 1;
        legacy.base.outpostStage = jf::OutpostStage::PioneerOutpost;
        legacy.base.unlockedNodeIds.insert("simple_forge");
        legacy.base.unlockedNodeIds.insert("craft_heavy_spear");
        legacy.base.unlockedNodeIds.insert("trait_hide_wrapped_grip");
        legacy.base.builtNodeIds.insert("simple_forge");
        legacy.selectedPartyIds = {"player0", "player1", "player2", "player3"};
        legacy.weaponOverrides[jf::UnitClass::Spearman] = "heavy_spear";
        legacy.equippedTraits[jf::UnitClass::Spearman] = "hide_wrapped_grip";
        jf::GameApp app(data);
        assert(app.applySaveData(legacy));
        assert(app.weaponOverrides().at("player0") == "heavy_spear");
        assert(app.equippedTraits().at("player0") == jf::TuningTraitId::HideWrappedGrip);
    }

    {
        // Atomic file replacement keeps a previous backup and recovers it if
        // the current save is corrupt.
        const std::filesystem::path path = std::filesystem::temp_directory_path() / "joji_frontier_save_test.json";
        std::filesystem::remove(path);
        std::filesystem::remove(path.string() + ".bak");
        jf::SaveStore store(path.string());
        jf::SaveData first;
        first.base.addStorage("wood", 1);
        jf::SaveData second;
        second.base.addStorage("wood", 2);
        assert(store.save(first));
        assert(store.save(second));
        {
            std::ofstream corrupt(path, std::ios::trunc);
            corrupt << "not json";
        }
        std::string error;
        auto recovered = store.load(&error);
        assert(recovered.has_value());
        assert(error.empty());
        assert(recovered->base.storageCount("wood") == 1);
        std::filesystem::remove(path);
        std::filesystem::remove(path.string() + ".bak");
    }

    {
        // SaveStore::importFrom() keeps a distinct ".preimport.bak" of
        // whatever save existed before the import, separate from the
        // regular ".bak" safety copy that save() itself maintains.
        const std::filesystem::path path = std::filesystem::temp_directory_path() / "joji_frontier_import_test.json";
        std::filesystem::remove(path);
        std::filesystem::remove(path.string() + ".bak");
        std::filesystem::remove(path.string() + ".preimport.bak");
        jf::SaveStore store(path.string());
        jf::SaveData original;
        original.base.addStorage("wood", 1);
        assert(store.save(original));

        jf::SaveData imported;
        imported.base.addStorage("wood", 9);
        assert(store.importFrom(imported));

        auto loaded = store.load();
        assert(loaded.has_value());
        assert(loaded->base.storageCount("wood") == 9);
        std::ifstream preimportFile(path.string() + ".preimport.bak");
        std::ostringstream preimportContents;
        preimportContents << preimportFile.rdbuf();
        auto preimport = jf::deserializeSave(preimportContents.str());
        assert(preimport.has_value());
        assert(preimport->base.storageCount("wood") == 1);

        std::filesystem::remove(path);
        std::filesystem::remove(path.string() + ".bak");
        std::filesystem::remove(path.string() + ".preimport.bak");
    }

    {
        // Export writes a timestamped JSON file under exports/ (next to the
        // default save path) that round-trips through deserializeSave, and
        // listImportCandidates() finds files placed under imports/, newest
        // first.
        std::filesystem::remove_all("exports");
        std::filesystem::remove_all("imports");
        jf::SaveData toExport;
        toExport.base.addStorage("herb", 4);
        std::string exportError;
        std::string exportedPath = jf::exportSaveData(toExport, &exportError);
        assert(!exportedPath.empty());
        assert(exportError.empty());
        assert(std::filesystem::exists(exportedPath));

        std::filesystem::create_directories("imports");
        {
            std::ofstream candidate("imports/candidate.json", std::ios::trunc);
            candidate << jf::serializeSave(toExport);
        }
        auto candidates = jf::listImportCandidates();
        assert(!candidates.empty());
        assert(candidates.front().filename == "candidate.json");

        std::string importError;
        auto reloaded = jf::loadImportCandidate(candidates.front().path, &importError);
        assert(reloaded.has_value());
        assert(importError.empty());
        assert(reloaded->base.storageCount("herb") == 4);

        std::filesystem::remove_all("exports");
        std::filesystem::remove_all("imports");
    }

    {
        // BattleController::lastDamage()/lastAttackHit() must reflect the
        // real HP delta, not a duplicated/independent calculation - this is
        // what the battle-message UI (hit/miss/damage banners) relies on.
        jf::Unit player = makeUnit("player", jf::Team::Player, {1, 0});
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 1});
        jf::BattleController controller(jf::BattleState({player, enemy}));
        controller.selectUnit(controller.battle().units().front());
        controller.selectMoveTile({1, 1}); // attack shortcut: enemy tile is in range
        assert(controller.inputState() == jf::BattleInputState::ConfirmAttack);
        int hpBefore = controller.battle().units()[1].currentHp;
        controller.confirmAttack();
        int hpAfter = controller.battle().units()[1].currentHp;
        assert(controller.attackEventId() == 1);
        assert(controller.lastAttacker()->id == "player");
        assert(controller.lastAttackTarget()->id == "enemy");
        assert(controller.lastAttackHit()); // Floor terrain -> 100% hit chance, deterministic
        assert(controller.lastDamage() == hpBefore - hpAfter);
        assert(controller.lastDamage() > 0);
    }

    {
        // Miss path: Brush terrain grants 20% evasion (80% hit chance), so
        // some seeds miss. Brute-force one and confirm lastAttackHit()/
        // lastDamage() correctly report a whiffed attack (0 damage dealt).
        bool foundMiss = false;
        for (std::uint32_t seed = 0; seed < 200 && !foundMiss; ++seed) {
            jf::Unit player = makeUnit("player", jf::Team::Player, {1, 0});
            jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 1});
            jf::BattleState battle({player, enemy}, {}, seed);
            battle.setTerrain({1, 1}, jf::TerrainType::Brush);
            jf::BattleController controller(std::move(battle));
            controller.selectUnit(controller.battle().units().front());
            controller.selectMoveTile({1, 1});
            int hpBefore = controller.battle().units()[1].currentHp;
            controller.confirmAttack();
            if (!controller.lastAttackHit()) {
                foundMiss = true;
                assert(controller.lastDamage() == 0);
                assert(controller.battle().units()[1].currentHp == hpBefore);
            }
        }
        assert(foundMiss);
    }

    {
        // retireExpedition(): a no-op at Base, but from any other screen it
        // forfeits pending loot/discoveries and returns to Base - the same
        // forfeiture rule as a Defeat - without touching the party selection.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(!app.retireExpedition()); // no-op: already at Base

        std::vector<std::string> partyBefore = app.selectedPartyIds();
        assert(startCinderwatchExpedition(app));
        assert(app.screen() == jf::Screen::Exploration);
        assert(app.retireExpedition());
        assert(app.screen() == jf::Screen::Base);
        assert(app.selectedPartyIds() == partyBefore); // party selection survives a retire

        assert(startCinderwatchExpedition(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        assert(app.screen() == jf::Screen::Battle);
        winCurrentBattle(app);
        app.proceedToCamp(); // stage 0 loot/discovery now pending, not yet secured
        assert(app.screen() == jf::Screen::Camp);
        assert(app.retireExpedition());
        assert(app.screen() == jf::Screen::Base);
        assert(app.baseState().storage.empty()); // pending loot from the retired run was forfeited
        assert(app.baseState().discoveryRegistry.empty());
    }

    {
        // Status effects (docs/status_effects.md) - data foundation: apply
        // sets the documented count/flag, reapplying resets rather than
        // stacking, and boss scaling reduces magnitude/count without full
        // immunity.
        jf::Unit unit = makeUnit("poisoned", jf::Team::Player, {1, 0});
        jf::applyPoison(unit);
        assert(unit.poisonRemainingProcs == 3);
        unit.poisonRemainingProcs = 1;
        jf::applyPoison(unit); // reapply resets to full count, doesn't stack
        assert(unit.poisonRemainingProcs == 3);

        jf::Unit boss = makeUnit("boss", jf::Team::Enemy, {1, 5});
        boss.isBoss = true;
        jf::applyPoison(boss);
        assert(boss.poisonRemainingProcs == 2); // ボス補正: 3回ではなく2回

        jf::applyBurn(unit);
        assert(unit.burnRemainingProcs == 2);

        jf::applyMoveDown(unit);
        assert(unit.moveDownActive);
        assert(unit.effectiveMove() == unit.stats.move - 2);
        jf::Unit lowMove = makeUnit("lowmove", jf::Team::Player, {1, 0}, 1);
        jf::applyMoveDown(lowMove);
        assert(lowMove.effectiveMove() == 1); // 最低MOVは1

        jf::applyDefenseDown(unit);
        assert(unit.effectiveDefense() == std::max(unit.stats.defense - 3, 0));
        assert(unit.stats.resistance == 1); // RESは低下しない

        jf::Unit boss2 = makeUnit("boss2", jf::Team::Enemy, {1, 5});
        boss2.isBoss = true;
        jf::applyMoveDown(boss2);
        jf::applyDefenseDown(boss2);
        assert(boss2.effectiveMove() == boss2.stats.move - 1);
        assert(boss2.effectiveDefense() == boss2.stats.defense - 2);
    }

    {
        // よろめき: locks movement for a normal unit outright (not merely a
        // penalty), clears when the unit finishes its next action, and
        // cannot be reapplied while immune. A boss instead just takes MOV-1.
        jf::Unit unit = makeUnit("staggered", jf::Team::Player, {1, 0});
        jf::applyStagger(unit);
        assert(unit.staggerActive);
        assert(unit.effectiveMove() == 0);

        jf::BattleState battle({unit});
        jf::processActionEndStatusEffects(battle, battle.units()[0]);
        assert(!battle.units()[0].staggerActive);
        assert(battle.units()[0].staggerImmune);
        assert(battle.units()[0].effectiveMove() == battle.units()[0].stats.move);

        jf::applyStagger(battle.units()[0]); // no-op while immune
        assert(!battle.units()[0].staggerActive);

        jf::Unit boss = makeUnit("boss", jf::Team::Enemy, {1, 5});
        boss.isBoss = true;
        jf::applyStagger(boss);
        assert(boss.effectiveMove() == boss.stats.move - 1); // MOV-1, not a full lock
    }

    {
        // Phase-end pipeline: poison ticks (never below 1 HP) and expires
        // move-down/defense-down/stagger-immunity, but only for the team
        // whose phase is ending - the other team's statuses are untouched.
        jf::Unit player = makeUnit("player", jf::Team::Player, {1, 0});
        player.currentHp = 3;
        jf::applyPoison(player);
        player.moveDownActive = true;
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 5});
        enemy.moveDownActive = true;
        jf::BattleState battle({player, enemy});

        jf::processPhaseEndStatusEffects(battle, jf::Team::Player);
        assert(battle.units()[0].currentHp == 1); // 2 damage, floored at 1
        assert(battle.units()[0].poisonRemainingProcs == 2);
        assert(!battle.units()[0].moveDownActive);
        assert(battle.units()[1].moveDownActive); // enemy untouched by Player Phase end

        jf::processPhaseEndStatusEffects(battle, jf::Team::Player);
        assert(battle.units()[0].currentHp == 1); // still floored, never a defeat via poison
        assert(battle.units()[0].poisonRemainingProcs == 1);
        jf::processPhaseEndStatusEffects(battle, jf::Team::Player);
        assert(battle.units()[0].poisonRemainingProcs == 0); // 3 procs then cleared

        jf::processPhaseEndStatusEffects(battle, jf::Team::Enemy);
        assert(!battle.units()[1].moveDownActive);
    }

    {
        // clearAllStatusEffects(Unit&) is the 万能薬/状態治療 cure - clears
        // everything except the stagger-immunity cooldown, which is not a
        // status to cure. clearAllStatusEffects(BattleState&) (battle end)
        // clears every unit completely, immunity included.
        jf::Unit unit = makeUnit("cured", jf::Team::Player, {1, 0});
        jf::applyPoison(unit);
        jf::applyBurn(unit);
        jf::applyMoveDown(unit);
        jf::applyDefenseDown(unit);
        unit.staggerImmune = true;
        jf::clearAllStatusEffects(unit);
        assert(unit.poisonRemainingProcs == 0 && unit.burnRemainingProcs == 0);
        assert(!unit.moveDownActive && !unit.defenseDownActive && !unit.staggerActive);
        assert(unit.staggerImmune); // untouched by a cure

        jf::BattleState battle({unit});
        jf::clearAllStatusEffects(battle);
        assert(!battle.units()[0].staggerImmune); // battle end clears everything
    }

    {
        // docs/initial_skill_effects.md 暁の衛生兵`protective_treatment`'s
        // RES+3 buff: unlike moveDownActive/defenseDownActive (which expire
        // at the AFFECTED unit's own team's next phase end),
        // resistanceUpActive always expires at the next ENEMY Phase end
        // regardless of the buffed unit's team, so it needs its own clear
        // function rather than processPhaseEndStatusEffects().
        jf::Unit mage = makeUnit("mage", jf::Team::Player, {1, 0});
        mage.stats.resistance = 5;
        assert(mage.effectiveResistance() == 5);
        jf::applyResistanceUp(mage);
        assert(mage.effectiveResistance() == 8);

        jf::BattleState battle({mage});
        jf::processPhaseEndStatusEffects(battle, jf::Team::Player); // Player Phase end: untouched
        assert(battle.units()[0].effectiveResistance() == 8);
        jf::clearSkillBuffsAtEnemyPhaseEnd(battle); // Enemy Phase end: now clears
        assert(battle.units()[0].effectiveResistance() == 5);

        // computeDamage actually reads effectiveResistance() for magic hits.
        jf::Unit mageAttacker = makeUnit("mageAttacker", jf::Team::Enemy, {1, 1});
        mageAttacker.weapon = {.id = "staff", .name = "Staff", .might = 5, .minRange = 1, .maxRange = 2,
                              .damageType = jf::DamageType::Magical};
        mageAttacker.stats.magic = 6;
        jf::Unit target = makeUnit("target", jf::Team::Player, {1, 2});
        target.stats.resistance = 5;
        int normalDamage = jf::computeDamage(mageAttacker, target, 0);
        jf::applyResistanceUp(target);
        int buffedDamage = jf::computeDamage(mageAttacker, target, 0);
        assert(buffedDamage == normalDamage - 3);
    }

    {
        // effectiveDefense() composes hold_formation's DEF+2 buff and
        // defenseDownActive's debuff correctly when both are active, and
        // clearSkillBuffsAtEnemyPhaseEnd() clears the buff (but not the
        // separately-timed debuff, which clears via
        // processPhaseEndStatusEffects() on the affected unit's own team).
        jf::Unit unit = makeUnit("buffed", jf::Team::Player, {1, 0});
        unit.stats.defense = 5;
        assert(unit.effectiveDefense() == 5);
        jf::applyDefenseUp(unit);
        assert(unit.effectiveDefense() == 7);
        jf::applyDefenseDown(unit);
        assert(unit.effectiveDefense() == 4); // 5 + 2 - 3 (statusDefenseDownAmount)

        jf::BattleState battle({unit});
        jf::clearSkillBuffsAtEnemyPhaseEnd(battle);
        assert(!battle.units()[0].defenseUpActive);
        assert(battle.units()[0].defenseDownActive); // untouched by this clear
        assert(battle.units()[0].effectiveDefense() == 2); // 5 - 3, buff gone
    }

    {
        // docs/initial_skill_effects.md 監視弓兵`mark_target`(標的指定):
        // Unit::markedBonusDamage adds to computeDamage() (read-only, so
        // previewAttack() stays side-effect-free) and is consumed by
        // resolveAttack() the moment a real hit actually lands - a miss must
        // not consume it.
        jf::Unit attacker = makeUnit("attacker", jf::Team::Player, {1, 0});
        jf::Unit marked = makeUnit("marked", jf::Team::Enemy, {1, 1});
        marked.markedBonusDamage = 2;
        const int normalDamage = jf::computeDamage(attacker, marked, 0);
        marked.markedBonusDamage = 0;
        const int baseline = jf::computeDamage(attacker, marked, 0);
        assert(normalDamage == baseline + 2);

        marked.markedBonusDamage = 2;
        jf::resolveAttack(attacker, marked, 0, false); // miss: must not consume the mark
        assert(marked.markedBonusDamage == 2);
        jf::resolveAttack(attacker, marked, 0, true); // hit: consumes it
        assert(marked.markedBonusDamage == 0);
    }

    {
        // docs/initial_skill_effects.md 行軍隊長`advance_order`'s MOV+1 buff:
        // unlike every other skill buff so far, this one expires at THIS
        // Player Phase's own end (clearMoveUpAtPlayerPhaseEnd()), not the
        // next Enemy Phase end (clearSkillBuffsAtEnemyPhaseEnd()) - confirmed
        // independent of the other clear function.
        jf::Unit unit = makeUnit("mover", jf::Team::Player, {1, 0}, 4);
        assert(unit.effectiveMove() == 4);
        jf::applyMoveUp(unit);
        assert(unit.effectiveMove() == 5);

        jf::BattleState battle({unit});
        jf::clearSkillBuffsAtEnemyPhaseEnd(battle); // wrong clear function: untouched
        assert(battle.units()[0].effectiveMove() == 5);
        jf::clearMoveUpAtPlayerPhaseEnd(battle);
        assert(battle.units()[0].effectiveMove() == 4);
    }

    {
        // Integration: poison on a player unit only ticks at Player Phase
        // end (not immediately on apply, not from that unit's own action-end
        // while other party members still haven't acted), driven through the
        // real BattleController phase-transition path. Uses 2 player units
        // so "action ended" and "Player Phase ended" are observably distinct
        // moments (with only 1 unit, a single chooseWait() call does both
        // synchronously, leaving nothing to check in between).
        jf::Unit poisoned = makeUnit("poisoned", jf::Team::Player, {1, 0});
        jf::Unit other = makeUnit("other", jf::Team::Player, {1, 1});
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 7});
        jf::BattleController controller(jf::BattleState({poisoned, other, enemy}));
        jf::applyPoison(controller.battle().units()[0]);
        int hpBeforeEndTurn = controller.battle().units()[0].currentHp;

        // The other unit acts first - the team isn't done yet, so no phase
        // transition, and poison hasn't ticked.
        controller.selectUnit(controller.battle().units()[1]);
        controller.selectMoveTile(controller.battle().units()[1].position);
        controller.chooseWait();
        assert(controller.battle().units()[0].currentHp == hpBeforeEndTurn);
        assert(controller.inputState() == jf::BattleInputState::SelectUnit);

        // The poisoned unit acts too - now the team is done, Player Phase
        // ends, and poison ticks exactly then.
        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseWait();
        assert(controller.inputState() == jf::BattleInputState::EnemyTurn);
        assert(controller.battle().units()[0].currentHp == hpBeforeEndTurn - 2);
        assert(controller.battle().units()[0].poisonRemainingProcs == 2);
    }

    {
        // Skill data foundation (docs/skill_system.md): registry lookups and
        // the class-branch mapping used to gate equipping.
        const jf::SkillDefinition* holdFormation = jf::findSkill("hold_formation");
        assert(holdFormation && holdFormation->unitClass == jf::UnitClass::MarchCaptain);
        assert(holdFormation->category == jf::SkillCategory::Active);
        assert(holdFormation->usageType == jf::SkillUsageType::Cooldown2);
        assert(jf::findSkill("no_such_skill") == nullptr);
        assert(jf::skillsForClass(jf::UnitClass::Spearman).size() == 3);
        assert(jf::requiredTrainingNodeIdFor(jf::UnitClass::Spearman) == "vanguard_training");
        assert(jf::requiredTrainingNodeIdFor(jf::UnitClass::WatchArcher) == "mobility_training");
        assert(jf::requiredTrainingNodeIdFor(jf::UnitClass::DawnChirurgeon) == "specialist_training");
    }

    {
        // Skill charge/cooldown lifecycle (jf/battle/SkillCharges.hpp): fresh
        // battle = full charges; Cooldown2 needs 2 of the user's own Phase
        // starts to recharge; OncePerBattle never refills mid-battle; Always
        // is never consumed.
        jf::Unit unit = makeUnit("skilled", jf::Team::Player, {1, 0});
        unit.skillSlots[0].skillId = "hold_formation";  // Active, Cooldown2
        unit.skillSlots[1].skillId = "advance_order";   // Active, OncePerBattle
        jf::initializeSkillCharges(unit);
        assert(jf::skillSlotAvailable(unit, 0));
        assert(jf::skillSlotAvailable(unit, 1));

        assert(jf::consumeSkillCharge(unit, 0));
        assert(!jf::skillSlotAvailable(unit, 0));
        assert(!jf::consumeSkillCharge(unit, 0)); // already on cooldown

        jf::BattleState battle({unit});
        jf::refreshSkillChargesOnPhaseStart(battle, jf::Team::Player);
        assert(!jf::skillSlotAvailable(battle.units()[0], 0)); // 1 of 2 Phase starts elapsed
        jf::refreshSkillChargesOnPhaseStart(battle, jf::Team::Player);
        assert(jf::skillSlotAvailable(battle.units()[0], 0)); // 2 elapsed, off cooldown

        assert(jf::consumeSkillCharge(battle.units()[0], 1));
        assert(!jf::skillSlotAvailable(battle.units()[0], 1));
        jf::refreshSkillChargesOnPhaseStart(battle, jf::Team::Player);
        assert(!jf::skillSlotAvailable(battle.units()[0], 1)); // OncePerBattle: no mid-battle refill

        jf::initializeSkillCharges(battle.units()[0]); // fresh battle resets everything
        assert(jf::skillSlotAvailable(battle.units()[0], 0));
        assert(jf::skillSlotAvailable(battle.units()[0], 1));

        jf::Unit passive = makeUnit("passive", jf::Team::Player, {1, 1});
        passive.skillSlots[0].skillId = "immovable_stance"; // Always
        jf::initializeSkillCharges(passive);
        assert(jf::skillSlotAvailable(passive, 0));
        assert(jf::consumeSkillCharge(passive, 0));
        assert(jf::skillSlotAvailable(passive, 0)); // never consumed

        auto listing = jf::availableSkills(battle.units()[0]);
        assert(listing.size() == 2);
        assert(listing[0].skillId == "hold_formation" && listing[0].available);
        assert(jf::skillSlotAvailable(unit, 5) == false); // out-of-range slot
        assert(!jf::consumeSkillCharge(unit, -1));
    }

    {
        // docs/implementation_roadmap.md M4 item 1 "Skill Effect Executor":
        // emergency_treatment (docs/initial_skill_effects.md - HP<=50% ally,
        // range 2, heal 12, OncePerBattle) is the first equipped skill with
        // a real in-battle effect, exercised through BattleController's
        // chooseSkill()/selectSkillTarget() the same way chooseHeal()/
        // selectHealTarget() are.
        jf::Unit medic = makeUnit("medic", jf::Team::Player, {1, 3}, 4, jf::UnitClass::DawnChirurgeon);
        medic.skillSlots[0].skillId = "emergency_treatment";
        jf::Unit farAlly = makeUnit("farAlly", jf::Team::Player, {1, 1}); // distance 2, in range
        farAlly.currentHp = 5; // <= 50% of 20
        jf::Unit tooFarAlly = makeUnit("tooFarAlly", jf::Team::Player, {1, 0}); // distance 3, out of range
        tooFarAlly.currentHp = 5;
        jf::Unit healthyAlly = makeUnit("healthyAlly", jf::Team::Player, {1, 2}); // adjacent but full HP
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {2, 7}); // keeps the mission from auto-winning
        jf::BattleState battle({medic, farAlly, tooFarAlly, healthyAlly, enemy});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position); // stay put
        assert(controller.inputState() == jf::BattleInputState::SelectAction);

        controller.chooseSkill(1); // slot 1 is empty
        assert(controller.inputState() == jf::BattleInputState::SelectAction);

        controller.chooseSkill(0);
        assert(controller.inputState() == jf::BattleInputState::SelectSkillTarget);
        assert(contains(controller.skillTargetTiles(), jf::GridPos{1, 1}));
        assert(!contains(controller.skillTargetTiles(), jf::GridPos{1, 0})); // out of range
        assert(!contains(controller.skillTargetTiles(), jf::GridPos{1, 2})); // full HP, not eligible

        assert(!controller.selectSkillTarget(jf::GridPos{1, 0})); // not a valid target
        assert(controller.inputState() == jf::BattleInputState::SelectSkillTarget);

        assert(controller.selectSkillTarget(jf::GridPos{1, 1}));
        assert(controller.battle().findUnit("farAlly")->currentHp == 17); // 5 + 12
        assert(controller.inputState() == jf::BattleInputState::SelectUnit);
        // OncePerBattle: the charge is spent and won't refill mid-battle, so
        // a second use this battle is unavailable regardless of range/HP.
        assert(!jf::skillSlotAvailable(*controller.battle().findUnit("medic"), 0));
    }

    {
        // docs/initial_skill_effects.md 暁の衛生兵`cleanse`(状態治療): clears
        // poison/burn/moveDown/defenseDown/stagger from self or one adjacent
        // ally - the second skill wired through M4-A's executor, chosen to
        // exercise a different effect shape (status-clear, not healing) and
        // confirm chooseSkill()'s target rule isn't hardcoded to one skill.
        jf::Unit medic = makeUnit("medic", jf::Team::Player, {1, 3}, 4, jf::UnitClass::DawnChirurgeon);
        medic.skillSlots[0].skillId = "cleanse";
        medic.poisonRemainingProcs = 2;
        jf::Unit adjacentAlly = makeUnit("adjacentAlly", jf::Team::Player, {1, 2});
        adjacentAlly.burnRemainingProcs = 1;
        adjacentAlly.moveDownActive = true;
        adjacentAlly.defenseDownActive = true;
        adjacentAlly.staggerActive = true;
        jf::Unit farAlly = makeUnit("farAlly", jf::Team::Player, {1, 0}); // distance 3, out of range
        farAlly.poisonRemainingProcs = 3;
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {2, 7});
        jf::BattleState battle({medic, adjacentAlly, farAlly, enemy});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.inputState() == jf::BattleInputState::SelectSkillTarget);
        assert(contains(controller.skillTargetTiles(), jf::GridPos{1, 3})); // self
        assert(contains(controller.skillTargetTiles(), jf::GridPos{1, 2})); // adjacent ally
        assert(!contains(controller.skillTargetTiles(), jf::GridPos{1, 0})); // out of range

        assert(controller.selectSkillTarget(jf::GridPos{1, 2}));
        const jf::Unit* healed = controller.battle().findUnit("adjacentAlly");
        assert(healed->burnRemainingProcs == 0 && !healed->moveDownActive && !healed->defenseDownActive &&
               !healed->staggerActive);
        // Untouched: self wasn't the chosen target, and the out-of-range ally.
        assert(controller.battle().findUnit("medic")->poisonRemainingProcs == 2);
        assert(controller.battle().findUnit("farAlly")->poisonRemainingProcs == 3);
    }

    {
        // docs/initial_skill_effects.md 監視弓兵`suppressing_shot`(制圧射撃):
        // a normal attack (weapon range, not a fixed skill range) that also
        // applies moveDownActive on hit - the third skill wired through
        // M4-A's executor, chosen to exercise the "attack-shaped" family
        // (combat resolution + status) distinct from the 2 support skills.
        jf::Unit archer = makeUnit("archer", jf::Team::Player, {1, 5}, 4, jf::UnitClass::WatchArcher);
        archer.weapon = {.id = "watch_bow", .name = "Watch Bow", .might = 5, .minRange = 2, .maxRange = 3,
                        .damageType = jf::DamageType::Physical};
        archer.skillSlots[0].skillId = "suppressing_shot";
        jf::Unit inRangeEnemy = makeUnit("inRangeEnemy", jf::Team::Enemy, {1, 3}); // distance 2
        jf::Unit adjacentEnemy = makeUnit("adjacentEnemy", jf::Team::Enemy, {1, 4}); // distance 1, too close
        jf::BattleState battle({archer, inRangeEnemy, adjacentEnemy});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.inputState() == jf::BattleInputState::SelectSkillTarget);
        assert(contains(controller.skillTargetTiles(), jf::GridPos{1, 3}));
        assert(!contains(controller.skillTargetTiles(), jf::GridPos{1, 4})); // below minimumAttackRange

        const int maxHpBefore = controller.battle().findUnit("inRangeEnemy")->stats.maxHp;
        assert(controller.selectSkillTarget(jf::GridPos{1, 3}));
        const jf::Unit* hit = controller.battle().findUnit("inRangeEnemy");
        assert(hit->currentHp < maxHpBefore); // a normal attack landed
        assert(hit->moveDownActive);
        assert(!jf::skillSlotAvailable(*controller.battle().findUnit("archer"), 0)); // CD2, just used
    }

    {
        // docs/initial_skill_effects.md 暁の衛生兵`protective_treatment`(守護
        // 処置): the 4th skill wired through M4-A's executor, exercising the
        // "buff until next Enemy Phase end" family (distinct from all 3
        // shapes so far) via effectiveResistance()/
        // clearSkillBuffsAtEnemyPhaseEnd().
        jf::Unit medic = makeUnit("medic", jf::Team::Player, {1, 3}, 4, jf::UnitClass::DawnChirurgeon);
        medic.skillSlots[0].skillId = "protective_treatment";
        jf::Unit adjacentAlly = makeUnit("adjacentAlly", jf::Team::Player, {1, 2});
        adjacentAlly.stats.resistance = 4;
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {2, 7});
        jf::BattleState battle({medic, adjacentAlly, enemy});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.inputState() == jf::BattleInputState::SelectSkillTarget);
        assert(contains(controller.skillTargetTiles(), jf::GridPos{1, 2}));

        assert(controller.selectSkillTarget(jf::GridPos{1, 2}));
        assert(controller.battle().findUnit("adjacentAlly")->effectiveResistance() == 7); // 4 + 3
        assert(!jf::skillSlotAvailable(*controller.battle().findUnit("medic"), 0)); // CD2, just used
    }

    {
        // docs/initial_skill_effects.md 行軍隊長`hold_formation`(隊形維持):
        // the 5th skill, and the first "self-cast, no target selection"
        // shape - applies to self + every adjacent ally in one action,
        // resolving immediately instead of entering SelectSkillTarget.
        jf::Unit captain = makeUnit("captain", jf::Team::Player, {1, 3});
        captain.skillSlots[0].skillId = "hold_formation";
        jf::Unit adjacentAlly = makeUnit("adjacentAlly", jf::Team::Player, {1, 2});
        jf::Unit farAlly = makeUnit("farAlly", jf::Team::Player, {1, 0}); // distance 3, untouched
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {2, 7});
        jf::BattleState battle({captain, adjacentAlly, farAlly, enemy});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        // Resolves immediately: no SelectSkillTarget detour for this skill.
        assert(controller.inputState() == jf::BattleInputState::SelectUnit);
        assert(controller.battle().findUnit("captain")->effectiveDefense() ==
               controller.battle().findUnit("captain")->stats.defense + 2); // self included
        assert(controller.battle().findUnit("adjacentAlly")->effectiveDefense() ==
               controller.battle().findUnit("adjacentAlly")->stats.defense + 2);
        assert(controller.battle().findUnit("farAlly")->effectiveDefense() ==
               controller.battle().findUnit("farAlly")->stats.defense); // out of range, untouched
        assert(!jf::skillSlotAvailable(*controller.battle().findUnit("captain"), 0)); // CD2, just used
    }

    {
        // docs/initial_skill_effects.md 槍兵`halting_thrust`(足止め突き): the
        // 6th skill, identical effect shape to suppressing_shot (attack +
        // moveDown) but on a different class - shares the same
        // BattleController branch rather than duplicating it.
        jf::Unit spearman = makeUnit("spearman", jf::Team::Player, {1, 4}, 4, jf::UnitClass::Spearman);
        spearman.weapon = {.id = "iron_spear", .name = "Iron Spear", .might = 6, .minRange = 1, .maxRange = 2,
                          .damageType = jf::DamageType::Physical};
        spearman.skillSlots[0].skillId = "halting_thrust";
        jf::Unit inRangeEnemy = makeUnit("inRangeEnemy", jf::Team::Enemy, {1, 3}); // distance 1
        jf::Unit outOfRangeEnemy = makeUnit("outOfRangeEnemy", jf::Team::Enemy, {1, 0}); // distance 4
        jf::BattleState battle({spearman, inRangeEnemy, outOfRangeEnemy});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.inputState() == jf::BattleInputState::SelectSkillTarget);
        assert(contains(controller.skillTargetTiles(), jf::GridPos{1, 3}));
        assert(!contains(controller.skillTargetTiles(), jf::GridPos{1, 0})); // out of weapon range

        const int maxHpBefore = controller.battle().findUnit("inRangeEnemy")->stats.maxHp;
        assert(controller.selectSkillTarget(jf::GridPos{1, 3}));
        const jf::Unit* hit = controller.battle().findUnit("inRangeEnemy");
        assert(hit->currentHp < maxHpBefore);
        assert(hit->moveDownActive);
        assert(!jf::skillSlotAvailable(*controller.battle().findUnit("spearman"), 0)); // CD2, just used
    }

    {
        // docs/initial_skill_effects.md 辺境斥候`ambush`(奇襲): the 7th skill,
        // and the first added purely via a new attackSkillShapes() table row
        // in BattleController.cpp - no new branching logic, proving the
        // shape-table refactor is actually reusable. Effect: normal attack +
        // flat Damage+3, restricted to an unacted enemy, OncePerBattle.
        jf::Unit scout = makeUnit("scout", jf::Team::Player, {1, 4}, 5, jf::UnitClass::FrontierScout);
        scout.weapon = {.id = "scout_blade", .name = "Scout Blade", .might = 4, .minRange = 1, .maxRange = 1,
                       .damageType = jf::DamageType::Physical};
        scout.skillSlots[0].skillId = "ambush";
        jf::Unit unactedEnemy = makeUnit("unactedEnemy", jf::Team::Enemy, {1, 3}); // adjacent, hasn't acted
        jf::Unit actedEnemy = makeUnit("actedEnemy", jf::Team::Enemy, {1, 5});     // adjacent, already acted
        actedEnemy.hasActed = true;
        jf::BattleState battle({scout, unactedEnemy, actedEnemy});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.inputState() == jf::BattleInputState::SelectSkillTarget);
        assert(contains(controller.skillTargetTiles(), jf::GridPos{1, 3}));
        assert(!contains(controller.skillTargetTiles(), jf::GridPos{1, 5})); // already acted, excluded

        // Normal-attack damage alone (no skill) for comparison, computed with
        // the same stats but without the ambush bonus.
        const int normalDamage = jf::computeDamage(*controller.battle().findUnit("scout"),
                                                    *controller.battle().findUnit("unactedEnemy"), 0);
        assert(controller.selectSkillTarget(jf::GridPos{1, 3}));
        const jf::Unit* hit = controller.battle().findUnit("unactedEnemy");
        assert(hit->stats.maxHp - hit->currentHp == normalDamage + 3); // base damage + flat bonus
        assert(!hit->moveDownActive); // ambush doesn't apply moveDown, unlike suppressing_shot
        assert(!jf::skillSlotAvailable(*controller.battle().findUnit("scout"), 0)); // OncePerBattle, just used
    }

    {
        // docs/initial_skill_effects.md 古参守備兵`extended_lockdown`(封鎖
        // 強化): the 8th skill, added by extending buffSkillShapes()'s
        // BuffKind enum (Resistance/Defense/ZocRange) and a new `selfOnly`
        // resolution mode - no new branching logic in chooseSkill() beyond
        // the table row and the ZocRange case in applyBuff().
        jf::Unit guard = makeUnit("guard", jf::Team::Player, {1, 3}, 3, jf::UnitClass::VeteranGuard);
        guard.skillSlots[0].skillId = "extended_lockdown";
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {1, 0}); // keeps Team::Player from being "done"
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {2, 7});
        jf::BattleState battle({guard, ally, enemy});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        // Resolves immediately: self-only, no SelectSkillTarget detour.
        assert(controller.inputState() == jf::BattleInputState::SelectUnit);
        assert(controller.battle().findUnit("guard")->zocRangeExtended);
        assert(!jf::skillSlotAvailable(*controller.battle().findUnit("guard"), 0)); // CD2, just used
    }

    {
        // docs/initial_skill_effects.md 監視弓兵`mark_target`(標的指定): the
        // 9th skill, and the first "Mark" shape (no attack roll at all - just
        // sets a flag consumed by a later attack). Added via a new
        // markSkillShapes() table + one dispatch branch.
        jf::Unit archer = makeUnit("archer", jf::Team::Player, {1, 4}, 4, jf::UnitClass::WatchArcher);
        archer.weapon = {.id = "watch_bow", .name = "Watch Bow", .might = 5, .minRange = 2, .maxRange = 3,
                        .damageType = jf::DamageType::Physical};
        archer.skillSlots[0].skillId = "mark_target";
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {1, 0});
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 2}); // distance 2, in range
        jf::BattleState battle({archer, ally, enemy});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.inputState() == jf::BattleInputState::SelectSkillTarget);
        assert(contains(controller.skillTargetTiles(), jf::GridPos{1, 2}));

        assert(controller.selectSkillTarget(jf::GridPos{1, 2}));
        assert(controller.battle().findUnit("enemy")->currentHp ==
               controller.battle().findUnit("enemy")->stats.maxHp); // Damageなし
        assert(controller.battle().findUnit("enemy")->markedBonusDamage == 2);
        assert(!jf::skillSlotAvailable(*controller.battle().findUnit("archer"), 0)); // CD2, just used
    }

    {
        // docs/initial_skill_effects.md 行軍隊長`support_order`(援護命令): the
        // 10th skill, reusing mark_target's exact mechanism with a negative
        // delta on an adjacent ally instead of a positive one on an enemy -
        // MarkSkillShape extended with `targetsAlly` rather than a new shape.
        jf::Unit captain = makeUnit("captain", jf::Team::Player, {1, 3});
        captain.skillSlots[0].skillId = "support_order";
        jf::Unit adjacentAlly = makeUnit("adjacentAlly", jf::Team::Player, {1, 2});
        jf::Unit farAlly = makeUnit("farAlly", jf::Team::Player, {1, 0}); // distance 3, out of range
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {2, 7});
        jf::BattleState battle({captain, adjacentAlly, farAlly, enemy});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.inputState() == jf::BattleInputState::SelectSkillTarget);
        assert(contains(controller.skillTargetTiles(), jf::GridPos{1, 2}));
        assert(!contains(controller.skillTargetTiles(), jf::GridPos{1, 3})); // self excluded
        assert(!contains(controller.skillTargetTiles(), jf::GridPos{1, 0})); // out of range

        assert(controller.selectSkillTarget(jf::GridPos{1, 2}));
        assert(controller.battle().findUnit("adjacentAlly")->markedBonusDamage == -3);

        // The shield reduces the next real hit this ally takes, floored at 1
        // like any other attack.
        jf::Unit attacker = makeUnit("attacker", jf::Team::Enemy, {1, 1});
        jf::Unit* shielded = controller.battle().findUnit("adjacentAlly");
        const int shieldedDamage = jf::computeDamage(attacker, *shielded, 0);
        shielded->markedBonusDamage = 0; // true baseline, without the shield
        const int normalDamage = jf::computeDamage(attacker, *shielded, 0);
        assert(shieldedDamage == std::max(normalDamage - 3, 1));
    }

    {
        // docs/initial_skill_effects.md 行軍隊長`advance_order`(前進命令): the
        // 11th skill - a dedicated branch (not a shared shape table) since
        // its target rule (未行動限定, self excluded) and clear timing (this
        // Player Phase's own end) both differ from every existing shape.
        jf::Unit captain = makeUnit("captain", jf::Team::Player, {1, 3});
        captain.skillSlots[0].skillId = "advance_order";
        jf::Unit unactedAlly = makeUnit("unactedAlly", jf::Team::Player, {1, 2}, 4);
        jf::Unit actedAlly = makeUnit("actedAlly", jf::Team::Player, {1, 4});
        actedAlly.hasActed = true;
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {2, 7});
        jf::BattleState battle({captain, unactedAlly, actedAlly, enemy});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.inputState() == jf::BattleInputState::SelectSkillTarget);
        assert(contains(controller.skillTargetTiles(), jf::GridPos{1, 2}));
        assert(!contains(controller.skillTargetTiles(), jf::GridPos{1, 4})); // already acted
        assert(!contains(controller.skillTargetTiles(), jf::GridPos{1, 3})); // self excluded

        assert(controller.selectSkillTarget(jf::GridPos{1, 2}));
        assert(controller.battle().findUnit("unactedAlly")->effectiveMove() == 5); // 4 + 1
        assert(!jf::skillSlotAvailable(*controller.battle().findUnit("captain"), 0)); // OncePerBattle, just used
    }

    {
        // GameApp equip validation (docs/skill_system.md "解放と装備"):
        // requires the class's training branch to be unlocked, and the
        // skill's class must match the unit's class.
        jf::GameData data = makeFactoryData();
        data.playerParty[0].classId = jf::UnitClass::Spearman;
        jf::GameApp app(data);

        // Not yet unlocked: vanguard_training isn't built.
        assert(!app.equipSkillForUnit("player0", 0, "spear_wall"));
        assert(app.equippedSkills().find("player0") == app.equippedSkills().end());

        // Wrong class for this skill.
        jf::BaseState& testBase = const_cast<jf::BaseState&>(app.baseState());
        testBase.unlockedNodeIds.insert("vanguard_training");
        assert(!app.equipSkillForUnit("player0", 0, "hold_formation")); // March Captain skill

        assert(app.equipSkillForUnit("player0", 0, "spear_wall"));
        assert(app.equipSkillForUnit("player0", 1, "halting_thrust"));
        assert(app.equippedSkills().at("player0").equippedSkillIds[0] == "spear_wall");
        assert(app.equippedSkills().at("player0").equippedSkillIds[1] == "halting_thrust");

        assert(app.equipSkillForUnit("player0", 0, "")); // unequip
        assert(app.equippedSkills().at("player0").equippedSkillIds[0].empty());

        // Equipped skills carry into battle with fresh charges.
        assert(startCinderwatchExpedition(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        assert(app.screen() == jf::Screen::Battle);
        const jf::Unit* battleUnit = app.battle().battle().findUnit("player0");
        assert(battleUnit && battleUnit->skillSlots[1].skillId == "halting_thrust");
        assert(jf::skillSlotAvailable(*battleUnit, 1));
    }

    {
        // Skill equip round-trips through SaveData (docs/skill_system.md
        // "保存データ"): survives export/import and requires the training
        // branch to still be unlocked on the receiving side.
        jf::GameData data = makeFactoryData();
        data.playerParty[0].classId = jf::UnitClass::Spearman;
        jf::GameApp app(data);
        jf::BaseState& testBase = const_cast<jf::BaseState&>(app.baseState());
        testBase.unlockedNodeIds.insert("vanguard_training");
        assert(app.equipSkillForUnit("player0", 0, "spear_wall"));

        jf::SaveData saved = app.createSaveData("en");
        assert(saved.unitEquippedSkillsSlot0.at("player0") == "spear_wall");

        jf::GameApp restoredApp(data);
        assert(restoredApp.applySaveData(saved));
        assert(restoredApp.equippedSkills().at("player0").equippedSkillIds[0] == "spear_wall");
    }

    {
        // Objective system (docs/mission_objectives.md) - data foundation:
        // the default mission matches the game's pre-existing behavior
        // (defeat every enemy = victory), and defeat is always evaluated
        // before victory even when both would be true at once.
        jf::Unit player = makeUnit("player", jf::Team::Player, {1, 0});
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 7});
        {
            jf::BattleState battle({player, enemy});
            jf::syncObjectiveProgress(battle);
            assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Ongoing);
        }
        {
            jf::Unit deadEnemy = enemy;
            deadEnemy.currentHp = 0;
            jf::BattleState battle({player, deadEnemy});
            jf::syncObjectiveProgress(battle);
            auto outcome = jf::evaluateBattleOutcome(battle);
            assert(outcome.kind == jf::BattleOutcomeKind::Victory);
            assert(outcome.completedPrimaryObjectives == std::vector<jf::ObjectiveId>{"eliminate_enemies"});
        }
        {
            jf::Unit deadPlayer = player;
            deadPlayer.currentHp = 0;
            jf::Unit deadEnemy = enemy;
            deadEnemy.currentHp = 0;
            jf::BattleState battle({deadPlayer, deadEnemy});
            // Same batch defeats both sides: defeat wins regardless of the
            // primary objective also being satisfied.
            jf::syncObjectiveProgress(battle);
            assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Defeat);
        }
    }

    {
        // DefeatUnit kind: satisfied once the named unit is no longer alive
        // (or isn't found at all - e.g. after a future "escaped" removal).
        jf::Unit player = makeUnit("player", jf::Team::Player, {1, 0});
        jf::Unit commander = makeUnit("commander", jf::Team::Enemy, {1, 7});
        jf::ObjectiveDefinition defeatCommander;
        defeatCommander.id = "defeat_commander";
        defeatCommander.kind = jf::ObjectiveKind::DefeatUnit;
        defeatCommander.primary = true;
        defeatCommander.groupId = "primary";
        defeatCommander.target.unitId = "commander";
        jf::BattleMissionState mission;
        mission.groups.push_back({"primary", jf::ObjectiveGroupRule::All});
        mission.definitions.push_back(defeatCommander);
        mission.progress[defeatCommander.id] = jf::ObjectiveProgress{defeatCommander.id};

        jf::BattleState battle({player, commander}, {}, 0, mission);
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Ongoing);
        battle.units()[1].currentHp = 0;
        jf::syncObjectiveProgress(battle);
        auto outcome = jf::evaluateBattleOutcome(battle);
        assert(outcome.kind == jf::BattleOutcomeKind::Victory);
        assert(outcome.completedPrimaryObjectives == std::vector<jf::ObjectiveId>{"defeat_commander"});
    }

    {
        // AND vs OR groups: an "Any" group is satisfied once one of its
        // objectives is; an "All" group needs every one of them.
        jf::Unit player = makeUnit("player", jf::Team::Player, {1, 0});
        jf::Unit enemyA = makeUnit("enemyA", jf::Team::Enemy, {1, 6});
        jf::Unit enemyB = makeUnit("enemyB", jf::Team::Enemy, {1, 7});
        jf::ObjectiveDefinition defeatA;
        defeatA.id = "defeat_a";
        defeatA.kind = jf::ObjectiveKind::DefeatUnit;
        defeatA.primary = true;
        defeatA.groupId = "primary";
        defeatA.target.unitId = "enemyA";
        jf::ObjectiveDefinition defeatB = defeatA;
        defeatB.id = "defeat_b";
        defeatB.target.unitId = "enemyB";

        jf::BattleMissionState anyMission;
        anyMission.groups.push_back({"primary", jf::ObjectiveGroupRule::Any});
        anyMission.definitions = {defeatA, defeatB};
        anyMission.progress[defeatA.id] = jf::ObjectiveProgress{defeatA.id};
        anyMission.progress[defeatB.id] = jf::ObjectiveProgress{defeatB.id};
        jf::BattleState anyBattle({player, enemyA, enemyB}, {}, 0, anyMission);
        anyBattle.units()[1].currentHp = 0; // only enemyA defeated
        jf::syncObjectiveProgress(anyBattle);
        assert(jf::evaluateBattleOutcome(anyBattle).kind == jf::BattleOutcomeKind::Victory);

        jf::BattleMissionState allMission = anyMission;
        allMission.groups[0].rule = jf::ObjectiveGroupRule::All;
        jf::BattleState allBattle({player, enemyA, enemyB}, {}, 0, allMission);
        allBattle.units()[1].currentHp = 0; // only enemyA defeated - not enough for All
        jf::syncObjectiveProgress(allBattle);
        assert(jf::evaluateBattleOutcome(allBattle).kind == jf::BattleOutcomeKind::Ongoing);
        allBattle.units()[2].currentHp = 0; // both defeated now
        jf::syncObjectiveProgress(allBattle);
        assert(jf::evaluateBattleOutcome(allBattle).kind == jf::BattleOutcomeKind::Victory);
    }

    {
        // SecureTile: reaching the tile alone doesn't complete it (no
        // ActionResolvedEvent fired yet); ending an action there does, and
        // exactly once. Duplicate event ids don't re-credit a different actor.
        jf::ObjectiveDefinition secure;
        secure.id = "secure_point";
        secure.kind = jf::ObjectiveKind::SecureTile;
        secure.primary = true;
        secure.groupId = "primary";
        secure.target.tile = {1, 4};
        jf::BattleMissionState mission;
        mission.groups.push_back({"primary", jf::ObjectiveGroupRule::All});
        mission.definitions.push_back(secure);
        mission.progress[secure.id] = jf::ObjectiveProgress{secure.id};

        // Before any ActionResolvedEvent reaches it, the objective is still
        // Active - BattleController only fires that event once an action
        // actually ends (see the integration test below for the full path
        // proving plain movement alone never fires it).
        assert(mission.progress.at("secure_point").status == jf::ObjectiveStatus::Active);

        jf::BattleEvent actionEnd{
            2, 2, jf::ActionResolvedEvent{2, "scout", jf::Team::Player, jf::ActionKind::Wait, {1, 4}}};
        jf::handleObjectiveEvent(mission, actionEnd);
        assert(mission.progress.at("secure_point").status == jf::ObjectiveStatus::Completed);
        assert(mission.progress.at("secure_point").creditedTargetIds.count("scout") == 1);

        // Same event id again, different actor: ignored entirely (dedup).
        jf::BattleEvent duplicateId{
            2, 2, jf::ActionResolvedEvent{2, "someone_else", jf::Team::Player, jf::ActionKind::Wait, {1, 4}}};
        jf::handleObjectiveEvent(mission, duplicateId);
        assert(mission.progress.at("secure_point").creditedTargetIds.count("someone_else") == 0);
    }

    {
        // SecureTile only credits the side it's written for - an enemy
        // ending its action on a player objective's tile must not complete
        // it (this was previously a real bug: the team wasn't checked).
        jf::ObjectiveDefinition secure;
        secure.id = "secure_point";
        secure.kind = jf::ObjectiveKind::SecureTile;
        secure.primary = true;
        secure.groupId = "primary";
        secure.target.tile = {1, 4};
        secure.target.securingTeam = jf::Team::Player;
        jf::BattleMissionState mission;
        mission.groups.push_back({"primary", jf::ObjectiveGroupRule::All});
        mission.definitions.push_back(secure);
        mission.progress[secure.id] = jf::ObjectiveProgress{secure.id};

        jf::BattleEvent enemyEndsOnTile{
            1, 1, jf::ActionResolvedEvent{1, "wolf", jf::Team::Enemy, jf::ActionKind::Wait, {1, 4}}};
        jf::handleObjectiveEvent(mission, enemyEndsOnTile);
        assert(mission.progress.at("secure_point").status == jf::ObjectiveStatus::Active);

        jf::BattleEvent playerEndsOnTile{
            2, 2, jf::ActionResolvedEvent{2, "scout", jf::Team::Player, jf::ActionKind::Wait, {1, 4}}};
        jf::handleObjectiveEvent(mission, playerEndsOnTile);
        assert(mission.progress.at("secure_point").status == jf::ObjectiveStatus::Completed);
    }

    {
        // DefeatUnit with a target id that doesn't exist in the battle is a
        // mission-authoring error, not an automatic win.
        jf::Unit player = makeUnit("player", jf::Team::Player, {1, 0});
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 7});
        jf::ObjectiveDefinition defeatGhost;
        defeatGhost.id = "defeat_ghost";
        defeatGhost.kind = jf::ObjectiveKind::DefeatUnit;
        defeatGhost.primary = true;
        defeatGhost.groupId = "primary";
        defeatGhost.target.unitId = "no_such_unit";
        jf::BattleMissionState mission;
        mission.groups.push_back({"primary", jf::ObjectiveGroupRule::All});
        mission.definitions.push_back(defeatGhost);
        mission.progress[defeatGhost.id] = jf::ObjectiveProgress{defeatGhost.id};
        jf::BattleState battle({player, enemy}, {}, 0, mission);
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Ongoing);
    }

    {
        // ObjectiveProgress stays in sync with the live-evaluated kinds:
        // syncObjectiveProgress() marks a satisfied objective Completed, and
        // in an Any group the unmet side becomes Superseded rather than
        // being left Active forever.
        jf::Unit player = makeUnit("player", jf::Team::Player, {1, 0});
        jf::Unit enemyA = makeUnit("enemyA", jf::Team::Enemy, {1, 6});
        jf::Unit enemyB = makeUnit("enemyB", jf::Team::Enemy, {1, 7});
        jf::ObjectiveDefinition defeatA;
        defeatA.id = "defeat_a";
        defeatA.kind = jf::ObjectiveKind::DefeatUnit;
        defeatA.primary = true;
        defeatA.groupId = "primary";
        defeatA.target.unitId = "enemyA";
        jf::ObjectiveDefinition defeatB = defeatA;
        defeatB.id = "defeat_b";
        defeatB.target.unitId = "enemyB";
        jf::BattleMissionState mission;
        mission.groups.push_back({"primary", jf::ObjectiveGroupRule::Any});
        mission.definitions = {defeatA, defeatB};
        mission.progress[defeatA.id] = jf::ObjectiveProgress{defeatA.id};
        mission.progress[defeatB.id] = jf::ObjectiveProgress{defeatB.id};
        jf::BattleState battle({player, enemyA, enemyB}, {}, 0, mission);
        battle.units()[1].currentHp = 0; // only enemyA defeated
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);
        assert(battle.missionState().progress.at("defeat_a").status == jf::ObjectiveStatus::Completed);
        assert(battle.missionState().progress.at("defeat_b").status == jf::ObjectiveStatus::Superseded);
    }

    {
        // docs/mission_objectives.md "ORグループ": if multiple Any-group
        // members become satisfied in the same sync (both enemies die in the
        // same batch), only the FIRST one in Definition order is Completed;
        // the other is Superseded, not also Completed.
        jf::Unit player = makeUnit("player", jf::Team::Player, {1, 0});
        jf::Unit enemyA = makeUnit("enemyA", jf::Team::Enemy, {1, 6});
        jf::Unit enemyB = makeUnit("enemyB", jf::Team::Enemy, {1, 7});
        jf::ObjectiveDefinition defeatA;
        defeatA.id = "defeat_a";
        defeatA.kind = jf::ObjectiveKind::DefeatUnit;
        defeatA.primary = true;
        defeatA.groupId = "primary";
        defeatA.target.unitId = "enemyA";
        jf::ObjectiveDefinition defeatB = defeatA;
        defeatB.id = "defeat_b";
        defeatB.target.unitId = "enemyB";
        jf::BattleMissionState mission;
        mission.groups.push_back({"primary", jf::ObjectiveGroupRule::Any});
        mission.definitions = {defeatA, defeatB}; // defeat_a first
        mission.progress[defeatA.id] = jf::ObjectiveProgress{defeatA.id};
        mission.progress[defeatB.id] = jf::ObjectiveProgress{defeatB.id};
        jf::BattleState battle({player, enemyA, enemyB}, {}, 0, mission);
        battle.units()[1].currentHp = 0; // both enemies die in the same batch
        battle.units()[2].currentHp = 0;
        jf::syncObjectiveProgress(battle);
        assert(battle.missionState().progress.at("defeat_a").status == jf::ObjectiveStatus::Completed);
        assert(battle.missionState().progress.at("defeat_b").status == jf::ObjectiveStatus::Superseded);

        // A later sync (e.g. next action) must not re-race or flip anything.
        jf::syncObjectiveProgress(battle);
        assert(battle.missionState().progress.at("defeat_a").status == jf::ObjectiveStatus::Completed);
        assert(battle.missionState().progress.at("defeat_b").status == jf::ObjectiveStatus::Superseded);
    }

    {
        // docs/battle_resolution_contract.md "同時発生": "同じBatchで敵味方全滅
        // なら敗北を優先する" - even though EliminateTeam is also satisfied in
        // this same batch, allPlayersDefeated() must win.
        jf::Unit player = makeUnit("player", jf::Team::Player, {1, 0});
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 7});
        jf::ObjectiveDefinition eliminate;
        eliminate.id = "eliminate_enemy";
        eliminate.kind = jf::ObjectiveKind::EliminateTeam;
        eliminate.primary = true;
        eliminate.groupId = "primary";
        eliminate.target.team = jf::Team::Enemy;
        jf::BattleMissionState mission;
        mission.groups.push_back({"primary", jf::ObjectiveGroupRule::All});
        mission.definitions.push_back(eliminate);
        mission.progress[eliminate.id] = jf::ObjectiveProgress{eliminate.id};
        jf::BattleState battle({player, enemy}, {}, 0, mission);
        battle.units()[0].currentHp = 0; // player wiped
        battle.units()[1].currentHp = 0; // enemy wiped, in the same batch
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Defeat);
    }

    {
        // docs/battle_resolution_contract.md "同時発生": "同時撃破はUnit ID順に
        // Event化する" - emitUnitDefeatedEvents() must walk battle.units() (a
        // fixed, deterministic order), not the AliveSnapshot unordered_map
        // itself, so simultaneous defeats produce the same event order on
        // every run for the same battle setup. Exercised here by defeating 3
        // units simultaneously and checking every one gets exactly one
        // consumed event id (no duplicate/missing emission), which would be
        // sensitive to iteration order if the snapshot map's own (hash-
        // dependent) order were used to walk and re-derive state instead.
        jf::Unit playerA = makeUnit("playerA", jf::Team::Player, {1, 0});
        jf::Unit playerB = makeUnit("playerB", jf::Team::Player, {1, 1});
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 7});
        jf::BattleState battle({playerA, playerB, enemy});
        const jf::AliveSnapshot before = jf::captureAliveSnapshot(battle);
        battle.units()[0].currentHp = 0;
        battle.units()[1].currentHp = 0;
        const std::size_t consumedBefore = battle.missionState().consumedEventIds.size();
        jf::emitUnitDefeatedEvents(battle, before);
        assert(battle.missionState().consumedEventIds.size() == consumedBefore + 2); // exactly 2 new events,
                                                                                      // one per simultaneously
                                                                                      // defeated unit
    }

    {
        // Battle-start validation (docs/mission_objectives.md "戦闘開始時の
        //検証"): a well-formed mission reports no errors; common authoring
        // mistakes are each caught with a specific message.
        jf::Unit player = makeUnit("player", jf::Team::Player, {1, 0});
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 7});
        {
            jf::BattleState battle({player, enemy}); // default mission
            assert(jf::validateBattleMission(battle.missionState(), battle).empty());
        }
        {
            // DefeatUnit targeting a unit that doesn't exist.
            jf::ObjectiveDefinition defeatGhost;
            defeatGhost.id = "defeat_ghost";
            defeatGhost.kind = jf::ObjectiveKind::DefeatUnit;
            defeatGhost.primary = true;
            defeatGhost.groupId = "primary";
            defeatGhost.target.unitId = "no_such_unit";
            jf::BattleMissionState mission;
            mission.groups.push_back({"primary", jf::ObjectiveGroupRule::All});
            mission.definitions.push_back(defeatGhost);
            mission.progress[defeatGhost.id] = jf::ObjectiveProgress{defeatGhost.id};
            jf::BattleState battle({player, enemy}, {}, 0, mission);
            assert(!jf::validateBattleMission(battle.missionState(), battle).empty());
        }
        {
            // SecureTile targeting a tile occupied at battle start (the
            // player itself, at {1, 0}).
            jf::ObjectiveDefinition secure;
            secure.id = "secure_point";
            secure.kind = jf::ObjectiveKind::SecureTile;
            secure.primary = true;
            secure.groupId = "primary";
            secure.target.tile = {1, 0};
            jf::BattleMissionState mission;
            mission.groups.push_back({"primary", jf::ObjectiveGroupRule::All});
            mission.definitions.push_back(secure);
            mission.progress[secure.id] = jf::ObjectiveProgress{secure.id};
            jf::BattleState battle({player, enemy}, {}, 0, mission);
            assert(!jf::validateBattleMission(battle.missionState(), battle).empty());
        }
        {
            // Two primary groups is a Phase 1 violation (only one allowed).
            jf::ObjectiveDefinition defeatA;
            defeatA.id = "defeat_a";
            defeatA.kind = jf::ObjectiveKind::DefeatUnit;
            defeatA.primary = true;
            defeatA.groupId = "group_a";
            defeatA.target.unitId = "enemy";
            jf::ObjectiveDefinition defeatB = defeatA;
            defeatB.id = "defeat_b";
            defeatB.groupId = "group_b";
            jf::BattleMissionState mission;
            mission.groups.push_back({"group_a", jf::ObjectiveGroupRule::All});
            mission.groups.push_back({"group_b", jf::ObjectiveGroupRule::All});
            mission.definitions = {defeatA, defeatB};
            mission.progress[defeatA.id] = jf::ObjectiveProgress{defeatA.id};
            mission.progress[defeatB.id] = jf::ObjectiveProgress{defeatB.id};
            jf::BattleState battle({player, enemy}, {}, 0, mission);
            assert(!jf::validateBattleMission(battle.missionState(), battle).empty());
        }
    }

    {
        // UnitDefeatedEvent fires exactly once for a combat-caused defeat,
        // reaching the mission's consumedEventIds (this was previously
        // never fired at all from the real battle flow).
        jf::Unit attacker = makeUnit("attacker", jf::Team::Player, {1, 0});
        attacker.stats.strength = 99;
        jf::Unit victim = makeUnit("victim", jf::Team::Enemy, {1, 1});
        victim.stats.maxHp = 1;
        victim.currentHp = 1;
        jf::BattleController controller(jf::BattleState({attacker, victim}));
        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile({1, 0});
        controller.chooseAttack();
        controller.selectTargetTile({1, 1});
        controller.confirmAttack();
        assert(!controller.battle().units()[1].isAlive());
        assert(controller.battle().missionState().consumedEventIds.size() >= 2); // UnitDefeated + ActionResolved
    }

    {
        // Integration: a custom SecureTile mission through the real
        // BattleController flow - moving onto the tile alone doesn't win,
        // but ending the action there does (docs/mission_objectives.md
        // "SecureTileは到達だけでは達成しない").
        jf::ObjectiveDefinition secure;
        secure.id = "secure_point";
        secure.kind = jf::ObjectiveKind::SecureTile;
        secure.primary = true;
        secure.groupId = "primary";
        secure.target.tile = {1, 0};
        jf::BattleMissionState mission;
        mission.groups.push_back({"primary", jf::ObjectiveGroupRule::All});
        mission.definitions.push_back(secure);
        mission.progress[secure.id] = jf::ObjectiveProgress{secure.id};

        jf::Unit player = makeUnit("player", jf::Team::Player, {1, 0});
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 7});
        jf::BattleController controller(jf::BattleState({player, enemy}, {}, 0, mission));

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile({1, 0}); // reach the tile...
        assert(controller.inputState() != jf::BattleInputState::Victory); // ...but not achieved yet
        controller.chooseWait(); // ...ends the action there
        assert(controller.inputState() == jf::BattleInputState::Victory);
    }

    {
        // Wolf pack AI: even one wolf closes distance and attacks when its
        // selected target is reachable this turn.
        jf::Unit player = makeUnit("player", jf::Team::Player, {1, 7});
        jf::Unit wolf = makeUnit("lonewolf", jf::Team::Enemy, {1, 3}, 5, jf::UnitClass::Wolf);
        jf::BattleState battle({player, wolf});
        jf::Unit* attacked = jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(attacked == &battle.units()[0]);
        assert(battle.units()[1].hasActed);
        assert(battle.units()[0].currentHp < battle.units()[0].stats.maxHp);
        assert(jf::manhattanDistance(battle.units()[1].position, battle.units()[0].position) == 1);
    }

    {
        // Multiple wolves use the same target priority and attack without a
        // separate pack-readiness gate.
        jf::Unit player = makeUnit("player", jf::Team::Player, {1, 7});
        jf::Unit wolfA = makeUnit("wolfA", jf::Team::Enemy, {1, 6}, 5, jf::UnitClass::Wolf); // already adjacent
        jf::Unit wolfB = makeUnit("wolfB", jf::Team::Enemy, {0, 6}, 5, jf::UnitClass::Wolf); // reachable this turn
        jf::BattleState battle({player, wolfA, wolfB});
        jf::Unit* attacked = jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(attacked == &battle.units()[0]);
    }

    {
        // Wolves focus current HP before distance or UnitId.
        jf::Unit healthy = makeUnit("healthy", jf::Team::Player, {1, 4});
        jf::Unit wounded = makeUnit("wounded", jf::Team::Player, {1, 2});
        wounded.currentHp = 3;
        jf::Unit wolf = makeUnit("wolf", jf::Team::Enemy, {1, 3}, 5, jf::UnitClass::Wolf);
        jf::BattleState battle({healthy, wounded, wolf});
        jf::Unit* attacked = jf::takeEnemyTurn(battle, battle.units()[2]);
        assert(attacked == &battle.units()[1]);
        assert(battle.units()[0].currentHp == battle.units()[0].stats.maxHp);
    }

    {
        // 灰角大猪 (docs/regions/ashbough_forest.md "灰角大猪"): a charge is
        // telegraphed one turn, then executes the next - traveling along the
        // boar's own row, damaging (but not stopping for) any ally it
        // passes, and covering the full normal range (3) when nothing
        // blocks it.
        jf::Unit boar = makeUnit("boar", jf::Team::Enemy, {1, 5}, 2, jf::UnitClass::AshenhornBoar);
        boar.stats.strength = 9;
        boar.stats.defense = 5;
        boar.stats.resistance = 1;
        boar.stats.maxHp = 56;
        boar.currentHp = 56;
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {1, 2}); // same row, distance 3
        jf::BattleState battle({ally, boar});

        jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(battle.units()[1].chargeTelegraphed);
        assert(battle.units()[1].position == (jf::GridPos{1, 5})); // hasn't moved yet
        assert(battle.units()[0].currentHp == battle.units()[0].stats.maxHp); // untouched

        battle.units()[1].hasActed = false; // simulate the next turn
        jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(!battle.units()[1].chargeTelegraphed);
        assert(battle.units()[1].position == (jf::GridPos{1, 2})); // covered the full range-3
        // The exact charge power bonus is a tunable balance constant (not
        // asserted here); a charge that passes over an ally must always deal
        // at least 1 damage.
        assert(battle.units()[0].currentHp < battle.units()[0].stats.maxHp);
    }

    {
        // Charge direction is selected at telegraph time and supports a
        // target to the boar's right as well as the traditional left side.
        jf::Unit boar = makeUnit("boar", jf::Team::Enemy, {1, 2}, 2, jf::UnitClass::AshenhornBoar);
        boar.stats.strength = 9;
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {1, 5});
        jf::BattleState battle({ally, boar});

        jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(battle.units()[1].chargeTelegraphed);
        assert(battle.units()[1].chargeDirection == 1);

        battle.units()[1].hasActed = false;
        jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(!battle.units()[1].chargeTelegraphed);
        assert(battle.units()[1].position == (jf::GridPos{1, 5}));
        assert(battle.units()[0].currentHp < battle.units()[0].stats.maxHp);
    }

    {
        // Colliding with a fallen log stops the charge immediately, destroys
        // the log, and applies the DEF2/RES0 stun - an ally further down the
        // same row than the log is never reached, let alone damaged.
        jf::Unit boar = makeUnit("boar", jf::Team::Enemy, {1, 5}, 2, jf::UnitClass::AshenhornBoar);
        boar.stats.strength = 9;
        boar.stats.defense = 5;
        boar.stats.resistance = 1;
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {1, 2}); // in range, but behind the log
        jf::Unit escort = makeUnit("escort", jf::Team::Enemy, {0, 5}, 5, jf::UnitClass::Wolf);
        jf::BattleState battle({ally, boar, escort});
        jf::BattleObjectDefinition logDef;
        logDef.definitionId = "fallen_log";
        logDef.kind = jf::BattleObjectKind::Barrier;
        logDef.blocksMovement = true;
        assert(battle.registerObjectDefinition(logDef));
        assert(battle.placeObject({"log1", "fallen_log", {1, 4}}));

        jf::takeEnemyTurn(battle, battle.units()[1]); // telegraph
        assert(battle.units()[1].chargeTelegraphed);
        battle.units()[1].hasActed = false;
        jf::takeEnemyTurn(battle, battle.units()[1]); // execute -> collides at col 4

        assert(battle.units()[0].currentHp == battle.units()[0].stats.maxHp); // never reached
        assert(battle.units()[1].position == (jf::GridPos{1, 4}));
        assert(battle.objectAt({1, 4})->state == jf::BattleObjectStateKind::Destroyed);
        assert(battle.bossHasCollidedWithBarrier());
        assert(battle.units()[1].bossStunnedNextEnemyPhase);
        assert(battle.units()[1].bossWeakenedFromStun);
        assert(battle.units()[1].stats.defense == 2 && battle.units()[1].stats.resistance == 0);

        // Next turn: skipped entirely, stun consumed, still weakened.
        battle.units()[1].hasActed = false;
        jf::Unit* attacked = jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(attacked == nullptr);
        assert(!battle.units()[1].bossStunnedNextEnemyPhase);
        assert(battle.units()[1].stats.defense == 2); // still weakened during the skipped turn

        // The stun belongs only to the boss. Its escort still resolves a
        // normal wolf action during the same Enemy Phase.
        assert(!battle.units()[2].hasActed);
        jf::takeEnemyTurn(battle, battle.units()[2]);
        assert(battle.units()[2].hasActed);

        // The turn after that: stats restore right before it acts again.
        battle.units()[1].hasActed = false;
        jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(battle.units()[1].stats.defense == 5 && battle.units()[1].stats.resistance == 1);
    }

    {
        // Sweep hits up to 3 allies in the column immediately toward the
        // player side, spanning boar.row-1..boar.row+1 - triggered whenever
        // 2+ are there, ahead of telegraphing a new charge.
        jf::Unit boar = makeUnit("boar", jf::Team::Enemy, {1, 5}, 2, jf::UnitClass::AshenhornBoar);
        boar.stats.strength = 9;
        jf::Unit allyUp = makeUnit("allyUp", jf::Team::Player, {0, 4});
        jf::Unit allyDown = makeUnit("allyDown", jf::Team::Player, {2, 4});
        jf::BattleState battle({allyUp, allyDown, boar});
        // The exact sweep power bonus is a tunable balance constant (not
        // asserted here); both allies in the pattern must take equal,
        // positive damage, and the boss itself never moves during a sweep.
        jf::takeEnemyTurn(battle, battle.units()[2]);
        assert(battle.units()[0].currentHp < battle.units()[0].stats.maxHp);
        assert(battle.units()[0].currentHp == battle.units()[1].currentHp);
        assert(battle.units()[2].position == (jf::GridPos{1, 5})); // sweep doesn't move the boss
    }

    {
        // Enrage triggers once at HP<=50%, permanently raising STR (the
        // exact target value is a tunable balance constant, not asserted
        // here) without consuming the turn; the boss immediately telegraphs
        // a charge when no sweep target is available.
        jf::Unit boar = makeUnit("boar", jf::Team::Enemy, {1, 5}, 2, jf::UnitClass::AshenhornBoar);
        boar.stats.strength = 9;
        boar.stats.maxHp = 56;
        boar.currentHp = 28; // exactly 50%
        jf::Unit farAlly = makeUnit("far", jf::Team::Player, {0, 0});
        jf::BattleState battle({farAlly, boar});
        jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(battle.units()[1].bossEnraged);
        assert(battle.units()[1].stats.strength > 9);
        assert(battle.units()[1].chargeTelegraphed);
        assert(battle.units()[1].position.row == 0);
    }

    {
        // Before enrage, the boss does not stop after movement when the new
        // position enables an attack plan: it moves onto the target's row
        // and telegraphs a charge in the same action.
        jf::Unit target = makeUnit("target", jf::Team::Player, {0, 3});
        jf::Unit boar = makeUnit("boar", jf::Team::Enemy, {1, 7}, 2, jf::UnitClass::AshenhornBoar);
        boar.stats.strength = 9;
        jf::BattleState battle({target, boar});
        const jf::GridPos origin = battle.units()[1].position;
        jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(battle.units()[1].position != origin);
        assert(battle.units()[1].chargeTelegraphed);
        assert(battle.units()[1].hasActed);
    }

    {
        // Ashbough Verge vertical slice (docs/regions/ashbough_forest.md,
        // docs/implementation_roadmap.md Phase 2): a second region, fully
        // data-driven, reached through the exact same GameApp API as
        // Cinderwatch (no per-region GameApp methods). Normal route: 4
        // wolves and base reward (wood2/hide1). The regional route continues
        // to Herbwater Hollow instead of treating the first site as clear.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(app.startExpedition(jf::RegionId::AshboughForest));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        assert(app.screen() == jf::Screen::Battle);

        int wolfCount = 0;
        for (const jf::Unit& unit : app.battle().battle().units()) {
            if (unit.team == jf::Team::Enemy) {
                assert(unit.unitClass == jf::UnitClass::Wolf);
                ++wolfCount;
            }
        }
        assert(wolfCount == 4);

        winCurrentBattle(app);
        app.proceedToCamp();
        assert(app.screen() == jf::Screen::Camp);
        assert(!app.expeditionComplete());

        int wood = 0, hide = 0;
        for (const auto& loot : app.expedition().pendingLoot) {
            if (loot.id == "wood") wood = loot.quantity;
            if (loot.id == "hide") hide = loot.quantity;
        }
        assert(wood == 2 && hide == 1);

        app.returnToBase();
        assert(app.baseState().storageCount("wood") == 2);
        assert(app.baseState().storageCount("hide") == 1);
    }

    {
        // docs/region_mission_data_contract.md "二重付与防止": calling
        // proceedToCamp() again after it already transitioned to Camp must
        // be a no-op, not a second reward grant. inputState() stays Victory
        // forever once set, so this can only be guarded on GameApp's own
        // screen state - a bug that let this double-grant wood/hide and
        // battlesWon.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(app.startExpedition(jf::RegionId::AshboughForest));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        assert(app.screen() == jf::Screen::Camp);
        const int battlesWonAfterOnce = app.expedition().battlesWon;
        int woodAfterOnce = 0;
        for (const auto& loot : app.expedition().pendingLoot)
            if (loot.id == "wood") woodAfterOnce = loot.quantity;

        app.proceedToCamp(); // second call: must be a no-op
        assert(app.expedition().battlesWon == battlesWonAfterOnce);
        int woodAfterTwice = 0;
        for (const auto& loot : app.expedition().pendingLoot)
            if (loot.id == "wood") woodAfterTwice = loot.quantity;
        assert(woodAfterTwice == woodAfterOnce);
    }

    {
        // Rush route: one fewer wolf, HP-2 attrition, and the route's wood
        // delta (-2) fully cancels the base reward's wood.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(app.startExpedition(jf::RegionId::AshboughForest));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::CollapsedSidePath));

        int wolfCount = 0;
        for (const jf::Unit& unit : app.battle().battle().units()) {
            if (unit.team == jf::Team::Enemy) ++wolfCount;
            else assert(unit.currentHp == unit.stats.maxHp - 2);
        }
        assert(wolfCount == 3);

        winCurrentBattle(app);
        app.proceedToCamp();
        bool hasWood = false;
        for (const auto& loot : app.expedition().pendingLoot) hasWood |= loot.id == "wood";
        assert(!hasWood); // base 2 - delta 2 = 0, dropped entirely
    }

    {
        // Scout route: free deployment in the left 3 columns, and the
        // route's hide+1 delta.
        jf::GameData data = makeFactoryData();
        data.playerParty[0].classId = jf::UnitClass::FrontierScout;
        jf::GameApp app(data);
        assert(app.startExpedition(jf::RegionId::AshboughForest));
        assert(app.partyHasFrontierScout());
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::ScoutRoute));
        assert(app.screen() == jf::Screen::PreBattleDeployment);
        assert(app.placeDeploymentUnit(0, {0, 0}));
        assert(app.placeDeploymentUnit(1, {1, 0}));
        assert(app.placeDeploymentUnit(2, {1, 1}));
        assert(app.placeDeploymentUnit(3, {2, 1}));
        assert(app.confirmDeployment());
        assert(app.screen() == jf::Screen::Battle);

        winCurrentBattle(app);
        app.proceedToCamp();
        int wood = 0, hide = 0;
        for (const auto& loot : app.expedition().pendingLoot) {
            if (loot.id == "wood") wood = loot.quantity;
            if (loot.id == "hide") hide = loot.quantity;
        }
        assert(wood == 2 && hide == 2);
    }

    {
        // Survey secondary objective: completing it adds the wood+1 bonus
        // on top of the route's reward, without affecting expeditionComplete
        // or the primary EliminateTeam objective.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(app.startExpedition(jf::RegionId::AshboughForest));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));

        jf::BattleState& battle = app.battle().battle();
        const jf::ObjectiveDefinition* surveyDef = nullptr;
        for (const auto& def : battle.missionState().definitions) {
            if (def.kind == jf::ObjectiveKind::SecureTile) surveyDef = &def;
        }
        assert(surveyDef != nullptr);
        jf::GridPos surveyTile = surveyDef->target.tile;
        assert(jf::isPassable(battle.terrainAt(surveyTile)));
        assert(!battle.unitAt(surveyTile));

        const std::string surveyId = surveyDef->id;
        jf::Unit* scout = nullptr;
        for (jf::Unit& unit : battle.units()) {
            if (unit.team == jf::Team::Player) { scout = &unit; break; }
        }
        assert(scout != nullptr);
        scout->position = surveyTile; // teleport for the test - only the
                                       // objective-completion mechanism is
                                       // under test here, not pathing range
        app.battle().selectUnit(*scout);
        app.battle().selectMoveTile(surveyTile);
        app.battle().chooseWait();
        assert(battle.missionState().progress.at(surveyId).status == jf::ObjectiveStatus::Completed);

        winCurrentBattle(app);
        app.proceedToCamp();
        int wood = 0;
        for (const auto& loot : app.expedition().pendingLoot)
            if (loot.id == "wood") wood = loot.quantity;
        assert(wood == 3); // base 2 + survey bonus 1, FrontalAdvance has no route delta
    }

    {
        // docs/region_unlocks.md: Cinderwatch Gate (第2地域) starts locked,
        // Ashbough Forest (第1地域) starts unlocked. Winning and safely
        // returning from Ashbough Verge - the only location implemented so
        // far - must NOT unlock Cinderwatch Gate: a single cleared location
        // is not the region's real completion (regions/ashbough_forest.md
        // requires defeating 灰角大猪 and securing all 3 locations, which
        // don't exist in code until Phase 4). Only an explicit
        // completedRegionIds commit unlocks it.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(app.isRegionUnlocked(jf::RegionId::AshboughForest));
        assert(!app.isRegionUnlocked(jf::RegionId::CinderwatchGate));
        assert(!app.startExpedition(jf::RegionId::CinderwatchGate));
        assert(app.screen() == jf::Screen::Base); // rejected attempt leaves screen untouched

        auto summaries = app.regionSummaries();
        assert(summaries.size() == 2);
        bool sawAshboughUnlocked = false, sawCinderwatchLocked = false;
        for (const auto& summary : summaries) {
            if (summary.id == jf::RegionId::AshboughForest) sawAshboughUnlocked = summary.unlocked;
            if (summary.id == jf::RegionId::CinderwatchGate) sawCinderwatchLocked = !summary.unlocked;
        }
        assert(sawAshboughUnlocked && sawCinderwatchLocked);

        assert(app.startExpedition(jf::RegionId::AshboughForest));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        app.returnToBase();

        // Still locked: one site's Surveyed/Secured tier is not a region
        // completion.
        assert(!app.isRegionUnlocked(jf::RegionId::CinderwatchGate));
        assert(!app.startExpedition(jf::RegionId::CinderwatchGate));

        // Only an explicit completedRegionIds entry (the future Phase 4
        // region-complete safe return, simulated here via save data - same
        // mechanism startCinderwatchExpedition() uses) unlocks it.
        jf::SaveData save = app.createSaveData("en");
        save.base.completedRegionIds.insert(jf::RegionId::AshboughForest);
        assert(app.applySaveData(save));
        assert(app.isRegionUnlocked(jf::RegionId::CinderwatchGate));
        assert(app.startExpedition(jf::RegionId::CinderwatchGate));
        assert(app.screen() == jf::Screen::Exploration);
    }

    {
        // Phase 3 "周回・地域経路の開拓": completing the survey objective and
        // returning safely promotes the site to Secured; a defeated run
        // (or one where the survey wasn't completed) must not.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(app.startExpedition(jf::RegionId::AshboughForest));
        assert(app.currentSiteAccess() == jf::SiteAccessState::Unknown);
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));

        jf::BattleState& battle = app.battle().battle();
        const jf::ObjectiveDefinition* surveyDef = nullptr;
        for (const auto& def : battle.missionState().definitions)
            if (def.kind == jf::ObjectiveKind::SecureTile) surveyDef = &def;
        assert(surveyDef != nullptr);
        jf::GridPos surveyTile = surveyDef->target.tile;
        jf::Unit* scout = nullptr;
        for (jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Player) { scout = &unit; break; }
        assert(scout != nullptr);
        scout->position = surveyTile;
        app.battle().selectUnit(*scout);
        app.battle().selectMoveTile(surveyTile);
        app.battle().chooseWait();

        winCurrentBattle(app);
        app.proceedToCamp();
        // Still pending - not yet committed until a safe return.
        assert(app.currentSiteAccess() == jf::SiteAccessState::Unknown);
        app.returnToBase();

        assert(app.startExpedition(jf::RegionId::AshboughForest));
        assert(app.currentSiteAccess() == jf::SiteAccessState::Secured);
        // Normal exploration is no longer offered once Secured.
        assert(!app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
    }

    {
        // A defeated run must not promote the site, even if the survey
        // objective was completed mid-battle.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(app.startExpedition(jf::RegionId::AshboughForest));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        // Wait out every player unit to end Player Phase, then zero their HP
        // and let the first Enemy Phase update() notice allPlayersDefeated().
        for (jf::Unit& unit : app.battle().battle().units()) {
            if (unit.team != jf::Team::Player) continue;
            app.battle().selectUnit(unit);
            app.battle().selectMoveTile(unit.position);
            app.battle().chooseWait();
        }
        for (jf::Unit& unit : app.battle().battle().units())
            if (unit.team == jf::Team::Player) unit.currentHp = 0;
        app.battle().update(0.1f);
        assert(app.battle().inputState() == jf::BattleInputState::Defeat);
        app.acknowledgeDefeat();
        assert(app.startExpedition(jf::RegionId::AshboughForest));
        assert(app.currentSiteAccess() == jf::SiteAccessState::Unknown);
    }

    {
        // Once Secured: safe passage skips the battle outright (no loot, no
        // rewards), and reconnaissance re-fights for the ordinary base
        // reward only (no survey bonus even if re-completed).
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(app.startExpedition(jf::RegionId::AshboughForest));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        jf::BattleState& firstBattle = app.battle().battle();
        const jf::ObjectiveDefinition* firstSurveyDef = nullptr;
        for (const auto& def : firstBattle.missionState().definitions)
            if (def.kind == jf::ObjectiveKind::SecureTile) firstSurveyDef = &def;
        jf::GridPos surveyTile = firstSurveyDef->target.tile;
        jf::Unit* firstScout = nullptr;
        for (jf::Unit& unit : firstBattle.units())
            if (unit.team == jf::Team::Player) { firstScout = &unit; break; }
        firstScout->position = surveyTile;
        app.battle().selectUnit(*firstScout);
        app.battle().selectMoveTile(surveyTile);
        app.battle().chooseWait();
        winCurrentBattle(app);
        app.proceedToCamp();
        app.returnToBase();
        int woodAfterFirstRun = app.baseState().storageCount("wood");

        assert(app.startExpedition(jf::RegionId::AshboughForest));
        assert(app.currentSiteAccess() == jf::SiteAccessState::Secured);
        assert(app.chooseSafePassage());
        assert(app.screen() == jf::Screen::Camp);
        assert(app.expedition().pendingLoot.empty());
        assert(!app.expeditionComplete());
        app.returnToBase();
        assert(app.baseState().storageCount("wood") == woodAfterFirstRun); // no reward

        assert(app.startExpedition(jf::RegionId::AshboughForest));
        assert(app.chooseReconnaissance());
        assert(app.screen() == jf::Screen::Battle);
        winCurrentBattle(app);
        app.proceedToCamp();
        int reconWood = 0, reconHide = 0;
        for (const auto& loot : app.expedition().pendingLoot) {
            if (loot.id == "wood") reconWood = loot.quantity;
            if (loot.id == "hide") reconHide = loot.quantity;
        }
        assert(reconWood == 2 && reconHide == 1); // base reward only, no survey bonus
        app.returnToBase();
    }

    {
        // Phase 4 route vertical slice: victory at Ashbough Verge returns to
        // Camp, then Continue enters Herbwater Hollow Exploration. It must
        // not create the next battle directly, and HP/bag/pending loot must
        // survive both the transition and an Exploration checkpoint reload.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        // addPreparedItem() now only draws from owned storage (crafted via
        // craftItem(), consuming materials) rather than creating items for
        // free - seed one owned FieldTreatmentKit directly since this test
        // isn't about the craft/material path itself.
        jf::SaveData ownedItem = app.createSaveData("ja");
        ownedItem.base.itemStorage[jf::ItemType::FieldTreatmentKit] = 1;
        assert(app.applySaveData(ownedItem));
        assert(app.addPreparedItem(jf::ItemType::FieldTreatmentKit));
        assert(app.startExpedition(jf::RegionId::AshboughForest));
        assert(app.expedition().routeProgress);
        assert(app.expedition().routeProgress->currentNodeId == "ashbough_verge");
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        jf::Unit* wounded = app.battle().battle().findUnit("player0");
        assert(wounded);
        wounded->currentHp = 7;
        winCurrentBattle(app);
        app.proceedToCamp();
        assert(app.expedition().battlesWon == 1);
        assert(!app.expedition().pendingLoot.empty());
        const auto pendingBefore = app.expedition().pendingLoot;
        const auto bagBefore = app.expedition().bag;

        app.continueExpedition();
        assert(app.screen() == jf::Screen::Exploration);
        assert(app.currentMissionNameJa() == "薬草の沢");
        // docs/regions/ashbough_forest.md "2. 薬草の沢" is now implemented -
        // arriving at its Exploration screen (before picking a route) must
        // not by itself change any pending/bag state from the previous site.
        assert(app.currentSiteContentImplemented());
        assert(app.expedition().pendingLoot.size() == pendingBefore.size());
        for (std::size_t i = 0; i < pendingBefore.size(); ++i) {
            assert(app.expedition().pendingLoot[i].id == pendingBefore[i].id);
            assert(app.expedition().pendingLoot[i].quantity == pendingBefore[i].quantity);
        }
        assert(app.expedition().bag == bagBefore);

        jf::SaveData saved = app.createSaveData("ja");
        assert(saved.expedition && saved.expedition->routeProgress);
        assert(saved.expedition->routeProgress->currentNodeId == "herbwater_hollow");
        auto hp = std::find_if(saved.expedition->partyUnits.begin(), saved.expedition->partyUnits.end(),
                               [](const auto& unit) { return unit.id == "player0"; });
        assert(hp != saved.expedition->partyUnits.end() && hp->currentHp == 7);
        saved.base.siteAccess[jf::siteAccessKey(jf::RegionId::AshboughForest, "herbwater_hollow")] =
            jf::SiteAccessState::Secured;

        jf::GameApp restored(data);
        assert(restored.applySaveData(saved));
        assert(restored.screen() == jf::Screen::Exploration);
        assert(restored.currentMissionNameJa() == "薬草の沢");
        jf::SaveData resaved = restored.createSaveData("ja");
        auto restoredHp = std::find_if(resaved.expedition->partyUnits.begin(), resaved.expedition->partyUnits.end(),
                                       [](const auto& unit) { return unit.id == "player0"; });
        assert(restoredHp != resaved.expedition->partyUnits.end() && restoredHp->currentHp == 7);
        assert(restored.chooseSafePassage());
        assert(restored.battle().battle().findUnit("player0")->currentHp == 7);
    }

    {
        // docs/item_system.md: craftItem() consumes materials all-or-nothing
        // and caps owned storage at 99; addPreparedItem()/removePreparedItem()
        // move items between owned storage and the prepared bag without
        // creating or destroying any.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(!app.craftItem(jf::ItemType::FieldTreatmentKit)); // no materials yet
        assert(app.baseState().ownedItemCount(jf::ItemType::FieldTreatmentKit) == 0);

        jf::SaveData materials = app.createSaveData("ja");
        materials.base.addStorage("herb", 1);
        materials.base.addStorage("wood", 1);
        assert(app.applySaveData(materials));
        assert(app.craftItem(jf::ItemType::FieldTreatmentKit));
        assert(app.baseState().ownedItemCount(jf::ItemType::FieldTreatmentKit) == 1);
        assert(app.baseState().storageCount("herb") == 0 && app.baseState().storageCount("wood") == 0);
        assert(!app.craftItem(jf::ItemType::FieldTreatmentKit)); // materials spent, can't afford a second

        assert(app.addPreparedItem(jf::ItemType::FieldTreatmentKit));
        assert(app.baseState().ownedItemCount(jf::ItemType::FieldTreatmentKit) == 0); // moved into the bag
        assert(!app.addPreparedItem(jf::ItemType::FieldTreatmentKit)); // none left owned

        app.removePreparedItem(0);
        assert(app.baseState().ownedItemCount(jf::ItemType::FieldTreatmentKit) == 1); // refunded, not destroyed
        assert(app.preparedBag().empty());
    }

    {
        // Unused prepared items survive an entire expedition and return to
        // owned storage on ANY exit path (safe return here; defeat/retire
        // share the same resetToBase() code path), matching item_system.md
        // "未使用消耗品は帰還・敗北のどちらでも倉庫へ戻る".
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        jf::SaveData materials = app.createSaveData("ja");
        materials.base.itemStorage[jf::ItemType::FieldTreatmentKit] = 2;
        assert(app.applySaveData(materials));
        assert(app.addPreparedItem(jf::ItemType::FieldTreatmentKit));
        assert(app.addPreparedItem(jf::ItemType::FieldTreatmentKit));
        assert(app.baseState().ownedItemCount(jf::ItemType::FieldTreatmentKit) == 0);

        assert(app.startExpedition(jf::RegionId::AshboughForest));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        // Consume exactly one of the two prepared kits mid-expedition.
        auto& units = app.battle().battle().units();
        jf::Unit* wounded = nullptr;
        for (jf::Unit& unit : units)
            if (unit.team == jf::Team::Player) { wounded = &unit; wounded->currentHp = 1; break; }
        assert(wounded);
        assert(app.useCampItem(jf::ItemType::FieldTreatmentKit, wounded->id));
        assert(app.expedition().bag.size() == 1); // one consumed, one still unused

        app.returnToBase();
        app.acknowledgeLootSecured();
        assert(app.baseState().ownedItemCount(jf::ItemType::FieldTreatmentKit) == 1); // the unused one came back
    }

    {
        // M3-A "更新後の復旧" 優先順位4: a checkpoint whose routeProgress
        // points at a node id that no longer exists (e.g. a later content
        // update renamed/removed it) must not be silently discarded -
        // Pending loot/bag/HP survive and the expedition falls back to the
        // region's entrance instead of vanishing outright.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        jf::SaveData ownedItem = app.createSaveData("ja");
        ownedItem.base.itemStorage[jf::ItemType::FieldTreatmentKit] = 1;
        assert(app.applySaveData(ownedItem));
        assert(app.addPreparedItem(jf::ItemType::FieldTreatmentKit));
        assert(app.startExpedition(jf::RegionId::AshboughForest));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        jf::Unit* wounded = app.battle().battle().findUnit("player0");
        assert(wounded);
        wounded->currentHp = 9;
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition();
        assert(app.screen() == jf::Screen::Exploration);

        jf::SaveData saved = app.createSaveData("ja");
        assert(saved.expedition && saved.expedition->routeProgress);
        assert(!saved.expedition->pendingLoot.empty());
        const auto pendingBefore = saved.expedition->pendingLoot;
        const auto bagBefore = saved.expedition->bag;
        saved.expedition->routeProgress->currentNodeId = "no_longer_exists";

        jf::GameApp restored(data);
        assert(restored.applySaveData(saved));
        assert(restored.screen() == jf::Screen::Exploration);
        assert(restored.currentMissionNameJa() == "灰枝の林縁"); // region entrance
        assert(restored.expedition().pendingLoot.size() == pendingBefore.size());
        for (std::size_t i = 0; i < pendingBefore.size(); ++i) {
            assert(restored.expedition().pendingLoot[i].id == pendingBefore[i].id);
            assert(restored.expedition().pendingLoot[i].quantity == pendingBefore[i].quantity);
        }
        assert(restored.expedition().bag == bagBefore);
        jf::SaveData resaved = restored.createSaveData("ja");
        assert(resaved.expedition);
        auto restoredHp = std::find_if(resaved.expedition->partyUnits.begin(), resaved.expedition->partyUnits.end(),
                                       [](const auto& unit) { return unit.id == "player0"; });
        assert(restoredHp != resaved.expedition->partyUnits.end() && restoredHp->currentHp == 9);
    }

    {
        // Same recovery path, triggered via a corrupted legacy stageIndex
        // (out of range) on Cinderwatch Gate instead of a bad route node id.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(startCinderwatchExpedition(app));
        jf::SaveData saved = app.createSaveData("ja");
        assert(saved.expedition);
        saved.expedition->expeditionStage = 999;

        jf::GameApp restored(data);
        assert(restored.applySaveData(saved));
        assert(restored.screen() == jf::Screen::Exploration);
    }

    {
        // Safe passage resolves a secured site without rewards, victory
        // count, or healing. The following site is still Exploration.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        jf::SaveData setup = app.createSaveData("ja");
        setup.base.siteAccess[jf::siteAccessKey(jf::RegionId::AshboughForest, "ashbough_verge")] =
            jf::SiteAccessState::Secured;
        setup.base.siteAccess[jf::siteAccessKey(jf::RegionId::AshboughForest, "herbwater_hollow")] =
            jf::SiteAccessState::Secured;
        assert(app.applySaveData(setup));
        assert(app.startExpedition(jf::RegionId::AshboughForest));
        assert(app.chooseSafePassage());
        assert(app.expedition().battlesWon == 0);
        assert(app.expedition().pendingLoot.empty());
        jf::Unit* wounded = app.battle().battle().findUnit("player0");
        assert(wounded);
        wounded->currentHp = 6;
        app.continueExpedition();
        assert(app.screen() == jf::Screen::Exploration);
        assert(app.currentMissionNameJa() == "薬草の沢");
        assert(app.chooseSafePassage());
        assert(app.expedition().battlesWon == 0);
        assert(app.expedition().pendingLoot.empty());
        assert(app.battle().battle().findUnit("player0")->currentHp == 6);
    }

    {
        // M2-D "周回短縮": bulkPassSecuredSites() skips every consecutive
        // Secured site in one call, stopping at the first non-Secured one -
        // no reward, battlesWon still counts each site passed. HP
        // preservation itself reuses chooseSafePassage()'s own construction
        // path (already covered by the test above) rather than being
        // re-verified here.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        jf::SaveData setup = app.createSaveData("ja");
        setup.base.siteAccess[jf::siteAccessKey(jf::RegionId::AshboughForest, "ashbough_verge")] =
            jf::SiteAccessState::Secured;
        setup.base.siteAccess[jf::siteAccessKey(jf::RegionId::AshboughForest, "herbwater_hollow")] =
            jf::SiteAccessState::Secured;
        assert(app.applySaveData(setup));
        assert(app.startExpedition(jf::RegionId::AshboughForest));

        int passed = app.bulkPassSecuredSites();
        assert(passed == 2);
        assert(app.screen() == jf::Screen::Exploration); // stopped at brokenwood_territory
        assert(app.currentMissionNameJa() == "折れ木の縄張り");
        assert(app.expedition().battlesWon == 2);
        assert(app.expedition().pendingLoot.empty());
    }

    {
        // A no-op (returns 0, screen/state untouched) when the current site
        // isn't Secured yet.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(app.startExpedition(jf::RegionId::AshboughForest));
        assert(app.bulkPassSecuredSites() == 0);
        assert(app.screen() == jf::Screen::Exploration);
        assert(app.expedition().battlesWon == 0);
    }

    {
        // Synthetic (灰角大猪 has no survey objective, so brokenwood_territory
        // can never actually reach Secured through real play): if every site
        // in the region were Secured, bulk-passing all the way to the Exit
        // lands on Camp with the expedition already complete.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        jf::SaveData setup = app.createSaveData("ja");
        setup.base.siteAccess[jf::siteAccessKey(jf::RegionId::AshboughForest, "ashbough_verge")] =
            jf::SiteAccessState::Secured;
        setup.base.siteAccess[jf::siteAccessKey(jf::RegionId::AshboughForest, "herbwater_hollow")] =
            jf::SiteAccessState::Secured;
        setup.base.siteAccess[jf::siteAccessKey(jf::RegionId::AshboughForest, "brokenwood_territory")] =
            jf::SiteAccessState::Secured;
        assert(app.applySaveData(setup));
        assert(app.startExpedition(jf::RegionId::AshboughForest));
        int passed = app.bulkPassSecuredSites();
        assert(passed == 3);
        assert(app.screen() == jf::Screen::Camp);
        assert(app.expeditionComplete());
        assert(app.expedition().pendingLoot.empty());
    }

    {
        // 薬草の沢, そのまま通過: all-wolf roster (exact headcount is a
        // tunable balance value, checked against the live region data
        // instead of a hardcoded number), no attrition, base reward 木材1.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        const jf::RegionDescriptor forestRegion = jf::regionDescriptor(jf::RegionId::AshboughForest, data);
        const auto& herbwaterStage = forestRegion.stages[1];
        reachHerbwaterHollow(app);
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        int wolfCount = 0;
        for (const jf::Unit& unit : app.battle().battle().units()) {
            if (unit.team == jf::Team::Enemy) {
                assert(unit.unitClass == jf::UnitClass::Wolf);
                ++wolfCount;
            }
        }
        assert(wolfCount == static_cast<int>(herbwaterStage.enemyRoster.size()));
        winCurrentBattle(app);
        app.proceedToCamp();
        int wood = 0;
        for (const auto& loot : app.expedition().pendingLoot)
            if (loot.id == "wood") wood = loot.quantity;
        assert(wood == 1);
    }

    {
        // 薬草の沢, 薬草を採取: same all-wolf roster size as the other route
        // (no attrition/removal on this route), reward adds 薬草2 on top of
        // the base 木材1.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        const jf::RegionDescriptor forestRegion = jf::regionDescriptor(jf::RegionId::AshboughForest, data);
        const auto& herbwaterStage = forestRegion.stages[1];
        reachHerbwaterHollow(app);
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::CollapsedSidePath));
        int wolfCount = 0;
        for (const jf::Unit& unit : app.battle().battle().units())
            if (unit.team == jf::Team::Enemy) ++wolfCount;
        assert(wolfCount == static_cast<int>(herbwaterStage.enemyRoster.size()));
        winCurrentBattle(app);
        app.proceedToCamp();
        int wood = 0, herb = 0;
        for (const auto& loot : app.expedition().pendingLoot) {
            if (loot.id == "wood") wood = loot.quantity;
            if (loot.id == "herb") herb = loot.quantity;
        }
        assert(wood == 1 && herb == 2);
    }

    {
        // 薬草の沢, [暁の衛生兵]薬草を選別: gated on a Dawn Chirurgeon in the
        // party (not Frontier Scout, since this stage overrides
        // scoutRouteRequiredClass); without one, the route is rejected.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        reachHerbwaterHollow(app);
        assert(!app.chooseExplorationRoute(jf::ExplorationChoice::ScoutRoute));
    }

    {
        // With a Dawn Chirurgeon: route succeeds, party is auto-placed (not
        // freely player-placed) but confined to the left 2 columns (0-1)
        // instead of the usual 3, and the reward is 高品質薬草1.
        jf::GameData data = makeChirurgeonFactoryData();
        jf::GameApp app(data);
        reachHerbwaterHollow(app);
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::ScoutRoute));
        assert(app.screen() == jf::Screen::Battle); // auto-placed, not PreBattleDeployment
        for (const jf::Unit& unit : app.battle().battle().units()) {
            if (unit.team == jf::Team::Player) assert(unit.position.col <= 1);
        }
        winCurrentBattle(app);
        app.proceedToCamp();
        int qualityHerb = 0;
        for (const auto& loot : app.expedition().pendingLoot)
            if (loot.id == "quality_herb") qualityHerb = loot.quantity;
        assert(qualityHerb == 1);
    }

    {
        // 共通副目標「薬草地点確保」: ending a turn on either generated
        // HerbPatch tile, then winning, adds 薬草+1 on top of the route's
        // own reward - regardless of which of the 2 tiles was used.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        reachHerbwaterHollow(app);
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));

        jf::BattleState& battle = app.battle().battle();
        const jf::ObjectiveDefinition* herbDef = nullptr;
        for (const auto& def : battle.missionState().definitions) {
            if (def.kind == jf::ObjectiveKind::SecureTile) herbDef = &def;
        }
        assert(herbDef != nullptr);
        jf::GridPos herbTile = herbDef->target.tile;
        assert(herbTile.col >= 2 && herbTile.col <= 5);

        jf::Unit* mover = nullptr;
        for (jf::Unit& unit : battle.units()) {
            if (unit.team == jf::Team::Player) { mover = &unit; break; }
        }
        assert(mover != nullptr);
        mover->position = herbTile;
        app.battle().selectUnit(*mover);
        app.battle().selectMoveTile(herbTile);
        app.battle().chooseWait();
        assert(battle.missionState().progress.at(herbDef->id).status == jf::ObjectiveStatus::Completed);

        winCurrentBattle(app);
        app.proceedToCamp();
        int herb = 0;
        for (const auto& loot : app.expedition().pendingLoot)
            if (loot.id == "herb") herb = loot.quantity;
        assert(herb == 1); // base has no herb; this is purely the secondary bonus
    }

    {
        // Raw terrain generation (no units - HerbPatch placement happens
        // later in BattleFactory::assembleScenario(), after units are
        // placed, so it can avoid spawning tiles): no Barrier/Rubble/
        // WatchPost, and Shallows costs 2 and is passable.
        const jf::GameData data = makeFactoryData();
        for (std::uint32_t seed = 0; seed < 25; ++seed) {
            auto terrain = jf::generateFieldTerrain(
                data.terrainProfile(jf::kHerbwaterHollowTerrain), seed);
            for (jf::TerrainType tile : terrain) {
                assert(tile != jf::TerrainType::Barrier && tile != jf::TerrainType::Rubble &&
                      tile != jf::TerrainType::WatchPost);
            }
        }
        assert(jf::movementCost(jf::TerrainType::Shallows) == 2);
        assert(jf::isPassable(jf::TerrainType::Shallows));
    }

    {
        // Assembled-battle level: exactly 2 HerbPatch tiles, always in the
        // center 4 columns (2-5) and never on a spawned unit's tile, across
        // many starts of the same site.
        for (int trial = 0; trial < 25; ++trial) {
            jf::GameData data = makeFactoryData();
            jf::GameApp app(data);
            reachHerbwaterHollow(app);
            assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
            const jf::BattleState& battle = app.battle().battle();
            int herbCount = 0;
            for (int row = 0; row < jf::kGridRows; ++row) {
                for (int col = 0; col < jf::kGridCols; ++col) {
                    jf::GridPos pos{row, col};
                    if (battle.terrainAt(pos) == jf::TerrainType::HerbPatch) {
                        ++herbCount;
                        assert(col >= 2 && col <= 5);
                        assert(!battle.unitAt(pos));
                    }
                }
            }
            assert(herbCount == 2);
        }
    }

    {
        // 折れ木の縄張り, 慎重に縄張りへ入る (route A): 1 log, the boss and
        // its escort wolves (exact escort headcount is a tunable balance
        // value, checked against the live region data), base reward + route
        // A's wood+1.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        const jf::RegionDescriptor forestRegion = jf::regionDescriptor(jf::RegionId::AshboughForest, data);
        const auto& brokenwoodStage = forestRegion.stages[2];
        const int expectedWolfCount = static_cast<int>(brokenwoodStage.enemyRoster.size()) - 1; // minus the boar
        reachBrokenwoodTerritory(app);
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        jf::BattleState& battle = app.battle().battle();
        int enemyCount = 0, boarCount = 0, wolfCount = 0, logCount = 0;
        for (const jf::Unit& unit : battle.units()) {
            if (unit.team == jf::Team::Enemy) {
                ++enemyCount;
                if (unit.unitClass == jf::UnitClass::AshenhornBoar) ++boarCount;
                if (unit.unitClass == jf::UnitClass::Wolf) ++wolfCount;
            }
        }
        for (const jf::BattleObjectState& object : battle.objects()) {
            if (object.definitionId == "fallen_log") ++logCount;
        }
        assert(boarCount == 1 && wolfCount == expectedWolfCount &&
               enemyCount == static_cast<int>(brokenwoodStage.enemyRoster.size()));
        assert(logCount == 1);

        // Defeat the boss outright (bypassing its AI) and win normally.
        for (jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Enemy) unit.currentHp = 0;
        winCurrentBattle(app);
        app.proceedToCamp();
        int wood = 0, hide = 0, fang = 0;
        for (const auto& loot : app.expedition().pendingLoot) {
            if (loot.id == "wood") wood = loot.quantity;
            if (loot.id == "hide") hide = loot.quantity;
            if (loot.id == "ashenhorn_fang") fang = loot.quantity;
        }
        assert(wood == 3 && hide == 2 + 1 && fang == 1); // base(2)+A(1)=3 wood; base hide2 + no-casualties bonus 1
    }

    {
        // 折れ木の縄張り, 倒木を戦場へ誘導する (route B): 2 logs, no extra wood.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        reachBrokenwoodTerritory(app);
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::CollapsedSidePath));
        jf::BattleState& battle = app.battle().battle();
        int logCount = 0;
        for (const jf::BattleObjectState& object : battle.objects())
            if (object.definitionId == "fallen_log") ++logCount;
        assert(logCount == 2);

        for (jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Enemy) unit.currentHp = 0;
        winCurrentBattle(app);
        app.proceedToCamp();
        int wood = 0;
        for (const auto& loot : app.expedition().pendingLoot)
            if (loot.id == "wood") wood = loot.quantity;
        assert(wood == 2); // base only, no route delta on B
    }

    {
        // Route C ([辺境猟兵]獣の痕跡を追う) is disabled outright for this
        // stage - deferred per the doc's own text, regardless of party.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        reachBrokenwoodTerritory(app);
        assert(!app.chooseExplorationRoute(jf::ExplorationChoice::ScoutRoute));
    }

    {
        // 副目標「倒木衝突」: colliding the boss into a log during the battle
        // adds 灰角の欠片1, on top of the normal victory reward.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        reachBrokenwoodTerritory(app);
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        jf::BattleState& battle = app.battle().battle();
        jf::Unit* boss = nullptr;
        for (jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Enemy) { boss = &unit; break; }
        assert(boss != nullptr);
        const jf::BattleObjectState* log = nullptr;
        for (const jf::BattleObjectState& object : battle.objects())
            if (object.definitionId == "fallen_log") { log = &object; break; }
        assert(log != nullptr);
        // Force the boss directly onto the log's tile and mark the
        // collision as if a charge had just resolved there - exercises the
        // reward wiring without depending on the AI actually pathing there.
        boss->position = log->position;
        battle.markBossCollidedWithBarrier();
        boss->currentHp = 0;

        winCurrentBattle(app);
        app.proceedToCamp();
        int fragment = 0;
        for (const auto& loot : app.expedition().pendingLoot)
            if (loot.id == "ashenhorn_fragment") fragment = loot.quantity;
        assert(fragment == 1);
    }

    {
        // 副目標「無傷」: if a party member was incapacitated during the
        // battle, the no-casualties bonus (獣皮+1) is withheld.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        reachBrokenwoodTerritory(app);
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        jf::BattleState& battle = app.battle().battle();
        for (jf::Unit& unit : battle.units()) {
            if (unit.team == jf::Team::Player) { unit.currentHp = 0; break; } // one casualty
        }
        for (jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Enemy) unit.currentHp = 0;
        winCurrentBattle(app);
        app.proceedToCamp();
        int hide = 0;
        for (const auto& loot : app.expedition().pendingLoot)
            if (loot.id == "hide") hide = loot.quantity;
        assert(hide == 2); // base only, no-casualties bonus withheld
    }

    {
        // Terrain generation: no Barrier/Rubble/WatchPost for 折れ木の縄張り.
        const jf::GameData data = makeFactoryData();
        for (std::uint32_t seed = 0; seed < 25; ++seed) {
            auto terrain = jf::generateFieldTerrain(
                data.terrainProfile(jf::kBrokenwoodTerritoryTerrain), seed);
            for (jf::TerrainType tile : terrain) {
                assert(tile != jf::TerrainType::Barrier && tile != jf::TerrainType::Rubble &&
                      tile != jf::TerrainType::WatchPost);
            }
        }
    }

    {
        // Assembled-battle level: the fallen log(s) are always in the center
        // 4 columns (2-5) and never share the boss's row.
        for (int trial = 0; trial < 25; ++trial) {
            jf::GameData data = makeFactoryData();
            jf::GameApp app(data);
            reachBrokenwoodTerritory(app);
            assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
            const jf::BattleState& battle = app.battle().battle();
            const jf::Unit* boss = nullptr;
            for (const jf::Unit& unit : battle.units())
                if (unit.team == jf::Team::Enemy) { boss = &unit; break; }
            assert(boss != nullptr);
            for (const jf::BattleObjectState& object : battle.objects()) {
                if (object.definitionId != "fallen_log") continue;
                assert(object.position.col >= 2 && object.position.col <= 5);
                assert(object.position.row != boss->position.row);
            }
        }
    }

    {
        // Camp decision-support (docs/campaign_balance.md "情報と安全路を持ち
        // 帰る正規の進行にする"): nextSiteEnemyRosterNames() previews the next
        // site's roster (checked against the live region data, not a
        // hardcoded count/list, per the earlier lesson about hardcoding
        // tunable roster sizes) and returns nullopt once there is no site
        // left ahead.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        const jf::RegionDescriptor forestRegion = jf::regionDescriptor(jf::RegionId::AshboughForest, data);
        assert(app.startExpedition(jf::RegionId::AshboughForest));
        auto roster = app.nextSiteEnemyRosterNames();
        assert(roster && roster->size() == forestRegion.stages[1].enemyRoster.size());

        // Winning ashbough_verge and reaching Camp doesn't itself advance the
        // route position (only continueExpedition() does), so the preview
        // still points at herbwater_hollow right after proceedToCamp().
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        roster = app.nextSiteEnemyRosterNames();
        assert(roster && roster->size() == forestRegion.stages[1].enemyRoster.size());

        app.continueExpedition(); // now at herbwater_hollow's Exploration
        roster = app.nextSiteEnemyRosterNames();
        assert(roster && roster->size() == forestRegion.stages[2].enemyRoster.size());

        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition(); // now at brokenwood_territory's Exploration, the last site
        assert(!app.nextSiteEnemyRosterNames());
    }

    {
        // Region-route battles preserve current HP between sites. Starting
        // the next exploration choice must not instantiate a fresh party.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(app.startExpedition(jf::RegionId::AshboughForest));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        jf::Unit* wounded = nullptr;
        for (jf::Unit& unit : app.battle().battle().units()) {
            if (unit.team == jf::Team::Player && !wounded) {
                unit.currentHp = std::max(1, unit.stats.maxHp - 7);
                wounded = &unit;
            } else if (unit.team == jf::Team::Enemy) {
                unit.currentHp = 0;
            }
        }
        assert(wounded != nullptr);
        const std::string woundedId = wounded->id;
        const int carriedHp = wounded->currentHp;
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition();
        assert(app.screen() == jf::Screen::Exploration);
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        const jf::Unit* continued = app.battle().battle().findUnit(woundedId);
        assert(continued != nullptr);
        assert(continued->currentHp == carriedHp);
    }

    {
        // M2-C 地域完了: winning all 3 sites (灰枝の林縁, 薬草の沢, 折れ木の縄張り)
        // queues the region completion Discovery on the win that completes
        // the last one, and safe return commits it - unlocking Cinderwatch
        // Gate (region_unlocks.md) and satisfying Ashbough Forest's own
        // outpost-advancement eligibility (fang + wood>=3).
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(!app.isRegionUnlocked(jf::RegionId::CinderwatchGate));

        assert(app.startExpedition(jf::RegionId::AshboughForest));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)); // 灰枝の林縁
        winCurrentBattle(app);
        app.proceedToCamp();
        assert(app.baseState().discoveryRegistry.count(jf::kAshboughForestSurveyCompleteDiscovery) == 0);

        app.continueExpedition();
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)); // 薬草の沢
        winCurrentBattle(app);
        app.proceedToCamp();
        // 2 of 3 sites done - not complete yet.
        assert(std::none_of(app.expedition().pendingDiscoveries.begin(), app.expedition().pendingDiscoveries.end(),
                            [](const std::string& id) { return id == jf::kAshboughForestSurveyCompleteDiscovery; }));

        app.continueExpedition();
        assert(app.currentMissionNameJa() == "折れ木の縄張り");
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        for (jf::Unit& unit : app.battle().battle().units())
            if (unit.team == jf::Team::Enemy) unit.currentHp = 0;
        winCurrentBattle(app);
        app.proceedToCamp();
        // The win that completes the last site queues the Discovery immediately.
        assert(std::find(app.expedition().pendingDiscoveries.begin(), app.expedition().pendingDiscoveries.end(),
                         jf::kAshboughForestSurveyCompleteDiscovery) != app.expedition().pendingDiscoveries.end());
        assert(app.baseState().completedRegionIds.count(jf::RegionId::AshboughForest) == 0); // still pending

        app.returnToBase();
        assert(app.baseState().discoveryRegistry.count(jf::kAshboughForestSurveyCompleteDiscovery) == 1);
        assert(app.baseState().completedRegionIds.count(jf::RegionId::AshboughForest) == 1);
        assert(app.isRegionUnlocked(jf::RegionId::CinderwatchGate));
        assert(app.startExpedition(jf::RegionId::CinderwatchGate));
        app.retireExpedition();

        // Outpost advancement: fang(1) + wood(2 base + 2 base + 1 routeA + ...
        // whatever accumulated) >= 3 should already be eligible.
        assert(app.baseState().storageCount(jf::kAshenhornFangMaterial) >= 1);
        assert(jf::eligibleForOutpostStage(app.baseState(), jf::OutpostStage::PioneerOutpost));
    }

    {
        // A defeat at the final site must not commit the region completion,
        // even though the win condition would otherwise have been met -
        // matches "敗北した遠征の踏査・工作は恒久化しない".
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(app.startExpedition(jf::RegionId::AshboughForest));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        app.returnToBase();

        // Re-enter and clear the remaining 2 sites, but retire (abandon)
        // right after the region-completing win instead of returning home.
        assert(app.startExpedition(jf::RegionId::AshboughForest));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)); // Surveyed, not
                                                                                   // Secured - still fights normally
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition();
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition();
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        for (jf::Unit& unit : app.battle().battle().units())
            if (unit.team == jf::Team::Enemy) unit.currentHp = 0;
        winCurrentBattle(app);
        app.proceedToCamp(); // region-completion queued as pending here
        app.retireExpedition(); // abandon instead of returning home

        assert(app.baseState().completedRegionIds.count(jf::RegionId::AshboughForest) == 0);
        assert(app.baseState().discoveryRegistry.count(jf::kAshboughForestSurveyCompleteDiscovery) == 0);
        assert(!app.isRegionUnlocked(jf::RegionId::CinderwatchGate));
    }

    {
        std::string graphError;
        const jf::RegionRouteGraph& graph = jf::regionRouteGraph(jf::RegionId::AshboughForest);
        assert(jf::validateRouteGraph(graph, &graphError));
        assert(graph.nodes.size() == 6);
        assert(graph.entranceNodeId == "ashbough_entrance");
        assert(graph.exitNodeId == "ashbough_exit");
    }

    {
        // docs/battle_objects.md "Definition検証": each invalid combination is
        // rejected with a specific message; a well-formed Definition passes.
        jf::BattleObjectDefinition occupyAndBlock;
        occupyAndBlock.definitionId = "bad_occupy_block";
        occupyAndBlock.canOccupy = true;
        occupyAndBlock.blocksMovement = true;
        std::vector<std::string> errors;
        assert(!jf::validateObjectDefinition(occupyAndBlock, &errors));
        assert(!errors.empty());

        jf::BattleObjectDefinition zeroDurabilityAttackable;
        zeroDurabilityAttackable.definitionId = "bad_zero_durability_attackable";
        zeroDurabilityAttackable.maxDurability = 0;
        zeroDurabilityAttackable.canBeAttacked = true;
        errors.clear();
        assert(!jf::validateObjectDefinition(zeroDurabilityAttackable, &errors));

        jf::BattleObjectDefinition zeroDurabilityRepairable;
        zeroDurabilityRepairable.definitionId = "bad_zero_durability_repairable";
        zeroDurabilityRepairable.maxDurability = 0;
        zeroDurabilityRepairable.canBeRepaired = true;
        errors.clear();
        assert(!jf::validateObjectDefinition(zeroDurabilityRepairable, &errors));

        jf::BattleObjectDefinition wellFormed;
        wellFormed.definitionId = "fallen_tree";
        wellFormed.kind = jf::BattleObjectKind::Barrier;
        wellFormed.maxDurability = 6;
        wellFormed.canBeAttacked = true;
        wellFormed.blocksMovement = true;
        errors.clear();
        assert(jf::validateObjectDefinition(wellFormed, &errors));
        assert(errors.empty());
    }

    {
        // BattleState registration/placement failure modes: duplicate
        // definition id, unknown definition id, out-of-bounds position, and
        // two objects sharing a tile are all rejected.
        jf::Unit player = makeUnit("player", jf::Team::Player, {1, 0});
        jf::BattleState battle({player});

        jf::BattleObjectDefinition barrier;
        barrier.definitionId = "fallen_tree";
        barrier.kind = jf::BattleObjectKind::Barrier;
        barrier.maxDurability = 6;
        barrier.canBeAttacked = true;
        barrier.blocksMovement = true;
        assert(battle.registerObjectDefinition(barrier));
        assert(!battle.registerObjectDefinition(barrier)); // duplicate id

        assert(!battle.placeObject({"tree1", "no_such_definition", {1, 3}}));
        assert(!battle.placeObject({"tree1", "fallen_tree", {-1, -1}})); // out of bounds
        assert(battle.placeObject({"tree1", "fallen_tree", {1, 3}}));
        assert(!battle.placeObject({"tree2", "fallen_tree", {1, 3}})); // tile already occupied
    }

    {
        // 倒木 (Barrier): blocks movement/path expansion while Active, becomes
        // passable the instant it's Destroyed, and durability never drops
        // below 0 or gets destroyed a second time.
        jf::Unit mover = makeUnit("mover", jf::Team::Player, {1, 0}, /*move=*/6);
        jf::Unit attacker = makeUnit("attacker", jf::Team::Player, {1, 0});
        attacker.stats.strength = 5;
        attacker.weapon.might = 3; // 8 power, 0 defense -> 8 damage per hit
        jf::BattleState battle({mover, attacker});

        jf::BattleObjectDefinition barrier;
        barrier.definitionId = "fallen_tree";
        barrier.kind = jf::BattleObjectKind::Barrier;
        barrier.maxDurability = 10;
        barrier.canBeAttacked = true;
        barrier.blocksMovement = true;
        assert(battle.registerObjectDefinition(barrier));
        assert(battle.placeObject({"tree1", "fallen_tree", {1, 2}}));

        assert(battle.objectBlocksMovementAt({1, 2}));
        auto reachableBefore = jf::computeReachableTiles(battle, battle.units()[0]);
        assert(std::find(reachableBefore.begin(), reachableBefore.end(), jf::GridPos{1, 2}) ==
               reachableBefore.end());

        jf::BattleObjectState* tree = battle.findObject("tree1");
        assert(tree != nullptr);
        assert(!jf::resolveObjectAttack(battle, battle.units()[1], *tree)); // 8 dmg: 10 -> 2, not destroyed yet
        assert(tree->durability == 2);
        assert(tree->state == jf::BattleObjectStateKind::Active);
        assert(battle.objectBlocksMovementAt({1, 2}));

        assert(jf::resolveObjectAttack(battle, battle.units()[1], *tree)); // 8 dmg floors at 0 -> destroyed now
        assert(tree->durability == 0);
        assert(tree->state == jf::BattleObjectStateKind::Destroyed);
        assert(!battle.objectBlocksMovementAt({1, 2})); // passable the instant it's destroyed

        assert(!jf::resolveObjectAttack(battle, battle.units()[1], *tree)); // already destroyed: no-op, no re-fire

        auto reachableAfter = jf::computeReachableTiles(battle, battle.units()[0]);
        assert(std::find(reachableAfter.begin(), reachableAfter.end(), jf::GridPos{1, 2}) != reachableAfter.end());
    }

    {
        // 踏査地点 (Marker): steppable (doesn't block movement/stopping) and
        // coexists with a Unit on the same tile - only Object/Object
        // placement collides, never Object/Unit.
        jf::Unit unit = makeUnit("scout", jf::Team::Player, {1, 0}, /*move=*/6);
        jf::BattleState battle({unit});

        jf::BattleObjectDefinition marker;
        marker.definitionId = "survey_marker";
        marker.kind = jf::BattleObjectKind::Marker;
        marker.canOccupy = true;
        assert(battle.registerObjectDefinition(marker));
        assert(battle.placeObject({"marker1", "survey_marker", {1, 2}}));

        assert(!battle.objectBlocksMovementAt({1, 2}));
        assert(!battle.objectBlocksStoppingAt({1, 2}));
        auto reachable = jf::computeReachableTiles(battle, battle.units()[0]);
        assert(std::find(reachable.begin(), reachable.end(), jf::GridPos{1, 2}) != reachable.end());
        assert(battle.moveUnit(battle.units()[0], {1, 2}));
        assert(battle.objectAt({1, 2}) != nullptr); // Unit's arrival doesn't displace the Marker
    }

    {
        // Exit: data-model presence only (docs/battle_objects.md kind
        // ExitPoint) - steppable like a Marker; EscapeUnits Objective
        // consumption is out of this Slice's scope (no such Objective kind
        // exists yet).
        jf::Unit unit = makeUnit("runner", jf::Team::Player, {1, 0});
        jf::BattleState battle({unit});
        jf::BattleObjectDefinition exit;
        exit.definitionId = "region_exit";
        exit.kind = jf::BattleObjectKind::ExitPoint;
        exit.canOccupy = true;
        assert(battle.registerObjectDefinition(exit));
        assert(battle.placeObject({"exit1", "region_exit", {1, 7}}));
        assert(!battle.objectBlocksMovementAt({1, 7}));
        assert(battle.objectAt({1, 7})->definitionId == "region_exit");
    }

    {
        // 操作 (Interact): range, allowedClasses, requiredState, and maxUses
        // are all validated before a real state transition happens; a
        // rejected attempt never mutates state or interactionCount.
        jf::Unit engineer = makeUnit("engineer", jf::Team::Player, {1, 1}, 4, jf::UnitClass::Spearman);
        jf::Unit farAway = makeUnit("far", jf::Team::Player, {1, 6}, 4, jf::UnitClass::Spearman);
        jf::Unit wrongClass = makeUnit("wrong_class", jf::Team::Player, {1, 1}, 4, jf::UnitClass::MarchCaptain);

        jf::BattleObjectState device{"lever1", "lever", {1, 0}, jf::BattleObjectTeam::Neutral,
                                     jf::BattleObjectStateKind::Active, 0, 0};
        jf::ObjectInteractionDefinition interaction;
        interaction.interactionId = "pull_lever";
        interaction.range = 1;
        interaction.allowedClasses = {jf::UnitClass::Spearman};
        interaction.requiredState = jf::BattleObjectStateKind::Active;
        interaction.maxUses = 1;

        // Wrong class: rejected, no mutation.
        assert(!jf::resolveObjectInteraction(wrongClass, device, interaction, jf::BattleObjectStateKind::Opened));
        assert(device.state == jf::BattleObjectStateKind::Active && device.interactionCount == 0);

        // Out of range: rejected, no mutation.
        assert(!jf::resolveObjectInteraction(farAway, device, interaction, jf::BattleObjectStateKind::Opened));
        assert(device.state == jf::BattleObjectStateKind::Active && device.interactionCount == 0);

        // Valid: adjacent, right class, right required state, under maxUses.
        assert(jf::resolveObjectInteraction(engineer, device, interaction, jf::BattleObjectStateKind::Opened));
        assert(device.state == jf::BattleObjectStateKind::Opened && device.interactionCount == 1);

        // Requires Active but it's now Opened: rejected even by the same
        // engineer standing in the same spot.
        assert(!jf::resolveObjectInteraction(engineer, device, interaction, jf::BattleObjectStateKind::Opened));

        // maxUses exhausted: even after manually resetting state back to
        // Active, interactionCount already at maxUses blocks further use -
        // "同じ一回限り操作を連打してObjectiveを増やさない".
        device.state = jf::BattleObjectStateKind::Active;
        assert(!jf::resolveObjectInteraction(engineer, device, interaction, jf::BattleObjectStateKind::Opened));
        assert(device.interactionCount == 1);
    }

    std::cout << "Battle tests PASSED\n";
    return 0;
}
