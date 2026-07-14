#include "jf/battle/BattleState.hpp"

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
        if (&ally != &defender && manhattanDistance(ally.position, defender.position) == 1) {
            ++bonus;
            break;
        }
    }
    if ((hasBrace(defender.unitClass) || defender.weapon.braceBoost) && attacker.tilesMovedThisAction >= 2) {
        bonus += defender.weapon.braceBoost ? 3 : 2;
    }
    return bonus;
}

int BattleState::combatHitChance(const Unit& defender) const {
    return std::clamp(100 - evasionBonus(terrainAt(defender.position)), 0, 100);
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
    if (!isInBounds(dest) || unitAt(dest) || !isPassable(terrainAt(dest))) return;
    defender.position = dest;
}

Unit* BattleState::unitAt(GridPos pos) {
    for (auto& u : units_) {
        if (u.isAlive() && u.position == pos) return &u;
    }
    return nullptr;
}

const Unit* BattleState::unitAt(GridPos pos) const {
    for (const auto& u : units_) {
        if (u.isAlive() && u.position == pos) return &u;
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
    unit.position = destination;
    return true;
}

bool BattleState::isTeamDone(Team team) const {
    for (const auto& u : units_) {
        if (u.team == team && u.isAlive() && !u.hasActed) return false;
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
        }
    }
}

void BattleState::beginEnemyPhase() {
    phase_ = Phase::EnemyPhase;
    for (auto& u : units_) {
        if (u.team == Team::Enemy) {
            u.hasActed = false;
            u.tilesMovedThisAction = 0;
        }
    }
}

bool BattleState::allEnemiesDefeated() const {
    return std::none_of(units_.begin(), units_.end(),
                         [](const Unit& u) { return u.team == Team::Enemy && u.isAlive(); });
}

bool BattleState::allPlayersDefeated() const {
    return std::none_of(units_.begin(), units_.end(),
                         [](const Unit& u) { return u.team == Team::Player && u.isAlive(); });
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
