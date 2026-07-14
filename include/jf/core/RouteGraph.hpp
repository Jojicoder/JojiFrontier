#pragma once

#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "jf/core/BaseState.hpp"

namespace jf {

enum class RouteNodeKind { Entrance, Site, Camp, Exit };

struct RouteNodeDefinition {
    std::string id;
    RouteNodeKind kind = RouteNodeKind::Site;
    std::optional<std::string> stageId;
};

struct RouteEdgeDefinition {
    std::string from;
    std::string to;
};

struct RegionRouteGraph {
    RegionId regionId = RegionId::AshboughForest;
    std::string routeId;
    std::string entranceNodeId;
    std::string exitNodeId;
    std::vector<RouteNodeDefinition> nodes;
    std::vector<RouteEdgeDefinition> edges;
};

struct RouteProgressSnapshot {
    std::string routeId;
    std::string currentNodeId;
    std::string lastCheckpointNodeId;
    std::unordered_set<std::string> resolvedNodeIds;
    std::unordered_set<std::string> safelyPassedNodeIds;
    std::vector<std::string> traversalHistory;
};

bool usesRouteGraph(RegionId regionId);
const RegionRouteGraph& regionRouteGraph(RegionId regionId);
bool validateRouteGraph(const RegionRouteGraph& graph, std::string* error = nullptr);
const RouteNodeDefinition* findRouteNode(const RegionRouteGraph& graph, const std::string& nodeId);
const RouteNodeDefinition* nextRouteNode(const RegionRouteGraph& graph, const std::string& nodeId);
RouteProgressSnapshot initialRouteProgress(RegionId regionId);

} // namespace jf
