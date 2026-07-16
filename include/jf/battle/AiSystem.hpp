#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "jf/battle/BattleState.hpp"

namespace jf {

enum class AiProfileId { Wolf, Human, Defender, Ranged, Support, Bandit };
enum class AiActionType { Attack, Move, Support, Wait, Retreat };

struct AiCandidate {
    AiActionType type = AiActionType::Wait;
    GridPos destination{};
    std::string targetUnitId;
    std::string actionId = "wait";
    int score = 0;
    int missionProgress = 0;
    int predictedDamage = 0;
    int predictedIncomingDamage = 0;
    int movementCost = 0;
    bool defeatsTarget = false;
};

struct AiProfile {
    AiProfileId id = AiProfileId::Human;
    int damageWeight = 40;
    int defeatBonus = 1500;
    int lowHpWeight = 1;
    int distanceWeight = 20;
    int dangerWeight = 20;
    int cohesionWeight = 0;
    int preferredRange = 1;
    int pursuitLimit = kGridCols;
    // docs/enemy_ai_rules.md "撤退と降伏": HP% (of max) at or below which a
    // Retreat candidate is generated - Wildlife (Wolf) uses 20, everyone
    // else (Human-derived profiles) uses the doc's 25. Bosses never read
    // this (they don't go through generateAiCandidates() at all).
    int retreatHpPercent = 25;
};

struct AiSquadReservations {
    std::unordered_set<int> destinationKeys;
    std::unordered_map<std::string, int> reservedDamage;
    std::unordered_set<std::string> supportTargets;

    void clear();
    void reserve(const AiCandidate& candidate);
};

AiProfile profileFor(const Unit& unit);
std::vector<AiCandidate> generateAiCandidates(const BattleState& battle,
                                               const Unit& unit,
                                               const AiProfile& profile,
                                               const AiSquadReservations& reservations = {});
AiCandidate chooseBestAiCandidate(std::vector<AiCandidate> candidates);

} // namespace jf
