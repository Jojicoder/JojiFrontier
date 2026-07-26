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

// docs/regions/blackwater_lowlands.md「地点構成」: 7-site skeleton +
// 3 camps. Site 3 (herb_islet)/4 (resin_grove) are order-flexible but BOTH
// required (BranchCompletion::AllMembers, same shape as Cinderwatch's
// ironwatch_stores/old_barracks branch) before continuing. Only site 1
// (sunken_path) is real content this Slice; sites 2/3/4/5/6/7 stay pre-spec
// placeholders until later Slices replace them one at a time.
const RegionRouteGraph& blackwaterLowlandsGraph() {
    static const RegionRouteGraph graph{
        RegionId::BlackwaterLowlands,
        "blackwater_lowlands_main_route",
        "blackwater_lowlands_entrance",
        "blackwater_lowlands_exit",
        {
            {"blackwater_lowlands_entrance", RouteNodeKind::Entrance, std::nullopt},
            {"sunken_path", RouteNodeKind::Site, "sunken_path"},
            {"reedway_fork", RouteNodeKind::Site, "reedway_fork"},
            {"blackwater_camp1", RouteNodeKind::Camp, std::nullopt},
            {"herb_resin_branch", RouteNodeKind::BranchGroup, std::nullopt,
             {"herb_islet", "resin_grove"}, BranchCompletion::AllMembers},
            {"herb_islet", RouteNodeKind::Site, "herb_islet"},
            {"resin_grove", RouteNodeKind::Site, "resin_grove"},
            {"blackwater_camp2", RouteNodeKind::Camp, std::nullopt},
            {"blackwater_crossing", RouteNodeKind::Site, "blackwater_crossing"},
            {"sunken_sluice", RouteNodeKind::Site, "sunken_sluice"},
            {"blackwater_camp3", RouteNodeKind::Camp, std::nullopt},
            {"deep_mire", RouteNodeKind::Site, "deep_mire"},
            {"blackwater_lowlands_exit", RouteNodeKind::Exit, std::nullopt},
        },
        {
            {"blackwater_lowlands_entrance", "sunken_path"},
            {"sunken_path", "reedway_fork"},
            {"reedway_fork", "blackwater_camp1"},
            {"blackwater_camp1", "herb_resin_branch"},
            {"herb_islet", "herb_resin_branch"},
            {"resin_grove", "herb_resin_branch"},
            {"herb_resin_branch", "blackwater_camp2"},
            {"blackwater_camp2", "blackwater_crossing"},
            {"blackwater_crossing", "sunken_sluice"},
            {"sunken_sluice", "blackwater_camp3"},
            {"blackwater_camp3", "deep_mire"},
            {"deep_mire", "blackwater_lowlands_exit"},
        },
    };
    return graph;
}

// docs/regions/windscar_plateau.md「地点構成」: entrance -> 1.風下の登り口 ->
// 2.崩れた中継路 -> キャンプI -> (3.風見台 と 4.分断された輸送隊、どちらを
// 先に攻略してもよいが両方必須) -> キャンプII -> 5.断崖荷車道 ->
// 6.高原伝令所 -> exit. Site 3/4's branch is BranchCompletion::AllMembers
// (both required, order-free), the same shape as Cinderwatch's
// ironwatch_stores/old_barracks and Blackwater's herb_islet/resin_grove -
// not AnyMember (Ashiron's mine/hoist branch), matching the doc's "地点5へ
// 進むには地点3・4の両方を確保する" explicitly requiring both. Sites 1-5
// (`windscar_ascent`/`windscar_relay`/`windwatch_station`/`split_convoy`/
// `cliff_cart_road`) are real content as of M9-P; site 6 (`plateau_relay`) is
// real content as of this Slice too (`data/regions.json`), completing the
// region - this graph's own wiring is unchanged.
const RegionRouteGraph& windscarPlateauGraph() {
    static const RegionRouteGraph graph{
        RegionId::WindscarPlateau,
        "windscar_plateau_main_route",
        "windscar_plateau_entrance",
        "windscar_plateau_exit",
        {
            {"windscar_plateau_entrance", RouteNodeKind::Entrance, std::nullopt},
            {"windscar_ascent", RouteNodeKind::Site, "windscar_ascent"},
            {"windscar_relay", RouteNodeKind::Site, "windscar_relay"},
            {"windscar_camp1", RouteNodeKind::Camp, std::nullopt},
            {"windwatch_convoy_branch", RouteNodeKind::BranchGroup, std::nullopt,
             {"windwatch_station", "split_convoy"}, BranchCompletion::AllMembers},
            {"windwatch_station", RouteNodeKind::Site, "windwatch_station"},
            {"split_convoy", RouteNodeKind::Site, "split_convoy"},
            {"windscar_camp2", RouteNodeKind::Camp, std::nullopt},
            {"cliff_cart_road", RouteNodeKind::Site, "cliff_cart_road"},
            {"plateau_relay", RouteNodeKind::Site, "plateau_relay"},
            {"windscar_plateau_exit", RouteNodeKind::Exit, std::nullopt},
        },
        {
            {"windscar_plateau_entrance", "windscar_ascent"},
            {"windscar_ascent", "windscar_relay"},
            {"windscar_relay", "windscar_camp1"},
            {"windscar_camp1", "windwatch_convoy_branch"},
            {"windwatch_station", "windwatch_convoy_branch"},
            {"split_convoy", "windwatch_convoy_branch"},
            {"windwatch_convoy_branch", "windscar_camp2"},
            {"windscar_camp2", "cliff_cart_road"},
            {"cliff_cart_road", "plateau_relay"},
            {"plateau_relay", "windscar_plateau_exit"},
        },
    };
    return graph;
}

// docs/regions/old_frontier_settlement.md「地点構成」: entrance -> 1.風化した
// 外柵 -> 2.共同井戸 -> キャンプI -> (3.旧穀物庫 と 4.集会家屋、どちらを先に
// 攻略してもよいが両方必須) -> キャンプII -> 5.夜明けの共同防衛 -> exit.
// Site 3/4's branch is BranchCompletion::AllMembers, the same shape as
// Windscar's windwatch_station/split_convoy branch (both required,
// order-free) - matching the doc's "地点3・4はどちらを先に進めてもよい"/
// "最終防衛には地点3・4の両方が必要". Site 1 (`settlement_outer_fence`) is
// real content as of this Slice; sites 2-5 are placeholders (see
// oldFrontierSettlementRegion() in Region.cpp) - this graph's own wiring is
// unaffected by that distinction, same as every prior region's skeleton
// Slice.
const RegionRouteGraph& oldFrontierSettlementGraph() {
    static const RegionRouteGraph graph{
        RegionId::OldFrontierSettlement,
        "old_frontier_settlement_main_route",
        "old_frontier_settlement_entrance",
        "old_frontier_settlement_exit",
        {
            {"old_frontier_settlement_entrance", RouteNodeKind::Entrance, std::nullopt},
            {"settlement_outer_fence", RouteNodeKind::Site, "settlement_outer_fence"},
            {"settlement_common_well", RouteNodeKind::Site, "settlement_common_well"},
            {"settlement_camp1", RouteNodeKind::Camp, std::nullopt},
            {"settlement_granary_hall_branch", RouteNodeKind::BranchGroup, std::nullopt,
             {"settlement_old_granary", "settlement_gathering_hall"}, BranchCompletion::AllMembers},
            {"settlement_old_granary", RouteNodeKind::Site, "settlement_old_granary"},
            {"settlement_gathering_hall", RouteNodeKind::Site, "settlement_gathering_hall"},
            {"settlement_camp2", RouteNodeKind::Camp, std::nullopt},
            {"settlement_dawn_defense", RouteNodeKind::Site, "settlement_dawn_defense"},
            {"old_frontier_settlement_exit", RouteNodeKind::Exit, std::nullopt},
        },
        {
            {"old_frontier_settlement_entrance", "settlement_outer_fence"},
            {"settlement_outer_fence", "settlement_common_well"},
            {"settlement_common_well", "settlement_camp1"},
            {"settlement_camp1", "settlement_granary_hall_branch"},
            {"settlement_old_granary", "settlement_granary_hall_branch"},
            {"settlement_gathering_hall", "settlement_granary_hall_branch"},
            {"settlement_granary_hall_branch", "settlement_camp2"},
            {"settlement_camp2", "settlement_dawn_defense"},
            {"settlement_dawn_defense", "old_frontier_settlement_exit"},
        },
    };
    return graph;
}

// docs/regions/ember_ravine.md「地点構成」: entrance -> 1.焼け石の入口 ->
// 2.熱風の棚道 -> キャンプI -> (3.硫黄窪地 と 4.破損冷却水路、どちらを先に
// 攻略してもよいが両方必須) -> キャンプII -> 5.灰晶採取棚 -> 6.旧耐熱工房 ->
// キャンプIII -> 7.灰封観測所 -> 8.赤熱裂け目 -> exit. Site 3/4's branch is
// BranchCompletion::AllMembers, same shape as Windscar's windwatch_station/
// split_convoy and Old Frontier Settlement's settlement_old_granary/
// settlement_gathering_hall branches (both required, order-free) - matching
// the doc's own "地点3・4は攻略順を選択可能"/"地点5へ進むには地点3・4の両方を
// 確保する". Site 1 (`ember_ravine_entrance`) is real content as of this
// Slice; sites 2-8 are placeholders (see emberRavineRegion() in Region.cpp) -
// this graph's own wiring is unaffected by that distinction, same as every
// prior region's skeleton Slice.
// docs/regions/buried_dawn_sanctum.md「地点と周回」: entrance -> 1.埋没参道 ->
// 2.崩れた礼拝堂 -> キャンプI -> (3.救護室 と 4.写本庫、順序選択、両方必須) ->
// キャンプII -> 5.封鎖回廊 -> 6.夜明け祭壇 -> exit. The site 3/4 branch is
// BranchCompletion::AllMembers, same shape as Old Frontier Settlement's
// settlement_old_granary/settlement_gathering_hall branch and Ember Ravine's
// sulfur_hollow/ravine_cooling_channel branch (both required, order-free) -
// the doc's "順序選択" is that same shape, not a new mechanism.
const RegionRouteGraph& buriedDawnSanctumGraph() {
    static const RegionRouteGraph graph{
        RegionId::BuriedDawnSanctum,
        "buried_dawn_sanctum_main_route",
        "buried_dawn_sanctum_entrance_node",
        "buried_dawn_sanctum_exit",
        {
            {"buried_dawn_sanctum_entrance_node", RouteNodeKind::Entrance, std::nullopt},
            {"sanctum_approach", RouteNodeKind::Site, "sanctum_approach"},
            {"collapsed_nave", RouteNodeKind::Site, "collapsed_nave"},
            {"sanctum_camp1", RouteNodeKind::Camp, std::nullopt},
            {"sanctum_infirmary_archive_branch", RouteNodeKind::BranchGroup, std::nullopt,
             {"sanctum_infirmary", "sanctum_archive"}, BranchCompletion::AllMembers},
            {"sanctum_infirmary", RouteNodeKind::Site, "sanctum_infirmary"},
            {"sanctum_archive", RouteNodeKind::Site, "sanctum_archive"},
            {"sanctum_camp2", RouteNodeKind::Camp, std::nullopt},
            {"sealed_passage", RouteNodeKind::Site, "sealed_passage"},
            {"dawn_altar", RouteNodeKind::Site, "dawn_altar"},
            {"buried_dawn_sanctum_exit", RouteNodeKind::Exit, std::nullopt},
        },
        {
            {"buried_dawn_sanctum_entrance_node", "sanctum_approach"},
            {"sanctum_approach", "collapsed_nave"},
            {"collapsed_nave", "sanctum_camp1"},
            {"sanctum_camp1", "sanctum_infirmary_archive_branch"},
            {"sanctum_infirmary", "sanctum_infirmary_archive_branch"},
            {"sanctum_archive", "sanctum_infirmary_archive_branch"},
            {"sanctum_infirmary_archive_branch", "sanctum_camp2"},
            {"sanctum_camp2", "sealed_passage"},
            {"sealed_passage", "dawn_altar"},
            {"dawn_altar", "buried_dawn_sanctum_exit"},
        },
    };
    return graph;
}

const RegionRouteGraph& emberRavineGraph() {
    static const RegionRouteGraph graph{
        RegionId::EmberRavine,
        "ember_ravine_main_route",
        "ember_ravine_entrance_node",
        "ember_ravine_exit",
        {
            {"ember_ravine_entrance_node", RouteNodeKind::Entrance, std::nullopt},
            {"ember_ravine_entrance", RouteNodeKind::Site, "ember_ravine_entrance"},
            {"ember_ravine_ledge", RouteNodeKind::Site, "ember_ravine_ledge"},
            {"ember_ravine_camp1", RouteNodeKind::Camp, std::nullopt},
            {"ember_ravine_sulfur_channel_branch", RouteNodeKind::BranchGroup, std::nullopt,
             {"sulfur_hollow", "ravine_cooling_channel"}, BranchCompletion::AllMembers},
            {"sulfur_hollow", RouteNodeKind::Site, "sulfur_hollow"},
            {"ravine_cooling_channel", RouteNodeKind::Site, "ravine_cooling_channel"},
            {"ember_ravine_camp2", RouteNodeKind::Camp, std::nullopt},
            {"ash_crystal_shelf", RouteNodeKind::Site, "ash_crystal_shelf"},
            {"heatwork_shop", RouteNodeKind::Site, "heatwork_shop"},
            {"ember_ravine_camp3", RouteNodeKind::Camp, std::nullopt},
            {"ashsealed_observatory", RouteNodeKind::Site, "ashsealed_observatory"},
            {"redheat_fissure", RouteNodeKind::Site, "redheat_fissure"},
            {"ember_ravine_exit", RouteNodeKind::Exit, std::nullopt},
        },
        {
            {"ember_ravine_entrance_node", "ember_ravine_entrance"},
            {"ember_ravine_entrance", "ember_ravine_ledge"},
            {"ember_ravine_ledge", "ember_ravine_camp1"},
            {"ember_ravine_camp1", "ember_ravine_sulfur_channel_branch"},
            {"sulfur_hollow", "ember_ravine_sulfur_channel_branch"},
            {"ravine_cooling_channel", "ember_ravine_sulfur_channel_branch"},
            {"ember_ravine_sulfur_channel_branch", "ember_ravine_camp2"},
            {"ember_ravine_camp2", "ash_crystal_shelf"},
            {"ash_crystal_shelf", "heatwork_shop"},
            {"heatwork_shop", "ember_ravine_camp3"},
            {"ember_ravine_camp3", "ashsealed_observatory"},
            {"ashsealed_observatory", "redheat_fissure"},
            {"redheat_fissure", "ember_ravine_exit"},
        },
    };
    return graph;
}

} // namespace

bool usesRouteGraph(RegionId regionId) {
    return regionId == RegionId::AshboughForest || regionId == RegionId::CinderwatchGate ||
           regionId == RegionId::AshironQuarry || regionId == RegionId::BlackwaterLowlands ||
           regionId == RegionId::WindscarPlateau || regionId == RegionId::OldFrontierSettlement ||
           regionId == RegionId::EmberRavine || regionId == RegionId::BuriedDawnSanctum;
}

const RegionRouteGraph& regionRouteGraph(RegionId regionId) {
    if (regionId == RegionId::AshboughForest) return ashboughGraph();
    if (regionId == RegionId::CinderwatchGate) return cinderwatchGraph();
    if (regionId == RegionId::AshironQuarry) return ashironQuarryGraph();
    if (regionId == RegionId::BlackwaterLowlands) return blackwaterLowlandsGraph();
    if (regionId == RegionId::WindscarPlateau) return windscarPlateauGraph();
    if (regionId == RegionId::EmberRavine) return emberRavineGraph();
    if (regionId == RegionId::OldFrontierSettlement) return oldFrontierSettlementGraph();
    if (regionId == RegionId::BuriedDawnSanctum) return buriedDawnSanctumGraph();
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
