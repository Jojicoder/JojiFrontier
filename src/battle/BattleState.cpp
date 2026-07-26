#include "jf/battle/BattleState.hpp"

#include "jf/battle/StatusEffects.hpp"
#include "jf/battle/ObjectiveTracker.hpp"

#include <algorithm>

namespace jf {

BattleState::BattleState(std::vector<Unit> units,
                         std::array<TerrainType, kGridRows * kGridCols> terrain,
                         std::uint32_t randomSeed,
                         BattleMissionState mission)
    : units_(std::move(units)), terrain_(terrain), randomSeed_(randomSeed), mission_(std::move(mission)) {}

TerrainType BattleState::terrainAt(GridPos pos) const {
    if (!isInBounds(pos)) return TerrainType::Barrier;
    return terrain_[pos.row * kGridCols + pos.col];
}

void BattleState::setTerrain(GridPos pos, TerrainType terrain) {
    if (isInBounds(pos)) terrain_[pos.row * kGridCols + pos.col] = terrain;
}

int BattleState::combatDefenseBonus(const Unit& defender, const Unit& attacker) const {
    int bonus = defenseBonus(terrainAt(defender.position));
    for (const Unit& ally : units_) {
        if (!ally.isAlive() || ally.team != defender.team || !providesFormationBonus(ally.unitClass)) continue;
        if (&ally == &defender) continue;
        // Command Sword(号令剣, docs/base_development.md「M7項目3続き 武器分岐
        // 固有効果(高コストTier)」): extends this provider's Formation Bonus
        // radius from adjacent(1) to distance 2, but restricts the actual DEF+1
        // to whichever 2 same-team allies are nearest to the provider (not
        // every ally within radius). Every other provider keeps the plain
        // "adjacent(1), no filter" shape.
        const int radius = ally.weapon.id == "command_sword" ? 2 : 1;
        if (manhattanDistance(ally.position, defender.position) > radius) continue;
        if (ally.weapon.id == "command_sword") {
            std::vector<const Unit*> nearby;
            for (const Unit& other : units_) {
                if (!other.isAlive() || other.team != ally.team || &other == &ally) continue;
                if (manhattanDistance(ally.position, other.position) <= radius) nearby.push_back(&other);
            }
            std::sort(nearby.begin(), nearby.end(), [&](const Unit* a, const Unit* b) {
                const int da = manhattanDistance(ally.position, a->position);
                const int db = manhattanDistance(ally.position, b->position);
                if (da != db) return da < db;
                return a->id < b->id; // undocumented tie-break, deterministic
            });
            bool inNearestTwo = false;
            for (std::size_t i = 0; i < nearby.size() && i < 2; ++i) {
                if (nearby[i] == &defender) inNearestTwo = true;
            }
            if (!inNearestTwo) continue;
        }
        ++bonus;
        break;
    }
    // Guard Sword(護衛剣): once per battle, an adjacent ally's first hit taken
    // is reduced by 3. Read here dynamically (so previewAttack() reflects it
    // too) - actual per-battle consumption happens in CombatResolver.cpp's
    // resolveAttack() the moment a real hit lands.
    for (const Unit& ally : units_) {
        if (!ally.isAlive() || ally.team != defender.team || ally.weapon.id != "guard_sword") continue;
        if (&ally == &defender || ally.guardSwordShieldUsed) continue;
        if (manhattanDistance(ally.position, defender.position) == 1) {
            bonus += 3;
            break;
        }
    }
    // Warding Standard(守護旗槍, 旗手): replaces 戦旗's usual STR/MAG+1 offensive
    // aura with a defensive DEF+1/RES+1 aura instead. Applied here rather than
    // CombatResolver.cpp's bannerAuraBonus() since this single return value is
    // already subtracted from whichever of DEF/RES the attack actually uses
    // (see CombatResolver.cpp's computeDamage()), so adding it once here grants
    // both stats symmetrically without a separate RES-only code path. Same
    // radius(2)/no-self-application rule as the offensive 戦旗 aura.
    for (const Unit& bearer : units_) {
        if (!bearer.isAlive() || bearer.team != defender.team || bearer.weapon.id != "warding_standard") continue;
        if (&bearer == &defender) continue;
        if (manhattanDistance(bearer.position, defender.position) <= 2) {
            ++bonus;
            break;
        }
    }
    if ((hasBrace(defender.unitClass) || defender.weapon.braceBoost || defender.braceSkillActive) &&
        attacker.tilesMovedThisAction >= 2) {
        bonus += defender.weapon.braceBoost ? 3 : 2;
    }
    // 連携作戦`paired_braced_breakthrough`(支え合う突破): same "attacker moved
    // >=2 tiles this action" trigger as the Brace-family check above, DEF+2,
    // stacks with it (independent sources - docs doesn't say otherwise, and
    // skill_system.md's DEF cap already bounds the combined total).
    if (defender.pairedBracedBreakthroughActive && attacker.tilesMovedThisAction >= 2) bonus += 2;
    return bonus;
}

int BattleState::combatHitChance(const Unit& defender) const {
    const int totalEvasion = evasionBonus(terrainAt(defender.position)) +
                             passiveEvasionBonus(defender.unitClass);
    return std::clamp(100 - totalEvasion, 0, 100);
}

bool BattleState::rollAttackHit(const Unit& defender) {
    const int chance = combatHitChance(defender);
    if (chance >= 100) return true;
    if (chance <= 0) return false;
    std::uint32_t value = randomSeed_ ^ static_cast<std::uint32_t>(++attackRollIndex_ * 0x9e3779b9ULL);
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return static_cast<int>(value % 100U) < chance;
}

bool BattleState::consumeHerbPatch(Unit& unit, int healing) {
    if (unit.team != Team::Player || !unit.isAlive() || healing <= 0 ||
        terrainAt(unit.position) != TerrainType::HerbPatch) return false;
    unit.currentHp = std::min(unit.currentHp + healing, unit.stats.maxHp);
    setTerrain(unit.position, TerrainType::Floor);
    ++collectedHerbPatches_;
    return true;
}

void BattleState::applyKnockback(const Unit& attacker, Unit& defender) {
    // 重装兵「重量装甲」(常時、無条件)と`brace_for_impact`(次の自分の行動終了まで)
    // はどちらも消費型ではない - Hide-Wrapped Gripの`knockbackNegatesRemaining`
    // (1回限りの消費)とは別扱いで、カウンタを一切減らさず毎回無効化する。
    if (hasHeavyArmor(defender.unitClass) || defender.braceForImpactActive || defender.pairedBracedBreakthroughActive) return;
    if (defender.knockbackNegatesRemaining > 0) {
        --defender.knockbackNegatesRemaining;
        return;
    }
    const int rowDelta = defender.position.row - attacker.position.row;
    const int colDelta = defender.position.col - attacker.position.col;
    GridPos dest = defender.position;
    // A grid knockback always travels exactly one orthogonal tile. Horizontal
    // wins ties so diagonal range attacks cannot skip across a blocked corner.
    if (std::abs(colDelta) >= std::abs(rowDelta)) dest.col += (colDelta > 0) - (colDelta < 0);
    else dest.row += (rowDelta > 0) - (rowDelta < 0);
    // 状態異常「よろめき」(docs/status_effects.md "主な発生源": 障害物への
    // ノックバック衝突): a knockback that can't reach its destination -
    // whether blocked by bounds, terrain, another unit, or a Battle Object
    // (this previously wasn't checked at all here, letting a knockback
    // silently ignore a Barrier like a fallen log) - staggers the defender
    // in place instead of just doing nothing.
    if (!isInBounds(dest) || unitAt(dest) || !isPassable(terrainAt(dest)) || objectBlocksMovementAt(dest) ||
        objectBlocksStoppingAt(dest)) {
        applyStagger(*this, defender);
        return;
    }
    defender.position = dest;
}

void BattleState::applyPull(const Unit& attacker, Unit& defender) {
    if (hasHeavyArmor(defender.unitClass) || defender.braceForImpactActive || defender.pairedBracedBreakthroughActive) return;
    if (defender.knockbackNegatesRemaining > 0) {
        --defender.knockbackNegatesRemaining;
        return;
    }
    const int rowDelta = defender.position.row - attacker.position.row;
    const int colDelta = defender.position.col - attacker.position.col;
    GridPos dest = defender.position;
    // Same axis-priority tie-break as applyKnockback(), just toward instead
    // of away from the attacker.
    if (std::abs(colDelta) >= std::abs(rowDelta)) dest.col -= (colDelta > 0) - (colDelta < 0);
    else dest.row -= (rowDelta > 0) - (rowDelta < 0);
    if (!isInBounds(dest) || unitAt(dest) || !isPassable(terrainAt(dest)) || objectBlocksMovementAt(dest) ||
        objectBlocksStoppingAt(dest)) {
        return; // "空いていれば" - simply no-op if blocked, no stagger.
    }
    defender.position = dest;
}

// docs/regions/windscar_plateau.md "強風ルール" - see the declaration's
// comment (BattleState.hpp) for the full rule summary.
void resolveWindGustRoundEnd(BattleState& battle) {
    const auto& config = battle.windGust();
    if (!config || battle.round() != config->triggerRound) return;
    // Snapshot the unit ids on wind tiles before moving anyone, so a unit
    // pushed onto another wind tile this same resolution doesn't get pushed
    // twice in one Round End.
    std::vector<std::string> windedUnitIds;
    for (const Unit& unit : battle.units()) {
        if (unit.isAlive() && battle.terrainAt(unit.position) == TerrainType::WindGust)
            windedUnitIds.push_back(unit.id);
    }
    for (const std::string& id : windedUnitIds) {
        Unit* unit = battle.findUnit(id);
        if (!unit || !unit->isAlive()) continue;
        if (hasHeavyArmor(unit->unitClass) || unit->pairedBracedBreakthroughActive) continue;
        GridPos dest = unit->position;
        dest.row += config->delta.row;
        dest.col += config->delta.col;
        if (!isInBounds(dest) || battle.unitAt(dest) || !isPassable(battle.terrainAt(dest)) ||
            battle.objectBlocksMovementAt(dest) || battle.objectBlocksStoppingAt(dest)) {
            unit->currentHp = std::max(0, unit->currentHp - 2);
            continue;
        }
        unit->position = dest;
    }
}

// docs/regions/ember_ravine.md "共通地形"「噴気予告床」 - see the declaration's
// comment (BattleState.hpp) for the full rule summary. Snapshot-then-convert
// (same shape as resolveWindGustRoundEnd() above) even though converting a
// tile can't itself trigger a second conversion, for consistency with that
// function's style.
void resolveEmberFumeRoundEnd(BattleState& battle) {
    std::vector<GridPos> fumeTiles;
    for (int row = 0; row < kGridRows; ++row) {
        for (int col = 0; col < kGridCols; ++col) {
            GridPos pos{row, col};
            if (battle.terrainAt(pos) == TerrainType::FumeWarning) fumeTiles.push_back(pos);
        }
    }
    for (const GridPos& pos : fumeTiles) battle.setTerrain(pos, TerrainType::FireFloor);
}

// docs/regions/ember_ravine.md "戦場熱量" level 2 - see the declaration's
// comment (BattleState.hpp).
void resolveEmberHeatRoundStart(BattleState& battle) {
    if (battle.heatLevel() < 2) return;
    for (int row = 0; row < kGridRows; ++row) {
        for (int col = 0; col < kGridCols; ++col) {
            GridPos pos{row, col};
            TerrainType terrain = battle.terrainAt(pos);
            if (terrain != TerrainType::EmberFloor && terrain != TerrainType::HotSand &&
                terrain != TerrainType::Floor) {
                continue;
            }
            if (battle.unitAt(pos) || battle.objectBlocksDeploymentAt(pos)) continue;
            battle.setTerrain(pos, TerrainType::FumeWarning);
            return;
        }
    }
}

// docs/regions/ember_ravine.md "戦場熱量" level 3 - see the declaration's
// comment (BattleState.hpp).
void resolveEmberHeatPhaseEnd(BattleState& battle) {
    if (battle.heatLevel() < 3) return;
    for (Unit& unit : battle.units()) {
        if (!unit.isAlive() || battle.terrainAt(unit.position) == TerrainType::CoolFloor) continue;
        unit.currentHp = std::max(unit.currentHp - 1, 1);
    }
}

Unit* BattleState::unitAt(GridPos pos) {
    for (auto& u : units_) {
        if (u.isPresent() && u.position == pos) return &u;
    }
    return nullptr;
}

const Unit* BattleState::unitAt(GridPos pos) const {
    for (const auto& u : units_) {
        if (u.isPresent() && u.position == pos) return &u;
    }
    return nullptr;
}

Unit* BattleState::findUnit(const std::string& id) {
    auto it = std::find_if(units_.begin(), units_.end(),
                            [&](const Unit& u) { return u.id == id; });
    return it == units_.end() ? nullptr : &(*it);
}

const Unit* BattleState::findUnit(const std::string& id) const {
    auto it = std::find_if(units_.begin(), units_.end(),
                            [&](const Unit& u) { return u.id == id; });
    return it == units_.end() ? nullptr : &(*it);
}

bool BattleState::moveUnit(Unit& unit, GridPos destination) {
    if (!isInBounds(destination)) return false;
    if (!isPassable(terrainAt(destination))) return false;
    if (objectBlocksMovementAt(destination) || objectBlocksStoppingAt(destination)) return false;
    Unit* occupant = unitAt(destination);
    if (occupant != nullptr && occupant != &unit) return false;
    unit.tilesMovedThisAction = manhattanDistance(unit.position, destination);
    const GridPos origin = unit.position;
    unit.position = destination;
    // Fortress Lance(城塞槍): entering (not merely already standing in) a
    // Fortress-Lance-wielding enemy's Zone of Control (range 1 - this branch
    // doesn't extend ZoC range) costs -2 damage dealt until the entering
    // unit's next action ends (CombatResolver.cpp's computeDamage() reads
    // it, markActed() below clears it). Checked here rather than at each
    // individual move call site since every movement path (normal move,
    // re-move, self-movement skills, EnemyAI, bosses) funnels through this
    // one function.
    for (const Unit& lancer : units_) {
        if (!lancer.isPresent() || lancer.team == unit.team || lancer.weapon.id != "fortress_lance") continue;
        if (manhattanDistance(lancer.position, destination) <= 1 && manhattanDistance(lancer.position, origin) > 1) {
            unit.zocEntryDamageDownActive = true;
            break;
        }
    }
    return true;
}

void BattleState::markTrailblazed(GridPos pos) {
    if (!isTrailblazed(pos)) trailblazedTiles_.push_back(pos);
}

bool BattleState::isTrailblazed(GridPos pos) const {
    return std::find(trailblazedTiles_.begin(), trailblazedTiles_.end(), pos) != trailblazedTiles_.end();
}

bool BattleState::isTeamDone(Team team) const {
    for (const auto& u : units_) {
        // isPresent() (not isAlive()): a unit that retreated (docs/
        // enemy_ai_rules.md) has isAlive()==true (HP unaffected) but never
        // gets marked hasActed again after a later beginEnemyPhase() reset
        // - without this, it would permanently block this phase from ever
        // completing.
        if (u.team == team && u.isPresent() && !u.hasActed) return false;
    }
    return true;
}

void BattleState::beginPlayerPhase() {
    phase_ = Phase::PlayerPhase;
    ++round_;
    for (auto& u : units_) {
        if (u.team == Team::Player) {
            u.hasActed = false;
            u.tilesMovedThisAction = 0;
            // Patrol Lance(巡回槍): DEF+2 lasts "until the next Player Phase
            // begins" - this is that moment.
            u.patrolLanceReadyDefenseActive = false;
        }
    }
    announceReinforcements();
    resolveReinforcementsForPhase();
}

void BattleState::beginEnemyPhase() {
    phase_ = Phase::EnemyPhase;
    for (auto& u : units_) {
        if (u.team == Team::Enemy) {
            u.hasActed = false;
            u.tilesMovedThisAction = 0;
        }
    }
    announceReinforcements();
    resolveReinforcementsForPhase();
}

bool BattleState::addReinforcementWave(ReinforcementWave wave) {
    std::vector<ReinforcementWave> proposed = reinforcementWaves_;
    proposed.push_back(wave);
    if (!validateReinforcementWaves(proposed, false)) return false;
    reinforcementWaves_.push_back(std::move(wave));
    announceReinforcements();
    return true;
}

void BattleState::announceReinforcements() {
    for (ReinforcementWave& wave : reinforcementWaves_) {
        if (wave.state != ReinforcementState::Scheduled) continue;
        if (round_ < wave.spawnRound - wave.announceRoundsBefore) continue;
        wave.state = ReinforcementState::Announced;
        wave.announcementConsumed = true;
        handleObjectiveEvent(mission_, {issueEventId(), 0, ReinforcementAnnouncedEvent{wave.id, wave.spawnRound}});
    }
}

void BattleState::resolveReinforcementsForPhase() {
    for (ReinforcementWave& wave : reinforcementWaves_) {
        if ((wave.state != ReinforcementState::Scheduled && wave.state != ReinforcementState::Announced) ||
            wave.spawnRound != round_ || wave.spawnPhase != phase_) continue;

        std::vector<GridPos> placements;
        for (GridPos pos : wave.orderedSpawnCandidates) {
            const BattleObjectState* object = objectAt(pos);
            bool objectAllowsSpawn = true;
            if (object && object->state != BattleObjectStateKind::Destroyed) {
                const BattleObjectDefinition* def = objectDefinition(object->definitionId);
                objectAllowsSpawn = def && def->kind == BattleObjectKind::SpawnPoint;
            }
            if (!isInBounds(pos) || !isPassable(terrainAt(pos)) || unitAt(pos) || !objectAllowsSpawn) continue;
            placements.push_back(pos);
            if (placements.size() == wave.units.size()) break;
        }
        if (placements.size() == wave.units.size()) {
            for (std::size_t i = 0; i < wave.units.size(); ++i) {
                Unit unit = wave.units[i].unit;
                unit.team = wave.team;
                unit.position = placements[i];
                unit.hasActed = true;
                unit.lastActedRound = round_;
                units_.push_back(std::move(unit));
            }
            wave.state = ReinforcementState::Spawned;
        } else {
            wave.state = ReinforcementState::Prevented;
        }
        const ReinforcementResult result = wave.state == ReinforcementState::Spawned
                                               ? ReinforcementResult::Spawned : ReinforcementResult::Prevented;
        handleObjectiveEvent(mission_, {issueEventId(), 0, ReinforcementResolvedEvent{wave.id, result}});
    }
}

bool BattleState::hasPendingRequiredEnemyReinforcements() const {
    return std::any_of(reinforcementWaves_.begin(), reinforcementWaves_.end(), [](const ReinforcementWave& wave) {
        return wave.team == Team::Enemy && wave.requiredForElimination &&
               (wave.state == ReinforcementState::Scheduled || wave.state == ReinforcementState::Announced);
    });
}

bool BattleState::allEnemiesDefeated() const {
    if (hasPendingRequiredEnemyReinforcements()) return false;
    return std::none_of(units_.begin(), units_.end(),
                         [](const Unit& u) { return u.team == Team::Enemy && u.isPresent(); });
}

bool BattleState::allPlayersDefeated() const {
    return std::none_of(units_.begin(), units_.end(),
                         [](const Unit& u) { return u.team == Team::Player && u.isAlive(); });
}

bool BattleState::allGuestsLost() const {
    if (mission_.guestUnitIds.empty()) return false;
    return std::all_of(mission_.guestUnitIds.begin(), mission_.guestUnitIds.end(), [this](const std::string& id) {
        const Unit* u = findUnit(id);
        return u == nullptr || !u->isPresent();
    });
}

bool BattleState::registerObjectDefinition(BattleObjectDefinition definition, std::vector<std::string>* errors) {
    if (!validateObjectDefinition(definition, errors)) return false;
    if (objectDefinitions_.count(definition.definitionId)) {
        if (errors) errors->push_back("duplicate object definition id: " + definition.definitionId);
        return false;
    }
    objectDefinitions_[definition.definitionId] = std::move(definition);
    return true;
}

const BattleObjectDefinition* BattleState::objectDefinition(const BattleObjectDefinitionId& definitionId) const {
    auto it = objectDefinitions_.find(definitionId);
    return it == objectDefinitions_.end() ? nullptr : &it->second;
}

bool BattleState::placeObject(BattleObjectState state) {
    const BattleObjectDefinition* def = objectDefinition(state.definitionId);
    if (!def) return false;
    if (!isInBounds(state.position)) return false;
    if (objectAt(state.position)) return false;
    // Fresh placement starts at full durability unless the caller already
    // supplied one (e.g. restoring a mid-battle Save Snapshot per
    // docs/battle_objects.md "Save" - current durability/state/position win
    // over the Definition there).
    if (state.durability <= 0 && def->maxDurability > 0) state.durability = def->maxDurability;
    objects_.push_back(std::move(state));
    return true;
}

BattleObjectState* BattleState::objectAt(GridPos pos) {
    for (auto& object : objects_) {
        if (object.position == pos) return &object;
    }
    return nullptr;
}

const BattleObjectState* BattleState::objectAt(GridPos pos) const {
    for (const auto& object : objects_) {
        if (object.position == pos) return &object;
    }
    return nullptr;
}

BattleObjectState* BattleState::findObject(const BattleObjectId& id) {
    auto it = std::find_if(objects_.begin(), objects_.end(),
                           [&](const BattleObjectState& object) { return object.id == id; });
    return it == objects_.end() ? nullptr : &(*it);
}

const BattleObjectState* BattleState::findObject(const BattleObjectId& id) const {
    auto it = std::find_if(objects_.begin(), objects_.end(),
                           [&](const BattleObjectState& object) { return object.id == id; });
    return it == objects_.end() ? nullptr : &(*it);
}

namespace {
// Destroyed objects never block anything (docs/battle_objects.md "破壊後の
//状態": Barriers become passable, Devices/Containers/etc. are inert).
bool objectBlocksAt(const BattleState& battle, GridPos pos, bool (*flagOf)(const BattleObjectDefinition&)) {
    const BattleObjectState* object = battle.objectAt(pos);
    if (!object || object->state == BattleObjectStateKind::Destroyed) return false;
    const BattleObjectDefinition* def = battle.objectDefinition(object->definitionId);
    return def && flagOf(*def);
}
} // namespace

bool BattleState::objectBlocksMovementAt(GridPos pos) const {
    return objectBlocksAt(*this, pos, [](const BattleObjectDefinition& def) { return def.blocksMovement; });
}

bool BattleState::objectBlocksStoppingAt(GridPos pos) const {
    return objectBlocksAt(*this, pos, [](const BattleObjectDefinition& def) { return def.blocksStopping; });
}

bool BattleState::objectBlocksDeploymentAt(GridPos pos) const {
    return objectBlocksAt(*this, pos, [](const BattleObjectDefinition& def) { return def.blocksDeployment; });
}

bool BattleState::objectBlocksProjectilesAt(GridPos pos) const {
    return objectBlocksAt(*this, pos, [](const BattleObjectDefinition& def) { return def.blocksProjectiles; });
}

} // namespace jf
