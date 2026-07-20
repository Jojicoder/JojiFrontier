#include "jf/core/ExpeditionService.hpp"

#include <algorithm>

namespace jf {

StageDescriptor computeCurrentStage(const ExpeditionState& expedition, const GameData& data) {
    RegionDescriptor region = regionDescriptor(expedition.regionId, data);
    if (expedition.routeProgress) {
        const RegionRouteGraph& graph = regionRouteGraph(expedition.regionId);
        const RouteNodeDefinition* node = findRouteNode(graph, expedition.routeProgress->currentNodeId);
        if (!node || !node->stageId) throw std::logic_error("current route node is not a site");
        auto stage = std::find_if(region.stages.begin(), region.stages.end(),
                                  [&](const StageDescriptor& candidate) { return candidate.id == *node->stageId; });
        if (stage == region.stages.end()) throw std::logic_error("route stage is not registered");
        return *stage;
    }
    return region.stages.at(static_cast<std::size_t>(expedition.stageIndex));
}

bool computeExpeditionComplete(const ExpeditionState& expedition, const GameData& data) {
    if (!expedition.routeProgress)
        return expedition.battlesWon >= static_cast<int>(regionDescriptor(expedition.regionId, data).stages.size());
    const RouteProgressSnapshot& progress = *expedition.routeProgress;
    if (!progress.resolvedNodeIds.count(progress.currentNodeId)) return false;
    const RegionRouteGraph& graph = regionRouteGraph(expedition.regionId);
    const RouteNodeDefinition* node = nextRouteNode(graph, progress.currentNodeId);
    while (node && node->kind != RouteNodeKind::Site) {
        if (node->kind == RouteNodeKind::Exit) return true;
        node = nextRouteNode(graph, node->id);
    }
    return node == nullptr;
}

std::optional<std::string> computeNextMissionNameJa(const ExpeditionState& expedition, const GameData& data) {
    if (!expedition.routeProgress) {
        RegionDescriptor region = regionDescriptor(expedition.regionId, data);
        std::size_t next = static_cast<std::size_t>(expedition.stageIndex + 1);
        if (next < region.stages.size()) return region.stages[next].missionNameJa;
        return std::nullopt;
    }
    const RegionRouteGraph& graph = regionRouteGraph(expedition.regionId);
    const RouteNodeDefinition* node = nextRouteNode(graph, expedition.routeProgress->currentNodeId);
    while (node && node->kind != RouteNodeKind::Site) {
        if (node->kind == RouteNodeKind::Exit) return std::nullopt;
        node = nextRouteNode(graph, node->id);
    }
    if (!node || !node->stageId) return std::nullopt;
    for (const StageDescriptor& stage : regionDescriptor(expedition.regionId, data).stages)
        if (stage.id == *node->stageId) return stage.missionNameJa;
    return std::nullopt;
}

std::optional<std::vector<std::string>> computeNextSiteEnemyRosterNames(const ExpeditionState& expedition,
                                                                         const GameData& data,
                                                                         const std::vector<Unit>& partyUnits) {
    std::optional<std::string> nextStageId;
    RegionDescriptor region = regionDescriptor(expedition.regionId, data);
    if (!expedition.routeProgress) {
        std::size_t next = static_cast<std::size_t>(expedition.stageIndex + 1);
        if (next >= region.stages.size()) return std::nullopt;
        nextStageId = region.stages[next].id;
    } else {
        const RegionRouteGraph& graph = regionRouteGraph(expedition.regionId);
        const RouteNodeDefinition* node = nextRouteNode(graph, expedition.routeProgress->currentNodeId);
        while (node && node->kind != RouteNodeKind::Site) {
            if (node->kind == RouteNodeKind::Exit) return std::nullopt;
            node = nextRouteNode(graph, node->id);
        }
        if (!node || !node->stageId) return std::nullopt;
        nextStageId = *node->stageId;
    }
    for (const StageDescriptor& stage : region.stages) {
        if (stage.id != *nextStageId) continue;
        std::vector<std::string> names;
        for (const UnitTemplate& enemy : stage.enemyRoster) names.push_back(enemy.name);
        // docs/regions/ashbough_forest.md "折れ木の縄張り": show the
        // understaffed reinforcement too when it will actually spawn, so
        // this preview doesn't undercount what the battle will contain.
        if (stage.understaffedReinforcement) {
            int livingPlayerCount = 0;
            for (const Unit& unit : partyUnits) livingPlayerCount += unit.isAlive();
            if (livingPlayerCount < stage.understaffedThreshold)
                names.push_back(stage.understaffedReinforcement->name);
        }
        return names;
    }
    return std::nullopt;
}

const RouteNodeDefinition* findNextUnresolvedBranchMember(const RegionRouteGraph& graph,
                                                           const RouteNodeDefinition& branch,
                                                           const RouteProgressSnapshot& progress,
                                                           RegionId regionId, const BaseState& baseState) {
    for (const std::string& memberId : branch.branchMembers) {
        if (progress.resolvedNodeIds.count(memberId)) continue;
        const RouteNodeDefinition* member = findRouteNode(graph, memberId);
        if (member && member->stageId) {
            auto access = baseState.siteAccess.find(siteAccessKey(regionId, *member->stageId));
            if (access != baseState.siteAccess.end() && access->second >= SiteAccessState::Secured) continue;
        }
        return member;
    }
    return nullptr;
}

bool advanceExpeditionRouteToNextSite(ExpeditionState& expedition, const BaseState& baseState, const GameData& data) {
    if (!expedition.routeProgress) return false;
    RouteProgressSnapshot& progress = *expedition.routeProgress;
    const RegionRouteGraph& graph = regionRouteGraph(expedition.regionId);
    const RouteNodeDefinition* node = nextRouteNode(graph, progress.currentNodeId);
    while (node) {
        if (node->kind == RouteNodeKind::BranchGroup) {
            // docs/route_graph_data.md「分岐と合流」: enter the first
            // unresolved member (this expedition, or not yet permanently
            // Secured); once every member qualifies, fall through to this
            // BranchGroup's own single outgoing edge instead.
            if (const RouteNodeDefinition* member =
                    findNextUnresolvedBranchMember(graph, *node, progress, expedition.regionId, baseState)) {
                node = member;
                continue;
            }
            node = nextRouteNode(graph, node->id);
            continue;
        }
        progress.traversalHistory.push_back(node->id);
        if (node->kind == RouteNodeKind::Exit) {
            progress.currentNodeId = node->id;
            return false;
        }
        if (node->kind == RouteNodeKind::Camp) {
            progress.lastCheckpointNodeId = node->id;
        } else if (node->kind == RouteNodeKind::Site) {
            progress.currentNodeId = node->id;
            progress.lastCheckpointNodeId = node->id;
            RegionDescriptor region = regionDescriptor(expedition.regionId, data);
            auto stage = std::find_if(region.stages.begin(), region.stages.end(),
                                      [&](const StageDescriptor& candidate) {
                                          return node->stageId && candidate.id == *node->stageId;
                                      });
            if (stage == region.stages.end()) return false;
            expedition.stageIndex = static_cast<int>(std::distance(region.stages.begin(), stage));
            return true;
        }
        node = nextRouteNode(graph, node->id);
    }
    return false;
}

std::vector<RegionSummary> computeRegionSummaries(const GameData& data, const BaseState& baseState) {
    std::vector<RegionSummary> summaries;
    for (RegionId id : {RegionId::AshboughForest, RegionId::CinderwatchGate}) {
        RegionDescriptor region = regionDescriptor(id, data);
        summaries.push_back({id, region.displayNameEn, region.displayNameJa, regionUnlocked(id, baseState, data)});
    }
    return summaries;
}

SiteAccessState computeCurrentSiteAccess(const ExpeditionState& expedition, const BaseState& baseState,
                                         const GameData& data) {
    auto it = baseState.siteAccess.find(siteAccessKey(expedition.regionId, computeCurrentStage(expedition, data).id));
    return it == baseState.siteAccess.end() ? SiteAccessState::Unknown : it->second;
}

void queueExpeditionSiteAccessPromotion(ExpeditionState& expedition, const std::string& key,
                                        SiteAccessState achieved, const BaseState& baseState) {
    auto persistedIt = baseState.siteAccess.find(key);
    SiteAccessState persisted = persistedIt == baseState.siteAccess.end() ? SiteAccessState::Unknown : persistedIt->second;
    if (achieved <= persisted) return;
    for (auto& [pendingKey, pendingState] : expedition.pendingSiteAccessUpdates) {
        if (pendingKey == key) {
            if (achieved > pendingState) pendingState = achieved;
            return;
        }
    }
    expedition.pendingSiteAccessUpdates.push_back({key, achieved});
}

bool computeWouldRegionBeCleared(RegionId regionId, const ExpeditionState& expedition, const BaseState& baseState,
                                 const GameData& data) {
    RegionDescriptor region = regionDescriptor(regionId, data);
    for (const StageDescriptor& stage : region.stages) {
        const std::string key = siteAccessKey(regionId, stage.id);
        SiteAccessState state = SiteAccessState::Unknown;
        auto persistedIt = baseState.siteAccess.find(key);
        if (persistedIt != baseState.siteAccess.end()) state = persistedIt->second;
        for (const auto& [pendingKey, pendingState] : expedition.pendingSiteAccessUpdates) {
            if (pendingKey == key && pendingState > state) state = pendingState;
        }
        if (state < SiteAccessState::Surveyed) return false;
    }
    return true;
}

} // namespace jf
