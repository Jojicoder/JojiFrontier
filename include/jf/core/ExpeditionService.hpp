#pragma once

#include <optional>
#include <string>
#include <vector>

#include "jf/core/BaseState.hpp"
#include "jf/core/ExpeditionState.hpp"
#include "jf/core/RouteGraph.hpp"
#include "jf/core/Unit.hpp"
#include "jf/data/GameData.hpp"

namespace jf {

// docs/implementation_roadmap.md M6「コンポーネント分離メモ」: the pure
// state-query/transition slice of GameApp's Route/expedition-progression
// logic, extracted as free functions (matching this codebase's existing
// style - Region.hpp's computeStageVictoryLoot()/RouteGraph.hpp's
// findRouteNode(), not a stateful "Service" class). Every function here
// reads/writes only ExpeditionState/BaseState/GameData - none of them touch
// BattleController or Screen, so GameApp's screen-transition methods
// (chooseExplorationRoute(), continueExpedition(), etc.) stay put and just
// call into these.
//
// Naming: prefixed with compute*/find*/advance*/queue* rather than reusing
// GameApp's original method names, because GameApp.cpp lives in the same
// `jf` namespace - an unqualified call to an identically-named free
// function from inside the matching member function would resolve to
// itself (class-scope lookup wins) and recurse silently.

StageDescriptor computeCurrentStage(const ExpeditionState& expedition, const GameData& data);

bool computeExpeditionComplete(const ExpeditionState& expedition, const GameData& data);

std::optional<std::string> computeNextMissionNameJa(const ExpeditionState& expedition, const GameData& data);

std::optional<std::vector<std::string>> computeNextSiteEnemyRosterNames(const ExpeditionState& expedition,
                                                                         const GameData& data,
                                                                         const std::vector<Unit>& partyUnits);

// docs/route_graph_data.md「分岐と合流」: the first BranchGroup member not yet
// resolved this expedition and not already permanently Secured from a prior
// one - nullptr once every member qualifies, meaning traversal may continue
// past the branch.
const RouteNodeDefinition* findNextUnresolvedBranchMember(const RegionRouteGraph& graph,
                                                           const RouteNodeDefinition& branch,
                                                           const RouteProgressSnapshot& progress,
                                                           RegionId regionId, const BaseState& baseState);

// Mutates `expedition.routeProgress` in place, advancing to the next Site
// node (skipping Camp/BranchGroup nodes along the way). Returns true iff it
// landed on a real Site (expedition.stageIndex updated to match); false at
// the region's Exit or on any other early-out.
bool advanceExpeditionRouteToNextSite(ExpeditionState& expedition, const BaseState& baseState, const GameData& data);

struct RegionSummary {
    RegionId id;
    std::string displayNameEn;
    std::string displayNameJa;
    bool unlocked;
};
std::vector<RegionSummary> computeRegionSummaries(const GameData& data, const BaseState& baseState);

SiteAccessState computeCurrentSiteAccess(const ExpeditionState& expedition, const BaseState& baseState,
                                         const GameData& data);

// Queues a promotion (never a demotion) of the given site's access tier,
// to be committed on a safe return. Deduplicates against both the
// currently-persisted tier and any already-pending update for the same
// site this run.
void queueExpeditionSiteAccessPromotion(ExpeditionState& expedition, const std::string& key,
                                        SiteAccessState achieved, const BaseState& baseState);

// docs/region_mission_data_contract.md「地域完了判定」: true once every stage
// of `regionId` is at least Surveyed, counting both baseState.siteAccess
// (persisted from earlier runs) and this run's still-pending promotions.
bool computeWouldRegionBeCleared(RegionId regionId, const ExpeditionState& expedition, const BaseState& baseState,
                                 const GameData& data);

} // namespace jf
