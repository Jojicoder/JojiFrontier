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
#include "jf/core/RouteGraph.hpp"
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
    static const auto realData = [] {
        auto loaded = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(loaded);
        return *loaded;
    }();
    data.terrainProfilesById = realData.terrainProfilesById;
    // regionDescriptor(RegionId::AshboughForest, ...) reads Ashbough Verge's
    // stage content straight from data/regions.json (docs/implementation_
    // roadmap.md M1-E slice1) - this synthetic GameData needs the real
    // Loader's copy of it too, same reasoning as terrainProfilesById above.
    data.stageContentById = realData.stageContentById;
    jf::Stats stats{.maxHp = 20, .strength = 6, .magic = 0, .speed = 5,
                    .defense = 2, .resistance = 1, .move = 4};
    jf::Weapon sword{.id = "sword", .name = "Sword", .might = 5, .minRange = 1,
                     .maxRange = 1, .damageType = jf::DamageType::Physical};
    data.weaponsById.emplace("sword", sword);
    jf::Weapon ironSpear{.id = "iron_spear", .name = "Iron Spear", .might = 6, .minRange = 1,
                         .maxRange = 2, .damageType = jf::DamageType::Physical};
    jf::Weapon heavySpear{.id = "heavy_spear", .name = "Heavy Spear", .might = 8, .minRange = 1, .maxRange = 2,
                          .damageType = jf::DamageType::Physical, .moveModifier = -1, .causesKnockback = true};
    jf::Weapon longSpear{.id = "long_spear", .name = "Long Spear", .might = 4, .minRange = 1, .maxRange = 3,
                         .damageType = jf::DamageType::Physical};
    data.weaponsById.emplace("iron_spear", ironSpear);
    data.weaponsById.emplace("heavy_spear", heavySpear);
    data.weaponsById.emplace("long_spear", longSpear);
    data.classesById.emplace(jf::UnitClass::MarchCaptain,
                             jf::ClassDefinition{jf::UnitClass::MarchCaptain, stats, "sword"});
    data.classesById.emplace(jf::UnitClass::FrontierScout,
                             jf::ClassDefinition{jf::UnitClass::FrontierScout, stats, "sword"});
    data.classesById.emplace(jf::UnitClass::Spearman,
                             jf::ClassDefinition{jf::UnitClass::Spearman, stats, "iron_spear"});
    // docs/regions/cinderwatch_gate.md's 回収団/元守備隊 rosters (site 1/2's
    // enemyRoster in data/regions.json) use these two classes -
    // GameApp::idlePlaceholderStage() always builds a battle from
    // Cinderwatch's real stage[0] at construction time, so every test's
    // synthetic GameData needs them registered too, not just the classes
    // this file's own hand-authored StageDescriptors use.
    jf::Weapon watchBow{.id = "watch_bow", .name = "Watch Bow", .might = 5, .minRange = 2,
                        .maxRange = 3, .damageType = jf::DamageType::Physical};
    data.weaponsById.emplace("watch_bow", watchBow);
    data.classesById.emplace(jf::UnitClass::WatchArcher,
                             jf::ClassDefinition{jf::UnitClass::WatchArcher, stats, "watch_bow"});
    jf::Weapon ironAxe{.id = "iron_axe", .name = "Iron Axe", .might = 6, .minRange = 1,
                      .maxRange = 1, .damageType = jf::DamageType::Physical};
    data.weaponsById.emplace("iron_axe", ironAxe);
    data.classesById.emplace(jf::UnitClass::Bandit, jf::ClassDefinition{jf::UnitClass::Bandit, stats, "iron_axe"});
    // docs/regions/cinderwatch_gate.md's old_barracks (site 3B) roster.
    jf::Weapon ironLance{.id = "iron_lance", .name = "Iron Lance", .might = 6, .minRange = 1,
                         .maxRange = 1, .damageType = jf::DamageType::Physical};
    data.weaponsById.emplace("iron_lance", ironLance);
    data.classesById.emplace(jf::UnitClass::VeteranGuard,
                             jf::ClassDefinition{jf::UnitClass::VeteranGuard, stats, "iron_lance"});
    jf::Weapon dawnStaff{.id = "dawn_staff", .name = "Dawn Staff", .might = 3, .minRange = 1,
                        .maxRange = 2, .damageType = jf::DamageType::Magical};
    data.weaponsById.emplace("dawn_staff", dawnStaff);
    data.classesById.emplace(jf::UnitClass::DawnChirurgeon,
                             jf::ClassDefinition{jf::UnitClass::DawnChirurgeon, stats, "dawn_staff"});
    jf::Stats heavyInfantryStats{.maxHp = 32, .strength = 8, .magic = 0, .speed = 2,
                                 .defense = 12, .resistance = 2, .move = 3};
    jf::Weapon ironGreathammer{.id = "iron_greathammer", .name = "Iron Greathammer", .might = 7, .minRange = 1,
                               .maxRange = 1, .damageType = jf::DamageType::Physical};
    data.weaponsById.emplace("iron_greathammer", ironGreathammer);
    data.classesById.emplace(jf::UnitClass::HeavyInfantry,
                             jf::ClassDefinition{jf::UnitClass::HeavyInfantry, heavyInfantryStats, "iron_greathammer"});
    jf::Stats frontierEngineerStats{.maxHp = 21, .strength = 6, .magic = 1, .speed = 5,
                                    .defense = 5, .resistance = 4, .move = 4};
    jf::Weapon engineerHammer{.id = "engineer_hammer", .name = "Engineer Hammer", .might = 5, .minRange = 1,
                              .maxRange = 1, .damageType = jf::DamageType::Physical};
    data.weaponsById.emplace("engineer_hammer", engineerHammer);
    data.classesById.emplace(jf::UnitClass::FrontierEngineer,
                             jf::ClassDefinition{jf::UnitClass::FrontierEngineer, frontierEngineerStats, "engineer_hammer"});
    jf::Stats messengerCavalryStats{.maxHp = 22, .strength = 7, .magic = 0, .speed = 9,
                                    .defense = 4, .resistance = 3, .move = 6};
    jf::Weapon messengerSword{.id = "messenger_sword", .name = "Messenger Sword", .might = 5, .minRange = 1,
                              .maxRange = 1, .damageType = jf::DamageType::Physical};
    data.weaponsById.emplace("messenger_sword", messengerSword);
    data.classesById.emplace(jf::UnitClass::MessengerCavalry,
                             jf::ClassDefinition{jf::UnitClass::MessengerCavalry, messengerCavalryStats, "messenger_sword"});
    jf::Stats frontierRangerStats{.maxHp = 20, .strength = 6, .magic = 0, .speed = 7,
                                  .defense = 4, .resistance = 4, .move = 4};
    jf::Weapon huntingBow{.id = "hunting_bow", .name = "Hunting Bow", .might = 4, .minRange = 2,
                          .maxRange = 2, .damageType = jf::DamageType::Physical};
    data.weaponsById.emplace("hunting_bow", huntingBow);
    data.classesById.emplace(jf::UnitClass::FrontierRanger,
                             jf::ClassDefinition{jf::UnitClass::FrontierRanger, frontierRangerStats, "hunting_bow"});
    jf::Stats bannerBearerStats{.maxHp = 22, .strength = 5, .magic = 2, .speed = 5,
                                .defense = 5, .resistance = 6, .move = 4};
    jf::Weapon bannerSpear{.id = "banner_spear", .name = "Banner Spear", .might = 4, .minRange = 1,
                           .maxRange = 2, .damageType = jf::DamageType::Physical};
    data.weaponsById.emplace("banner_spear", bannerSpear);
    data.classesById.emplace(jf::UnitClass::BannerBearer,
                             jf::ClassDefinition{jf::UnitClass::BannerBearer, bannerBearerStats, "banner_spear"});
    jf::Stats battleMageStats{.maxHp = 16, .strength = 1, .magic = 9, .speed = 5,
                              .defense = 2, .resistance = 7, .move = 4};
    jf::Weapon arcaneFocus{.id = "arcane_focus", .name = "Arcane Focus", .might = 6, .minRange = 1,
                           .maxRange = 2, .damageType = jf::DamageType::Magical};
    data.weaponsById.emplace("arcane_focus", arcaneFocus);
    data.classesById.emplace(jf::UnitClass::BattleMage,
                             jf::ClassDefinition{jf::UnitClass::BattleMage, battleMageStats, "arcane_focus"});
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
    jf::Weapon grubwormMandibles{.id = "grubworm_mandibles", .name = "Mandibles", .might = 0, .minRange = 1,
                                .maxRange = 1, .damageType = jf::DamageType::Physical};
    data.weaponsById.emplace("grubworm_mandibles", grubwormMandibles);
    jf::Stats grubwormStats{.maxHp = 56, .strength = 9, .magic = 0, .speed = 4,
                           .defense = 8, .resistance = 2, .move = 3};
    data.classesById.emplace(
        jf::UnitClass::AshironGrubworm,
        jf::ClassDefinition{jf::UnitClass::AshironGrubworm, grubwormStats, "grubworm_mandibles"});
    jf::Weapon serpentFangs{.id = "serpent_fangs", .name = "Fangs", .might = 0, .minRange = 1,
                           .maxRange = 1, .damageType = jf::DamageType::Physical};
    data.weaponsById.emplace("serpent_fangs", serpentFangs);
    jf::Stats serpentStats{.maxHp = 60, .strength = 9, .magic = 0, .speed = 5,
                          .defense = 6, .resistance = 3, .move = 4};
    data.classesById.emplace(
        jf::UnitClass::MarshFangSerpent,
        jf::ClassDefinition{jf::UnitClass::MarshFangSerpent, serpentStats, "serpent_fangs"});
    // Weapon-branch generalization to all 12 classes (docs/implementation_
    // roadmap.md "M7項目3(残り) ...特性・武器分岐の他兵種一般化") - the
    // synthetic branch weapons this file's own tests craft/equip, mirroring
    // data/weapons.json.
    auto addWeapon = [&](const char* id, const char* name, int might, int minRange, int maxRange,
                          jf::DamageType damageType, int moveModifier = 0, bool causesKnockback = false,
                          std::vector<jf::StatusEffectType> onHitStatuses = {}) {
        jf::Weapon weapon{.id = id, .name = name, .might = might, .minRange = minRange, .maxRange = maxRange,
                          .damageType = damageType};
        weapon.moveModifier = moveModifier;
        weapon.causesKnockback = causesKnockback;
        weapon.onHitStatuses = std::move(onHitStatuses);
        data.weaponsById.emplace(id, weapon);
    };
    addWeapon("command_sword", "Command Sword", 4, 1, 1, jf::DamageType::Physical);
    addWeapon("duel_sword", "Duel Sword", 7, 1, 1, jf::DamageType::Physical);
    addWeapon("guard_sword", "Guard Sword", 4, 1, 1, jf::DamageType::Physical);
    addWeapon("hook_lance", "Hook Lance", 5, 1, 2, jf::DamageType::Physical);
    addWeapon("fortress_lance", "Fortress Lance", 4, 1, 2, jf::DamageType::Physical, -1);
    addWeapon("patrol_lance", "Patrol Lance", 5, 1, 2, jf::DamageType::Physical, 1);
    addWeapon("long_watch_bow", "Long Watch Bow", 3, 3, 4, jf::DamageType::Physical);
    addWeapon("war_bow", "War Bow", 8, 2, 2, jf::DamageType::Physical);
    addWeapon("pinning_bow", "Pinning Bow", 4, 2, 3, jf::DamageType::Physical, 0, false,
              {jf::StatusEffectType::MoveDown});
    addWeapon("trail_blade", "Trail Blade", 3, 1, 1, jf::DamageType::Physical, 1);
    addWeapon("ambush_blade", "Ambush Blade", 6, 1, 1, jf::DamageType::Physical);
    addWeapon("withdrawal_blade", "Withdrawal Blade", 3, 1, 1, jf::DamageType::Physical);
    addWeapon("mercy_staff", "Mercy Staff", 1, 1, 2, jf::DamageType::Magical);
    addWeapon("ward_staff", "Ward Staff", 2, 1, 2, jf::DamageType::Magical);
    addWeapon("march_staff", "March Staff", 2, 1, 2, jf::DamageType::Magical);
    addWeapon("bulwark_maul", "Bulwark Maul", 5, 1, 1, jf::DamageType::Physical);
    addWeapon("breaker_maul", "Breaker Maul", 9, 1, 1, jf::DamageType::Physical, -1);
    addWeapon("driving_maul", "Driving Maul", 6, 1, 1, jf::DamageType::Physical, 0, true);
    addWeapon("builder_hammer", "Builder Hammer", 3, 1, 1, jf::DamageType::Physical);
    addWeapon("demolition_hammer", "Demolition Hammer", 7, 1, 1, jf::DamageType::Physical);
    addWeapon("repair_hammer", "Repair Hammer", 4, 1, 1, jf::DamageType::Physical);
    addWeapon("road_sabre", "Road Sabre", 3, 1, 1, jf::DamageType::Physical, 1);
    addWeapon("charge_lance", "Charge Lance", 7, 1, 1, jf::DamageType::Physical);
    addWeapon("escort_blade", "Escort Blade", 4, 1, 1, jf::DamageType::Physical);
    addWeapon("snare_bow", "Snare Bow", 3, 2, 2, jf::DamageType::Physical, 0, false,
              {jf::StatusEffectType::MoveDown});
    addWeapon("quarry_bow", "Quarry Bow", 5, 2, 2, jf::DamageType::Physical);
    addWeapon("driving_bow", "Driving Bow", 3, 2, 2, jf::DamageType::Physical, 0, true);
    addWeapon("far_standard", "Far Standard", 2, 1, 2, jf::DamageType::Physical);
    addWeapon("valor_standard", "Valor Standard", 5, 1, 2, jf::DamageType::Physical);
    addWeapon("warding_standard", "Warding Standard", 3, 1, 2, jf::DamageType::Physical);
    addWeapon("resonant_focus", "Resonant Focus", 4, 1, 2, jf::DamageType::Magical);
    addWeapon("war_focus", "War Focus", 9, 1, 2, jf::DamageType::Magical, -1);
    addWeapon("ember_focus", "Ember Focus", 5, 1, 2, jf::DamageType::Magical, 0, false,
              {jf::StatusEffectType::Burn});

    data.recruitDefinitionsById.emplace("heavy_recruit",
                                        jf::UnitTemplate{"heavy_recruit", "Hadric", jf::UnitClass::HeavyInfantry});
    data.recruitDefinitionsById.emplace(
        "engineer_recruit", jf::UnitTemplate{"engineer_recruit", "Oren", jf::UnitClass::FrontierEngineer});
    data.recruitDefinitionsById.emplace(
        "cavalry_recruit", jf::UnitTemplate{"cavalry_recruit", "Kael", jf::UnitClass::MessengerCavalry});
    data.recruitDefinitionsById.emplace(
        "banner_recruit", jf::UnitTemplate{"banner_recruit", "Lessa", jf::UnitClass::BannerBearer});
    data.recruitDefinitionsById.emplace(
        "ranger_recruit", jf::UnitTemplate{"ranger_recruit", "Vayla", jf::UnitClass::FrontierRanger});
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

// Forces a live GameApp battle to Victory by zeroing every enemy's HP,
// satisfying any OperateObject primary objectives (docs/regions/
// cinderwatch_gate.md「5. 信号塔下層」's 2 control panels replace the
// default EliminateTeam primary member - see BattleFactory.cpp's
// assembleScenario() - so zeroing HP alone doesn't win that stage), and
// then running one no-op player action (select -> stay in place -> wait) so
// BattleController::evaluateOutcome() notices completion.
void winCurrentBattle(jf::GameApp& app) {
    for (jf::Unit& unit : app.battle().battle().units()) {
        if (unit.team == jf::Team::Enemy) unit.currentHp = 0;
    }
    // docs/regions/ashiron_quarry.md「3A. 旧採掘坑」/「4. 灰鉄鉱脈」(M9-T):
    // a stage whose primaryEscapeUnitsAlternative replaced the default
    // EliminateTeam primary (blackwater_crossing/quarry_old_mine/ashiron_vein)
    // won't reach Victory from zeroed enemy HP alone - it needs a guest
    // credited on the escape tile, same as the direct BattleState-level test
    // above this helper. Teleports the first guest onto the target tile and
    // synthesizes the ActionResolvedEvent credit directly (test-only
    // shortcut - a real player turn would path-move there).
    for (const jf::ObjectiveDefinition& def : app.battle().battle().missionState().definitions) {
        if (def.groupId != "primary" || def.kind != jf::ObjectiveKind::EscapeUnits) continue;
        const auto& guestIds = app.battle().battle().missionState().guestUnitIds;
        if (guestIds.empty()) break;
        jf::Unit* guest = app.battle().battle().findUnit(guestIds[0]);
        if (!guest) break;
        guest->position = def.target.tile;
        jf::BattleEvent guestEscapes{static_cast<jf::BattleEventId>(app.battle().battle().round()), 1,
                                     jf::ActionResolvedEvent{1, guestIds[0], jf::Team::Player, jf::ActionKind::Wait,
                                                             def.target.tile}};
        jf::handleObjectiveEvent(app.battle().battle().missionState(), guestEscapes);
        jf::syncObjectiveProgress(app.battle().battle());
        break;
    }
    for (const jf::BattleObjectState& object : app.battle().battle().objects()) {
        const jf::BattleObjectDefinition* definition = app.battle().battle().objectDefinition(object.definitionId);
        if (definition && definition->interaction) {
            if (jf::BattleObjectState* mutableObject = app.battle().battle().findObject(object.id))
                mutableObject->interactionCount = std::max(mutableObject->interactionCount, 1);
        }
    }
    // Test helper only: advance the BattleState clock directly until every
    // scheduled mandatory wave resolves (loops rather than a single cycle
    // since not every stage's spawnRound is 2 - docs/regions/
    // cinderwatch_gate.md「地域ボス 元守備隊長」's axeman is spawnRound 3),
    // then defeat the spawned units each time.
    while (app.battle().battle().hasPendingRequiredEnemyReinforcements()) {
        app.battle().battle().beginEnemyPhase();
        app.battle().battle().beginPlayerPhase();
        app.battle().battle().beginEnemyPhase();
        for (jf::Unit& unit : app.battle().battle().units())
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

// Same idea as startCinderwatchExpedition(), one region further: AshironQuarry
// unlocks on CinderwatchGate completion (docs/region_unlocks.md).
bool startAshironQuarryExpedition(jf::GameApp& app) {
    jf::SaveData save = app.createSaveData("en");
    save.base.completedRegionIds.insert(jf::RegionId::AshboughForest);
    save.base.completedRegionIds.insert(jf::RegionId::CinderwatchGate);
    if (!app.applySaveData(save)) return false;
    return app.startExpedition(jf::RegionId::AshironQuarry);
}

// Same idea, one region further: BlackwaterLowlands unlocks on AshironQuarry
// completion (docs/region_unlocks.md).
bool startBlackwaterExpedition(jf::GameApp& app) {
    jf::SaveData save = app.createSaveData("en");
    save.base.completedRegionIds.insert(jf::RegionId::AshboughForest);
    save.base.completedRegionIds.insert(jf::RegionId::CinderwatchGate);
    save.base.completedRegionIds.insert(jf::RegionId::AshironQuarry);
    if (!app.applySaveData(save)) return false;
    return app.startExpedition(jf::RegionId::BlackwaterLowlands);
}

// docs/regions/blackwater_lowlands.md「5. 黒水渡し」: wins sites 1-4 (all via
// FrontalAdvance, same "AllMembers" branch-resolution shape the resin_grove
// test above uses) and continues to Blackwater Crossing's Exploration
// screen, the shared setup every Blackwater Crossing GameApp test needs.
bool reachBlackwaterCrossing(jf::GameApp& app) {
    if (!startBlackwaterExpedition(app)) return false;
    if (!app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)) return false; // sunken_path
    winCurrentBattle(app);
    app.proceedToCamp();
    app.continueExpedition();
    if (!app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)) return false; // reedway_fork
    winCurrentBattle(app);
    app.proceedToCamp();
    app.continueExpedition();
    if (!app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)) return false; // herb_islet
    winCurrentBattle(app);
    app.proceedToCamp();
    app.continueExpedition();
    if (!app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)) return false; // resin_grove
    winCurrentBattle(app);
    app.proceedToCamp();
    app.continueExpedition(); // both branch members resolved -> Camp II -> blackwater_crossing
    return app.screen() == jf::Screen::Exploration && app.currentMissionNameJa() == "黒水渡し";
}

// docs/regions/cinderwatch_gate.md「1. シンダーウォッチ外門」disables ScoutRoute
// (its class-gated 3rd choice needs 重装兵/HeavyInfantry, not implemented yet
// - docs/implementation_roadmap.md M6-A). Tests that exercise the generic
// ScoutRoute/PreBattleDeployment machinery advance past site 1 first and use
// site 2 (灰道の監視所, ashroad_watch) instead, which keeps the default
// FrontierScout-gated ScoutRoute untouched.
bool advanceToAshroadWatch(jf::GameApp& app) {
    if (!startCinderwatchExpedition(app)) return false;
    if (!app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)) return false;
    winCurrentBattle(app);
    app.proceedToCamp();
    app.continueExpedition();
    return app.screen() == jf::Screen::Exploration;
}

// docs/regions/cinderwatch_gate.md「5. 信号塔下層」: wins site 1/2, both
// branch members (ironwatch_stores, old_barracks), and continues through
// Camp II to signal_tower's Exploration screen, the shared setup every
// signal_tower test needs.
bool advanceToSignalTower(jf::GameApp& app) {
    if (!advanceToAshroadWatch(app)) return false; // -> Exploration for site 2 (ashroad_watch)
    if (!app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)) return false; // site 2 -> Battle
    winCurrentBattle(app);
    app.proceedToCamp();
    app.continueExpedition(); // -> branch, first unresolved member
    if (!app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)) return false;
    winCurrentBattle(app);
    app.proceedToCamp();
    app.continueExpedition(); // -> branch again, other member
    if (!app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)) return false;
    winCurrentBattle(app);
    app.proceedToCamp();
    app.continueExpedition(); // both members resolved -> Camp II -> signal_tower
    return app.screen() == jf::Screen::Exploration && app.currentMissionNameJa() == "信号塔下層";
}

// docs/regions/cinderwatch_gate.md「6. 最後の信号」: wins signal_tower and
// continues to last_signal's Exploration screen, the shared setup every
// last_signal (boss) test needs.
bool advanceToLastSignal(jf::GameApp& app) {
    if (!advanceToSignalTower(app)) return false;
    if (!app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)) return false;
    winCurrentBattle(app);
    app.proceedToCamp();
    app.continueExpedition();
    return app.screen() == jf::Screen::Exploration && app.currentMissionNameJa() == "最後の信号";
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
        const auto loaded = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(loaded);
        const jf::UnitTemplate* heavyRecruit = loaded->recruitDefinition("heavy_recruit");
        assert(heavyRecruit);
        assert(heavyRecruit->name == "Hadric");
        assert(heavyRecruit->classId == jf::UnitClass::HeavyInfantry);
    }

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
        jf::resolveAttack(first, first.units()[0], first.units()[1], 0, false);
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
            advanceToAshroadWatch(app);
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
        assert(advanceToAshroadWatch(app));
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
        advanceToAshroadWatch(app);
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

        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)); // site 1 -> Battle
        // docs/implementation_roadmap.md M6-C: Cinderwatch now has 6 real
        // battles end-to-end (site 1/2/3A/3B/5 real, last_signal still the
        // old placeholder standing in for site 6's boss fight) - both
        // branch members (ironwatch_stores, old_barracks) are required
        // before Camp II, and since the region is on the Route Graph now,
        // every stage transition goes through Exploration first (not just
        // stage 0).
        for (int stage = 0; stage < 6; ++stage) {
            assert(app.screen() == jf::Screen::Battle);
            winCurrentBattle(app);
            assert(app.battle().inputState() == jf::BattleInputState::Victory);
            app.proceedToCamp();
            assert(app.screen() == jf::Screen::Camp);
            if (stage < 5) {
                app.continueExpedition();
                assert(app.screen() == jf::Screen::Exploration);
                assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
            }
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
        // outpost, then unlock all 4 optional stage-1 facilities in parallel
        // using only materials/discoveries actually earned from real victories
        // (docs/base_development.md: "素材が足りれば4施設すべてを順次建設できる" -
        // no facility-slot cap, no dismantle/rebuild dance required to fit
        // them). Confirms branch nodes require their facility to be actually
        // built (not just historically unlocked), and that material scarcity
        // still gates individual nodes.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(startCinderwatchExpedition(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        for (int stage = 0; stage < 6; ++stage) {
            winCurrentBattle(app);
            app.proceedToCamp();
            if (stage < 5) {
                app.continueExpedition();
                assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
            }
        }
        app.returnToBase();
        assert(app.advanceOutpostStage()); // -> PioneerOutpost
        // 6 real victories (docs/implementation_roadmap.md M6-C: site 1/2/
        // 3A/3B/5 now real, their baseVictoryLoot includes a small wood/hide
        // top-up specifically to keep this total intact - last_signal, still
        // a placeholder, carries the rest) bank exactly enough for
        // all 4 optional facilities (docs/base_development.md "初期4施設の
        // 確定表"): wood:10, hide:5, herb:2.
        assert(app.baseState().storageCount("wood") == 10);
        assert(app.baseState().storageCount("hide") == 5);
        assert(app.baseState().discoveryRegistry.count(jf::kHerbThicketDiscovery) == 1);

        assert(!app.baseState().unlockedNodeIds.count("training_field"));
        assert(app.unlockFacilityNode("training_field")); // wood:3 + hide:2
        assert(app.baseState().unlockedNodeIds.count("training_field") == 1);
        assert(app.baseState().constructedFacilityIds.count("training_field") == 1);
        assert(app.baseState().storageCount("wood") == 7);
        assert(app.baseState().storageCount("hide") == 3);

        assert(app.unlockFacilityNode("vanguard_training")); // branch: no cost, just needs the facility built
        assert(app.baseState().unlockedNodeIds.count("vanguard_training") == 1);

        // The other 3 optional facilities build in parallel - no slot cap to
        // work around, so no need to dismantle training_field first.
        assert(app.unlockFacilityNode("simple_forge"));    // wood:2 + hide:1
        assert(app.unlockFacilityNode("workshop_bench"));  // wood:3 + hide:1
        assert(app.unlockFacilityNode("field_infirmary")); // wood:2 + herb:2, needs herb-thicket discovery
        assert(app.baseState().constructedFacilityIds.size() == 4);

        assert(app.unlockFacilityNode("weapon_forging")); // branch, no cost
        assert(jf::facilityNodeEligible(app.baseState(), *jf::findFacilityNode("craft_heavy_spear")));
        assert(app.unlockFacilityNode("craft_heavy_spear"));

        assert(app.unlockFacilityNode("trait_hide_wrapped_grip")); // hide:1, exactly what's left
    }

    {
        // docs/route_graph_data.md「分岐と合流」/「受入条件」: "2地点Branchを
        // 任意順で解決し、片方だけ持ち帰れる" - resolve old_barracks (branch
        // member 2, listed AFTER ironwatch_stores in branchMembers) before
        // ironwatch_stores, safe-return with only that one secured, then
        // confirm a fresh expedition safely passes the already-secured
        // member and still reaches Camp II / signal_tower via the other.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(startCinderwatchExpedition(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)); // site 1
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition();
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)); // site 2
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition(); // -> branch, picks the first unresolved member (ironwatch_stores)
        assert(app.screen() == jf::Screen::Exploration);
        // Deliberately win the OTHER member first to prove order doesn't
        // matter for AllMembers completion: retire without finishing the
        // branch, so nothing is permanently secured yet...
        assert(app.retireExpedition());
        assert(app.screen() == jf::Screen::Base);

        jf::SaveData primed = app.createSaveData("en");
        primed.base.completedRegionIds.insert(jf::RegionId::AshboughForest);
        primed.base.siteAccess[jf::siteAccessKey(jf::RegionId::CinderwatchGate, "cinderwatch_outer_gate")] =
            jf::SiteAccessState::Secured;
        primed.base.siteAccess[jf::siteAccessKey(jf::RegionId::CinderwatchGate, "ashroad_watch")] =
            jf::SiteAccessState::Secured;
        assert(app.applySaveData(primed));
        assert(app.startExpedition(jf::RegionId::CinderwatchGate));
        // Both site 1/2 already Secured - bulkPassSecuredSites() must walk
        // straight through them without a battle and land in the branch.
        assert(app.bulkPassSecuredSites() == 2);
        assert(app.screen() == jf::Screen::Exploration);
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)); // enters old_barracks first
        winCurrentBattle(app);
        app.proceedToCamp();
        assert(app.screen() == jf::Screen::Camp);
        assert(!app.expeditionComplete()); // ironwatch_stores (the other member) is still unresolved

        app.continueExpedition(); // -> branch again, only ironwatch_stores left unresolved
        assert(app.screen() == jf::Screen::Exploration);
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition(); // both members resolved -> past the branch, into Camp II -> signal_tower
        assert(app.screen() == jf::Screen::Exploration);
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        assert(!app.expeditionComplete()); // last_signal (site 6) still unresolved

        app.continueExpedition(); // -> last_signal
        assert(app.screen() == jf::Screen::Exploration);
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        assert(app.expeditionComplete());
        assert(app.returnToBase());
        assert(app.baseState().siteAccess.at(jf::siteAccessKey(jf::RegionId::CinderwatchGate, "old_barracks")) >=
              jf::SiteAccessState::Surveyed);
        assert(app.baseState().siteAccess.at(jf::siteAccessKey(jf::RegionId::CinderwatchGate, "ironwatch_stores")) >=
              jf::SiteAccessState::Surveyed);
    }

    {
        // docs/implementation_roadmap.md M6-C item1 / docs/regions/
        // cinderwatch_gate.md「3. アイアンウォッチ物資庫」: FrontalAdvance
        // (医療区画を先に確保) keeps the full 4-unit roster and grants
        // ironwatch_field_medicine_records.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(startCinderwatchExpedition(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)); // site 1
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition();
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)); // site 2
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition(); // -> branch, ironwatch_stores first
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));

        int enemyCount = 0;
        for (const jf::Unit& unit : app.battle().battle().units())
            if (unit.team == jf::Team::Enemy) ++enemyCount;
        assert(enemyCount == 4);

        // docs/regions/cinderwatch_gate.md「3. アイアンウォッチ物資庫」's "物資箱
        // 2個のうち1個以上を確保": 2 SecureTile objectives grouped Any under
        // "ironwatch_stores_crates", one supply_crate Container marker per
        // tile, and - unlike Herbwater Hollow's HerbPatch tiles - the tiles
        // themselves must NOT have had their Terrain changed (a supply crate
        // shouldn't heal a unit that ends its turn there).
        const jf::BattleMissionState& mission = app.battle().battle().missionState();
        const auto group = std::find_if(mission.groups.begin(), mission.groups.end(), [](const auto& g) {
            return g.id == "ironwatch_stores_crates";
        });
        assert(group != mission.groups.end() && group->rule == jf::ObjectiveGroupRule::Any);
        int crateObjectiveCount = 0;
        std::vector<jf::GridPos> crateTiles;
        for (const jf::ObjectiveDefinition& def : mission.definitions) {
            if (def.groupId == "ironwatch_stores_crates") {
                ++crateObjectiveCount;
                assert(def.kind == jf::ObjectiveKind::SecureTile);
                crateTiles.push_back(def.target.tile);
            }
        }
        assert(crateObjectiveCount == 2);
        for (const jf::GridPos& tile : crateTiles)
            assert(app.battle().battle().terrainAt(tile) != jf::TerrainType::HerbPatch);
        int crateObjectCount = 0;
        for (const jf::BattleObjectState& object : app.battle().battle().objects())
            if (object.definitionId == "supply_crate") ++crateObjectCount;
        assert(crateObjectCount == 2);

        winCurrentBattle(app);
        app.proceedToCamp();
        assert(app.baseState().discoveryRegistry.count(jf::kFieldMedicineDiscovery) == 0); // not registered yet
    }

    {
        // 工具庫を先に確保 (CollapsedSidePath): drops the archer
        // (enemiesRemoved on a roster with the archer listed last), adds 2
        // stacked-crate obstacles (extraBarrierCount ->
        // objectPlacementRules' scalesWithExtraBarrierOutcome), grants +1
        // iron on top of the base victory loot, and does NOT grant
        // ironwatch_field_medicine_records (route-gated to FrontalAdvance
        // via the new routeDiscoveries field) - herb_thicket_grounds still
        // does, unconditionally, since nothing else grants it.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(startCinderwatchExpedition(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)); // site 1
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition();
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)); // site 2
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition(); // -> branch, ironwatch_stores first
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::CollapsedSidePath));

        int enemyCount = 0;
        int objectCount = 0;
        for (const jf::Unit& unit : app.battle().battle().units())
            if (unit.team == jf::Team::Enemy) ++enemyCount;
        for (const jf::BattleObjectState& object : app.battle().battle().objects())
            if (object.definitionId == "stacked_crate") ++objectCount;
        assert(enemyCount == 3);
        assert(objectCount == 2);

        winCurrentBattle(app);
        app.proceedToCamp();
        app.returnToBase();
        // Storage only commits pendingLoot on returnToBase(), so this totals
        // site 1 (cinderwatch_outer_gate)'s iron:1 plus ironwatch_stores'
        // own iron:2 (baseVictoryLoot) + iron:1 (routeVictoryLootDelta).
        assert(app.baseState().storageCount("iron") == 4);
        assert(app.baseState().discoveryRegistry.count(jf::kFieldMedicineDiscovery) == 0);
        assert(app.baseState().discoveryRegistry.count(jf::kHerbThicketDiscovery) == 1);
    }

    {
        // docs/implementation_roadmap.md M6-C item2 / docs/regions/
        // cinderwatch_gate.md「5. 信号塔下層」: primary is 2 OperateObject
        // Objectives (副信号機・主信号機), replacing the default EliminateTeam
        // member entirely (BattleFactory.cpp's assembleScenario()) - so
        // defeating every enemy without operating both panels must NOT win.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(advanceToSignalTower(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));

        for (jf::Unit& unit : app.battle().battle().units())
            if (unit.team == jf::Team::Enemy) unit.currentHp = 0;
        // Resolve the round-2 axeman wave first (same shortcut as
        // winCurrentBattle()) - hasPendingRequiredEnemyReinforcements()
        // blocks Victory regardless of the primary objectives, and it's
        // Scheduled from round 1 since this stage's reinforcement isn't
        // choice-conditional.
        if (app.battle().battle().hasPendingRequiredEnemyReinforcements()) {
            app.battle().battle().beginEnemyPhase();
            app.battle().battle().beginPlayerPhase();
            app.battle().battle().beginEnemyPhase();
            for (jf::Unit& unit : app.battle().battle().units())
                if (unit.team == jf::Team::Enemy) unit.currentHp = 0;
        }
        auto actOnce = [&]() {
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
        };

        jf::BattleObjectState* secondaryPanel = app.battle().battle().findObject("secondary_signal_panel_1");
        assert(secondaryPanel != nullptr);
        secondaryPanel->interactionCount = 1; // only ONE of the 2 panels operated
        actOnce();
        assert(app.battle().inputState() != jf::BattleInputState::Victory); // primary_signal_panel still unoperated

        jf::BattleObjectState* primaryPanel = app.battle().battle().findObject("primary_signal_panel_1");
        assert(primaryPanel != nullptr);
        primaryPanel->interactionCount = 1;
        actOnce();
        assert(app.battle().inputState() == jf::BattleInputState::Victory); // both panels now operated

        const jf::BattleMissionState& mission = app.battle().battle().missionState();
        const auto group = std::find_if(mission.groups.begin(), mission.groups.end(),
                                        [](const auto& g) { return g.id == "primary"; });
        assert(group != mission.groups.end() && group->rule == jf::ObjectiveGroupRule::All);
        assert(mission.progress.count("eliminate_enemies") == 0); // default primary member was removed, not widened

        app.proceedToCamp();
        assert(app.screen() == jf::Screen::Camp);
        assert(app.baseState().discoveryRegistry.count(jf::kReturnSignalDiscovery) == 0); // not banked until return
    }

    {
        // 軍旗保管箱を確保: same surveyObjectiveId/surveyTileCount(1)/
        // surveyTileObjectDefinitionId pattern as 3A's supply crates, just a
        // single tile this time. Also confirms the round-2 axeman
        // reinforcement (docs' "制御盤1個目の操作後" trigger, approximated
        // to a fixed round since no event-conditioned reinforcement trigger
        // exists) actually spawns.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(advanceToSignalTower(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));

        const jf::BattleMissionState& mission = app.battle().battle().missionState();
        const auto group = std::find_if(mission.groups.begin(), mission.groups.end(), [](const auto& g) {
            return g.id == "signal_tower_banner_chest";
        });
        assert(group != mission.groups.end() && group->rule == jf::ObjectiveGroupRule::Any);
        int chestObjectiveCount = 0;
        for (const jf::ObjectiveDefinition& def : mission.definitions) {
            if (def.groupId == "signal_tower_banner_chest") {
                ++chestObjectiveCount;
                assert(def.kind == jf::ObjectiveKind::SecureTile);
            }
        }
        assert(chestObjectiveCount == 1);
        int chestObjectCount = 0;
        for (const jf::BattleObjectState& object : app.battle().battle().objects())
            if (object.definitionId == "banner_chest") ++chestObjectCount;
        assert(chestObjectCount == 1);

        assert(app.battle().battle().hasPendingRequiredEnemyReinforcements());
        winCurrentBattle(app); // advances through the round-2 wave internally
        assert(!app.battle().battle().hasPendingRequiredEnemyReinforcements());
        assert(app.battle().inputState() == jf::BattleInputState::Victory);
    }

    {
        // docs/implementation_roadmap.md M6-C item3 / docs/regions/
        // cinderwatch_gate.md「地域ボス 元守備隊長」: primary is a single
        // DefeatUnit targeting the boss, replacing the default EliminateTeam
        // member entirely - so leaving every OTHER enemy alive while
        // defeating only the boss must still win.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(advanceToLastSignal(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));

        // Resolve the round-3 axeman wave first (same shortcut as
        // winCurrentBattle()) - hasPendingRequiredEnemyReinforcements()
        // blocks Victory regardless of the primary objective.
        while (app.battle().battle().hasPendingRequiredEnemyReinforcements()) {
            app.battle().battle().beginEnemyPhase();
            app.battle().battle().beginPlayerPhase();
            app.battle().battle().beginEnemyPhase();
        }

        jf::Unit* boss = nullptr;
        for (jf::Unit& unit : app.battle().battle().units()) {
            if (unit.team == jf::Team::Enemy && unit.id == "last_signal_boss1") {
                boss = &unit;
                break;
            }
        }
        assert(boss != nullptr);
        boss->currentHp = 0; // only the boss defeated - every other enemy left alive

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
        assert(app.battle().inputState() == jf::BattleInputState::Victory);

        const jf::BattleMissionState& mission = app.battle().battle().missionState();
        assert(mission.progress.count("eliminate_enemies") == 0); // default primary member was removed
        assert(mission.progress.at("defeat_boss").status == jf::ObjectiveStatus::Completed);
    }

    {
        // `[行軍隊長]` (MarchCaptain is a real UnitClass, unlike 3A/5's
        // disabled 3rd choices): route is selectable when the party has a
        // MarchCaptain, and grants +1 old_gear on top of the base reward.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data); // data.playerParty[0] defaults to MarchCaptain (makeFactoryData())
        assert(advanceToLastSignal(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::ScoutRoute));

        winCurrentBattle(app);
        app.proceedToCamp();
        app.returnToBase();
        // 2 (this stage's own baseVictoryLoot) + 1 (routeVictoryLootDelta) is
        // this stage's own contribution; storageCount reflects the whole
        // 6-battle expedition's old_gear, so just check the delta landed.
        assert(app.baseState().storageCount("old_gear") >= 3);
    }

    {
        // 味方戦闘不能者0 (docs/regions/cinderwatch_gate.md's noCasualties
        // secondary) reuses StageDescriptor::noCasualtiesBonusLoot - already
        // implemented/JSON-wired since Brokenwood Territory, no new code.
        // Also confirms the round-3 axeman reinforcement (spawnRound 3,
        // unlike site 5's round-2 one) actually spawns.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(advanceToLastSignal(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));

        assert(app.battle().battle().hasPendingRequiredEnemyReinforcements());
        winCurrentBattle(app); // advances through the round-3 wave internally
        assert(!app.battle().battle().hasPendingRequiredEnemyReinforcements());
        assert(app.battle().inputState() == jf::BattleInputState::Victory);
        app.proceedToCamp();
        app.returnToBase();
        assert(app.baseState().storageCount("quality_iron") == 1);
    }

    {
        // M6完了Gate: 6地点すべて実装済みの状態で最後まで攻略・安全帰還すると、
        // 汎用の`wouldRegionBeCleared()`/`pendingRegionCompletions`機構
        // (地点固有のC++分岐なし)がRegionId::CinderwatchGateを
        // `completedRegionIds`へ追加すること。
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(startCinderwatchExpedition(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        for (int stage = 0; stage < 6; ++stage) {
            winCurrentBattle(app);
            app.proceedToCamp();
            if (stage < 5) {
                app.continueExpedition();
                assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
            }
        }
        assert(!app.baseState().completedRegionIds.count(jf::RegionId::CinderwatchGate));
        app.returnToBase();
        assert(app.baseState().completedRegionIds.count(jf::RegionId::CinderwatchGate) == 1);
    }

    {
        // docs/regions/cinderwatch_gate.md「地域の最低保証報酬」: a missed
        // Discovery (ironwatch_stores' 野戦医療記録, only granted via the
        // FrontalAdvance route) is caught up at region completion.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(startCinderwatchExpedition(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)); // site1 outer_gate
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition(); // -> site2 ashroad_watch
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition(); // -> branch, ironwatch_stores first
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::CollapsedSidePath)); // skips 野戦医療記録
        winCurrentBattle(app);
        app.proceedToCamp();
        assert(!app.baseState().discoveryRegistry.count(jf::kFieldMedicineDiscovery));
        app.continueExpedition(); // -> branch, old_barracks
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition(); // both resolved -> Camp II -> signal_tower
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition(); // -> last_signal
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        assert(!app.baseState().discoveryRegistry.count(jf::kFieldMedicineDiscovery)); // still missing pre-return
        assert(app.returnToBase());
        assert(app.baseState().completedRegionIds.count(jf::RegionId::CinderwatchGate));
        // Region completion's floor top-up caught the missed Discovery.
        assert(app.baseState().discoveryRegistry.count(jf::kFieldMedicineDiscovery));
        assert(app.baseState().discoveryRegistry.count(jf::kCinderwatchReconDiscovery));
        assert(app.baseState().discoveryRegistry.count(jf::kReturnSignalDiscovery));
        assert(app.baseState().discoveryRegistry.count(jf::kBannerRecordsDiscovery)); // no normal grant path exists yet
        // Material floor: 鉄材5、石材3、旧軍備3、信号機の中核部品1.
        assert(app.baseState().storageCount("iron") >= 5);
        assert(app.baseState().storageCount("stone") >= 3);
        assert(app.baseState().storageCount("old_gear") >= 3);
        assert(app.baseState().storageCount("signal_core") >= 1);
    }

    {
        // Floor accumulation must span multiple expeditions (safe returns)
        // without double-granting once the region completes.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(startCinderwatchExpedition(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)); // site1: iron+... this partial run
        winCurrentBattle(app);
        app.proceedToCamp();
        assert(app.returnToBase()); // safe return after only 1 of 6 sites
        assert(!app.baseState().completedRegionIds.count(jf::RegionId::CinderwatchGate));
        const int earnedIronAfterFirstReturn = app.baseState().cinderwatchMaterialsEarned.count("iron")
                                                    ? app.baseState().cinderwatchMaterialsEarned.at("iron")
                                                    : 0;
        assert(earnedIronAfterFirstReturn > 0); // outer_gate's own baseVictoryLoot includes iron

        // Second expedition: replay the full 6-site route (site1 is already
        // permanently Secured, so this just re-earns its loot on top of the
        // first expedition's tally) through to region completion. AshboughForest
        // is still marked completed from the first startCinderwatchExpedition()
        // call, so a direct startExpedition() suffices here.
        assert(app.startExpedition(jf::RegionId::CinderwatchGate));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        for (int stage = 0; stage < 6; ++stage) {
            winCurrentBattle(app);
            app.proceedToCamp();
            if (stage < 5) {
                app.continueExpedition();
                assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
            }
        }
        assert(app.returnToBase());
        assert(app.baseState().completedRegionIds.count(jf::RegionId::CinderwatchGate));
        const int ironAfterCompletion = app.baseState().storageCount("iron");
        assert(ironAfterCompletion >= 5);

        // Re-clearing the (already-completed) region a 3rd time must not
        // reapply the floor top-up a second time.
        assert(app.startExpedition(jf::RegionId::CinderwatchGate));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        assert(app.returnToBase());
        assert(app.baseState().storageCount("iron") == ironAfterCompletion + 1); // just this stage's own base loot
    }

    {
        // docs/implementation_roadmap.md M6-D: 灰鉄採石場 stays locked until
        // CinderwatchGate completes, then becomes selectable/startable.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(startCinderwatchExpedition(app));
        assert(!app.isRegionUnlocked(jf::RegionId::AshironQuarry));
        assert(!app.startExpedition(jf::RegionId::AshironQuarry));
        for (int stage = 0; stage < 6; ++stage) {
            assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
            winCurrentBattle(app);
            app.proceedToCamp();
            if (stage < 5) app.continueExpedition();
        }
        assert(app.returnToBase());
        assert(app.baseState().completedRegionIds.count(jf::RegionId::CinderwatchGate));
        assert(app.isRegionUnlocked(jf::RegionId::AshironQuarry));
        assert(app.startExpedition(jf::RegionId::AshironQuarry));
        assert(app.screen() == jf::Screen::Exploration);
    }

    {
        // Save/Load round-trip for cinderwatchMaterialsEarned.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(startCinderwatchExpedition(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        assert(app.returnToBase());
        assert(app.baseState().cinderwatchMaterialsEarned.count("iron"));

        jf::SaveData saved = app.createSaveData("en");
        assert(saved.base.cinderwatchMaterialsEarned.count("iron"));
        std::string json = jf::serializeSave(saved);
        auto reloaded = jf::deserializeSave(json);
        assert(reloaded);
        assert(reloaded->base.cinderwatchMaterialsEarned.count("iron"));
        assert(reloaded->base.cinderwatchMaterialsEarned.at("iron") ==
               saved.base.cinderwatchMaterialsEarned.at("iron"));
    }

    {
        // docs/regions/cinderwatch_gate.md「報酬と加入」(approximated per M7-2's
        // plan): ironwatch_stores' ordinary victory grants engineer_recruit's
        // candidate, and the CollapsedSidePath route also registers 野戦工作記録
        // (ironwatch_field_construction_records).
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(advanceToAshroadWatch(app)); // -> Exploration for site2 (ashroad_watch)
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition(); // -> branch, ironwatch_stores first
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::CollapsedSidePath));
        winCurrentBattle(app);
        app.proceedToCamp();
        assert(app.returnToBase());
        assert(app.baseState().joinReadyCandidateIds.count("engineer_recruit"));
        assert(app.baseState().discoveryRegistry.count("ironwatch_field_construction_records"));
        assert(app.recruitCapacity() == 11); // 野戦工作記録あり
    }

    {
        // old_barracks' ordinary victory grants cavalry_recruit's candidate.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(advanceToAshroadWatch(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition(); // -> branch, ironwatch_stores first
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)); // skip CollapsedSidePath here
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition(); // -> branch, old_barracks
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        assert(app.returnToBase());
        assert(app.baseState().joinReadyCandidateIds.count("cavalry_recruit"));
        // ironwatch_stores was cleared via FrontalAdvance this time, so no
        // 野戦工作記録 - capacity stays at the AshboughForest-only tier.
        assert(!app.baseState().discoveryRegistry.count("ironwatch_field_construction_records"));
        assert(app.recruitCapacity() == 8);
    }

    {
        // Full 6-site clear + safe return grants banner_recruit's candidate
        // (docs/roster_design.md「加入タイミング」: 軍旗記録registeredそのものが
        // 条件、Pendingを経由せず地域完了Transaction内で直接付与される).
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(startCinderwatchExpedition(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        for (int stage = 0; stage < 6; ++stage) {
            winCurrentBattle(app);
            app.proceedToCamp();
            if (stage < 5) {
                app.continueExpedition();
                assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
            }
        }
        assert(app.returnToBase());
        assert(app.baseState().completedRegionIds.count(jf::RegionId::CinderwatchGate));
        assert(app.baseState().joinReadyCandidateIds.count("banner_recruit"));
    }

    {
        // recruitCapacity(): 3rd tier (専門区画11人) requires
        // ironwatch_field_construction_records specifically, independent of
        // AshboughForest completion.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(app.recruitCapacity() == 6);
        jf::BaseState& testBase = const_cast<jf::BaseState&>(app.baseState());
        testBase.discoveryRegistry.insert("ironwatch_field_construction_records");
        assert(app.recruitCapacity() == 11);
        testBase.completedRegionIds.insert(jf::RegionId::AshboughForest);
        assert(app.recruitCapacity() == 11); // still 11, not superseded by the lower tier
    }

    {
        // GameApp::confirmRecruitJoin(): the now-data-driven path (GameData::
        // recruitDefinition()) works identically for a non-heavy_recruit id -
        // adds FrontierEngineer to the roster and auto-equips its Tier1 skill.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        jf::BaseState& testBase = const_cast<jf::BaseState&>(app.baseState());
        testBase.joinReadyCandidateIds.insert("engineer_recruit");
        testBase.completedRegionIds.insert(jf::RegionId::AshboughForest); // room in the roster (capacity 8)
        const std::size_t rosterSizeBefore = app.roster().size();
        assert(app.confirmRecruitJoin("engineer_recruit"));
        assert(app.roster().size() == rosterSizeBefore + 1);
        auto joined = std::find_if(app.roster().begin(), app.roster().end(),
                                   [](const jf::UnitTemplate& u) { return u.id == "engineer_recruit"; });
        assert(joined != app.roster().end() && joined->classId == jf::UnitClass::FrontierEngineer);
        assert(app.baseState().joinedRecruitIds.count("engineer_recruit"));
        auto skillIt = app.equippedSkills().find("engineer_recruit");
        assert(skillIt != app.equippedSkills().end() && !skillIt->second.equippedSkillIds[0].empty());
        const jf::SkillDefinition* skill = jf::findSkill(skillIt->second.equippedSkillIds[0]);
        assert(skill && skill->unitClass == jf::UnitClass::FrontierEngineer && skill->unlockTier == 1);
        assert(skillIt->second.equippedSkillIds[1].empty());
    }

    {
        // Same as above, for ranger_recruit (FrontierRanger).
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        jf::BaseState& testBase = const_cast<jf::BaseState&>(app.baseState());
        testBase.joinReadyCandidateIds.insert("ranger_recruit");
        testBase.completedRegionIds.insert(jf::RegionId::AshboughForest); // room in the roster (capacity 8)
        const std::size_t rosterSizeBefore = app.roster().size();
        assert(app.confirmRecruitJoin("ranger_recruit"));
        assert(app.roster().size() == rosterSizeBefore + 1);
        auto joined = std::find_if(app.roster().begin(), app.roster().end(),
                                   [](const jf::UnitTemplate& u) { return u.id == "ranger_recruit"; });
        assert(joined != app.roster().end() && joined->classId == jf::UnitClass::FrontierRanger);
        assert(app.baseState().joinedRecruitIds.count("ranger_recruit"));
        auto skillIt = app.equippedSkills().find("ranger_recruit");
        assert(skillIt != app.equippedSkills().end() && !skillIt->second.equippedSkillIds[0].empty());
        const jf::SkillDefinition* skill = jf::findSkill(skillIt->second.equippedSkillIds[0]);
        assert(skill && skill->unitClass == jf::UnitClass::FrontierRanger && skill->unlockTier == 1);
        assert(skillIt->second.equippedSkillIds[1].empty());
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
        // M10-A (docs/deep_layers.md「分岐/Tier解放を『拠点段階ゲート』から
        // 『素材ゲート』へ変更」): every craft_* weapon-branch node (and its
        // *_forging prerequisite) is pinned to OutpostStage::Encampment, so a
        // still-at-Encampment outpost is only blocked by materials/
        // discoveries, never by requiredStage - unlike an ordinary node
        // (e.g. "simple_forge" itself), which still gates on stage.
        const jf::FacilityNode* craftDuelSword = jf::findFacilityNode("craft_duel_sword");
        assert(craftDuelSword && craftDuelSword->requiredStage == jf::OutpostStage::Encampment);
        const jf::FacilityNode* marchCaptainForging = jf::findFacilityNode("march_captain_forging");
        assert(marchCaptainForging && marchCaptainForging->requiredStage == jf::OutpostStage::Encampment);
        const jf::FacilityNode* simpleForge = jf::findFacilityNode("simple_forge");
        assert(simpleForge && simpleForge->requiredStage == jf::OutpostStage::PioneerOutpost); // unaffected

        jf::BaseState freshBase; // outpostStage == Encampment (default)
        freshBase.discoveryRegistry.insert("quarry_combat_records"); // craft_duel_sword's own required Discovery
        freshBase.addStorage("iron", 3);
        freshBase.addStorage("hide", 1);
        freshBase.unlockedNodeIds.insert("march_captain_forging"); // craft_duel_sword's own prerequisite node
        // No stage gate left to block this: only the (now-satisfied)
        // discovery/prerequisite/material checks matter.
        assert(jf::facilityNodeEligible(freshBase, *craftDuelSword));
        // march_captain_forging itself is craftable from Encampment too (no
        // materials/discoveries of its own beyond "simple_forge" built).
        freshBase.unlockedNodeIds.erase("march_captain_forging");
        freshBase.constructedFacilityIds.insert("simple_forge");
        freshBase.unlockedNodeIds.insert("simple_forge");
        assert(jf::facilityNodeEligible(freshBase, *marchCaptainForging));
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
        testBase.constructedFacilityIds.insert("simple_forge");
        assert(!app.equipWeaponForUnit("player0", "heavy_spear"));
        testBase.unlockedNodeIds.insert("craft_heavy_spear");
        assert(app.equipWeaponForUnit("player0", "heavy_spear"));
        assert(app.weaponOverrides().at("player0") == "heavy_spear");
        assert(app.equipWeaponForUnit("player0", "")); // revert to default
        assert(!app.weaponOverrides().count("player0"));

        testBase.constructedFacilityIds.erase("simple_forge");
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
        // docs/item_system.md「武器と特性の共有」: a crafted branch weapon is a
        // single shared-warehouse copy - equipWeaponForUnit() must reject
        // assigning it to a second unit while a first still holds it.
        jf::GameData data = makeFactoryData();
        data.playerParty[0].classId = jf::UnitClass::Spearman; // "player0"
        data.playerParty[1].classId = jf::UnitClass::Spearman; // "player1"
        jf::GameApp app(data);
        jf::BaseState& testBase = const_cast<jf::BaseState&>(app.baseState());
        testBase.outpostStage = jf::OutpostStage::PioneerOutpost;
        testBase.unlockedNodeIds.insert("simple_forge");
        testBase.constructedFacilityIds.insert("simple_forge");
        testBase.unlockedNodeIds.insert("craft_long_spear");
        testBase.unlockedNodeIds.insert("craft_heavy_spear");

        assert(app.equipWeaponForUnit("player0", "long_spear"));
        assert(!app.equipWeaponForUnit("player1", "long_spear")); // already held by player0
        assert(!app.weaponOverrides().count("player1"));

        assert(app.equipWeaponForUnit("player1", "heavy_spear")); // a different weapon is fine
        assert(app.weaponOverrides().at("player1") == "heavy_spear");

        // iron_spear (the base weapon, not a crafted branch) is exempt - both
        // units may hold it "simultaneously" since each is issued their own.
        assert(app.equipWeaponForUnit("player0", "iron_spear"));
        assert(app.equipWeaponForUnit("player1", "iron_spear"));

        // Freeing player0's long_spear (by switching away from it) lets
        // player1 claim it.
        assert(app.equipWeaponForUnit("player0", "long_spear"));
        assert(app.equipWeaponForUnit("player0", "")); // revert to default, releasing long_spear
        assert(app.equipWeaponForUnit("player1", "long_spear"));
    }

    {
        // M10-A (docs/deep_layers.md「Lv制」): strengthenWeapon() requires
        // simple_forge built and the weapon already crafted, consumes
        // jf::weaponLevelUpCost() from storage, and increments the weapon's
        // Lv - which is not per-unit (BaseState::weaponLevel() is a global
        // weaponId -> Lv lookup, matching the single-shared-copy model above).
        jf::GameData data = makeFactoryData();
        data.playerParty[0].classId = jf::UnitClass::MarchCaptain;
        jf::GameApp app(data);
        jf::BaseState& testBase = const_cast<jf::BaseState&>(app.baseState());
        testBase.outpostStage = jf::OutpostStage::PioneerCity; // clears every requiredStage check
        testBase.unlockedNodeIds.insert("simple_forge");
        testBase.constructedFacilityIds.insert("simple_forge");

        // Not crafted yet - strengthening a weapon nobody owns fails.
        assert(!app.strengthenWeapon("command_sword"));
        assert(app.weaponLevel("command_sword") == 1);

        testBase.unlockedNodeIds.insert("craft_command_sword");
        assert(app.weaponLevel("command_sword") == 1); // still Lv1: crafted, not yet strengthened

        // Lv2 cost (docs/deep_layers.md worked example): iron x2, hide x1.
        assert(!app.strengthenWeapon("command_sword")); // no materials yet
        testBase.addStorage("iron", 2);
        testBase.addStorage("hide", 1);
        assert(app.strengthenWeapon("command_sword"));
        assert(app.weaponLevel("command_sword") == 2);
        assert(testBase.storageCount("iron") == 0);
        assert(testBase.storageCount("hide") == 0);

        // Lv3: iron x3, hide x2.
        testBase.addStorage("iron", 3);
        testBase.addStorage("hide", 1); // deliberately short by 1
        assert(!app.strengthenWeapon("command_sword"));
        assert(app.weaponLevel("command_sword") == 2); // unchanged, nothing partially spent
        assert(testBase.storageCount("iron") == 3); // untouched
        testBase.addStorage("hide", 1);
        assert(app.strengthenWeapon("command_sword"));
        assert(app.weaponLevel("command_sword") == 3);

        // Lv4: iron x3, wood x1, marsh_resin x1.
        testBase.addStorage("iron", 3);
        testBase.addStorage("wood", 1);
        testBase.addStorage("marsh_resin", 1);
        assert(app.strengthenWeapon("command_sword"));
        assert(app.weaponLevel("command_sword") == 4);

        // Lv5: iron x4, wood x1, rare_material x2.
        testBase.addStorage("iron", 4);
        testBase.addStorage("wood", 1);
        testBase.addStorage("rare_material", 2);
        assert(app.strengthenWeapon("command_sword"));
        assert(app.weaponLevel("command_sword") == 5);

        // Lv5 -> Lv6 needs deep-layer materials not implemented in this Slice
        // (weaponLevelUpCost() returns empty past Lv5) - strengthening stops
        // here even with unlimited materials.
        testBase.addStorage("iron", 999);
        testBase.addStorage("wood", 999);
        testBase.addStorage("rare_material", 999);
        assert(!app.strengthenWeapon("command_sword"));
        assert(app.weaponLevel("command_sword") == 5);

        // Cap: manually pinning a weapon at Lv15 (the eventual deep-layer
        // ceiling) must refuse to strengthen further even if a cost existed.
        testBase.weaponLevels["command_sword"] = jf::BaseState::kMaxWeaponLevel;
        assert(!app.strengthenWeapon("command_sword"));

        // A weapon id never wired into weaponLevelEligibleWeapons() never
        // produces a cost, so strengthening it always fails cleanly rather
        // than crashing on a missing table entry. (M10-C registered
        // resonant_focus/etc for all 12 classes, so this now uses a
        // deliberately nonexistent id instead.)
        assert(jf::weaponLevelUpCost("nonexistent_weapon_id", 2).empty());
    }

    {
        // M10-C: same strengthenWeapon()/weaponLevelUpCost() generator,
        // exercised on one of the 6 newly-registered classes (HeavyInfantry /
        // Bulwark Maul) to prove the registration itself (not just the
        // already-tested generator logic) is wired correctly end to end.
        jf::GameData data = makeFactoryData();
        data.playerParty[0].classId = jf::UnitClass::HeavyInfantry;
        jf::GameApp app(data);
        jf::BaseState& testBase = const_cast<jf::BaseState&>(app.baseState());
        testBase.outpostStage = jf::OutpostStage::PioneerCity;
        testBase.unlockedNodeIds.insert("simple_forge");
        testBase.constructedFacilityIds.insert("simple_forge");
        testBase.unlockedNodeIds.insert("heavy_infantry_forging");
        testBase.unlockedNodeIds.insert("craft_bulwark_maul");

        // Lv1 recipe: iron x2, wood x1. Lv2 cost: iron x2 (x1.0), stone x1
        // (otherA).
        assert(!app.strengthenWeapon("bulwark_maul")); // no materials yet
        testBase.addStorage("iron", 2);
        testBase.addStorage("stone", 1);
        assert(app.strengthenWeapon("bulwark_maul"));
        assert(app.weaponLevel("bulwark_maul") == 2);
        assert(testBase.storageCount("iron") == 0);
        assert(testBase.storageCount("stone") == 0);
    }

    {
        // M10-A: weapon Lv applies a flat +1 might per Lv at battle
        // instantiation (docs/deep_layers.md「1Lvあたりの数値」), independent
        // of which unit equips the weapon.
        jf::GameData data = makeFactoryData();
        data.playerParty[0].classId = jf::UnitClass::MarchCaptain;
        jf::GameApp app(data);
        jf::BaseState& testBase = const_cast<jf::BaseState&>(app.baseState());
        testBase.outpostStage = jf::OutpostStage::PioneerCity;
        testBase.unlockedNodeIds.insert("simple_forge");
        testBase.constructedFacilityIds.insert("simple_forge");
        testBase.unlockedNodeIds.insert("craft_command_sword");
        assert(app.equipWeaponForUnit("player0", "command_sword"));
        const int baseMight = data.weaponsById.at("command_sword").might;
        testBase.weaponLevels["command_sword"] = 4; // +3 might over base

        assert(app.startExpedition(jf::RegionId::AshboughForest)); // starts unlocked, no gate needed
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        bool found = false;
        for (const jf::Unit& unit : app.battle().battle().units()) {
            if (unit.id != "player0") continue;
            found = true;
            assert(unit.weapon.id == "command_sword");
            assert(unit.weapon.might == baseMight + 3);
        }
        assert(found);
    }

    {
        // Save/Load: a Save claiming the same crafted weapon for 2 units
        // (hand-edited or from an older schema) must only restore it to one -
        // the lexicographically-first unitId wins deterministically.
        jf::GameData data = makeFactoryData();
        data.playerParty[0].classId = jf::UnitClass::Spearman; // "player0"
        data.playerParty[1].classId = jf::UnitClass::Spearman; // "player1"
        jf::GameApp app(data);
        jf::BaseState& testBase = const_cast<jf::BaseState&>(app.baseState());
        testBase.unlockedNodeIds.insert("simple_forge");
        testBase.constructedFacilityIds.insert("simple_forge");
        testBase.unlockedNodeIds.insert("craft_long_spear");

        jf::SaveData save = app.createSaveData("en");
        save.unitWeaponOverrides["player1"] = "long_spear";
        save.unitWeaponOverrides["player0"] = "long_spear";
        assert(app.applySaveData(save));
        // "player0" < "player1" lexicographically, so it wins the claim.
        assert(app.weaponOverrides().at("player0") == "long_spear");
        assert(!app.weaponOverrides().count("player1"));
    }

    {
        // M10-B (docs/deep_layers.md「防具システム(新設)」「分岐/Tier解放を
        // 『拠点段階ゲート』から『素材ゲート』へ変更」): every craft_armor_*
        // node is pinned to OutpostStage::Encampment, same material-only-gate
        // treatment as weapon branches - no facility-tier visibility gate.
        const jf::FacilityNode* craftArmorTier1 = jf::findFacilityNode("craft_armor_march_captain_tier1");
        assert(craftArmorTier1 && craftArmorTier1->requiredStage == jf::OutpostStage::Encampment);
        assert(craftArmorTier1->weaponBranchClass == jf::UnitClass::MarchCaptain);
        const jf::FacilityNode* craftArmorTier2 = jf::findFacilityNode("craft_armor_march_captain_tier2");
        assert(craftArmorTier2 && craftArmorTier2->requiredStage == jf::OutpostStage::Encampment);
        const jf::FacilityNode* craftArmorTier3 = jf::findFacilityNode("craft_armor_march_captain_tier3");
        assert(craftArmorTier3 && craftArmorTier3->requiredStage == jf::OutpostStage::Encampment);

        jf::BaseState freshBase; // outpostStage == Encampment (default)
        freshBase.discoveryRegistry.insert("cinderwatch_command_drills"); // craft_armor_march_captain_tier1's own gate
        freshBase.addStorage("iron", 2);
        freshBase.constructedFacilityIds.insert("simple_forge");
        freshBase.unlockedNodeIds.insert("simple_forge");
        assert(jf::facilityNodeEligible(freshBase, *craftArmorTier1));
    }

    {
        // Forge armor equipment: equipArmorForUnit() validates class match,
        // requires simple_forge built and the armor's own craft node
        // unlocked, and enforces the shared-warehouse single-owner rule
        // (docs/deep_layers.md「スロットと基本ルール」) exactly like
        // equipWeaponForUnit().
        jf::GameData data = makeFactoryData();
        data.playerParty[0].classId = jf::UnitClass::MarchCaptain; // "player0"
        data.playerParty[1].classId = jf::UnitClass::VeteranGuard; // "player1"
        jf::GameApp app(data);
        assert(!app.equipArmorForUnit("player0", "armor_march_captain_tier1")); // simple_forge not built yet
        jf::BaseState& testBase = const_cast<jf::BaseState&>(app.baseState());
        testBase.unlockedNodeIds.insert("simple_forge");
        testBase.constructedFacilityIds.insert("simple_forge");
        assert(!app.equipArmorForUnit("player0", "armor_march_captain_tier1")); // craft node not unlocked yet
        testBase.unlockedNodeIds.insert("craft_armor_march_captain_tier1");
        // Wrong class: player1 is VeteranGuard, this armor is MarchCaptain-only.
        assert(!app.equipArmorForUnit("player1", "armor_march_captain_tier1"));
        assert(app.equipArmorForUnit("player0", "armor_march_captain_tier1"));
        assert(app.armorOverrides().at("player0") == "armor_march_captain_tier1");
        assert(app.equipArmorForUnit("player0", "")); // unequip
        assert(!app.armorOverrides().count("player0"));

        // Shared-warehouse: a single copy can't be equipped by 2 units.
        data.playerParty[1].classId = jf::UnitClass::MarchCaptain;
        jf::GameApp app2(data);
        jf::BaseState& testBase2 = const_cast<jf::BaseState&>(app2.baseState());
        testBase2.unlockedNodeIds.insert("simple_forge");
        testBase2.constructedFacilityIds.insert("simple_forge");
        testBase2.unlockedNodeIds.insert("craft_armor_march_captain_tier1");
        assert(app2.equipArmorForUnit("player0", "armor_march_captain_tier1"));
        assert(!app2.equipArmorForUnit("player1", "armor_march_captain_tier1"));
        assert(app2.equipArmorForUnit("player0", "")); // release
        assert(app2.equipArmorForUnit("player1", "armor_march_captain_tier1")); // now free
    }

    {
        // M10-B: strengthenArmor() requires simple_forge built and the armor
        // already crafted, consumes jf::armorLevelUpCost() from storage
        // (docs/deep_layers.md「防具Lv1〜5」行軍隊長Tier1 worked example), and
        // increments the armor's Lv - a global armorId -> Lv lookup, same
        // per-id-stack model as weapons.
        jf::GameData data = makeFactoryData();
        data.playerParty[0].classId = jf::UnitClass::MarchCaptain;
        jf::GameApp app(data);
        jf::BaseState& testBase = const_cast<jf::BaseState&>(app.baseState());
        testBase.unlockedNodeIds.insert("simple_forge");
        testBase.constructedFacilityIds.insert("simple_forge");

        // Not crafted yet - strengthening an armor nobody owns fails.
        assert(!app.strengthenArmor("armor_march_captain_tier1"));
        assert(app.armorLevel("armor_march_captain_tier1") == 1);

        testBase.unlockedNodeIds.insert("craft_armor_march_captain_tier1");
        assert(app.armorLevel("armor_march_captain_tier1") == 1); // crafted, not yet strengthened

        // Lv2 cost: iron x1 (primary x0.5, rounded), wood x1 (otherA).
        assert(!app.strengthenArmor("armor_march_captain_tier1")); // no materials yet
        testBase.addStorage("iron", 1);
        testBase.addStorage("wood", 1);
        assert(app.strengthenArmor("armor_march_captain_tier1"));
        assert(app.armorLevel("armor_march_captain_tier1") == 2);
        assert(testBase.storageCount("iron") == 0);
        assert(testBase.storageCount("wood") == 0);

        // Lv3: iron x2, wood x1, cloth x1.
        testBase.addStorage("iron", 2);
        testBase.addStorage("wood", 1); // deliberately short by 1 cloth
        assert(!app.strengthenArmor("armor_march_captain_tier1"));
        assert(app.armorLevel("armor_march_captain_tier1") == 2); // unchanged, nothing partially spent
        assert(testBase.storageCount("iron") == 2); // untouched
        testBase.addStorage("cloth", 1);
        assert(app.strengthenArmor("armor_march_captain_tier1"));
        assert(app.armorLevel("armor_march_captain_tier1") == 3);

        // Lv4: iron x2, wood x2, cloth x1, herb x1.
        testBase.addStorage("iron", 2);
        testBase.addStorage("wood", 2);
        testBase.addStorage("cloth", 1);
        testBase.addStorage("herb", 1);
        assert(app.strengthenArmor("armor_march_captain_tier1"));
        assert(app.armorLevel("armor_march_captain_tier1") == 4);

        // Lv5: iron x2, wood x2, cloth x1, rare_material x2.
        testBase.addStorage("iron", 2);
        testBase.addStorage("wood", 2);
        testBase.addStorage("cloth", 1);
        testBase.addStorage("rare_material", 2);
        assert(app.strengthenArmor("armor_march_captain_tier1"));
        assert(app.armorLevel("armor_march_captain_tier1") == 5);

        // Lv5 -> Lv6 needs deep-layer materials not implemented in this
        // Slice (armorLevelUpCost() returns empty past Lv5).
        testBase.addStorage("iron", 999);
        testBase.addStorage("wood", 999);
        testBase.addStorage("rare_material", 999);
        assert(!app.strengthenArmor("armor_march_captain_tier1"));
        assert(app.armorLevel("armor_march_captain_tier1") == 5);

        // Cap: manually pinning an armor at Lv15 must refuse to strengthen
        // further even if a cost existed.
        testBase.armorLevels["armor_march_captain_tier1"] = jf::BaseState::kMaxArmorLevel;
        assert(!app.strengthenArmor("armor_march_captain_tier1"));

        // An armor id not registered in jf::armorRegistry() never produces a
        // cost. (M10-C registered armor_heavy_infantry_tier1/etc for all 12
        // classes, so this now uses a deliberately nonexistent id instead.)
        assert(jf::armorLevelUpCost("nonexistent_armor_id", "iron", 2).empty());
    }

    {
        // M10-B: armor Lv applies a Lv-scaled DEF/RES bonus at battle
        // instantiation (docs/deep_layers.md「1Lvあたりの数値」), via
        // GameApp::applyArmorBonus() - called at every real-battle entry
        // point alongside applyEquipmentTraits()/applyEquippedSkills().
        jf::GameData data = makeFactoryData();
        data.playerParty[0].classId = jf::UnitClass::MarchCaptain;
        jf::GameApp app(data);
        jf::BaseState& testBase = const_cast<jf::BaseState&>(app.baseState());
        testBase.unlockedNodeIds.insert("simple_forge");
        testBase.constructedFacilityIds.insert("simple_forge");
        testBase.unlockedNodeIds.insert("craft_armor_march_captain_tier1");
        assert(app.equipArmorForUnit("player0", "armor_march_captain_tier1"));
        testBase.armorLevels["armor_march_captain_tier1"] = 5; // Tier1: base(1,1), extra=4 -> def+2, res+2

        const int baseDef = data.classDefinition(jf::UnitClass::MarchCaptain).baseStats.defense;
        const int baseRes = data.classDefinition(jf::UnitClass::MarchCaptain).baseStats.resistance;

        assert(app.startExpedition(jf::RegionId::AshboughForest));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        bool found = false;
        for (const jf::Unit& unit : app.battle().battle().units()) {
            if (unit.id != "player0") continue;
            found = true;
            const jf::ArmorDefinition* armor = jf::findArmorDefinition("armor_march_captain_tier1");
            assert(armor);
            assert(unit.stats.defense == baseDef + jf::armorDefBonusAtLevel(*armor, 5));
            assert(unit.stats.resistance == baseRes + jf::armorResBonusAtLevel(*armor, 5));
            assert(unit.stats.defense == baseDef + 3); // base 1 + (4+1)/2 = 3
            assert(unit.stats.resistance == baseRes + 3); // base 1 + 4/2 = 3
        }
        assert(found);
    }

    {
        // M10-C: same strengthenArmor()/DEF-RES-at-Lv generator, exercised on
        // one of the 6 newly-registered classes' Tier2 (DEF-specialized)
        // piece (BattleMage / Battle Mage Plated Robe) to prove the
        // registration is wired correctly end to end.
        jf::GameData data = makeFactoryData();
        data.playerParty[0].classId = jf::UnitClass::BattleMage;
        jf::GameApp app(data);
        jf::BaseState& testBase = const_cast<jf::BaseState&>(app.baseState());
        testBase.unlockedNodeIds.insert("simple_forge");
        testBase.constructedFacilityIds.insert("simple_forge");
        testBase.unlockedNodeIds.insert("craft_armor_battle_mage_tier2");

        // Lv1 recipe: ruin_fragment x2. Lv2 cost: ruin_fragment x1 (primary
        // x0.5, rounded), stone x1 (otherA).
        assert(!app.strengthenArmor("armor_battle_mage_tier2")); // no materials yet
        testBase.addStorage("ruin_fragment", 1);
        testBase.addStorage("stone", 1);
        assert(app.strengthenArmor("armor_battle_mage_tier2"));
        assert(app.armorLevel("armor_battle_mage_tier2") == 2);
        assert(testBase.storageCount("ruin_fragment") == 0);
        assert(testBase.storageCount("stone") == 0);

        assert(app.equipArmorForUnit("player0", "armor_battle_mage_tier2"));
        const jf::ArmorDefinition* armor = jf::findArmorDefinition("armor_battle_mage_tier2");
        assert(armor);
        assert(jf::armorDefBonusAtLevel(*armor, 2) == 3); // Tier2: base 2 + extra(1) = 3
        assert(jf::armorResBonusAtLevel(*armor, 2) == 0); // Tier2: RES stays at base (0)
    }

    {
        // M10-B (docs/deep_layers.md「防具用調整特性」): the Ward Step armor
        // tuning trait negates exactly the first status effect applied
        // through jf::applyStatusEffect() each battle - equipArmorTraitForUnit()
        // requires "armor_trait_ward_step" unlocked, and the effect is
        // stamped onto the unit by applyArmorBonus() (verified indirectly
        // here via direct Unit state, mirroring the Hide-Wrapped Grip test
        // below).
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        jf::BaseState& testBase = const_cast<jf::BaseState&>(app.baseState());
        assert(!app.equipArmorTraitForUnit("player0", jf::ArmorTuningTraitId::WardStep)); // simple_forge not built
        testBase.unlockedNodeIds.insert("simple_forge");
        testBase.constructedFacilityIds.insert("simple_forge");
        assert(!app.equipArmorTraitForUnit("player0", jf::ArmorTuningTraitId::WardStep)); // trait not unlocked
        testBase.unlockedNodeIds.insert("armor_trait_ward_step");
        assert(app.equipArmorTraitForUnit("player0", jf::ArmorTuningTraitId::WardStep));
        assert(app.equippedArmorTraits().at("player0") == jf::ArmorTuningTraitId::WardStep);
        assert(app.equipArmorTraitForUnit("player0", jf::ArmorTuningTraitId::None)); // unequip
        assert(!app.equippedArmorTraits().count("player0"));

        // Direct effect check: firstStatusNegatesRemaining negates exactly
        // the first applyStatusEffect() call, then behaves normally.
        jf::Unit warded = makeUnit("warded", jf::Team::Player, {1, 1});
        warded.firstStatusNegatesRemaining = 1;
        jf::BattleState battle({warded});
        jf::applyStatusEffect(battle, battle.units()[0], jf::StatusEffectType::Poison);
        assert(battle.units()[0].poisonRemainingProcs == 0); // negated
        assert(battle.units()[0].firstStatusNegatesRemaining == 0);
        jf::applyStatusEffect(battle, battle.units()[0], jf::StatusEffectType::Poison);
        assert(battle.units()[0].poisonRemainingProcs > 0); // applies normally now
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
        // docs/status_effects.md 状態異常「よろめき」主な発生源「障害物への
        // ノックバック衝突」: a knockback that can't reach its destination
        // (here, another unit occupying it) staggers the defender in place
        // instead of just silently doing nothing.
        jf::Unit attacker = makeUnit("attacker", jf::Team::Player, {1, 2});
        jf::Unit defender = makeUnit("defender", jf::Team::Enemy, {1, 3});
        jf::Unit blocker = makeUnit("blocker", jf::Team::Enemy, {1, 4});
        jf::BattleState battle({attacker, defender, blocker});
        battle.applyKnockback(battle.units()[0], battle.units()[1]);
        assert((battle.units()[1].position == jf::GridPos{1, 3})); // unmoved
        assert(battle.units()[1].staggerActive);
    }

    {
        // Same collision-staggers rule, but colliding with a Battle Object
        // Barrier (e.g. a fallen log) rather than a Unit or impassable
        // terrain - previously applyKnockback() didn't check Battle Objects
        // at all, so a knockback could silently push a unit onto/through
        // one.
        jf::Unit attacker = makeUnit("attacker", jf::Team::Player, {1, 2});
        jf::Unit defender = makeUnit("defender", jf::Team::Enemy, {1, 3});
        jf::BattleState battle({attacker, defender});
        jf::BattleObjectDefinition logDef;
        logDef.definitionId = "fallen_log";
        logDef.kind = jf::BattleObjectKind::Barrier;
        logDef.blocksMovement = true;
        battle.registerObjectDefinition(logDef);
        assert(battle.placeObject({"log1", "fallen_log", {1, 4}}));
        battle.applyKnockback(battle.units()[0], battle.units()[1]);
        assert((battle.units()[1].position == jf::GridPos{1, 3})); // unmoved
        assert(battle.units()[1].staggerActive);
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
        // Weapon-branch generalization to the other 11 classes
        // (docs/implementation_roadmap.md "M7項目3(残り) ...特性・武器分岐の
        // 他兵種一般化"): recipe unlock + weapon-stat-swap (incl. MOV where
        // applicable) for each class, mirroring the Spearman tests above.
        // Required Discoveries are seeded directly since several aren't
        // granted by any implemented region content yet (see
        // docs/implementation_status.md).
        struct BranchCheck {
            jf::UnitClass unitClass;
            const char* forgingNode;
            const char* craftNode;
            const char* discovery;
            const char* weaponId;
            int might;
            int minRange;
            int maxRange;
            int moveModifier;
        };
        const BranchCheck checks[] = {
            {jf::UnitClass::MarchCaptain, "march_captain_forging", "craft_command_sword",
             "cinderwatch_command_drills", "command_sword", 4, 1, 1, 0},
            {jf::UnitClass::VeteranGuard, "veteran_guard_forging", "craft_hook_lance",
             "cinderwatch_defense_manual", "hook_lance", 5, 1, 2, 0},
            {jf::UnitClass::WatchArcher, "watch_archer_forging", "craft_long_watch_bow",
             "cinderwatch_watch_records", "long_watch_bow", 3, 3, 4, 0},
            {jf::UnitClass::FrontierScout, "frontier_scout_forging", "craft_trail_blade",
             "ashbough_forest_survey_complete", "trail_blade", 3, 1, 1, 1},
            {jf::UnitClass::DawnChirurgeon, "dawn_chirurgeon_forging", "craft_mercy_staff",
             jf::kHerbThicketDiscovery, "mercy_staff", 1, 1, 2, 0},
            {jf::UnitClass::HeavyInfantry, "heavy_infantry_forging", "craft_driving_maul",
             "impact_balance_record", "driving_maul", 6, 1, 1, 0},
            {jf::UnitClass::FrontierEngineer, "frontier_engineer_forging", "craft_repair_hammer",
             "structural_repair_guide", "repair_hammer", 4, 1, 1, 0},
            {jf::UnitClass::MessengerCavalry, "messenger_cavalry_forging", "craft_road_sabre",
             "courier_route_chart", "road_sabre", 3, 1, 1, 1},
            {jf::UnitClass::FrontierRanger, "frontier_ranger_forging", "craft_driving_bow",
             "herding_shot_method", "driving_bow", 3, 2, 2, 0},
            {jf::UnitClass::BannerBearer, "banner_bearer_forging", "craft_far_standard",
             "long_range_signal_code", "far_standard", 2, 1, 2, 0},
            {jf::UnitClass::BattleMage, "battle_mage_forging", "craft_ember_focus",
             "controlled_ember_formula", "ember_focus", 5, 1, 2, 0},
        };
        for (const BranchCheck& check : checks) {
            jf::GameData data = makeFactoryData();
            data.playerParty[0].classId = check.unitClass;
            jf::GameApp app(data);
            jf::BaseState& testBase = const_cast<jf::BaseState&>(app.baseState());
            testBase.outpostStage = jf::OutpostStage::PioneerCity; // clear every stage gate
            testBase.unlockedNodeIds.insert("simple_forge");
            testBase.constructedFacilityIds.insert("simple_forge");
            testBase.discoveryRegistry.insert(check.discovery);
            assert(!jf::facilityNodeEligible(app.baseState(), *jf::findFacilityNode(check.craftNode)))
                ; // forging branch not unlocked yet
            assert(app.unlockFacilityNode(check.forgingNode));
            const jf::FacilityNode* craftNode = jf::findFacilityNode(check.craftNode);
            assert(craftNode != nullptr);
            assert(craftNode->weaponBranchClass == check.unitClass);
            for (const jf::LootStack& cost : craftNode->materialCosts) testBase.addStorage(cost.id, cost.quantity + 1);
            assert(app.unlockFacilityNode(check.craftNode));
            assert(app.baseState().unlockedNodeIds.count(check.craftNode) == 1);

            assert(app.equipWeaponForUnit("player0", check.weaponId));
            jf::UnitTemplate branchTemplate{"branch_test", "Branch Test", check.unitClass};
            jf::WeaponOverrides overrides{{"branch_test", check.weaponId}};
            jf::Unit branched = jf::instantiateUnit(data, branchTemplate, jf::Team::Player, {0, 0}, &overrides);
            assert(branched.weapon.id == check.weaponId);
            assert(branched.weapon.might == check.might);
            assert(branched.weapon.minRange == check.minRange);
            assert(branched.weapon.maxRange == check.maxRange);
            assert(branched.stats.move ==
                   data.classDefinition(check.unitClass).baseStats.move + check.moveModifier);
        }
    }

    {
        // Driving Maul / Driving Bow: knockback branches reuse the same
        // generic Weapon::causesKnockback hook Heavy Spear already wired.
        jf::Unit attacker = makeUnit("attacker", jf::Team::Player, {1, 2}, 4, jf::UnitClass::HeavyInfantry);
        attacker.weapon.causesKnockback = true;
        jf::Unit defender = makeUnit("defender", jf::Team::Enemy, {1, 3});
        jf::BattleState battle({attacker, defender});
        battle.applyKnockback(battle.units()[0], battle.units()[1]);
        assert((battle.units()[1].position == jf::GridPos{1, 4}));
    }

    {
        // Pinning Bow / Snare Bow / Ember Focus: on-hit status branches reuse
        // the generic Weapon::onHitStatuses hook.
        jf::Unit attacker = makeUnit("attacker", jf::Team::Player, {1, 2}, 4, jf::UnitClass::WatchArcher);
        attacker.weapon.onHitStatuses = {jf::StatusEffectType::MoveDown};
        jf::Unit defender = makeUnit("defender", jf::Team::Enemy, {1, 3});
        jf::BattleState battle({attacker, defender});
        jf::applyWeaponOnHitStatuses(battle, battle.units()[0], battle.units()[1]);
        assert(battle.units()[1].moveDownActive);
    }

    {
        // Scout Network exploration preview: available before the node is
        // unlocked (UI gates its visibility on scoutNetworkUnlocked()), and
        // reflects the same stage-0 enemy count as the real battle.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(startCinderwatchExpedition(app));
        assert(!app.scoutNetworkUnlocked());
        // docs/implementation_roadmap.md M6-A: site 1's real roster (4 units,
        // no enemyCountOverride) replaced the old 3-of-4 placeholder.
        assert(app.explorationEnemyPreview().size() == 4);
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
        source.base.constructedFacilityIds.insert("simple_forge");
        source.base.completedRegionIds.insert(jf::RegionId::AshboughForest);
        source.selectedPartyIds = {"a", "b", "c", "d"};
        source.unitWeaponOverrides["rowan"] = "heavy_spear";
        source.language = "ja";
        std::string error;
        auto restored = jf::deserializeSave(jf::serializeSave(source), &error);
        assert(restored.has_value());
        assert(error.empty());
        assert(restored->base.storageCount("wood") == 7);
        assert(restored->base.constructedFacilityIds.count("simple_forge") == 1);
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
        testBase.constructedFacilityIds.insert("simple_forge");
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
        legacy.base.constructedFacilityIds.insert("simple_forge");
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

        jf::BattleState soloBattle({unit});
        jf::applyMoveDown(soloBattle, soloBattle.units()[0]);
        assert(soloBattle.units()[0].moveDownActive);
        assert(soloBattle.units()[0].effectiveMove() == unit.stats.move - 2);
        jf::Unit lowMove = makeUnit("lowmove", jf::Team::Player, {1, 0}, 1);
        jf::BattleState lowMoveBattle({lowMove});
        jf::applyMoveDown(lowMoveBattle, lowMoveBattle.units()[0]);
        assert(lowMoveBattle.units()[0].effectiveMove() == 1); // 最低MOVは1

        jf::applyDefenseDown(unit);
        assert(unit.effectiveDefense() == std::max(unit.stats.defense - 3, 0));
        assert(unit.stats.resistance == 1); // RESは低下しない

        jf::Unit boss2 = makeUnit("boss2", jf::Team::Enemy, {1, 5});
        boss2.isBoss = true;
        jf::BattleState boss2Battle({boss2});
        jf::applyMoveDown(boss2Battle, boss2Battle.units()[0]);
        jf::applyDefenseDown(boss2Battle.units()[0]);
        assert(boss2Battle.units()[0].effectiveMove() == boss2.stats.move - 1);
        assert(boss2Battle.units()[0].effectiveDefense() == boss2.stats.defense - 2);
    }

    {
        // よろめき: locks movement for a normal unit outright (not merely a
        // penalty), clears when the unit finishes its next action, and
        // cannot be reapplied while immune. A boss instead just takes MOV-1.
        jf::Unit unit = makeUnit("staggered", jf::Team::Player, {1, 0});
        jf::BattleState battle({unit});
        jf::applyStagger(battle, battle.units()[0]);
        assert(battle.units()[0].staggerActive);
        assert(battle.units()[0].effectiveMove() == 0);

        jf::processActionEndStatusEffects(battle, battle.units()[0]);
        assert(!battle.units()[0].staggerActive);
        assert(battle.units()[0].staggerImmune);
        assert(battle.units()[0].effectiveMove() == battle.units()[0].stats.move);

        jf::applyStagger(battle, battle.units()[0]); // no-op while immune
        assert(!battle.units()[0].staggerActive);

        jf::Unit boss = makeUnit("boss", jf::Team::Enemy, {1, 5});
        boss.isBoss = true;
        jf::BattleState bossBattle({boss});
        jf::applyStagger(bossBattle, bossBattle.units()[0]);
        assert(bossBattle.units()[0].effectiveMove() == boss.stats.move - 1); // MOV-1, not a full lock
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
        unit.moveDownActive = true; // not testing applyMoveDown() itself here, just the cure
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
        jf::BattleState battle2({attacker, marked});
        jf::resolveAttack(battle2, battle2.units()[0], battle2.units()[1], 0, false); // miss: must not consume the mark
        assert(battle2.units()[1].markedBonusDamage == 2);
        jf::resolveAttack(battle2, battle2.units()[0], battle2.units()[1], 0, true); // hit: consumes it
        assert(battle2.units()[1].markedBonusDamage == 0);
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
        // docs/initial_skill_effects.md 古参守備兵`immovable_stance`(不動の
        // 構え)'s raw field composition: DEF+3 and MOV forced to 0 while
        // active, regardless of any other buff/debuff also present.
        jf::Unit unit = makeUnit("guard", jf::Team::Player, {1, 0}, 4);
        unit.stats.defense = 5;
        assert(unit.effectiveMove() == 4 && unit.effectiveDefense() == 5);
        unit.immovableStanceActive = true;
        assert(unit.effectiveMove() == 0); // no movement on the next action
        assert(unit.effectiveDefense() == 8); // 5 + 3
        jf::applyDefenseDown(unit); // stacks normally with the debuff
        assert(unit.effectiveDefense() == 5); // 5 + 3 - 3 (statusDefenseDownAmount)
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
        // M4 item 3 (Preview/Resolverの一致): attack-shape Skillはconfirmを
        // 挟むようになった - ConfirmAttack同様、Confirm前にPreviewを見せる。
        assert(controller.inputState() == jf::BattleInputState::ConfirmSkillAttack);
        auto preview = controller.pendingSkillPreview();
        assert(preview.has_value());
        controller.confirmSkillAttack();
        const jf::Unit* hit = controller.battle().findUnit("inRangeEnemy");
        assert(hit->currentHp < maxHpBefore); // a normal attack landed
        assert(maxHpBefore - hit->currentHp == preview->damage); // Preview matched the real result
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
        assert(controller.inputState() == jf::BattleInputState::ConfirmSkillAttack);
        auto preview = controller.pendingSkillPreview();
        assert(preview.has_value());
        controller.confirmSkillAttack();
        const jf::Unit* hit = controller.battle().findUnit("inRangeEnemy");
        assert(hit->currentHp < maxHpBefore);
        assert(maxHpBefore - hit->currentHp == preview->damage); // Preview matched the real result
        assert(hit->moveDownActive);
        assert(!jf::skillSlotAvailable(*controller.battle().findUnit("spearman"), 0)); // CD2, just used
    }

    {
        // confirmSkillAttack() must apply weapon-level effects the same way
        // confirmAttack()/EnemyAI.cpp's attack paths do - here, Heavy Spear's
        // causesKnockback (docs/base_development.md "Heavy Spear: 火力型...
        // 命中時に相手を1マスノックバック"). A Spearman using halting_thrust
        // with Heavy Spear equipped must knock the target back exactly like
        // a normal attack with the same weapon would.
        jf::Unit spearman = makeUnit("spearman", jf::Team::Player, {1, 2}, 4, jf::UnitClass::Spearman);
        spearman.weapon = {.id = "heavy_spear", .name = "Heavy Spear", .might = 8, .minRange = 1, .maxRange = 2,
                          .damageType = jf::DamageType::Physical, .causesKnockback = true};
        spearman.skillSlots[0].skillId = "halting_thrust";
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 3});
        jf::BattleState battle({spearman, enemy});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.selectSkillTarget(jf::GridPos{1, 3}));
        assert(controller.inputState() == jf::BattleInputState::ConfirmSkillAttack);
        controller.confirmSkillAttack();
        assert((controller.battle().findUnit("enemy")->position == jf::GridPos{1, 4})); // pushed back 1 tile
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
        assert(controller.inputState() == jf::BattleInputState::ConfirmSkillAttack);
        auto preview = controller.pendingSkillPreview();
        assert(preview.has_value());
        assert(preview->damage == normalDamage + 3); // Preview already folds in the flat bonus
        controller.confirmSkillAttack();
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
        // docs/initial_skill_effects.md 古参守備兵`immovable_stance`(不動の
        // 構え): the 12th skill, and the first Passive - no chooseSkill()
        // step at all, it auto-triggers on chooseWait(). Confirms the "until
        // this unit's own next action ends" timing survives an intervening
        // Enemy Phase and only clears once that next action resolves.
        jf::Unit guard = makeUnit("guard", jf::Team::Player, {1, 3}, 3, jf::UnitClass::VeteranGuard);
        guard.stats.defense = 5;
        guard.skillSlots[0].skillId = "immovable_stance";
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {2, 7});
        jf::BattleState battle({guard, enemy});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseWait();
        assert(controller.battle().findUnit("guard")->immovableStanceActive);
        assert(controller.battle().findUnit("guard")->effectiveDefense() == 8); // 5 + 3

        for (int i = 0; i < 10 && controller.inputState() == jf::BattleInputState::EnemyTurn; ++i)
            controller.update(1.0f);
        assert(controller.inputState() == jf::BattleInputState::SelectUnit);
        assert(controller.battle().findUnit("guard")->immovableStanceActive); // survives the Enemy Phase
        assert(controller.battle().findUnit("guard")->effectiveMove() == 0); // forced no movement this action

        // The next action must be something other than Wait: Wait itself
        // re-triggers this Always/Passive skill (it fires on every Wait
        // confirmation, not a one-shot resource), so a second Wait would
        // just re-grant it rather than let it expire. A different action
        // kind (here: placing a board) is what actually clears it.
        controller.selectUnit(*controller.battle().findUnit("guard"));
        controller.selectMoveTile(controller.battle().findUnit("guard")->position);
        controller.chooseProtectiveBoard();
        assert(controller.inputState() == jf::BattleInputState::SelectBoardTarget);
        assert(controller.selectBoardTarget(controller.boardTargetTiles().front()));
        assert(!controller.battle().findUnit("guard")->immovableStanceActive);
    }

    {
        // docs/initial_skill_effects.md 辺境斥候`emergency_withdrawal`(緊急
        // 離脱): the 13th skill, and the first "self movement" shape - the
        // destination is an empty tile, not a Unit. Fixed budget of 3
        // regardless of MOV, ignores Zone of Control entirely ("敵隣接から
        // 開始可能"), but still respects normal occupancy rules for path
        // expansion (enemies block passage entirely, allies can be crossed
        // but not landed on) - same as Movement.cpp's computeReachableTiles.
        // MOV set to 3 (matching the skill's own fixed budget) so a detour
        // around the guard's ZoC costs more than either can afford - the
        // only manhattan-distance-3 path from {1,0} to {1,3} is the straight
        // row, cleanly isolating "does this ignore ZoC" from occupancy
        // effects and from the two budgets simply differing.
        jf::Unit scout = makeUnit("scout", jf::Team::Player, {1, 0}, 3, jf::UnitClass::FrontierScout);
        scout.skillSlots[0].skillId = "emergency_withdrawal";
        jf::Unit unactedAlly = makeUnit("unactedAlly", jf::Team::Player, {2, 7});
        jf::Unit guard = makeUnit("guard", jf::Team::Enemy, {0, 2}, 3, jf::UnitClass::VeteranGuard);
        jf::BattleState battle({scout, unactedAlly, guard});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        // Normal movement is stopped by the guard's ZoC at {1,2} - it can
        // enter that tile but not continue past it to {1,3}.
        const auto normalReach = jf::computeReachableTiles(controller.battle(), controller.battle().units()[0]);
        assert(contains(normalReach, jf::GridPos{1, 2}));
        assert(!contains(normalReach, jf::GridPos{1, 3}));

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.inputState() == jf::BattleInputState::SelectSkillTarget);
        // Budget of 3, ZoC ignored: {1,3} (distance 3, straight through the
        // guard's ZoC) is reachable even though normal movement can't get
        // there. {1,4} (distance 4) is out of budget.
        assert(contains(controller.skillTargetTiles(), jf::GridPos{1, 3}));
        assert(!contains(controller.skillTargetTiles(), jf::GridPos{1, 4}));

        assert(controller.selectSkillTarget(jf::GridPos{1, 3}));
        assert((controller.battle().findUnit("scout")->position == jf::GridPos{1, 3}));
        assert(!jf::skillSlotAvailable(*controller.battle().findUnit("scout"), 0)); // Cooldown2, just used
    }

    {
        // Occupancy-during-expansion for emergency_withdrawal: an enemy
        // sitting on the only path tile blocks passage entirely (not just
        // landing), while an ally on that same tile can be crossed. This is
        // the bug this skill's BFS initially had (it only filtered occupancy
        // in the final result, not during frontier expansion).
        jf::Unit scout = makeUnit("scout", jf::Team::Player, {1, 0}, 5, jf::UnitClass::FrontierScout);
        scout.skillSlots[0].skillId = "emergency_withdrawal";
        jf::Unit blocker = makeUnit("blocker", jf::Team::Enemy, {1, 2});
        jf::BattleState enemyBlockedBattle({scout, blocker});
        jf::BattleController enemyBlockedController(std::move(enemyBlockedBattle));
        jf::initializeSkillCharges(enemyBlockedController.battle().units()[0]);
        enemyBlockedController.selectUnit(enemyBlockedController.battle().units()[0]);
        enemyBlockedController.selectMoveTile(enemyBlockedController.battle().units()[0].position);
        enemyBlockedController.chooseSkill(0);
        assert(!contains(enemyBlockedController.skillTargetTiles(), jf::GridPos{1, 2})); // occupied, can't land
        assert(!contains(enemyBlockedController.skillTargetTiles(), jf::GridPos{1, 3})); // blocked passage

        jf::Unit scout2 = makeUnit("scout2", jf::Team::Player, {1, 0}, 5, jf::UnitClass::FrontierScout);
        scout2.skillSlots[0].skillId = "emergency_withdrawal";
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {1, 2});
        jf::BattleState allyCrossedBattle({scout2, ally});
        jf::BattleController allyCrossedController(std::move(allyCrossedBattle));
        jf::initializeSkillCharges(allyCrossedController.battle().units()[0]);
        allyCrossedController.selectUnit(allyCrossedController.battle().units()[0]);
        allyCrossedController.selectMoveTile(allyCrossedController.battle().units()[0].position);
        allyCrossedController.chooseSkill(0);
        assert(!contains(allyCrossedController.skillTargetTiles(), jf::GridPos{1, 2})); // occupied, can't land
        assert(contains(allyCrossedController.skillTargetTiles(), jf::GridPos{1, 3})); // crossable
    }

    {
        // docs/class_reference.md「後半6兵種」/M7項目1: 重装兵(HeavyInfantry)の
        // Class/武器データ整合性。加入経路(M7項目2)はまだ無いため
        // playerParty/reserveRosterには登場しないが、Classとしては完全に有効。
        jf::GameData data = makeFactoryData();
        const jf::ClassDefinition& def = data.classDefinition(jf::UnitClass::HeavyInfantry);
        assert(def.baseStats.maxHp == 32 && def.baseStats.strength == 8 && def.baseStats.magic == 0 &&
              def.baseStats.speed == 2 && def.baseStats.defense == 12 && def.baseStats.resistance == 2 &&
              def.baseStats.move == 3);
        assert(def.weaponId == "iron_greathammer");
        const jf::Weapon& weapon = data.weaponFor(jf::UnitClass::HeavyInfantry);
        assert(weapon.id == "iron_greathammer" && weapon.might == 7 && weapon.minRange == 1 && weapon.maxRange == 1 &&
              weapon.damageType == jf::DamageType::Physical);
        assert(jf::unitClassFromString("HeavyInfantry") == jf::UnitClass::HeavyInfantry);
        assert(jf::skillsForClass(jf::UnitClass::HeavyInfantry).size() == 3);
        assert(jf::requiredTrainingNodeIdFor(jf::UnitClass::HeavyInfantry) == "vanguard_training");
    }

    {
        // docs/class_reference.md「重量装甲」: this engine's knockback is
        // always exactly 1 tile, so "reduce distance by 1, floor 0" means
        // HeavyInfantry never gets knocked back at all - unconditionally,
        // unlike Hide-Wrapped Grip's consumable knockbackNegatesRemaining
        // (which this must NOT touch).
        jf::Unit attacker = makeUnit("attacker", jf::Team::Player, {1, 2});
        jf::Unit defender = makeUnit("defender", jf::Team::Enemy, {1, 3}, 4, jf::UnitClass::HeavyInfantry);
        jf::BattleState battle({attacker, defender});
        battle.applyKnockback(battle.units()[0], battle.units()[1]);
        assert((battle.units()[1].position == jf::GridPos{1, 3})); // unmoved
        assert(!battle.units()[1].staggerActive); // not even staggered - the knockback never happens at all
        assert(battle.units()[1].knockbackNegatesRemaining == 0); // the consumable counter was never touched
    }

    {
        // 重装兵`brace_for_impact`(衝撃防御): same auto-trigger-on-Wait shape
        // as immovable_stance, but DEF+3 + full knockback negation - unlike
        // immovable_stance, movement is NOT restricted on the next action.
        jf::Unit heavy = makeUnit("heavy", jf::Team::Player, {1, 3}, 3, jf::UnitClass::HeavyInfantry);
        heavy.stats.defense = 12;
        heavy.skillSlots[0].skillId = "brace_for_impact";
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {2, 7});
        jf::BattleState battle({heavy, enemy});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseWait();
        assert(controller.battle().findUnit("heavy")->braceForImpactActive);
        assert(controller.battle().findUnit("heavy")->effectiveDefense() == 15); // 12 + 3
        assert(controller.battle().findUnit("heavy")->effectiveMove() == 3); // NOT forced to 0, unlike immovable_stance

        for (int i = 0; i < 10 && controller.inputState() == jf::BattleInputState::EnemyTurn; ++i)
            controller.update(1.0f);
        assert(controller.inputState() == jf::BattleInputState::SelectUnit);
        assert(controller.battle().findUnit("heavy")->braceForImpactActive); // survives the Enemy Phase

        controller.selectUnit(*controller.battle().findUnit("heavy"));
        controller.selectMoveTile(controller.battle().findUnit("heavy")->position);
        controller.chooseProtectiveBoard();
        assert(controller.inputState() == jf::BattleInputState::SelectBoardTarget);
        assert(controller.selectBoardTarget(controller.boardTargetTiles().front()));
        assert(!controller.battle().findUnit("heavy")->braceForImpactActive); // cleared after the next action
    }

    {
        // 重装兵`armor_advance`(装甲前進): same self-movement shape as
        // emergency_withdrawal, budget 2, ignoring Zone of Control.
        jf::Unit heavy = makeUnit("heavy", jf::Team::Player, {1, 0}, 3, jf::UnitClass::HeavyInfantry);
        heavy.skillSlots[0].skillId = "armor_advance";
        jf::Unit guard = makeUnit("guard", jf::Team::Enemy, {0, 1}, 3, jf::UnitClass::VeteranGuard);
        jf::BattleState battle({heavy, guard});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        // Normal movement (MOV 3) is stopped by the guard's ZoC at {1,1}.
        const auto normalReach = jf::computeReachableTiles(controller.battle(), controller.battle().units()[0]);
        assert(contains(normalReach, jf::GridPos{1, 1}));
        assert(!contains(normalReach, jf::GridPos{1, 2}));

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.inputState() == jf::BattleInputState::SelectSkillTarget);
        // Budget 2, ZoC ignored: {1,2} (distance 2, straight through the
        // guard's ZoC) is reachable even though normal movement can't get
        // there. {1,3} (distance 3) is out of the skill's fixed budget.
        assert(contains(controller.skillTargetTiles(), jf::GridPos{1, 2}));
        assert(!contains(controller.skillTargetTiles(), jf::GridPos{1, 3}));

        assert(controller.selectSkillTarget(jf::GridPos{1, 2}));
        assert((controller.battle().findUnit("heavy")->position == jf::GridPos{1, 2}));
        assert(!jf::skillSlotAvailable(*controller.battle().findUnit("heavy"), 0)); // Cooldown2, just used
    }

    {
        // 重装兵`break_obstacle`(障害物破砕): destroys an adjacent
        // destructible Object instantly, regardless of remaining durability -
        // unlike a normal Object attack, no damage roll at all. canBeAttacked
        // == false objects (mission-critical/indestructible) aren't offered
        // as targets, and it's OncePerBattle (can't be reused).
        jf::Unit heavy = makeUnit("heavy", jf::Team::Player, {1, 3}, 4, jf::UnitClass::HeavyInfantry);
        heavy.skillSlots[0].skillId = "break_obstacle";
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {2, 7});
        jf::BattleState battle({heavy, enemy});
        jf::BattleObjectDefinition logDef;
        logDef.definitionId = "fallen_log";
        logDef.kind = jf::BattleObjectKind::Barrier;
        logDef.blocksMovement = true;
        logDef.canBeAttacked = true;
        logDef.maxDurability = 16; // high durability - an instant kill must ignore this entirely
        battle.registerObjectDefinition(logDef);
        assert(battle.placeObject({"log1", "fallen_log", {1, 4}}));
        jf::BattleObjectDefinition indestructibleDef;
        indestructibleDef.definitionId = "indestructible_wall";
        indestructibleDef.kind = jf::BattleObjectKind::Barrier;
        indestructibleDef.blocksMovement = true;
        indestructibleDef.canBeAttacked = false;
        battle.registerObjectDefinition(indestructibleDef);
        assert(battle.placeObject({"wall1", "indestructible_wall", {1, 2}}));

        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);
        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.inputState() == jf::BattleInputState::SelectSkillTarget);
        assert(contains(controller.skillTargetTiles(), jf::GridPos{1, 4}));
        assert(!contains(controller.skillTargetTiles(), jf::GridPos{1, 2})); // canBeAttacked == false

        assert(controller.selectSkillTarget(jf::GridPos{1, 4}));
        assert(controller.battle().findObject("log1")->state == jf::BattleObjectStateKind::Destroyed);
        assert(controller.battle().findObject("log1")->durability == 0);
        assert(!jf::skillSlotAvailable(*controller.battle().findUnit("heavy"), 0)); // OncePerBattle, just used
    }

    {
        // docs/class_reference.md「後半6兵種」/M7項目1: 辺境工兵
        // (FrontierEngineer)のClass/武器データ整合性。
        jf::GameData data = makeFactoryData();
        const jf::ClassDefinition& def = data.classDefinition(jf::UnitClass::FrontierEngineer);
        assert(def.baseStats.maxHp == 21 && def.baseStats.strength == 6 && def.baseStats.magic == 1 &&
              def.baseStats.speed == 5 && def.baseStats.defense == 5 && def.baseStats.resistance == 4 &&
              def.baseStats.move == 4);
        assert(def.weaponId == "engineer_hammer");
        const jf::Weapon& weapon = data.weaponFor(jf::UnitClass::FrontierEngineer);
        assert(weapon.id == "engineer_hammer" && weapon.might == 5 && weapon.minRange == 1 && weapon.maxRange == 1 &&
              weapon.damageType == jf::DamageType::Physical);
        assert(jf::unitClassFromString("FrontierEngineer") == jf::UnitClass::FrontierEngineer);
        assert(jf::skillsForClass(jf::UnitClass::FrontierEngineer).size() == 3);
        assert(jf::requiredTrainingNodeIdFor(jf::UnitClass::FrontierEngineer) == "specialist_training");
    }

    {
        // 辺境工兵「野戦工作」: once-per-battle Active固有能力, own dedicated
        // command outside the 2 equip slots (same shape as canHeal()/
        // chooseHeal()). Places a durability-10 barricade on an adjacent
        // empty tile; a 2nd use is refused via fieldFortificationUsed.
        jf::Unit engineer = makeUnit("engineer", jf::Team::Player, {1, 3}, 4, jf::UnitClass::FrontierEngineer);
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {2, 7});
        jf::BattleState battle({engineer, enemy});
        battle.registerObjectDefinition(jf::BattleObjectDefinition{.definitionId = "field_barricade",
                                                                    .kind = jf::BattleObjectKind::Barrier,
                                                                    .maxDurability = 10,
                                                                    .blocksMovement = true,
                                                                    .canBeAttacked = true,
                                                                    .canBeRepaired = true});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseFieldFortification();
        assert(controller.inputState() == jf::BattleInputState::SelectFieldFortificationTarget);
        assert(contains(controller.fieldFortificationTiles(), jf::GridPos{1, 4}));

        controller.selectFieldFortificationTarget(jf::GridPos{1, 4});
        const jf::BattleObjectState* placed = controller.battle().objectAt(jf::GridPos{1, 4});
        assert(placed && placed->definitionId == "field_barricade" && placed->durability == 10);
        assert(controller.battle().findUnit("engineer")->fieldFortificationUsed);

        for (int i = 0; i < 10 && controller.inputState() == jf::BattleInputState::EnemyTurn; ++i)
            controller.update(1.0f);
        controller.selectUnit(*controller.battle().findUnit("engineer"));
        controller.selectMoveTile(controller.battle().findUnit("engineer")->position);
        controller.chooseFieldFortification();
        assert(controller.inputState() == jf::BattleInputState::SelectAction); // no-op, already used
    }

    {
        // 辺境工兵`field_repair`(野戦補修): heals a damaged adjacent friendly
        // placed object's durability by 6 (capped at max). Full-durability
        // and enemy-team objects aren't offered as targets.
        jf::Unit engineer = makeUnit("engineer", jf::Team::Player, {1, 3}, 4, jf::UnitClass::FrontierEngineer);
        engineer.skillSlots[0].skillId = "field_repair";
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {2, 7});
        jf::BattleState battle({engineer, enemy});
        jf::BattleObjectDefinition barricadeDef{.definitionId = "field_barricade",
                                                .kind = jf::BattleObjectKind::Barrier,
                                                .maxDurability = 10,
                                                .blocksMovement = true,
                                                .canBeAttacked = true,
                                                .canBeRepaired = true};
        battle.registerObjectDefinition(barricadeDef);
        assert(battle.placeObject({"dmg1", "field_barricade", {1, 4}, jf::BattleObjectTeam::Player,
                                   jf::BattleObjectStateKind::Active, 4, 0}));
        assert(battle.placeObject({"full1", "field_barricade", {1, 2}, jf::BattleObjectTeam::Player,
                                   jf::BattleObjectStateKind::Active, 10, 0}));
        assert(battle.placeObject({"enemy1", "field_barricade", {0, 3}, jf::BattleObjectTeam::Enemy,
                                   jf::BattleObjectStateKind::Active, 4, 0}));

        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);
        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.inputState() == jf::BattleInputState::SelectSkillTarget);
        assert(contains(controller.skillTargetTiles(), jf::GridPos{1, 4})); // damaged, friendly
        assert(!contains(controller.skillTargetTiles(), jf::GridPos{1, 2})); // already full
        assert(!contains(controller.skillTargetTiles(), jf::GridPos{0, 3})); // enemy team

        assert(controller.selectSkillTarget(jf::GridPos{1, 4}));
        assert(controller.battle().findObject("dmg1")->durability == 10); // 4 + 6, capped at max
    }

    {
        // 辺境工兵`rubble_charge`(瓦礫爆破): destroys a range-2 breakable
        // Object instantly and deals 3 fixed damage to enemies directly
        // above/below it, ignoring DEF. OncePerBattle.
        jf::Unit engineer = makeUnit("engineer", jf::Team::Player, {1, 3}, 4, jf::UnitClass::FrontierEngineer);
        engineer.skillSlots[0].skillId = "rubble_charge";
        jf::BattleState battle({engineer});
        jf::BattleObjectDefinition logDef;
        logDef.definitionId = "fallen_log";
        logDef.kind = jf::BattleObjectKind::Barrier;
        logDef.blocksMovement = true;
        logDef.canBeAttacked = true;
        logDef.maxDurability = 16;
        battle.registerObjectDefinition(logDef);
        assert(battle.placeObject({"log1", "fallen_log", {1, 5}}));
        jf::Unit enemyAbove = makeUnit("enemyAbove", jf::Team::Enemy, {0, 5});
        jf::Unit enemyBelow = makeUnit("enemyBelow", jf::Team::Enemy, {2, 5});
        battle.units().push_back(enemyAbove);
        battle.units().push_back(enemyBelow);

        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);
        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.inputState() == jf::BattleInputState::SelectSkillTarget);
        assert(contains(controller.skillTargetTiles(), jf::GridPos{1, 5})); // range 2

        int hpBeforeAbove = controller.battle().findUnit("enemyAbove")->currentHp;
        int hpBeforeBelow = controller.battle().findUnit("enemyBelow")->currentHp;
        assert(controller.selectSkillTarget(jf::GridPos{1, 5}));
        assert(controller.battle().findObject("log1")->state == jf::BattleObjectStateKind::Destroyed);
        assert(controller.battle().findUnit("enemyAbove")->currentHp == hpBeforeAbove - 3);
        assert(controller.battle().findUnit("enemyBelow")->currentHp == hpBeforeBelow - 3);
        assert(!jf::skillSlotAvailable(*controller.battle().findUnit("engineer"), 0)); // OncePerBattle, just used
    }

    {
        // 辺境工兵`rapid_barricade`(即席防壁): places a durability-6 barricade
        // on a range-2 empty tile that auto-expires at the next allied
        // Phase start, and shares a combined 2-object cap with
        // `field_barricade`.
        jf::Unit engineer = makeUnit("engineer", jf::Team::Player, {1, 3}, 4, jf::UnitClass::FrontierEngineer);
        engineer.skillSlots[0].skillId = "rapid_barricade";
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {2, 7});
        jf::BattleState battle({engineer, enemy});
        battle.registerObjectDefinition(jf::BattleObjectDefinition{.definitionId = "field_barricade",
                                                                    .kind = jf::BattleObjectKind::Barrier,
                                                                    .maxDurability = 10,
                                                                    .blocksMovement = true,
                                                                    .canBeAttacked = true,
                                                                    .canBeRepaired = true});
        battle.registerObjectDefinition(jf::BattleObjectDefinition{.definitionId = "rapid_barricade",
                                                                    .kind = jf::BattleObjectKind::Barrier,
                                                                    .maxDurability = 6,
                                                                    .blocksMovement = true,
                                                                    .canBeAttacked = true,
                                                                    .canBeRepaired = true});
        // Cap test setup: 2 barricades already on the field.
        assert(battle.placeObject({"cap1", "field_barricade", {0, 6}, jf::BattleObjectTeam::Player,
                                   jf::BattleObjectStateKind::Active, 10, 0}));
        assert(battle.placeObject({"cap2", "rapid_barricade", {0, 7}, jf::BattleObjectTeam::Player,
                                   jf::BattleObjectStateKind::Active, 6, 0}));

        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);
        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.inputState() == jf::BattleInputState::SelectAction); // cap reached, no target offered

        // Destroy one existing barricade to drop below the cap, then place.
        controller.battle().findObject("cap2")->state = jf::BattleObjectStateKind::Destroyed;
        controller.chooseSkill(0);
        assert(controller.inputState() == jf::BattleInputState::SelectSkillTarget);
        assert(contains(controller.skillTargetTiles(), jf::GridPos{1, 5})); // range 2, empty

        assert(controller.selectSkillTarget(jf::GridPos{1, 5}));
        const jf::BattleObjectState* placed = controller.battle().objectAt(jf::GridPos{1, 5});
        assert(placed && placed->definitionId == "rapid_barricade" && placed->durability == 6);

        for (int i = 0; i < 10 && controller.inputState() == jf::BattleInputState::EnemyTurn; ++i)
            controller.update(1.0f);
        assert(controller.inputState() == jf::BattleInputState::SelectUnit); // next Player Phase started
        const jf::BattleObjectState* afterExpiry = controller.battle().objectAt(jf::GridPos{1, 5});
        assert(afterExpiry && afterExpiry->state == jf::BattleObjectStateKind::Destroyed); // auto-expired
    }

    {
        // docs/class_reference.md「後半6兵種」/M7項目1: 伝令騎兵
        // (MessengerCavalry)のClass/武器データ整合性。
        jf::GameData data = makeFactoryData();
        const jf::ClassDefinition& def = data.classDefinition(jf::UnitClass::MessengerCavalry);
        assert(def.baseStats.maxHp == 22 && def.baseStats.strength == 7 && def.baseStats.magic == 0 &&
              def.baseStats.speed == 9 && def.baseStats.defense == 4 && def.baseStats.resistance == 3 &&
              def.baseStats.move == 6);
        assert(def.weaponId == "messenger_sword");
        const jf::Weapon& weapon = data.weaponFor(jf::UnitClass::MessengerCavalry);
        assert(weapon.id == "messenger_sword" && weapon.might == 5 && weapon.minRange == 1 && weapon.maxRange == 1 &&
              weapon.damageType == jf::DamageType::Physical);
        assert(jf::unitClassFromString("MessengerCavalry") == jf::UnitClass::MessengerCavalry);
        assert(jf::skillsForClass(jf::UnitClass::MessengerCavalry).size() == 3);
        assert(jf::requiredTrainingNodeIdFor(jf::UnitClass::MessengerCavalry) == "mobility_training");
    }

    {
        // 伝令騎兵「再移動」: after a normal Attack, if still alive, defers into
        // SelectReMoveTarget (budget 2) instead of finishing immediately -
        // the current tile (=stay) is always a valid choice.
        jf::Unit cavalry = makeUnit("cavalry", jf::Team::Player, {1, 3}, 6, jf::UnitClass::MessengerCavalry);
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 4});
        enemy.stats.maxHp = 40;
        enemy.currentHp = 40; // survives the hit
        jf::BattleState battle({cavalry, enemy});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseAttack();
        assert(controller.inputState() == jf::BattleInputState::SelectTarget);
        controller.selectTargetTile(jf::GridPos{1, 4});
        assert(controller.inputState() == jf::BattleInputState::ConfirmAttack);
        controller.confirmAttack();
        assert(controller.inputState() == jf::BattleInputState::SelectReMoveTarget);
        assert(contains(controller.reMoveTiles(), jf::GridPos{1, 3})); // staying is always valid
        assert(contains(controller.reMoveTiles(), jf::GridPos{1, 1})); // 2 tiles away, unobstructed
        assert(!controller.battle().findUnit("cavalry")->hasActed); // not yet - still pending re-move

        controller.selectReMoveTarget(jf::GridPos{1, 3}); // choose to stay
        assert(controller.inputState() != jf::BattleInputState::SelectReMoveTarget);
        assert(controller.battle().findUnit("cavalry")->hasActed);
        assert((controller.battle().findUnit("cavalry")->position == jf::GridPos{1, 3}));
    }

    {
        // 伝令騎兵`urgent_dispatch`(緊急伝令): 射程2の味方1人にMOV+2を付与。
        // 使用後も再移動が発生し(スキル行動も対象)、その再移動は敵
        // 古参守備兵のZone of Controlに従う - 装甲前進(ZoC無視)とは異なる点を
        // HeavyInfantry armor_advanceテストと同じ座標({1,0}起点、Guardが
        // {0,1})で確認する({1,2}へは budget2でも届かない)。
        jf::Unit cavalry = makeUnit("cavalry", jf::Team::Player, {1, 0}, 6, jf::UnitClass::MessengerCavalry);
        cavalry.skillSlots[0].skillId = "urgent_dispatch";
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {2, 0});
        jf::Unit guard = makeUnit("guard", jf::Team::Enemy, {0, 1}, 3, jf::UnitClass::VeteranGuard);
        jf::BattleState battle({cavalry, ally, guard});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.inputState() == jf::BattleInputState::SelectSkillTarget);
        assert(contains(controller.skillTargetTiles(), jf::GridPos{2, 0})); // range 2
        assert(!contains(controller.skillTargetTiles(), jf::GridPos{1, 0})); // can't target self

        assert(controller.selectSkillTarget(jf::GridPos{2, 0}));
        assert(controller.battle().findUnit("ally")->urgentDispatchActive);
        assert(controller.battle().findUnit("ally")->effectiveMove() == 4 + 2); // MarchCaptain base 4

        assert(controller.inputState() == jf::BattleInputState::SelectReMoveTarget); // skill action also re-moves
        assert(contains(controller.reMoveTiles(), jf::GridPos{1, 0})); // self
        assert(!contains(controller.reMoveTiles(), jf::GridPos{1, 2})); // ZoC-blocked, unlike armor_advance
        controller.selectReMoveTarget(jf::GridPos{1, 0});
        assert(!jf::skillSlotAvailable(*controller.battle().findUnit("cavalry"), 0)); // Cooldown2, just used
    }

    {
        // 伝令騎兵`ride_through`(駆け抜け): self-only, 戦闘1回。使用直後の
        // 再移動予算が2ではなく4マスへ増加する。
        jf::Unit cavalry = makeUnit("cavalry", jf::Team::Player, {1, 0}, 6, jf::UnitClass::MessengerCavalry);
        cavalry.skillSlots[0].skillId = "ride_through";
        jf::BattleState battle({cavalry});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.inputState() == jf::BattleInputState::SelectReMoveTarget); // self-only, resolves at once
        assert(contains(controller.reMoveTiles(), jf::GridPos{1, 4})); // budget 4
        assert(!contains(controller.reMoveTiles(), jf::GridPos{1, 5})); // beyond budget 4

        controller.selectReMoveTarget(jf::GridPos{1, 4});
        assert(!jf::skillSlotAvailable(*controller.battle().findUnit("cavalry"), 0)); // OncePerBattle, just used
    }

    {
        // 伝令騎兵`rescue_transfer`(救援搬送): 隣接味方1人を反対側の空きマスへ
        // 1マス移動させる。対象のhasActedは変化しない。重装兵・ボス・敵は
        // 対象にならない。
        jf::Unit cavalry = makeUnit("cavalry", jf::Team::Player, {1, 3}, 6, jf::UnitClass::MessengerCavalry);
        cavalry.skillSlots[0].skillId = "rescue_transfer";
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {1, 4}); // adjacent, opposite tile {1,2} is empty
        jf::Unit heavy = makeUnit("heavy", jf::Team::Player, {0, 3}, 3, jf::UnitClass::HeavyInfantry);
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {2, 3});
        jf::BattleState battle({cavalry, ally, heavy, enemy});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.inputState() == jf::BattleInputState::SelectSkillTarget);
        assert(contains(controller.skillTargetTiles(), jf::GridPos{1, 4})); // ally
        assert(!contains(controller.skillTargetTiles(), jf::GridPos{0, 3})); // 重装兵
        assert(!contains(controller.skillTargetTiles(), jf::GridPos{2, 3})); // enemy

        assert(controller.selectSkillTarget(jf::GridPos{1, 4}));
        assert((controller.battle().findUnit("ally")->position == jf::GridPos{1, 2})); // reflected
        assert(!controller.battle().findUnit("ally")->hasActed); // ally's own action state untouched

        assert(controller.inputState() == jf::BattleInputState::SelectReMoveTarget); // caster's own re-move
        controller.selectReMoveTarget(jf::GridPos{1, 3});
        assert(controller.battle().findUnit("cavalry")->hasActed);
    }

    {
        // docs/class_reference.md「後半6兵種」/M7項目1: 辺境猟兵
        // (FrontierRanger)のClass/武器データ整合性。
        jf::GameData data = makeFactoryData();
        const jf::ClassDefinition& def = data.classDefinition(jf::UnitClass::FrontierRanger);
        assert(def.baseStats.maxHp == 20 && def.baseStats.strength == 6 && def.baseStats.magic == 0 &&
              def.baseStats.speed == 7 && def.baseStats.defense == 4 && def.baseStats.resistance == 4 &&
              def.baseStats.move == 4);
        assert(def.weaponId == "hunting_bow");
        const jf::Weapon& weapon = data.weaponFor(jf::UnitClass::FrontierRanger);
        assert(weapon.id == "hunting_bow" && weapon.might == 4 && weapon.minRange == 2 && weapon.maxRange == 2 &&
              weapon.damageType == jf::DamageType::Physical);
        assert(jf::unitClassFromString("FrontierRanger") == jf::UnitClass::FrontierRanger);
        assert(jf::skillsForClass(jf::UnitClass::FrontierRanger).size() == 3);
        assert(jf::requiredTrainingNodeIdFor(jf::UnitClass::FrontierRanger) == "mobility_training");
    }

    {
        // 辺境猟兵「簡易罠」/`snare_trap`(拘束罠): both place the same
        // "ranger_trap" Object and share a combined cap of 2. Only an
        // enemy's own movement (EnemyAI.cpp, via triggerRangerTrapIfPresent())
        // triggers it - a player unit stepping on it does nothing.
        // (2 separate ranger units so the 2nd placement isn't blocked by the
        // 1st's hasActed - the cap is a battle-wide Object count, not a
        // per-unit limit.)
        jf::Unit ranger1 = makeUnit("ranger1", jf::Team::Player, {1, 3}, 4, jf::UnitClass::FrontierRanger);
        jf::Unit ranger2 = makeUnit("ranger2", jf::Team::Player, {1, 0}, 4, jf::UnitClass::FrontierRanger);
        ranger2.skillSlots[0].skillId = "snare_trap";
        jf::Unit ranger3 = makeUnit("ranger3", jf::Team::Player, {1, 7}, 4, jf::UnitClass::FrontierRanger);
        ranger3.skillSlots[0].skillId = "snare_trap";
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {1, 6});
        jf::Unit distantEnemy = makeUnit("distantEnemy", jf::Team::Enemy, {0, 0});
        jf::BattleState battle({ranger1, ranger2, ranger3, ally, distantEnemy});
        battle.registerObjectDefinition(
            jf::BattleObjectDefinition{.definitionId = "ranger_trap", .kind = jf::BattleObjectKind::Marker,
                                       .canOccupy = true});
        jf::BattleController controller(std::move(battle));
        for (jf::Unit& u : controller.battle().units()) jf::initializeSkillCharges(u);

        // 固有能力で1個目。
        controller.selectUnit(*controller.battle().findUnit("ranger1"));
        controller.selectMoveTile(controller.battle().findUnit("ranger1")->position);
        controller.chooseSimpleTrap();
        assert(controller.inputState() == jf::BattleInputState::SelectSimpleTrapTarget);
        controller.selectSimpleTrapTarget(jf::GridPos{1, 4});
        assert(controller.battle().objectAt(jf::GridPos{1, 4}) != nullptr);
        assert(controller.battle().findUnit("ranger1")->simpleTrapUsed);

        // スキルで2個目 - 合計2個の上限に達する。
        controller.selectUnit(*controller.battle().findUnit("ranger2"));
        controller.selectMoveTile(controller.battle().findUnit("ranger2")->position);
        controller.chooseSkill(0);
        assert(controller.inputState() == jf::BattleInputState::SelectSkillTarget);
        assert(controller.selectSkillTarget(jf::GridPos{1, 1}));
        assert(controller.battle().objectAt(jf::GridPos{1, 1}) != nullptr);

        // 上限到達により3個目は置けない(固有能力・スキルどちらも対象なし)。
        controller.selectUnit(*controller.battle().findUnit("ranger3"));
        controller.selectMoveTile(controller.battle().findUnit("ranger3")->position);
        controller.chooseSimpleTrap();
        assert(controller.inputState() == jf::BattleInputState::SelectAction); // cap reached, no-op
        controller.chooseSkill(0);
        assert(controller.inputState() == jf::BattleInputState::SelectAction); // cap reached, no-op

        // 味方が罠を踏んでも発動しない。
        jf::Unit* allyUnit = controller.battle().findUnit("ally");
        assert(!allyUnit->moveDownActive);
        controller.battle().moveUnit(*allyUnit, jf::GridPos{1, 4});
        assert(!allyUnit->moveDownActive);
        assert(controller.battle().objectAt(jf::GridPos{1, 4})->state == jf::BattleObjectStateKind::Active);

        // 敵の自発移動でのみ発動する。
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 3}, 1);
        jf::Unit farPlayer = makeUnit("farPlayer", jf::Team::Player, {1, 5}, 1);
        jf::BattleState trapBattle({enemy, farPlayer});
        trapBattle.registerObjectDefinition(
            jf::BattleObjectDefinition{.definitionId = "ranger_trap", .kind = jf::BattleObjectKind::Marker,
                                       .canOccupy = true});
        assert(trapBattle.placeObject({"trap1", "ranger_trap", {1, 4}, jf::BattleObjectTeam::Player,
                                       jf::BattleObjectStateKind::Active, 0, 0}));
        jf::takeEnemyTurn(trapBattle, trapBattle.units()[0]);
        assert((trapBattle.findUnit("enemy")->position == jf::GridPos{1, 4})); // stepped onto the trap
        assert(trapBattle.findUnit("enemy")->moveDownActive);
        assert(trapBattle.findObject("trap1")->state == jf::BattleObjectStateKind::Destroyed);
    }

    {
        // 辺境猟兵`read_quarry`(獲物を読む): 純粋なデータフラグ - 対象の
        // quarryRevealedが立ち、次のEnemy Phase終了で解除される。
        jf::Unit ranger = makeUnit("ranger", jf::Team::Player, {1, 0}, 4, jf::UnitClass::FrontierRanger);
        ranger.skillSlots[0].skillId = "read_quarry";
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 3});
        jf::BattleState battle({ranger, enemy});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.inputState() == jf::BattleInputState::SelectSkillTarget);
        assert(contains(controller.skillTargetTiles(), jf::GridPos{1, 3})); // range 3

        assert(controller.selectSkillTarget(jf::GridPos{1, 3}));
        assert(controller.battle().findUnit("enemy")->quarryRevealed);
        assert(!jf::skillSlotAvailable(*controller.battle().findUnit("ranger"), 0)); // OncePerBattle

        controller.endPlayerTurn();
        for (int i = 0; i < 10 && controller.inputState() == jf::BattleInputState::EnemyTurn; ++i)
            controller.update(1.0f);
        assert(!controller.battle().findUnit("enemy")->quarryRevealed); // cleared at Enemy Phase end
    }

    {
        // 辺境猟兵`driving_shot`(追い込み射撃): 通常攻撃+命中時に対象を離れる
        // 方向へ1マス押し出す。重装兵/brace_for_impactは無効化し、押し出し先が
        // 塞がっている場合はよろめきを付与せず何もしない(通常ノックバックとの
        // 違い)。
        jf::Unit ranger = makeUnit("ranger", jf::Team::Player, {1, 0}, 4, jf::UnitClass::FrontierRanger);
        ranger.weapon = {.id = "hunting_bow", .name = "Hunting Bow", .might = 4, .minRange = 2,
                         .maxRange = 2, .damageType = jf::DamageType::Physical};
        ranger.skillSlots[0].skillId = "driving_shot";
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 2});
        enemy.stats.maxHp = 40;
        enemy.currentHp = 40;
        jf::BattleState battle({ranger, enemy});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.inputState() == jf::BattleInputState::SelectSkillTarget);
        assert(controller.selectSkillTarget(jf::GridPos{1, 2}));
        assert(controller.inputState() == jf::BattleInputState::ConfirmSkillAttack);
        controller.confirmSkillAttack();
        assert((controller.battle().findUnit("enemy")->position == jf::GridPos{1, 3})); // pushed away
    }

    {
        // driving_shot: 重装兵は押し出しを無効化する。
        jf::Unit ranger = makeUnit("ranger", jf::Team::Player, {1, 0}, 4, jf::UnitClass::FrontierRanger);
        ranger.weapon = {.id = "hunting_bow", .name = "Hunting Bow", .might = 4, .minRange = 2,
                         .maxRange = 2, .damageType = jf::DamageType::Physical};
        ranger.skillSlots[0].skillId = "driving_shot";
        jf::Unit heavy = makeUnit("heavy", jf::Team::Enemy, {1, 2}, 3, jf::UnitClass::HeavyInfantry);
        heavy.stats.maxHp = 40;
        heavy.currentHp = 40;
        jf::BattleState battle({ranger, heavy});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.selectSkillTarget(jf::GridPos{1, 2}));
        controller.confirmSkillAttack();
        assert((controller.battle().findUnit("heavy")->position == jf::GridPos{1, 2})); // unmoved
    }

    {
        // driving_shot: 押し出し先が塞がっていれば、よろめきを付与せず通常
        // ダメージだけ与える(通常ノックバックとの違い)。
        jf::Unit ranger = makeUnit("ranger", jf::Team::Player, {1, 0}, 4, jf::UnitClass::FrontierRanger);
        ranger.weapon = {.id = "hunting_bow", .name = "Hunting Bow", .might = 4, .minRange = 2,
                         .maxRange = 2, .damageType = jf::DamageType::Physical};
        ranger.skillSlots[0].skillId = "driving_shot";
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 2});
        enemy.stats.maxHp = 40;
        enemy.currentHp = 40;
        jf::Unit blocker = makeUnit("blocker", jf::Team::Enemy, {1, 3});
        jf::BattleState battle({ranger, enemy, blocker});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.selectSkillTarget(jf::GridPos{1, 2}));
        controller.confirmSkillAttack();
        assert((controller.battle().findUnit("enemy")->position == jf::GridPos{1, 2})); // blocked, unmoved
        assert(!controller.battle().findUnit("enemy")->staggerActive); // no stagger, unlike a real knockback
    }

    {
        // docs/class_reference.md「後半6兵種」/M7項目1: 旗手(BannerBearer)の
        // Class/武器データ整合性。
        jf::GameData data = makeFactoryData();
        const jf::ClassDefinition& def = data.classDefinition(jf::UnitClass::BannerBearer);
        assert(def.baseStats.maxHp == 22 && def.baseStats.strength == 5 && def.baseStats.magic == 2 &&
              def.baseStats.speed == 5 && def.baseStats.defense == 5 && def.baseStats.resistance == 6 &&
              def.baseStats.move == 4);
        assert(def.weaponId == "banner_spear");
        const jf::Weapon& weapon = data.weaponFor(jf::UnitClass::BannerBearer);
        assert(weapon.id == "banner_spear" && weapon.might == 4 && weapon.minRange == 1 && weapon.maxRange == 2 &&
              weapon.damageType == jf::DamageType::Physical);
        assert(jf::unitClassFromString("BannerBearer") == jf::UnitClass::BannerBearer);
        assert(jf::skillsForClass(jf::UnitClass::BannerBearer).size() == 3);
        assert(jf::requiredTrainingNodeIdFor(jf::UnitClass::BannerBearer) == "specialist_training");
    }

    {
        // 旗手「戦旗」: マンハッタン距離2以内の味方STR/MAGを+1する常時Aura。
        // 旗手自身は対象外、敵は対象外、距離3では効かない、複数いても+1のまま。
        jf::Unit attacker = makeUnit("attacker", jf::Team::Player, {1, 0});
        jf::Unit bearer = makeUnit("bearer", jf::Team::Player, {1, 2}, 4, jf::UnitClass::BannerBearer);
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 5});
        std::vector<jf::Unit> units{attacker, bearer, enemy};
        assert(jf::bannerAuraBonus(units, units[0]) == 1); // attacker: distance 2
        assert(jf::bannerAuraBonus(units, units[1]) == 0); // bearer itself excluded
        assert(jf::bannerAuraBonus(units, units[2]) == 0); // enemy team, irrelevant anyway

        jf::Unit far = makeUnit("far", jf::Team::Player, {1, 7});
        units.push_back(far);
        assert(jf::bannerAuraBonus(units, units[3]) == 0); // distance 5, out of range

        jf::Unit bearer2 = makeUnit("bearer2", jf::Team::Player, {1, 1}, 4, jf::UnitClass::BannerBearer);
        units.push_back(bearer2);
        assert(jf::bannerAuraBonus(units, units[0]) == 1); // 2 bearers in range, still +1

        const int baseline = jf::computeDamage(attacker, enemy, 0);
        const int boosted = jf::computeDamage(attacker, enemy, 0, jf::bannerAuraBonus(units, attacker));
        assert(boosted == baseline + 1);
    }

    {
        // 旗手`rallying_banner`(奮起の旗): 距離2以内の味方(自身含む)へ
        // DEF+1/RES+1、次のEnemy Phase終了で解除。
        jf::Unit bearer = makeUnit("bearer", jf::Team::Player, {1, 3}, 4, jf::UnitClass::BannerBearer);
        bearer.skillSlots[0].skillId = "rallying_banner";
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {1, 5}); // distance 2
        jf::Unit farAlly = makeUnit("farAlly", jf::Team::Player, {1, 7}); // distance 4, out of range
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {2, 7});
        jf::BattleState battle({bearer, ally, farAlly, enemy});
        jf::BattleController controller(std::move(battle));
        for (jf::Unit& u : controller.battle().units()) jf::initializeSkillCharges(u);

        controller.selectUnit(*controller.battle().findUnit("bearer"));
        controller.selectMoveTile(controller.battle().findUnit("bearer")->position);
        controller.chooseSkill(0);
        assert(controller.battle().findUnit("bearer")->rallyingBannerActive);
        assert(controller.battle().findUnit("bearer")->effectiveDefense() == 2 + 1); // makeUnit()'s default DEF
        assert(controller.battle().findUnit("bearer")->effectiveResistance() == 1 + 1); // makeUnit()'s default RES
        assert(controller.battle().findUnit("ally")->rallyingBannerActive);
        assert(!controller.battle().findUnit("farAlly")->rallyingBannerActive);

        controller.endPlayerTurn();
        for (int i = 0; i < 10 && controller.inputState() == jf::BattleInputState::EnemyTurn; ++i)
            controller.update(1.0f);
        assert(!controller.battle().findUnit("bearer")->rallyingBannerActive); // cleared at Enemy Phase end
        assert(!controller.battle().findUnit("ally")->rallyingBannerActive);
    }

    {
        // 旗手`marching_rhythm`(行軍の律動): 距離2以内の未行動の味方のMOV+1、
        // このPlayer Phase終了で解除。既に行動済みの味方は対象外。
        jf::Unit bearer = makeUnit("bearer", jf::Team::Player, {1, 3}, 4, jf::UnitClass::BannerBearer);
        bearer.skillSlots[0].skillId = "marching_rhythm";
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {1, 5}); // distance 2, unacted
        jf::Unit actedAlly = makeUnit("actedAlly", jf::Team::Player, {1, 4}); // distance 1, but already acted
        actedAlly.hasActed = true;
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {2, 7});
        jf::BattleState battle({bearer, ally, actedAlly, enemy});
        jf::BattleController controller(std::move(battle));
        for (jf::Unit& u : controller.battle().units()) jf::initializeSkillCharges(u);

        controller.selectUnit(*controller.battle().findUnit("bearer"));
        controller.selectMoveTile(controller.battle().findUnit("bearer")->position);
        controller.chooseSkill(0);
        assert(controller.battle().findUnit("ally")->moveUpActive);
        assert(controller.battle().findUnit("ally")->effectiveMove() == 4 + 1);
        assert(!controller.battle().findUnit("actedAlly")->moveUpActive); // already acted, excluded
        assert(!jf::skillSlotAvailable(*controller.battle().findUnit("bearer"), 0)); // OncePerBattle, just used

        controller.endPlayerTurn();
        assert(!controller.battle().findUnit("ally")->moveUpActive); // cleared at this Player Phase end
    }

    {
        // 旗手`unyielding_signal`(不退の合図): 距離2以内の味方が受ける最初の
        // 移動低下/よろめきを無効化し、1Phase1回のチャージを消費する。射程外は
        // 保護しない。
        jf::Unit bearer = makeUnit("bearer", jf::Team::Player, {1, 3}, 4, jf::UnitClass::BannerBearer);
        bearer.skillSlots[0].skillId = "unyielding_signal";
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {1, 5}); // distance 2
        jf::Unit farAlly = makeUnit("farAlly", jf::Team::Player, {1, 7}); // distance 4, unprotected
        jf::BattleState battle({bearer, ally, farAlly});
        for (jf::Unit& u : battle.units()) jf::initializeSkillCharges(u);
        assert(jf::skillSlotAvailable(battle.units()[0], 0));

        jf::applyMoveDown(battle, *battle.findUnit("ally"));
        assert(!battle.findUnit("ally")->moveDownActive); // negated
        assert(!jf::skillSlotAvailable(battle.units()[0], 0)); // charge consumed

        jf::applyMoveDown(battle, *battle.findUnit("farAlly"));
        assert(battle.findUnit("farAlly")->moveDownActive); // out of range, not protected (and charge spent anyway)

        // よろめきも同様に無効化する。
        jf::Unit bearer2 = makeUnit("bearer2", jf::Team::Player, {1, 3}, 4, jf::UnitClass::BannerBearer);
        bearer2.skillSlots[0].skillId = "unyielding_signal";
        jf::Unit ally2 = makeUnit("ally2", jf::Team::Player, {1, 4});
        jf::BattleState battle2({bearer2, ally2});
        for (jf::Unit& u : battle2.units()) jf::initializeSkillCharges(u);
        jf::applyStagger(battle2, *battle2.findUnit("ally2"));
        assert(!battle2.findUnit("ally2")->staggerActive);

        // 毒は対象外(consumeUnyieldingSignalIfAvailableを経由しない)。
        jf::applyPoison(*battle2.findUnit("ally2"));
        assert(battle2.findUnit("ally2")->poisonRemainingProcs > 0);
    }

    {
        // docs/class_reference.md「後半6兵種」/M7項目1: 戦闘魔導士(BattleMage)の
        // Class/武器データ整合性。希少な名前付き加入クラスのため通常訓練ゲート無し。
        jf::GameData data = makeFactoryData();
        const jf::ClassDefinition& def = data.classDefinition(jf::UnitClass::BattleMage);
        assert(def.baseStats.maxHp == 16 && def.baseStats.strength == 1 && def.baseStats.magic == 9 &&
              def.baseStats.speed == 5 && def.baseStats.defense == 2 && def.baseStats.resistance == 7 &&
              def.baseStats.move == 4);
        assert(def.weaponId == "arcane_focus");
        const jf::Weapon& weapon = data.weaponFor(jf::UnitClass::BattleMage);
        assert(weapon.id == "arcane_focus" && weapon.might == 6 && weapon.minRange == 1 && weapon.maxRange == 2 &&
              weapon.damageType == jf::DamageType::Magical);
        assert(jf::unitClassFromString("BattleMage") == jf::UnitClass::BattleMage);
        assert(jf::skillsForClass(jf::UnitClass::BattleMage).size() == 3);
        assert(jf::requiredTrainingNodeIdFor(jf::UnitClass::BattleMage) == "");
    }

    {
        // 戦闘魔導士「魔力波及」: 戦闘中1回、通常攻撃(常にMagical武器)命中後、
        // 対象の上下隣接する敵へ固定3ダメージ。味方には影響せず、2回目は発動しない。
        jf::Unit mage = makeUnit("mage", jf::Team::Player, {1, 3}, 4, jf::UnitClass::BattleMage);
        mage.weapon = {.id = "arcane_focus", .name = "Arcane Focus", .might = 6, .minRange = 1,
                      .maxRange = 2, .damageType = jf::DamageType::Magical};
        jf::Unit target = makeUnit("target", jf::Team::Enemy, {1, 5});
        target.stats.maxHp = 40;
        target.currentHp = 40;
        jf::Unit enemyAbove = makeUnit("enemyAbove", jf::Team::Enemy, {0, 5});
        jf::Unit allyBelow = makeUnit("allyBelow", jf::Team::Player, {2, 5});
        jf::BattleState battle({mage, target, enemyAbove, allyBelow});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseAttack();
        assert(controller.inputState() == jf::BattleInputState::SelectTarget);
        controller.selectTargetTile(jf::GridPos{1, 5});
        assert(controller.inputState() == jf::BattleInputState::ConfirmAttack);
        const int hpBeforeAbove = controller.battle().findUnit("enemyAbove")->currentHp;
        const int hpBeforeAllyBelow = controller.battle().findUnit("allyBelow")->currentHp;
        controller.confirmAttack();
        assert(controller.battle().findUnit("enemyAbove")->currentHp == hpBeforeAbove - 3);
        assert(controller.battle().findUnit("allyBelow")->currentHp == hpBeforeAllyBelow); // ally untouched
        assert(controller.battle().findUnit("mage")->arcaneOverflowUsed);

        // 戦闘中1回のため2回目は発動しない。
        jf::Unit* mageUnit = controller.battle().findUnit("mage");
        mageUnit->hasActed = false;
        controller.selectUnit(*mageUnit);
        controller.selectMoveTile(mageUnit->position);
        controller.chooseAttack();
        controller.selectTargetTile(jf::GridPos{1, 5});
        const int hpBeforeAbove2 = controller.battle().findUnit("enemyAbove")->currentHp;
        controller.confirmAttack();
        assert(controller.battle().findUnit("enemyAbove")->currentHp == hpBeforeAbove2); // no more splash
    }

    {
        // 戦闘魔導士`arc_burst`(連鎖魔弾): スキル攻撃命中後、対象の上下隣接の敵へ
        // 固定2ダメージ。
        jf::Unit mage = makeUnit("mage", jf::Team::Player, {1, 3}, 4, jf::UnitClass::BattleMage);
        mage.weapon = {.id = "arcane_focus", .name = "Arcane Focus", .might = 6, .minRange = 1,
                      .maxRange = 2, .damageType = jf::DamageType::Magical};
        mage.skillSlots[0].skillId = "arc_burst";
        jf::Unit target = makeUnit("target", jf::Team::Enemy, {1, 5});
        target.stats.maxHp = 40;
        target.currentHp = 40;
        jf::Unit enemyAbove = makeUnit("enemyAbove", jf::Team::Enemy, {0, 5});
        jf::BattleState battle({mage, target, enemyAbove});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.inputState() == jf::BattleInputState::SelectSkillTarget);
        assert(controller.selectSkillTarget(jf::GridPos{1, 5}));
        assert(controller.inputState() == jf::BattleInputState::ConfirmSkillAttack);
        const int hpBeforeAbove = controller.battle().findUnit("enemyAbove")->currentHp;
        controller.confirmSkillAttack();
        assert(controller.battle().findUnit("enemyAbove")->currentHp == hpBeforeAbove - 2);
    }

    {
        // 戦闘魔導士`ward_break`(魔防破砕): 命中後、対象のmagicMarkedBonusDamageが
        // 3になり、次の魔法攻撃でのみ消費される(物理攻撃では消費されない)。
        jf::Unit mage = makeUnit("mage", jf::Team::Player, {1, 0}, 4, jf::UnitClass::BattleMage);
        mage.weapon = {.id = "arcane_focus", .name = "Arcane Focus", .might = 6, .minRange = 1,
                      .maxRange = 2, .damageType = jf::DamageType::Magical};
        mage.skillSlots[0].skillId = "ward_break";
        jf::Unit target = makeUnit("target", jf::Team::Enemy, {1, 2});
        target.stats.maxHp = 40;
        target.currentHp = 40;
        jf::BattleState battle({mage, target});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.selectSkillTarget(jf::GridPos{1, 2}));
        assert(controller.inputState() == jf::BattleInputState::ConfirmSkillAttack);
        controller.confirmSkillAttack();
        jf::Unit marked = *controller.battle().findUnit("target");
        assert(marked.magicMarkedBonusDamage == 3);

        // 物理攻撃では消費されない。
        jf::Unit physAttacker = makeUnit("physAttacker", jf::Team::Player, {1, 3});
        jf::BattleState physBattle({physAttacker, marked});
        jf::resolveAttack(physBattle, physBattle.units()[0], physBattle.units()[1], 0, true);
        assert(physBattle.units()[1].magicMarkedBonusDamage == 3); // untouched by the physical hit

        // 魔法攻撃でのみ消費される。
        jf::Unit magicAttacker = makeUnit("magicAttacker", jf::Team::Player, {1, 3});
        magicAttacker.weapon.damageType = jf::DamageType::Magical;
        jf::Unit unmarked = physBattle.units()[1];
        unmarked.magicMarkedBonusDamage = 0;
        const int withoutBonus = jf::computeDamage(magicAttacker, unmarked, 0);
        const int withBonus = jf::computeDamage(magicAttacker, physBattle.units()[1], 0);
        assert(withBonus == withoutBonus + 3);

        jf::BattleState magicBattle({magicAttacker, physBattle.units()[1]});
        jf::resolveAttack(magicBattle, magicBattle.units()[0], magicBattle.units()[1], 0, true);
        assert(magicBattle.units()[1].magicMarkedBonusDamage == 0); // consumed by the magic hit
    }

    {
        // 戦闘魔導士`scorch_ground`(地表灼熱): 射程3の空きマスへ設置でき、浅瀬は
        // 対象にならない。次の自軍Phase開始時にそのマスにいるユニットへ炎上が
        // 付与され設置物がDestroyedになる。
        jf::Unit mage = makeUnit("mage", jf::Team::Player, {1, 3}, 4, jf::UnitClass::BattleMage);
        mage.skillSlots[0].skillId = "scorch_ground";
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {2, 7});
        jf::BattleState battle({mage, enemy});
        battle.registerObjectDefinition(
            jf::BattleObjectDefinition{.definitionId = "scorch_mark", .kind = jf::BattleObjectKind::Marker,
                                       .canOccupy = true});
        battle.setTerrain(jf::GridPos{1, 6}, jf::TerrainType::Shallows); // within range 3
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.inputState() == jf::BattleInputState::SelectSkillTarget);
        assert(contains(controller.skillTargetTiles(), jf::GridPos{1, 5})); // range 3, empty
        assert(!contains(controller.skillTargetTiles(), jf::GridPos{1, 6})); // Shallows excluded

        assert(controller.selectSkillTarget(jf::GridPos{1, 5}));
        const jf::BattleObjectState* mark = controller.battle().objectAt(jf::GridPos{1, 5});
        assert(mark && mark->definitionId == "scorch_mark" && mark->state == jf::BattleObjectStateKind::Active);

        // そのマスにいるユニットへPhase開始時に炎上が付与され、設置物が消滅する。
        jf::Unit* enemyUnit = controller.battle().findUnit("enemy");
        enemyUnit->position = jf::GridPos{1, 5};
        enemyUnit->hasActed = true; // avoid AI moving it away from the marked tile
        controller.endPlayerTurn();
        for (int i = 0; i < 10 && controller.inputState() == jf::BattleInputState::EnemyTurn; ++i)
            controller.update(1.0f);
        assert(controller.battle().findUnit("enemy")->burnRemainingProcs > 0);
        const jf::BattleObjectState* markAfter = controller.battle().objectAt(jf::GridPos{1, 5});
        assert(markAfter && markAfter->state == jf::BattleObjectStateKind::Destroyed);
    }

    {
        // 戦闘魔導士`scorch_ground`: 誰もいなければ炎上せず、設置物は消滅だけする。
        jf::Unit mage = makeUnit("mage", jf::Team::Player, {1, 3}, 4, jf::UnitClass::BattleMage);
        mage.skillSlots[0].skillId = "scorch_ground";
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {2, 0}, 0); // MOV 0, stays put
        jf::BattleState battle({mage, enemy});
        battle.registerObjectDefinition(
            jf::BattleObjectDefinition{.definitionId = "scorch_mark", .kind = jf::BattleObjectKind::Marker,
                                       .canOccupy = true});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.selectSkillTarget(jf::GridPos{1, 5}));

        controller.battle().findUnit("enemy")->hasActed = true;
        controller.endPlayerTurn();
        for (int i = 0; i < 10 && controller.inputState() == jf::BattleInputState::EnemyTurn; ++i)
            controller.update(1.0f);
        assert(!controller.battle().findUnit("enemy")->burnRemainingProcs); // nobody was there
        const jf::BattleObjectState* markAfter = controller.battle().objectAt(jf::GridPos{1, 5});
        assert(markAfter && markAfter->state == jf::BattleObjectStateKind::Destroyed); // still expires
    }

    {
        // docs/initial_skill_effects.md 槍兵`spear_wall`(槍壁): the 14th
        // skill - reuses the buff shape table via a new `alsoSelf` flag
        // (self AND one chosen adjacent ally both receive the buff, unlike
        // every prior single-target buff which grants it to whichever ONE
        // unit is picked). Grants the same conditional DEF+2 as the
        // Spearman class's baseline Brace trait (only against an attacker
        // who moved 2+ tiles this action) to a unit that doesn't already
        // have it - here, a non-Spearman ally.
        jf::Unit spearman = makeUnit("spearman", jf::Team::Player, {1, 1}, 4, jf::UnitClass::Spearman);
        spearman.skillSlots[0].skillId = "spear_wall";
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {1, 2}); // MarchCaptain: no baseline Brace
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {2, 7}); // keeps the battle from instantly ending
        jf::BattleState battle({spearman, ally, enemy});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.inputState() == jf::BattleInputState::SelectSkillTarget);
        assert(!contains(controller.skillTargetTiles(), jf::GridPos{1, 1})); // self not selectable
        assert(contains(controller.skillTargetTiles(), jf::GridPos{1, 2})); // adjacent ally selectable

        assert(controller.selectSkillTarget(jf::GridPos{1, 2}));
        jf::Unit* buffedSpearman = controller.battle().findUnit("spearman");
        jf::Unit* buffedAlly = controller.battle().findUnit("ally");
        assert(buffedSpearman->braceSkillActive); // self also received it
        assert(buffedAlly->braceSkillActive);
        assert(!jf::skillSlotAvailable(*buffedSpearman, 0)); // Cooldown2, just used

        jf::Unit chargingAttacker = makeUnit("chargingAttacker", jf::Team::Enemy, {1, 3});
        chargingAttacker.tilesMovedThisAction = 2;
        assert(controller.battle().combatDefenseBonus(*buffedAlly, chargingAttacker) == 2);
        jf::Unit stillAttacker = makeUnit("stillAttacker", jf::Team::Enemy, {1, 3});
        assert(controller.battle().combatDefenseBonus(*buffedAlly, stillAttacker) == 0); // attacker didn't move 2+
    }

    {
        // docs/initial_skill_effects.md 古参守備兵`provoke`(挑発): the 15th
        // skill -敵1体・射程2、Damageなし、Mark形状に似るが書き込む値が
        // Damageの符号付き整数ではなく「使用者のid」なので専用分岐にした。
        // Effect is consumed by EnemyAI.cpp's takeEnemyTurn(), not by
        // anything in BattleController itself.
        jf::Unit guard = makeUnit("guard", jf::Team::Player, {1, 1}, 4, jf::UnitClass::VeteranGuard);
        guard.skillSlots[0].skillId = "provoke";
        // Closer to the enemy than guard is (distance 1 vs. 2), so without
        // provoke findNearestPlayer() would pick nearPlayer instead.
        jf::Unit nearPlayer = makeUnit("nearPlayer", jf::Team::Player, {1, 4});
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 3});
        jf::BattleState battle({guard, nearPlayer, enemy});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        assert(controller.inputState() == jf::BattleInputState::SelectSkillTarget);
        assert(contains(controller.skillTargetTiles(), jf::GridPos{1, 3})); // enemy, within range 2
        assert(!contains(controller.skillTargetTiles(), jf::GridPos{1, 4})); // ally, never a valid target

        assert(controller.selectSkillTarget(jf::GridPos{1, 3}));
        assert(controller.battle().findUnit("enemy")->provokedByUnitId == "guard");
        assert(!jf::skillSlotAvailable(*controller.battle().findUnit("guard"), 0)); // Cooldown2, just used

        // Without provoke the enemy (nearer to nearPlayer, and already in
        // range of it) would attack nearPlayer without even needing to
        // move; provoked, it instead moves to and attacks guard, who is
        // farther away.
        jf::Unit* attacked = jf::takeEnemyTurn(controller.battle(), *controller.battle().findUnit("enemy"));
        assert(attacked == controller.battle().findUnit("guard"));
        assert(controller.battle().findUnit("nearPlayer")->currentHp ==
               controller.battle().findUnit("nearPlayer")->stats.maxHp);
    }

    {
        // docs/initial_skill_effects.md 槍兵`counterthrust`(反撃準備): the
        // 16th skill, and the first Reactive one (SkillCategory::Reactive) -
        // unlike every skill so far it has no chooseSkill()/
        // selectSkillTarget() step at all; it auto-triggers from
        // EnemyAI.cpp's attackIfPossible() whenever the equipped unit is
        // attacked and survives, provided the attacker ends up within the
        // DEFENDER's own weapon range (not the attacker's - see the range
        // check test below).
        jf::Unit spearman = makeUnit("spearman", jf::Team::Player, {1, 1}, 4, jf::UnitClass::Spearman);
        spearman.skillSlots[0].skillId = "counterthrust";
        jf::Unit meleeEnemy = makeUnit("meleeEnemy", jf::Team::Enemy, {1, 2}); // adjacent, within both weapons' range
        jf::BattleState battle({spearman, meleeEnemy});
        jf::initializeSkillCharges(battle.units()[0]);

        jf::Unit* attacked = jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(attacked == &battle.units()[0]);
        assert(battle.units()[0].isAlive());
        assert(battle.units()[1].currentHp < battle.units()[1].stats.maxHp); // retaliation landed
        assert(!jf::skillSlotAvailable(battle.units()[0], 0)); // OncePerBattle, consumed by the retaliation
    }

    {
        // Range check: the attacker must end up within the DEFENDER's own
        // weapon range, not just the attacker's - an archer hitting from
        // distance 2 is beyond the Spearman's melee-only range, so no
        // retaliation happens even though the archer's own attack landed.
        jf::Unit spearman = makeUnit("spearman", jf::Team::Player, {1, 0}, 4, jf::UnitClass::Spearman);
        spearman.skillSlots[0].skillId = "counterthrust";
        jf::Unit archer = makeUnit("archer", jf::Team::Enemy, {1, 2});
        archer.weapon = {.id = "bow", .name = "Bow", .might = 5,
                        .minRange = 1, .maxRange = 2, .damageType = jf::DamageType::Physical};
        jf::BattleState battle({spearman, archer});
        jf::initializeSkillCharges(battle.units()[0]);

        jf::Unit* attacked = jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(attacked == &battle.units()[0]);
        assert(battle.units()[1].currentHp == battle.units()[1].stats.maxHp); // no retaliation: out of range
        assert(jf::skillSlotAvailable(battle.units()[0], 0)); // charge untouched
    }

    {
        // Once per battle: after the charge is consumed by one retaliation,
        // a second attack (even fully in range) doesn't trigger another one.
        jf::Unit spearman = makeUnit("spearman", jf::Team::Player, {1, 1}, 4, jf::UnitClass::Spearman);
        spearman.skillSlots[0].skillId = "counterthrust";
        jf::Unit firstEnemy = makeUnit("firstEnemy", jf::Team::Enemy, {1, 2});
        jf::Unit secondEnemy = makeUnit("secondEnemy", jf::Team::Enemy, {1, 0});
        jf::BattleState battle({spearman, firstEnemy, secondEnemy});
        jf::initializeSkillCharges(battle.units()[0]);

        jf::takeEnemyTurn(battle, battle.units()[1]); // firstEnemy attacks, consumes the charge
        assert(battle.units()[1].currentHp < battle.units()[1].stats.maxHp);
        assert(!jf::skillSlotAvailable(battle.units()[0], 0));

        jf::takeEnemyTurn(battle, battle.units()[2]); // secondEnemy attacks too, but charge is gone
        assert(battle.units()[2].currentHp == battle.units()[2].stats.maxHp);
    }

    {
        // docs/initial_skill_effects.md 監視弓兵`overwatch`(警戒射撃): the
        // 17th skill - self-only, no target selection (resolves immediately
        // like hold_formation/extended_lockdown), but arms
        // Unit::overwatchActive rather than a BuffKind. Consumed reactively
        // by EnemyAI.cpp's triggerOverwatch(), not by anything here.
        jf::Unit watcher = makeUnit("watcher", jf::Team::Player, {1, 0}, 4, jf::UnitClass::WatchArcher);
        watcher.skillSlots[0].skillId = "overwatch";
        jf::Unit unactedAlly = makeUnit("unactedAlly", jf::Team::Player, {2, 7});
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 5});
        jf::BattleState battle({watcher, unactedAlly, enemy});
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseSkill(0);
        // Resolves immediately: no SelectSkillTarget detour for this skill.
        assert(controller.inputState() == jf::BattleInputState::SelectUnit);
        assert(controller.battle().findUnit("watcher")->overwatchActive);
        assert(!jf::skillSlotAvailable(*controller.battle().findUnit("watcher"), 0)); // OncePerBattle, just used
    }

    {
        // Overwatch ambushes an enemy already within the watcher's own
        // weapon range going into that enemy's turn - before it gets to act
        // at all. A high-Might weapon and 1 HP enemy make the ambush lethal
        // so the test can cleanly show the enemy never got to attack back.
        jf::Unit watcher = makeUnit("watcher", jf::Team::Player, {1, 0}, 4, jf::UnitClass::WatchArcher);
        watcher.weapon = {.id = "bow", .name = "Bow", .might = 20,
                         .minRange = 1, .maxRange = 2, .damageType = jf::DamageType::Physical};
        watcher.overwatchActive = true;
        jf::Unit doomedEnemy = makeUnit("doomedEnemy", jf::Team::Enemy, {1, 2}); // distance 2, already in range
        doomedEnemy.currentHp = 1;
        jf::Unit secondEnemy = makeUnit("secondEnemy", jf::Team::Enemy, {1, 2});
        jf::BattleState battle({watcher, doomedEnemy, secondEnemy});

        jf::Unit* attacked = jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(attacked == nullptr); // ambushed before it could act
        assert(!battle.units()[1].isAlive());
        assert(battle.units()[0].currentHp == battle.units()[0].stats.maxHp); // watcher was never attacked
        assert(!battle.units()[0].overwatchActive); // consumed by firing

        // Once per battle: a second enemy entering the same range afterward
        // is not ambushed - the charge is already spent.
        jf::takeEnemyTurn(battle, battle.units()[2]);
        assert(battle.units()[2].isAlive());
    }

    {
        // Overwatch also catches an enemy that starts out of range and
        // moves into it mid-turn - the ambush fires after movement, before
        // the enemy's own attack. The enemy's MOV is capped so it lands
        // exactly at distance 2 (the far edge of the watcher's range)
        // rather than closing all the way to melee range.
        jf::Unit watcher = makeUnit("watcher", jf::Team::Player, {1, 0}, 4, jf::UnitClass::WatchArcher);
        watcher.weapon = {.id = "bow", .name = "Bow", .might = 20,
                         .minRange = 1, .maxRange = 2, .damageType = jf::DamageType::Physical};
        watcher.overwatchActive = true;
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 5}, 3); // MOV 3: {1,5}->{1,2}, distance 2
        // Above the 25% retreat threshold (docs/enemy_ai_rules.md) so this
        // enemy still approaches normally instead of fleeing toward the
        // exit edge - retreat behavior has its own dedicated tests below.
        enemy.currentHp = 15;
        jf::BattleState battle({watcher, enemy});

        jf::Unit* attacked = jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(attacked == nullptr); // ambushed mid-move, before its own attack
        assert(!battle.units()[1].isAlive());
        assert((battle.units()[1].position == jf::GridPos{1, 2}));
    }

    {
        // Movement.cpp's computeMovementPath() (docs/initial_skill_effects.md
        // 辺境斥候`trailblaze`'s "仮移動で通過した"Tile tracking): reconstructs
        // the exact tile-by-tile shortest path, excluding the origin but
        // including the destination.
        jf::Unit mover = makeUnit("mover", jf::Team::Player, {1, 0}, 3);
        jf::BattleState battle({mover});
        auto path = jf::computeMovementPath(battle, battle.units()[0], {1, 3});
        std::vector<jf::GridPos> expected{{1, 1}, {1, 2}, {1, 3}};
        assert(path == expected);
        assert(jf::computeMovementPath(battle, battle.units()[0], {1, 0}).empty()); // no movement
    }

    {
        // docs/initial_skill_effects.md 辺境斥候`trailblaze`(道拓き): the
        // 18th and final skill - self-only, no target selection (like
        // overwatch), but marks the Ash/Shallows tiles from the move that
        // used it (BattleState::trailblazedTiles_, captured via
        // computeMovementPath() before the move happens) rather than arming
        // a Unit flag. Only Ash/Shallows tiles get marked - the Floor tile
        // in the middle of this path does not.
        jf::Unit scout = makeUnit("scout", jf::Team::Player, {1, 0}, 4, jf::UnitClass::FrontierScout);
        scout.skillSlots[0].skillId = "trailblaze";
        jf::Unit unactedAlly = makeUnit("unactedAlly", jf::Team::Player, {2, 7});
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {0, 7});
        jf::BattleState battle({scout, unactedAlly, enemy});
        battle.setTerrain({1, 1}, jf::TerrainType::Ash);       // scout ignores this cost personally
        battle.setTerrain({1, 2}, jf::TerrainType::Floor);
        battle.setTerrain({1, 3}, jf::TerrainType::Shallows);  // cost 2, not ignored by FrontierScout
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile({1, 3}); // cost 1 (Ash, ignored) + 1 (Floor) + 2 (Shallows) = 4
        controller.chooseSkill(0);
        // Resolves immediately: no SelectSkillTarget detour for this skill.
        assert(controller.inputState() == jf::BattleInputState::SelectUnit);
        assert(controller.battle().isTrailblazed(jf::GridPos{1, 1})); // Ash: marked
        assert(controller.battle().isTrailblazed(jf::GridPos{1, 3})); // Shallows: marked
        assert(!controller.battle().isTrailblazed(jf::GridPos{1, 2})); // Floor: not marked
        assert(!jf::skillSlotAvailable(*controller.battle().findUnit("scout"), 0)); // CD2, just used
    }

    {
        // The trailblazed-cost override actually changes ally pathfinding:
        // an Ash tile normally costs 2, but once trailblazed it costs 1,
        // making a tile 1 further away newly reachable within the same MOV.
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {1, 0}, 2);
        jf::BattleState battle({ally});
        battle.setTerrain({1, 1}, jf::TerrainType::Ash);
        assert(contains(jf::computeReachableTiles(battle, battle.units()[0]), jf::GridPos{1, 1}));
        assert(!contains(jf::computeReachableTiles(battle, battle.units()[0]), jf::GridPos{1, 2}));

        battle.markTrailblazed({1, 1});
        assert(contains(jf::computeReachableTiles(battle, battle.units()[0]), jf::GridPos{1, 2}));
    }

    {
        // Trailblazed tiles only last "このPlayer Phase中だけ" - cleared once
        // every living Player unit has acted and the Player Phase ends.
        jf::Unit scout = makeUnit("scout", jf::Team::Player, {1, 0}, 4, jf::UnitClass::FrontierScout);
        scout.skillSlots[0].skillId = "trailblaze";
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {2, 0});
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {0, 7});
        jf::BattleState battle({scout, ally, enemy});
        battle.setTerrain({1, 1}, jf::TerrainType::Ash);
        jf::BattleController controller(std::move(battle));
        jf::initializeSkillCharges(controller.battle().units()[0]);

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile({1, 1});
        controller.chooseSkill(0);
        assert(controller.battle().isTrailblazed(jf::GridPos{1, 1}));

        controller.selectUnit(*controller.battle().findUnit("ally"));
        controller.selectMoveTile(controller.battle().findUnit("ally")->position);
        controller.chooseWait(); // last living Player unit to act -> Player Phase ends
        assert(!controller.battle().isTrailblazed(jf::GridPos{1, 1}));
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
        // docs/implementation_roadmap.md M7項目3: equipSkillForUnit() is
        // class-generic, not Spearman-only - confirm it works the same way
        // for one of the M7項目1 classes (FrontierEngineer). Every tier
        // (including Tier1) requires the training branch here - only the
        // auto-equip-at-join path (confirmRecruitJoin()) bypasses that gate.
        jf::GameData data = makeFactoryData();
        data.playerParty[0].classId = jf::UnitClass::FrontierEngineer;
        jf::GameApp app(data);

        assert(!app.equipSkillForUnit("player0", 0, "field_repair")); // specialist_training not built
        jf::BaseState& testBase = const_cast<jf::BaseState&>(app.baseState());
        testBase.unlockedNodeIds.insert("specialist_training");
        assert(app.equipSkillForUnit("player0", 0, "field_repair"));
        assert(app.equippedSkills().at("player0").equippedSkillIds[0] == "field_repair");
        assert(app.equipSkillForUnit("player0", 1, "rubble_charge"));
        assert(app.equippedSkills().at("player0").equippedSkillIds[1] == "rubble_charge");
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
        // EscapeUnits (docs/mission_objectives.md "脱出"): same
        // ActionResolved-on-tile credit as SecureTile, but needs
        // requiredEscapeCount DISTINCT units, not just one. The same unit
        // ending multiple actions there only ever counts once
        // (creditedTargetIds is a set).
        jf::ObjectiveDefinition escape;
        escape.id = "escape_point";
        escape.kind = jf::ObjectiveKind::EscapeUnits;
        escape.primary = true;
        escape.groupId = "primary";
        escape.target.tile = {1, 7};
        escape.target.securingTeam = jf::Team::Player;
        escape.target.requiredEscapeCount = 2;
        jf::BattleMissionState mission;
        mission.groups.push_back({"primary", jf::ObjectiveGroupRule::All});
        mission.definitions.push_back(escape);
        mission.progress[escape.id] = jf::ObjectiveProgress{escape.id};

        jf::BattleEvent firstEscapes{
            1, 1, jf::ActionResolvedEvent{1, "scout", jf::Team::Player, jf::ActionKind::Wait, {1, 7}}};
        jf::handleObjectiveEvent(mission, firstEscapes);
        assert(mission.progress.at("escape_point").status == jf::ObjectiveStatus::Active); // only 1 of 2

        // The same unit ending a second action on the tile must not count
        // as a second distinct escapee.
        jf::BattleEvent sameUnitAgain{
            2, 2, jf::ActionResolvedEvent{2, "scout", jf::Team::Player, jf::ActionKind::Wait, {1, 7}}};
        jf::handleObjectiveEvent(mission, sameUnitAgain);
        assert(mission.progress.at("escape_point").status == jf::ObjectiveStatus::Active);
        assert(mission.progress.at("escape_point").creditedTargetIds.size() == 1);

        jf::BattleEvent secondEscapes{
            3, 3, jf::ActionResolvedEvent{3, "guard", jf::Team::Player, jf::ActionKind::Wait, {1, 7}}};
        jf::handleObjectiveEvent(mission, secondEscapes);
        assert(mission.progress.at("escape_point").status == jf::ObjectiveStatus::Completed);

        // Validation: requiredEscapeCount must be >= 1, and the tile must be
        // in-bounds/passable/unoccupied at battle start (same rules as
        // SecureTile).
        jf::Unit player = makeUnit("player", jf::Team::Player, {1, 0});
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {2, 7});
        jf::BattleState battle({player, enemy}, {}, 0, mission);
        auto errors = jf::validateBattleMission(mission, battle);
        assert(errors.empty());

        jf::ObjectiveDefinition badEscape = escape;
        badEscape.id = "bad_escape";
        badEscape.target.requiredEscapeCount = 0;
        jf::BattleMissionState badMission = mission;
        badMission.definitions = {badEscape};
        badMission.progress = {{badEscape.id, jf::ObjectiveProgress{badEscape.id}}};
        auto badErrors = jf::validateBattleMission(badMission, battle);
        assert(!badErrors.empty());

        jf::ObjectiveDefinition occupiedEscape = escape;
        occupiedEscape.id = "occupied_escape";
        occupiedEscape.target.tile = {2, 7}; // enemy's own start tile
        jf::BattleMissionState occupiedMission = mission;
        occupiedMission.definitions = {occupiedEscape};
        occupiedMission.progress = {{occupiedEscape.id, jf::ObjectiveProgress{occupiedEscape.id}}};
        auto occupiedErrors = jf::validateBattleMission(occupiedMission, battle);
        assert(!occupiedErrors.empty());
    }

    {
        // ProtectUnit (docs/mission_objectives.md "対象保護"): always
        // secondary, and structurally a falling edge (starts "satisfied",
        // only ever moves Active -> Failed, never Completed on its own -
        // see Objective.hpp's comment on ObjectiveKind::ProtectUnit).
        jf::Unit escort = makeUnit("escort", jf::Team::Player, {1, 0});
        jf::Unit guard = makeUnit("guard", jf::Team::Player, {2, 0});
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 7}); // far away: no combat happens
        jf::ObjectiveDefinition eliminate;
        eliminate.id = "eliminate_enemies";
        eliminate.kind = jf::ObjectiveKind::EliminateTeam;
        eliminate.primary = true;
        eliminate.groupId = "primary";
        eliminate.target.team = jf::Team::Enemy;
        jf::ObjectiveDefinition protect;
        protect.id = "protect_escort";
        protect.kind = jf::ObjectiveKind::ProtectUnit;
        protect.primary = false;
        protect.groupId = "secondary";
        protect.target.unitId = "escort";
        jf::BattleMissionState mission;
        mission.groups.push_back({"primary", jf::ObjectiveGroupRule::All});
        mission.groups.push_back({"secondary", jf::ObjectiveGroupRule::All});
        mission.definitions = {eliminate, protect};
        mission.progress[eliminate.id] = jf::ObjectiveProgress{eliminate.id};
        mission.progress[protect.id] = jf::ObjectiveProgress{protect.id};

        jf::BattleState battle({escort, guard, enemy}, {}, 0, mission);
        auto errors = jf::validateBattleMission(mission, battle);
        assert(errors.empty());

        // Escort alive: stays Active through repeated syncs, never
        // Completed (unlike every other Kind, "satisfied so far" isn't a
        // final state here).
        jf::syncObjectiveProgress(battle);
        jf::syncObjectiveProgress(battle);
        assert(battle.missionState().progress.at("protect_escort").status == jf::ObjectiveStatus::Active);

        // Escort defeated: Failed, and it locks in even if something were to
        // resurrect currentHp later (never reverts to Active).
        battle.units()[0].currentHp = 0;
        jf::syncObjectiveProgress(battle);
        assert(battle.missionState().progress.at("protect_escort").status == jf::ObjectiveStatus::Failed);
        battle.units()[0].currentHp = 20;
        jf::syncObjectiveProgress(battle);
        assert(battle.missionState().progress.at("protect_escort").status == jf::ObjectiveStatus::Failed);

        // A secondary ProtectUnit failing must not affect the primary
        // group's own win condition (docs: "副目標失敗だけでは戦闘を敗北にしない").
        for (jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Enemy) unit.currentHp = 0;
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);

        // Validation: unknown target unit, and a ProtectUnit mistakenly
        // marked primary (which would silently participate in the wrong
        // - rising-edge - evaluation loop), are both rejected.
        jf::ObjectiveDefinition ghostProtect = protect;
        ghostProtect.id = "protect_ghost";
        ghostProtect.target.unitId = "no_such_unit";
        jf::BattleMissionState ghostMission = mission;
        ghostMission.definitions = {eliminate, ghostProtect};
        ghostMission.progress = {{eliminate.id, jf::ObjectiveProgress{eliminate.id}},
                                 {ghostProtect.id, jf::ObjectiveProgress{ghostProtect.id}}};
        auto ghostErrors = jf::validateBattleMission(ghostMission, battle);
        assert(!ghostErrors.empty());

        jf::ObjectiveDefinition primaryProtect = protect;
        primaryProtect.id = "protect_primary";
        primaryProtect.primary = true;
        jf::BattleMissionState primaryMission = mission;
        primaryMission.definitions = {eliminate, primaryProtect};
        primaryMission.progress = {{eliminate.id, jf::ObjectiveProgress{eliminate.id}},
                                   {primaryProtect.id, jf::ObjectiveProgress{primaryProtect.id}}};
        auto primaryErrors = jf::validateBattleMission(primaryMission, battle);
        assert(!primaryErrors.empty());
    }

    {
        // OperateObject (docs/mission_objectives.md "装置操作"): Live-
        // evaluated off interactionCount, same "unknown target never
        // trivially wins" rule as DestroyObject. The unit/class restriction
        // is enforced upstream by resolveObjectInteraction() itself, so this
        // Objective doesn't re-check who performed it.
        jf::Unit engineer = makeUnit("engineer", jf::Team::Player, {1, 1}, 4, jf::UnitClass::Spearman);
        jf::Unit farAway = makeUnit("far", jf::Team::Player, {1, 6}, 4, jf::UnitClass::Spearman);
        jf::BattleObjectDefinition leverDef;
        leverDef.definitionId = "lever";
        leverDef.kind = jf::BattleObjectKind::Device;
        leverDef.interaction = jf::ObjectInteractionDefinition{};
        leverDef.interaction->interactionId = "pull_lever";
        leverDef.interaction->range = 1;
        leverDef.interaction->allowedClasses = {jf::UnitClass::Spearman};
        leverDef.interaction->requiredState = jf::BattleObjectStateKind::Active;
        leverDef.interaction->maxUses = 1;
        leverDef.interactionResultState = jf::BattleObjectStateKind::Opened;

        jf::ObjectiveDefinition operate;
        operate.id = "operate_lever";
        operate.kind = jf::ObjectiveKind::OperateObject;
        operate.primary = true;
        operate.groupId = "primary";
        operate.target.objectId = "lever1";
        jf::BattleMissionState mission;
        mission.groups.push_back({"primary", jf::ObjectiveGroupRule::All});
        mission.definitions.push_back(operate);
        mission.progress[operate.id] = jf::ObjectiveProgress{operate.id};

        jf::BattleState battle({engineer, farAway}, {}, 0, mission);
        assert(battle.registerObjectDefinition(leverDef));
        assert(battle.placeObject({"lever1", "lever", {1, 0}}));
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Ongoing);

        jf::BattleObjectState* lever = battle.findObject("lever1");
        assert(lever != nullptr);
        assert(jf::resolveObjectInteraction(engineer, *lever, *leverDef.interaction, leverDef.interactionResultState));
        jf::syncObjectiveProgress(battle);
        assert(battle.missionState().progress.at("operate_lever").status == jf::ObjectiveStatus::Completed);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);

        // Validation: unknown object id, and a real object with no
        // Interact defined (e.g. a plain Barrier), are both rejected.
        jf::ObjectiveDefinition ghostOperate = operate;
        ghostOperate.id = "operate_ghost";
        ghostOperate.target.objectId = "no_such_object";
        jf::BattleMissionState ghostMission = mission;
        ghostMission.definitions = {ghostOperate};
        ghostMission.progress = {{ghostOperate.id, jf::ObjectiveProgress{ghostOperate.id}}};
        auto ghostErrors = jf::validateBattleMission(ghostMission, battle);
        assert(!ghostErrors.empty());

        jf::BattleObjectDefinition barrierDef;
        barrierDef.definitionId = "plain_barrier";
        barrierDef.kind = jf::BattleObjectKind::Barrier;
        barrierDef.blocksMovement = true;
        assert(battle.registerObjectDefinition(barrierDef));
        assert(battle.placeObject({"barrier1", "plain_barrier", {2, 4}}));
        jf::ObjectiveDefinition barrierOperate = operate;
        barrierOperate.id = "operate_barrier";
        barrierOperate.target.objectId = "barrier1";
        jf::BattleMissionState barrierMission = mission;
        barrierMission.definitions = {barrierOperate};
        barrierMission.progress = {{barrierOperate.id, jf::ObjectiveProgress{barrierOperate.id}}};
        auto barrierErrors = jf::validateBattleMission(barrierMission, battle);
        assert(!barrierErrors.empty());
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
        // DestroyObject (docs/battle_objects.md): live-evaluated against the
        // target Object's BattleObjectState, same "unknown target never
        // trivially wins" rule as DefeatUnit.
        jf::Unit player = makeUnit("player", jf::Team::Player, {1, 0});
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 7});
        jf::BattleObjectDefinition crateDef;
        crateDef.definitionId = "supply_crate";
        crateDef.kind = jf::BattleObjectKind::Container;
        crateDef.canBeAttacked = true;
        crateDef.maxDurability = 10;

        jf::ObjectiveDefinition destroyCrate;
        destroyCrate.id = "destroy_crate";
        destroyCrate.kind = jf::ObjectiveKind::DestroyObject;
        destroyCrate.primary = true;
        destroyCrate.groupId = "primary";
        destroyCrate.target.objectId = "crate1";
        jf::BattleMissionState mission;
        mission.groups.push_back({"primary", jf::ObjectiveGroupRule::All});
        mission.definitions.push_back(destroyCrate);
        mission.progress[destroyCrate.id] = jf::ObjectiveProgress{destroyCrate.id};

        jf::BattleState battle({player, enemy}, {}, 0, mission);
        assert(battle.registerObjectDefinition(crateDef));
        assert(battle.placeObject({"crate1", "supply_crate", {1, 4}}));
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Ongoing);

        jf::BattleObjectState* crate = battle.findObject("crate1");
        assert(crate != nullptr);
        crate->durability = 0;
        crate->state = jf::BattleObjectStateKind::Destroyed;
        jf::syncObjectiveProgress(battle);
        assert(battle.missionState().progress.at("destroy_crate").status == jf::ObjectiveStatus::Completed);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);

        // Validation: an unknown object id, and a reference to a real but
        // non-attackable object, are both rejected rather than accepted as
        // an unwinnable-but-well-formed mission.
        jf::ObjectiveDefinition destroyGhost = destroyCrate;
        destroyGhost.id = "destroy_ghost";
        destroyGhost.target.objectId = "no_such_object";
        jf::BattleMissionState ghostMission = mission;
        ghostMission.definitions = {destroyGhost};
        ghostMission.progress = {{destroyGhost.id, jf::ObjectiveProgress{destroyGhost.id}}};
        auto ghostErrors = jf::validateBattleMission(ghostMission, battle);
        assert(!ghostErrors.empty());

        jf::BattleObjectDefinition markerDef;
        markerDef.definitionId = "waypoint";
        markerDef.kind = jf::BattleObjectKind::Marker;
        markerDef.canOccupy = true;
        assert(battle.registerObjectDefinition(markerDef));
        assert(battle.placeObject({"marker1", "waypoint", {2, 4}}));
        jf::ObjectiveDefinition destroyMarker = destroyCrate;
        destroyMarker.id = "destroy_marker";
        destroyMarker.target.objectId = "marker1";
        jf::BattleMissionState markerMission = mission;
        markerMission.definitions = {destroyMarker};
        markerMission.progress = {{destroyMarker.id, jf::ObjectiveProgress{destroyMarker.id}}};
        auto markerErrors = jf::validateBattleMission(markerMission, battle);
        assert(!markerErrors.empty());
    }

    {
        // SurviveRounds (docs/mission_objectives.md "防衛"): "指定ラウンド
        //終了まで敗北条件を回避" - satisfied once battle.round() has moved
        // past the target, not merely reached it (round 2 must actually END
        // before a target of 2 is satisfied).
        jf::Unit player = makeUnit("player", jf::Team::Player, {1, 0});
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 7}); // far enough away: no combat happens
        jf::ObjectiveDefinition survive;
        survive.id = "survive_2";
        survive.kind = jf::ObjectiveKind::SurviveRounds;
        survive.primary = true;
        survive.groupId = "primary";
        survive.target.surviveUntilRound = 2;
        jf::BattleMissionState mission;
        mission.groups.push_back({"primary", jf::ObjectiveGroupRule::All});
        mission.definitions.push_back(survive);
        mission.progress[survive.id] = jf::ObjectiveProgress{survive.id};

        jf::BattleController controller(jf::BattleState({player, enemy}, {}, 0, mission));
        assert(controller.battle().round() == 1);
        jf::syncObjectiveProgress(controller.battle());
        assert(jf::evaluateBattleOutcome(controller.battle()).kind == jf::BattleOutcomeKind::Ongoing);

        controller.endPlayerTurn();
        for (int i = 0; i < 10 && controller.inputState() == jf::BattleInputState::EnemyTurn; ++i)
            controller.update(1.0f);
        assert(controller.inputState() == jf::BattleInputState::SelectUnit);
        assert(controller.battle().round() == 2);
        jf::syncObjectiveProgress(controller.battle());
        assert(jf::evaluateBattleOutcome(controller.battle()).kind == jf::BattleOutcomeKind::Ongoing);

        controller.endPlayerTurn();
        for (int i = 0; i < 10 && controller.inputState() == jf::BattleInputState::EnemyTurn; ++i)
            controller.update(1.0f);
        assert(controller.inputState() == jf::BattleInputState::SelectUnit);
        assert(controller.battle().round() == 3);
        jf::syncObjectiveProgress(controller.battle());
        assert(jf::evaluateBattleOutcome(controller.battle()).kind == jf::BattleOutcomeKind::Victory);

        // Validation: surviveUntilRound must be >= 1 (0 would be trivially
        // satisfied the instant the battle starts, round_'s initial value).
        jf::ObjectiveDefinition badSurvive = survive;
        badSurvive.target.surviveUntilRound = 0;
        jf::BattleMissionState badMission = mission;
        badMission.definitions = {badSurvive};
        badMission.progress = {{badSurvive.id, jf::ObjectiveProgress{badSurvive.id}}};
        auto surviveErrors = jf::validateBattleMission(badMission, controller.battle());
        assert(!surviveErrors.empty());
    }

    {
        // HoldTile (docs/mission_objectives.md「地点維持」/
        // docs/regions/cinderwatch_gate.md「2. 灰道の監視所」): a
        // securingTeam unit must occupy the target tile for
        // requiredHoldRounds CONSECUTIVE rounds - resolveHoldTileRoundEnd()
        // is called once per round (BattleController's Enemy Phase end,
        // right where RoundEndedEvent fires).
        jf::Unit player = makeUnit("player", jf::Team::Player, {1, 4});
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 7}); // far enough away: no combat happens
        jf::ObjectiveDefinition hold;
        hold.id = "hold_watchpost";
        hold.kind = jf::ObjectiveKind::HoldTile;
        hold.primary = true;
        hold.groupId = "primary";
        hold.target.tile = {1, 4};
        hold.target.securingTeam = jf::Team::Player;
        hold.target.requiredHoldRounds = 2;
        jf::BattleMissionState mission;
        mission.groups.push_back({"primary", jf::ObjectiveGroupRule::All});
        mission.definitions.push_back(hold);
        mission.progress[hold.id] = jf::ObjectiveProgress{hold.id};

        jf::BattleController controller(jf::BattleState({player, enemy}, {}, 0, mission));
        assert(controller.battle().round() == 1);

        // Round 1 ends with the player still on the tile: 1 of 2 consecutive
        // rounds held, not yet Completed.
        controller.endPlayerTurn();
        for (int i = 0; i < 10 && controller.inputState() == jf::BattleInputState::EnemyTurn; ++i)
            controller.update(1.0f);
        assert(controller.inputState() == jf::BattleInputState::SelectUnit);
        assert(controller.battle().round() == 2);
        assert(controller.battle().missionState().progress.at(hold.id).consecutiveRoundsHeld == 1);
        jf::syncObjectiveProgress(controller.battle());
        assert(jf::evaluateBattleOutcome(controller.battle()).kind == jf::BattleOutcomeKind::Ongoing);

        // Round 2 ends with the player still on the tile: 2 of 2 - Completed.
        controller.endPlayerTurn();
        for (int i = 0; i < 10 && controller.inputState() == jf::BattleInputState::EnemyTurn; ++i)
            controller.update(1.0f);
        assert(controller.inputState() == jf::BattleInputState::SelectUnit);
        assert(controller.battle().round() == 3);
        assert(controller.battle().missionState().progress.at(hold.id).consecutiveRoundsHeld == 2);
        jf::syncObjectiveProgress(controller.battle());
        assert(jf::evaluateBattleOutcome(controller.battle()).kind == jf::BattleOutcomeKind::Victory);

        // Leaving the tile mid-way resets the consecutive counter rather
        // than merely pausing it - a fresh mission that moves off-tile
        // before round 1 ends must show 0, not 1.
        jf::Unit movedAway = makeUnit("player", jf::Team::Player, {1, 0});
        jf::BattleMissionState awayMission;
        awayMission.groups.push_back({"primary", jf::ObjectiveGroupRule::All});
        awayMission.definitions.push_back(hold);
        awayMission.progress[hold.id] = jf::ObjectiveProgress{hold.id};
        jf::BattleController awayController(jf::BattleState({movedAway, enemy}, {}, 0, awayMission));
        awayController.endPlayerTurn();
        for (int i = 0; i < 10 && awayController.inputState() == jf::BattleInputState::EnemyTurn; ++i)
            awayController.update(1.0f);
        assert(awayController.battle().missionState().progress.at(hold.id).consecutiveRoundsHeld == 0);
        assert(awayController.battle().missionState().progress.at(hold.id).status == jf::ObjectiveStatus::Active);

        // Validation: requiredHoldRounds must be >= 1, and the tile must be
        // in-bounds, passable, and unoccupied at battle start (same shape as
        // SecureTile's checks).
        jf::ObjectiveDefinition badHold = hold;
        badHold.target.requiredHoldRounds = 0;
        jf::BattleMissionState badMission = mission;
        badMission.definitions = {badHold};
        badMission.progress = {{badHold.id, jf::ObjectiveProgress{badHold.id}}};
        auto holdErrors = jf::validateBattleMission(badMission, controller.battle());
        assert(!holdErrors.empty());

        jf::ObjectiveDefinition occupiedHold = hold;
        occupiedHold.target.tile = {1, 4}; // player still stands here in `controller`'s battle
        jf::BattleMissionState occupiedMission = mission;
        occupiedMission.definitions = {occupiedHold};
        occupiedMission.progress = {{occupiedHold.id, jf::ObjectiveProgress{occupiedHold.id}}};
        auto occupiedErrors = jf::validateBattleMission(occupiedMission, controller.battle());
        assert(!occupiedErrors.empty());
    }

    {
        // StageDescriptor::primaryHoldTileAlternative (docs/regions/
        // cinderwatch_gate.md「2. 灰道の監視所」): BattleFactory widens the
        // default single-member "primary" group to Any and adds a HoldTile
        // objective targeting a real generated WatchPost tile, alongside the
        // untouched default EliminateTeam member - both must remain valid
        // win paths.
        jf::GameData data = makeFactoryData();
        jf::StageDescriptor stage = testStage0(jf::kSignalTowerTerrain); // highest WatchPost weight
        stage.enemyCountOverride.reset();                                // irrelevant here, keep default roster
        stage.primaryHoldTileAlternative =
            jf::StageDescriptor::HoldTileMissionRule{"watchpost_hold", 2, 0, jf::kGridCols - 1};

        jf::BattleState battle = jf::createScenarioBattle(data, stage, /*seed=*/7);

        const jf::ObjectiveGroupDefinition* primaryGroup = nullptr;
        for (const auto& group : battle.missionState().groups)
            if (group.id == "primary") primaryGroup = &group;
        assert(primaryGroup && primaryGroup->rule == jf::ObjectiveGroupRule::Any);

        const jf::ObjectiveDefinition* holdDef = nullptr;
        for (const auto& def : battle.missionState().definitions)
            if (def.kind == jf::ObjectiveKind::HoldTile) holdDef = &def;
        assert(holdDef && holdDef->id == "watchpost_hold" && holdDef->primary);
        assert(holdDef->target.requiredHoldRounds == 2);
        assert(holdDef->target.securingTeam == jf::Team::Player);
        assert(jf::isInBounds(holdDef->target.tile));
        assert(battle.terrainAt(holdDef->target.tile) == jf::TerrainType::WatchPost);
        assert(!battle.unitAt(holdDef->target.tile)); // unoccupied at battle start

        // EliminateTeam remains an independent win path: defeating every
        // enemy wins even though nobody ever touched the HoldTile.
        jf::AliveSnapshot before = jf::captureAliveSnapshot(battle);
        for (jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Enemy) unit.currentHp = 0;
        jf::emitUnitDefeatedEvents(battle, before);
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);
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
        // The UI's danger-zone highlight reads BossTelegraph::lockedTiles,
        // populated at telegraph time - must match the tiles the charge
        // actually walks below (range 3, direction -1 from col 5).
        assert((battle.units()[1].bossRuntime.telegraph.lockedTiles ==
               std::vector<jf::GridPos>{{1, 4}, {1, 3}, {1, 2}}));

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
        assert((battle.units()[1].bossRuntime.telegraph.lockedTiles ==
               std::vector<jf::GridPos>{{1, 3}, {1, 4}, {1, 5}}));

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
        // The log at col 4 is the first obstacle in range - lockedTiles stops
        // there (the log's own tile IS included, it gets hit/destroyed),
        // never reaching the ally at col 2.
        assert((battle.units()[1].bossRuntime.telegraph.lockedTiles == std::vector<jf::GridPos>{{1, 4}}));
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
        // 灰角大猪の激昂(docs/regions/ashbough_forest.md「HPが半分以下になると
        // 激昂し」): bossEnraged flips exactly once, the instant HP drops to
        // (or below) 50% - a non-turn-consuming state update that happens
        // before that same turn's charge telegraph (docs/boss_common_rules.md
        // "Phase移行").
        jf::Unit boar = makeUnit("boar", jf::Team::Enemy, {1, 5}, 2, jf::UnitClass::AshenhornBoar);
        boar.stats.strength = 9;
        boar.stats.maxHp = 56;
        boar.currentHp = 28; // exactly 50%
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {1, 2}); // same row, distance 3
        jf::BattleState battle({ally, boar});

        assert(!battle.units()[1].bossEnraged);
        jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(battle.units()[1].bossEnraged);
        assert(battle.units()[1].bossRuntime.stageIndex == 1);
        // Same turn's telegraph already reflects the enraged range (4, not 3).
        assert(battle.units()[1].chargeTelegraphed);
        assert((battle.units()[1].bossRuntime.telegraph.lockedTiles ==
               std::vector<jf::GridPos>{{1, 4}, {1, 3}, {1, 2}, {1, 1}}));
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
        const std::size_t eventsBefore = battle.missionState().consumedEventIds.size();
        jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(battle.units()[1].bossEnraged);
        assert(battle.units()[1].stats.strength > 9);
        assert(battle.units()[1].chargeTelegraphed);
        assert(battle.units()[1].position.row == 0);
        // docs/boss_common_rules.md "Phase移行": enrage fires exactly one
        // BossStageChangedEvent (in addition to whatever ActionResolvedEvent
        // this same turn's telegraph/move already consumes).
        assert(battle.missionState().consumedEventIds.size() > eventsBefore);
    }

    {
        // docs/boss_common_rules.md "Bossの退場理由": a defeated
        // AshenhornBoar gets UnitExitReason::ScriptedWithdrawal ("撃破相当"),
        // every other unit gets the plain Defeated case - both set exactly
        // once, the moment HP first reaches 0, by emitUnitDefeatedEvents().
        jf::Unit attacker = makeUnit("attacker", jf::Team::Player, {1, 0});
        jf::Unit boar = makeUnit("boar", jf::Team::Enemy, {1, 1}, 2, jf::UnitClass::AshenhornBoar);
        boar.currentHp = 1;
        jf::Unit plainEnemy = makeUnit("plainEnemy", jf::Team::Enemy, {2, 0});
        plainEnemy.currentHp = 1;
        jf::BattleState battle({attacker, boar, plainEnemy});
        assert(battle.units()[1].exitReason == jf::UnitExitReason::Defeated); // default, still alive
        const jf::AliveSnapshot before = jf::captureAliveSnapshot(battle);
        battle.units()[1].currentHp = 0;
        battle.units()[2].currentHp = 0;
        jf::emitUnitDefeatedEvents(battle, before);
        assert(battle.units()[1].exitReason == jf::UnitExitReason::ScriptedWithdrawal);
        assert(battle.units()[2].exitReason == jf::UnitExitReason::Defeated);
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
        // M9-Y: fixed a pre-existing gap where computeRegionSummaries()'s
        // region list stopped at WindscarPlateau (OldFrontierSettlement/
        // EmberRavine were both missing) - now covers all 7 regions. M9-AG
        // added an 8th (BuriedDawnSanctum, EmberRavine's region-clear stub);
        // this Slice added a 9th (ShatteredMarchFort, BuriedDawnSanctum's own
        // region-clear stub). A later Slice added a 10th (MappedEdge,
        // ShatteredMarchFort's own region-clear stub - the 10th and FINAL
        // region of the whole campaign).
        assert(summaries.size() == 10);
        bool sawAshboughUnlocked = false, sawCinderwatchLocked = false, sawAshironLocked = false;
        for (const auto& summary : summaries) {
            if (summary.id == jf::RegionId::AshboughForest) sawAshboughUnlocked = summary.unlocked;
            if (summary.id == jf::RegionId::CinderwatchGate) sawCinderwatchLocked = !summary.unlocked;
            if (summary.id == jf::RegionId::AshironQuarry) sawAshironLocked = !summary.unlocked;
        }
        assert(sawAshboughUnlocked && sawCinderwatchLocked && sawAshironLocked);
        assert(!app.isRegionUnlocked(jf::RegionId::AshironQuarry));

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
        // docs/implementation_roadmap.md M1-E slice1/2: regions.json is the
        // first Loader/Validation pass for StageDescriptor content (Ashbough
        // Verge, per StageContentData's own comment on which fields are
        // covered so far). Confirms the real Loader actually reaches the
        // fields Region.cpp's stageDescriptorFromContent() reads.
        auto loaded = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(loaded);
        assert(loaded->stageContentById.contains("ashbough_verge"));
        const jf::StageContentData& verge = loaded->stageContentById.at("ashbough_verge");
        assert(verge.terrainProfileId == jf::kAshboughVergeTerrain);
        assert(verge.enemyRoster.size() == 4);
        for (const jf::UnitTemplate& t : verge.enemyRoster) assert(t.classId == jf::UnitClass::Wolf);
        // docs/implementation_roadmap.md M1-E「M9前ブロッカー」項目2: reward fields
        // are now unified into victoryRewardRules (RewardRule list) - filter by
        // Condition instead of reading the old parallel fields directly.
        auto ruleLoot = [](const std::vector<jf::RewardRule>& rules, jf::RewardRule::Condition condition) {
            std::vector<const jf::RewardRule*> matches;
            for (const jf::RewardRule& rule : rules)
                if (rule.condition == condition) matches.push_back(&rule);
            return matches;
        };
        auto alwaysRules = ruleLoot(verge.victoryRewardRules, jf::RewardRule::Condition::Always);
        assert(alwaysRules.size() == 1 && alwaysRules[0]->loot.size() == 2);
        auto routeRules = ruleLoot(verge.victoryRewardRules, jf::RewardRule::Condition::RouteChoice);
        assert(routeRules.size() == 2);
        assert(verge.surveyObjectiveId == "ashbough_verge_surveyed");
        auto surveyRules = ruleLoot(verge.victoryRewardRules, jf::RewardRule::Condition::SurveySuccess);
        assert(surveyRules.size() == 1 && surveyRules[0]->loot.size() == 1 &&
              surveyRules[0]->loot[0].id == "wood" && surveyRules[0]->loot[0].quantity == 1);
        assert(verge.missionNameEn == "Ashbough Verge" && verge.missionNameJa == "灰枝の林縁");

        // The real regionDescriptor() output built from this Loader must
        // match the hand-authored values every other test in this file
        // exercises through GameApp/BattleFactory - this is the "無挙動
        // 移行" fixture check for the one stage that's already migrated.
        const jf::RegionDescriptor region = jf::regionDescriptor(jf::RegionId::AshboughForest, *loaded);
        const jf::StageDescriptor& vergeStage = region.stages[0];
        assert(vergeStage.id == "ashbough_verge");
        auto vergeAlwaysRules = ruleLoot(vergeStage.victoryRewardRules, jf::RewardRule::Condition::Always);
        assert(vergeAlwaysRules.size() == 1 && vergeAlwaysRules[0]->loot.size() == 2);
        bool sawWoodMinus2 = false, sawHidePlus1 = false;
        for (const jf::RewardRule& rule : vergeStage.victoryRewardRules) {
            if (rule.condition != jf::RewardRule::Condition::RouteChoice) continue;
            if (rule.routeChoice == jf::ExplorationChoice::CollapsedSidePath) {
                assert(rule.loot.size() == 1 && rule.loot[0].id == "wood" && rule.loot[0].quantity == -2);
                sawWoodMinus2 = true;
            } else if (rule.routeChoice == jf::ExplorationChoice::ScoutRoute) {
                assert(rule.loot.size() == 1 && rule.loot[0].id == "hide" && rule.loot[0].quantity == 1);
                sawHidePlus1 = true;
            }
        }
        assert(sawWoodMinus2 && sawHidePlus1);

        // docs/implementation_roadmap.md M1-E slice1続き: Herbwater Hollow is
        // the second stage migrated to regions.json, proving the Schema
        // extension (routeOutcomes/scoutRouteRequiredClass/
        // timedReinforcement/herbPatchGeneration) on a richer stage than
        // Ashbough Verge's.
        assert(loaded->stageContentById.contains("herbwater_hollow"));
        const jf::StageContentData& herbContent = loaded->stageContentById.at("herbwater_hollow");
        assert(herbContent.scoutRouteRequiredClass == jf::UnitClass::DawnChirurgeon);
        assert(herbContent.herbPatchGeneration && herbContent.herbPatchGeneration->count == 2 &&
              herbContent.herbPatchGeneration->zoneMinCol == 2 && herbContent.herbPatchGeneration->zoneMaxCol == 5);
        assert(herbContent.timedReinforcement && herbContent.timedReinforcement->id == "herbwater_harvest_wolf" &&
              herbContent.timedReinforcement->spawnRound == 2 &&
              herbContent.timedReinforcement->orderedSpawnCandidates.size() == 3);
        assert(herbContent.routeOutcomes.size() == 3);

        const jf::StageDescriptor& herbStage = region.stages[1];
        assert(herbStage.id == "herbwater_hollow");
        assert(herbStage.scoutRouteRequiredClass == jf::UnitClass::DawnChirurgeon);
        assert(herbStage.herbPatchGeneration && herbStage.herbPatchGeneration->count == 2);
        assert(herbStage.timedReinforcement && herbStage.timedReinforcement->units.size() == 1 &&
              herbStage.timedReinforcement->units[0].classId == jf::UnitClass::Wolf);
        bool sawReinforcementRoute = false;
        for (const auto& [choice, outcome] : herbStage.routeOutcomes) {
            if (choice == jf::ExplorationChoice::CollapsedSidePath) {
                assert(outcome.enableReinforcementWave);
                sawReinforcementRoute = true;
            }
        }
        assert(sawReinforcementRoute);

        // docs/implementation_roadmap.md M1-E slice1続き: Brokenwood
        // Territory - the richest stage migrated (roster, route loot/
        // outcomes, disabled scout route, objectPlacementRules,
        // understaffedReinforcement, both Ad-hoc bonus loot fields) - and
        // the Cinderwatch trio (simpler, but the first stages whose
        // enemyRoster is deliberately absent from JSON, meaning "use
        // GameData::enemyRoster").
        assert(loaded->stageContentById.contains("brokenwood_territory"));
        const jf::StageContentData& brokenContent = loaded->stageContentById.at("brokenwood_territory");
        assert(brokenContent.objectPlacementRules.size() == 1);
        assert(brokenContent.objectPlacementRules[0].definition.definitionId == "fallen_log");
        assert(brokenContent.objectPlacementRules[0].definition.canBeAttacked);
        assert(brokenContent.objectPlacementRules[0].scalesWithExtraBarrierOutcome);
        assert(brokenContent.understaffedReinforcement &&
              brokenContent.understaffedReinforcement->id == "brokenwood_guard_wolf2");
        assert(brokenContent.scoutRouteDisabled);
        assert(brokenContent.logCollisionBonusLoot.size() == 1 && brokenContent.noCasualtiesBonusLoot.size() == 1);

        const jf::StageDescriptor& brokenStage = region.stages[2];
        assert(brokenStage.id == "brokenwood_territory");
        assert(brokenStage.objectPlacementRules.size() == 1);
        assert(brokenStage.objectPlacementRules[0].definition.maxDurability == 16);
        assert(brokenStage.understaffedReinforcement && brokenStage.understaffedThreshold == 4);
        assert(brokenStage.scoutRouteDisabled);

        jf::RegionDescriptor cinderwatch = jf::regionDescriptor(jf::RegionId::CinderwatchGate, *loaded);
        // docs/implementation_roadmap.md M6-A: site 1 (cinderwatch_outer_gate)
        // and site 2 (ashroad_watch) replaced the old placeholder stage 0,
        // pushing ironwatch_stores/signal_tower to indices 2/3. M6-C item2
        // then split signal_tower (site 5, index 4, now real) from
        // last_signal (site 6, index 5, still the boss placeholder).
        assert(cinderwatch.stages.size() == 6);
        assert(cinderwatch.stages[0].id == "cinderwatch_outer_gate" && cinderwatch.stages[0].enemyRoster.size() == 4 &&
              !cinderwatch.stages[0].enemyCountOverride && cinderwatch.stages[0].scoutRouteDisabled);
        assert(cinderwatch.stages[1].id == "ashroad_watch" && cinderwatch.stages[1].primaryHoldTileAlternative &&
              cinderwatch.stages[1].primaryHoldTileAlternative->requiredHoldRounds == 2);
        // docs/implementation_roadmap.md M6-C item1: ironwatch_stores (site
        // 3A) is real content now - own roster (斧兵・槍兵2・弓兵, archer last
        // so CollapsedSidePath's enemiesRemoved:1 drops it), disabled 3rd
        // exploration choice (辺境工兵 isn't a real UnitClass yet), and a
        // 2-tile "物資箱確保" survey objective via the new surveyTileCount.
        assert(cinderwatch.stages[2].id == "ironwatch_stores" && cinderwatch.stages[2].enemyRoster.size() == 4 &&
              cinderwatch.stages[2].scoutRouteDisabled && cinderwatch.stages[2].surveyTileCount &&
              *cinderwatch.stages[2].surveyTileCount == 2 && cinderwatch.stages[2].routeDiscoveries.size() == 2);
        assert(cinderwatch.stages[3].id == "old_barracks" && cinderwatch.stages[3].enemyRoster.size() == 4);
        // docs/implementation_roadmap.md M6-C item2: signal_tower (site 5)
        // is real content now - its own roster (古参守備兵・監視弓兵2・槍兵2),
        // 2 operable Device control panels (secondary_signal_panel_1/
        // primary_signal_panel_1, each its own objectPlacementRules entry
        // with operateObjectiveId set) replacing the default EliminateTeam
        // primary, and a single-tile "軍旗保管箱確保" survey objective.
        assert(cinderwatch.stages[4].id == "signal_tower" && cinderwatch.stages[4].enemyRoster.size() == 5 &&
              cinderwatch.stages[4].objectPlacementRules.size() == 2 &&
              cinderwatch.stages[4].objectPlacementRules[0].operateObjectiveId &&
              cinderwatch.stages[4].objectPlacementRules[1].operateObjectiveId &&
              cinderwatch.stages[4].surveyTileCount && *cinderwatch.stages[4].surveyTileCount == 1);
        // docs/implementation_roadmap.md M6-C item3: last_signal (site 6) is
        // real content now - own roster (元守備隊長・古参守備兵・監視弓兵2・
        // 槍兵), a single DefeatUnit primary targeting the boss (replacing
        // the default EliminateTeam), and the real `[行軍隊長]` class gate
        // (MarchCaptain is a real UnitClass, unlike ironwatch_stores/
        // signal_tower's disabled 3rd choices).
        assert(cinderwatch.stages[5].id == "last_signal" && cinderwatch.stages[5].boostedFirstEnemy &&
              cinderwatch.stages[5].boostedFirstEnemy->displayName == "Former Captain" &&
              cinderwatch.stages[5].boostedFirstEnemy->maxHpBonus == 10 &&
              cinderwatch.stages[5].boostedFirstEnemy->strengthBonus == 2 &&
              cinderwatch.stages[5].primaryDefeatUnitId &&
              *cinderwatch.stages[5].primaryDefeatUnitId == "last_signal_boss1" &&
              cinderwatch.stages[5].scoutRouteRequiredClass == jf::UnitClass::MarchCaptain &&
              !cinderwatch.stages[5].scoutRouteDisabled);

        // docs/route_graph_data.md「分岐と合流」: J1{ironwatch_stores,
        // old_barracks} is a single BranchGroup node (AllMembers) between
        // Camp I and Camp II.
        const jf::RegionRouteGraph& cinderwatchRoute = jf::regionRouteGraph(jf::RegionId::CinderwatchGate);
        const jf::RouteNodeDefinition* branch = jf::findRouteNode(cinderwatchRoute, "cinderwatch_stores_barracks");
        assert(branch && branch->kind == jf::RouteNodeKind::BranchGroup &&
              branch->branchCompletion == jf::BranchCompletion::AllMembers && branch->branchMembers.size() == 2);
        assert(jf::validateRouteGraph(cinderwatchRoute, nullptr));
        const jf::RegionRouteGraph& ashironRoute = jf::regionRouteGraph(jf::RegionId::AshironQuarry);
        assert(jf::usesRouteGraph(jf::RegionId::AshironQuarry));
        assert(jf::validateRouteGraph(ashironRoute, nullptr));
        assert(jf::findRouteNode(ashironRoute, "quarry_entrance"));
        assert(jf::findRouteNode(ashironRoute, "quarry_collapse_core"));
        const jf::RouteNodeDefinition* ashironBranch = jf::findRouteNode(ashironRoute, "quarry_mine_hoist_branch");
        assert(ashironBranch && ashironBranch->kind == jf::RouteNodeKind::BranchGroup &&
              ashironBranch->branchCompletion == jf::BranchCompletion::AnyMember &&
              ashironBranch->branchMembers.size() == 2);

        // docs/regions/blackwater_lowlands.md「地点構成」: 7-site skeleton +
        // 3 camps, herb_islet/resin_grove branch requires BOTH members
        // (AllMembers, same shape as Cinderwatch's ironwatch_stores/
        // old_barracks branch).
        const jf::RegionRouteGraph& blackwaterRoute = jf::regionRouteGraph(jf::RegionId::BlackwaterLowlands);
        assert(jf::usesRouteGraph(jf::RegionId::BlackwaterLowlands));
        assert(jf::validateRouteGraph(blackwaterRoute, nullptr));
        assert(jf::findRouteNode(blackwaterRoute, "sunken_path"));
        assert(jf::findRouteNode(blackwaterRoute, "deep_mire"));
        const jf::RouteNodeDefinition* blackwaterBranch = jf::findRouteNode(blackwaterRoute, "herb_resin_branch");
        assert(blackwaterBranch && blackwaterBranch->kind == jf::RouteNodeKind::BranchGroup &&
              blackwaterBranch->branchCompletion == jf::BranchCompletion::AllMembers &&
              blackwaterBranch->branchMembers.size() == 2);

        jf::RegionRouteGraph disconnectedExit = cinderwatchRoute;
        disconnectedExit.edges.erase(std::remove_if(disconnectedExit.edges.begin(), disconnectedExit.edges.end(),
                                                    [](const jf::RouteEdgeDefinition& edge) {
                                                        return edge.to == "cinderwatch_exit";
                                                    }),
                                     disconnectedExit.edges.end());
        std::string routeError;
        assert(!jf::validateRouteGraph(disconnectedExit, &routeError));
        assert(routeError == "route exit is not reachable from entrance");

        // Invalid regions.json (a stage referencing an unknown terrain
        // profile) must fail the whole Load, never silently drop the stage -
        // corrupt only a *scratch copy* of the data dir, same reasoning as
        // the Locale test's "never touch the real source file" comment.
        const std::filesystem::path scratchDir =
            std::filesystem::temp_directory_path() / "jf_regions_test_scratch";
        std::filesystem::remove_all(scratchDir);
        std::filesystem::copy(JF_SOURCE_DATA_DIR, scratchDir, std::filesystem::copy_options::recursive);
        std::ofstream corrupted(scratchDir / "regions.json", std::ios::trunc);
        corrupted << R"({"stages": [{"id": "bad_stage", "terrainProfileId": "no_such_profile",
                       "missionNameEn": "x", "missionNameJa": "x"}]})";
        corrupted.close();
        assert(!jf::loadGameData(scratchDir.string()));
        std::filesystem::remove_all(scratchDir);
    }

    {
        // docs/regions/ashiron_quarry.md「地点構成」: AnyMember branch -
        // clearing just quarry_old_mine (without touching quarry_hoist_works)
        // must be enough to advance past the branch to ashiron_vein.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        jf::GameApp app(*data);
        assert(startAshironQuarryExpedition(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)); // quarry_entrance
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition(); // -> quarry_terrace
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition(); // -> Camp I -> branch, first unresolved member
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition(); // AnyMember: one resolved member is enough -> ashiron_vein
        assert(app.currentMissionNameJa() == "灰鉄鉱脈");
    }

    {
        // docs/regions/ashiron_quarry.md「1. 崩落した搬入口」: 3 exploration
        // routes each with their own enemy count/loot per the spec table.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor ashironRegion = jf::regionDescriptor(jf::RegionId::AshironQuarry, *data);
        const jf::StageDescriptor& entranceStage = ashironRegion.stages[0];
        assert(entranceStage.id == "quarry_entrance" && entranceStage.enemyRoster.size() == 4);
        assert(entranceStage.scoutRouteRequiredClass == jf::UnitClass::HeavyInfantry);

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(entranceStage, choice, false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "stone") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::CollapsedSidePath), "stone") == 1); // 2 - 1
        assert(findLoot(lootFor(jf::ExplorationChoice::ScoutRoute), "stone") == 3); // 2 + 1

        jf::GameApp app(*data);
        assert(startAshironQuarryExpedition(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::CollapsedSidePath));
        int enemyCount = 0;
        for (const jf::Unit& unit : app.battle().battle().units())
            if (unit.team == jf::Team::Enemy) ++enemyCount;
        assert(enemyCount == 3); // base 4 - enemiesRemoved:1
    }

    {
        // docs/regions/ashiron_quarry.md「2. 砕石段丘」: StageDescriptor::
        // primarySecureTileAlternative (new, mirrors primaryHoldTileAlternative
        // above but with ObjectiveKind::SecureTile - a single touch, not a
        // multi-round hold). BattleFactory widens the default single-member
        // "primary" group to Any and adds the SecureTile objective, alongside
        // the untouched default EliminateTeam member - both must remain valid
        // win paths.
        jf::GameData data = makeFactoryData();
        jf::StageDescriptor stage = testStage0();
        stage.primarySecureTileAlternative =
            jf::StageDescriptor::HoldTileMissionRule{"haul_marker", 0, 5, 7};

        jf::BattleState battle = jf::createScenarioBattle(data, stage, /*seed=*/7);

        const jf::ObjectiveGroupDefinition* primaryGroup = nullptr;
        for (const auto& group : battle.missionState().groups)
            if (group.id == "primary") primaryGroup = &group;
        assert(primaryGroup && primaryGroup->rule == jf::ObjectiveGroupRule::Any);

        const jf::ObjectiveDefinition* secureDef = nullptr;
        for (const auto& def : battle.missionState().definitions)
            if (def.kind == jf::ObjectiveKind::SecureTile && def.id == "haul_marker") secureDef = &def;
        assert(secureDef && secureDef->primary);
        assert(secureDef->target.securingTeam == jf::Team::Player);
        assert(jf::isInBounds(secureDef->target.tile));
        assert(secureDef->target.tile.col >= 5 && secureDef->target.tile.col <= 7);

        // EliminateTeam remains an independent win path even with the SecureTile
        // alternative present: defeating every enemy wins without touching the tile.
        jf::AliveSnapshot before = jf::captureAliveSnapshot(battle);
        for (jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Enemy) unit.currentHp = 0;
        jf::emitUnitDefeatedEvents(battle, before);
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);
    }

    {
        // Same setup, but win via the SecureTile path instead: a player unit
        // ends its turn on the marker tile without any enemy being defeated.
        jf::GameData data = makeFactoryData();
        jf::StageDescriptor stage = testStage0();
        stage.primarySecureTileAlternative =
            jf::StageDescriptor::HoldTileMissionRule{"haul_marker", 0, 5, 7};
        jf::BattleState battle = jf::createScenarioBattle(data, stage, /*seed=*/7);
        const jf::ObjectiveDefinition* secureDef = nullptr;
        for (const auto& def : battle.missionState().definitions)
            if (def.kind == jf::ObjectiveKind::SecureTile && def.id == "haul_marker") secureDef = &def;
        assert(secureDef);
        jf::Unit* mover = nullptr;
        for (jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Player) { mover = &unit; break; }
        assert(mover);
        mover->position = secureDef->target.tile;
        // SecureTile only completes on an ActionResolvedEvent ending there
        // (plain position sync doesn't fire one) - see the dedicated
        // SecureTile unit test above this one for the same reasoning.
        jf::handleObjectiveEvent(battle.missionState(),
                                 jf::BattleEvent{1, 1,
                                                 jf::ActionResolvedEvent{1, mover->id, jf::Team::Player,
                                                                        jf::ActionKind::Wait, secureDef->target.tile}});
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);
    }

    {
        // docs/regions/ashiron_quarry.md「2. 砕石段丘」: enemy roster/loot per
        // route, and the ore-crate secondary bonus.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor ashironRegion = jf::regionDescriptor(jf::RegionId::AshironQuarry, *data);
        const jf::StageDescriptor& terraceStage = ashironRegion.stages[1];
        assert(terraceStage.id == "quarry_terrace" && terraceStage.enemyRoster.size() == 5);
        assert(terraceStage.scoutRouteRequiredClass == jf::UnitClass::FrontierEngineer);
        assert(terraceStage.primarySecureTileAlternative &&
              terraceStage.primarySecureTileAlternative->id == "quarry_terrace_haul_marker");

        auto lootFor = [&](jf::ExplorationChoice choice, bool surveySucceeded) {
            return jf::computeStageVictoryLoot(terraceStage, choice, surveySucceeded);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance, false), "iron") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance, true), "iron") == 3); // +ore crate bonus
        assert(findLoot(lootFor(jf::ExplorationChoice::ScoutRoute, false), "combustion_oil") == 1);
    }

    {
        // docs/regions/ashiron_quarry.md「地点構成」: AnyMember branch, mirror
        // of the quarry_old_mine-only test above - resolving just
        // quarry_hoist_works (without touching quarry_old_mine) must also be
        // enough to advance past the branch to ashiron_vein. The branch
        // always presents quarry_old_mine first (branchMembers order), so
        // resolution is forced directly via siteAccess rather than through
        // chooseExplorationRoute (which has no way to reach the 2nd member).
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        jf::GameApp app(*data);
        jf::SaveData save = app.createSaveData("en");
        save.base.completedRegionIds.insert(jf::RegionId::AshboughForest);
        save.base.completedRegionIds.insert(jf::RegionId::CinderwatchGate);
        save.base.siteAccess[jf::siteAccessKey(jf::RegionId::AshironQuarry, "quarry_hoist_works")] =
            jf::SiteAccessState::Secured;
        assert(app.applySaveData(save));
        assert(app.startExpedition(jf::RegionId::AshironQuarry));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)); // quarry_entrance
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition(); // -> quarry_terrace
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition(); // AnyMember: quarry_hoist_works already resolved -> ashiron_vein
        assert(app.currentMissionNameJa() == "灰鉄鉱脈");
    }

    {
        // docs/regions/ashiron_quarry.md「3B. 巻上機区画」: operating the
        // hoist Device wins the battle. Built directly via createScenarioBattle
        // (the RouteGraph's branch always routes normal play to
        // quarry_old_mine first, so quarry_hoist_works's own battle content
        // is exercised standalone here, the same way the SecureTile tests
        // above test primarySecureTileAlternative content directly).
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor ashironRegion = jf::regionDescriptor(jf::RegionId::AshironQuarry, *data);
        const jf::StageDescriptor* hoistStage = nullptr;
        for (const jf::StageDescriptor& stage : ashironRegion.stages)
            if (stage.id == "quarry_hoist_works") hoistStage = &stage;
        assert(hoistStage);

        jf::BattleState battle = jf::createScenarioBattle(
            *data, *hoistStage, /*seed=*/11,
            jf::stageRouteOutcome(*hoistStage, jf::ExplorationChoice::FrontalAdvance));
        for (const jf::BattleObjectState& object : battle.objects()) {
            const jf::BattleObjectDefinition* definition = battle.objectDefinition(object.definitionId);
            if (definition && definition->interaction) {
                if (jf::BattleObjectState* mutableObject = battle.findObject(object.id))
                    mutableObject->interactionCount = std::max(mutableObject->interactionCount, 1);
            }
        }
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);
    }

    {
        // docs/regions/ashiron_quarry.md「3B. 巻上機区画」: enemy roster/loot
        // per route (斧兵2・槍兵1・弓兵2, mirroring quarry_terrace's shape).
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor ashironRegion = jf::regionDescriptor(jf::RegionId::AshironQuarry, *data);
        const jf::StageDescriptor* hoistStage = nullptr;
        for (const jf::StageDescriptor& stage : ashironRegion.stages)
            if (stage.id == "quarry_hoist_works") hoistStage = &stage;
        assert(hoistStage && hoistStage->enemyRoster.size() == 5);
        assert(hoistStage->scoutRouteRequiredClass == jf::UnitClass::FrontierEngineer);

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(*hoistStage, choice, false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "iron") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::CollapsedSidePath), "iron") == 1); // 2 - 1
        assert(findLoot(lootFor(jf::ExplorationChoice::ScoutRoute), "combustion_oil") == 1);

        // The CollapsedSidePath route ("鉱石箱を先に落とす") removes 1 enemy,
        // exercised via createScenarioBattle directly (see above).
        jf::BattleState battle = jf::createScenarioBattle(
            *data, *hoistStage, /*seed=*/11,
            jf::stageRouteOutcome(*hoistStage, jf::ExplorationChoice::CollapsedSidePath));
        int enemyCount = 0;
        for (const jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Enemy) ++enemyCount;
        assert(enemyCount == 4); // base 5 - enemiesRemoved:1
    }

    {
        // docs/regions/ashiron_quarry.md「灰殻穿岩虫」「潜行突進」: telegraphs
        // one turn, executes the next - traveling along the boss's own row,
        // damaging any ally it passes (STR+3), covering the full range (3)
        // when unblocked. Mirrors the AshenhornBoar charge tests above.
        jf::Unit grubworm = makeUnit("grubworm", jf::Team::Enemy, {1, 5}, 3, jf::UnitClass::AshironGrubworm);
        grubworm.stats.strength = 9;
        grubworm.stats.defense = 8;
        grubworm.stats.resistance = 2;
        grubworm.stats.maxHp = 56;
        grubworm.currentHp = 56;
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {1, 2}); // same row, distance 3
        jf::BattleState battle({ally, grubworm});

        jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(battle.units()[1].chargeTelegraphed);
        assert(battle.units()[1].position == (jf::GridPos{1, 5})); // hasn't moved yet
        assert(battle.units()[0].currentHp == battle.units()[0].stats.maxHp); // untouched
        assert((battle.units()[1].bossRuntime.telegraph.lockedTiles ==
               std::vector<jf::GridPos>{{1, 4}, {1, 3}, {1, 2}}));
        // 岩殻防御: DEF+2 by default (no charge yet to recover from).
        assert(battle.units()[1].stats.defense == 10);

        battle.units()[1].hasActed = false; // simulate the next turn
        jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(!battle.units()[1].chargeTelegraphed);
        assert(battle.units()[1].position == (jf::GridPos{1, 2})); // covered the full range-3
        assert(battle.units()[0].currentHp < battle.units()[0].stats.maxHp);
        // The DEF+2 bonus is still in effect for this action (it was already
        // applied before the charge executed); the drop is queued for the
        // action right after.
        assert(battle.units()[1].stats.defense == 10);
        assert(battle.units()[1].bossChargeRecoveryPending);

        battle.units()[1].hasActed = false; // one more action: DEF+2 drops
        jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(battle.units()[1].stats.defense == 8);
        assert(!battle.units()[1].bossChargeRecoveryPending);
    }

    {
        // docs/regions/ashiron_quarry.md「灰殻穿岩虫」「崩落誘発」: HP<=50%で
        // 1回だけ発火し、盤面の空きマス2つがRubble(通行不可)へ変わる。
        jf::Unit grubworm = makeUnit("grubworm", jf::Team::Enemy, {1, 5}, 3, jf::UnitClass::AshironGrubworm);
        grubworm.stats.strength = 9;
        grubworm.stats.defense = 8;
        grubworm.stats.resistance = 2;
        grubworm.stats.maxHp = 56;
        grubworm.currentHp = 28; // exactly 50%
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {1, 0}); // out of charge range (distance 5)
        jf::BattleState battle({ally, grubworm});

        auto countRubble = [&]() {
            int count = 0;
            for (int row = 0; row < jf::kGridRows; ++row)
                for (int col = 0; col < jf::kGridCols; ++col)
                    if (battle.terrainAt({row, col}) == jf::TerrainType::Rubble) ++count;
            return count;
        };

        jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(battle.units()[1].bossCollapseUsed);
        assert(countRubble() == 2);

        battle.units()[1].hasActed = false; // doesn't fire a second time
        jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(countRubble() == 2);
    }

    {
        // docs/regions/ashiron_quarry.md「崩落核」: defeating the boss (HP0,
        // ScriptedWithdrawal like the AshenhornBoar) wins the standard
        // EliminateTeam battle - the "封鎖杭2箇所" AND-requirement is
        // approximated to a survey bonus instead (see M9-D plan), so no
        // Device interaction is needed to win.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor ashironRegion = jf::regionDescriptor(jf::RegionId::AshironQuarry, *data);
        const jf::StageDescriptor* coreStage = nullptr;
        for (const jf::StageDescriptor& stage : ashironRegion.stages)
            if (stage.id == "quarry_collapse_core") coreStage = &stage;
        assert(coreStage);

        jf::BattleState battle = jf::createScenarioBattle(*data, *coreStage, /*seed=*/13);
        const jf::AliveSnapshot before = jf::captureAliveSnapshot(battle);
        for (jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Enemy) unit.currentHp = 0;
        jf::emitUnitDefeatedEvents(battle, before);
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);

        const jf::Unit* boss = nullptr;
        for (const jf::Unit& unit : battle.units())
            if (unit.unitClass == jf::UnitClass::AshironGrubworm) boss = &unit;
        assert(boss && boss->exitReason == jf::UnitExitReason::ScriptedWithdrawal);
    }

    {
        // docs/regions/ashiron_quarry.md「崩落核」: 封鎖杭2箇所確保のボーナス、
        // scoutRouteDisabled(戦闘魔導士候補確定ルートは未配線のため無効化)、
        // 敵編成(ボス+雑魚2)を確認。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor ashironRegion = jf::regionDescriptor(jf::RegionId::AshironQuarry, *data);
        const jf::StageDescriptor* coreStage = nullptr;
        for (const jf::StageDescriptor& stage : ashironRegion.stages)
            if (stage.id == "quarry_collapse_core") coreStage = &stage;
        assert(coreStage && coreStage->enemyRoster.size() == 3);
        assert(coreStage->scoutRouteDisabled);
        assert(coreStage->surveyObjectiveId == "quarry_collapse_core_stakes");
        assert(coreStage->surveyTileCount && *coreStage->surveyTileCount == 2);

        auto lootFor = [&](bool surveySucceeded) {
            return jf::computeStageVictoryLoot(*coreStage, jf::ExplorationChoice::FrontalAdvance, surveySucceeded);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(false), "quality_iron") == 0);
        assert(findLoot(lootFor(true), "quality_iron") == 1); // stakes secured
        assert(findLoot(lootFor(false), "ashiron_shell") == 1);
    }

    {
        // docs/regions/blackwater_lowlands.md「沼牙の大蛇」「毒牙」: range-1
        // STR+3 physical attack that poisons on hit unless already poisoned.
        jf::Unit serpent = makeUnit("serpent", jf::Team::Enemy, {1, 1}, 4, jf::UnitClass::MarshFangSerpent);
        serpent.stats.strength = 9;
        serpent.stats.defense = 6;
        serpent.stats.resistance = 3;
        serpent.stats.maxHp = 60;
        serpent.currentHp = 60;
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {1, 2}); // adjacent
        jf::BattleState battle({ally, serpent});

        jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(battle.units()[0].currentHp < battle.units()[0].stats.maxHp);
        assert(battle.units()[0].poisonRemainingProcs > 0);
        const int hpAfterFirstBite = battle.units()[0].currentHp;

        battle.units()[1].hasActed = false; // simulate the next turn, still adjacent+poisoned
        jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(battle.units()[0].currentHp < hpAfterFirstBite); // still takes normal attack damage
    }

    {
        // docs/regions/blackwater_lowlands.md「沼牙の大蛇」「締め付け」: fires
        // (STR+1, Move Down, no stacking) only once 2+ players are adjacent -
        // otherwise falls through to a normal single-target attack.
        jf::Unit serpent = makeUnit("serpent", jf::Team::Enemy, {1, 1}, 4, jf::UnitClass::MarshFangSerpent);
        serpent.stats.strength = 9;
        serpent.stats.defense = 6;
        serpent.stats.resistance = 3;
        serpent.stats.maxHp = 60;
        serpent.currentHp = 60;
        // allyA is orthogonally adjacent AND in the front-3 column (takes
        // the constrict hit); allyB is orthogonally adjacent but NOT in the
        // front-3 column (only counts toward the 2+ trigger condition).
        jf::Unit allyA = makeUnit("allyA", jf::Team::Player, {1, 0});
        jf::Unit allyB = makeUnit("allyB", jf::Team::Player, {0, 1});
        jf::BattleState battle({allyA, allyB, serpent});

        jf::takeEnemyTurn(battle, battle.units()[2]);
        assert(battle.units()[0].currentHp < battle.units()[0].stats.maxHp);
        assert(battle.units()[0].moveDownActive);
        assert(battle.units()[1].currentHp == battle.units()[1].stats.maxHp); // outside the front-3 pattern
    }

    {
        // docs/regions/blackwater_lowlands.md「沼牙の大蛇」「激しい身震い」:
        // HP<=50%で一度だけ、隣接4マスのユニットを1マス押し出す。
        jf::Unit serpent = makeUnit("serpent", jf::Team::Enemy, {2, 2}, 4, jf::UnitClass::MarshFangSerpent);
        serpent.stats.strength = 9;
        serpent.stats.defense = 6;
        serpent.stats.resistance = 3;
        serpent.stats.maxHp = 60;
        serpent.currentHp = 30; // exactly 50%
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {2, 3}); // adjacent, east
        jf::BattleState battle({ally, serpent});

        jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(battle.units()[1].bossShudderUsed);
        assert(battle.units()[0].position == (jf::GridPos{2, 4})); // knocked back 1 tile away

        battle.units()[1].hasActed = false; // doesn't fire a second time
        battle.units()[0].position = jf::GridPos{2, 3}; // move back adjacent to re-test
        jf::takeEnemyTurn(battle, battle.units()[1]);
        assert((battle.units()[0].position == jf::GridPos{2, 3})); // no second push-out
    }

    {
        // docs/regions/blackwater_lowlands.md「深泥の水源」: defeating the boss
        // (HP0, ScriptedWithdrawal) wins the standard EliminateTeam battle -
        // the "水源標識1個以上で行動終了" AND-component of the primary is
        // approximated away (same M9-D precedent: no AND-composition infra
        // for a single site), and "標識2個確保" is a survey bonus instead.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor blackwaterRegion = jf::regionDescriptor(jf::RegionId::BlackwaterLowlands, *data);
        const jf::StageDescriptor* mireStage = nullptr;
        for (const jf::StageDescriptor& stage : blackwaterRegion.stages)
            if (stage.id == "deep_mire") mireStage = &stage;
        assert(mireStage);
        assert(mireStage->surveyObjectiveId == "deep_mire_water_markers");
        assert(mireStage->surveyTileCount && *mireStage->surveyTileCount == 2);

        jf::BattleState battle = jf::createScenarioBattle(*data, *mireStage, /*seed=*/17);
        const jf::AliveSnapshot before = jf::captureAliveSnapshot(battle);
        for (jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Enemy) unit.currentHp = 0;
        jf::emitUnitDefeatedEvents(battle, before);
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);

        const jf::Unit* boss = nullptr;
        for (const jf::Unit& unit : battle.units())
            if (unit.unitClass == jf::UnitClass::MarshFangSerpent) boss = &unit;
        assert(boss && boss->exitReason == jf::UnitExitReason::ScriptedWithdrawal);
    }

    {
        // docs/regions/blackwater_lowlands.md「地域攻略と拠点接続」: Windswept
        // Highland (第5地域) becomes selectable once Blackwater Lowlands
        // completes, mirroring how Ashiron Quarry unlocked after Cinderwatch.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(!app.isRegionUnlocked(jf::RegionId::WindscarPlateau));
        jf::SaveData save = app.createSaveData("en");
        save.base.completedRegionIds.insert(jf::RegionId::BlackwaterLowlands);
        assert(app.applySaveData(save));
        assert(app.isRegionUnlocked(jf::RegionId::WindscarPlateau));
    }

    {
        // docs/regions/windscar_plateau.md「地点構成」: 6-site skeleton + 2
        // camps + the site 3/4 either-order-but-both-required branch, mirror
        // of the BlackwaterLowlands skeleton test above.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor windscarRegion = jf::regionDescriptor(jf::RegionId::WindscarPlateau, *data);
        assert(windscarRegion.stages.size() == 6);
        assert(windscarRegion.stages[0].id == "windscar_ascent");

        const jf::RegionRouteGraph& windscarRoute = jf::regionRouteGraph(jf::RegionId::WindscarPlateau);
        std::string error;
        assert(jf::validateRouteGraph(windscarRoute, &error));
        assert(jf::findRouteNode(windscarRoute, "windwatch_station"));
        assert(jf::findRouteNode(windscarRoute, "split_convoy"));
        const jf::RouteNodeDefinition* branch = jf::findRouteNode(windscarRoute, "windwatch_convoy_branch");
        assert(branch && branch->kind == jf::RouteNodeKind::BranchGroup &&
               branch->branchCompletion == jf::BranchCompletion::AllMembers);
    }

    {
        // docs/regions/windscar_plateau.md「1. 風下の登り口」: 主目的4体・
        // 副目標(標識で行動終了)・勝利報酬(獣皮2、硬木1)・斥候ルート報酬
        // (織物1)。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor windscarRegion = jf::regionDescriptor(jf::RegionId::WindscarPlateau, *data);
        const jf::StageDescriptor& ascentStage = windscarRegion.stages[0];
        assert(ascentStage.enemyRoster.size() == 4);
        assert(ascentStage.surveyObjectiveId == "windscar_ascent_marker");
        assert(ascentStage.scoutRouteRequiredClass == jf::UnitClass::FrontierScout);
        assert(ascentStage.windGust && ascentStage.windGust->triggerRound == 3);

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(ascentStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "hide") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "hardwood") == 1);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "cloth") == 0);
        assert(findLoot(lootFor(jf::ExplorationChoice::ScoutRoute), "cloth") == 1);

        jf::BattleState battle = jf::createScenarioBattle(*data, ascentStage, /*seed=*/5);
        int enemyCount = 0;
        for (const jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Enemy) ++enemyCount;
        assert(enemyCount == 4);
    }

    {
        // docs/regions/windscar_plateau.md「2. 崩れた中継路」: mirror of
        // blackwater_crossing's own guest-escort tests above - the guest
        // reaching the escape tile wins standalone (primary is EscapeUnits,
        // not EliminateTeam), and losing the guest is Defeat independent of
        // the player squad's own state.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor windscarRegion = jf::regionDescriptor(jf::RegionId::WindscarPlateau, *data);
        const jf::StageDescriptor* relayStage = nullptr;
        for (const jf::StageDescriptor& stage : windscarRegion.stages)
            if (stage.id == "windscar_relay") relayStage = &stage;
        assert(relayStage && relayStage->guestUnits.size() == 1);
        assert(relayStage->enemyRoster.size() == 4);
        assert(relayStage->scoutRouteRequiredClass == jf::UnitClass::FrontierEngineer);

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(*relayStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "cloth") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "riding_gear") == 1);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "hardwood") == 0);
        assert(findLoot(lootFor(jf::ExplorationChoice::ScoutRoute), "hardwood") == 1);

        jf::BattleState battle = jf::createScenarioBattle(*data, *relayStage, /*seed=*/7);
        assert(battle.missionState().guestUnitIds.size() == 1);
        for (const jf::Unit& unit : battle.units())
            if (unit.isGuest) assert(unit.team == jf::Team::Player);

        const jf::ObjectiveDefinition* escapeDef = nullptr;
        for (const auto& def : battle.missionState().definitions)
            if (def.id == "windscar_relay_escape") escapeDef = &def;
        assert(escapeDef && escapeDef->primary && escapeDef->kind == jf::ObjectiveKind::EscapeUnits);

        const std::string& guestId = battle.missionState().guestUnitIds[0];
        jf::BattleEvent guestEscapes{
            1, 1,
            jf::ActionResolvedEvent{1, guestId, jf::Team::Player, jf::ActionKind::Wait, escapeDef->target.tile}};
        jf::handleObjectiveEvent(battle.missionState(), guestEscapes);
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);
    }

    {
        // docs/regions/windscar_plateau.md「2. 崩れた中継路」敗北条件「護衛対象の
        // 撤退」: allGuestsLost() fires Defeat even with the player squad
        // fully alive, same shape as blackwater_crossing's own test.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor windscarRegion = jf::regionDescriptor(jf::RegionId::WindscarPlateau, *data);
        const jf::StageDescriptor* relayStage = nullptr;
        for (const jf::StageDescriptor& stage : windscarRegion.stages)
            if (stage.id == "windscar_relay") relayStage = &stage;
        assert(relayStage);

        jf::BattleState battle = jf::createScenarioBattle(*data, *relayStage, /*seed=*/7);
        assert(!battle.allGuestsLost());
        for (jf::Unit& unit : battle.units())
            if (unit.isGuest) unit.currentHp = 0;
        assert(battle.allGuestsLost());
        assert(!battle.allPlayersDefeated());
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Defeat);
    }

    {
        // docs/regions/windscar_plateau.md「3. 風見台」: primary is 2
        // OperateObject Objectives (風見盤2個), mirroring signal_tower's own
        // dual-panel shape (cinderwatch_gate.md「5. 信号塔下層」) via the same
        // objectPlacementRules/operateObjectiveId JSON Schema - so defeating
        // every enemy without operating both panels must NOT win, and
        // operating only one must NOT win either.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor windscarRegion = jf::regionDescriptor(jf::RegionId::WindscarPlateau, *data);
        const jf::StageDescriptor* windwatchStage = nullptr;
        for (const jf::StageDescriptor& stage : windscarRegion.stages)
            if (stage.id == "windwatch_station") windwatchStage = &stage;
        assert(windwatchStage);
        assert(windwatchStage->enemyRoster.size() == 5);
        assert(windwatchStage->scoutRouteRequiredClass == jf::UnitClass::WatchArcher);

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(*windwatchStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "hardwood") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "cloth") == 1);

        jf::BattleState battle = jf::createScenarioBattle(*data, *windwatchStage, /*seed=*/11);
        int enemyCount = 0;
        for (const jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Enemy) ++enemyCount;
        assert(enemyCount == 5);

        for (jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Enemy) unit.currentHp = 0;

        jf::BattleObjectState* northPanel = battle.findObject("windwatch_panel_north_1");
        assert(northPanel != nullptr);
        northPanel->interactionCount = 1; // only ONE of the 2 panels operated
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind != jf::BattleOutcomeKind::Victory);

        jf::BattleObjectState* southPanel = battle.findObject("windwatch_panel_south_1");
        assert(southPanel != nullptr);
        southPanel->interactionCount = 1; // both panels now operated
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);
    }

    {
        // docs/regions/windscar_plateau.md「4. 分断された輸送隊」: mirror of
        // windscar_relay's own guest-escort tests above - the guest reaching
        // the escape tile wins standalone (primary is EscapeUnits), and
        // losing all guests is Defeat independent of the player squad's own
        // state.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor windscarRegion = jf::regionDescriptor(jf::RegionId::WindscarPlateau, *data);
        const jf::StageDescriptor* convoyStage = nullptr;
        for (const jf::StageDescriptor& stage : windscarRegion.stages)
            if (stage.id == "split_convoy") convoyStage = &stage;
        assert(convoyStage && convoyStage->guestUnits.size() == 2);
        assert(convoyStage->enemyRoster.size() == 5);
        assert(convoyStage->scoutRouteRequiredClass == jf::UnitClass::MessengerCavalry);

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(*convoyStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::CollapsedSidePath), "cloth") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::CollapsedSidePath), "riding_gear") == 1);
        // ルート1「荷物報酬-1」: riding_gearが1減って0(quantity>0のみ残す既存
        // フィルタにより結果からriding_gear自体が消える)。
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "cloth") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "riding_gear") == 0);

        jf::ExplorationOutcome frontalOutcome =
            jf::stageRouteOutcome(*convoyStage, jf::ExplorationChoice::FrontalAdvance);
        jf::BattleState battle = jf::createScenarioBattle(*data, *convoyStage, /*seed=*/13, frontalOutcome);
        assert(battle.missionState().guestUnitIds.size() == 2);
        for (const jf::Unit& unit : battle.units())
            if (unit.isGuest) assert(unit.team == jf::Team::Player);
        int enemyCount = 0;
        for (const jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Enemy) ++enemyCount;
        assert(enemyCount == 4); // FrontalAdvance route: enemiesRemoved=1 from the 5-unit base roster

        const jf::ObjectiveDefinition* escapeDef = nullptr;
        for (const auto& def : battle.missionState().definitions)
            if (def.id == "split_convoy_escape") escapeDef = &def;
        assert(escapeDef && escapeDef->primary && escapeDef->kind == jf::ObjectiveKind::EscapeUnits);

        const std::string& guestId = battle.missionState().guestUnitIds[0];
        jf::BattleEvent guestEscapes{
            1, 1,
            jf::ActionResolvedEvent{1, guestId, jf::Team::Player, jf::ActionKind::Wait, escapeDef->target.tile}};
        jf::handleObjectiveEvent(battle.missionState(), guestEscapes);
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);
    }

    {
        // docs/regions/windscar_plateau.md「4. 分断された輸送隊」敗北条件
        // 「負傷者をすべて失う」: allGuestsLost() fires Defeat even with the
        // player squad fully alive, same shape as windscar_relay's own test.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor windscarRegion = jf::regionDescriptor(jf::RegionId::WindscarPlateau, *data);
        const jf::StageDescriptor* convoyStage = nullptr;
        for (const jf::StageDescriptor& stage : windscarRegion.stages)
            if (stage.id == "split_convoy") convoyStage = &stage;
        assert(convoyStage);

        jf::BattleState battle = jf::createScenarioBattle(*data, *convoyStage, /*seed=*/13);
        assert(!battle.allGuestsLost());
        for (jf::Unit& unit : battle.units())
            if (unit.isGuest) unit.currentHp = 0;
        assert(battle.allGuestsLost());
        assert(!battle.allPlayersDefeated());
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Defeat);
    }

    {
        // docs/regions/windscar_plateau.md「4. 分断された輸送隊」`[伝令騎兵]`
        // ルート: enemyRoster's 5th unit (軽装剣士相当) only appears via
        // ScoutRoute (no enemiesRemoved on that route), while the other 2
        // routes drop it (enemiesRemoved=1).
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor windscarRegion = jf::regionDescriptor(jf::RegionId::WindscarPlateau, *data);
        const jf::StageDescriptor* convoyStage = nullptr;
        for (const jf::StageDescriptor& stage : windscarRegion.stages)
            if (stage.id == "split_convoy") convoyStage = &stage;
        assert(convoyStage);

        jf::ExplorationOutcome scoutOutcome =
            jf::stageRouteOutcome(*convoyStage, jf::ExplorationChoice::ScoutRoute);
        jf::BattleState battle = jf::createScenarioBattle(*data, *convoyStage, /*seed=*/13, scoutOutcome);
        int enemyCount = 0;
        for (const jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Enemy) ++enemyCount;
        assert(enemyCount == 5);
    }

    {
        // docs/regions/windscar_plateau.md「キャンプII」: available only once
        // both site 3 (windwatch_station) and site 4 (split_convoy) are
        // complete - RouteGraph.cpp's windscarPlateauGraph() wires this via
        // BranchCompletion::AllMembers on the branch node feeding
        // windscar_camp2, same mechanism CinderwatchGate/BlackwaterLowlands's
        // own branches use. Verified here directly on the graph data (not a
        // full campaign-state test, matching this project's existing
        // reachability-test precedent for these branches).
        const jf::RegionRouteGraph& graph = jf::regionRouteGraph(jf::RegionId::WindscarPlateau);
        const jf::RouteNodeDefinition* convoyBranch = nullptr;
        for (const jf::RouteNodeDefinition& node : graph.nodes)
            if (node.kind == jf::RouteNodeKind::BranchGroup && node.branchMembers.size() == 2 &&
                std::find(node.branchMembers.begin(), node.branchMembers.end(), "windwatch_station") !=
                    node.branchMembers.end() &&
                std::find(node.branchMembers.begin(), node.branchMembers.end(), "split_convoy") !=
                    node.branchMembers.end())
                convoyBranch = &node;
        assert(convoyBranch);
        assert(convoyBranch->branchCompletion == jf::BranchCompletion::AllMembers);
    }

    {
        // docs/regions/windscar_plateau.md「5. 断崖荷車道」: mirror of
        // split_convoy's own guest-escort tests above - the cart-guest
        // reaching the escape tile wins standalone (primary is EscapeUnits),
        // and losing the cart-guest is Defeat independent of the player
        // squad's own state.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor windscarRegion = jf::regionDescriptor(jf::RegionId::WindscarPlateau, *data);
        const jf::StageDescriptor* cartStage = nullptr;
        for (const jf::StageDescriptor& stage : windscarRegion.stages)
            if (stage.id == "cliff_cart_road") cartStage = &stage;
        assert(cartStage && cartStage->guestUnits.size() == 1);
        assert(cartStage->enemyRoster.size() == 5);
        assert(cartStage->scoutRouteRequiredClass == jf::UnitClass::HeavyInfantry);

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(*cartStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "hardwood") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "hide") == 1);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "riding_gear") == 1);
        // ルート2「騎具素材-1」: riding_gearが1減って0(quantity>0のみ残す既存
        // フィルタにより結果からriding_gear自体が消える)。
        assert(findLoot(lootFor(jf::ExplorationChoice::CollapsedSidePath), "hardwood") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::CollapsedSidePath), "riding_gear") == 0);

        jf::ExplorationOutcome frontalOutcome =
            jf::stageRouteOutcome(*cartStage, jf::ExplorationChoice::FrontalAdvance);
        jf::BattleState battle = jf::createScenarioBattle(*data, *cartStage, /*seed=*/13, frontalOutcome);
        assert(battle.missionState().guestUnitIds.size() == 1);
        for (const jf::Unit& unit : battle.units())
            if (unit.isGuest) assert(unit.team == jf::Team::Player);
        int enemyCount = 0;
        for (const jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Enemy) ++enemyCount;
        assert(enemyCount == 5); // FrontalAdvance route: base 5-unit roster, no removal

        const jf::ObjectiveDefinition* escapeDef = nullptr;
        for (const auto& def : battle.missionState().definitions)
            if (def.id == "cliff_cart_escape") escapeDef = &def;
        assert(escapeDef && escapeDef->primary && escapeDef->kind == jf::ObjectiveKind::EscapeUnits);

        const std::string& guestId = battle.missionState().guestUnitIds[0];
        jf::BattleEvent guestEscapes{
            1, 1,
            jf::ActionResolvedEvent{1, guestId, jf::Team::Player, jf::ActionKind::Wait, escapeDef->target.tile}};
        jf::handleObjectiveEvent(battle.missionState(), guestEscapes);
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);
    }

    {
        // docs/regions/windscar_plateau.md「5. 断崖荷車道」敗北条件「全輸送
        // 対象の撤退」: allGuestsLost() fires Defeat even with the player
        // squad fully alive, same shape as split_convoy's own test.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor windscarRegion = jf::regionDescriptor(jf::RegionId::WindscarPlateau, *data);
        const jf::StageDescriptor* cartStage = nullptr;
        for (const jf::StageDescriptor& stage : windscarRegion.stages)
            if (stage.id == "cliff_cart_road") cartStage = &stage;
        assert(cartStage);

        jf::BattleState battle = jf::createScenarioBattle(*data, *cartStage, /*seed=*/13);
        assert(!battle.allGuestsLost());
        for (jf::Unit& unit : battle.units())
            if (unit.isGuest) unit.currentHp = 0;
        assert(battle.allGuestsLost());
        assert(!battle.allPlayersDefeated());
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Defeat);
    }

    {
        // docs/regions/windscar_plateau.md「5. 断崖荷車道」ルート2「敵4体」:
        // enemiesRemoved=1 from the 5-unit base roster (route1/3 keep the
        // full roster).
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor windscarRegion = jf::regionDescriptor(jf::RegionId::WindscarPlateau, *data);
        const jf::StageDescriptor* cartStage = nullptr;
        for (const jf::StageDescriptor& stage : windscarRegion.stages)
            if (stage.id == "cliff_cart_road") cartStage = &stage;
        assert(cartStage);

        jf::ExplorationOutcome sideOutcome =
            jf::stageRouteOutcome(*cartStage, jf::ExplorationChoice::CollapsedSidePath);
        jf::BattleState battle = jf::createScenarioBattle(*data, *cartStage, /*seed=*/13, sideOutcome);
        int enemyCount = 0;
        for (const jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Enemy) ++enemyCount;
        assert(enemyCount == 4);
    }

    {
        // docs/regions/windscar_plateau.md「強風ルール」: unit standing on a
        // WindGust tile at the configured trigger Round is pushed 1 tile
        // along `delta`; a blocked destination deals fixed 2 damage instead
        // (not BattleState::applyKnockback()'s stagger outcome).
        jf::Unit windedUnit = makeUnit("winded", jf::Team::Player, {0, 3});
        jf::Unit heavyUnit = makeUnit("heavy", jf::Team::Player, {0, 4}, 4, jf::UnitClass::HeavyInfantry);
        jf::Unit blockedUnit = makeUnit("blocked", jf::Team::Player, {2, 5});
        std::array<jf::TerrainType, jf::kGridRows * jf::kGridCols> terrain{};
        terrain.fill(jf::TerrainType::Floor);
        terrain[0 * jf::kGridCols + 3] = jf::TerrainType::WindGust;
        terrain[0 * jf::kGridCols + 4] = jf::TerrainType::WindGust;
        terrain[2 * jf::kGridCols + 5] = jf::TerrainType::WindGust; // pushed toward row 3: out of bounds

        jf::BattleState battle({windedUnit, heavyUnit, blockedUnit}, terrain);
        battle.setWindGust(jf::BattleState::WindGustConfig{jf::GridPos{1, 0}, /*triggerRound=*/1});
        assert(battle.round() == 1);
        jf::resolveWindGustRoundEnd(battle);

        const jf::Unit* windedAfter = battle.findUnit("winded");
        assert(windedAfter && windedAfter->position == (jf::GridPos{1, 3}));

        const jf::Unit* heavyAfter = battle.findUnit("heavy");
        assert(heavyAfter && heavyAfter->position == (jf::GridPos{0, 4})); // 重量装甲: unmoved
        assert(heavyAfter->currentHp == heavyAfter->stats.maxHp); // and no collision damage

        const jf::Unit* blockedAfter = battle.findUnit("blocked");
        assert(blockedAfter && blockedAfter->position == (jf::GridPos{2, 5})); // board edge: unmoved
        assert(blockedAfter->currentHp == blockedAfter->stats.maxHp - 2); // fixed 2 collision damage
    }

    {
        // docs/regions/blackwater_lowlands.md「1. 灰水の沈み道」: 3探索ルートの
        // 敵数・報酬差分。ルート効果はCinderwatch/Ashbough共通の
        // cinderwatchOutcome()デフォルト(rush=partyDamage2+enemiesRemoved1、
        // scout=自由配置左3列)とそのまま一致するため、routeOutcomesの上書きは
        // 無く、routeVictoryLootDelta(薬草報酬なし/湿地樹脂+1)だけで表現している。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor blackwaterRegion = jf::regionDescriptor(jf::RegionId::BlackwaterLowlands, *data);
        const jf::StageDescriptor& sunkenPathStage = blackwaterRegion.stages[0];
        assert(sunkenPathStage.id == "sunken_path" && sunkenPathStage.enemyRoster.size() == 4);

        auto lootFor = [&](jf::ExplorationChoice choice, bool surveySucceeded) {
            return jf::computeStageVictoryLoot(sunkenPathStage, choice, surveySucceeded);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance, false), "herb") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance, true), "herb") == 3); // +marker bonus
        assert(findLoot(lootFor(jf::ExplorationChoice::CollapsedSidePath, false), "herb") == 0); // no herb reward
        assert(findLoot(lootFor(jf::ExplorationChoice::ScoutRoute, false), "wetland_resin") == 1);

        jf::GameApp app(*data);
        assert(startBlackwaterExpedition(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::CollapsedSidePath));
        int enemyCount = 0;
        for (const jf::Unit& unit : app.battle().battle().units())
            if (unit.team == jf::Team::Enemy) ++enemyCount;
        assert(enemyCount == 3); // base 4 - enemiesRemoved:1 (cinderwatchOutcome default)
    }

    {
        // docs/regions/blackwater_lowlands.md「地点構成」: herb_islet/
        // resin_grove branch requires BOTH members (AllMembers) - winning
        // only one and returning to the branch must present the OTHER
        // member next, not skip ahead to Camp II (mirrors Cinderwatch's
        // ironwatch_stores/old_barracks AllMembers test).
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        jf::GameApp app(*data);
        assert(startBlackwaterExpedition(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)); // sunken_path
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition(); // -> reedway_fork
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition(); // -> Camp I -> branch, first unresolved member
        assert(app.currentMissionNameJa() == "薬草洲");
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition(); // AllMembers: only 1 of 2 resolved -> branch again, other member
        assert(app.screen() == jf::Screen::Exploration);
        assert(app.currentMissionNameJa() == "樹脂林");
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition(); // both resolved -> Camp II -> blackwater_crossing
        assert(app.currentMissionNameJa() == "黒水渡し");
    }

    {
        // docs/regions/blackwater_lowlands.md「2. 葦原の分岐」: 3探索ルートの
        // 敵数・報酬差分。基本編成5体(「葦を刈って見通す」相当)へ
        // enemiesRemoved(1/2)を適用する反転トリック(M9-Bで確立済み)で表現。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor blackwaterRegion = jf::regionDescriptor(jf::RegionId::BlackwaterLowlands, *data);
        const jf::StageDescriptor& reedwayStage = blackwaterRegion.stages[1];
        assert(reedwayStage.id == "reedway_fork" && reedwayStage.enemyRoster.size() == 5);
        assert(reedwayStage.scoutRouteRequiredClass == jf::UnitClass::FrontierRanger);
        assert(reedwayStage.primarySecureTileAlternative &&
              reedwayStage.primarySecureTileAlternative->id == "reedway_fork_exit");
        assert(reedwayStage.surveyObjectiveId == "reedway_fork_markers");

        auto lootFor = [&](jf::ExplorationChoice choice, bool surveySucceeded) {
            return jf::computeStageVictoryLoot(reedwayStage, choice, surveySucceeded);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance, false), "herb") == 1);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance, true), "herb") == 2); // +marker bonus
        assert(findLoot(lootFor(jf::ExplorationChoice::ScoutRoute, false), "poison_material") == 1);

        data->playerParty[0].classId = jf::UnitClass::FrontierRanger; // ScoutRoute gate
        jf::GameApp app(*data);
        assert(startBlackwaterExpedition(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)); // sunken_path
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition(); // -> reedway_fork
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::ScoutRoute));
        int enemyCount = 0;
        for (const jf::Unit& unit : app.battle().battle().units())
            if (unit.team == jf::Team::Enemy) ++enemyCount;
        assert(enemyCount == 3); // base 5 - enemiesRemoved:2
    }

    {
        // docs/regions/blackwater_lowlands.md「2. 葦原の分岐」:
        // primarySecureTileAlternative - EliminateTeamと独立にSecureTileでも
        // 勝利できること(ashroad_watch/quarry_terraceの低レベルテストと同型)。
        jf::GameData data = makeFactoryData();
        jf::StageDescriptor stage = testStage0();
        stage.primarySecureTileAlternative =
            jf::StageDescriptor::HoldTileMissionRule{"reedway_fork_exit", 0, 5, 7};
        jf::BattleState battle = jf::createScenarioBattle(data, stage, /*seed=*/7);

        const jf::ObjectiveDefinition* secureDef = nullptr;
        for (const auto& def : battle.missionState().definitions)
            if (def.kind == jf::ObjectiveKind::SecureTile && def.id == "reedway_fork_exit") secureDef = &def;
        assert(secureDef && secureDef->primary);
        assert(secureDef->target.tile.col >= 5 && secureDef->target.tile.col <= 7);

        jf::Unit* mover = nullptr;
        for (jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Player) { mover = &unit; break; }
        assert(mover);
        mover->position = secureDef->target.tile;
        jf::handleObjectiveEvent(battle.missionState(),
                                 jf::BattleEvent{1, 1,
                                                 jf::ActionResolvedEvent{1, mover->id, jf::Team::Player,
                                                                        jf::ActionKind::Wait, secureDef->target.tile}});
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);
    }

    {
        // docs/regions/blackwater_lowlands.md「3. 薬草洲」: primarySurviveRoundsAlternative
        // - primary objective is EliminateTeam OR surviving until a given
        // round. Mirrors the primarySecureTileAlternative tests above.
        jf::GameData data = makeFactoryData();
        jf::StageDescriptor stage = testStage0();
        stage.primarySurviveRoundsAlternative =
            jf::StageDescriptor::SurviveRoundsMissionRule{"herb_islet_defense", 3};
        jf::BattleState battle = jf::createScenarioBattle(data, stage, /*seed=*/7);

        const jf::ObjectiveGroupDefinition* primaryGroup = nullptr;
        for (const auto& group : battle.missionState().groups)
            if (group.id == "primary") primaryGroup = &group;
        assert(primaryGroup && primaryGroup->rule == jf::ObjectiveGroupRule::Any);

        const jf::ObjectiveDefinition* surviveDef = nullptr;
        for (const auto& def : battle.missionState().definitions)
            if (def.kind == jf::ObjectiveKind::SurviveRounds && def.id == "herb_islet_defense") surviveDef = &def;
        assert(surviveDef && surviveDef->primary && surviveDef->target.surviveUntilRound == 3);

        // EliminateTeam remains an independent win path.
        jf::AliveSnapshot before = jf::captureAliveSnapshot(battle);
        for (jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Enemy) unit.currentHp = 0;
        jf::emitUnitDefeatedEvents(battle, before);
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);
    }

    {
        // Same setup, but win via SurviveRounds instead: no enemy is
        // defeated, the battle simply outlasts round 3.
        jf::GameData data = makeFactoryData();
        jf::StageDescriptor stage = testStage0();
        stage.primarySurviveRoundsAlternative =
            jf::StageDescriptor::SurviveRoundsMissionRule{"herb_islet_defense", 3};
        jf::BattleState battle = jf::createScenarioBattle(data, stage, /*seed=*/7);
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind != jf::BattleOutcomeKind::Victory);
        while (battle.round() <= 3) {
            battle.beginEnemyPhase();
            battle.beginPlayerPhase();
        }
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);
    }

    {
        // docs/regions/blackwater_lowlands.md「3. 薬草洲」: 3探索ルートの
        // 敵数・報酬差分、`[暁の衛生兵]`ルートのrouteDiscoveries確認。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor blackwaterRegion = jf::regionDescriptor(jf::RegionId::BlackwaterLowlands, *data);
        const jf::StageDescriptor& herbStage = blackwaterRegion.stages[2];
        assert(herbStage.id == "herb_islet" && herbStage.enemyRoster.size() == 4);
        assert(herbStage.scoutRouteRequiredClass == jf::UnitClass::DawnChirurgeon);
        assert(herbStage.primarySurviveRoundsAlternative &&
              herbStage.primarySurviveRoundsAlternative->id == "herb_islet_defense");
        assert(herbStage.timedReinforcement && herbStage.timedReinforcement->id == "herb_islet_spider_wave");

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(herbStage, choice, false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "herb") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::CollapsedSidePath), "herb") == 4); // 2 + 2
        assert(findLoot(lootFor(jf::ExplorationChoice::ScoutRoute), "herb") == 0); // 2 - 2
        assert(findLoot(lootFor(jf::ExplorationChoice::ScoutRoute), "quality_herb") == 2);

        auto discoveriesFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageDiscoveries(herbStage, choice);
        };
        assert(discoveriesFor(jf::ExplorationChoice::FrontalAdvance).empty());
        const auto scoutDiscoveries = discoveriesFor(jf::ExplorationChoice::ScoutRoute);
        assert(scoutDiscoveries.size() == 1 && scoutDiscoveries[0] == "marsh_emergency_medicine");

        // CollapsedSidePath ("奥まで採取") installs the round-2 spider wave;
        // the other 2 routes don't (mirrors Herbwater Hollow's wolf-wave test).
        jf::ExplorationOutcome harvest = jf::stageRouteOutcome(herbStage, jf::ExplorationChoice::CollapsedSidePath);
        jf::BattleState withWave = jf::createScenarioBattle(*data, herbStage, 42, harvest);
        assert(withWave.reinforcementWaves().size() == 1);
        assert(withWave.reinforcementWaves()[0].id == "herb_islet_spider_wave");
        assert(withWave.reinforcementWaves()[0].spawnRound == 2);
        jf::ExplorationOutcome standard = jf::stageRouteOutcome(herbStage, jf::ExplorationChoice::FrontalAdvance);
        jf::BattleState withoutWave = jf::createScenarioBattle(*data, herbStage, 42, standard);
        assert(withoutWave.reinforcementWaves().empty());
    }

    {
        // docs/regions/blackwater_lowlands.md「4. 樹脂林」: 3探索ルートの
        // 敵数・報酬差分。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor blackwaterRegion = jf::regionDescriptor(jf::RegionId::BlackwaterLowlands, *data);
        const jf::StageDescriptor& resinStage = blackwaterRegion.stages[3];
        assert(resinStage.id == "resin_grove" && resinStage.enemyRoster.size() == 4);
        assert(resinStage.scoutRouteRequiredClass == jf::UnitClass::MarchCaptain);

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(resinStage, choice, false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "wetland_resin") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::CollapsedSidePath), "wetland_resin") == 3); // 2 + 1
        assert(findLoot(lootFor(jf::ExplorationChoice::ScoutRoute), "quality_herb") == 1);

        jf::GameData mutableData = *data;
        mutableData.playerParty[0].classId = jf::UnitClass::MarchCaptain; // ScoutRoute gate
        jf::GameApp app(mutableData);
        assert(startBlackwaterExpedition(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)); // sunken_path
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition(); // -> reedway_fork
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition(); // -> Camp I -> branch, herb_islet first
        assert(app.currentMissionNameJa() == "薬草洲");
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition(); // -> branch again, resin_grove
        assert(app.currentMissionNameJa() == "樹脂林");
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::ScoutRoute));
        int enemyCount = 0;
        for (const jf::Unit& unit : app.battle().battle().units())
            if (unit.team == jf::Team::Enemy) ++enemyCount;
        assert(enemyCount == 3); // base 4 - enemiesRemoved:1
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition(); // both branch members resolved -> Camp II -> blackwater_crossing
        assert(app.currentMissionNameJa() == "黒水渡し");
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
        // Battle Object統合(2026-07): the shipped fallen_log Barrier is now a
        // real attack target, not just a movement-blocking prop.
        const jf::BattleObjectDefinition* logDef = battle.objectDefinition("fallen_log");
        assert(logDef != nullptr && logDef->canBeAttacked && logDef->maxDurability > 0);
        for (const jf::BattleObjectState& object : battle.objects()) {
            if (object.definitionId == "fallen_log") assert(object.durability == logDef->maxDurability);
        }

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
            // docs/implementation_roadmap.md M1-E: enemyZoneWidth=2
            // (data/regions.json) - the boar always spawns in the
            // rightmost 2 columns, not the usual 3.
            assert(boss->position.col >= jf::kGridCols - 2);
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
        // docs/roster_design.md「加入タイミング」: 辺境猟兵の加入候補は森の踏査記録
        // (=AshboughForest地域完了)そのものが条件。
        assert(app.baseState().joinReadyCandidateIds.count("ranger_recruit"));
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
        assert(!app.baseState().joinReadyCandidateIds.count("ranger_recruit"));
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
        // Battle Object統合: BattleController経由でObjectを攻撃対象として
        // 選択・確定できること(resolveObjectAttack()自体は上のテストで検証済み
        // - ここではchooseAttack()/selectTargetTile()/confirmObjectAttack()の
        // 配線を検証する)。
        jf::Unit attacker = makeUnit("attacker", jf::Team::Player, {1, 0});
        attacker.stats.strength = 5;
        attacker.weapon.might = 3; // 8 power, 0 defense -> 8 damage per hit
        // A distant, untouched enemy so the default EliminateTeam objective
        // doesn't trivially auto-resolve to Victory (no enemies at all would
        // otherwise make evaluateOutcome() fire immediately).
        jf::Unit farEnemy = makeUnit("far_enemy", jf::Team::Enemy, {jf::kGridRows - 1, jf::kGridCols - 1});
        // A second, not-yet-acted Player unit so isTeamDone(Player) stays
        // false after the attacker acts - otherwise evaluateOutcome() would
        // auto-advance straight to EnemyTurn, same as any single-actor test.
        jf::Unit reserve = makeUnit("reserve", jf::Team::Player, {2, 0});
        jf::BattleController controller(jf::BattleState({attacker, reserve, farEnemy}));

        jf::BattleObjectDefinition barrier;
        barrier.definitionId = "fallen_tree";
        barrier.kind = jf::BattleObjectKind::Barrier;
        barrier.maxDurability = 10;
        barrier.canBeAttacked = true;
        barrier.blocksMovement = true;
        assert(controller.battle().registerObjectDefinition(barrier));
        assert(controller.battle().placeObject({"tree1", "fallen_tree", {1, 1}}));

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile({1, 0}); // no actual move, just leaves SelectMove
        controller.chooseAttack();
        assert(contains(controller.objectTargetableTiles(), {1, 1}));
        assert(controller.targetableTiles().empty()); // no Unit target exists

        controller.selectTargetTile({1, 1});
        assert(controller.inputState() == jf::BattleInputState::ConfirmObjectAttack);
        assert(controller.pendingObjectTarget() != nullptr);
        assert(controller.pendingObjectTarget()->id == "tree1");

        auto preview = controller.pendingObjectPreview();
        assert(preview.has_value());
        assert(preview->damage == 8);
        assert(preview->durabilityBefore == 10);
        assert(preview->durabilityAfter == 2);

        controller.confirmObjectAttack();
        assert(controller.inputState() == jf::BattleInputState::SelectUnit);
        const jf::BattleObjectState* tree = controller.battle().findObject("tree1");
        assert(tree != nullptr);
        assert(tree->durability == 2);
        assert(tree->state == jf::BattleObjectStateKind::Active);
        // One real state transition (ActionResolved from finishPlayerAction());
        // ObjectDestroyedEvent hasn't fired yet since the tree is still Active.
        assert(controller.battle().missionState().consumedEventIds.size() == 1);
    }

    {
        // Battle Object統合: a lethal hit through the real BattleController
        // flow fires ObjectDestroyedEvent (reaching consumedEventIds) exactly
        // once, and the object becomes passable immediately afterward.
        jf::Unit attacker = makeUnit("attacker", jf::Team::Player, {1, 0});
        attacker.stats.strength = 99;
        jf::Unit farEnemy = makeUnit("far_enemy", jf::Team::Enemy, {jf::kGridRows - 1, jf::kGridCols - 1});
        // Second not-yet-acted Player unit, same reason as the test above -
        // keeps isTeamDone(Player) false so the Phase doesn't also end this
        // Batch (which would add PhaseEnded/PhaseStarted events on top).
        jf::Unit reserve = makeUnit("reserve", jf::Team::Player, {2, 0});
        jf::BattleController controller(jf::BattleState({attacker, reserve, farEnemy}));

        jf::BattleObjectDefinition barrier;
        barrier.definitionId = "fallen_tree";
        barrier.kind = jf::BattleObjectKind::Barrier;
        barrier.maxDurability = 6;
        barrier.canBeAttacked = true;
        barrier.blocksMovement = true;
        assert(controller.battle().registerObjectDefinition(barrier));
        assert(controller.battle().placeObject({"tree1", "fallen_tree", {1, 1}}));

        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile({1, 0});
        controller.chooseAttack();
        controller.selectTargetTile({1, 1});
        controller.confirmObjectAttack();

        const jf::BattleObjectState* tree = controller.battle().findObject("tree1");
        assert(tree != nullptr);
        assert(tree->state == jf::BattleObjectStateKind::Destroyed);
        assert(!controller.battle().objectBlocksMovementAt({1, 1}));
        // ActionResolved + ObjectDestroyed, both reaching consumedEventIds.
        assert(controller.battle().missionState().consumedEventIds.size() == 2);
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

    {
        // 操作 (Interact) 配線: BattleController経由でInteractコマンドを
        // 実行できること(resolveObjectInteraction()自体は上のテストで検証済み
        // - ここではcanInteract()/chooseInteract()/objectInteractableTiles()/
        // selectInteractTarget()の配線を検証する)。allowedClassesを満たさない
        // Unitの場合はそもそも候補Tileに現れないことも確認する(main.cppの
        // canInteract()による条件付きButton表示の前提)。
        jf::Unit wrongClass = makeUnit("wrong_class", jf::Team::Player, {1, 0}, 4, jf::UnitClass::MarchCaptain);
        jf::Unit engineer = makeUnit("engineer", jf::Team::Player, {2, 1}, 4, jf::UnitClass::Spearman);
        jf::Unit farEnemy = makeUnit("far_enemy", jf::Team::Enemy, {jf::kGridRows - 1, jf::kGridCols - 1});
        jf::BattleController controller(jf::BattleState({wrongClass, engineer, farEnemy}));

        jf::BattleObjectDefinition leverDef;
        leverDef.definitionId = "lever";
        leverDef.kind = jf::BattleObjectKind::Device;
        leverDef.interaction = jf::ObjectInteractionDefinition{};
        leverDef.interaction->interactionId = "pull_lever";
        leverDef.interaction->range = 1;
        leverDef.interaction->allowedClasses = {jf::UnitClass::Spearman};
        leverDef.interaction->requiredState = jf::BattleObjectStateKind::Active;
        leverDef.interaction->maxUses = 1;
        leverDef.interactionResultState = jf::BattleObjectStateKind::Opened;
        assert(controller.battle().registerObjectDefinition(leverDef));
        assert(controller.battle().placeObject({"lever1", "lever", {1, 1}}));

        // Wrong class, adjacent: chooseInteract() no-ops, stays in SelectAction.
        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile({1, 0});
        assert(!controller.canInteract());
        controller.chooseInteract();
        assert(controller.inputState() == jf::BattleInputState::SelectAction);
        controller.cancelToUnitSelect();

        // Right class, adjacent: full flow through to a real state change.
        controller.selectUnit(*controller.battle().unitAt({2, 1}));
        controller.selectMoveTile({2, 1});
        assert(controller.canInteract());
        controller.chooseInteract();
        assert(controller.inputState() == jf::BattleInputState::SelectInteractTarget);
        assert(contains(controller.objectInteractableTiles(), {1, 1}));

        // A Tile the lever isn't on is rejected without mutating anything.
        controller.selectInteractTarget({1, 0});
        assert(controller.inputState() == jf::BattleInputState::SelectInteractTarget);

        controller.selectInteractTarget({1, 1});
        assert(controller.inputState() == jf::BattleInputState::SelectUnit);
        const jf::BattleObjectState* lever = controller.battle().findObject("lever1");
        assert(lever != nullptr);
        assert(lever->state == jf::BattleObjectStateKind::Opened);
        assert(lever->interactionCount == 1);
        // ActionResolved + ObjectStateChanged, both reaching consumedEventIds.
        assert(controller.battle().missionState().consumedEventIds.size() == 2);
        assert(controller.battle().unitAt({2, 1})->hasActed);
    }

    {
        // M4 item 4: every generic on-hit status uses the same weapon
        // resolver path. Shallows clears burn before its action-end tick.
        jf::Unit attacker = makeUnit("status_attacker", jf::Team::Player, {1, 0});
        jf::Unit target = makeUnit("status_target", jf::Team::Enemy, {1, 1});
        attacker.weapon.onHitStatuses = {jf::StatusEffectType::Poison, jf::StatusEffectType::Burn,
                                         jf::StatusEffectType::MoveDown, jf::StatusEffectType::DefenseDown,
                                         jf::StatusEffectType::Stagger};
        jf::BattleState battle({attacker, target});
        jf::resolveAttack(battle, battle.units()[0], battle.units()[1], 0, true);
        assert(battle.units()[1].poisonRemainingProcs > 0 && battle.units()[1].burnRemainingProcs > 0);
        assert(battle.units()[1].moveDownActive && battle.units()[1].defenseDownActive &&
              battle.units()[1].staggerActive);
        battle.setTerrain({1, 1}, jf::TerrainType::Shallows);
        battle.units()[1].position = {1, 1};
        jf::processActionEndStatusEffects(battle, battle.units()[1]);
        assert(battle.units()[1].burnRemainingProcs == 0);
    }

    {
        // M4 items 6/7: same state gives the same candidate, and squad
        // reservations steer the next attacker away from overkill.
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 4});
        jf::Unit a = makeUnit("a", jf::Team::Player, {0, 1});
        jf::Unit b = makeUnit("b", jf::Team::Player, {2, 1});
        a.currentHp = b.currentHp = 1;
        jf::BattleState battle({a, b, enemy});
        jf::AiSquadReservations none;
        auto first = jf::chooseBestAiCandidate(
            jf::generateAiCandidates(battle, battle.units()[2], jf::profileFor(battle.units()[2]), none));
        auto repeated = jf::chooseBestAiCandidate(
            jf::generateAiCandidates(battle, battle.units()[2], jf::profileFor(battle.units()[2]), none));
        assert(first.targetUnitId == repeated.targetUnitId && first.destination == repeated.destination);
        jf::AiSquadReservations reserved;
        reserved.reservedDamage[first.targetUnitId] = 999;
        auto next = jf::chooseBestAiCandidate(
            jf::generateAiCandidates(battle, battle.units()[2], jf::profileFor(battle.units()[2]), reserved));
        assert(next.targetUnitId != first.targetUnitId);
        assert(jf::profileFor(makeUnit("wolf", jf::Team::Enemy, {0, 0}, 4,
                                      jf::UnitClass::Wolf)).id == jf::AiProfileId::Wolf);
        assert(jf::profileFor(makeUnit("guard", jf::Team::Enemy, {0, 0}, 4,
                                      jf::UnitClass::VeteranGuard)).id == jf::AiProfileId::Defender);
        assert(jf::profileFor(makeUnit("archer", jf::Team::Enemy, {0, 0}, 4,
                                      jf::UnitClass::WatchArcher)).id == jf::AiProfileId::Ranged);
        assert(jf::profileFor(makeUnit("medic", jf::Team::Enemy, {0, 0}, 4,
                                      jf::UnitClass::DawnChirurgeon)).id == jf::AiProfileId::Support);
        // docs/enemy_ai_rules.md "Faction差分" 野盗: 低HP優先を他Profileより
        // 強め、深追いしない(pursuitLimitを短く)。Loot Container/離脱経路の
        // 評価はObject/Exit認識が無いため保留。
        const jf::AiProfile banditProfile =
            jf::profileFor(makeUnit("bandit", jf::Team::Enemy, {0, 0}, 4, jf::UnitClass::Bandit));
        assert(banditProfile.id == jf::AiProfileId::Bandit);
        assert(banditProfile.lowHpWeight > jf::profileFor(makeUnit("human", jf::Team::Enemy, {0, 0})).lowHpWeight);
        assert(banditProfile.pursuitLimit < jf::profileFor(makeUnit("human", jf::Team::Enemy, {0, 0})).pursuitLimit);
    }

    {
        // docs/enemy_ai_rules.md "撤退と降伏": a Human-profile enemy at or
        // below 25% HP, with no attack available at all (player far out of
        // weapon range) and the exit edge (col kGridCols-1) reachable,
        // retreats instead of merely closing distance. isAlive() stays true
        // (HP untouched) but isPresent() becomes false and exitReason is set.
        jf::Unit farPlayer = makeUnit("farPlayer", jf::Team::Player, {0, 0});
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 5}, 3); // reaches col 7 (distance 2)
        enemy.currentHp = 4; // 20% of 20 maxHp, below the 25% threshold
        jf::BattleState battle({farPlayer, enemy});

        jf::Unit* attacked = jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(attacked == nullptr);
        jf::Unit* retreated = battle.findUnit("enemy");
        assert(retreated->hasExited);
        assert(retreated->exitReason == jf::UnitExitReason::Retreated);
        assert(retreated->isAlive());     // HP untouched
        assert(!retreated->isPresent());  // but no longer a threat/target
        assert(retreated->position.col == jf::kGridCols - 1);
        assert(!battle.unitAt(retreated->position));                    // no longer occupies its tile
        assert(jf::computeTargetableTiles(battle.units(), farPlayer, farPlayer.position).empty());
    }

    {
        // Same low-HP scenario, but the exit edge is unreachable this turn
        // (MOV too low) - no Retreat candidate exists at all, so the enemy
        // falls back to ordinary move-toward-target behavior instead.
        jf::Unit farPlayer = makeUnit("farPlayer", jf::Team::Player, {1, 0});
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 5}, 1); // MOV 1: col 7 unreachable
        enemy.currentHp = 4;
        jf::BattleState battle({farPlayer, enemy});

        jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(!battle.findUnit("enemy")->hasExited);
    }

    {
        // Retreat never overrides a guaranteed kill this action - a lethal
        // Attack candidate's defeatBonus always outscores a Retreat
        // candidate, even for a critically wounded enemy with an open exit.
        jf::Unit victim = makeUnit("victim", jf::Team::Player, {1, 4});
        victim.currentHp = 1;
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 5}, 3);
        enemy.currentHp = 4;
        jf::BattleState battle({victim, enemy});

        jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(!battle.findUnit("enemy")->hasExited);
        assert(!battle.findUnit("victim")->isAlive()); // finished off instead of retreating
    }

    {
        // docs/enemy_ai_rules.md: a retreated enemy still counts as
        // "eliminated" for EliminateTeam (it's no longer a threat), and
        // isTeamDone()/the Enemy Phase loop must not get stuck on it once a
        // later beginEnemyPhase() resets hasActed - regression for a bug
        // found while implementing this: nextUnactedEnemy()/isTeamDone()
        // originally checked isAlive() instead of isPresent(), so a
        // retreated (still-alive) unit would be treated as "hasn't acted
        // yet" forever from the next Enemy Phase onward, since nothing ever
        // marks it acted again.
        jf::Unit player = makeUnit("player", jf::Team::Player, {0, 0});
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 5}, 3);
        enemy.currentHp = 4;
        jf::BattleState battle({player, enemy});

        jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(battle.findUnit("enemy")->hasExited);
        assert(battle.isTeamDone(jf::Team::Enemy));
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);

        battle.beginEnemyPhase(); // resets hasActed for the whole Enemy team
        assert(!battle.findUnit("enemy")->hasActed);       // reset, as normal
        assert(battle.isTeamDone(jf::Team::Enemy));         // still done: isPresent() skips it
    }

    {
        // M4 item 5: mandatory scheduled waves stop early victory, announce
        // once, spawn acted, and become normal enemies on the next phase.
        jf::Unit player = makeUnit("player", jf::Team::Player, {1, 0});
        jf::BattleState battle({player});
        jf::ReinforcementWave wave;
        wave.id = "wave_1";
        wave.spawnRound = 2;
        wave.spawnPhase = jf::Phase::EnemyPhase;
        wave.units = {{makeUnit("reinforcement", jf::Team::Enemy, {0, 0})}};
        wave.orderedSpawnCandidates = {{1, 7}, {0, 7}};
        assert(battle.addReinforcementWave(wave));
        assert(battle.hasPendingRequiredEnemyReinforcements());
        assert(!battle.allEnemiesDefeated());
        assert(battle.reinforcementWaves()[0].state == jf::ReinforcementState::Announced);
        battle.beginEnemyPhase();
        assert(battle.reinforcementWaves()[0].state == jf::ReinforcementState::Announced);
        battle.beginPlayerPhase();
        battle.beginEnemyPhase();
        assert(battle.reinforcementWaves()[0].state == jf::ReinforcementState::Spawned);
        const jf::Unit* spawned = battle.findUnit("reinforcement");
        assert(spawned && spawned->position == (jf::GridPos{1, 7}) && spawned->hasActed);
        assert(!battle.hasPendingRequiredEnemyReinforcements());
    }

    {
        // Every candidate blocked means Prevented, never delayed or moved to
        // an undeclared entrance; the mandatory-wave victory lock releases.
        jf::Unit player = makeUnit("player", jf::Team::Player, {1, 0});
        jf::Unit blocker = makeUnit("blocker", jf::Team::Player, {1, 7});
        jf::BattleState battle({player, blocker});
        jf::ReinforcementWave wave;
        wave.id = "blocked_wave";
        wave.spawnRound = 2;
        wave.spawnPhase = jf::Phase::EnemyPhase;
        wave.units = {{makeUnit("never_spawned", jf::Team::Enemy, {0, 0})}};
        wave.orderedSpawnCandidates = {{1, 7}};
        assert(battle.addReinforcementWave(wave));
        battle.beginEnemyPhase();
        battle.beginPlayerPhase();
        battle.beginEnemyPhase();
        assert(battle.reinforcementWaves()[0].state == jf::ReinforcementState::Prevented);
        assert(!battle.findUnit("never_spawned"));
        assert(!battle.hasPendingRequiredEnemyReinforcements());
    }

    {
        // The first live content connection: Herbwater Hollow's harvest
        // choice deterministically installs the round-2 wolf wave, while
        // the safe pass route does not.
        jf::GameData data = makeFactoryData();
        jf::RegionDescriptor region = jf::regionDescriptor(jf::RegionId::AshboughForest, data);
        auto stage = std::find_if(region.stages.begin(), region.stages.end(), [](const jf::StageDescriptor& value) {
            return value.id == "herbwater_hollow";
        });
        assert(stage != region.stages.end());
        jf::ExplorationOutcome harvest = jf::stageRouteOutcome(*stage, jf::ExplorationChoice::CollapsedSidePath);
        jf::BattleState withWave = jf::createScenarioBattle(data, *stage, 42, harvest);
        assert(withWave.reinforcementWaves().size() == 1);
        assert(withWave.reinforcementWaves()[0].id == "herbwater_harvest_wolf");
        // The Announced-transition banner (main.cpp's reinforcementUiStates())
        // reports this spawnRound, so its data source must actually carry it.
        assert(withWave.reinforcementWaves()[0].spawnRound == 2);
        jf::BattleState withoutWave = jf::createScenarioBattle(
            data, *stage, 42, jf::stageRouteOutcome(*stage, jf::ExplorationChoice::FrontalAdvance));
        assert(withoutWave.reinforcementWaves().empty());
    }

    {
        // docs/inventory_overflow.md「上限」/「帰還処理」: a material's
        // overflow past the 999 cap goes to RewardOverflowState rather than
        // being silently dropped, and the in-cap portion still lands in
        // storage normally.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(startCinderwatchExpedition(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        assert(app.screen() == jf::Screen::Camp);

        jf::BaseState& base = const_cast<jf::BaseState&>(app.baseState());
        base.storage.push_back({"wood", 998});
        app.expedition().pendingLoot = {{"wood", 5}};
        assert(app.returnToBase());
        assert(app.baseState().storageCount("wood") == 999);
        assert(app.rewardOverflow().stacks.size() == 1);
        assert(app.rewardOverflow().stacks[0].itemId == "wood");
        assert(app.rewardOverflow().stacks[0].quantity == 4);
    }

    {
        // Unused bag items must not disappear when an imported/older save has
        // already filled that item's storage to the current 99-item cap.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(startCinderwatchExpedition(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();

        jf::BaseState& base = const_cast<jf::BaseState&>(app.baseState());
        base.itemStorage[jf::ItemType::FirstAidKit] = jf::BaseState::kItemStorageCap;
        app.expedition().bag.push_back(jf::ItemType::FirstAidKit);
        assert(app.returnToBase());
        assert(app.baseState().ownedItemCount(jf::ItemType::FirstAidKit) == jf::BaseState::kItemStorageCap);
        assert(app.rewardOverflow().stacks.size() == 1);
        assert(app.rewardOverflow().stacks[0].itemId ==
               "item:" + std::to_string(static_cast<int>(jf::ItemType::FirstAidKit)));
        assert(app.rewardOverflow().stacks[0].quantity == 1);
    }

    {
        // Crafting at the 99-item cap is rejected before any ingredients are
        // consumed, preserving the all-or-nothing crafting contract.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        jf::BaseState& base = const_cast<jf::BaseState&>(app.baseState());
        base.itemStorage[jf::ItemType::FieldTreatmentKit] = jf::BaseState::kItemStorageCap;
        for (const jf::ItemCraftCost& line : jf::itemCraftCost(jf::ItemType::FieldTreatmentKit))
            base.addStorage(line.materialId, line.quantity);
        const auto cost = jf::itemCraftCost(jf::ItemType::FieldTreatmentKit);
        assert(!app.craftItem(jf::ItemType::FieldTreatmentKit));
        assert(app.baseState().ownedItemCount(jf::ItemType::FieldTreatmentKit) == jf::BaseState::kItemStorageCap);
        for (const jf::ItemCraftCost& line : cost)
            assert(app.baseState().storageCount(line.materialId) == line.quantity);
    }

    {
        // Region key materials never enter the overflow pool - excess beyond
        // the 1-cap is silently deduplicated (docs/inventory_overflow.md
        // 「保留中のキー素材...は存在させない。これらは重複除去して直接恒久化する」).
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(startCinderwatchExpedition(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        app.expedition().pendingLoot = {{jf::kAshveilFangMaterial, 3}};
        assert(app.returnToBase());
        assert(app.baseState().storageCount(jf::kAshveilFangMaterial) == 1);
        assert(app.rewardOverflow().stacks.empty());
    }

    {
        // 200-Stack ceiling: a return that would push the overflow pool past
        // the cap must not commit anything (storage, overflow, pending all
        // unchanged), and returnToBase() reports failure so the caller can
        // route to the warehouse cleanup screen.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(startCinderwatchExpedition(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();

        jf::BaseState& base = const_cast<jf::BaseState&>(app.baseState());
        for (int i = 0; i < 200; ++i) {
            base.rewardOverflow.stacks.push_back({"prior-grant", "material_" + std::to_string(i), 1});
        }
        base.storage.push_back({"wood", 998});
        app.expedition().pendingLoot = {{"wood", 5}};
        const std::size_t stacksBefore = app.rewardOverflow().stacks.size();
        const int woodBefore = app.baseState().storageCount("wood");
        assert(!app.returnToBase());
        assert(app.screen() == jf::Screen::Camp); // unchanged - no resetToBase() happened
        assert(app.rewardOverflow().stacks.size() == stacksBefore);
        assert(app.baseState().storageCount("wood") == woodBefore);
        assert(!app.expedition().pendingLoot.empty()); // Pending preserved, not discarded
    }

    {
        // Discard: Cancel (never invoked) leaves quantities untouched; a
        // successful discard removes exactly the requested amount, and
        // discarding a key material is refused outright.
        jf::BaseState base;
        base.addStorage("hide", 5);
        assert(!base.consumeStorage("nonexistent", 1));
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        jf::BaseState& liveBase = const_cast<jf::BaseState&>(app.baseState());
        liveBase.addStorage("hide", 5);
        liveBase.addStorage(jf::kAshveilFangMaterial, 1);
        assert(!app.discardStorage(jf::kAshveilFangMaterial, 1)); // key material: refused
        assert(app.baseState().storageCount(jf::kAshveilFangMaterial) == 1);
        assert(!app.discardStorage("hide", 99)); // more than owned: refused, nothing changes
        assert(app.baseState().storageCount("hide") == 5);
        assert(app.discardStorage("hide", 2));
        assert(app.baseState().storageCount("hide") == 3);

        liveBase.rewardOverflow.stacks.push_back({"g1", "wood", 4});
        assert(!app.discardOverflowStack(0, 10)); // more than the stack holds
        assert(app.rewardOverflow().stacks.size() == 1);
        assert(app.discardOverflowStack(0, 4));
        assert(app.rewardOverflow().stacks.empty());
    }

    {
        // Same-screen re-invocation of returnToBase() (e.g. a double click on
        // "Return to Base" before the screen changes) must not double-grant -
        // the existing `screen_ != Screen::Camp` guard (same pattern that
        // fixed a prior proceedToCamp() double-grant bug) makes the second
        // call a no-op.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(startCinderwatchExpedition(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        app.expedition().pendingLoot = {{"wood", 3}};
        assert(app.returnToBase());
        assert(app.screen() == jf::Screen::Base);
        assert(app.baseState().storageCount("wood") == 3);
        assert(!app.returnToBase()); // screen_ is now Base, not Camp - guarded no-op
        assert(app.baseState().storageCount("wood") == 3); // unchanged, not doubled
    }

    {
        // docs/save_system.md「Schema移行」: a v1-shaped save (missing every
        // v2-only field: itemStorage, rewardOverflow, unit-level equipment
        // maps) must migrate to schemaVersion 2 with safe defaults, not fail
        // to parse or silently stay at version 1.
        const std::string v1Json = R"({
            "schemaVersion": 1,
            "gameVersion": "0.0.1",
            "base": {
                "storage": [{"id": "wood", "quantity": 3}],
                "discoveries": [],
                "outpostStage": 0,
                "unlockedNodes": [],
                "builtNodes": [],
                "siteAccess": {},
                "completedRegions": []
            },
            "selectedPartyIds": [],
            "settings": {"language": "en"},
            "expedition": null
        })";
        std::string parseError;
        auto parsed = jf::deserializeSave(v1Json, &parseError);
        assert(parsed);
        assert(parsed->schemaVersion == 1);
        assert(parsed->base.itemStorage.empty());          // v2-only field: safe default
        assert(parsed->base.rewardOverflow.stacks.empty()); // v2-only field: safe default
        assert(parsed->base.storageCount("wood") == 3);     // pre-existing v1 data preserved

        jf::SaveData migrated = jf::migrateSave(*parsed);
        assert(migrated.schemaVersion == jf::kCurrentSaveSchemaVersion);
        assert(migrated.base.storageCount("wood") == 3);
        assert(migrated.base.itemStorage.empty());
    }

    {
        // SaveStore::load() applies the migration+backup automatically: an
        // on-disk v1 file gets backed up to ".schema-v1.bak" and the loaded
        // SaveData comes back already at the current schema version.
        const std::filesystem::path tempPath =
            std::filesystem::temp_directory_path() / "jf_migration_test_save.json";
        std::filesystem::remove(tempPath);
        std::filesystem::remove(std::filesystem::path(tempPath.string() + ".schema-v1.bak"));
        {
            std::ofstream file(tempPath, std::ios::trunc);
            file << R"({
                "schemaVersion": 1,
                "gameVersion": "0.0.1",
                "base": {"storage": [], "discoveries": [], "outpostStage": 0, "unlockedNodes": [],
                         "builtNodes": [], "siteAccess": {}, "completedRegions": []},
                "selectedPartyIds": [],
                "settings": {"language": "en"},
                "expedition": null
            })";
        }
        jf::SaveStore store(tempPath.string());
        auto loaded = store.load();
        assert(loaded);
        assert(loaded->schemaVersion == jf::kCurrentSaveSchemaVersion);
        assert(std::filesystem::exists(tempPath.string() + ".schema-v1.bak"));
        std::filesystem::remove(tempPath);
        std::filesystem::remove(std::filesystem::path(tempPath.string() + ".schema-v1.bak"));
    }

    {
        // docs/save_system.md「破損復旧画面」"Restore Backup": when the
        // primary is corrupt but a ".schema-vN.bak" written by an earlier
        // migration is still valid, restoreFromBackup() recovers it and
        // writes it back as the new primary.
        const std::filesystem::path tempPath =
            std::filesystem::temp_directory_path() / "jf_restore_test_save.json";
        const std::filesystem::path schemaBak(tempPath.string() + ".schema-v1.bak");
        std::filesystem::remove(tempPath);
        std::filesystem::remove(schemaBak);
        std::filesystem::remove(std::filesystem::path(tempPath.string() + ".bak"));

        {
            std::ofstream corrupt(tempPath, std::ios::trunc);
            corrupt << "{ not valid json";
        }
        {
            std::ofstream backup(schemaBak, std::ios::trunc);
            backup << R"({
                "schemaVersion": 1,
                "gameVersion": "0.0.1",
                "base": {"storage": [{"id": "hide", "quantity": 7}], "discoveries": [], "outpostStage": 0,
                         "unlockedNodes": [], "builtNodes": [], "siteAccess": {}, "completedRegions": []},
                "selectedPartyIds": [],
                "settings": {"language": "en"},
                "expedition": null
            })";
        }
        jf::SaveStore store(tempPath.string());
        assert(!store.load()); // primary corrupt, no ".bak" - nothing load() itself can use
        assert(store.restoreFromBackup());
        auto recovered = store.load();
        assert(recovered);
        assert(recovered->schemaVersion == jf::kCurrentSaveSchemaVersion);
        assert(recovered->base.storageCount("hide") == 7);

        std::filesystem::remove(tempPath);
        std::filesystem::remove(schemaBak);
        std::filesystem::remove(std::filesystem::path(tempPath.string() + ".bak"));
    }

    {
        // docs/save_system.md「破損復旧画面」"Start New": quarantining moves
        // the corrupt primary aside (never deletes it) so a fresh save can
        // be written without colliding, and is a no-op when nothing exists.
        const std::filesystem::path tempPath =
            std::filesystem::temp_directory_path() / "jf_quarantine_test_save.json";
        std::filesystem::remove(tempPath);
        jf::SaveStore store(tempPath.string());
        assert(store.quarantineCorruptSave()); // no file yet: no-op success

        {
            std::ofstream corrupt(tempPath, std::ios::trunc);
            corrupt << "{ not valid json";
        }
        assert(store.quarantineCorruptSave());
        assert(!std::filesystem::exists(tempPath)); // moved aside, not left in place
        bool foundQuarantined = false;
        for (const auto& entry : std::filesystem::directory_iterator(tempPath.parent_path())) {
            const std::string name = entry.path().filename().string();
            if (name.rfind(tempPath.filename().string() + ".corrupt-", 0) == 0) {
                foundQuarantined = true;
                std::filesystem::remove(entry.path());
            }
        }
        assert(foundQuarantined);
    }

    {
        // docs/roster_design.md「加入処理の共通ルール」: 灰角大猪撃破
        // (brokenwood_territory勝利)でheavy_recruitが加入候補になり、安全帰還で
        // 加入可能候補として恒久化される。敗北すると保留分は失う。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        jf::GameApp app(*data);
        reachBrokenwoodTerritory(app);
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        assert(app.screen() == jf::Screen::Camp);
        assert(app.returnToBase());
        assert(app.screen() == jf::Screen::Base);
        assert(app.baseState().joinReadyCandidateIds.count("heavy_recruit"));
        assert(!app.baseState().joinedRecruitIds.count("heavy_recruit"));
    }

    {
        // 別遠征の敗北でも、既に安全帰還済みの加入可能候補は失わない。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        jf::GameApp app(*data);
        reachBrokenwoodTerritory(app);
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        assert(app.returnToBase());
        assert(app.baseState().joinReadyCandidateIds.count("heavy_recruit"));

        // 次の遠征を始めて何も達成せず敗北しても、既存の加入可能候補は残る。
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
        assert(app.screen() == jf::Screen::Base);
        assert(app.baseState().joinReadyCandidateIds.count("heavy_recruit")); // untouched by this defeat
    }

    {
        // GameApp::confirmRecruitJoin(): rosterへ追加し、Tier1スキルを第1枠へ
        // 装備する。二重呼び出しは何も増やさない(idempotent)。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        jf::GameApp app(*data);
        reachBrokenwoodTerritory(app);
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        assert(app.returnToBase());

        const std::size_t rosterSizeBefore = app.roster().size();
        assert(app.confirmRecruitJoin("heavy_recruit"));
        assert(app.roster().size() == rosterSizeBefore + 1);
        auto joined = std::find_if(app.roster().begin(), app.roster().end(),
                                   [](const jf::UnitTemplate& u) { return u.id == "heavy_recruit"; });
        assert(joined != app.roster().end() && joined->classId == jf::UnitClass::HeavyInfantry);
        assert(app.baseState().joinedRecruitIds.count("heavy_recruit"));
        auto skillIt = app.equippedSkills().find("heavy_recruit");
        assert(skillIt != app.equippedSkills().end());
        const jf::SkillDefinition* tier1 = jf::findSkill(skillIt->second.equippedSkillIds[0]);
        assert(tier1 && tier1->unitClass == jf::UnitClass::HeavyInfantry && tier1->unlockTier == 1);
        assert(skillIt->second.equippedSkillIds[1].empty()); // slot 2 stays empty

        // 二重呼び出し: 何も増えない。
        assert(!app.confirmRecruitJoin("heavy_recruit"));
        assert(app.roster().size() == rosterSizeBefore + 1);
    }

    {
        // recruitCapacity(): 共同テント6人 → 灰枝の森安全帰還後8人。枠不足では
        // confirmRecruitJoinが失敗し、候補は残り続ける。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        jf::GameApp app(*data);
        assert(app.recruitCapacity() == 6);
        assert(app.roster().size() == 6); // 初期6人でちょうど満員

        reachBrokenwoodTerritory(app);
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        assert(app.returnToBase());
        // 灰枝の森は折れ木の縄張りの勝利でも地域完了する(全サイトSurveyed+)。
        assert(app.baseState().completedRegionIds.count(jf::RegionId::AshboughForest));
        assert(app.recruitCapacity() == 8);
        assert(app.confirmRecruitJoin("heavy_recruit")); // 枠8、現在6人 -> 成功
    }

    {
        // Save/Load往復でjoinReadyCandidateIds/joinedRecruitIds/加入済みUnitが
        // 復元される。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        jf::GameApp app(*data);
        reachBrokenwoodTerritory(app);
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app);
        app.proceedToCamp();
        assert(app.returnToBase());
        assert(app.confirmRecruitJoin("heavy_recruit"));

        jf::SaveData saved = app.createSaveData("en");
        assert(saved.base.joinReadyCandidateIds.count("heavy_recruit"));
        assert(saved.base.joinedRecruitIds.count("heavy_recruit"));

        std::string json = jf::serializeSave(saved);
        auto reloaded = jf::deserializeSave(json);
        assert(reloaded);
        assert(reloaded->base.joinReadyCandidateIds.count("heavy_recruit"));
        assert(reloaded->base.joinedRecruitIds.count("heavy_recruit"));

        jf::GameApp freshApp(*data); // fresh instance, roster_ has no recruit yet
        assert(std::none_of(freshApp.roster().begin(), freshApp.roster().end(),
                            [](const jf::UnitTemplate& u) { return u.id == "heavy_recruit"; }));
        assert(freshApp.applySaveData(*reloaded));
        auto restored = std::find_if(freshApp.roster().begin(), freshApp.roster().end(),
                                     [](const jf::UnitTemplate& u) { return u.id == "heavy_recruit"; });
        assert(restored != freshApp.roster().end() && restored->classId == jf::UnitClass::HeavyInfantry);
        assert(freshApp.equippedSkills().count("heavy_recruit")); // Tier1スキルも復元
    }

    {
        // docs/regions/blackwater_lowlands.md「5. 黒水渡し」: a guest reaching
        // the escape tile and ending an action there completes the primary
        // EscapeUnits objective standalone (it replaced the default
        // EliminateTeam member entirely - see
        // StageDescriptor::primaryEscapeUnitsAlternative) - Victory even
        // though every enemy is still alive.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor blackwaterRegion = jf::regionDescriptor(jf::RegionId::BlackwaterLowlands, *data);
        const jf::StageDescriptor* crossingStage = nullptr;
        for (const jf::StageDescriptor& stage : blackwaterRegion.stages)
            if (stage.id == "blackwater_crossing") crossingStage = &stage;
        assert(crossingStage && crossingStage->guestUnits.size() == 2);
        assert(crossingStage->enemyRoster.size() == 5);

        jf::BattleState battle = jf::createScenarioBattle(*data, *crossingStage, /*seed=*/7);
        assert(battle.missionState().guestUnitIds.size() == 2);
        for (const jf::Unit& unit : battle.units())
            if (unit.isGuest) assert(unit.team == jf::Team::Player);

        const jf::ObjectiveDefinition* escapeDef = nullptr;
        for (const auto& def : battle.missionState().definitions)
            if (def.id == "blackwater_crossing_escape") escapeDef = &def;
        assert(escapeDef && escapeDef->primary && escapeDef->kind == jf::ObjectiveKind::EscapeUnits);

        const std::string& guestId = battle.missionState().guestUnitIds[0];
        jf::BattleEvent guestEscapes{
            1, 1,
            jf::ActionResolvedEvent{1, guestId, jf::Team::Player, jf::ActionKind::Wait, escapeDef->target.tile}};
        jf::handleObjectiveEvent(battle.missionState(), guestEscapes);
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);
    }

    {
        // Both guests reduced to 0 HP (isPresent() false): allGuestsLost()
        // gates Defeat independently of the player squad still being fully
        // alive - the "荷運び役2人の撤退" loss condition.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor blackwaterRegion = jf::regionDescriptor(jf::RegionId::BlackwaterLowlands, *data);
        const jf::StageDescriptor* crossingStage = nullptr;
        for (const jf::StageDescriptor& stage : blackwaterRegion.stages)
            if (stage.id == "blackwater_crossing") crossingStage = &stage;
        assert(crossingStage);

        jf::BattleState battle = jf::createScenarioBattle(*data, *crossingStage, /*seed=*/7);
        assert(!battle.allGuestsLost()); // both still present at battle start
        for (jf::Unit& unit : battle.units())
            if (unit.isGuest) unit.currentHp = 0;
        assert(battle.allGuestsLost());
        // Party squad is untouched (still fully alive) - Defeat must still
        // fire purely from the guests, not allPlayersDefeated().
        assert(!battle.allPlayersDefeated());
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Defeat);
    }

    {
        // docs/regions/mapped_edge.md 地点6「石盆地」: 護衛2人中1人以上が
        // 脱出タイルに達すればVictory - blackwater_crossing(M9-I)と全く
        // 同じprimaryEscapeUnitsAlternative+guestUnitsの直接再利用の検証。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor mappedEdge = jf::regionDescriptor(jf::RegionId::MappedEdge, *data);
        const jf::StageDescriptor* stoneBasinStage = nullptr;
        for (const jf::StageDescriptor& stage : mappedEdge.stages)
            if (stage.id == "mapped_edge_stone_basin") stoneBasinStage = &stage;
        assert(stoneBasinStage && stoneBasinStage->guestUnits.size() == 2);
        assert(stoneBasinStage->enemyRoster.size() == 5); // 大型獣1 + 野生獣4

        jf::BattleState battle2 = jf::createScenarioBattle(*data, *stoneBasinStage, /*seed=*/7);
        assert(battle2.missionState().guestUnitIds.size() == 2);
        for (const jf::Unit& unit : battle2.units())
            if (unit.isGuest) assert(unit.team == jf::Team::Player);

        const jf::ObjectiveDefinition* escapeDef2 = nullptr;
        for (const auto& def : battle2.missionState().definitions)
            if (def.id == "mapped_edge_stone_basin_escape") escapeDef2 = &def;
        assert(escapeDef2 && escapeDef2->primary && escapeDef2->kind == jf::ObjectiveKind::EscapeUnits);

        const std::string& guestId2 = battle2.missionState().guestUnitIds[0];
        jf::BattleEvent guestEscapes2{
            1, 1,
            jf::ActionResolvedEvent{1, guestId2, jf::Team::Player, jf::ActionKind::Wait, escapeDef2->target.tile}};
        jf::handleObjectiveEvent(battle2.missionState(), guestEscapes2);
        jf::syncObjectiveProgress(battle2);
        assert(jf::evaluateBattleOutcome(battle2).kind == jf::BattleOutcomeKind::Victory);
    }

    {
        // 地点6「石盆地」敗北条件「全護衛撤退」: allGuestsLost()がプレイヤー
        // 部隊の生死とは独立にDefeatを判定する(blackwater_crossingと同型)。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor mappedEdge = jf::regionDescriptor(jf::RegionId::MappedEdge, *data);
        const jf::StageDescriptor* stoneBasinStage = nullptr;
        for (const jf::StageDescriptor& stage : mappedEdge.stages)
            if (stage.id == "mapped_edge_stone_basin") stoneBasinStage = &stage;
        assert(stoneBasinStage);

        jf::BattleState battle2 = jf::createScenarioBattle(*data, *stoneBasinStage, /*seed=*/7);
        assert(!battle2.allGuestsLost());
        for (jf::Unit& unit : battle2.units())
            if (unit.isGuest) unit.currentHp = 0;
        assert(battle2.allGuestsLost());
        assert(!battle2.allPlayersDefeated());
        assert(jf::evaluateBattleOutcome(battle2).kind == jf::BattleOutcomeKind::Defeat);
    }

    {
        // docs/regions/mapped_edge.md「9地点仕様」地点7「折れた見張台」: 主目的
        // 「観測盤+記録箱」はsunken_sluice(M9-J)/ravine_cooling_channel(M9-AC)/
        // heatwork_shop/fort_broken_gate以来のOperateObject-primary近似
        // (Kindの異なるOperateObject+crateのAND合成は本プロジェクトに存在しない
        // 既知ギャップのため、観測盤operateのみを主目的とし、記録箱は
        // surveyObjectiveId(surveyTileCount:2)経由のsecondary/bonusパスへ
        // 回す)。敵「追跡者6」はmapped_edge_unrecorded_camp(M9-AV)と同型の
        // Pursuer/Bandit x6再利用。`[監視弓兵]`「高所確保」は
        // scoutRouteRequiredClass=WatchArcher+enemiesRemoved=1
        // (mappedEdgeStoneBasinStage()の「落石受止め」と同型の近似)。
        // 「記録優先」「観測優先」はwindscarRelayStage()以来のフレーバーのみ
        // ペア(数値差分なし)。敗北条件「両方破壊」はObject耐久ギャップとして
        // 見送り(部隊全滅は既存Engineで常時有効)。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor mappedEdge = jf::regionDescriptor(jf::RegionId::MappedEdge, *data);
        const jf::StageDescriptor* watchtowerStage = nullptr;
        for (const jf::StageDescriptor& stage : mappedEdge.stages)
            if (stage.id == "mapped_edge_broken_watchtower") watchtowerStage = &stage;
        assert(watchtowerStage);
        assert(watchtowerStage->enemyRoster.size() == 6); // 追跡者6
        assert(watchtowerStage->scoutRouteRequiredClass == jf::UnitClass::WatchArcher);
        assert(watchtowerStage->surveyObjectiveId == "mapped_edge_broken_watchtower_crate");
        assert(watchtowerStage->surveyTileCount == 2);
        assert(watchtowerStage->objectPlacementRules.size() == 1);
        assert(watchtowerStage->objectPlacementRules[0].operateObjectiveId ==
              "operate_mapped_edge_broken_watchtower_panel");

        const jf::ExplorationOutcome recordsFirst =
            jf::stageRouteOutcome(*watchtowerStage, jf::ExplorationChoice::FrontalAdvance);
        assert(recordsFirst.enemiesRemoved == 0); // 敵6体
        const jf::ExplorationOutcome observeFirst =
            jf::stageRouteOutcome(*watchtowerStage, jf::ExplorationChoice::CollapsedSidePath);
        assert(observeFirst.enemiesRemoved == 0); // フレーバーのみ、敵6体
        const jf::ExplorationOutcome highGround =
            jf::stageRouteOutcome(*watchtowerStage, jf::ExplorationChoice::ScoutRoute);
        assert(highGround.enemiesRemoved == 1); // 高所確保で1体足止め、敵5体

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(*watchtowerStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "ruin_fragment") == 2);

        // (a) 観測盤を操作するだけでVictory(記録箱は未回収でもよい - 真の
        // AND合成ではなくOperateObject-primary近似であることの直接検証)。
        jf::BattleState battle = jf::createScenarioBattle(*data, *watchtowerStage, /*seed=*/37);
        const jf::ObjectiveDefinition* operateDef = nullptr;
        std::vector<const jf::ObjectiveDefinition*> crateDefs;
        for (const auto& def : battle.missionState().definitions) {
            if (def.kind == jf::ObjectiveKind::OperateObject) operateDef = &def;
            if (def.groupId == "mapped_edge_broken_watchtower_crate") crateDefs.push_back(&def);
        }
        assert(operateDef && operateDef->primary && operateDef->groupId == "primary");
        assert(crateDefs.size() == 2);
        for (const jf::ObjectiveDefinition* def : crateDefs) assert(!def->primary);
        bool hasCrateGroup = false;
        for (const auto& group : battle.missionState().groups)
            if (group.id == "mapped_edge_broken_watchtower_crate" && group.rule == jf::ObjectiveGroupRule::Any)
                hasCrateGroup = true;
        assert(hasCrateGroup);

        jf::BattleObjectState* panel = battle.findObject("mapped_edge_broken_watchtower_panel_1");
        assert(panel != nullptr);
        panel->interactionCount = 1; // 観測盤を操作、記録箱は未回収
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);
    }

    {
        // docs/regions/mapped_edge.md「9地点仕様」地点8「帰還基点」: 主目的
        // 「基点Objectを4Round防衛」はfort_reserve_wall(shattered_march_fort)
        // 同型のprimarySurviveRoundsAlternative(SurviveRoundsMissionRule)の
        // 直接再利用(Object耐久タイのある「基点0」敗北条件はObject耐久機構
        // 未実装の既知ギャップとして見送り、部隊全滅は既存Engineで常時有効)。
        // 敵「2波計7」はfort_reserve_wallと同型に4初期(Pursuer2+WatchArcher2)+
        // 3増援1波(timedReinforcement、2ラウンド目、1ラウンド前予告)へ分割。
        // 探索3択「避難所設置/物資庫設置」はwindscarRelayStage()以来のフレー
        // バーのみペア(数値差分なし)、`[旗手]`「集合地点統一」は
        // scoutRouteRequiredClass=BannerBearer+enemiesRemoved=1(mappedEdge
        // StoneBasinStage()の「落石受止め」と同型の近似)。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor mappedEdge = jf::regionDescriptor(jf::RegionId::MappedEdge, *data);
        const jf::StageDescriptor* returnBaseStage = nullptr;
        for (const jf::StageDescriptor& stage : mappedEdge.stages)
            if (stage.id == "mapped_edge_return_base") returnBaseStage = &stage;
        assert(returnBaseStage);
        assert(returnBaseStage->enemyRoster.size() == 4); // 2波計7のうち初期4体
        assert(returnBaseStage->scoutRouteRequiredClass == jf::UnitClass::BannerBearer);
        assert(returnBaseStage->primarySurviveRoundsAlternative &&
              returnBaseStage->primarySurviveRoundsAlternative->id == "mapped_edge_return_base_defense" &&
              returnBaseStage->primarySurviveRoundsAlternative->surviveUntilRound == 4);
        assert(returnBaseStage->timedReinforcement && returnBaseStage->timedReinforcement->spawnRound == 2 &&
              returnBaseStage->timedReinforcement->units.size() == 3); // 増援3体、初期4体と合わせ計7体

        const jf::ExplorationOutcome shelterFirst =
            jf::stageRouteOutcome(*returnBaseStage, jf::ExplorationChoice::FrontalAdvance);
        assert(shelterFirst.enemiesRemoved == 0); // フレーバーのみ
        const jf::ExplorationOutcome depotFirst =
            jf::stageRouteOutcome(*returnBaseStage, jf::ExplorationChoice::CollapsedSidePath);
        assert(depotFirst.enemiesRemoved == 0); // フレーバーのみ
        const jf::ExplorationOutcome rallyUnified =
            jf::stageRouteOutcome(*returnBaseStage, jf::ExplorationChoice::ScoutRoute);
        assert(rallyUnified.enemiesRemoved == 1); // 集合地点統一で1体足止め

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(*returnBaseStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "food") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "herb") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "quality_iron") == 1);

        // 主目的: 4ラウンド終了まで基点を守る(SurviveRoundsとEliminateTeamの
        // OR)。敵を1体も倒さなくても4ラウンド生存でVictory。増援(2ラウンド目)
        // が到着してもなお生存し続けられることも合わせて確認する。
        jf::BattleState defense = jf::createScenarioBattle(*data, *returnBaseStage, /*seed=*/13, shelterFirst);
        jf::syncObjectiveProgress(defense);
        assert(jf::evaluateBattleOutcome(defense).kind != jf::BattleOutcomeKind::Victory);
        while (defense.round() <= 4) {
            defense.beginEnemyPhase();
            defense.beginPlayerPhase();
        }
        jf::syncObjectiveProgress(defense);
        assert(jf::evaluateBattleOutcome(defense).kind == jf::BattleOutcomeKind::Victory);

        int enemyCountAfterReinforcement = 0;
        for (const jf::Unit& unit : defense.units())
            if (unit.team == jf::Team::Enemy) ++enemyCountAfterReinforcement;
        assert(enemyCountAfterReinforcement == 7); // 増援3体到着済み、計7体
    }

    {
        // docs/regions/mapped_edge.md「最終戦「地図外縁」」(M9-BC、本編最終地点):
        // 主目的「標識3個設置後、4人中1人以上を帰還基点へ脱出」は3件の
        // objectPlacementRules(operateObjectiveId、真の3-way AND、windwatch_
        // station/fort_signal_yard/mapped_edge_abandoned_relay/mapped_edge_
        // split_survey_routeと同型)+primaryEscapeUnitsAlternative(mapped_
        // edge_stone_basin/blackwater_crossing以来)を同じ"primary"グループ
        // (デフォルトのObjectiveGroupRule::All)へ合成した近似 - 敵全滅のみ、
        // 標識1〜2個だけの操作、脱出のみ、のいずれでもVictoryにならず、
        // 3個全て操作+1人以上脱出して初めてVictoryになることを直接検証する
        // (正本が定める順序制約「設置後に」までは表現できない既知の近似)。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor mappedEdge = jf::regionDescriptor(jf::RegionId::MappedEdge, *data);
        const jf::StageDescriptor* finalStage = nullptr;
        for (const jf::StageDescriptor& stage : mappedEdge.stages)
            if (stage.id == "mapped_edge_outermost_marker") finalStage = &stage;
        assert(finalStage);
        assert(finalStage->objectPlacementRules.size() == 3);
        assert(finalStage->enemyRoster.size() == 3); // 機動波2体+環境波の大型獣1体
        assert(finalStage->timedReinforcement && finalStage->timedReinforcement->spawnRound == 2 &&
              finalStage->timedReinforcement->units.size() == 4); // 制圧波(守備兵2+弓兵2)
        assert(finalStage->scoutRouteRequiredClass == jf::UnitClass::MarchCaptain);
        bool sawBeast = false;
        for (const jf::UnitTemplate& unit : finalStage->enemyRoster)
            if (unit.classId == jf::UnitClass::FrontierBeast) sawBeast = true;
        assert(sawBeast);

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(*finalStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "frontier_final_key") == 1);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "rare_material") == 3);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "ruin_fragment") == 2);

        jf::BattleState battle = jf::createScenarioBattle(*data, *finalStage, /*seed=*/11);
        for (jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Enemy) unit.currentHp = 0;
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind != jf::BattleOutcomeKind::Victory); // 敵全滅だけでは勝利しない

        const jf::ObjectiveDefinition* escapeDef = nullptr;
        for (const auto& def : battle.missionState().definitions)
            if (def.id == "mapped_edge_final_escape") escapeDef = &def;
        assert(escapeDef && escapeDef->primary && escapeDef->groupId == "primary" &&
              escapeDef->kind == jf::ObjectiveKind::EscapeUnits);

        // 標識を1個ずつ操作していく - 3個全て操作するまでVictoryにならない。
        jf::BattleObjectState* marker1 = battle.findObject("mapped_edge_final_marker_1_1");
        jf::BattleObjectState* marker2 = battle.findObject("mapped_edge_final_marker_2_1");
        jf::BattleObjectState* marker3 = battle.findObject("mapped_edge_final_marker_3_1");
        assert(marker1 && marker2 && marker3);
        marker1->interactionCount = 1;
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind != jf::BattleOutcomeKind::Victory);
        marker2->interactionCount = 1;
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind != jf::BattleOutcomeKind::Victory);

        // 標識2個だけでも、脱出せずにいる限りまだVictoryにならない(脱出との
        // ANDが効いていることの直接証跡)。
        marker3->interactionCount = 1;
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind != jf::BattleOutcomeKind::Victory); // 標識3個、まだ未脱出

        jf::Unit* playerUnit = nullptr;
        for (jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Player) { playerUnit = &unit; break; }
        assert(playerUnit);
        jf::BattleEvent unitEscapes{
            1, 1,
            jf::ActionResolvedEvent{1, playerUnit->id, jf::Team::Player, jf::ActionKind::Wait, escapeDef->target.tile}};
        jf::handleObjectiveEvent(battle.missionState(), unitEscapes);
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory); // 標識3個+脱出1人でVictory
    }

    {
        // docs/regions/mapped_edge.md「最終戦「地図外縁」」の環境波「普通の
        // 大型獣1体」: HP58/STR10/DEF7/RES3/MOV4の新規UnitClass::FrontierBeast
        // + 自己完結した直線突進テレグラフ(takeFrontierBeastBossTurn())の
        // 直接検証。AshenhornBoar等5体の既存ボスと同じ
        // chargeTelegraphed/bossRuntime.telegraph/chargeCooldownActions
        // フィールドを再利用し、enrage/sweep/leash等の追加機構は一切無い
        // (「普通の大型獣」であり、撃破もこの地点の勝敗に一切影響しない)。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::ClassDefinition& beastClass = data->classDefinition(jf::UnitClass::FrontierBeast);
        assert(beastClass.baseStats.maxHp == 58 && beastClass.baseStats.strength == 10 &&
              beastClass.baseStats.defense == 7 && beastClass.baseStats.resistance == 3 &&
              beastClass.baseStats.move == 4);

        const jf::RegionDescriptor mappedEdge = jf::regionDescriptor(jf::RegionId::MappedEdge, *data);
        const jf::StageDescriptor* finalStage = nullptr;
        for (const jf::StageDescriptor& stage : mappedEdge.stages)
            if (stage.id == "mapped_edge_outermost_marker") finalStage = &stage;
        assert(finalStage);

        jf::BattleState battle = jf::createScenarioBattle(*data, *finalStage, /*seed=*/23);
        jf::Unit* beast = nullptr;
        for (jf::Unit& unit : battle.units())
            if (unit.unitClass == jf::UnitClass::FrontierBeast) beast = &unit;
        assert(beast);
        // Line the beast up on the same row as a player unit, within charge
        // range, with nothing else in the way, then let it take its turn:
        // it must telegraph (not immediately execute) a straight-line charge.
        jf::Unit* target = nullptr;
        for (jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Player) { target = &unit; break; }
        assert(target);
        beast->position = jf::GridPos{target->position.row, target->position.col + 2};
        beast->hasActed = false;
        jf::takeEnemyTurn(battle, *beast, nullptr);
        assert(beast->chargeTelegraphed);
        assert(beast->bossRuntime.telegraph.pending());

        // Next turn, the telegraphed charge executes (no further warning) -
        // damages the player unit it passes over, then clears the telegraph.
        const int hpBefore = target->currentHp;
        beast->hasActed = false;
        jf::takeEnemyTurn(battle, *beast, nullptr);
        assert(!beast->chargeTelegraphed);
        assert(!beast->bossRuntime.telegraph.pending());
        assert(target->currentHp < hpBefore);
    }

    {
        // 地点7「折れた見張台」の副目標「記録箱2個とも回収 -> 地図外縁踏査
        // 記録」: heatwork_shop(M9-AE)のkSpecialForgingRecordsDiscoveryと
        // 全く同じall-group-members-Completed ad-hocチェック(GameApp.cpp)を
        // 直接検証する - 1個だけ回収した状態ではまだ全メンバー未完了、2個とも
        // 回収して初めてグループ全体がCompletedになる。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor mappedEdge = jf::regionDescriptor(jf::RegionId::MappedEdge, *data);
        const jf::StageDescriptor* watchtowerStage = nullptr;
        for (const jf::StageDescriptor& stage : mappedEdge.stages)
            if (stage.id == "mapped_edge_broken_watchtower") watchtowerStage = &stage;
        assert(watchtowerStage);

        jf::BattleState battle = jf::createScenarioBattle(*data, *watchtowerStage, /*seed=*/37);
        std::vector<const jf::ObjectiveDefinition*> crateDefs;
        for (const auto& def : battle.missionState().definitions)
            if (def.groupId == "mapped_edge_broken_watchtower_crate") crateDefs.push_back(&def);
        assert(crateDefs.size() == 2);

        auto allCrateGroupCompleted = [&]() {
            for (const jf::ObjectiveDefinition* def : crateDefs)
                if (battle.missionState().progress.at(def->id).status != jf::ObjectiveStatus::Completed)
                    return false;
            return true;
        };

        auto& mutableMission = const_cast<jf::BattleMissionState&>(battle.missionState());
        mutableMission.progress.at(crateDefs[0]->id).status = jf::ObjectiveStatus::Completed;
        assert(!allCrateGroupCompleted()); // 1個だけではまだボーナス条件未達
        mutableMission.progress.at(crateDefs[1]->id).status = jf::ObjectiveStatus::Completed;
        assert(allCrateGroupCompleted()); // 2個とも回収 -> kMappedEdgeSurveyRecordsDiscoveryの付与条件を満たす
    }

    {
        // Both guests escaping grants the "全員脱出: 高品質薬草1" bonus
        // (GameApp::proceedToCamp()'s ad-hoc creditedTargetIds.size()>=2
        // check).
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        jf::GameApp app(*data);
        assert(reachBlackwaterCrossing(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));

        const std::vector<std::string> guestIds = app.battle().battle().missionState().guestUnitIds;
        assert(guestIds.size() == 2);
        const jf::ObjectiveDefinition* escapeDef = nullptr;
        for (const auto& def : app.battle().battle().missionState().definitions)
            if (def.id == "blackwater_crossing_escape") escapeDef = &def;
        assert(escapeDef);
        const jf::GridPos exitTile = escapeDef->target.tile;

        // requiredEscapeCount is 1, so the FIRST guest to cross already
        // satisfies the primary objective and locks BattleController into
        // Victory (no further actions accepted) - crediting the second
        // guest that way is impossible through real play within the same
        // battle. Credit it directly first (as if it had already crossed
        // moments earlier), then have the first guest cross for real so
        // BattleController's own evaluateOutcome() sees both credits
        // already present.
        jf::BattleEvent secondGuestEscapes{
            app.battle().battle().issueEventId(), 1,
            jf::ActionResolvedEvent{1, guestIds[1], jf::Team::Player, jf::ActionKind::Wait, exitTile}};
        jf::handleObjectiveEvent(app.battle().battle().missionState(), secondGuestEscapes);

        jf::Unit* guest = app.battle().battle().findUnit(guestIds[0]);
        assert(guest);
        guest->position = exitTile; // bypass real pathing, same trick winCurrentBattle uses
        guest->hasActed = false;
        app.battle().selectUnit(*guest);
        app.battle().selectMoveTile(guest->position);
        app.battle().chooseWait();
        assert(app.battle().inputState() == jf::BattleInputState::Victory);
        app.proceedToCamp();
        int qualityHerb = 0;
        for (const auto& loot : app.expedition().pendingLoot)
            if (loot.id == "quality_herb") qualityHerb = loot.quantity;
        assert(qualityHerb == 1);
    }

    {
        // Holding the cargo box (SecureTile secondary under
        // surveyObjectiveId="blackwater_crossing_crate") grants "荷物箱保持:
        // 毒素材1" via the ordinary SurveySuccess RewardRule path - no
        // GameApp-side special-casing needed for this one.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        jf::GameApp app(*data);
        assert(reachBlackwaterCrossing(app));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));

        const jf::ObjectiveDefinition* crateDef = nullptr;
        const jf::ObjectiveDefinition* escapeDef = nullptr;
        for (const auto& def : app.battle().battle().missionState().definitions) {
            if (def.groupId == "blackwater_crossing_crate") crateDef = &def;
            if (def.id == "blackwater_crossing_escape") escapeDef = &def;
        }
        assert(crateDef && escapeDef);

        jf::Unit* carrier = nullptr;
        for (jf::Unit& unit : app.battle().battle().units())
            if (unit.team == jf::Team::Player && !unit.isGuest) carrier = &unit;
        assert(carrier);
        carrier->position = crateDef->target.tile;
        carrier->hasActed = false;
        app.battle().selectUnit(*carrier);
        app.battle().selectMoveTile(carrier->position);
        app.battle().chooseWait();

        const std::string& guestId = app.battle().battle().missionState().guestUnitIds[0];
        jf::Unit* guest = app.battle().battle().findUnit(guestId);
        assert(guest);
        guest->position = escapeDef->target.tile;
        guest->hasActed = false;
        app.battle().selectUnit(*guest);
        app.battle().selectMoveTile(guest->position);
        app.battle().chooseWait();

        assert(app.battle().inputState() == jf::BattleInputState::Victory);
        app.proceedToCamp();
        int poisonMaterial = 0;
        for (const auto& loot : app.expedition().pendingLoot)
            if (loot.id == "poison_material") poisonMaterial = loot.quantity;
        assert(poisonMaterial == 1);
    }

    {
        // docs/regions/ashiron_quarry.md「3A. 旧採掘坑」(M9-T): a worker
        // reaching the escape tile wins standalone, same shape as
        // blackwater_crossing's own standalone-escape test above.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor ashironRegion = jf::regionDescriptor(jf::RegionId::AshironQuarry, *data);
        const jf::StageDescriptor* oldMineStage = nullptr;
        for (const jf::StageDescriptor& stage : ashironRegion.stages)
            if (stage.id == "quarry_old_mine") oldMineStage = &stage;
        assert(oldMineStage && oldMineStage->guestUnits.size() == 2);

        jf::BattleState battle = jf::createScenarioBattle(*data, *oldMineStage, /*seed=*/3);
        assert(battle.missionState().guestUnitIds.size() == 2);
        const jf::ObjectiveDefinition* escapeDef = nullptr;
        for (const auto& def : battle.missionState().definitions)
            if (def.id == "quarry_old_mine_escape") escapeDef = &def;
        assert(escapeDef && escapeDef->primary && escapeDef->kind == jf::ObjectiveKind::EscapeUnits);

        const std::string& guestId = battle.missionState().guestUnitIds[0];
        jf::BattleEvent guestEscapes{
            1, 1,
            jf::ActionResolvedEvent{1, guestId, jf::Team::Player, jf::ActionKind::Wait, escapeDef->target.tile}};
        jf::handleObjectiveEvent(battle.missionState(), guestEscapes);
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);
    }

    {
        // docs/regions/ashiron_quarry.md「3A. 旧採掘坑」の敗北条件「全作業員の
        // 撤退」: allGuestsLost() gates Defeat independently of the party
        // squad, same shape as blackwater_crossing's own guest-loss test.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor ashironRegion = jf::regionDescriptor(jf::RegionId::AshironQuarry, *data);
        const jf::StageDescriptor* oldMineStage = nullptr;
        for (const jf::StageDescriptor& stage : ashironRegion.stages)
            if (stage.id == "quarry_old_mine") oldMineStage = &stage;
        assert(oldMineStage);

        jf::BattleState battle = jf::createScenarioBattle(*data, *oldMineStage, /*seed=*/3);
        assert(!battle.allGuestsLost());
        for (jf::Unit& unit : battle.units())
            if (unit.isGuest) unit.currentHp = 0;
        assert(battle.allGuestsLost());
        assert(!battle.allPlayersDefeated());
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Defeat);
    }

    {
        // docs/regions/ashiron_quarry.md「4. 灰鉄鉱脈」(M9-T): Irien reaching
        // the LEFT-column exit tile wins standalone - the "左側退路" primary
        // approximation (PrimaryEscapeUnitsRule::zoneMinCol/zoneMaxCol=0).
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor ashironRegion = jf::regionDescriptor(jf::RegionId::AshironQuarry, *data);
        const jf::StageDescriptor* veinStage = nullptr;
        for (const jf::StageDescriptor& stage : ashironRegion.stages)
            if (stage.id == "ashiron_vein") veinStage = &stage;
        assert(veinStage && veinStage->guestUnits.size() == 1 &&
              veinStage->guestUnits[0].unitTemplate.id == "ashiron_vein_irien");

        jf::BattleState battle = jf::createScenarioBattle(*data, *veinStage, /*seed=*/3);
        assert(battle.missionState().guestUnitIds.size() == 1);
        const jf::ObjectiveDefinition* escapeDef = nullptr;
        for (const auto& def : battle.missionState().definitions)
            if (def.id == "ashiron_vein_escape") escapeDef = &def;
        assert(escapeDef && escapeDef->primary && escapeDef->kind == jf::ObjectiveKind::EscapeUnits);
        assert(escapeDef->target.tile.col <= 2); // left-side zone, not the usual right edge

        const std::string& guestId = battle.missionState().guestUnitIds[0];
        jf::BattleEvent guestEscapes{
            1, 1,
            jf::ActionResolvedEvent{1, guestId, jf::Team::Player, jf::ActionKind::Wait, escapeDef->target.tile}};
        jf::handleObjectiveEvent(battle.missionState(), guestEscapes);
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);
    }

    {
        // docs/regions/ashiron_quarry.md「4. 灰鉄鉱脈」の副目標「イリエンを
        // 撤退させない」の近似(ProtectUnit機構は未使用、GameApp.cppの
        // ad-hoc isPresent()チェック): Irien surviving to Victory grants
        // mage_recruit + 異常鉱脈記録.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        jf::GameApp app(*data);
        jf::SaveData save = app.createSaveData("en");
        save.base.completedRegionIds.insert(jf::RegionId::AshboughForest);
        save.base.completedRegionIds.insert(jf::RegionId::CinderwatchGate);
        assert(app.applySaveData(save));
        assert(app.startExpedition(jf::RegionId::AshironQuarry));
        for (int i = 0; i < 3; ++i) { // quarry_entrance, quarry_terrace, branch member -> ashiron_vein
            assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
            winCurrentBattle(app);
            app.proceedToCamp();
            app.continueExpedition();
        }
        assert(app.currentMissionNameJa() == "灰鉄鉱脈");
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
        winCurrentBattle(app); // teleports Irien onto the escape tile (winCurrentBattle's guest-escape path)
        assert(app.battle().inputState() == jf::BattleInputState::Victory);
        app.proceedToCamp();
        assert(app.expedition().pendingRecruitCandidateIds.count("mage_recruit"));
        assert(std::find(app.expedition().pendingDiscoveries.begin(), app.expedition().pendingDiscoveries.end(),
                         jf::kAnomalousVeinRecordsDiscovery) != app.expedition().pendingDiscoveries.end());
    }

    {
        // docs/regions/windscar_plateau.md「高原運び手の隊長」「通り抜け攻撃」:
        // usable only after a straight 2+-tile move, then repositions up to 2
        // tiles away from the target.
        jf::Unit captain = makeUnit("captain", jf::Team::Enemy, {2, 0}, 6, jf::UnitClass::PlateauCourierCaptain);
        captain.stats.strength = 9;
        captain.stats.defense = 6;
        captain.stats.resistance = 4;
        captain.stats.maxHp = 40;
        captain.currentHp = 40;
        jf::Unit target = makeUnit("target", jf::Team::Player, {2, 3});
        jf::BattleState battle({target, captain});

        jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(battle.units()[0].currentHp < battle.units()[0].stats.maxHp); // hit
        // Moved at least 2 straight tiles before attacking (started at col 0,
        // had to reach col 2 to be adjacent to the col-3 target), then
        // repositioned away from the target afterward (not left adjacent).
        assert(jf::manhattanDistance(battle.units()[1].position, battle.units()[0].position) != 1);
    }

    {
        // docs/regions/windscar_plateau.md「高原運び手の隊長」「退路確保」:
        // HP<=50%で一度だけ発動、次の行動終了までMOV+1/DEF-2(このSliceでは
        // 同一行動内で適用・復帰)。
        jf::Unit captain = makeUnit("captain", jf::Team::Enemy, {2, 5}, 6, jf::UnitClass::PlateauCourierCaptain);
        captain.stats.strength = 9;
        captain.stats.defense = 6;
        captain.stats.resistance = 4;
        captain.stats.maxHp = 40;
        captain.currentHp = 20; // exactly 50%
        jf::Unit target = makeUnit("target", jf::Team::Player, {6, 5}); // far away, out of melee range
        jf::BattleState battle({target, captain});

        jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(battle.units()[1].bossEscapeRouteUsed);
        assert(battle.units()[1].stats.defense == 6); // reverted after this action
        assert(battle.units()[1].stats.move == 6);    // reverted after this action

        battle.units()[1].hasActed = false; // doesn't fire a second time
        battle.units()[1].currentHp = 20;
        jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(battle.units()[1].bossEscapeRouteUsed); // still true, no re-trigger event needed
    }

    {
        // docs/regions/windscar_plateau.md「高原運び手の隊長」「迂回命令」:
        // 戦闘中1回、1ラウンド前に上下いずれかの行を予告。予告解決時に対象が
        // いなければ通常AIへ戻る(無料の追加攻撃は発生しない)。
        jf::Unit captain = makeUnit("captain", jf::Team::Enemy, {2, 5}, 6, jf::UnitClass::PlateauCourierCaptain);
        captain.stats.strength = 9;
        captain.stats.defense = 6;
        captain.stats.resistance = 4;
        captain.stats.maxHp = 40;
        captain.currentHp = 40;
        jf::Unit target = makeUnit("target", jf::Team::Player, {6, 5}); // far away, forces telegraph step
        jf::BattleState battle({target, captain});

        jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(battle.units()[1].bossFlankUsed);
        assert(battle.units()[1].bossRuntime.telegraph.pending());

        battle.units()[1].hasActed = false;
        jf::takeEnemyTurn(battle, battle.units()[1]); // resolves the telegraph this turn
        assert(!battle.units()[1].bossRuntime.telegraph.pending());
        assert(!battle.units()[1].bossFlankUsed || battle.units()[1].bossFlankUsed); // stays used, no re-telegraph path taken
    }

    {
        // docs/regions/windscar_plateau.md「6. 高原伝令所」: defeating the
        // boss (HP0, ScriptedWithdrawal) wins the standard EliminateTeam
        // battle - the courier-escape alternate primary is deferred (same
        // M9-D/M9-K precedent: no OR-composition infra for a single site).
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor windscarRegion = jf::regionDescriptor(jf::RegionId::WindscarPlateau, *data);
        const jf::StageDescriptor* relayStage = nullptr;
        for (const jf::StageDescriptor& stage : windscarRegion.stages)
            if (stage.id == "plateau_relay") relayStage = &stage;
        assert(relayStage);
        assert(relayStage->scoutRouteRequiredClass && *relayStage->scoutRouteRequiredClass == jf::UnitClass::MarchCaptain);
        assert(!relayStage->noCasualtiesBonusLoot.empty());

        jf::BattleState battle = jf::createScenarioBattle(*data, *relayStage, /*seed=*/23);
        const jf::AliveSnapshot before = jf::captureAliveSnapshot(battle);
        for (jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Enemy) unit.currentHp = 0;
        jf::emitUnitDefeatedEvents(battle, before);
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);

        const jf::Unit* boss = nullptr;
        for (const jf::Unit& unit : battle.units())
            if (unit.unitClass == jf::UnitClass::PlateauCourierCaptain) boss = &unit;
        assert(boss && boss->exitReason == jf::UnitExitReason::ScriptedWithdrawal);
    }

    {
        // docs/regions/windscar_plateau.md「地域攻略と拠点接続」: Old Frontier
        // Settlement (第6地域) becomes selectable once Windscar Plateau
        // completes, mirroring the M9-K unlock test.
        jf::GameData data = makeFactoryData();
        jf::GameApp app(data);
        assert(!app.isRegionUnlocked(jf::RegionId::OldFrontierSettlement));
        jf::SaveData save = app.createSaveData("en");
        save.base.completedRegionIds.insert(jf::RegionId::WindscarPlateau);
        assert(app.applySaveData(save));
        assert(app.isRegionUnlocked(jf::RegionId::OldFrontierSettlement));
    }

    {
        // docs/regions/old_frontier_settlement.md「地点構成」: 5-site skeleton
        // + 2 camps + the site 3/4 either-order-but-both-required branch,
        // mirror of the WindscarPlateau skeleton test above.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor settlementRegion =
            jf::regionDescriptor(jf::RegionId::OldFrontierSettlement, *data);
        assert(settlementRegion.stages.size() == 5);
        assert(settlementRegion.stages[0].id == "settlement_outer_fence");

        const jf::RegionRouteGraph& settlementRoute = jf::regionRouteGraph(jf::RegionId::OldFrontierSettlement);
        std::string error;
        assert(jf::validateRouteGraph(settlementRoute, &error));
        assert(jf::findRouteNode(settlementRoute, "settlement_old_granary"));
        assert(jf::findRouteNode(settlementRoute, "settlement_gathering_hall"));
        const jf::RouteNodeDefinition* branch = jf::findRouteNode(settlementRoute, "settlement_granary_hall_branch");
        assert(branch && branch->kind == jf::RouteNodeKind::BranchGroup &&
               branch->branchCompletion == jf::BranchCompletion::AllMembers);
    }

    {
        // docs/regions/old_frontier_settlement.md「1. 風化した外柵」: 主目的
        // (灰道襲撃団4体)・勝利報酬(建築材1、食料1)・ルート2(全員HP-2、敵1体
        // 除外、建築材-1)・ルート3(辺境工兵限定、防護柵1個追加、建築材+1)。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor settlementRegion =
            jf::regionDescriptor(jf::RegionId::OldFrontierSettlement, *data);
        const jf::StageDescriptor& fenceStage = settlementRegion.stages[0];
        assert(fenceStage.enemyRoster.size() == 4);
        assert(fenceStage.scoutRouteRequiredClass == jf::UnitClass::FrontierEngineer);

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(fenceStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "building_material") == 1);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "food") == 1);
        assert(findLoot(lootFor(jf::ExplorationChoice::CollapsedSidePath), "building_material") == 0);
        assert(findLoot(lootFor(jf::ExplorationChoice::ScoutRoute), "building_material") == 2);

        jf::BattleState frontal = jf::createScenarioBattle(*data, fenceStage, /*seed=*/5);
        int enemyCount = 0;
        for (const jf::Unit& unit : frontal.units())
            if (unit.team == jf::Team::Enemy) ++enemyCount;
        assert(enemyCount == 4);

        const jf::ExplorationOutcome* collapsedOutcome = nullptr;
        for (const auto& [choice, outcome] : fenceStage.routeOutcomes)
            if (choice == jf::ExplorationChoice::CollapsedSidePath) collapsedOutcome = &outcome;
        assert(collapsedOutcome);
        jf::BattleState collapsed = jf::createScenarioBattle(*data, fenceStage, /*seed=*/5, *collapsedOutcome);
        int collapsedEnemyCount = 0;
        for (const jf::Unit& unit : collapsed.units())
            if (unit.team == jf::Team::Enemy) ++collapsedEnemyCount;
        assert(collapsedEnemyCount == 3);
    }

    {
        // docs/regions/old_frontier_settlement.md「2. 共同井戸」: 主目的(3
        // ラウンド防衛、または敵全滅)・敵構成(斧兵1/弓兵2/軽装剣士1、井戸優先
        // ルートで+1)・中立Unit4人・副目標(全員避難)・報酬(食料2、織物1、
        // 井戸優先ルートで食料+1)。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor settlementRegion =
            jf::regionDescriptor(jf::RegionId::OldFrontierSettlement, *data);
        const jf::StageDescriptor& wellStage = settlementRegion.stages[1];
        assert(wellStage.id == "settlement_common_well");
        assert(wellStage.enemyRoster.size() == 5);
        assert(wellStage.guestUnits.size() == 4);
        assert(wellStage.scoutRouteRequiredClass == jf::UnitClass::BannerBearer);
        assert(wellStage.primarySurviveRoundsAlternative &&
              wellStage.primarySurviveRoundsAlternative->id == "settlement_well_defense" &&
              wellStage.primarySurviveRoundsAlternative->surviveUntilRound == 3);
        assert(wellStage.secondaryEscapeUnitsAlternative &&
              wellStage.secondaryEscapeUnitsAlternative->id == "settlement_well_evacuate_all" &&
              wellStage.secondaryEscapeUnitsAlternative->requiredEscapeCount == 4);

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(wellStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "food") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "cloth") == 1);
        assert(findLoot(lootFor(jf::ExplorationChoice::CollapsedSidePath), "food") == 3);

        const jf::ExplorationOutcome* frontalOutcome = nullptr;
        for (const auto& [choice, outcome] : wellStage.routeOutcomes)
            if (choice == jf::ExplorationChoice::FrontalAdvance) frontalOutcome = &outcome;
        assert(frontalOutcome);
        jf::BattleState frontal = jf::createScenarioBattle(*data, wellStage, /*seed=*/5, *frontalOutcome);
        int frontalEnemies = 0;
        for (const jf::Unit& unit : frontal.units())
            if (unit.team == jf::Team::Enemy) ++frontalEnemies;
        assert(frontalEnemies == 4);

        const jf::ExplorationOutcome* wellFirstOutcome = nullptr;
        for (const auto& [choice, outcome] : wellStage.routeOutcomes)
            if (choice == jf::ExplorationChoice::CollapsedSidePath) wellFirstOutcome = &outcome;
        assert(wellFirstOutcome);
        jf::BattleState wellFirst = jf::createScenarioBattle(*data, wellStage, /*seed=*/5, *wellFirstOutcome);
        int wellFirstEnemies = 0;
        for (const jf::Unit& unit : wellFirst.units())
            if (unit.team == jf::Team::Enemy) ++wellFirstEnemies;
        assert(wellFirstEnemies == 5);
    }

    {
        // 敗北条件「中立住民全員の撤退」: allGuestsLost() fires Defeat even
        // with the player squad fully alive (blackwater_crossing's own test
        // shape, reused here).
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor settlementRegion =
            jf::regionDescriptor(jf::RegionId::OldFrontierSettlement, *data);
        const jf::StageDescriptor* wellStage = nullptr;
        for (const jf::StageDescriptor& stage : settlementRegion.stages)
            if (stage.id == "settlement_common_well") wellStage = &stage;
        assert(wellStage);

        jf::BattleState battle = jf::createScenarioBattle(*data, *wellStage, /*seed=*/7);
        assert(battle.missionState().guestUnitIds.size() == 4);
        assert(!battle.allGuestsLost());
        for (jf::Unit& unit : battle.units())
            if (unit.isGuest) unit.currentHp = 0;
        assert(battle.allGuestsLost());
        assert(!battle.allPlayersDefeated());
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Defeat);
    }

    {
        // 副目標「中立住民を全員避難」: a REAL independent secondary
        // EscapeUnits group (StageDescriptor::secondaryEscapeUnitsAlternative)
        // completes on its own, without the primary group (SurviveRounds OR
        // EliminateTeam) being satisfied at all - confirms it doesn't collide
        // with or get superseded by the primary group.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor settlementRegion =
            jf::regionDescriptor(jf::RegionId::OldFrontierSettlement, *data);
        const jf::StageDescriptor* wellStage = nullptr;
        for (const jf::StageDescriptor& stage : settlementRegion.stages)
            if (stage.id == "settlement_common_well") wellStage = &stage;
        assert(wellStage);

        jf::BattleState battle = jf::createScenarioBattle(*data, *wellStage, /*seed=*/7);
        const jf::ObjectiveDefinition* evacuateDef = nullptr;
        for (const auto& def : battle.missionState().definitions)
            if (def.id == "settlement_well_evacuate_all") evacuateDef = &def;
        assert(evacuateDef && !evacuateDef->primary && evacuateDef->kind == jf::ObjectiveKind::EscapeUnits &&
              evacuateDef->groupId == "settlement_well_evacuate_all");

        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind != jf::BattleOutcomeKind::Victory);

        const auto& guestIds = battle.missionState().guestUnitIds;
        assert(guestIds.size() == 4);
        for (std::size_t i = 0; i < guestIds.size(); ++i) {
            jf::BattleEvent guestEscapes{
                static_cast<jf::BattleEventId>(i + 1), 1,
                jf::ActionResolvedEvent{1, guestIds[i], jf::Team::Player, jf::ActionKind::Wait,
                                        evacuateDef->target.tile}};
            jf::handleObjectiveEvent(battle.missionState(), guestEscapes);
        }
        jf::syncObjectiveProgress(battle);
        assert(battle.missionState().progress.at("settlement_well_evacuate_all").status ==
              jf::ObjectiveStatus::Completed);
        // The primary group (SurviveRounds OR EliminateTeam) is untouched by
        // the secondary's completion - still no Victory from this alone.
        assert(jf::evaluateBattleOutcome(battle).kind != jf::BattleOutcomeKind::Victory);
    }

    {
        // GameApp integration: reaching Victory with all 4 guests evacuated
        // grants 集落証言記録 (kSettlementCommunalTestimonyDiscovery) via
        // GameApp::proceedToCamp()'s ad-hoc secondary-group check.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        jf::GameApp app(*data);
        jf::SaveData save = app.createSaveData("en");
        save.base.completedRegionIds.insert(jf::RegionId::AshboughForest);
        save.base.completedRegionIds.insert(jf::RegionId::CinderwatchGate);
        save.base.completedRegionIds.insert(jf::RegionId::AshironQuarry);
        save.base.completedRegionIds.insert(jf::RegionId::BlackwaterLowlands);
        save.base.completedRegionIds.insert(jf::RegionId::WindscarPlateau);
        assert(app.applySaveData(save));
        assert(app.startExpedition(jf::RegionId::OldFrontierSettlement));
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)); // settlement_outer_fence
        winCurrentBattle(app);
        app.proceedToCamp();
        app.continueExpedition();
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)); // settlement_common_well

        const auto& guestIds = app.battle().battle().missionState().guestUnitIds;
        assert(guestIds.size() == 4);
        const jf::ObjectiveDefinition* evacuateDef = nullptr;
        for (const auto& def : app.battle().battle().missionState().definitions)
            if (def.id == "settlement_well_evacuate_all") evacuateDef = &def;
        assert(evacuateDef);
        for (const std::string& guestId : guestIds) {
            jf::BattleEvent guestEscapes{
                app.battle().battle().issueEventId(), 1,
                jf::ActionResolvedEvent{1, guestId, jf::Team::Player, jf::ActionKind::Wait,
                                        evacuateDef->target.tile}};
            jf::handleObjectiveEvent(app.battle().battle().missionState(), guestEscapes);
        }
        jf::syncObjectiveProgress(app.battle().battle());
        winCurrentBattle(app);
        assert(app.battle().inputState() == jf::BattleInputState::Victory);
        app.proceedToCamp();
        assert(std::find(app.expedition().pendingDiscoveries.begin(), app.expedition().pendingDiscoveries.end(),
                         jf::kSettlementCommunalTestimonyDiscovery) != app.expedition().pendingDiscoveries.end());
    }

    {
        // docs/regions/old_frontier_settlement.md「3. 旧穀物庫」: 主目的(4
        // ラウンド防衛、または敵全滅)・敵構成(斧兵2/弓兵2/軽装剣士1、3ラウンド目
        // に軽装剣士1の予告増援)・副目標(食料箱2個保持)・報酬(建築材2、織物1、
        // 食料1、封鎖ルートで食料+1、食料箱2個で食料+2)。JSON-authored via
        // data/regions.json (stageDescriptorFromContent()) like site 1 - no
        // guestUnits needed, so no hand-written Region.cpp stage function.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor settlementRegion =
            jf::regionDescriptor(jf::RegionId::OldFrontierSettlement, *data);
        const jf::StageDescriptor* granaryStage = nullptr;
        for (const jf::StageDescriptor& stage : settlementRegion.stages)
            if (stage.id == "settlement_old_granary") granaryStage = &stage;
        assert(granaryStage);
        assert(granaryStage->enemyRoster.size() == 5);
        assert(granaryStage->scoutRouteRequiredClass == jf::UnitClass::VeteranGuard);
        assert(granaryStage->primarySurviveRoundsAlternative &&
              granaryStage->primarySurviveRoundsAlternative->id == "settlement_granary_defense" &&
              granaryStage->primarySurviveRoundsAlternative->surviveUntilRound == 4);
        assert(granaryStage->timedReinforcement &&
              granaryStage->timedReinforcement->id == "settlement_granary_swordsman_wave" &&
              granaryStage->timedReinforcement->spawnRound == 3);

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(*granaryStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "building_material") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "cloth") == 1);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "food") == 1);
        assert(findLoot(lootFor(jf::ExplorationChoice::CollapsedSidePath), "food") == 2);
        assert(findLoot(jf::computeStageVictoryLoot(*granaryStage, jf::ExplorationChoice::FrontalAdvance,
                                                     /*surveyObjectiveSucceeded=*/true),
                        "food") == 3);

        const jf::ExplorationOutcome* collapsedOutcome = nullptr;
        for (const auto& [choice, outcome] : granaryStage->routeOutcomes)
            if (choice == jf::ExplorationChoice::CollapsedSidePath) collapsedOutcome = &outcome;
        assert(collapsedOutcome);
        jf::BattleState collapsed = jf::createScenarioBattle(*data, *granaryStage, /*seed=*/5, *collapsedOutcome);
        int collapsedEnemies = 0;
        for (const jf::Unit& unit : collapsed.units())
            if (unit.team == jf::Team::Enemy) ++collapsedEnemies;
        assert(collapsedEnemies == 4);

        // Primary win via SurviveRounds to round 4 (no enemy kills needed),
        // same shape as blackwater_lowlands「薬草洲」/settlement_common_well.
        jf::BattleState defense = jf::createScenarioBattle(*data, *granaryStage, /*seed=*/5);
        const jf::ObjectiveDefinition* surviveDef = nullptr;
        for (const auto& def : defense.missionState().definitions)
            if (def.kind == jf::ObjectiveKind::SurviveRounds && def.id == "settlement_granary_defense")
                surviveDef = &def;
        assert(surviveDef && surviveDef->primary);
        jf::syncObjectiveProgress(defense);
        assert(jf::evaluateBattleOutcome(defense).kind != jf::BattleOutcomeKind::Victory);
        while (defense.round() <= 4) {
            defense.beginEnemyPhase();
            defense.beginPlayerPhase();
        }
        jf::syncObjectiveProgress(defense);
        assert(jf::evaluateBattleOutcome(defense).kind == jf::BattleOutcomeKind::Victory);
    }

    {
        // docs/regions/old_frontier_settlement.md「4. 集会家屋」: 主目的(記録箱
        // 2個以上搬入、OR敵全滅)は`surveyTileCount`が「Nのうち任意1個で
        // グループ完了(Any)」であって真のN-of-M閾値ではないため(M9-H「樹脂箱」
        // 前例と同型)、標準EliminateTeamで近似 - ドキュメントのOR条件(敵全滅)
        // と自然に一致する。ルート別記録箱個数(3/2/3)は固定3個で近似
        // (surveyTileCount:3、food_crate等ownd既存パターンと同型)。
        // guestUnitsはルート3のみ登場し他2ルートには無い(M9-Vの4人固定ケースと
        // 非対称)ため、本Sliceでは実装せず「中立住民2人が運搬」を未配線の
        // flavor textとして記録する判断とした - 対応する副目標「運搬人撤退
        // させない」/報酬(織物1)も同時に見送り。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor settlementRegion =
            jf::regionDescriptor(jf::RegionId::OldFrontierSettlement, *data);
        const jf::StageDescriptor* hallStage = nullptr;
        for (const jf::StageDescriptor& stage : settlementRegion.stages)
            if (stage.id == "settlement_gathering_hall") hallStage = &stage;
        assert(hallStage);
        assert(hallStage->enemyRoster.size() == 4);
        assert(hallStage->scoutRouteRequiredClass == jf::UnitClass::MarchCaptain);
        assert(!hallStage->primarySurviveRoundsAlternative); // default EliminateTeam primary
        assert(hallStage->surveyObjectiveId && *hallStage->surveyObjectiveId ==
                                                    "settlement_gathering_hall_ledger_crates");
        assert(hallStage->surveyTileCount && *hallStage->surveyTileCount == 3);

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(*hallStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "building_material") == 1);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "iron") == 1);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "food") == 1);
        assert(jf::computeStageDiscoveries(*hallStage, jf::ExplorationChoice::FrontalAdvance) ==
              std::vector<jf::DiscoveryId>{"settlement_command_ledger"});

        const jf::ExplorationOutcome* collapsedOutcome = nullptr;
        for (const auto& [choice, outcome] : hallStage->routeOutcomes)
            if (choice == jf::ExplorationChoice::CollapsedSidePath) collapsedOutcome = &outcome;
        assert(collapsedOutcome);
        assert(collapsedOutcome->enemiesRemoved == 1);
        assert(collapsedOutcome->extraBarrierCount == 1);
        jf::BattleState collapsed = jf::createScenarioBattle(*data, *hallStage, /*seed=*/5, *collapsedOutcome);
        int collapsedEnemies = 0;
        for (const jf::Unit& unit : collapsed.units())
            if (unit.team == jf::Team::Enemy) ++collapsedEnemies;
        assert(collapsedEnemies == 3);
        bool foundBarrier = false;
        for (const jf::BattleObjectState& object : collapsed.objects())
            if (object.definitionId == "settlement_reinforced_barrier") foundBarrier = true;
        assert(foundBarrier);

        // Primary win via plain EliminateTeam (default), no crate collection
        // required - matches the doc's OR condition (敵全滅) directly.
        jf::BattleState hall = jf::createScenarioBattle(*data, *hallStage, /*seed=*/5);
        jf::syncObjectiveProgress(hall);
        assert(jf::evaluateBattleOutcome(hall).kind != jf::BattleOutcomeKind::Victory);
        for (jf::Unit& unit : hall.units())
            if (unit.team == jf::Team::Enemy) unit.currentHp = 0;
        jf::syncObjectiveProgress(hall);
        assert(jf::evaluateBattleOutcome(hall).kind == jf::BattleOutcomeKind::Victory);
    }

    {
        // docs/regions/old_frontier_settlement.md「5. 夜明けの共同防衛」: 地域
        // 最終地点の構成検証。敵6体(頭領+斧兵2+弓兵2+ルート2専用のaxeman3)、
        // 主目的サブ条件1(SurviveRounds、5ラウンド)、副目標(全員避難の独立
        // Secondary、警鐘操作の独立Secondary)、guestUnits4人、2ラウンド目
        // timedReinforcement、味方戦闘不能者0ボーナス。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor settlementRegion =
            jf::regionDescriptor(jf::RegionId::OldFrontierSettlement, *data);
        assert(settlementRegion.stages.size() == 5);
        const jf::StageDescriptor* dawnStage = nullptr;
        for (const jf::StageDescriptor& stage : settlementRegion.stages)
            if (stage.id == "settlement_dawn_defense") dawnStage = &stage;
        assert(dawnStage);
        assert(dawnStage->enemyRoster.size() == 6);
        assert(dawnStage->enemyRoster[0].classId == jf::UnitClass::RaidLeader);
        assert(dawnStage->scoutRouteRequiredClass == jf::UnitClass::BannerBearer);
        assert(dawnStage->primarySurviveRoundsAlternative &&
              dawnStage->primarySurviveRoundsAlternative->id == "settlement_dawn_defense_survive" &&
              dawnStage->primarySurviveRoundsAlternative->surviveUntilRound == 5);
        assert(dawnStage->secondaryEscapeUnitsAlternative &&
              dawnStage->secondaryEscapeUnitsAlternative->id == "settlement_dawn_evacuate_all" &&
              dawnStage->secondaryEscapeUnitsAlternative->requiredEscapeCount == 4);
        assert(dawnStage->guestUnits.size() == 4);
        assert(dawnStage->timedReinforcement && dawnStage->timedReinforcement->spawnRound == 2 &&
              dawnStage->timedReinforcement->units.size() == 2);
        assert(dawnStage->noCasualtiesBonusLoot.size() == 1 &&
              dawnStage->noCasualtiesBonusLoot[0].id == "building_material");
        bool foundBellRule = false;
        for (const auto& rule : dawnStage->objectPlacementRules)
            if (rule.secondaryOperateObjectiveId && *rule.secondaryOperateObjectiveId == "settlement_dawn_alarm_bell")
                foundBellRule = true;
        assert(foundBellRule);

        const jf::ExplorationOutcome* collapsedOutcome = nullptr;
        for (const auto& [choice, outcome] : dawnStage->routeOutcomes)
            if (choice == jf::ExplorationChoice::CollapsedSidePath) collapsedOutcome = &outcome;
        assert(collapsedOutcome);
        jf::BattleState collapsed = jf::createScenarioBattle(*data, *dawnStage, /*seed=*/5, *collapsedOutcome);
        int collapsedEnemies = 0;
        for (const jf::Unit& unit : collapsed.units())
            if (unit.team == jf::Team::Enemy) ++collapsedEnemies;
        assert(collapsedEnemies == 6); // "敵1体追加" route keeps the full base roster

        const jf::ExplorationOutcome* frontalOutcome = nullptr;
        for (const auto& [choice, outcome] : dawnStage->routeOutcomes)
            if (choice == jf::ExplorationChoice::FrontalAdvance) frontalOutcome = &outcome;
        assert(frontalOutcome && frontalOutcome->extraBarrierCount == 2);
        jf::BattleState frontal = jf::createScenarioBattle(*data, *dawnStage, /*seed=*/5, *frontalOutcome);
        int frontalEnemies = 0;
        for (const jf::Unit& unit : frontal.units())
            if (unit.team == jf::Team::Enemy) ++frontalEnemies;
        assert(frontalEnemies == 5);
        int barrierCount = 0;
        for (const jf::BattleObjectState& object : frontal.objects())
            if (object.definitionId == "settlement_reinforced_barrier") ++barrierCount;
        assert(barrierCount == 2);
    }

    {
        // docs/regions/old_frontier_settlement.md「5. 夜明けの共同防衛」主目的
        // サブ条件1: 5ラウンド終了まで避難所を守る(SurviveRoundsをそのまま
        // 再利用、EliminateTeamとのOR - herb_islet/settlement_granary以来の
        // パターン)。敵を1体も倒さなくても5ラウンド生存でVictory。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor settlementRegion =
            jf::regionDescriptor(jf::RegionId::OldFrontierSettlement, *data);
        const jf::StageDescriptor* dawnStage = nullptr;
        for (const jf::StageDescriptor& stage : settlementRegion.stages)
            if (stage.id == "settlement_dawn_defense") dawnStage = &stage;
        assert(dawnStage);

        jf::BattleState defense = jf::createScenarioBattle(*data, *dawnStage, /*seed=*/5);
        jf::syncObjectiveProgress(defense);
        assert(jf::evaluateBattleOutcome(defense).kind != jf::BattleOutcomeKind::Victory);
        while (defense.round() <= 5) {
            defense.beginEnemyPhase();
            defense.beginPlayerPhase();
        }
        jf::syncObjectiveProgress(defense);
        assert(jf::evaluateBattleOutcome(defense).kind == jf::BattleOutcomeKind::Victory);
    }

    {
        // docs/regions/old_frontier_settlement.md「5. 夜明けの共同防衛」敗北条件
        // 「中立住民全員の撤退」: allGuestsLost()経由でDefeat、部隊全滅とは
        // 独立(blackwater_crossing/windscar_relay以来のパターン)。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor settlementRegion =
            jf::regionDescriptor(jf::RegionId::OldFrontierSettlement, *data);
        const jf::StageDescriptor* dawnStage = nullptr;
        for (const jf::StageDescriptor& stage : settlementRegion.stages)
            if (stage.id == "settlement_dawn_defense") dawnStage = &stage;
        assert(dawnStage);

        jf::BattleState battle = jf::createScenarioBattle(*data, *dawnStage, /*seed=*/7);
        assert(!battle.allGuestsLost());
        for (jf::Unit& unit : battle.units())
            if (unit.isGuest) unit.currentHp = 0;
        assert(battle.allGuestsLost());
        assert(!battle.allPlayersDefeated());
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Defeat);
    }

    {
        // docs/regions/old_frontier_settlement.md「5. 夜明けの共同防衛」主目的
        // サブ条件3「警鐘を1回以上操作する」: `secondaryOperateObjectiveId`が
        // 実際に独立したSecondary Objectiveを生成し、primaryグループの状態に
        // 一切影響しないことを確認(settlement_common_well「全員避難」テストと
        // 同型のパターン、OperateObject版)。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor settlementRegion =
            jf::regionDescriptor(jf::RegionId::OldFrontierSettlement, *data);
        const jf::StageDescriptor* dawnStage = nullptr;
        for (const jf::StageDescriptor& stage : settlementRegion.stages)
            if (stage.id == "settlement_dawn_defense") dawnStage = &stage;
        assert(dawnStage);

        jf::BattleState battle = jf::createScenarioBattle(*data, *dawnStage, /*seed=*/5);
        const jf::ObjectiveDefinition* bellDef = nullptr;
        for (const auto& def : battle.missionState().definitions)
            if (def.groupId == "settlement_dawn_alarm_bell") bellDef = &def;
        assert(bellDef && !bellDef->primary && bellDef->kind == jf::ObjectiveKind::OperateObject);
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind != jf::BattleOutcomeKind::Victory);

        // OperateObject is live-evaluated off BattleObjectState::
        // interactionCount (see the OperateObject unit test above/
        // ObjectiveTracker.cpp's syncObjectiveProgress()), not a discrete
        // event - bump it directly, same as that existing test does.
        jf::GridPos bellPos{};
        for (const jf::BattleObjectState& object : battle.objects())
            if (object.id == bellDef->target.objectId) bellPos = object.position;
        jf::BattleObjectState* bell = battle.objectAt(bellPos);
        assert(bell && bell->definitionId == "settlement_alarm_bell");
        bell->interactionCount = 1;
        jf::syncObjectiveProgress(battle);
        assert(battle.missionState().progress.at(bellDef->id).status == jf::ObjectiveStatus::Completed);
        // The primary group (SurviveRounds OR EliminateTeam) is untouched.
        assert(jf::evaluateBattleOutcome(battle).kind != jf::BattleOutcomeKind::Victory);
    }

    {
        // GameApp integration: 頭領撤退(鉄材1)と全員避難(織物2)のad-hocボーナス、
        // 味方戦闘不能者0(建築材1、noCasualtiesBonusLoot経由)。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        jf::GameApp app(*data);
        jf::SaveData save = app.createSaveData("en");
        save.base.completedRegionIds.insert(jf::RegionId::AshboughForest);
        save.base.completedRegionIds.insert(jf::RegionId::CinderwatchGate);
        save.base.completedRegionIds.insert(jf::RegionId::AshironQuarry);
        save.base.completedRegionIds.insert(jf::RegionId::BlackwaterLowlands);
        save.base.completedRegionIds.insert(jf::RegionId::WindscarPlateau);
        assert(app.applySaveData(save));
        assert(app.startExpedition(jf::RegionId::OldFrontierSettlement));
        for (int i = 0; i < 4; ++i) {
            assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
            winCurrentBattle(app);
            app.proceedToCamp();
            app.continueExpedition();
        }
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)); // settlement_dawn_defense
        winCurrentBattle(app); // wins via the default EliminateTeam OR-member

        jf::BattleState& battle = app.battle().battle();
        // Override the raid leader to have retreated (rather than been
        // defeated by winCurrentBattle's HP-zeroing) and credit all 4 guests
        // onto the secondary evacuation group, both read by
        // GameApp::proceedToCamp()'s ad-hoc bonus blocks below.
        for (jf::Unit& unit : battle.units())
            if (unit.id == "settlement_dawn_raid_leader") unit.exitReason = jf::UnitExitReason::Retreated;
        const jf::ObjectiveDefinition* evacuateDef = nullptr;
        for (const auto& def : battle.missionState().definitions)
            if (def.groupId == "settlement_dawn_evacuate_all") evacuateDef = &def;
        assert(evacuateDef);
        for (const std::string& guestId : battle.missionState().guestUnitIds) {
            jf::BattleEvent guestEscapes{
                battle.issueEventId(), 1,
                jf::ActionResolvedEvent{1, guestId, jf::Team::Player, jf::ActionKind::Wait,
                                        evacuateDef->target.tile}};
            jf::handleObjectiveEvent(battle.missionState(), guestEscapes);
        }
        jf::syncObjectiveProgress(battle);
        app.proceedToCamp();

        auto lootQty = [&](const std::string& id) -> int {
            int total = 0;
            for (const jf::LootStack& stack : app.expedition().pendingLoot)
                if (stack.id == id) total += stack.quantity;
            return total;
        };
        assert(lootQty("iron") >= 1);
        assert(lootQty("cloth") >= 2);
        assert(lootQty("building_material") >= 1);
    }

    {
        // docs/regions/old_frontier_settlement.md「地域攻略と拠点接続」/
        // 「最低保証報酬」: safely returning after clearing all 5 sites
        // commits `RegionId::OldFrontierSettlement` to completedRegionIds
        // (regionCleared()/computeWouldRegionBeCleared()'s existing generic
        // mechanism is the whole implementation, same as Ashiron Quarry/
        // Blackwater/Windscar before it - no `old_frontier_settlement_secured`
        // code entity of its own), tops up the floor via
        // `settlementMaterialsEarned`, and unlocks EmberRavine (燼火峡谷).
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        jf::GameApp app(*data);
        jf::SaveData save = app.createSaveData("en");
        save.base.completedRegionIds.insert(jf::RegionId::AshboughForest);
        save.base.completedRegionIds.insert(jf::RegionId::CinderwatchGate);
        save.base.completedRegionIds.insert(jf::RegionId::AshironQuarry);
        save.base.completedRegionIds.insert(jf::RegionId::BlackwaterLowlands);
        save.base.completedRegionIds.insert(jf::RegionId::WindscarPlateau);
        assert(app.applySaveData(save));
        assert(!app.isRegionUnlocked(jf::RegionId::EmberRavine));
        assert(app.startExpedition(jf::RegionId::OldFrontierSettlement));
        for (int i = 0; i < 4; ++i) {
            assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance));
            winCurrentBattle(app);
            app.proceedToCamp();
            app.continueExpedition();
        }
        assert(app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance)); // settlement_dawn_defense
        winCurrentBattle(app);
        app.proceedToCamp();
        assert(app.expedition().pendingRegionCompletions.count(jf::RegionId::OldFrontierSettlement));
        assert(app.returnToBase());
        assert(app.baseState().completedRegionIds.count(jf::RegionId::OldFrontierSettlement));
        assert(app.isRegionUnlocked(jf::RegionId::EmberRavine));
        assert(app.baseState().storageCount("building_material") >= 6);
        assert(app.baseState().storageCount("iron") >= 3);
        assert(app.baseState().storageCount("food") >= 7);
        assert(app.baseState().storageCount("cloth") >= 2);
        assert(app.baseState().discoveryRegistry.count(jf::kSettlementCommandLedgerDiscovery));
        assert(app.baseState().discoveryRegistry.count(jf::kCollectiveDefenseRecordsDiscovery));

        auto summaries = app.regionSummaries();
        bool sawEmberRavine = false;
        for (const auto& summary : summaries)
            if (summary.id == jf::RegionId::EmberRavine) sawEmberRavine = true;
        assert(sawEmberRavine);
    }

    // M7項目3続き(基礎〜中程度Tier) 武器分岐固有効果 (docs/base_development.md).
    // Guard Spear's braceBoost is already covered above; these focus on the
    // OTHER 16 branches' unique effects (recipe/stat-swap tests already
    // exist from the prior generalization work).
    {
        // Charge Lance: +2 damage after moving 3+ tiles this action.
        jf::Unit rider = makeUnit("rider", jf::Team::Player, {1, 0}, 6, jf::UnitClass::MessengerCavalry);
        rider.weapon = {.id = "charge_lance", .name = "Charge Lance", .might = 7,
                        .minRange = 1, .maxRange = 1, .damageType = jf::DamageType::Physical};
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 4});
        jf::BattleController controller(jf::BattleState({rider, enemy}));
        controller.selectUnit(controller.battle().units().front());
        controller.selectMoveTile({1, 3}); // 3 tiles moved, adjacent to enemy
        assert(controller.battle().units()[0].tilesMovedThisAction == 3);
        controller.chooseAttack();
        controller.selectTargetTile({1, 4});
        assert(controller.inputState() == jf::BattleInputState::ConfirmAttack);
        auto preview = controller.pendingPreview();
        assert(preview.has_value());
        int baseline = 6 + 7 - 2; // STR + might - DEF
        assert(preview->damage == baseline + 2);
    }
    {
        // Ambush Blade: +2 damage vs an enemy that hasn't acted THIS round.
        // Regression test for a bug where raw `hasActed` never reset between
        // rounds during the Player Phase (it only clears at the owning
        // team's own next Phase start), so an enemy that acted in round 1
        // would incorrectly read as "already acted" for round 2's Player
        // Phase too (bonus wrongly denied, even though that enemy hasn't
        // acted in round 2 yet). `Unit::lastActedRound` + `BattleState::
        // markActed()` fix this by stamping the round the action happened
        // in, not just a boolean.
        jf::Unit scout = makeUnit("scout", jf::Team::Player, {1, 0}, 4, jf::UnitClass::FrontierScout);
        scout.weapon = {.id = "ambush_blade", .name = "Ambush Blade", .might = 6,
                        .minRange = 1, .maxRange = 1, .damageType = jf::DamageType::Physical};
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 1});
        assert(!enemy.hasActed);
        jf::BattleController controller(jf::BattleState({scout, enemy}));
        assert(controller.battle().round() == 1);
        assert(jf::weaponBranchBonusDamage(controller.battle().units(), controller.battle().units()[0],
                                           controller.battle().units()[1], controller.battle().round()) == 2);
        // `enemy` acts during round 1's Enemy Phase.
        controller.battle().markActed(controller.battle().units()[1]);
        controller.battle().beginPlayerPhase(); // now round 2, hasActed NOT reset for enemies
        assert(controller.battle().round() == 2);
        assert(controller.battle().units()[1].hasActed); // stale true from round 1 - the bug's trap
        // `enemy` hasn't acted in round 2 yet (its round-1 action doesn't
        // count) - the bonus must still apply here. The old raw-`hasActed`
        // check would wrongly deny it (the bug this test guards against).
        assert(jf::weaponBranchBonusDamage(controller.battle().units(), controller.battle().units()[0],
                                           controller.battle().units()[1], controller.battle().round()) == 2);
        // Now `enemy` acts again, this time IN round 2 - the bonus must be
        // denied against it for the rest of round 2.
        controller.battle().markActed(controller.battle().units()[1]);
        assert(jf::weaponBranchBonusDamage(controller.battle().units(), controller.battle().units()[0],
                                           controller.battle().units()[1], controller.battle().round()) == 0);
    }
    {
        // War Bow: +2 damage vs a target at full HP; no bonus once damaged.
        jf::Unit archer = makeUnit("archer", jf::Team::Player, {1, 0}, 4, jf::UnitClass::WatchArcher);
        archer.weapon = {.id = "war_bow", .name = "War Bow", .might = 8,
                         .minRange = 2, .maxRange = 2, .damageType = jf::DamageType::Physical};
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 2});
        jf::BattleController controller(jf::BattleState({archer, enemy}));
        auto& units = controller.battle().units();
        int fullBonus = jf::weaponBranchBonusDamage(units, units[0], units[1], controller.battle().round());
        auto full = jf::previewAttack(units[0], units[1], controller.battle().combatDefenseBonus(units[1], units[0]),
                                      100, fullBonus);
        assert(full.damage == 6 + 8 - 2 + 2);
        units[1].currentHp -= 1;
        int damagedBonus = jf::weaponBranchBonusDamage(units, units[0], units[1], controller.battle().round());
        auto damaged = jf::previewAttack(units[0], units[1], controller.battle().combatDefenseBonus(units[1], units[0]),
                                         100, damagedBonus);
        assert(damaged.damage == 6 + 8 - 2);
    }
    {
        // Duel Sword: +2 damage only when no other unit of the target's team
        // is adjacent to it.
        jf::Unit captain = makeUnit("captain", jf::Team::Player, {1, 0}, 4, jf::UnitClass::MarchCaptain);
        captain.weapon = {.id = "duel_sword", .name = "Duel Sword", .might = 7,
                          .minRange = 1, .maxRange = 1, .damageType = jf::DamageType::Physical};
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 1});
        jf::Unit escort = makeUnit("escort", jf::Team::Enemy, {1, 2});
        jf::BattleController isolated(jf::BattleState({captain, enemy}));
        assert(jf::weaponBranchBonusDamage(isolated.battle().units(), isolated.battle().units()[0],
                                           isolated.battle().units()[1], isolated.battle().round()) == 2);
        jf::BattleController escorted(jf::BattleState({captain, enemy, escort}));
        assert(jf::weaponBranchBonusDamage(escorted.battle().units(), escorted.battle().units()[0],
                                           escorted.battle().units()[1], escorted.battle().round()) == 0);
    }
    {
        // Quarry Bow: +2 damage only when no ally of the target is adjacent.
        jf::Unit ranger = makeUnit("ranger", jf::Team::Player, {1, 0}, 4, jf::UnitClass::FrontierRanger);
        ranger.weapon = {.id = "quarry_bow", .name = "Quarry Bow", .might = 5,
                         .minRange = 2, .maxRange = 2, .damageType = jf::DamageType::Physical};
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 2});
        jf::Unit ally = makeUnit("ally", jf::Team::Enemy, {1, 3});
        jf::BattleController isolated(jf::BattleState({ranger, enemy}));
        assert(jf::weaponBranchBonusDamage(isolated.battle().units(), isolated.battle().units()[0],
                                           isolated.battle().units()[1], isolated.battle().round()) == 2);
        jf::BattleController grouped(jf::BattleState({ranger, enemy, ally}));
        assert(jf::weaponBranchBonusDamage(grouped.battle().units(), grouped.battle().units()[0],
                                           grouped.battle().units()[1], grouped.battle().round()) == 0);
    }
    {
        // Mercy Staff: Heal amount 8 -> 12.
        jf::Unit healer = makeUnit("healer", jf::Team::Player, {1, 0}, 4, jf::UnitClass::DawnChirurgeon);
        healer.weapon = {.id = "mercy_staff", .name = "Mercy Staff", .might = 1, .minRange = 1, .maxRange = 2,
                         .damageType = jf::DamageType::Magical, .healAmountOverride = 12};
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {1, 1});
        ally.currentHp = 4;
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 7});
        jf::BattleController controller(jf::BattleState({healer, ally, enemy}));
        controller.selectUnit(controller.battle().units().front());
        controller.selectMoveTile({1, 0});
        controller.chooseHeal();
        controller.selectHealTarget({1, 1});
        assert(controller.battle().units()[1].currentHp == 16);
    }
    {
        // Ward Staff: Heal 6 + RES+3 until next Enemy Phase end.
        jf::Unit healer = makeUnit("healer", jf::Team::Player, {1, 0}, 4, jf::UnitClass::DawnChirurgeon);
        healer.weapon = {.id = "ward_staff", .name = "Ward Staff", .might = 2, .minRange = 1, .maxRange = 2,
                         .damageType = jf::DamageType::Magical, .healAmountOverride = 6,
                         .healGrantsResistanceUp = true};
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {1, 1});
        ally.currentHp = 4;
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 7});
        jf::BattleController controller(jf::BattleState({healer, ally, enemy}));
        controller.selectUnit(controller.battle().units().front());
        controller.selectMoveTile({1, 0});
        controller.chooseHeal();
        controller.selectHealTarget({1, 1});
        assert(controller.battle().units()[1].currentHp == 10);
        assert(controller.battle().units()[1].resistanceUpActive);
    }
    {
        // March Staff: Heal 6 + MOV+1 until this Player Phase ends, only if
        // the target hadn't acted yet.
        jf::Unit healer = makeUnit("healer", jf::Team::Player, {1, 0}, 4, jf::UnitClass::DawnChirurgeon);
        healer.weapon = {.id = "march_staff", .name = "March Staff", .might = 2, .minRange = 1, .maxRange = 2,
                         .damageType = jf::DamageType::Magical, .healAmountOverride = 6, .healGrantsMoveUp = true};
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {1, 1});
        ally.currentHp = 4;
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 7});
        jf::BattleController controller(jf::BattleState({healer, ally, enemy}));
        controller.selectUnit(controller.battle().units().front());
        controller.selectMoveTile({1, 0});
        controller.chooseHeal();
        controller.selectHealTarget({1, 1});
        assert(controller.battle().units()[1].currentHp == 10);
        assert(controller.battle().units()[1].moveUpActive);
    }
    {
        // Repair Hammer: field_repair amount 6 -> 9.
        jf::Unit engineer = makeUnit("engineer", jf::Team::Player, {1, 0}, 4, jf::UnitClass::FrontierEngineer);
        engineer.weapon = {.id = "repair_hammer", .name = "Repair Hammer", .might = 4, .minRange = 1, .maxRange = 1,
                           .damageType = jf::DamageType::Physical, .fieldRepairAmountOverride = 9};
        engineer.skillSlots[0].skillId = "field_repair";
        engineer.skillSlots[0].usesRemaining = 1;
        jf::BattleController controller(jf::BattleState({engineer}));
        jf::BattleObjectDefinition boardDef;
        boardDef.definitionId = "field_barricade";
        boardDef.kind = jf::BattleObjectKind::Barrier;
        boardDef.maxDurability = 20;
        boardDef.canBeRepaired = true;
        controller.battle().registerObjectDefinition(boardDef);
        assert(controller.battle().placeObject({"board1", "field_barricade", {1, 1}, jf::BattleObjectTeam::Player,
                                                jf::BattleObjectStateKind::Active, 5, 0}));
        controller.selectUnit(controller.battle().units().front());
        controller.selectMoveTile({1, 0});
        controller.chooseSkill(0);
        assert(controller.selectSkillTarget({1, 1}));
        assert(controller.battle().objectAt({1, 1})->durability == 14);
    }
    {
        // Snare Bow: move_down on-hit gated to the first hit only per battle.
        jf::Unit ranger = makeUnit("ranger", jf::Team::Player, {1, 0}, 4, jf::UnitClass::FrontierRanger);
        ranger.weapon = {.id = "snare_bow", .name = "Snare Bow", .might = 3, .minRange = 2, .maxRange = 2,
                         .damageType = jf::DamageType::Physical,
                         .onHitStatuses = {jf::StatusEffectType::MoveDown}, .firstHitOnly = true};
        jf::Unit enemy1 = makeUnit("enemy1", jf::Team::Enemy, {1, 2});
        jf::Unit enemy2 = makeUnit("enemy2", jf::Team::Enemy, {3, 0});
        jf::BattleController controller(jf::BattleState({ranger, enemy1, enemy2}));
        controller.selectUnit(controller.battle().units().front());
        controller.selectMoveTile({1, 2}); // attack shortcut: enemy1 tile is in range
        controller.confirmAttack();
        assert(controller.battle().units()[1].moveDownActive);
        assert(controller.battle().units()[0].weaponFirstHitUsed);
        controller.battle().units()[0].hasActed = false; // simulate a second engagement this battle
        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile({3, 0}); // attack shortcut: enemy2 tile is in range
        controller.confirmAttack();
        assert(!controller.battle().units()[2].moveDownActive); // gate already spent
    }
    {
        // Driving Bow: knockback gated to the first hit only per battle.
        jf::Unit ranger = makeUnit("ranger", jf::Team::Player, {1, 1}, 4, jf::UnitClass::FrontierRanger);
        ranger.weapon = {.id = "driving_bow", .name = "Driving Bow", .might = 3, .minRange = 2, .maxRange = 2,
                         .damageType = jf::DamageType::Physical, .causesKnockback = true, .firstHitOnly = true};
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 3});
        jf::BattleController controller(jf::BattleState({ranger, enemy}));
        controller.selectUnit(controller.battle().units().front());
        controller.selectMoveTile({1, 3}); // attack shortcut: enemy tile is in range
        controller.confirmAttack();
        assert((controller.battle().units()[1].position == jf::GridPos{1, 4}));
        assert(controller.battle().units()[0].weaponFirstHitUsed);
    }
    {
        // Ember Focus: burn on-hit gated to the first hit only per battle.
        jf::Unit mage = makeUnit("mage", jf::Team::Player, {1, 0}, 4, jf::UnitClass::BattleMage);
        mage.stats.magic = 6;
        mage.weapon = {.id = "ember_focus", .name = "Ember Focus", .might = 5, .minRange = 1, .maxRange = 2,
                       .damageType = jf::DamageType::Magical,
                       .onHitStatuses = {jf::StatusEffectType::Burn}, .firstHitOnly = true};
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 1});
        jf::BattleController controller(jf::BattleState({mage, enemy}));
        controller.selectUnit(controller.battle().units().front());
        controller.selectMoveTile({1, 1}); // attack shortcut: enemy tile is in range
        controller.confirmAttack();
        assert(controller.battle().units()[1].burnRemainingProcs > 0);
        assert(controller.battle().units()[0].weaponFirstHitUsed);
    }
    {
        // Resonant Focus: first successful hit each battle also splashes 2
        // fixed damage to enemies directly above/below the target. Battle
        // Mage's own class-innate 魔力波及(arcaneOverflowUsed, kArcaneOverflowSplashDamage=3)
        // fires on the same first hit too - both stack onto the same
        // adjacent tiles (2 + 3 = 5), so the test asserts on the total.
        jf::Unit mage = makeUnit("mage", jf::Team::Player, {1, 0}, 4, jf::UnitClass::BattleMage);
        mage.stats.magic = 6;
        mage.weapon = {.id = "resonant_focus", .name = "Resonant Focus", .might = 4, .minRange = 1, .maxRange = 2,
                       .damageType = jf::DamageType::Magical, .firstHitOnly = true, .splashDamage = 2};
        jf::Unit target = makeUnit("target", jf::Team::Enemy, {1, 1});
        jf::Unit above = makeUnit("above", jf::Team::Enemy, {0, 1});
        jf::BattleController controller(jf::BattleState({mage, target, above}));
        int hpBefore = controller.battle().units()[2].currentHp;
        controller.selectUnit(controller.battle().units().front());
        controller.selectMoveTile({1, 1}); // attack shortcut: target tile is in range
        controller.confirmAttack();
        assert(controller.battle().units()[2].currentHp == hpBefore - 5);
        assert(controller.battle().units()[0].weaponFirstHitUsed);
        assert(controller.battle().units()[0].arcaneOverflowUsed);
    }
    {
        // Hook Lance: hitting from range 2 pulls the target 1 tile toward
        // the attacker.
        jf::Unit guard = makeUnit("guard", jf::Team::Player, {1, 0}, 4, jf::UnitClass::VeteranGuard);
        guard.weapon = {.id = "hook_lance", .name = "Hook Lance", .might = 5, .minRange = 1, .maxRange = 2,
                        .damageType = jf::DamageType::Physical, .pullsAtRangeTwo = true};
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 2});
        jf::BattleController controller(jf::BattleState({guard, enemy}));
        controller.selectUnit(controller.battle().units().front());
        controller.selectMoveTile({1, 2}); // attack shortcut: enemy tile is in range
        controller.confirmAttack();
        assert((controller.battle().units()[1].position == jf::GridPos{1, 1}));
    }
    {
        // Trail Blade: moving through Ash trailblazes it for allies that
        // Player Phase.
        jf::Unit scout = makeUnit("scout", jf::Team::Player, {1, 0}, 4, jf::UnitClass::FrontierScout);
        scout.weapon = {.id = "trail_blade", .name = "Trail Blade", .might = 3, .minRange = 1, .maxRange = 1,
                        .damageType = jf::DamageType::Physical, .moveModifier = 1, .trailblazeOnMove = true};
        jf::BattleController controller(jf::BattleState({scout}));
        controller.battle().setTerrain({1, 1}, jf::TerrainType::Ash);
        assert(!controller.battle().isTrailblazed({1, 1}));
        controller.selectUnit(controller.battle().units().front());
        controller.selectMoveTile({1, 1});
        assert(controller.battle().isTrailblazed({1, 1}));
    }
    {
        // Escort Blade: re-move completing grants DEF+2 to an adjacent ally.
        jf::Unit rider = makeUnit("rider", jf::Team::Player, {1, 0}, 4, jf::UnitClass::MessengerCavalry);
        rider.weapon = {.id = "escort_blade", .name = "Escort Blade", .might = 4, .minRange = 1, .maxRange = 1,
                        .damageType = jf::DamageType::Physical};
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {2, 2});
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 1});
        jf::BattleController controller(jf::BattleState({rider, ally, enemy}));
        controller.selectUnit(controller.battle().units().front());
        controller.selectMoveTile({1, 1}); // attack shortcut: enemy tile is in range
        controller.confirmAttack();
        assert(controller.inputState() == jf::BattleInputState::SelectReMoveTarget);
        controller.selectReMoveTarget({2, 1}); // adjacent to ally at {2,2}
        assert(controller.battle().units()[1].defenseUpActive);
    }

    // M7項目3続き(高コストTier) 武器分岐固有効果 (docs/base_development.md).
    {
        // Command Sword: Formation Bonus radius 1->2, but DEF+1 only to the
        // nearest 2 allies of the Command Sword wielder - a 3rd, strictly
        // farther ally within radius 2 must NOT get the bonus. Both "near"
        // allies sit at distance 1 (no tie with "far3"'s distance 2), so
        // this doesn't depend on the id-string tie-break the implementation
        // uses for same-distance allies.
        jf::Unit captain = makeUnit("captain", jf::Team::Player, {2, 2}, 4, jf::UnitClass::MarchCaptain);
        captain.weapon = {.id = "command_sword", .name = "Command Sword", .might = 4, .minRange = 1, .maxRange = 1,
                          .damageType = jf::DamageType::Physical};
        jf::Unit near1 = makeUnit("near1", jf::Team::Player, {2, 3}); // distance 1
        jf::Unit near2 = makeUnit("near2", jf::Team::Player, {2, 1}); // distance 1
        jf::Unit far3 = makeUnit("far3", jf::Team::Player, {0, 2});   // distance 2, strictly farther
        jf::Unit attacker = makeUnit("attacker", jf::Team::Enemy, {5, 5}, 4);
        jf::BattleController controller(jf::BattleState({captain, near1, near2, far3, attacker}));
        auto& units = controller.battle().units();
        assert(controller.battle().combatDefenseBonus(units[1], units[4]) >= 1); // near1: bonus
        assert(controller.battle().combatDefenseBonus(units[2], units[4]) >= 1); // near2: bonus
        const int bonusOnFar3 = controller.battle().combatDefenseBonus(units[3], units[4]);
        const int bonusOnNear2 = controller.battle().combatDefenseBonus(units[2], units[4]);
        assert(bonusOnFar3 < bonusOnNear2);
    }
    {
        // Guard Sword: once per battle, reduces an adjacent ally's first hit
        // taken by 3; the shield is consumed after that first hit lands.
        jf::Unit swordsman = makeUnit("swordsman", jf::Team::Player, {1, 0}, 4, jf::UnitClass::MarchCaptain);
        swordsman.weapon = {.id = "guard_sword", .name = "Guard Sword", .might = 4, .minRange = 1, .maxRange = 1,
                            .damageType = jf::DamageType::Physical};
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {1, 1}, 4);
        jf::Unit attacker = makeUnit("attacker", jf::Team::Enemy, {1, 2}, 4);
        attacker.stats.strength = 8;
        attacker.weapon = {.id = "iron_sword", .name = "Iron Sword", .might = 5, .minRange = 1, .maxRange = 1,
                           .damageType = jf::DamageType::Physical};
        jf::BattleController controller(jf::BattleState({swordsman, ally, attacker}));
        auto& units = controller.battle().units();
        const int shieldedBonus = controller.battle().combatDefenseBonus(units[1], units[2]);
        int hpBefore = units[1].currentHp;
        jf::resolveAttack(controller.battle(), units[2], units[1], shieldedBonus, true);
        const int shieldedDamage = hpBefore - units[1].currentHp;
        assert(units[0].guardSwordShieldUsed);
        // Shield is spent - a second hit on the same ally gets no reduction.
        hpBefore = units[1].currentHp;
        const int unshieldedBonus = controller.battle().combatDefenseBonus(units[1], units[2]);
        jf::resolveAttack(controller.battle(), units[2], units[1], unshieldedBonus, true);
        const int unshieldedDamage = hpBefore - units[1].currentHp;
        assert(unshieldedDamage == shieldedDamage + 3);
    }
    {
        // Fortress Lance: an enemy moving into its ZoC (range 1) takes -2
        // damage dealt until its next action ends.
        jf::Unit lancer = makeUnit("lancer", jf::Team::Player, {1, 5}, 4, jf::UnitClass::VeteranGuard);
        lancer.weapon = {.id = "fortress_lance", .name = "Fortress Lance", .might = 4, .minRange = 1, .maxRange = 2,
                         .damageType = jf::DamageType::Physical};
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 0}, 4);
        enemy.stats.strength = 6;
        enemy.weapon = {.id = "enemy_sword", .name = "Enemy Sword", .might = 5, .minRange = 1, .maxRange = 1,
                        .damageType = jf::DamageType::Physical};
        jf::Unit victim = makeUnit("victim", jf::Team::Player, {2, 4}, 4);
        jf::BattleState battle({lancer, enemy, victim});
        auto& units = battle.units();
        assert(!units[1].zocEntryDamageDownActive);
        const int baseline = jf::computeDamage(units[1], units[2], 0, 0); // measured BEFORE entering the ZoC
        battle.moveUnit(units[1], {1, 4}); // enters lancer's ZoC (distance 1)
        assert(units[1].zocEntryDamageDownActive);
        const int penalized = jf::computeDamage(units[1], units[2], 0, 0);
        assert(penalized == baseline - 2);
        battle.moveUnit(units[1], {2, 4}); // still adjacent to victim, still inside ZoC (no re-trigger needed)
        assert(units[1].zocEntryDamageDownActive); // still active, not re-triggered or cleared by this move
        assert(jf::computeDamage(units[1], units[2], 0, 0) == baseline - 2);
        battle.markActed(units[1]); // enemy's next action ends
        assert(!units[1].zocEntryDamageDownActive);
        assert(jf::computeDamage(units[1], units[2], 0, 0) == baseline);
    }
    {
        // Patrol Lance: confirming Wait grants DEF+2 until the next Player
        // Phase begins (survives the intervening Enemy Phase).
        jf::Unit lancer = makeUnit("lancer", jf::Team::Player, {1, 0}, 4, jf::UnitClass::VeteranGuard);
        lancer.weapon = {.id = "patrol_lance", .name = "Patrol Lance", .might = 5, .minRange = 1, .maxRange = 2,
                         .damageType = jf::DamageType::Physical};
        jf::BattleController controller(jf::BattleState({lancer}));
        const int baseDef = controller.battle().units()[0].effectiveDefense();
        controller.selectUnit(controller.battle().units().front());
        controller.selectMoveTile(controller.battle().units()[0].position); // no movement
        controller.chooseWait();
        assert(controller.battle().units()[0].patrolLanceReadyDefenseActive);
        assert(controller.battle().units()[0].effectiveDefense() == baseDef + 2);
        controller.battle().beginEnemyPhase();
        assert(controller.battle().units()[0].patrolLanceReadyDefenseActive); // survives Enemy Phase
        controller.battle().beginPlayerPhase();
        assert(!controller.battle().units()[0].patrolLanceReadyDefenseActive); // cleared at next Player Phase
    }
    {
        // Bulwark Maul: confirming Wait grants DEF+2 until this unit's own
        // next action ends (immovable_stance/brace_for_impact's shape) - it
        // re-arms on every Wait confirmation (same as those two skills), so
        // a SECOND Wait would just re-grant it rather than let it expire;
        // a different action kind is what actually clears it (mirroring the
        // immovable_stance test's own use of a non-Wait action to prove
        // this), here a plain Attack.
        jf::Unit maul = makeUnit("maul", jf::Team::Player, {1, 0}, 4, jf::UnitClass::HeavyInfantry);
        maul.weapon = {.id = "bulwark_maul", .name = "Bulwark Maul", .might = 5, .minRange = 1, .maxRange = 1,
                       .damageType = jf::DamageType::Physical};
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 1}, 4);
        jf::BattleController controller(jf::BattleState({maul, enemy}));
        const int baseDef = controller.battle().units()[0].effectiveDefense();
        controller.selectUnit(controller.battle().units().front());
        controller.selectMoveTile(controller.battle().units()[0].position); // no movement
        controller.chooseWait();
        assert(controller.battle().units()[0].bulwarkMaulActive);
        assert(controller.battle().units()[0].effectiveDefense() == baseDef + 2);
        // Let the real Enemy Phase run (rather than calling
        // BattleState::beginPlayerPhase() directly, which would desync
        // BattleController's own inputState_ from the underlying phase -
        // it'd stay stuck wherever chooseWait() left it, e.g. EnemyTurn,
        // and every subsequent controller call would silently no-op against
        // the stale state) so BattleController transitions back to
        // SelectUnit properly for round 2, mirroring the immovable_stance
        // test's own use of this exact update() loop for the same reason.
        for (int i = 0; i < 20 && controller.inputState() == jf::BattleInputState::EnemyTurn; ++i)
            controller.update(1.0f);
        assert(controller.inputState() == jf::BattleInputState::SelectUnit);
        assert(controller.battle().round() == 2);
        assert(controller.battle().units()[0].bulwarkMaulActive); // survived the Enemy Phase
        // This unit's own next action (an Attack, not another Wait) clears
        // it. Re-find the enemy since it may have moved/counter-attacked
        // during the Enemy Phase above.
        jf::Unit* mauler = controller.battle().findUnit("maul");
        jf::Unit* foe = controller.battle().findUnit("enemy");
        assert(mauler && foe && foe->isAlive());
        controller.selectUnit(*mauler);
        controller.selectMoveTile(mauler->position); // no movement
        assert(controller.inputState() == jf::BattleInputState::SelectAction);
        controller.chooseAttack();
        assert(controller.inputState() == jf::BattleInputState::SelectTarget);
        controller.selectTargetTile(foe->position);
        assert(controller.inputState() == jf::BattleInputState::ConfirmAttack);
        controller.confirmAttack();
        assert(!controller.battle().findUnit("maul")->bulwarkMaulActive);
    }
    {
        // Far Standard: 戦旗 radius extended from 2 to 3.
        jf::Unit bearer = makeUnit("bearer", jf::Team::Player, {0, 0}, 4, jf::UnitClass::BannerBearer);
        bearer.weapon = {.id = "far_standard", .name = "Far Standard", .might = 2, .minRange = 1, .maxRange = 2,
                         .damageType = jf::DamageType::Physical};
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {0, 3}, 4); // distance 3
        ally.weapon = {.id = "iron_sword", .name = "Iron Sword", .might = 5, .minRange = 1, .maxRange = 1,
                       .damageType = jf::DamageType::Physical};
        jf::BattleState battle({bearer, ally});
        assert(jf::bannerAuraBonus(battle.units(), battle.units()[1]) == 1);
    }
    {
        // Valor Standard: STR+2 (not the usual +1) for a Physical attacker,
        // but 0 for a Magical attacker ("MAGは上昇させない").
        jf::Unit bearer = makeUnit("bearer", jf::Team::Player, {0, 0}, 4, jf::UnitClass::BannerBearer);
        bearer.weapon = {.id = "valor_standard", .name = "Valor Standard", .might = 5, .minRange = 1, .maxRange = 2,
                         .damageType = jf::DamageType::Physical};
        jf::Unit physicalAlly = makeUnit("physicalAlly", jf::Team::Player, {0, 2}, 4);
        physicalAlly.weapon = {.id = "iron_sword", .name = "Iron Sword", .might = 5, .minRange = 1, .maxRange = 1,
                               .damageType = jf::DamageType::Physical};
        jf::Unit magicAlly = makeUnit("magicAlly", jf::Team::Player, {0, 1}, 4);
        magicAlly.weapon = {.id = "arcane_focus", .name = "Arcane Focus", .might = 6, .minRange = 1, .maxRange = 2,
                            .damageType = jf::DamageType::Magical};
        jf::BattleState battle({bearer, physicalAlly, magicAlly});
        assert(jf::bannerAuraBonus(battle.units(), battle.units()[1]) == 2);
        assert(jf::bannerAuraBonus(battle.units(), battle.units()[2]) == 0);
    }
    {
        // Warding Standard: replaces 戦旗's STR/MAG+1 aura with a defensive
        // DEF+1/RES+1 aura (applied via combatDefenseBonus(), so 0 offense).
        jf::Unit bearer = makeUnit("bearer", jf::Team::Player, {0, 0}, 4, jf::UnitClass::BannerBearer);
        bearer.weapon = {.id = "warding_standard", .name = "Warding Standard", .might = 3, .minRange = 1,
                         .maxRange = 2, .damageType = jf::DamageType::Physical};
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {0, 2}, 4);
        ally.weapon = {.id = "iron_sword", .name = "Iron Sword", .might = 5, .minRange = 1, .maxRange = 1,
                       .damageType = jf::DamageType::Physical};
        jf::Unit attacker = makeUnit("attacker", jf::Team::Enemy, {5, 5}, 4);
        jf::BattleState battle({bearer, ally, attacker});
        assert(jf::bannerAuraBonus(battle.units(), battle.units()[1]) == 0); // no offensive aura
        assert(battle.combatDefenseBonus(battle.units()[1], battle.units()[2]) >= 1); // defensive aura present
    }
    {
        // Withdrawal Blade: after a successful attack, if the target is
        // still alive, re-move 1 tile strictly away from it.
        jf::Unit scout = makeUnit("scout", jf::Team::Player, {1, 1}, 4, jf::UnitClass::FrontierScout);
        scout.stats.maxHp = 20;
        scout.currentHp = 20;
        scout.weapon = {.id = "withdrawal_blade", .name = "Withdrawal Blade", .might = 3, .minRange = 1,
                        .maxRange = 1, .damageType = jf::DamageType::Physical};
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {1, 2}, 4);
        enemy.stats.maxHp = 30;
        enemy.currentHp = 30;
        enemy.stats.defense = 10; // survives the hit (low might, high DEF)
        jf::BattleController controller(jf::BattleState({scout, enemy}));
        controller.selectUnit(controller.battle().units().front());
        controller.selectMoveTile({1, 2}); // attack shortcut: enemy tile is in range
        assert(controller.inputState() == jf::BattleInputState::ConfirmAttack);
        controller.confirmAttack();
        assert(controller.battle().units()[1].isAlive());
        assert(controller.inputState() == jf::BattleInputState::SelectReMoveTarget);
        const int distBefore = jf::manhattanDistance(controller.battle().units()[0].position,
                                                      controller.battle().units()[1].position);
        controller.selectReMoveTarget({1, 0});
        const int distAfter = jf::manhattanDistance(controller.battle().units()[0].position,
                                                     controller.battle().units()[1].position);
        assert(distAfter > distBefore);
        // `scout` is the only Player unit, so finishing its action (the
        // re-move's own markActionResolved()) ends the Player Phase outright
        // rather than returning to SelectUnit for another unit to act.
        assert(controller.inputState() == jf::BattleInputState::EnemyTurn);
    }

    // 連携作戦(docs/character_progression.md「連携作戦」)
    {
        // paired_fallback_line: DEF+2 to both paired units and everyone
        // adjacent to either, until the next Enemy Phase ends; partner isn't
        // marked acted; once per battle.
        jf::Unit leon = makeUnit("leon", jf::Team::Player, {1, 0});
        jf::Unit gareth = makeUnit("gareth", jf::Team::Player, {1, 1});
        jf::Unit adjacentAlly = makeUnit("adjacent_ally", jf::Team::Player, {1, 2}); // adjacent to gareth
        jf::Unit farAlly = makeUnit("far_ally", jf::Team::Player, {5, 5});
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {6, 6});
        jf::BattleController controller(jf::BattleState({leon, gareth, adjacentAlly, farAlly, enemy}));
        controller.battle().setEquippedCooperationId("paired_fallback_line");
        controller.selectUnit(controller.battle().units()[0]); // leon
        controller.selectMoveTile(controller.battle().units()[0].position); // stay put
        assert(controller.canUseCooperation());
        controller.chooseCooperation();
        assert(controller.battle().units()[0].pairedFallbackLineActive); // leon
        assert(controller.battle().units()[1].pairedFallbackLineActive); // gareth
        assert(controller.battle().units()[2].pairedFallbackLineActive); // adjacent_ally
        assert(!controller.battle().units()[3].pairedFallbackLineActive); // far_ally untouched
        assert(controller.battle().units()[0].effectiveDefense() ==
               controller.battle().units()[0].stats.defense + 2);
        assert(!controller.battle().units()[1].hasActed); // partner not marked acted
        assert(controller.battle().units()[0].hasActed); // actor is
        assert(controller.battle().cooperationUsedThisBattle());
        assert(controller.inputState() == jf::BattleInputState::SelectUnit);

        // Second unit's turn: already used this battle, so unavailable even
        // though gareth is also one of the pair.
        controller.selectUnit(controller.battle().units()[1]); // gareth
        assert(!controller.canUseCooperation());
    }
    {
        // paired_braced_breakthrough: forced-move immunity + DEF+2 vs an
        // attacker that moved 2+ tiles this action, for both paired units.
        jf::Unit spear = makeUnit("spear_reserve", jf::Team::Player, {1, 0}, 4, jf::UnitClass::Spearman);
        jf::Unit heavy = makeUnit("heavy_recruit", jf::Team::Player, {1, 1});
        jf::BattleController controller(jf::BattleState({spear, heavy}));
        controller.battle().setEquippedCooperationId("paired_braced_breakthrough");
        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseCooperation();
        assert(controller.battle().units()[0].pairedBracedBreakthroughActive);
        assert(controller.battle().units()[1].pairedBracedBreakthroughActive);

        jf::Unit charger = makeUnit("charger", jf::Team::Enemy, {1, 5}, 6);
        jf::Unit plainHeavy = makeUnit("heavy_plain", jf::Team::Player, {1, 1}); // no flag, for comparison
        jf::BattleState combatCheck({controller.battle().units()[1], plainHeavy, charger});
        combatCheck.units()[0].pairedBracedBreakthroughActive = true; // heavy (flagged)
        combatCheck.units()[2].tilesMovedThisAction = 3; // charger moved 2+ tiles this action
        const int flaggedDefense = combatCheck.combatDefenseBonus(combatCheck.units()[0], combatCheck.units()[2]);
        const int plainDefense = combatCheck.combatDefenseBonus(combatCheck.units()[1], combatCheck.units()[2]);
        assert(flaggedDefense == plainDefense + 2);

        jf::Unit knockAttacker = makeUnit("knock_attacker", jf::Team::Enemy, {1, 0});
        jf::BattleState knockCheck({heavy, knockAttacker});
        knockCheck.units()[0].pairedBracedBreakthroughActive = true;
        const jf::GridPos before = knockCheck.units()[0].position;
        knockCheck.applyKnockback(knockCheck.units()[1], knockCheck.units()[0]);
        assert(knockCheck.units()[0].position == before); // immune
    }
    {
        // paired_field_recovery: heal 8 + cure poison (present) within
        // distance 2; partner not marked acted.
        jf::Unit mira = makeUnit("mira", jf::Team::Player, {1, 0});
        jf::Unit ranger = makeUnit("ranger_recruit", jf::Team::Player, {1, 1});
        jf::Unit wounded = makeUnit("wounded_ally", jf::Team::Player, {1, 2});
        wounded.currentHp = 5;
        wounded.poisonRemainingProcs = 3;
        wounded.burnRemainingProcs = 2;
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {5, 5}); // keeps the battle from auto-Victory
        jf::BattleController controller(jf::BattleState({mira, ranger, wounded, enemy}));
        controller.battle().setEquippedCooperationId("paired_field_recovery");
        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        controller.chooseCooperation();
        assert(controller.inputState() == jf::BattleInputState::SelectCooperationTarget);
        assert(contains(controller.cooperationTargetTiles(), {1, 2}));
        controller.selectCooperationTarget({1, 2});
        assert(controller.battle().units()[2].currentHp == 13); // 5 + 8
        assert(controller.battle().units()[2].poisonRemainingProcs == 0); // cured (poison prioritized)
        assert(controller.battle().units()[2].burnRemainingProcs == 2); // burn left uncured (doc doesn't specify both)
        assert(!controller.battle().units()[1].hasActed);
        assert(controller.battle().cooperationUsedThisBattle());
        assert(controller.inputState() == jf::BattleInputState::SelectUnit);
    }
    {
        // paired_rapid_works: places a durability-6 barrier within distance 2
        // and grants cavalry_recruit a normal re-move, regardless of which of
        // the pair acted.
        jf::Unit engineer = makeUnit("engineer_recruit", jf::Team::Player, {1, 0});
        jf::Unit cavalry = makeUnit("cavalry_recruit", jf::Team::Player, {1, 2});
        jf::Unit enemy = makeUnit("enemy", jf::Team::Enemy, {6, 6}); // keeps the battle from auto-Victory
        jf::BattleController controller(jf::BattleState({engineer, cavalry, enemy}));
        controller.battle().registerObjectDefinition(
            jf::BattleObjectDefinition{.definitionId = "rapid_barricade", .kind = jf::BattleObjectKind::Barrier,
                                       .maxDurability = 6, .blocksMovement = true, .canBeAttacked = true,
                                       .canBeRepaired = true});
        controller.battle().setEquippedCooperationId("paired_rapid_works");
        controller.selectUnit(controller.battle().units()[0]); // engineer (the actor)
        controller.selectMoveTile(controller.battle().units()[0].position);
        assert(controller.canUseCooperation());
        controller.chooseCooperation();
        assert(controller.inputState() == jf::BattleInputState::SelectCooperationTarget);
        assert(!controller.cooperationTargetTiles().empty());
        const jf::GridPos barrierPos = controller.cooperationTargetTiles().front();
        controller.selectCooperationTarget(barrierPos);
        const jf::BattleObjectState* placed = controller.battle().objectAt(barrierPos);
        assert(placed != nullptr);
        assert(placed->definitionId == "rapid_barricade");
        assert(placed->durability == 6);
        // cavalry_recruit (the OTHER paired unit, not the actor) gets the re-move
        assert(controller.inputState() == jf::BattleInputState::SelectCooperationCavalryReMoveTarget);
        assert(!controller.battle().findUnit("cavalry_recruit")->hasActed);
        const jf::GridPos cavalryBefore = controller.battle().findUnit("cavalry_recruit")->position;
        assert(!controller.cooperationCavalryReMoveTiles().empty());
        const jf::GridPos cavalryDest = controller.cooperationCavalryReMoveTiles().front();
        controller.selectCooperationCavalryReMoveTarget(cavalryDest);
        if (cavalryDest != cavalryBefore)
            assert(controller.battle().findUnit("cavalry_recruit")->position == cavalryDest);
        assert(!controller.battle().findUnit("cavalry_recruit")->hasActed); // re-move never marks acted
        assert(controller.battle().findUnit("engineer_recruit")->hasActed); // actor's own action did conclude
        assert(controller.inputState() == jf::BattleInputState::SelectUnit);
    }
    {
        // paired_cross_observation stays registered but never resolves to a
        // battle effect (deferred gap - see Cooperation.hpp's own comment).
        jf::Unit erin = makeUnit("erin", jf::Team::Player, {1, 0}, 4, jf::UnitClass::WatchArcher);
        jf::Unit scoutReserve = makeUnit("scout_reserve", jf::Team::Player, {1, 1}, 4, jf::UnitClass::FrontierScout);
        jf::BattleController controller(jf::BattleState({erin, scoutReserve}));
        controller.battle().setEquippedCooperationId("paired_cross_observation");
        controller.selectUnit(controller.battle().units()[0]);
        controller.selectMoveTile(controller.battle().units()[0].position);
        assert(!controller.canUseCooperation());
        controller.chooseCooperation(); // no-op
        assert(controller.inputState() == jf::BattleInputState::SelectAction);
        assert(!controller.battle().cooperationUsedThisBattle());
    }
    {
        // Unlock gating (jf::isCooperationUnlocked()): approximated onto
        // region safe-return completion, same style as heavy_recruit/
        // cavalry_recruit's own "加入候補確定" approximation.
        jf::BaseState base;
        assert(!jf::isCooperationUnlocked("paired_fallback_line", base));
        assert(!jf::isCooperationUnlocked("paired_signal_ward", base)); // BuriedDawnSanctum not cleared yet
        base.completedRegionIds.insert(jf::RegionId::CinderwatchGate);
        assert(jf::isCooperationUnlocked("paired_fallback_line", base));
        base.completedRegionIds.insert(jf::RegionId::WindscarPlateau);
        assert(jf::isCooperationUnlocked("paired_rapid_works", base));
        assert(!jf::isCooperationUnlocked("paired_signal_ward", base)); // still not reachable
        // This Slice (Ember Ravine region-clear) added RegionId::
        // BuriedDawnSanctum, resolving the "region has no enum value"
        // blocker this pair was previously stuck on - it's now reachable
        // the same way every other region-gated pair already is.
        base.completedRegionIds.insert(jf::RegionId::BuriedDawnSanctum);
        assert(jf::isCooperationUnlocked("paired_signal_ward", base));
    }

    {
        // docs/regions/ember_ravine.md「地点構成」: 8-site skeleton + 3 camps
        // + the site 3/4 either-order-but-both-required branch, mirror of
        // the Windscar/Old Frontier Settlement skeleton tests above.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor emberRegion = jf::regionDescriptor(jf::RegionId::EmberRavine, *data);
        assert(emberRegion.stages.size() == 8);
        assert(emberRegion.stages[0].id == "ember_ravine_entrance");

        const jf::RegionRouteGraph& emberRoute = jf::regionRouteGraph(jf::RegionId::EmberRavine);
        std::string error;
        assert(jf::validateRouteGraph(emberRoute, &error));
        assert(jf::findRouteNode(emberRoute, "sulfur_hollow"));
        assert(jf::findRouteNode(emberRoute, "ravine_cooling_channel"));
        const jf::RouteNodeDefinition* branch = jf::findRouteNode(emberRoute, "ember_ravine_sulfur_channel_branch");
        assert(branch && branch->kind == jf::RouteNodeKind::BranchGroup &&
               branch->branchCompletion == jf::BranchCompletion::AllMembers);
    }

    {
        // docs/regions/ember_ravine.md「1. 焼け石の入口」: 主目的(岩蜥蜴4体、
        // firstBurnNegated)・副目標(遮熱標識で行動終了)・3探索ルート(熱量1/
        // HP-2+敵1体減+耐熱素材-1/[重装兵]防護Object+耐熱素材+1)・勝利報酬
        // (耐熱素材2、鉄鉱石1)。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor emberRegion = jf::regionDescriptor(jf::RegionId::EmberRavine, *data);
        const jf::StageDescriptor& entranceStage = emberRegion.stages[0];
        assert(entranceStage.enemyRoster.size() == 4);
        for (const jf::UnitTemplate& enemy : entranceStage.enemyRoster) assert(enemy.firstBurnNegated);
        assert(entranceStage.surveyObjectiveId == "ember_ravine_entrance_heat_marker");
        assert(entranceStage.scoutRouteRequiredClass == jf::UnitClass::HeavyInfantry);

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(entranceStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "heat_resistant_material") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "iron") == 1);
        assert(findLoot(lootFor(jf::ExplorationChoice::CollapsedSidePath), "heat_resistant_material") == 1);
        assert(findLoot(lootFor(jf::ExplorationChoice::ScoutRoute), "heat_resistant_material") == 3);

        const jf::ExplorationOutcome frontalOutcome = jf::stageRouteOutcome(entranceStage,
                                                                            jf::ExplorationChoice::FrontalAdvance);
        assert(frontalOutcome.startingHeatLevel == 1);
        const jf::ExplorationOutcome rushOutcome = jf::stageRouteOutcome(entranceStage,
                                                                         jf::ExplorationChoice::CollapsedSidePath);
        assert(rushOutcome.partyDamage == 2 && rushOutcome.enemiesRemoved == 1);

        jf::BattleState battle = jf::createScenarioBattle(*data, entranceStage, /*seed=*/5, frontalOutcome);
        assert(battle.heatLevel() == 1);
        int enemyCount = 0;
        for (const jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Enemy) ++enemyCount;
        assert(enemyCount == 4);
    }

    {
        // docs/regions/ember_ravine.md「2. 熱風の棚道」: mirror of
        // windscar_relay's own guest-escort tests - the guest reaching the
        // escape tile wins standalone (primary is EscapeUnits, not
        // EliminateTeam), and losing the guest is Defeat independent of the
        // player squad's own state. Also covers the 3 explored routes'
        // enemy/loot deltas (敵4/3/4体、耐熱素材-1 on route 2).
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor emberRegion = jf::regionDescriptor(jf::RegionId::EmberRavine, *data);
        const jf::StageDescriptor* ledgeStage = nullptr;
        for (const jf::StageDescriptor& stage : emberRegion.stages)
            if (stage.id == "ember_ravine_ledge") ledgeStage = &stage;
        assert(ledgeStage && ledgeStage->guestUnits.size() == 1);
        assert(ledgeStage->enemyRoster.size() == 4);
        assert(ledgeStage->scoutRouteRequiredClass == jf::UnitClass::FrontierEngineer);

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(*ledgeStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "heat_resistant_material") == 1);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "sulfur") == 1);
        assert(findLoot(lootFor(jf::ExplorationChoice::CollapsedSidePath), "heat_resistant_material") == 0);
        assert(findLoot(lootFor(jf::ExplorationChoice::CollapsedSidePath), "sulfur") == 1);

        const jf::ExplorationOutcome sideOutcome = jf::stageRouteOutcome(*ledgeStage,
                                                                         jf::ExplorationChoice::CollapsedSidePath);
        assert(sideOutcome.enemiesRemoved == 1);

        jf::BattleState battle = jf::createScenarioBattle(*data, *ledgeStage, /*seed=*/7);
        assert(battle.missionState().guestUnitIds.size() == 1);
        for (const jf::Unit& unit : battle.units())
            if (unit.isGuest) assert(unit.team == jf::Team::Player);

        const jf::ObjectiveDefinition* escapeDef = nullptr;
        for (const auto& def : battle.missionState().definitions)
            if (def.id == "ember_ravine_ledge_escape") escapeDef = &def;
        assert(escapeDef && escapeDef->primary && escapeDef->kind == jf::ObjectiveKind::EscapeUnits);

        const std::string& guestId = battle.missionState().guestUnitIds[0];
        jf::BattleEvent guestEscapes{
            1, 1,
            jf::ActionResolvedEvent{1, guestId, jf::Team::Player, jf::ActionKind::Wait, escapeDef->target.tile}};
        jf::handleObjectiveEvent(battle.missionState(), guestEscapes);
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);
    }

    {
        // docs/regions/ember_ravine.md「2. 熱風の棚道」敗北条件「護衛対象の
        // 撤退」: allGuestsLost() fires Defeat even with the player squad
        // fully alive, same shape as windscar_relay's own test.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor emberRegion = jf::regionDescriptor(jf::RegionId::EmberRavine, *data);
        const jf::StageDescriptor* ledgeStage = nullptr;
        for (const jf::StageDescriptor& stage : emberRegion.stages)
            if (stage.id == "ember_ravine_ledge") ledgeStage = &stage;
        assert(ledgeStage);

        jf::BattleState battle = jf::createScenarioBattle(*data, *ledgeStage, /*seed=*/7);
        assert(!battle.allGuestsLost());
        for (jf::Unit& unit : battle.units())
            if (unit.isGuest) unit.currentHp = 0;
        assert(battle.allGuestsLost());
        assert(!battle.allPlayersDefeated());
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Defeat);
    }

    {
        // docs/regions/ember_ravine.md「3. 硫黄窪地」: 主目的(3ラウンド防衛、
        // または敵全滅)・敵構成(岩蜥蜴3/熱地弓兵1、深部ルートで+1)・採取者
        // 1人・熱量(ルート別1/2/1)・報酬(硫黄2、耐熱素材1、深部ルートで硫黄
        // +2、衛生兵ルートで耐熱素材+1)。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor emberRegion = jf::regionDescriptor(jf::RegionId::EmberRavine, *data);
        const jf::StageDescriptor* sulfurStage = nullptr;
        for (const jf::StageDescriptor& stage : emberRegion.stages)
            if (stage.id == "sulfur_hollow") sulfurStage = &stage;
        assert(sulfurStage);
        assert(sulfurStage->enemyRoster.size() == 5);
        // 岩蜥蜴(index 0-2, and index 4 for the deep-route 5th one): firstBurnNegated.
        // 熱地弓兵(index 3): not affected.
        assert(sulfurStage->enemyRoster[0].firstBurnNegated);
        assert(sulfurStage->enemyRoster[1].firstBurnNegated);
        assert(sulfurStage->enemyRoster[2].firstBurnNegated);
        assert(!sulfurStage->enemyRoster[3].firstBurnNegated);
        assert(sulfurStage->enemyRoster[4].firstBurnNegated);
        assert(sulfurStage->guestUnits.size() == 1);
        assert(sulfurStage->scoutRouteRequiredClass == jf::UnitClass::DawnChirurgeon);
        assert(sulfurStage->primarySurviveRoundsAlternative &&
              sulfurStage->primarySurviveRoundsAlternative->id == "sulfur_hollow_defense" &&
              sulfurStage->primarySurviveRoundsAlternative->surviveUntilRound == 3);
        assert(sulfurStage->secondaryProtectUnitAlternative &&
              sulfurStage->secondaryProtectUnitAlternative->id == "sulfur_hollow_protect_gatherer" &&
              sulfurStage->secondaryProtectUnitAlternative->unitId == "sulfur_hollow_gatherer");

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(*sulfurStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "sulfur") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "heat_resistant_material") == 1);
        assert(findLoot(lootFor(jf::ExplorationChoice::CollapsedSidePath), "sulfur") == 4);
        assert(findLoot(lootFor(jf::ExplorationChoice::ScoutRoute), "heat_resistant_material") == 2);

        const jf::ExplorationOutcome frontalOutcome =
            jf::stageRouteOutcome(*sulfurStage, jf::ExplorationChoice::FrontalAdvance);
        assert(frontalOutcome.startingHeatLevel == 1 && frontalOutcome.enemiesRemoved == 1);
        const jf::ExplorationOutcome deepOutcome =
            jf::stageRouteOutcome(*sulfurStage, jf::ExplorationChoice::CollapsedSidePath);
        assert(deepOutcome.startingHeatLevel == 2 && deepOutcome.enemiesRemoved == 0);

        jf::BattleState frontal = jf::createScenarioBattle(*data, *sulfurStage, /*seed=*/5, frontalOutcome);
        assert(frontal.heatLevel() == 1);
        int frontalEnemies = 0;
        for (const jf::Unit& unit : frontal.units())
            if (unit.team == jf::Team::Enemy) ++frontalEnemies;
        assert(frontalEnemies == 4);

        jf::BattleState deep = jf::createScenarioBattle(*data, *sulfurStage, /*seed=*/5, deepOutcome);
        assert(deep.heatLevel() == 2);
        int deepEnemies = 0;
        for (const jf::Unit& unit : deep.units())
            if (unit.team == jf::Team::Enemy) ++deepEnemies;
        assert(deepEnemies == 5);
    }

    {
        // docs/regions/ember_ravine.md「3. 硫黄窪地」の主目的「3ラウンド防衛、
        // または敵全滅」: primarySurviveRoundsAlternative wins the stage on
        // round threshold alone, same shape as herb_islet/
        // settlement_common_well.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor emberRegion = jf::regionDescriptor(jf::RegionId::EmberRavine, *data);
        const jf::StageDescriptor* sulfurStage = nullptr;
        for (const jf::StageDescriptor& stage : emberRegion.stages)
            if (stage.id == "sulfur_hollow") sulfurStage = &stage;
        assert(sulfurStage);

        jf::BattleState battle = jf::createScenarioBattle(*data, *sulfurStage, /*seed=*/7);
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind != jf::BattleOutcomeKind::Victory);
        while (battle.round() <= 3) {
            battle.beginEnemyPhase();
            battle.beginPlayerPhase();
        }
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);
    }

    {
        // docs/regions/ember_ravine.md「3. 硫黄窪地」敗北条件「採取者撤退」:
        // allGuestsLost() fires Defeat independent of the player squad's own
        // state, same shape as ember_ravine_ledge/settlement_common_well.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor emberRegion = jf::regionDescriptor(jf::RegionId::EmberRavine, *data);
        const jf::StageDescriptor* sulfurStage = nullptr;
        for (const jf::StageDescriptor& stage : emberRegion.stages)
            if (stage.id == "sulfur_hollow") sulfurStage = &stage;
        assert(sulfurStage);

        jf::BattleState battle = jf::createScenarioBattle(*data, *sulfurStage, /*seed=*/7);
        assert(battle.missionState().guestUnitIds.size() == 1);
        assert(!battle.allGuestsLost());
        for (jf::Unit& unit : battle.units())
            if (unit.isGuest) unit.currentHp = 0;
        assert(battle.allGuestsLost());
        assert(!battle.allPlayersDefeated());
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Defeat);
    }

    {
        // docs/regions/ember_ravine.md「3. 硫黄窪地」副目標「採取者を撤退させ
        // ない」: the FIRST real ObjectiveKind::ProtectUnit Definition in the
        // codebase - Active while the gatherer is present, Failed the
        // instant it's lost (falling edge), and does NOT itself drive
        // Victory/Defeat (that's still allGuestsLost() above).
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor emberRegion = jf::regionDescriptor(jf::RegionId::EmberRavine, *data);
        const jf::StageDescriptor* sulfurStage = nullptr;
        for (const jf::StageDescriptor& stage : emberRegion.stages)
            if (stage.id == "sulfur_hollow") sulfurStage = &stage;
        assert(sulfurStage);

        jf::BattleState battle = jf::createScenarioBattle(*data, *sulfurStage, /*seed=*/7);
        const jf::ObjectiveDefinition* protectDef = nullptr;
        for (const auto& def : battle.missionState().definitions)
            if (def.id == "sulfur_hollow_protect_gatherer") protectDef = &def;
        assert(protectDef && !protectDef->primary && protectDef->kind == jf::ObjectiveKind::ProtectUnit &&
              protectDef->groupId == "sulfur_hollow_protect_gatherer" &&
              protectDef->target.unitId == "sulfur_hollow_gatherer");
        assert(battle.missionState().progress.at("sulfur_hollow_protect_gatherer").status ==
              jf::ObjectiveStatus::Active);

        jf::syncObjectiveProgress(battle);
        assert(battle.missionState().progress.at("sulfur_hollow_protect_gatherer").status ==
              jf::ObjectiveStatus::Active); // gatherer still present, still Active

        jf::Unit* gatherer = battle.findUnit("sulfur_hollow_gatherer");
        assert(gatherer);
        gatherer->currentHp = 0;
        jf::syncObjectiveProgress(battle);
        assert(battle.missionState().progress.at("sulfur_hollow_protect_gatherer").status ==
              jf::ObjectiveStatus::Failed);
    }

    {
        // docs/regions/ember_ravine.md「4. 破損冷却水路」: JSON-authored via
        // stageDescriptorFromContent(), same operateObjectiveId pattern as
        // sunken_sluice (M9-J) - primary approximated as OperateObject-only,
        // the "OperateObject AND SurviveRounds(2)" AND-composition is
        // deferred (same category of gap M9-J/M9-D/M9-M already recorded).
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor emberRegion = jf::regionDescriptor(jf::RegionId::EmberRavine, *data);
        const jf::StageDescriptor* channelStage = nullptr;
        for (const jf::StageDescriptor& stage : emberRegion.stages)
            if (stage.id == "ravine_cooling_channel") channelStage = &stage;
        assert(channelStage);
        assert(channelStage->enemyRoster.size() == 5);
        assert(channelStage->scoutRouteRequiredClass == jf::UnitClass::FrontierEngineer);
        assert(channelStage->objectPlacementRules.size() == 1);
        assert(channelStage->objectPlacementRules[0].operateObjectiveId == "operate_cooling_valve");

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(*channelStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "iron") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "heat_resistant_material") == 1);
        assert(findLoot(lootFor(jf::ExplorationChoice::CollapsedSidePath), "iron") == 1); // 鉄鉱石-1

        const jf::ExplorationOutcome breakOutcome =
            jf::stageRouteOutcome(*channelStage, jf::ExplorationChoice::CollapsedSidePath);
        assert(breakOutcome.startingHeatLevel == 0); // 熱量を即座に0

        jf::BattleState battle = jf::createScenarioBattle(*data, *channelStage, /*seed=*/9);
        const jf::ObjectiveDefinition* operateDef = nullptr;
        for (const auto& def : battle.missionState().definitions)
            if (def.kind == jf::ObjectiveKind::OperateObject) operateDef = &def;
        assert(operateDef && operateDef->primary && operateDef->groupId == "primary");
    }

    {
        // docs/regions/ember_ravine.md「5. 灰晶採取棚」: 主目的「灰晶箱1個以上を
        // 確保」はM9-H(黒水低湿地地点4「樹脂箱2個のうち1個以上」)と同じ
        // crate-primary-approximation - 標準EliminateTeamへ近似し、灰晶箱は
        // surveyObjectiveId経由の secondary/bonus-reward パスとして残す。副目標
        // 「採取地点2個を操作」は初めてJSON Schema経由で配線した
        // secondaryOperateObjectiveIdの実例(old_frontier_settlementの警鐘は
        // これまでRegion.cpp手書き専用だった)。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor emberRegion = jf::regionDescriptor(jf::RegionId::EmberRavine, *data);
        const jf::StageDescriptor* shelfStage = nullptr;
        for (const jf::StageDescriptor& stage : emberRegion.stages)
            if (stage.id == "ash_crystal_shelf") shelfStage = &stage;
        assert(shelfStage);
        assert(shelfStage->enemyRoster.size() == 5); // 岩蜥蜴4 + 深部ルート専用の大型個体1
        assert(shelfStage->scoutRouteRequiredClass == jf::UnitClass::BattleMage);
        assert(shelfStage->surveyObjectiveId == "ash_crystal_shelf_crate");
        assert(shelfStage->objectPlacementRules.size() == 1);
        assert(shelfStage->objectPlacementRules[0].count == 2);
        assert(shelfStage->objectPlacementRules[0].secondaryOperateObjectiveId ==
               "ash_crystal_shelf_gather_points");

        const jf::ExplorationOutcome frontal =
            jf::stageRouteOutcome(*shelfStage, jf::ExplorationChoice::FrontalAdvance);
        assert(frontal.enemiesRemoved == 1); // 敵5-1=4体
        const jf::ExplorationOutcome deep =
            jf::stageRouteOutcome(*shelfStage, jf::ExplorationChoice::CollapsedSidePath);
        assert(deep.enemiesRemoved == 0 && deep.startingHeatLevel == 2); // 熱量2、敵5体
        const jf::ExplorationOutcome scout =
            jf::stageRouteOutcome(*shelfStage, jf::ExplorationChoice::ScoutRoute);
        assert(scout.enemiesRemoved == 1); // 敵4体、噴気予告1回無効はno-op(前例なしのため見送り)

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(*shelfStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "ash_crystal") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "sulfur") == 1);
        assert(findLoot(lootFor(jf::ExplorationChoice::CollapsedSidePath), "ash_crystal") == 4);

        jf::BattleState battle = jf::createScenarioBattle(*data, *shelfStage, /*seed=*/11);
        const jf::ObjectiveDefinition* eliminateDef = nullptr;
        std::vector<const jf::ObjectiveDefinition*> gatherDefs;
        for (const auto& def : battle.missionState().definitions) {
            if (def.kind == jf::ObjectiveKind::EliminateTeam) eliminateDef = &def;
            if (def.kind == jf::ObjectiveKind::OperateObject && def.groupId == "ash_crystal_shelf_gather_points")
                gatherDefs.push_back(&def);
        }
        assert(eliminateDef && eliminateDef->primary && eliminateDef->groupId == "primary");
        assert(gatherDefs.size() == 2);
        for (const jf::ObjectiveDefinition* def : gatherDefs) assert(!def->primary);
        bool hasGroup = false;
        for (const auto& group : battle.missionState().groups)
            if (group.id == "ash_crystal_shelf_gather_points" && group.rule == jf::ObjectiveGroupRule::Any)
                hasGroup = true;
        assert(hasGroup);
    }

    {
        // docs/regions/ember_ravine.md「6. 旧耐熱工房」: 主目的「冷却炉を
        // 停止し、記録箱1個以上を確保」はsunken_sluice(M9-J)/
        // ravine_cooling_channel(M9-AC)と同型のOperateObject-primary近似
        // (crateとのAND合成は見送り、記録箱はsurveyObjectiveId経由の
        // secondary/bonusパスへ)。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor emberRegion = jf::regionDescriptor(jf::RegionId::EmberRavine, *data);
        const jf::StageDescriptor* shopStage = nullptr;
        for (const jf::StageDescriptor& stage : emberRegion.stages)
            if (stage.id == "heatwork_shop") shopStage = &stage;
        assert(shopStage);
        assert(shopStage->enemyRoster.size() == 5); // 斧兵2+弓兵2+工兵型1
        assert(shopStage->scoutRouteRequiredClass == jf::UnitClass::HeavyInfantry);
        assert(shopStage->surveyObjectiveId == "heatwork_shop_crate");
        assert(shopStage->surveyTileCount == 2);
        assert(shopStage->objectPlacementRules.size() == 1);
        assert(shopStage->objectPlacementRules[0].operateObjectiveId == "operate_cooling_furnace");

        const jf::ExplorationOutcome frontal =
            jf::stageRouteOutcome(*shopStage, jf::ExplorationChoice::FrontalAdvance);
        assert(frontal.enemiesRemoved == 0); // 敵5体
        const jf::ExplorationOutcome coolFirst =
            jf::stageRouteOutcome(*shopStage, jf::ExplorationChoice::CollapsedSidePath);
        assert(coolFirst.enemiesRemoved == 1 && coolFirst.startingHeatLevel == 0); // 熱量0、敵4体
        const jf::ExplorationOutcome brace =
            jf::stageRouteOutcome(*shopStage, jf::ExplorationChoice::ScoutRoute);
        assert(brace.enemiesRemoved == 0); // 敵5体、炉扉耐久+5はObject耐久ギャップでno-op

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(*shopStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "heat_resistant_material") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "iron") == 1);

        jf::BattleState battle = jf::createScenarioBattle(*data, *shopStage, /*seed=*/13);
        const jf::ObjectiveDefinition* operateDef = nullptr;
        std::vector<const jf::ObjectiveDefinition*> crateDefs;
        for (const auto& def : battle.missionState().definitions) {
            if (def.kind == jf::ObjectiveKind::OperateObject) operateDef = &def;
            if (def.groupId == "heatwork_shop_crate") crateDefs.push_back(&def);
        }
        assert(operateDef && operateDef->primary && operateDef->groupId == "primary");
        assert(crateDefs.size() == 2);
        for (const jf::ObjectiveDefinition* def : crateDefs) assert(!def->primary);
        bool hasCrateGroup = false;
        for (const auto& group : battle.missionState().groups)
            if (group.id == "heatwork_shop_crate" && group.rule == jf::ObjectiveGroupRule::Any)
                hasCrateGroup = true;
        assert(hasCrateGroup);
    }

    {
        // docs/regions/ember_ravine.md「7. 灰封観測所」: 主目的「記録箱2個の
        // うち1個以上を左端へ運ぶ」は標準EliminateTeam-primary + crate
        // secondary近似(ash_crystal_shelf/heatwork_shopと同型)。「運ぶ」は
        // 未実装、「確保」のみモデル化。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor emberRegion = jf::regionDescriptor(jf::RegionId::EmberRavine, *data);
        const jf::StageDescriptor* obsStage = nullptr;
        for (const jf::StageDescriptor& stage : emberRegion.stages)
            if (stage.id == "ashsealed_observatory") obsStage = &stage;
        assert(obsStage);
        assert(obsStage->enemyRoster.size() == 5); // 岩蜥蜴3+採取団2
        assert(obsStage->scoutRouteRequiredClass == jf::UnitClass::BattleMage);
        assert(obsStage->surveyObjectiveId == "ashsealed_observatory_crate");
        assert(obsStage->surveyTileCount == 2);

        const jf::ExplorationOutcome frontal =
            jf::stageRouteOutcome(*obsStage, jf::ExplorationChoice::FrontalAdvance);
        assert(frontal.enemiesRemoved == 0); // 敵5体、6ラウンド制限はround-limit
                                              // defeatギャップとしてno-op
        const jf::ExplorationOutcome windowFirst =
            jf::stageRouteOutcome(*obsStage, jf::ExplorationChoice::CollapsedSidePath);
        assert(windowFirst.startingHeatLevel == 0); // 熱量-1の近似(0が床)、
                                                     // 記録箱1個減少は
                                                     // per-route箱数ギャップで
                                                     // no-op(箱は常に2個)
        const jf::ExplorationOutcome reading =
            jf::stageRouteOutcome(*obsStage, jf::ExplorationChoice::ScoutRoute);
        assert(reading.enemiesRemoved == 0); // 敵配置全公開は元々fog-of-war
                                              // 皆無でno-op、記録箱2個は元々一致

        jf::BattleState battle = jf::createScenarioBattle(*data, *obsStage, /*seed=*/17);
        const jf::ObjectiveDefinition* eliminateDef = nullptr;
        std::vector<const jf::ObjectiveDefinition*> crateDefs;
        for (const auto& def : battle.missionState().definitions) {
            if (def.kind == jf::ObjectiveKind::EliminateTeam) eliminateDef = &def;
            if (def.groupId == "ashsealed_observatory_crate") crateDefs.push_back(&def);
        }
        assert(eliminateDef && eliminateDef->primary && eliminateDef->groupId == "primary");
        assert(crateDefs.size() == 2);
        for (const jf::ObjectiveDefinition* def : crateDefs) assert(!def->primary);
        bool hasCrateGroup = false;
        for (const auto& group : battle.missionState().groups)
            if (group.id == "ashsealed_observatory_crate" && group.rule == jf::ObjectiveGroupRule::Any)
                hasCrateGroup = true;
        assert(hasCrateGroup);
    }

    {
        // docs/regions/ember_ravine.md「地点構成」: 地点3・地点4は"どちらを
        // 先に攻略してもよい、両方必須"の either-order branch - completing
        // BOTH members gates Camp II, mirroring every prior region's own
        // either-order-branch gate (e.g. Blackwater/Old Frontier Settlement).
        const jf::RegionRouteGraph& emberRoute = jf::regionRouteGraph(jf::RegionId::EmberRavine);
        const jf::RouteNodeDefinition* branch = jf::findRouteNode(emberRoute, "ember_ravine_sulfur_channel_branch");
        assert(branch && branch->branchCompletion == jf::BranchCompletion::AllMembers);
        assert(branch->branchMembers.size() == 2);
        bool hasSulfur = false, hasChannel = false;
        for (const std::string& member : branch->branchMembers) {
            if (member == "sulfur_hollow") hasSulfur = true;
            if (member == "ravine_cooling_channel") hasChannel = true;
        }
        assert(hasSulfur && hasChannel);
        const jf::RouteNodeDefinition* camp2 = jf::findRouteNode(emberRoute, "ember_ravine_camp2");
        assert(camp2 && camp2->kind == jf::RouteNodeKind::Camp);
    }

    {
        // docs/regions/ember_ravine.md「赤背の大蜥蜴」「尾払い」: front-3
        // pattern, STR+1 physical + 1-tile knockback, fires only once 2+
        // players are in front of the boss (same shape as
        // takeSerpentBossTurn()'s own 締め付け test above).
        jf::Unit lizard = makeUnit("lizard", jf::Team::Enemy, {1, 2}, 4, jf::UnitClass::RedbackLizard);
        lizard.stats.strength = 10;
        lizard.stats.defense = 8;
        lizard.stats.resistance = 3;
        lizard.stats.maxHp = 64;
        lizard.currentHp = 64;
        // Both in the front-3 column (col 1, rows 0/1/2) toward the lizard's
        // west side - kGridRows == 3, so this covers every row.
        jf::Unit allyA = makeUnit("allyA", jf::Team::Player, {0, 1});
        jf::Unit allyB = makeUnit("allyB", jf::Team::Player, {2, 1});
        jf::BattleState battle({allyA, allyB, lizard});

        jf::takeEnemyTurn(battle, battle.units()[2]);
        assert(battle.units()[0].currentHp < battle.units()[0].stats.maxHp);
        assert(battle.units()[1].currentHp < battle.units()[1].stats.maxHp);
        assert(battle.units()[0].position != (jf::GridPos{0, 1})); // knocked back off its starting tile
        assert(battle.units()[1].position != (jf::GridPos{2, 1})); // knocked back off its starting tile
    }

    {
        // docs/regions/ember_ravine.md「赤背の大蜥蜴」「噴気誘導」: HP<=50%で
        // 一度だけ、空き2マスをFumeWarning化する(同型: takeGrubwormBossTurn()の
        // 崩落誘発/takeSerpentBossTurn()の激しい身震い)。
        jf::Unit lizard = makeUnit("lizard", jf::Team::Enemy, {2, 2}, 4, jf::UnitClass::RedbackLizard);
        lizard.stats.strength = 10;
        lizard.stats.defense = 8;
        lizard.stats.resistance = 3;
        lizard.stats.maxHp = 64;
        lizard.currentHp = 32; // exactly 50%
        jf::Unit ally = makeUnit("ally", jf::Team::Player, {2, 6}); // far away, no melee interaction
        jf::BattleState battle({ally, lizard});

        jf::takeEnemyTurn(battle, battle.units()[1]);
        assert(battle.units()[1].bossFumeLureUsed);
        int fumeCount = 0;
        for (int row = 0; row < jf::kGridRows; ++row)
            for (int col = 0; col < jf::kGridCols; ++col)
                if (battle.terrainAt({row, col}) == jf::TerrainType::FumeWarning) ++fumeCount;
        assert(fumeCount == 2);

        battle.units()[1].hasActed = false; // doesn't fire a second time
        for (int row = 0; row < jf::kGridRows; ++row)
            for (int col = 0; col < jf::kGridCols; ++col)
                if (battle.terrainAt({row, col}) == jf::TerrainType::FumeWarning) battle.setTerrain({row, col}, jf::TerrainType::Floor);
        jf::takeEnemyTurn(battle, battle.units()[1]);
        fumeCount = 0;
        for (int row = 0; row < jf::kGridRows; ++row)
            for (int col = 0; col < jf::kGridCols; ++col)
                if (battle.terrainAt({row, col}) == jf::TerrainType::FumeWarning) ++fumeCount;
        assert(fumeCount == 0); // one-time only
    }

    {
        // docs/regions/ember_ravine.md「8. 赤熱裂け目」/「地域ボス 赤背の
        // 大蜥蜴」: standard EliminateTeam-primary approximation (same M9-D/
        // K/Q precedent: no AND-composition infra for a single site's 3
        // sub-conditions), with ScriptedWithdrawal wiring for the boss, plus
        // the "冷却弁2個を両方操作" secondary wired as a real
        // secondaryOperateObjectiveId group (cheap now that M9-AD exposed it
        // to JSON Schema).
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor emberRegion2 = jf::regionDescriptor(jf::RegionId::EmberRavine, *data);
        const jf::StageDescriptor* fissureStage = nullptr;
        for (const jf::StageDescriptor& stage : emberRegion2.stages)
            if (stage.id == "redheat_fissure") fissureStage = &stage;
        assert(fissureStage);
        assert(fissureStage->enemyRoster.size() == 3); // boss + 熱地採取団2
        assert(fissureStage->scoutRouteRequiredClass == jf::UnitClass::FrontierRanger);

        const jf::ExplorationOutcome frontal =
            jf::stageRouteOutcome(*fissureStage, jf::ExplorationChoice::FrontalAdvance);
        assert(frontal.startingHeatLevel == 1);

        jf::BattleState battle = jf::createScenarioBattle(*data, *fissureStage, /*seed=*/19);
        const jf::ObjectiveDefinition* eliminateDef = nullptr;
        std::vector<const jf::ObjectiveDefinition*> valveDefs;
        for (const auto& def : battle.missionState().definitions) {
            if (def.kind == jf::ObjectiveKind::EliminateTeam) eliminateDef = &def;
            if (def.groupId == "redheat_fissure_valves") valveDefs.push_back(&def);
        }
        assert(eliminateDef && eliminateDef->primary && eliminateDef->groupId == "primary");
        assert(valveDefs.size() == 2);
        for (const jf::ObjectiveDefinition* def : valveDefs) assert(!def->primary);

        const jf::AliveSnapshot before = jf::captureAliveSnapshot(battle);
        for (jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Enemy) unit.currentHp = 0;
        jf::emitUnitDefeatedEvents(battle, before);
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);

        const jf::Unit* boss = nullptr;
        for (const jf::Unit& unit : battle.units())
            if (unit.unitClass == jf::UnitClass::RedbackLizard) boss = &unit;
        assert(boss && boss->exitReason == jf::UnitExitReason::ScriptedWithdrawal);
    }

    {
        // docs/regions/ember_ravine.md「地域攻略と拠点接続」: committing
        // `RegionId::EmberRavine` to completedRegionIds (the safe-return
        // outcome of clearing all 8 sites, same generic regionCleared()/
        // completedRegionIds mechanism every prior region's own "X_secured"
        // stable ID already uses - `ember_ravine_secured` has no code
        // entity of its own) unlocks BuriedDawnSanctum (the 8th region) via
        // regionUnlocked(). As of this Slice, BuriedDawnSanctum's own
        // regionDescriptor() is the real 6-site skeleton (see the dedicated
        // skeleton test below), not the M9-AG single-site outpost stub.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        jf::BaseState base;
        assert(!jf::regionUnlocked(jf::RegionId::BuriedDawnSanctum, base, *data));
        base.completedRegionIds.insert(jf::RegionId::EmberRavine);
        assert(jf::regionUnlocked(jf::RegionId::BuriedDawnSanctum, base, *data));
        const jf::RegionDescriptor sanctum = jf::regionDescriptor(jf::RegionId::BuriedDawnSanctum, *data);
        assert(sanctum.displayNameJa == "埋没聖堂");
        assert(sanctum.stages.size() == 6);
    }

    {
        // docs/regions/buried_dawn_sanctum.md「地点と周回」: 6-site skeleton +
        // 2 camps + the site 3/4 "順序選択" (either order, both required)
        // branch, mirror of the Old Frontier Settlement skeleton test above.
        // This Slice replaced the single-site `buried_dawn_sanctum_outpost`
        // M9-AG stub with the real 6-stage regionDescriptor() (the stub JSON
        // entry itself is left in place, dead/unreferenced).
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor sanctumRegion =
            jf::regionDescriptor(jf::RegionId::BuriedDawnSanctum, *data);
        assert(sanctumRegion.stages.size() == 6);
        assert(sanctumRegion.stages[0].id == "sanctum_approach");

        const jf::RegionRouteGraph& sanctumRoute = jf::regionRouteGraph(jf::RegionId::BuriedDawnSanctum);
        std::string error;
        assert(jf::validateRouteGraph(sanctumRoute, &error));
        assert(jf::findRouteNode(sanctumRoute, "sanctum_infirmary"));
        assert(jf::findRouteNode(sanctumRoute, "sanctum_archive"));
        const jf::RouteNodeDefinition* branch = jf::findRouteNode(sanctumRoute, "sanctum_infirmary_archive_branch");
        assert(branch && branch->kind == jf::RouteNodeKind::BranchGroup &&
               branch->branchCompletion == jf::BranchCompletion::AllMembers);
    }

    {
        // docs/regions/buried_dawn_sanctum.md「1. 埋没参道」: 主目的(聖堂回収
        // 団4体)・勝利報酬(建築材2、石材1)・ルート2(全員HP-2で迂回)・
        // ルート3(`[重装兵]`梁を支える)。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor sanctumRegion =
            jf::regionDescriptor(jf::RegionId::BuriedDawnSanctum, *data);
        const jf::StageDescriptor& approachStage = sanctumRegion.stages[0];
        assert(approachStage.enemyRoster.size() == 4);
        assert(approachStage.scoutRouteRequiredClass == jf::UnitClass::HeavyInfantry);

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(approachStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "building_material") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "stone") == 1);

        jf::BattleState frontal = jf::createScenarioBattle(*data, approachStage, /*seed=*/5);
        int enemyCount = 0;
        for (const jf::Unit& unit : frontal.units())
            if (unit.team == jf::Team::Enemy) ++enemyCount;
        assert(enemyCount == 4);

        const jf::ExplorationOutcome* collapsedOutcome = nullptr;
        for (const auto& [choice, outcome] : approachStage.routeOutcomes)
            if (choice == jf::ExplorationChoice::CollapsedSidePath) collapsedOutcome = &outcome;
        assert(collapsedOutcome && collapsedOutcome->partyDamage == 2);
    }

    {
        // docs/regions/buried_dawn_sanctum.md「2. 崩れた礼拝堂」: guest-escort
        // primary(避難者1人以上脱出)・避難者2人・敵編成(回収団3、野生獣1)・
        // 勝利報酬(薬草2、聖堂器材1)・[暁の衛生兵]ルート要件。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor sanctumRegion =
            jf::regionDescriptor(jf::RegionId::BuriedDawnSanctum, *data);
        const jf::StageDescriptor& naveStage = sanctumRegion.stages[1];
        assert(naveStage.id == "collapsed_nave");
        assert(naveStage.enemyRoster.size() == 4);
        assert(naveStage.guestUnits.size() == 2);
        assert(naveStage.primaryEscapeUnitsAlternative &&
               naveStage.primaryEscapeUnitsAlternative->requiredEscapeCount == 1);
        assert(naveStage.scoutRouteRequiredClass == jf::UnitClass::DawnChirurgeon);

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(naveStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "herb") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "sanctum_equipment") == 1);

        jf::BattleState naveBattle = jf::createScenarioBattle(*data, naveStage, /*seed=*/7);
        int enemyCount = 0;
        int guestCount = 0;
        for (const jf::Unit& unit : naveBattle.units()) {
            if (unit.team == jf::Team::Enemy) ++enemyCount;
            if (unit.isGuest) ++guestCount;
        }
        assert(enemyCount == 4);
        assert(guestCount == 2);
        const auto& naveGuestIds = naveBattle.missionState().guestUnitIds;
        assert(std::find(naveGuestIds.begin(), naveGuestIds.end(), "collapsed_nave_evacuee1") != naveGuestIds.end());
        assert(std::find(naveGuestIds.begin(), naveGuestIds.end(), "collapsed_nave_evacuee2") != naveGuestIds.end());
    }

    {
        // docs/regions/buried_dawn_sanctum.md「3. 救護室」: 主目的(救護台3
        // Round防衛、primarySurviveRoundsAlternative)・敵編成(回収団4)・
        // 勝利報酬(薬草2、建築材1)・`[衛生兵]`ルート要件(DawnChirurgeon)。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor sanctumRegion =
            jf::regionDescriptor(jf::RegionId::BuriedDawnSanctum, *data);
        const jf::StageDescriptor& infirmaryStage = sanctumRegion.stages[2];
        assert(infirmaryStage.id == "sanctum_infirmary");
        assert(infirmaryStage.enemyRoster.size() == 4);
        assert(infirmaryStage.scoutRouteRequiredClass == jf::UnitClass::DawnChirurgeon);
        assert(infirmaryStage.primarySurviveRoundsAlternative &&
               infirmaryStage.primarySurviveRoundsAlternative->id == "sanctum_infirmary_defense" &&
               infirmaryStage.primarySurviveRoundsAlternative->surviveUntilRound == 3);

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(infirmaryStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "herb") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "building_material") == 1);

        jf::BattleState infirmaryBattle = jf::createScenarioBattle(*data, infirmaryStage, /*seed=*/9);
        int enemyCount = 0;
        for (const jf::Unit& unit : infirmaryBattle.units())
            if (unit.team == jf::Team::Enemy) ++enemyCount;
        assert(enemyCount == 4);
        jf::syncObjectiveProgress(infirmaryBattle);
        assert(jf::evaluateBattleOutcome(infirmaryBattle).kind != jf::BattleOutcomeKind::Victory);
        while (infirmaryBattle.round() <= 3) {
            infirmaryBattle.beginEnemyPhase();
            infirmaryBattle.beginPlayerPhase();
        }
        jf::syncObjectiveProgress(infirmaryBattle);
        assert(jf::evaluateBattleOutcome(infirmaryBattle).kind == jf::BattleOutcomeKind::Victory);
    }

    {
        // docs/regions/buried_dawn_sanctum.md「4. 写本庫」: 主目的「写本箱2個
        // 確保」は標準EliminateTeam-primary近似(ash_crystal_shelf/
        // heatwork_shop/ashsealed_observatoryと同型)、公開副目標「写本箱3個
        // 回収」はsurveyObjectiveId(surveyTileCount:3)経由のsecondary/bonus
        // パス。敵編成(回収団5)・勝利報酬(遺跡片2、石材1)・`[戦闘魔導士]`
        // ルート要件(BattleMage)。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor sanctumRegion =
            jf::regionDescriptor(jf::RegionId::BuriedDawnSanctum, *data);
        const jf::StageDescriptor& archiveStage = sanctumRegion.stages[3];
        assert(archiveStage.id == "sanctum_archive");
        assert(archiveStage.enemyRoster.size() == 5); // 回収団4+弓兵1
        assert(archiveStage.scoutRouteRequiredClass == jf::UnitClass::BattleMage);
        assert(archiveStage.surveyObjectiveId == "sanctum_archive_crate");
        assert(archiveStage.surveyTileCount == 3);

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(archiveStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "ruin_fragment") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "stone") == 1);

        jf::BattleState archiveBattle = jf::createScenarioBattle(*data, archiveStage, /*seed=*/19);
        const jf::ObjectiveDefinition* eliminateDef = nullptr;
        std::vector<const jf::ObjectiveDefinition*> crateDefs;
        for (const auto& def : archiveBattle.missionState().definitions) {
            if (def.kind == jf::ObjectiveKind::EliminateTeam) eliminateDef = &def;
            if (def.groupId == "sanctum_archive_crate") crateDefs.push_back(&def);
        }
        assert(eliminateDef && eliminateDef->primary && eliminateDef->groupId == "primary");
        assert(crateDefs.size() == 3);
        for (const jf::ObjectiveDefinition* def : crateDefs) assert(!def->primary);
        bool hasCrateGroup = false;
        for (const auto& group : archiveBattle.missionState().groups)
            if (group.id == "sanctum_archive_crate" && group.rule == jf::ObjectiveGroupRule::Any)
                hasCrateGroup = true;
        assert(hasCrateGroup);
    }

    {
        // docs/regions/buried_dawn_sanctum.md「地点と周回」: 地点3(救護室)・
        // 地点4(写本庫)は「順序選択」ペアでCAMP IIへ合流、両方到達可能に
        // なったことをRouteGraph経由で確認(sanctum_infirmary_archive_branch、
        // M9-AHで既に配線済みのBranchCompletion::AllMembers)。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionRouteGraph& sanctumRoute = jf::regionRouteGraph(jf::RegionId::BuriedDawnSanctum);
        assert(jf::findRouteNode(sanctumRoute, "sanctum_infirmary"));
        assert(jf::findRouteNode(sanctumRoute, "sanctum_archive"));
        assert(jf::findRouteNode(sanctumRoute, "sanctum_camp2"));
    }

    {
        // docs/regions/buried_dawn_sanctum.md「地点5: 封鎖回廊」: primary is
        // 2 OperateObject Objectives (封鎖輪2個操作), mirroring
        // windwatch_station's own dual-panel shape (windscar_plateau.md
        // 「3. 風見台」) via the same objectPlacementRules/operateObjectiveId
        // JSON Schema - defeating every enemy without operating both rings
        // must NOT win, operating only one must NOT win either, only both
        // together wins.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor sanctumRegion =
            jf::regionDescriptor(jf::RegionId::BuriedDawnSanctum, *data);
        const jf::StageDescriptor* sealedPassageStage = nullptr;
        for (const jf::StageDescriptor& stage : sanctumRegion.stages)
            if (stage.id == "sealed_passage") sealedPassageStage = &stage;
        assert(sealedPassageStage);
        assert(sealedPassageStage->enemyRoster.size() == 6);
        assert(sealedPassageStage->scoutRouteRequiredClass == jf::UnitClass::FrontierEngineer);

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(*sealedPassageStage, choice,
                                                /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "sanctum_equipment") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "quality_iron") == 1);

        jf::BattleState battle = jf::createScenarioBattle(*data, *sealedPassageStage, /*seed=*/13);
        int enemyCount = 0;
        for (const jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Enemy) ++enemyCount;
        assert(enemyCount == 6);

        for (jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Enemy) unit.currentHp = 0;

        jf::BattleObjectState* westRing = battle.findObject("sealed_passage_ring_west_1");
        assert(westRing != nullptr);
        westRing->interactionCount = 1; // only ONE of the 2 rings operated
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind != jf::BattleOutcomeKind::Victory);

        jf::BattleObjectState* eastRing = battle.findObject("sealed_passage_ring_east_1");
        assert(eastRing != nullptr);
        eastRing->interactionCount = 1; // both rings now operated
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);
    }
    {
        // M9-AM: 埋没聖堂 地点6「夜明け祭壇」/ 強敵「聖堂回収団長」/ 地域攻略.
        // Primary approximated as primarySurviveRoundsAlternative(4), same
        // "headline sub-condition as primary" reasoning as
        // settlement_dawn_defense's own SurviveRounds(5) - the elite leader
        // is an optional retreat-tuned unit (AiSystem.cpp's
        // retreatHpPercent=25), NOT a scripted boss.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor sanctumRegion =
            jf::regionDescriptor(jf::RegionId::BuriedDawnSanctum, *data);
        const jf::StageDescriptor* dawnAltarStage = nullptr;
        for (const jf::StageDescriptor& stage : sanctumRegion.stages)
            if (stage.id == "dawn_altar") dawnAltarStage = &stage;
        assert(dawnAltarStage);
        assert(dawnAltarStage->enemyRoster.size() == 6);
        assert(dawnAltarStage->enemyRoster[0].classId == jf::UnitClass::SanctumRetrievalLeader);
        assert(dawnAltarStage->scoutRouteRequiredClass == jf::UnitClass::MarchCaptain);
        assert(dawnAltarStage->primarySurviveRoundsAlternative.has_value() &&
              dawnAltarStage->primarySurviveRoundsAlternative->surviveUntilRound == 4);

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(*dawnAltarStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "building_material") == 3);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "ruin_fragment") == 2);

        // Victory via SurviveRounds alone, regardless of the elite's fate.
        jf::BattleState battle = jf::createScenarioBattle(*data, *dawnAltarStage, /*seed=*/7);
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind != jf::BattleOutcomeKind::Victory);
        while (battle.round() <= 4) {
            battle.beginEnemyPhase();
            battle.beginPlayerPhase();
        }
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);

        // Region-clear: RegionId::ShatteredMarchFort exists, is registered in
        // the region list, and unlocks exactly on BuriedDawnSanctum's own
        // completion (direct BaseState check - a full E2E playthrough
        // through all 8 prior regions is impractical here).
        jf::BaseState base;
        assert(!jf::regionUnlocked(jf::RegionId::ShatteredMarchFort, base, *data));
        base.completedRegionIds.insert(jf::RegionId::BuriedDawnSanctum);
        assert(jf::regionUnlocked(jf::RegionId::ShatteredMarchFort, base, *data));
        const jf::RegionDescriptor fortRegion = jf::regionDescriptor(jf::RegionId::ShatteredMarchFort, *data);
        assert(!fortRegion.stages.empty());
    }

    {
        // docs/regions/shattered_march_fort.md「地点・周回」: 7-site skeleton
        // + 3 camps + the site 3/4 "順序選択" (either order, both required)
        // branch, mirror of the BuriedDawnSanctum skeleton test above. This
        // Slice replaced the single-site `shattered_march_fort_outpost`
        // M9-AM stub with the real 7-stage regionDescriptor() (the stub JSON
        // entry itself is left in place, dead/unreferenced).
        //
        // Also guards against a repeat of the exact bug M9-AM found and
        // fixed for BuriedDawnSanctum: usesRouteGraph() omitting a region
        // entirely, silently skipping its route-graph validation in
        // GameData.cpp's startup loop.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        assert(jf::usesRouteGraph(jf::RegionId::ShatteredMarchFort));
        const jf::RegionDescriptor fortRegion =
            jf::regionDescriptor(jf::RegionId::ShatteredMarchFort, *data);
        assert(fortRegion.stages.size() == 7);
        assert(fortRegion.stages[0].id == "fort_outer_wall");

        const jf::RegionRouteGraph& fortRoute = jf::regionRouteGraph(jf::RegionId::ShatteredMarchFort);
        std::string error;
        assert(jf::validateRouteGraph(fortRoute, &error));
        assert(jf::findRouteNode(fortRoute, "fort_old_barracks"));
        assert(jf::findRouteNode(fortRoute, "fort_logistics_depot"));
        const jf::RouteNodeDefinition* fortBranch =
            jf::findRouteNode(fortRoute, "fort_barracks_logistics_branch");
        assert(fortBranch && fortBranch->kind == jf::RouteNodeKind::BranchGroup &&
               fortBranch->branchCompletion == jf::BranchCompletion::AllMembers &&
               fortBranch->branchMembers.size() == 2);
    }

    {
        // docs/regions/shattered_march_fort.md「破砕外郭」: 主目的(回収団5)・
        // 勝利報酬(石材2、高品質鉄材1)・ルート2(全員HP-2で破孔)・
        // ルート3(`[重装兵]`瓦礫突破)。主目的は本Slice時点でこのプロジェクトの
        // 一貫した前例(M9-D/J/Y/AC/AE/AG/AM等)どおりEliminateTeamのみへ近似
        // し(正本の「敵全滅+外郭標識」を genuine multi-Kind AND primaryとして
        // 実装せず)、「外郭標識で行動終了」は`surveyObjectiveId`(裸タイル、
        // count無し)による副目標としてのみ表現した。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor fortRegion =
            jf::regionDescriptor(jf::RegionId::ShatteredMarchFort, *data);
        const jf::StageDescriptor& outerWallStage = fortRegion.stages[0];
        assert(outerWallStage.enemyRoster.size() == 5);
        assert(outerWallStage.scoutRouteRequiredClass == jf::UnitClass::HeavyInfantry);
        assert(outerWallStage.surveyObjectiveId == "fort_outer_wall_marker");

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(outerWallStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "stone") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "quality_iron") == 1);

        jf::BattleState frontal = jf::createScenarioBattle(*data, outerWallStage, /*seed=*/5);
        int enemyCount = 0;
        for (const jf::Unit& unit : frontal.units())
            if (unit.team == jf::Team::Enemy) ++enemyCount;
        assert(enemyCount == 5);

        const jf::ExplorationOutcome* collapsedOutcome = nullptr;
        for (const auto& [choice, outcome] : outerWallStage.routeOutcomes)
            if (choice == jf::ExplorationChoice::CollapsedSidePath) collapsedOutcome = &outcome;
        assert(collapsedOutcome && collapsedOutcome->partyDamage == 2);
    }

    {
        // docs/regions/shattered_march_fort.md「崩れ門」: JSON-authored via
        // stageDescriptorFromContent(), same operateObjectiveId pattern as
        // sunken_sluice(M9-J)/ravine_cooling_channel(M9-AC)/heatwork_shop
        // (M9-AE) - primary approximated as OperateObject-only, the
        // "OperateObject AND SurviveRounds(2)" AND-composition (「城門操作後
        // 2Round防衛」) is deferred as the same category of gap those three
        // sites already recorded. 敗北条件「城門0」もObject耐久が未実装のため
        // 同様に見送り(部隊全滅は既存Engineデフォルトのまま常時有効)。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor fortRegion =
            jf::regionDescriptor(jf::RegionId::ShatteredMarchFort, *data);
        const jf::StageDescriptor* gateStage = nullptr;
        for (const jf::StageDescriptor& stage : fortRegion.stages)
            if (stage.id == "fort_broken_gate") gateStage = &stage;
        assert(gateStage);
        assert(gateStage->enemyRoster.size() == 5);
        assert(gateStage->scoutRouteRequiredClass == jf::UnitClass::FrontierEngineer);
        assert(gateStage->objectPlacementRules.size() == 1);
        assert(gateStage->objectPlacementRules[0].operateObjectiveId == "operate_fort_gate");

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(*gateStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "stone") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "military_supplies") == 1);

        jf::BattleState battle = jf::createScenarioBattle(*data, *gateStage, /*seed=*/11);
        const jf::ObjectiveDefinition* operateDef = nullptr;
        for (const auto& def : battle.missionState().definitions)
            if (def.kind == jf::ObjectiveKind::OperateObject) operateDef = &def;
        assert(operateDef && operateDef->primary && operateDef->groupId == "primary");
    }

    {
        // docs/regions/shattered_march_fort.md「7地点仕様」旧兵舎: same
        // guest-escort shape as blackwater_crossing/ember_ravine_ledge -
        // primaryEscapeUnitsAlternative(requiredEscapeCount=1) over 2 guests,
        // EliminateTeam not present as primary.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor fortRegion = jf::regionDescriptor(jf::RegionId::ShatteredMarchFort, *data);
        const jf::StageDescriptor* barracksStage = nullptr;
        for (const jf::StageDescriptor& stage : fortRegion.stages)
            if (stage.id == "fort_old_barracks") barracksStage = &stage;
        assert(barracksStage && barracksStage->guestUnits.size() == 2);
        assert(barracksStage->enemyRoster.size() == 4);
        assert(barracksStage->scoutRouteRequiredClass == jf::UnitClass::DawnChirurgeon);

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(*barracksStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "military_supplies") == 1);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "cloth") == 2);

        jf::BattleState battle = jf::createScenarioBattle(*data, *barracksStage, /*seed=*/13);
        assert(battle.missionState().guestUnitIds.size() == 2);
        for (const jf::Unit& unit : battle.units())
            if (unit.isGuest) assert(unit.team == jf::Team::Player);

        const jf::ObjectiveDefinition* escapeDef = nullptr;
        for (const auto& def : battle.missionState().definitions)
            if (def.id == "fort_old_barracks_escape") escapeDef = &def;
        assert(escapeDef && escapeDef->primary && escapeDef->kind == jf::ObjectiveKind::EscapeUnits);

        const std::string& guestId = battle.missionState().guestUnitIds[0];
        jf::BattleEvent guestEscapes{
            1, 1,
            jf::ActionResolvedEvent{1, guestId, jf::Team::Player, jf::ActionKind::Wait, escapeDef->target.tile}};
        jf::handleObjectiveEvent(battle.missionState(), guestEscapes);
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);

        // Both guests lost -> Defeat via allGuestsLost(), independent of the
        // player squad (same as blackwater_crossing's own equivalent test).
        jf::BattleState lossBattle = jf::createScenarioBattle(*data, *barracksStage, /*seed=*/13);
        assert(!lossBattle.allGuestsLost());
        for (jf::Unit& unit : lossBattle.units())
            if (unit.isGuest) unit.currentHp = 0;
        assert(lossBattle.allGuestsLost());
        assert(!lossBattle.allPlayersDefeated());
        assert(jf::evaluateBattleOutcome(lossBattle).kind == jf::BattleOutcomeKind::Defeat);
    }

    {
        // docs/regions/shattered_march_fort.md「副目標と重要発見」旧兵舎の
        // "負傷兵全員避難 -> 集団救護記録": both guests credited on the escape
        // objective grants kGroupTriageRecordsDiscovery via GameApp.cpp's
        // ad-hoc creditedTargetIds.size()>=2 check (same shape as
        // collapsed_nave's kFieldMedicalRecordsDiscovery test).
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor fortRegion = jf::regionDescriptor(jf::RegionId::ShatteredMarchFort, *data);
        const jf::StageDescriptor* barracksStage = nullptr;
        for (const jf::StageDescriptor& stage : fortRegion.stages)
            if (stage.id == "fort_old_barracks") barracksStage = &stage;
        assert(barracksStage);

        jf::BattleState battle = jf::createScenarioBattle(*data, *barracksStage, /*seed=*/13);
        const jf::ObjectiveDefinition* escapeDef = nullptr;
        for (const auto& def : battle.missionState().definitions)
            if (def.id == "fort_old_barracks_escape") escapeDef = &def;
        assert(escapeDef);
        auto& progress = const_cast<jf::BattleMissionState&>(battle.missionState()).progress.at(escapeDef->id);
        progress.creditedTargetIds.insert(battle.missionState().guestUnitIds[0]);
        progress.creditedTargetIds.insert(battle.missionState().guestUnitIds[1]);
        assert(progress.creditedTargetIds.size() >= 2);
    }

    {
        // docs/regions/shattered_march_fort.md「兵站庫」: 主目的「箱2個確保」は
        // 標準EliminateTeam-primary近似(sanctum_archive/ash_crystal_shelf/
        // heatwork_shop/ashsealed_observatoryと同型)、公開副目標「兵站箱全保全」
        // はsurveyObjectiveId(surveyTileCount:2)経由のsecondary/bonusパス。
        // 敵編成(回収団5)・勝利報酬(高品質鉄材1、軍需品2)・`[伝令騎兵]`
        // ルート要件(MessengerCavalry)。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor fortRegion = jf::regionDescriptor(jf::RegionId::ShatteredMarchFort, *data);
        const jf::StageDescriptor* depotStage = nullptr;
        for (const jf::StageDescriptor& stage : fortRegion.stages)
            if (stage.id == "fort_logistics_depot") depotStage = &stage;
        assert(depotStage);
        assert(depotStage->enemyRoster.size() == 5); // 回収団4+弓兵1
        assert(depotStage->scoutRouteRequiredClass == jf::UnitClass::MessengerCavalry);
        assert(depotStage->surveyObjectiveId == "fort_logistics_depot_crate");
        assert(depotStage->surveyTileCount == 2);

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(*depotStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "quality_iron") == 1);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "military_supplies") == 2);

        jf::BattleState depotBattle = jf::createScenarioBattle(*data, *depotStage, /*seed=*/17);
        const jf::ObjectiveDefinition* eliminateDef = nullptr;
        std::vector<const jf::ObjectiveDefinition*> crateDefs;
        for (const auto& def : depotBattle.missionState().definitions) {
            if (def.kind == jf::ObjectiveKind::EliminateTeam) eliminateDef = &def;
            if (def.groupId == "fort_logistics_depot_crate") crateDefs.push_back(&def);
        }
        assert(eliminateDef && eliminateDef->primary && eliminateDef->groupId == "primary");
        assert(crateDefs.size() == 2);
        for (const jf::ObjectiveDefinition* def : crateDefs) assert(!def->primary);
        bool hasCrateGroup = false;
        for (const auto& group : depotBattle.missionState().groups)
            if (group.id == "fort_logistics_depot_crate" && group.rule == jf::ObjectiveGroupRule::Any)
                hasCrateGroup = true;
        assert(hasCrateGroup);
    }

    {
        // docs/regions/shattered_march_fort.md「副目標と重要発見」兵站庫の
        // "兵站箱全保全 -> 軍需管理記録": both crate Objectives Completed grants
        // kLogisticsManagementRecordsDiscovery via GameApp.cpp's ad-hoc
        // all-group-members-Completed check (same shape as heatwork_shop's
        // kSpecialForgingRecordsDiscovery test).
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor fortRegion = jf::regionDescriptor(jf::RegionId::ShatteredMarchFort, *data);
        const jf::StageDescriptor* depotStage = nullptr;
        for (const jf::StageDescriptor& stage : fortRegion.stages)
            if (stage.id == "fort_logistics_depot") depotStage = &stage;
        assert(depotStage);

        jf::BattleState battle = jf::createScenarioBattle(*data, *depotStage, /*seed=*/17);
        std::vector<const jf::ObjectiveDefinition*> crateDefs;
        for (const auto& def : battle.missionState().definitions)
            if (def.groupId == "fort_logistics_depot_crate") crateDefs.push_back(&def);
        assert(crateDefs.size() == 2);
        auto& mutableMission = const_cast<jf::BattleMissionState&>(battle.missionState());
        for (const jf::ObjectiveDefinition* def : crateDefs)
            mutableMission.progress.at(def->id).status = jf::ObjectiveStatus::Completed;
        bool allCompleted = true;
        for (const jf::ObjectiveDefinition* def : crateDefs)
            if (battle.missionState().progress.at(def->id).status != jf::ObjectiveStatus::Completed)
                allCompleted = false;
        assert(allCompleted);
    }

    {
        // docs/regions/shattered_march_fort.md「地点・周回」: site 3
        // (fort_old_barracks)・site 4 (fort_logistics_depot) の順序選択ペアが
        // 両方実コンテンツ化されたことで、CAMP IIへ実質的に到達可能になった
        // ことを確認(グラフ配線自体はM9-ANから機能済み)。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor fortRegion = jf::regionDescriptor(jf::RegionId::ShatteredMarchFort, *data);
        const jf::StageDescriptor* barracksStage = nullptr;
        const jf::StageDescriptor* depotStage = nullptr;
        for (const jf::StageDescriptor& stage : fortRegion.stages) {
            if (stage.id == "fort_old_barracks") barracksStage = &stage;
            if (stage.id == "fort_logistics_depot") depotStage = &stage;
        }
        assert(barracksStage && !barracksStage->enemyRoster.empty());
        assert(depotStage && !depotStage->enemyRoster.empty());
        const jf::RegionRouteGraph& fortRoute = jf::regionRouteGraph(jf::RegionId::ShatteredMarchFort);
        assert(jf::findRouteNode(fortRoute, "fort_camp2"));
    }

    {
        // docs/regions/shattered_march_fort.md「地点5: 信号庭」: primary is
        // 2 genuine OperateObject Objectives (信号盤2個操作), mirroring
        // windwatch_station (M9-N)/sealed_passage (M9-AL)'s own dual-panel
        // shape via the same objectPlacementRules/operateObjectiveId JSON
        // Schema - defeating every enemy without operating both panels must
        // NOT win, operating only one must NOT win either, only both
        // together wins.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor fortRegion = jf::regionDescriptor(jf::RegionId::ShatteredMarchFort, *data);
        const jf::StageDescriptor* signalYardStage = nullptr;
        for (const jf::StageDescriptor& stage : fortRegion.stages)
            if (stage.id == "fort_signal_yard") signalYardStage = &stage;
        assert(signalYardStage);
        assert(signalYardStage->enemyRoster.size() == 6);
        assert(signalYardStage->scoutRouteRequiredClass == jf::UnitClass::BannerBearer);

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(*signalYardStage, choice,
                                                /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "stone") == 1);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "military_supplies") == 1);

        jf::BattleState battle = jf::createScenarioBattle(*data, *signalYardStage, /*seed=*/17);
        int enemyCount = 0;
        for (const jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Enemy) ++enemyCount;
        assert(enemyCount == 6);

        for (jf::Unit& unit : battle.units())
            if (unit.team == jf::Team::Enemy) unit.currentHp = 0;

        jf::BattleObjectState* westPanel = battle.findObject("fort_signal_yard_panel_west_1");
        assert(westPanel != nullptr);
        westPanel->interactionCount = 1; // only ONE of the 2 panels operated
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind != jf::BattleOutcomeKind::Victory);

        jf::BattleObjectState* eastPanel = battle.findObject("fort_signal_yard_panel_east_1");
        assert(eastPanel != nullptr);
        eastPanel->interactionCount = 1; // both panels now operated
        jf::syncObjectiveProgress(battle);
        assert(jf::evaluateBattleOutcome(battle).kind == jf::BattleOutcomeKind::Victory);
    }

    {
        // docs/regions/shattered_march_fort.md「地点6: 予備壁」: primary is
        // SurviveRounds(4) (sanctum_infirmary/herb_islet同型のSurviveRoundsMissionRule
        // 再利用), 敵「2波計7」は`StageDescriptor::timedReinforcement`が単一
        // `std::optional`である既知の制限(M9-Y "settlement_dawn_defense"で記録済み)
        // のため4初期+3増援1波へ近似。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor fortRegion = jf::regionDescriptor(jf::RegionId::ShatteredMarchFort, *data);
        const jf::StageDescriptor* reserveWallStage = nullptr;
        for (const jf::StageDescriptor& stage : fortRegion.stages)
            if (stage.id == "fort_reserve_wall") reserveWallStage = &stage;
        assert(reserveWallStage);
        assert(reserveWallStage->enemyRoster.size() == 4);
        assert(reserveWallStage->scoutRouteRequiredClass == jf::UnitClass::VeteranGuard);
        assert(reserveWallStage->primarySurviveRoundsAlternative &&
              reserveWallStage->primarySurviveRoundsAlternative->id == "fort_reserve_wall_defense" &&
              reserveWallStage->primarySurviveRoundsAlternative->surviveUntilRound == 4);
        assert(reserveWallStage->timedReinforcement && reserveWallStage->timedReinforcement->spawnRound == 2 &&
              reserveWallStage->timedReinforcement->units.size() == 3);

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(*reserveWallStage, choice,
                                                /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "quality_iron") == 1);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "stone") == 2);

        // 主目的: 4ラウンド終了まで避難所を守る(SurviveRoundsをそのまま再利用、
        // EliminateTeamとのOR)。敵を1体も倒さなくても4ラウンド生存でVictory。
        jf::BattleState defense = jf::createScenarioBattle(*data, *reserveWallStage, /*seed=*/9);
        jf::syncObjectiveProgress(defense);
        assert(jf::evaluateBattleOutcome(defense).kind != jf::BattleOutcomeKind::Victory);
        while (defense.round() <= 4) {
            defense.beginEnemyPhase();
            defense.beginPlayerPhase();
        }
        jf::syncObjectiveProgress(defense);
        assert(jf::evaluateBattleOutcome(defense).kind == jf::BattleOutcomeKind::Victory);
    }

    {
        // docs/regions/shattered_march_fort.md「地点7: 切離命令庫」: this
        // region's final site. Primary is SurviveRounds(5)
        // (sanctum_infirmary/herb_islet/fort_reserve_wall同型). 敵「隊長1、
        // 残留隊6」: `UnitClass::FortGarrisonCaptain`(HP48/STR10/DEF9/RES5/
        // MOV4) + Fort Garrison(Bandit3+WatchArcher3), matching the doc's
        // 隊長「隊長撃破を必須にしない」optional-elite shape (generic AI +
        // AiSystem.cpp's retreatHpPercent=25 tuning only, no bespoke
        // EnemyAI.cpp turn function - see UnitClass.hpp's own comment for
        // this class for the 3 deferred bespoke abilities).
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor fortRegion = jf::regionDescriptor(jf::RegionId::ShatteredMarchFort, *data);
        const jf::StageDescriptor* archiveStage = nullptr;
        for (const jf::StageDescriptor& stage : fortRegion.stages)
            if (stage.id == "fort_severance_order_archive") archiveStage = &stage;
        assert(archiveStage);
        // Explicit roster-size assertion (M9-AM's 3rd fix's own lesson: a new
        // UnitClass silently vanishing from a roster with no error must be
        // caught by an explicit size check, not just "it built").
        assert(archiveStage->enemyRoster.size() == 7);
        assert(archiveStage->enemyRoster.front().classId == jf::UnitClass::FortGarrisonCaptain);
        assert(archiveStage->scoutRouteRequiredClass == jf::UnitClass::MarchCaptain);
        assert(archiveStage->primarySurviveRoundsAlternative &&
              archiveStage->primarySurviveRoundsAlternative->id == "fort_severance_order_archive_defense" &&
              archiveStage->primarySurviveRoundsAlternative->surviveUntilRound == 5);
        assert(archiveStage->surveyObjectiveId && *archiveStage->surveyObjectiveId == "fort_severance_order_archive_crate");
        assert(archiveStage->surveyTileCount == 2);

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(*archiveStage, choice, /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "quality_iron") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "military_supplies") == 2);
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "stone") == 1);

        // 主目的: 5ラウンド生存(EliminateTeamとのOR)。隊長を1体も倒さなくても
        // 5ラウンド生存でVictory(「隊長撃破を必須にしない」)。
        jf::BattleState defense = jf::createScenarioBattle(*data, *archiveStage, /*seed=*/11);
        jf::syncObjectiveProgress(defense);
        assert(jf::evaluateBattleOutcome(defense).kind != jf::BattleOutcomeKind::Victory);
        while (defense.round() <= 5) {
            defense.beginEnemyPhase();
            defense.beginPlayerPhase();
        }
        jf::syncObjectiveProgress(defense);
        assert(jf::evaluateBattleOutcome(defense).kind == jf::BattleOutcomeKind::Victory);
    }

    {
        // docs/regions/mapped_edge.md「地点と周回」: RegionId::MappedEdge
        // (10th and FINAL region of the whole campaign) exists, is
        // registered in regionUnlocked()/regionDescriptor(), and is only
        // unlocked once ShatteredMarchFort is in completedRegionIds - same
        // direct-BaseState-manipulation test shape as M9-AM's own
        // ShatteredMarchFort-unlock test above (full E2E through all 9 prior
        // regions is impractical). As of this Slice (region skeleton +
        // site 1), MappedEdge now has a full 9-site RouteGraph -
        // usesRouteGraph() must return true (guards against a repeat of the
        // usesRouteGraph() omission bug fixed for BuriedDawnSanctum/M9-AM).
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        jf::BaseState base;
        assert(!jf::regionUnlocked(jf::RegionId::MappedEdge, base, *data));
        base.completedRegionIds.insert(jf::RegionId::ShatteredMarchFort);
        assert(jf::regionUnlocked(jf::RegionId::MappedEdge, base, *data));
        const jf::RegionDescriptor mappedEdgeRegion = jf::regionDescriptor(jf::RegionId::MappedEdge, *data);
        assert(mappedEdgeRegion.stages.size() == 9);
        assert(jf::usesRouteGraph(jf::RegionId::MappedEdge));
    }

    {
        // docs/regions/mapped_edge.md「地点と周回」/「9地点仕様」: this
        // Slice's region skeleton (9 sites + 4 camps + the site 3/4
        // 順序選択 pair via BranchCompletion::AllMembers, identical shape to
        // ShatteredMarchFort's fort_barracks_logistics_branch) plus site 1
        // "最後の既知標識" as real content.
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionRouteGraph& graph = jf::regionRouteGraph(jf::RegionId::MappedEdge);
        assert(graph.regionId == jf::RegionId::MappedEdge);
        const jf::RouteNodeDefinition* branch = jf::findRouteNode(graph, "mapped_edge_camp_survey_branch");
        assert(branch);
        assert(branch->branchCompletion == jf::BranchCompletion::AllMembers);
        assert(branch->branchMembers.size() == 2);

        const jf::StageContentData& markerStage = data->stageContent("mapped_edge_last_known_marker");
        assert(markerStage.enemyRoster.size() == 5);
        assert(markerStage.scoutRouteRequiredClass == jf::UnitClass::FrontierScout);
        bool foundRushRoute = false;
        for (const auto& [choice, outcome] : markerStage.routeOutcomes) {
            if (choice == jf::ExplorationChoice::CollapsedSidePath) {
                assert(outcome.partyDamage == 2);
                foundRushRoute = true;
            }
        }
        assert(foundRushRoute);

        const jf::RegionDescriptor mappedEdgeRegionForSite1 = jf::regionDescriptor(jf::RegionId::MappedEdge, *data);
        const jf::StageDescriptor& markerRegionStage = mappedEdgeRegionForSite1.stages.front();
        int rareMaterial = 0;
        int food = 0;
        for (const auto& rule : markerRegionStage.victoryRewardRules) {
            for (const auto& stack : rule.loot) {
                if (stack.id == "rare_material") rareMaterial += stack.quantity;
                if (stack.id == "food") food += stack.quantity;
            }
        }
        assert(rareMaterial == 1);
        assert(food == 1);
    }

    {
        // docs/regions/mapped_edge.md「9地点仕様」地点2「乾いた川床」: 主目的
        // 「水箱2個確保」は標準EliminateTeam-primary近似(sanctum_archive/
        // fort_logistics_depotと同型)、「水箱2個確保」自体はsurveyObjectiveId
        // (surveyTileCount:2)経由のsecondary/bonusパスとして残す。敵編成
        // (野生獣4+追跡者2、WolfそのままとBandit reskin "Pursuer")・勝利報酬
        // (薬草2、希少素材1)・`[衛生兵]`ルート要件(DawnChirurgeon)。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor mappedEdgeRegion = jf::regionDescriptor(jf::RegionId::MappedEdge, *data);
        const jf::StageDescriptor* riverbedStage = nullptr;
        for (const jf::StageDescriptor& stage : mappedEdgeRegion.stages)
            if (stage.id == "mapped_edge_dry_riverbed") riverbedStage = &stage;
        assert(riverbedStage);
        assert(riverbedStage->enemyRoster.size() == 6); // 野生獣4+追跡者2
        assert(riverbedStage->scoutRouteRequiredClass == jf::UnitClass::DawnChirurgeon);
        assert(riverbedStage->surveyObjectiveId == "mapped_edge_dry_riverbed_crate");
        assert(riverbedStage->surveyTileCount == 2);

        int herb = 0;
        int rareMaterial = 0;
        for (const auto& rule : riverbedStage->victoryRewardRules) {
            for (const auto& stack : rule.loot) {
                if (stack.id == "herb") herb += stack.quantity;
                if (stack.id == "rare_material") rareMaterial += stack.quantity;
            }
        }
        assert(herb == 2);
        assert(rareMaterial == 1);

        jf::BattleState riverbedBattle = jf::createScenarioBattle(*data, *riverbedStage, /*seed=*/23);
        const jf::ObjectiveDefinition* eliminateDef = nullptr;
        std::vector<const jf::ObjectiveDefinition*> crateDefs;
        for (const auto& def : riverbedBattle.missionState().definitions) {
            if (def.kind == jf::ObjectiveKind::EliminateTeam) eliminateDef = &def;
            if (def.groupId == "mapped_edge_dry_riverbed_crate") crateDefs.push_back(&def);
        }
        assert(eliminateDef && eliminateDef->primary && eliminateDef->groupId == "primary");
        assert(crateDefs.size() == 2);
        for (const jf::ObjectiveDefinition* def : crateDefs) assert(!def->primary);
        bool hasCrateGroup = false;
        for (const auto& group : riverbedBattle.missionState().groups)
            if (group.id == "mapped_edge_dry_riverbed_crate" && group.rule == jf::ObjectiveGroupRule::Any)
                hasCrateGroup = true;
        assert(hasCrateGroup);
    }

    {
        // docs/regions/mapped_edge.md「9地点仕様」地点3「無記録野営跡」: 主目的
        // 「記録箱2個回収」も地点2(乾いた川床)と同型のEliminateTeam-primary近似
        // (surveyObjectiveId/surveyTileCount:2経由のsecondary/bonusパス)。敵編成
        // 追跡者5(Bandit reskin)・勝利報酬(遺跡片2、食料2)・`[猟兵]`ルート要件
        // (FrontierRanger)。安定id`unrecorded_camp_catalogued`はdocs/regions/
        // mapped_edge.mdの「安定ID」節に記載があるが、本Sliceでは地点1・2と同様
        // 恒久成果配線自体は未実装(地域攻略Slice側の範囲)。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor mappedEdgeRegion = jf::regionDescriptor(jf::RegionId::MappedEdge, *data);
        const jf::StageDescriptor* campStage = nullptr;
        for (const jf::StageDescriptor& stage : mappedEdgeRegion.stages)
            if (stage.id == "mapped_edge_unrecorded_camp") campStage = &stage;
        assert(campStage);
        assert(campStage->enemyRoster.size() == 5); // 追跡者5
        assert(campStage->scoutRouteRequiredClass == jf::UnitClass::FrontierRanger);
        assert(campStage->surveyObjectiveId == "mapped_edge_unrecorded_camp_crate");
        assert(campStage->surveyTileCount == 2);

        int ruinFragment = 0;
        int food = 0;
        for (const auto& rule : campStage->victoryRewardRules) {
            for (const auto& stack : rule.loot) {
                if (stack.id == "ruin_fragment") ruinFragment += stack.quantity;
                if (stack.id == "food") food += stack.quantity;
            }
        }
        assert(ruinFragment == 2);
        assert(food == 2);

        jf::BattleState campBattle = jf::createScenarioBattle(*data, *campStage, /*seed=*/23);
        const jf::ObjectiveDefinition* eliminateDef = nullptr;
        std::vector<const jf::ObjectiveDefinition*> crateDefs;
        for (const auto& def : campBattle.missionState().definitions) {
            if (def.kind == jf::ObjectiveKind::EliminateTeam) eliminateDef = &def;
            if (def.groupId == "mapped_edge_unrecorded_camp_crate") crateDefs.push_back(&def);
        }
        assert(eliminateDef && eliminateDef->primary && eliminateDef->groupId == "primary");
        assert(crateDefs.size() == 2);
        for (const jf::ObjectiveDefinition* def : crateDefs) assert(!def->primary);
        bool hasCrateGroup = false;
        for (const auto& group : campBattle.missionState().groups)
            if (group.id == "mapped_edge_unrecorded_camp_crate" && group.rule == jf::ObjectiveGroupRule::Any)
                hasCrateGroup = true;
        assert(hasCrateGroup);
    }

    {
        // docs/regions/mapped_edge.md「9地点仕様」地点4「二股の踏査路」: primary
        // is 2 OperateObject Objectives (踏査標識2個操作), mirroring
        // windwatch_station(M9-N)/sealed_passage(M9-AL)/fort_signal_yard(M9-AR)'s
        // own dual-Device genuine AND-group shape via objectPlacementRules/
        // operateObjectiveId under the default "primary"/ObjectiveGroupRule::All
        // group - defeating every enemy without operating both markers must NOT
        // win, operating only one must NOT win either, only both together wins.
        // 敗北条件「8Round超過」はCinderwatch/Ember Ravine地点7と同型の未実装
        // round-limit機構であり本Sliceでも見送り(部隊全滅は既存Engineで常時
        // 有効)。敵「2組計6」はトリガー条件の記載が無いため追跡者3+Wolf3の
        // フラットな6体編成で近似した。主目的報酬「地域固有素材2」は正本の
        // 「9地点仕様」表で地点4・6に繰り返し登場し(最低保証欄の「地域固有
        // 素材3」=2+1と一致)、rare_materialの別名ではなく新規id
        // `frontier_edge_material`として登録した(data/locales/{en,ja}.json・
        // ui_shared.cppのknownセット・JAグリフcharset収集ループへ追加済み)。
        // 安定id`split_survey_routes_mapped`は正本の「安定ID」節に記載あるが、
        // 地点1〜3と同様、恒久成果配線自体は地域攻略Slice側の範囲として未実装。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor mappedEdgeRegion = jf::regionDescriptor(jf::RegionId::MappedEdge, *data);
        const jf::StageDescriptor* splitRouteStage = nullptr;
        for (const jf::StageDescriptor& stage : mappedEdgeRegion.stages)
            if (stage.id == "mapped_edge_split_survey_route") splitRouteStage = &stage;
        assert(splitRouteStage);
        assert(splitRouteStage->enemyRoster.size() == 6); // 2組計6
        assert(splitRouteStage->scoutRouteRequiredClass == jf::UnitClass::MessengerCavalry);

        auto lootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(*splitRouteStage, choice,
                                                /*surveyObjectiveSucceeded=*/false);
        };
        auto findLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        assert(findLoot(lootFor(jf::ExplorationChoice::FrontalAdvance), "frontier_edge_material") == 2);

        jf::BattleState splitRouteBattle = jf::createScenarioBattle(*data, *splitRouteStage, /*seed=*/29);
        int enemyCount = 0;
        for (const jf::Unit& unit : splitRouteBattle.units())
            if (unit.team == jf::Team::Enemy) ++enemyCount;
        assert(enemyCount == 6);

        for (jf::Unit& unit : splitRouteBattle.units())
            if (unit.team == jf::Team::Enemy) unit.currentHp = 0;

        jf::BattleObjectState* northMarker =
            splitRouteBattle.findObject("mapped_edge_split_survey_route_marker_north_1");
        assert(northMarker != nullptr);
        northMarker->interactionCount = 1; // only ONE of the 2 markers operated
        jf::syncObjectiveProgress(splitRouteBattle);
        assert(jf::evaluateBattleOutcome(splitRouteBattle).kind != jf::BattleOutcomeKind::Victory);

        jf::BattleObjectState* southMarker =
            splitRouteBattle.findObject("mapped_edge_split_survey_route_marker_south_1");
        assert(southMarker != nullptr);
        southMarker->interactionCount = 1; // both markers now operated
        jf::syncObjectiveProgress(splitRouteBattle);
        assert(jf::evaluateBattleOutcome(splitRouteBattle).kind == jf::BattleOutcomeKind::Victory);
    }

    {
        // docs/regions/mapped_edge.md「9地点仕様」地点5「放棄中継所」: primary
        // is 2 OperateObject Objectives (信号盤2個操作), mirroring
        // windwatch_station(M9-N)/sealed_passage(M9-AL)/fort_signal_yard(M9-AR)/
        // mapped_edge_split_survey_route(M9-AX)'s own dual-Device genuine
        // AND-group shape via objectPlacementRules/operateObjectiveId under
        // the default "primary"/ObjectiveGroupRule::All group - defeating
        // every enemy without operating both signal panels must NOT win,
        // operating only one must NOT win either, only both together wins.
        // 敗北条件「主盤0」はObject耐久ギャップとして本セッション既存の
        // crate/Device-primaryサイト全てと同様に見送り(部隊全滅は既存Engineで
        // 常時有効)。敵「人間敵6」は正本の「地形と脅威」節どおり既存クラスの
        // 再利用とし、M9-AV以降で確立済みの追跡してきた人間集団"Pursuer"とは
        // 別の中継所常駐要員という文脈のため、fort_signal_yard(M9-AR)と同型の
        // Bandit4+WatchArcher2混成へ表示名のみ新しいflavor"Relay Raider"を
        // 付けて再利用した(新規UnitClass・新規JAグリフ登録は無し)。素材
        // `quality_iron`・`ruin_fragment`はどちらも既存登録のためそのまま
        // 再利用。安定id`abandoned_relay_restored`は正本の「安定ID」節に記載
        // あるが、地点1〜4と同様、恒久成果配線自体は地域攻略Slice側の範囲
        // として未実装。
        auto data = jf::loadGameData(JF_SOURCE_DATA_DIR);
        assert(data);
        const jf::RegionDescriptor mappedEdgeRegion = jf::regionDescriptor(jf::RegionId::MappedEdge, *data);
        const jf::StageDescriptor* relayStage = nullptr;
        for (const jf::StageDescriptor& stage : mappedEdgeRegion.stages)
            if (stage.id == "mapped_edge_abandoned_relay") relayStage = &stage;
        assert(relayStage);
        assert(relayStage->enemyRoster.size() == 6); // 人間敵6
        assert(relayStage->scoutRouteRequiredClass == jf::UnitClass::FrontierEngineer);

        auto relayLootFor = [&](jf::ExplorationChoice choice) {
            return jf::computeStageVictoryLoot(*relayStage, choice,
                                                /*surveyObjectiveSucceeded=*/false);
        };
        auto findRelayLoot = [](const std::vector<jf::LootStack>& loot, const std::string& id) -> int {
            for (const jf::LootStack& stack : loot)
                if (stack.id == id) return stack.quantity;
            return 0;
        };
        std::vector<jf::LootStack> relayLoot = relayLootFor(jf::ExplorationChoice::FrontalAdvance);
        assert(findRelayLoot(relayLoot, "quality_iron") == 1);
        assert(findRelayLoot(relayLoot, "ruin_fragment") == 2);

        jf::BattleState relayBattle = jf::createScenarioBattle(*data, *relayStage, /*seed=*/31);
        int relayEnemyCount = 0;
        for (const jf::Unit& unit : relayBattle.units())
            if (unit.team == jf::Team::Enemy) ++relayEnemyCount;
        assert(relayEnemyCount == 6);

        for (jf::Unit& unit : relayBattle.units())
            if (unit.team == jf::Team::Enemy) unit.currentHp = 0;

        jf::BattleObjectState* westPanel =
            relayBattle.findObject("mapped_edge_abandoned_relay_panel_west_1");
        assert(westPanel != nullptr);
        westPanel->interactionCount = 1; // only ONE of the 2 panels operated
        jf::syncObjectiveProgress(relayBattle);
        assert(jf::evaluateBattleOutcome(relayBattle).kind != jf::BattleOutcomeKind::Victory);

        jf::BattleObjectState* eastPanel =
            relayBattle.findObject("mapped_edge_abandoned_relay_panel_east_1");
        assert(eastPanel != nullptr);
        eastPanel->interactionCount = 1; // both panels now operated
        jf::syncObjectiveProgress(relayBattle);
        assert(jf::evaluateBattleOutcome(relayBattle).kind == jf::BattleOutcomeKind::Victory);
    }

    {
        // docs/regions/ember_ravine.md "共通地形"「炎上床」/「冷却床」: 行動
        // 終了時、炎上床は炎上を確定付与し、冷却床は(ダメージ前に)炎上を解除
        // する。
        jf::Unit onFire = makeUnit("on_fire", jf::Team::Player, {0, 0});
        jf::Unit cooled = makeUnit("cooled", jf::Team::Player, {0, 1});
        std::array<jf::TerrainType, jf::kGridRows * jf::kGridCols> terrain{};
        terrain.fill(jf::TerrainType::Floor);
        terrain[0 * jf::kGridCols + 0] = jf::TerrainType::FireFloor;
        terrain[0 * jf::kGridCols + 1] = jf::TerrainType::CoolFloor;
        jf::BattleState battle({onFire, cooled}, terrain);
        jf::Unit* onFireUnit = battle.findUnit("on_fire");
        jf::Unit* cooledUnit = battle.findUnit("cooled");
        jf::applyBurn(*cooledUnit); // pre-existing Burn, standing on CoolFloor
        jf::processActionEndStatusEffects(battle, *onFireUnit);
        jf::processActionEndStatusEffects(battle, *cooledUnit);
        assert(onFireUnit->burnRemainingProcs > 0); // FireFloor's guaranteed Burn
        assert(cooledUnit->burnRemainingProcs == 0); // CoolFloor cleared it before damage
        assert(cooledUnit->currentHp == cooledUnit->stats.maxHp); // no damage this tick
    }

    {
        // docs/regions/ember_ravine.md 敵勢力「岩蜥蜴」: "各戦闘最初の炎上付与
        // だけ無効" - the first applyBurn() this battle is negated, the
        // second behaves normally.
        jf::Unit lizard = makeUnit("lizard", jf::Team::Enemy, {0, 0});
        lizard.firstBurnNegatesRemaining = 1;
        jf::applyBurn(lizard);
        assert(lizard.burnRemainingProcs == 0);
        assert(lizard.firstBurnNegatesRemaining == 0);
        jf::applyBurn(lizard);
        assert(lizard.burnRemainingProcs > 0);
    }

    {
        // docs/regions/ember_ravine.md "共通地形"「噴気予告床」: converts to
        // FireFloor at Round End, exactly 1 Round after being set.
        std::array<jf::TerrainType, jf::kGridRows * jf::kGridCols> terrain{};
        terrain.fill(jf::TerrainType::Floor);
        terrain[0] = jf::TerrainType::FumeWarning;
        jf::BattleState battle({}, terrain);
        jf::resolveEmberFumeRoundEnd(battle);
        assert(battle.terrainAt(jf::GridPos{0, 0}) == jf::TerrainType::FireFloor);
    }

    {
        // docs/regions/ember_ravine.md "戦場熱量" level 2: "各Round開始時、
        // 空きマス1個を噴気予告床にする" - no-op below level 2.
        std::array<jf::TerrainType, jf::kGridRows * jf::kGridCols> terrain{};
        terrain.fill(jf::TerrainType::EmberFloor);
        jf::BattleState belowLevel2({}, terrain);
        jf::resolveEmberHeatRoundStart(belowLevel2);
        assert(belowLevel2.terrainAt(jf::GridPos{0, 0}) == jf::TerrainType::EmberFloor);

        jf::BattleState atLevel2({}, terrain);
        atLevel2.setHeatLevel(2);
        jf::resolveEmberHeatRoundStart(atLevel2);
        bool anyFumeWarning = false;
        for (int row = 0; row < jf::kGridRows; ++row)
            for (int col = 0; col < jf::kGridCols; ++col)
                if (atLevel2.terrainAt(jf::GridPos{row, col}) == jf::TerrainType::FumeWarning) anyFumeWarning = true;
        assert(anyFumeWarning);
    }

    {
        // docs/regions/ember_ravine.md "戦場熱量" level 3: "各陣営Phase終了時、
        // 冷却床以外の全Unitへ固定1ダメージ。HP1で止まる" - CoolFloor exempts,
        // and the damage never drops a unit below 1 HP.
        jf::Unit exposed = makeUnit("exposed", jf::Team::Player, {0, 0});
        jf::Unit onCoolFloor = makeUnit("on_cool", jf::Team::Player, {0, 1});
        std::array<jf::TerrainType, jf::kGridRows * jf::kGridCols> terrain{};
        terrain.fill(jf::TerrainType::EmberFloor);
        terrain[0 * jf::kGridCols + 1] = jf::TerrainType::CoolFloor;
        jf::BattleState battle({exposed, onCoolFloor}, terrain);
        battle.setHeatLevel(3);
        jf::Unit* exposedUnit = battle.findUnit("exposed");
        exposedUnit->currentHp = 1;
        jf::resolveEmberHeatPhaseEnd(battle);
        assert(exposedUnit->currentHp == 1); // stopped at 1, not reduced to 0
        const jf::Unit* coolUnit = battle.findUnit("on_cool");
        assert(coolUnit->currentHp == coolUnit->stats.maxHp); // CoolFloor exemption
    }

    {
        // docs/regions/mapped_edge.md「地域攻略と最低保証」: the region-clear
        // floor top-up mechanism (M9-BD), same shape as CinderwatchGate's own
        // floor-topup test above - exercised directly against
        // jf::applyExpeditionReturnToBase() rather than replaying all 9 sites
        // through GameApp, since the mechanism itself only depends on
        // ExpeditionState::pendingRegionCompletions/pendingLoot and
        // BaseState::mappedEdgeMaterialsEarned/discoveryRegistry.
        jf::GameData data = makeFactoryData();
        jf::BaseState base;
        std::uint64_t returnGrantSequence = 0;

        // First (partial) return: below every floor value, region not yet
        // complete - should only bank the actual haul, no top-up.
        jf::ExpeditionState first;
        first.regionId = jf::RegionId::MappedEdge;
        first.pendingLoot = {{"rare_material", 1}, {"food", 1}};
        assert(jf::applyExpeditionReturnToBase(first, base, returnGrantSequence).success);
        assert(!base.completedRegionIds.count(jf::RegionId::MappedEdge));
        assert(base.mappedEdgeMaterialsEarned.at("rare_material") == 1);
        assert(base.storageCount("rare_material") == 1);
        assert(base.storageCount("food") == 1);
        assert(!base.discoveryRegistry.count(jf::kMappedEdgeSurveyRecordsDiscovery));

        // Second return: this run's own haul is still below every floor, but
        // it also completes the region (pendingRegionCompletions set, as
        // GameApp would do once every site reaches >= Surveyed) - the floor
        // top-up must fire and the Discovery backfill must apply.
        jf::ExpeditionState second;
        second.regionId = jf::RegionId::MappedEdge;
        second.pendingLoot = {{"herb", 1}};
        second.pendingRegionCompletions.insert(jf::RegionId::MappedEdge);
        assert(jf::applyExpeditionReturnToBase(second, base, returnGrantSequence).success);
        assert(base.completedRegionIds.count(jf::RegionId::MappedEdge));
        // Floor: 希少素材7、遺跡片8、地域固有素材3、高品質鉄材2、食料5、薬草4、
        // 最終キー素材1 (docs/regions/mapped_edge.md「地域攻略と最低保証」).
        assert(base.storageCount("rare_material") >= 7);
        assert(base.storageCount("ruin_fragment") >= 8);
        assert(base.storageCount("frontier_edge_material") >= 3);
        assert(base.storageCount("quality_iron") >= 2);
        assert(base.storageCount("food") >= 5);
        assert(base.storageCount("herb") >= 4);
        assert(base.storageCount("frontier_final_key") >= 1);
        // 地図外縁踏査記録(地点7「折れた見張台」記録箱2個保全、観測優先ルートでは
        // 個別に到達不能)はこの地域自身のフロア底上げで初めて到達可能になる。
        assert(base.discoveryRegistry.count(jf::kMappedEdgeSurveyRecordsDiscovery));

        // Re-clearing the (already-completed) region a 3rd time must not
        // reapply the floor top-up a second time.
        const int rareMaterialAfterCompletion = base.storageCount("rare_material");
        jf::ExpeditionState third;
        third.regionId = jf::RegionId::MappedEdge;
        third.pendingLoot = {{"rare_material", 1}};
        assert(jf::applyExpeditionReturnToBase(third, base, returnGrantSequence).success);
        assert(base.storageCount("rare_material") == rareMaterialAfterCompletion + 1);
    }

    std::cout << "Battle tests PASSED\n";
    return 0;
}
