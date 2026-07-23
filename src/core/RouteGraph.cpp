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

// docs/implementation_roadmap.md M6-B/C: docs/campaign_route_graph.md's
// Cinderwatch graph is `S1 外門 -> S2 監視所 -> C1 -> J1{S3 物資庫, S4 旧兵舎} ->
// J2 -> C2 -> S5 信号塔下層 -> S6 最後の信号 -> 出口`. J1/J2 is a single
// BranchGroup node here (`cinderwatch_stores_barracks`, AllMembers): a
// member Site's own outgoing edge always points straight back to the
// BranchGroup (see below), so advanceRouteToNextSite() naturally revisits
// it after each member resolves and only continues past once both are
// done - no separate "J2" node needed. Site 5 (信号塔下層) is real as of
// M6-C item2; site 6 (最後の信号) is `last_signal`, still a pre-spec
// placeholder standing in for the boss fight until the next Slice.
const RegionRouteGraph& cinderwatchGraph() {
    static const RegionRouteGraph graph{
        RegionId::CinderwatchGate,
        "cinderwatch_main_route",
        "cinderwatch_entrance",
        "cinderwatch_exit",
        {
            {"cinderwatch_entrance", RouteNodeKind::Entrance, std::nullopt, {}, BranchCompletion::AllMembers},
            {"cinderwatch_outer_gate", RouteNodeKind::Site, "cinderwatch_outer_gate", {}, BranchCompletion::AllMembers},
            {"ashroad_watch", RouteNodeKind::Site, "ashroad_watch", {}, BranchCompletion::AllMembers},
            {"cinderwatch_camp1", RouteNodeKind::Camp, std::nullopt, {}, BranchCompletion::AllMembers},
            {"cinderwatch_stores_barracks", RouteNodeKind::BranchGroup, std::nullopt,
             {"ironwatch_stores", "old_barracks"}, BranchCompletion::AllMembers},
            {"ironwatch_stores", RouteNodeKind::Site, "ironwatch_stores", {}, BranchCompletion::AllMembers},
            {"old_barracks", RouteNodeKind::Site, "old_barracks", {}, BranchCompletion::AllMembers},
            {"cinderwatch_camp2", RouteNodeKind::Camp, std::nullopt, {}, BranchCompletion::AllMembers},
            {"signal_tower", RouteNodeKind::Site, "signal_tower", {}, BranchCompletion::AllMembers},
            {"last_signal", RouteNodeKind::Site, "last_signal", {}, BranchCompletion::AllMembers},
            {"cinderwatch_exit", RouteNodeKind::Exit, std::nullopt, {}, BranchCompletion::AllMembers},
        },
        {
            {"cinderwatch_entrance", "cinderwatch_outer_gate"},
            {"cinderwatch_outer_gate", "ashroad_watch"},
            {"ashroad_watch", "cinderwatch_camp1"},
            {"cinderwatch_camp1", "cinderwatch_stores_barracks"},
            {"ironwatch_stores", "cinderwatch_stores_barracks"},
            {"old_barracks", "cinderwatch_stores_barracks"},
            {"cinderwatch_stores_barracks", "cinderwatch_camp2"},
            {"cinderwatch_camp2", "signal_tower"},
            {"signal_tower", "last_signal"},
            {"last_signal", "cinderwatch_exit"},
        },
    };
    return graph;
}

// docs/regions/ashiron_quarry.md「地点構成」: entrance -> 1.崩落した搬入口 ->
// 2.砕石段丘 -> キャンプI -> (3A.旧採掘坑 | 3B.巻上機区画、どちらか1つで進行可) ->
// 4.灰鉄鉱脈 -> キャンプII -> 5.崩落核(ボス) -> exit. Unlike Cinderwatch's only
// BranchGroup (AllMembers - clear both), this one is `AnyMember` - clearing
// either 3A or 3B lets the expedition continue (clearing both is a
// carry-over bonus per the doc, not required). M9-A implements only site1
// (`quarry_entrance`) as real content; sites 2/3A/3B/4/5 stay pre-spec
// placeholders (same role Cinderwatch's later sites played before M6-B/C
// fleshed them out) until later Slices replace them one at a time.
const RegionRouteGraph& ashironQuarryGraph() {
    static const RegionRouteGraph graph{
        RegionId::AshironQuarry,
        "ashiron_quarry_main_route",
        "ashiron_quarry_entrance",
        "ashiron_quarry_exit",
        {
            {"ashiron_quarry_entrance", RouteNodeKind::Entrance, std::nullopt},
            {"quarry_entrance", RouteNodeKind::Site, "quarry_entrance"},
            {"quarry_terrace", RouteNodeKind::Site, "quarry_terrace"},
            {"quarry_camp1", RouteNodeKind::Camp, std::nullopt},
            {"quarry_mine_hoist_branch", RouteNodeKind::BranchGroup, std::nullopt,
             {"quarry_old_mine", "quarry_hoist_works"}, BranchCompletion::AnyMember},
            {"quarry_old_mine", RouteNodeKind::Site, "quarry_old_mine"},
            {"quarry_hoist_works", RouteNodeKind::Site, "quarry_hoist_works"},
            {"ashiron_vein", RouteNodeKind::Site, "ashiron_vein"},
            {"quarry_camp2", RouteNodeKind::Camp, std::nullopt},
            {"quarry_collapse_core", RouteNodeKind::Site, "quarry_collapse_core"},
            {"ashiron_quarry_exit", RouteNodeKind::Exit, std::nullopt},
        },
        {
            {"ashiron_quarry_entrance", "quarry_entrance"},
            {"quarry_entrance", "quarry_terrace"},
            {"quarry_terrace", "quarry_camp1"},
            {"quarry_camp1", "quarry_mine_hoist_branch"},
            {"quarry_old_mine", "quarry_mine_hoist_branch"},
            {"quarry_hoist_works", "quarry_mine_hoist_branch"},
            {"quarry_mine_hoist_branch", "ashiron_vein"},
            {"ashiron_vein", "quarry_camp2"},
            {"quarry_camp2", "quarry_collapse_core"},
            {"quarry_collapse_core", "ashiron_quarry_exit"},
        },
    };
    return graph;
}

} // namespace

bool usesRouteGraph(RegionId regionId) {
    return regionId == RegionId::AshboughForest || regionId == RegionId::CinderwatchGate ||
           regionId == RegionId::AshironQuarry;
}

const RegionRouteGraph& regionRouteGraph(RegionId regionId) {
    if (regionId == RegionId::AshboughForest) return ashboughGraph();
    if (regionId == RegionId::CinderwatchGate) return cinderwatchGraph();
    if (regionId == RegionId::AshironQuarry) return ashironQuarryGraph();
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
        if (node.kind == RouteNodeKind::BranchGroup) {
            if (node.branchMembers.empty()) return fail("branch group has no members");
            for (const std::string& memberId : node.branchMembers) {
                const RouteNodeDefinition* member = findRouteNode(graph, memberId);
                if (!member || member->kind != RouteNodeKind::Site)
                    return fail("branch group references an unknown or non-Site member");
            }
        }
    }
    for (const RouteEdgeDefinition& edge : graph.edges)
        if (!findRouteNode(graph, edge.from) || !findRouteNode(graph, edge.to))
            return fail("route edge references an unknown node");
    std::unordered_set<std::string> reachable;
    std::vector<std::string> stack{graph.entranceNodeId};
    while (!stack.empty()) {
        std::string current = stack.back();
        stack.pop_back();
        if (!reachable.insert(current).second) continue;
        if (const RouteNodeDefinition* node = findRouteNode(graph, current);
            node && node->kind == RouteNodeKind::BranchGroup) {
            for (const std::string& memberId : node->branchMembers) stack.push_back(memberId);
        }
        for (const RouteEdgeDefinition& edge : graph.edges)
            if (edge.from == current) stack.push_back(edge.to);
    }
    if (!reachable.count(graph.exitNodeId)) return fail("route exit is not reachable from entrance");
    for (const RouteNodeDefinition& node : graph.nodes)
        if (node.kind == RouteNodeKind::Site && !reachable.count(node.id))
            return fail("site node is not reachable from entrance");
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
