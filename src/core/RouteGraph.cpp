#include "jf/core/RouteGraph.hpp"

#include <stdexcept>

namespace jf {
namespace {

const RegionRouteGraph& ashboughGraph() {
    static const RegionRouteGraph graph{
        RegionId::AshboughForest,
        "ashbough_main_route",
        "ashbough_entrance",
        "ashbough_exit",
        {
            {"ashbough_entrance", RouteNodeKind::Entrance, std::nullopt},
            {"ashbough_verge", RouteNodeKind::Site, "ashbough_verge"},
            {"herbwater_hollow", RouteNodeKind::Site, "herbwater_hollow"},
            {"ashbough_camp", RouteNodeKind::Camp, std::nullopt},
            {"brokenwood_territory", RouteNodeKind::Site, "brokenwood_territory"},
            {"ashbough_exit", RouteNodeKind::Exit, std::nullopt},
        },
        {
            {"ashbough_entrance", "ashbough_verge"},
            {"ashbough_verge", "herbwater_hollow"},
            {"herbwater_hollow", "ashbough_camp"},
            {"ashbough_camp", "brokenwood_territory"},
            {"brokenwood_territory", "ashbough_exit"},
        },
    };
    return graph;
}

} // namespace

bool usesRouteGraph(RegionId regionId) {
    return regionId == RegionId::AshboughForest;
}

const RegionRouteGraph& regionRouteGraph(RegionId regionId) {
    if (regionId == RegionId::AshboughForest) return ashboughGraph();
    throw std::invalid_argument("region has no route graph");
}

const RouteNodeDefinition* findRouteNode(const RegionRouteGraph& graph, const std::string& nodeId) {
    for (const RouteNodeDefinition& node : graph.nodes)
        if (node.id == nodeId) return &node;
    return nullptr;
}

const RouteNodeDefinition* nextRouteNode(const RegionRouteGraph& graph, const std::string& nodeId) {
    for (const RouteEdgeDefinition& edge : graph.edges)
        if (edge.from == nodeId) return findRouteNode(graph, edge.to);
    return nullptr;
}

bool validateRouteGraph(const RegionRouteGraph& graph, std::string* error) {
    auto fail = [&](const std::string& message) {
        if (error) *error = message;
        return false;
    };
    if (graph.routeId.empty() || graph.nodes.empty()) return fail("route graph is empty");
    if (!findRouteNode(graph, graph.entranceNodeId) || !findRouteNode(graph, graph.exitNodeId))
        return fail("route endpoints are missing");
    std::unordered_set<std::string> ids;
    for (const RouteNodeDefinition& node : graph.nodes) {
        if (node.id.empty() || !ids.insert(node.id).second) return fail("duplicate route node");
        if (node.kind == RouteNodeKind::Site && (!node.stageId || node.stageId->empty()))
            return fail("site node has no stage id");
    }
    for (const RouteEdgeDefinition& edge : graph.edges)
        if (!findRouteNode(graph, edge.from) || !findRouteNode(graph, edge.to))
            return fail("route edge references an unknown node");
    return true;
}

RouteProgressSnapshot initialRouteProgress(RegionId regionId) {
    const RegionRouteGraph& graph = regionRouteGraph(regionId);
    const RouteNodeDefinition* first = nextRouteNode(graph, graph.entranceNodeId);
    if (!first || first->kind != RouteNodeKind::Site) throw std::logic_error("route has no first site");
    RouteProgressSnapshot result;
    result.routeId = graph.routeId;
    result.currentNodeId = first->id;
    result.lastCheckpointNodeId = first->id;
    result.traversalHistory = {graph.entranceNodeId, first->id};
    return result;
}

} // namespace jf
