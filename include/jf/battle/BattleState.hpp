#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "jf/battle/BattleEvents.hpp"
#include "jf/battle/BattleObject.hpp"
#include "jf/battle/Objective.hpp"
#include "jf/battle/Phase.hpp"
#include "jf/battle/Reinforcement.hpp"
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
    // Also stamps `lastActedRound` (see its own comment on `Unit`) so
    // ambush_blade's "hasn't acted this round" check survives across Phase
    // boundaries, unlike raw `hasActed` which only resets at the unit's own
    // team's next Phase start.
    void markActed(Unit& unit) {
        unit.hasActed = true;
        unit.lastActedRound = round_;
        // Fortress Lance's ZoC-entry debuff (see moveUnit()'s own comment)
        // lasts exactly through the entering unit's next action - this is
        // that action ending, so clear it here regardless of action kind.
        unit.zocEntryDamageDownActive = false;
    }

    // Heavy Spear effect: pushes `defender` one tile straight back from
    // `attacker`, unless the destination is blocked/out of bounds (silently
    // no-ops) or the defender has a Hide-Wrapped Grip negation banked (which
    // is consumed instead of the push happening).
    void applyKnockback(const Unit& attacker, Unit& defender);

    // Hook Lance effect (docs/base_development.md): pulls `defender` one
    // tile TOWARD `attacker` instead of away - the inverse of
    // applyKnockback() above. Reuses the same negation rules (Heavy Guard's
    // hasHeavyArmor()/brace_for_impact, Hide-Wrapped Grip's
    // knockbackNegatesRemaining) since "cannot be knocked back" reads as
    // "cannot be forcibly repositioned by an attack" either way, and simply
    // no-ops (no stagger, unlike applyKnockback()) if the destination isn't
    // free - Hook Lance's own doc wording is "空いていれば" (only if open),
    // with no mention of a collision penalty.
    void applyPull(const Unit& attacker, Unit& defender);

    // True once every living unit on the given team has acted.
    bool isTeamDone(Team team) const;

    void beginPlayerPhase();
    void beginEnemyPhase();

    bool addReinforcementWave(ReinforcementWave wave);
    void announceReinforcements();
    void resolveReinforcementsForPhase();
    bool hasPendingRequiredEnemyReinforcements() const;
    const std::vector<ReinforcementWave>& reinforcementWaves() const { return reinforcementWaves_; }

    bool allEnemiesDefeated() const;
    bool allPlayersDefeated() const;
    bool allGuestsLost() const;

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

    // 辺境斥候`trailblaze`(道拓き) (docs/initial_skill_effects.md): Ash/
    // Shallows tiles the caster passed through this Player Phase, which
    // cost every ally only 1 to cross for the rest of it (see Movement.cpp's
    // computeReachableTilesImpl()'s costOverrideAt). Cleared alongside
    // moveUpActive at Player Phase end (both are "until THIS Player Phase
    // ends" effects) - see BattleController::evaluateOutcome().
    void markTrailblazed(GridPos pos);
    bool isTrailblazed(GridPos pos) const;
    void clearTrailblazedTiles() { trailblazedTiles_.clear(); }

    // docs/regions/windscar_plateau.md "強風ルール": fixed per-battle wind
    // direction (`delta` is the 1-tile push offset, e.g. {1,0} = downward)
    // and the single Round it triggers on. Optional - stages without a
    // WindGust terrain profile leave this unset and
    // resolveWindGustRoundEnd() is a no-op.
    struct WindGustConfig {
        GridPos delta;
        int triggerRound = 0;
    };
    void setWindGust(std::optional<WindGustConfig> config) { windGust_ = config; }
    const std::optional<WindGustConfig>& windGust() const { return windGust_; }

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
    std::vector<GridPos> trailblazedTiles_;
    std::vector<ReinforcementWave> reinforcementWaves_;
    std::optional<WindGustConfig> windGust_;
};

// docs/regions/windscar_plateau.md "強風ルール": called at Round End (same
// point processPhaseEndStatusEffects() already applies poison/burn damage -
// see BattleController's Enemy-Phase-end block) when `battle.round()` equals
// the configured trigger Round. Pushes every unit standing on a WindGust
// tile 1 tile along `delta`; a blocked destination (out of bounds, another
// Unit, impassable terrain/Object) deals a fixed 2 damage instead of moving
// (docs' collision rule - deliberately NOT BattleState::applyKnockback()'s
// stagger-status outcome, the doc calls for damage here). Heavy Guard's
// armor (hasHeavyArmor()) negates both the push and the collision damage,
// reusing the same check applyKnockback() already uses for the identical
// "重量装甲は移動距離を0にする" rule. Never moves/damages a unit already at
// 0 HP, never auto-completes objectives (it only ever changes position/HP).
void resolveWindGustRoundEnd(BattleState& battle);

} // namespace jf
