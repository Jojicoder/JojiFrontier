#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "jf/battle/BattleEvents.hpp"
#include "jf/battle/BattleObject.hpp"
#include "jf/battle/Objective.hpp"
#include "jf/battle/Phase.hpp"
#include "jf/core/Grid.hpp"
#include "jf/core/Unit.hpp"
#include "jf/core/Terrain.hpp"
#include <unordered_map>

namespace jf {

// Pure battle data model: the roster, positions, and phase. Contains no
// rendering or input concerns so it can be driven headlessly (tests, AI,
// future netcode) as well as from the raylib front end.
class BattleState {
public:
    explicit BattleState(std::vector<Unit> units,
                         std::array<TerrainType, kGridRows * kGridCols> terrain = {},
                         std::uint32_t randomSeed = 0,
                         BattleMissionState mission = defaultEliminateEnemiesMission());

    const std::vector<Unit>& units() const { return units_; }
    std::vector<Unit>& units() { return units_; }

    Unit* unitAt(GridPos pos);
    const Unit* unitAt(GridPos pos) const;
    Unit* findUnit(const std::string& id);
    const Unit* findUnit(const std::string& id) const;

    Phase phase() const { return phase_; }
    // docs/mission_objectives.md: one round = one Player Phase + one Enemy
    // Phase. Starts at 1; advances when a new Player Phase begins.
    int round() const { return round_; }
    TerrainType terrainAt(GridPos pos) const;
    void setTerrain(GridPos pos, TerrainType terrain);
    int combatDefenseBonus(const Unit& defender, const Unit& attacker) const;
    int combatHitChance(const Unit& defender) const;
    bool rollAttackHit(const Unit& defender);
    bool consumeHerbPatch(Unit& unit, int healing = 5);
    int collectedHerbPatches() const { return collectedHerbPatches_; }

    bool moveUnit(Unit& unit, GridPos destination);
    void markActed(Unit& unit) { unit.hasActed = true; }

    // Heavy Spear effect: pushes `defender` one tile straight back from
    // `attacker`, unless the destination is blocked/out of bounds (silently
    // no-ops) or the defender has a Hide-Wrapped Grip negation banked (which
    // is consumed instead of the push happening).
    void applyKnockback(const Unit& attacker, Unit& defender);

    // True once every living unit on the given team has acted.
    bool isTeamDone(Team team) const;

    void beginPlayerPhase();
    void beginEnemyPhase();

    bool allEnemiesDefeated() const;
    bool allPlayersDefeated() const;

    // docs/mission_objectives.md "所有権と責務": BattleState owns the
    // mission's Definition/Progress/consumed-event-id state.
    BattleMissionState& missionState() { return mission_; }
    const BattleMissionState& missionState() const { return mission_; }

    // Stamps and returns the next BattleEventId (starts at 1, monotonically
    // increasing for the life of the battle).
    BattleEventId issueEventId() { return mission_.nextEventId++; }

    // docs/battle_objects.md: Battle Object storage + placement. Definitions
    // are immutable/shared; States are the actual placed instances (several
    // States may share one Definition, e.g. several identical fallen trees).
    // Fails (returns false, appends to `errors` if given) on an invalid
    // Definition (see validateObjectDefinition()) or a duplicate id.
    bool registerObjectDefinition(BattleObjectDefinition definition, std::vector<std::string>* errors = nullptr);
    const BattleObjectDefinition* objectDefinition(const BattleObjectDefinitionId& definitionId) const;

    // Fails (returns false) if `state.definitionId` isn't registered, the
    // position is out of bounds, or another object already occupies it -
    // per the doc, two objects never share a tile (unlike Unit+Marker,
    // which is a Unit/Object occupancy question, not an Object/Object one).
    bool placeObject(BattleObjectState state);

    BattleObjectState* objectAt(GridPos pos);
    const BattleObjectState* objectAt(GridPos pos) const;
    BattleObjectState* findObject(const BattleObjectId& id);
    const BattleObjectState* findObject(const BattleObjectId& id) const;
    const std::vector<BattleObjectState>& objects() const { return objects_; }

    // These all read the Definition+State pair to answer the doc's "占有と
    //通行" rules; a Destroyed object never blocks anything, regardless of
    // what its Definition says (docs/battle_objects.md "破壊後の状態").
    bool objectBlocksMovementAt(GridPos pos) const;
    bool objectBlocksStoppingAt(GridPos pos) const;
    bool objectBlocksDeploymentAt(GridPos pos) const;
    bool objectBlocksProjectilesAt(GridPos pos) const;

    // docs/regions/ashbough_forest.md "折れ木の縄張り"'s
    // broken_boughs_log_collision secondary: true once any boss has
    // collided with a Barrier at least once this battle, regardless of how
    // many times. Battle-scoped (never saved, matches every other
    // mid-battle-only piece of state here).
    bool bossHasCollidedWithBarrier() const { return bossCollidedWithBarrier_; }
    void markBossCollidedWithBarrier() { bossCollidedWithBarrier_ = true; }

private:
    std::vector<Unit> units_;
    std::array<TerrainType, kGridRows * kGridCols> terrain_{};
    Phase phase_ = Phase::PlayerPhase;
    int round_ = 1;
    std::uint32_t randomSeed_ = 0;
    std::uint64_t attackRollIndex_ = 0;
    int collectedHerbPatches_ = 0;
    BattleMissionState mission_;
    std::unordered_map<BattleObjectDefinitionId, BattleObjectDefinition> objectDefinitions_;
    std::vector<BattleObjectState> objects_;
    bool bossCollidedWithBarrier_ = false;
};

} // namespace jf
