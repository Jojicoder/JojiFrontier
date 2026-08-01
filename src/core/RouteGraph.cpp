#include "jf/core/RouteGraph.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "jf/core/Region.hpp" // regionIdFromStringStrict()/toString()

namespace jf {
namespace {

// データ/ロジック分離方針(docs/implementation_status.md「データ/ロジック
// 分離方針」): 全10地域分のグラフ本体(node/edge)はdata/route_graphs.jsonへ
// 切り出し済み。ここには読み込みロジックと、グラフに対する汎用アルゴリズム
// (findRouteNode()/nextRouteNode()/validateRouteGraph()/initialRouteProgress())
// だけを残す。RegionId文字列⇔enum変換は既存のjf::regionIdFromStringStrict()
// (jf/core/Region.hpp、同じcore層内)をそのまま再利用する - Facilities.hpp/
// Skill.cpp/Armor.hppのように毎回ローカルテーブルを複製しない、というのが
// 唯一の違い(既に同じ用途の共有関数が同じ層に存在するケースなので)。
RouteNodeKind routeNodeKindFromJsonString(const std::string& name) {
    if (name == "Entrance") return RouteNodeKind::Entrance;
    if (name == "BranchGroup") return RouteNodeKind::BranchGroup;
    if (name == "Camp") return RouteNodeKind::Camp;
    if (name == "Exit") return RouteNodeKind::Exit;
    return RouteNodeKind::Site;
}

BranchCompletion branchCompletionFromJsonString(const std::string& name) {
    return name == "AnyMember" ? BranchCompletion::AnyMember : BranchCompletion::AllMembers;
}

const std::unordered_map<RegionId, RegionRouteGraph>& routeGraphsByRegion() {
    static const std::unordered_map<RegionId, RegionRouteGraph> graphs = [] {
        std::unordered_map<RegionId, RegionRouteGraph> t;
        // cwd is always the repo root at runtime - same convention as every
        // other data/*.json load.
        std::ifstream file("data/route_graphs.json");
        if (!file.is_open()) {
            std::cerr << "Failed to open data file: data/route_graphs.json" << std::endl;
            return t;
        }
        nlohmann::json parsed;
        try {
            file >> parsed;
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse JSON file data/route_graphs.json: " << e.what() << std::endl;
            return t;
        }
        for (const auto& g : parsed.at("routeGraphs")) {
            auto regionId = regionIdFromStringStrict(g.at("regionId").get<std::string>());
            if (!regionId) continue;
            RegionRouteGraph graph;
            graph.regionId = *regionId;
            graph.routeId = g.at("routeId").get<std::string>();
            graph.entranceNodeId = g.at("entranceNodeId").get<std::string>();
            graph.exitNodeId = g.at("exitNodeId").get<std::string>();
            for (const auto& n : g.at("nodes")) {
                RouteNodeDefinition node;
                node.id = n.at("id").get<std::string>();
                node.kind = routeNodeKindFromJsonString(n.at("kind").get<std::string>());
                if (n.contains("stageId")) node.stageId = n.at("stageId").get<std::string>();
                if (n.contains("branchMembers"))
                    for (const auto& b : n.at("branchMembers")) node.branchMembers.push_back(b.get<std::string>());
                if (n.contains("branchCompletion"))
                    node.branchCompletion = branchCompletionFromJsonString(n.at("branchCompletion").get<std::string>());
                graph.nodes.push_back(std::move(node));
            }
            for (const auto& e : g.at("edges"))
                graph.edges.push_back({e.at("from").get<std::string>(), e.at("to").get<std::string>()});
            t[*regionId] = std::move(graph);
        }
        return t;
    }();
    return graphs;
}

} // namespace

bool usesRouteGraph(RegionId regionId) { return routeGraphsByRegion().count(regionId) > 0; }

const RegionRouteGraph& regionRouteGraph(RegionId regionId) {
    auto it = routeGraphsByRegion().find(regionId);
    if (it == routeGraphsByRegion().end()) throw std::invalid_argument("region has no route graph");
    return it->second;
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
