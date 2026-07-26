#include "jf/battle/AiSystem.hpp"

#include <algorithm>
#include <limits>

#include "jf/battle/CombatResolver.hpp"
#include "jf/battle/Movement.hpp"

namespace jf {
namespace {

int key(GridPos pos) { return pos.row * kGridCols + pos.col; }

bool candidateLess(const AiCandidate& a, const AiCandidate& b) {
    if (a.score != b.score) return a.score > b.score;
    if (a.missionProgress != b.missionProgress) return a.missionProgress > b.missionProgress;
    if (a.defeatsTarget != b.defeatsTarget) return a.defeatsTarget;
    if (a.predictedIncomingDamage != b.predictedIncomingDamage)
        return a.predictedIncomingDamage < b.predictedIncomingDamage;
    if (a.movementCost != b.movementCost) return a.movementCost < b.movementCost;
    if (a.targetUnitId != b.targetUnitId) return a.targetUnitId < b.targetUnitId;
    if (a.destination.row != b.destination.row) return a.destination.row < b.destination.row;
    if (a.destination.col != b.destination.col) return a.destination.col < b.destination.col;
    return a.actionId < b.actionId;
}

int incomingThreat(const BattleState& battle, const Unit& unit, GridPos destination) {
    int threat = 0;
    Unit probe = unit;
    probe.position = destination;
    for (const Unit& player : battle.units()) {
        if (player.team != Team::Player || !player.isAlive()) continue;
        const int distance = manhattanDistance(player.position, destination);
        if (distance < player.minimumAttackRange() || distance > player.weapon.maxRange + player.effectiveMove()) continue;
        threat += previewAttack(player, probe, battle.combatDefenseBonus(probe, player), 100).damage;
    }
    return threat;
}

} // namespace

void AiSquadReservations::clear() {
    destinationKeys.clear();
    reservedDamage.clear();
    supportTargets.clear();
}

void AiSquadReservations::reserve(const AiCandidate& candidate) {
    destinationKeys.insert(key(candidate.destination));
    if (!candidate.targetUnitId.empty() && candidate.predictedDamage > 0)
        reservedDamage[candidate.targetUnitId] += candidate.predictedDamage;
    if (candidate.type == AiActionType::Support && !candidate.targetUnitId.empty())
        supportTargets.insert(candidate.targetUnitId);
}

AiProfile profileFor(const Unit& unit) {
    if (unit.unitClass == UnitClass::Wolf)
        // docs/enemy_ai_rules.md "撤退と降伏": Wildlifeの撤退HP閾値は20(他は
        // 既定の25)。
        return {AiProfileId::Wolf, 40, 1500, 35, 24, 8, 30, 1, kGridCols, 20};
    if (unit.unitClass == UnitClass::VeteranGuard)
        return {AiProfileId::Defender, 36, 1500, 8, 16, 22, 35, 1, 3};
    if (unit.unitClass == UnitClass::WatchArcher || unit.weapon.minRange > 1)
        return {AiProfileId::Ranged, 40, 1500, 12, 16, 30, 15, std::max(unit.minimumAttackRange(), 2), 5};
    if (unit.unitClass == UnitClass::DawnChirurgeon)
        return {AiProfileId::Support, 25, 1400, 8, 12, 35, 35, 2, 4};
    // enemy_ai_rules.md "Faction差分" 野盗: 「低HP、離脱経路を高評価」。Loot
    // Containerと離脱経路の評価はObject/Exit認識が無いAI候補生成にはまだ
    // 組み込めないため保留(継続課題) - lowHpWeightを他Profileより高く
    // (finishing低HP対象を優先)、pursuitLimitを短く(深追いしない、略奪目的で
    // 長期戦を避ける)することだけ、今のAiCandidate生成で表現できる範囲として
    // 反映した。
    if (unit.unitClass == UnitClass::Bandit)
        return {AiProfileId::Bandit, 40, 1500, 50, 20, 15, 10, 1, 4};
    // docs/regions/old_frontier_settlement.md "強敵 襲撃団頭領"'s "退路判断":
    // HP30%以下かつ4ラウンド目以降なら撤退 - this engine's generic
    // takeEnemyTurn()/generateAiCandidates() path has no round-aware hook on
    // AiProfile (retreatHpPercent is the only lever), so the "4ラウンド目
    // 以降" gate is approximated away: this unit considers retreating at
    // HP<=30% from round 1 onward, not just round 4+. Explicitly NOT a
    // bespoke takeXBossTurn() (UnitClass::RaidLeader's own comment) - the
    // doc frames this as an optional elite, not a true boss, so tuning the
    // existing generic profile is the intended level of investment.
    if (unit.unitClass == UnitClass::RaidLeader)
        return {AiProfileId::Human, 40, 1500, 12, 20, 20, 20, 1, 6, 30};
    // docs/regions/buried_dawn_sanctum.md "最終強敵 聖堂回収団長"'s 降伏条件:
    // "HP25%以下、記録箱2個保全、退路ありなら降伏する" - same simplification as
    // RaidLeader above (no round/Object-aware hook exists on AiProfile), only
    // the HP threshold is modeled. Explicitly not a bespoke takeXBossTurn()
    // for the same "optional elite, not a true boss" reasoning.
    if (unit.unitClass == UnitClass::SanctumRetrievalLeader)
        return {AiProfileId::Human, 40, 1500, 12, 20, 20, 20, 1, 6, 25};
    // docs/regions/shattered_march_fort.md「最終強敵 残留砦隊長」's 降伏条件:
    // "HP25%以下かつ命令箱2個保全なら撤退" - same simplification as RaidLeader/
    // SanctumRetrievalLeader above (no round/Object-aware hook exists on
    // AiProfile), only the HP threshold is modeled. Explicitly not a bespoke
    // takeXBossTurn() for the same "optional elite, not a true boss"
    // reasoning; the doc's other 3 bespoke abilities (交互防衛/壁際指揮/
    // 記録封鎖) are deferred entirely (see this class's own comment in
    // UnitClass.hpp).
    if (unit.unitClass == UnitClass::FortGarrisonCaptain)
        return {AiProfileId::Human, 40, 1500, 12, 20, 20, 20, 1, 6, 25};
    return {AiProfileId::Human, 40, 1500, 12, 20, 20, 20, 1, 6};
}

std::vector<AiCandidate> generateAiCandidates(const BattleState& battle,
                                               const Unit& unit,
                                               const AiProfile& profile,
                                               const AiSquadReservations& reservations) {
    std::vector<AiCandidate> result;
    std::vector<GridPos> destinations = computeReachableTiles(battle, unit);
    if (std::find(destinations.begin(), destinations.end(), unit.position) == destinations.end())
        destinations.push_back(unit.position);

    // docs/enemy_ai_rules.md "撤退と降伏": a unit at or below its profile's
    // retreat HP% generates a Retreat candidate for every reachable Exit
    // tile (the board's far edge column, matching its own spawn side).
    // Scored to beat a losing/non-lethal Attack or Move but never a lethal
    // Attack (defeatBonus alone outweighs it) - so retreat only actually
    // wins the Score comparison when nothing better is available, without
    // needing a separate "no lethal action available" precondition.
    const bool considersRetreat = unit.currentHp * 100 <= unit.stats.maxHp * profile.retreatHpPercent;
    if (considersRetreat) {
        for (GridPos destination : destinations) {
            if (destination.col != kGridCols - 1) continue;
            const int moved = manhattanDistance(unit.position, destination);
            AiCandidate retreat;
            retreat.type = AiActionType::Retreat;
            retreat.destination = destination;
            retreat.actionId = "retreat";
            retreat.movementCost = moved;
            const int missingHpPercent = 100 - (unit.currentHp * 100 / unit.stats.maxHp);
            retreat.score = 500 + missingHpPercent * 8 - moved * 10;
            result.push_back(std::move(retreat));
        }
    }

    for (GridPos destination : destinations) {
        const int moved = manhattanDistance(unit.position, destination);
        if (moved > profile.pursuitLimit) continue;
        const int danger = incomingThreat(battle, unit, destination);
        bool madeAttack = false;
        if (profile.id == AiProfileId::Support) {
            for (const Unit& ally : battle.units()) {
                if (ally.team != unit.team || !ally.isAlive() || ally.currentHp >= ally.stats.maxHp) continue;
                if (manhattanDistance(destination, ally.position) > 1) continue;
                AiCandidate support;
                support.type = AiActionType::Support;
                support.destination = destination;
                support.targetUnitId = ally.id;
                support.actionId = "field_heal";
                support.predictedIncomingDamage = danger;
                support.movementCost = moved;
                support.score = 600 + (ally.stats.maxHp - ally.currentHp) * 40
                                - danger * profile.dangerWeight / 10 - moved * 30;
                if (reservations.supportTargets.contains(ally.id)) support.score -= 700;
                result.push_back(std::move(support));
            }
        }
        for (const Unit& target : battle.units()) {
            if (target.team != Team::Player || !target.isAlive()) continue;
            const int distance = manhattanDistance(destination, target.position);
            if (distance < unit.minimumAttackRange() || distance > unit.weapon.maxRange) continue;
            Unit attacker = unit;
            attacker.position = destination;
            const int damage = previewAttack(attacker, target, battle.combatDefenseBonus(target, attacker), 100).damage;
            const int reserved = reservations.reservedDamage.contains(target.id)
                                     ? reservations.reservedDamage.at(target.id) : 0;
            AiCandidate candidate;
            candidate.type = AiActionType::Attack;
            candidate.destination = destination;
            candidate.targetUnitId = target.id;
            candidate.actionId = "weapon_attack";
            candidate.predictedDamage = damage;
            candidate.predictedIncomingDamage = danger;
            candidate.movementCost = moved;
            candidate.defeatsTarget = damage >= target.currentHp;
            candidate.score = damage * profile.damageWeight + (candidate.defeatsTarget ? profile.defeatBonus : 0)
                              + (target.stats.maxHp - target.currentHp) * profile.lowHpWeight
                              - danger * profile.dangerWeight / 10 - moved * 30;
            const int currentDistance = manhattanDistance(unit.position, target.position);
            const bool alreadyInRange = currentDistance >= unit.minimumAttackRange() &&
                                        currentDistance <= unit.weapon.maxRange;
            if (alreadyInRange && destination != unit.position) candidate.score -= 500;
            if (reserved >= target.currentHp) candidate.score -= 900;
            if (profile.id == AiProfileId::Wolf) {
                int adjacentAllies = 0;
                for (const Unit& ally : battle.units())
                    if (&ally != &unit && ally.team == unit.team && ally.isAlive() &&
                        manhattanDistance(ally.position, target.position) <= 2) ++adjacentAllies;
                candidate.score += adjacentAllies * profile.cohesionWeight;
            }
            if (profile.id == AiProfileId::Ranged)
                candidate.score -= std::abs(distance - profile.preferredRange) * 120;
            result.push_back(std::move(candidate));
            madeAttack = true;
        }
        if (!madeAttack && destination != unit.position) {
            int nearest = std::numeric_limits<int>::max();
            for (const Unit& target : battle.units())
                if (target.team == Team::Player && target.isAlive())
                    nearest = std::min(nearest, manhattanDistance(destination, target.position));
            // No living Team::Player unit exists (e.g. the whole party was
            // just wiped) - `nearest` stays at INT_MAX, and `-nearest *
            // profile.distanceWeight` below would be a signed-integer-
            // overflow UB (confirmed via UBSan: this silently corrupted
            // unrelated state under some builds, producing sporadic,
            // seemingly unrelated test flakes). There's nothing to move
            // toward in this case, so skip generating this candidate - the
            // `result.empty()` Wait fallback below still applies.
            if (nearest == std::numeric_limits<int>::max()) continue;
            AiCandidate move;
            move.type = AiActionType::Move;
            move.destination = destination;
            move.actionId = "move";
            move.predictedIncomingDamage = danger;
            move.movementCost = moved;
            move.score = -nearest * profile.distanceWeight - danger * profile.dangerWeight / 10;
            if (profile.id == AiProfileId::Ranged && nearest < unit.minimumAttackRange()) move.score -= 600;
            if (reservations.destinationKeys.contains(key(destination))) move.score -= 500;
            result.push_back(std::move(move));
        }
    }
    if (result.empty()) result.push_back({AiActionType::Wait, unit.position, {}, "wait"});
    return result;
}

AiCandidate chooseBestAiCandidate(std::vector<AiCandidate> candidates) {
    if (candidates.empty()) return {};
    std::stable_sort(candidates.begin(), candidates.end(), candidateLess);
    return candidates.front();
}

} // namespace jf
