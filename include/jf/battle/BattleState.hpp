#pragma once

#include <vector>

#include "jf/core/Grid.hpp"
#include "jf/core/Unit.hpp"

namespace jf {

enum class Phase {
    PlayerPhase,
    EnemyPhase
};

// Pure battle data model: the roster, positions, and phase. Contains no
// rendering or input concerns so it can be driven headlessly (tests, AI,
// future netcode) as well as from the raylib front end.
class BattleState {
public:
    explicit BattleState(std::vector<Unit> units);

    const std::vector<Unit>& units() const { return units_; }
    std::vector<Unit>& units() { return units_; }

    Unit* unitAt(GridPos pos);
    const Unit* unitAt(GridPos pos) const;
    Unit* findUnit(const std::string& id);

    Phase phase() const { return phase_; }

    bool moveUnit(Unit& unit, GridPos destination);
    void markActed(Unit& unit) { unit.hasActed = true; }

    // True once every living unit on the given team has acted.
    bool isTeamDone(Team team) const;

    void beginPlayerPhase();
    void beginEnemyPhase();

    bool allEnemiesDefeated() const;
    bool allPlayersDefeated() const;

private:
    std::vector<Unit> units_;
    Phase phase_ = Phase::PlayerPhase;
};

} // namespace jf
