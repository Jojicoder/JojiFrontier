#include "jf/core/ExpeditionService.hpp"

#include <algorithm>
#include <unordered_map>

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

ReturnToBaseResult applyExpeditionReturnToBase(ExpeditionState& expedition, BaseState& baseState,
                                               std::uint64_t& returnGrantSequence) {
    // docs/inventory_overflow.md「帰還処理」: compute what fits before
    // mutating anything, so a 200-Stack ceiling breach (checked below) leaves
    // storage/overflow untouched rather than partially applied.
    std::unordered_map<LootId, int> materialAdds;
    for (const LootStack& loot : expedition.pendingLoot) materialAdds[loot.id] += loot.quantity;

    std::unordered_map<LootId, int> fitPlan;
    std::vector<std::pair<LootId, int>> overflowPlan;
    for (const auto& [id, quantity] : materialAdds) {
        const bool isKeyMaterial = baseState.materialStorageCap(id) == BaseState::kKeyMaterialStorageCap;
        const int cap = baseState.materialStorageCap(id);
        const int current = baseState.storageCount(id);
        const int room = std::max(0, cap - current);
        const int fits = std::min(room, quantity);
        if (fits > 0) fitPlan[id] = fits;
        // docs/inventory_overflow.md「保留中のキー素材...は存在させない。これらは
        // 重複除去して直接恒久化する」: a key material's excess beyond its
        // 1-cap is deduplicated away here, never queued as overflow.
        if (!isKeyMaterial) {
            const int overflow = quantity - fits;
            if (overflow > 0) overflowPlan.push_back({id, overflow});
        }
    }

    // Unused expedition items are already owned by the player, but they still
    // need the same capacity-safe commit as newly secured materials. This also
    // makes imported/older saves safe when their storage and bag totals exceed
    // the current per-item cap.
    std::unordered_map<ItemType, int> returnedItems;
    for (ItemType item : expedition.bag) ++returnedItems[item];
    std::unordered_map<ItemType, int> itemFitPlan;
    for (const auto& [item, quantity] : returnedItems) {
        const int room = std::max(0, BaseState::kItemStorageCap - baseState.ownedItemCount(item));
        const int fits = std::min(room, quantity);
        if (fits > 0) itemFitPlan[item] = fits;
        const int overflow = quantity - fits;
        if (overflow > 0)
            overflowPlan.push_back({"item:" + std::to_string(static_cast<int>(item)), overflow});
    }

    if (baseState.rewardOverflow.stacks.size() + overflowPlan.size() > RewardOverflowState::kMaxStacks)
        return {};

    ReturnToBaseResult result;
    result.success = true;
    for (const LootStack& loot : expedition.pendingLoot) result.securedLootIds.push_back(loot.id);
    for (const auto& [id, quantity] : fitPlan) baseState.addStorage(id, quantity);
    for (const auto& [item, quantity] : itemFitPlan) baseState.addItemStorage(item, quantity);
    if (!overflowPlan.empty()) {
        const std::string grantId = "return-" + std::to_string(++returnGrantSequence);
        for (const auto& [id, quantity] : overflowPlan)
            baseState.rewardOverflow.stacks.push_back({grantId, id, quantity});
    }

    for (const DiscoveryId& discovery : expedition.pendingDiscoveries) baseState.discoveryRegistry.insert(discovery);
    for (const auto& [key, achieved] : expedition.pendingSiteAccessUpdates) {
        auto it = baseState.siteAccess.find(key);
        if (it == baseState.siteAccess.end() || it->second < achieved) baseState.siteAccess[key] = achieved;
    }
    for (RegionId regionId : expedition.pendingRegionCompletions) baseState.completedRegionIds.insert(regionId);
    // docs/roster_design.md「加入処理の共通ルール」: 加入候補→加入可能候補への
    // 恒久化。以後は別遠征の敗北でも失わない(joinReadyCandidateIdsは単調増加)。
    for (const std::string& id : expedition.pendingRecruitCandidateIds) baseState.joinReadyCandidateIds.insert(id);
    // The bag has been committed above; GameApp::resetToBase() must not
    // return it a second time.
    expedition.bag.clear();
    return result;
}

ExpeditionCheckpoint buildExpeditionCheckpoint(ExpeditionCheckpoint::Stage stage, const ExpeditionState& expedition,
                                               std::uint32_t seed, const std::vector<bool>& stageDiscoveryAwarded,
                                               const std::vector<Unit>& partyUnits) {
    ExpeditionCheckpoint checkpoint;
    checkpoint.stage = stage;
    checkpoint.regionId = expedition.regionId;
    checkpoint.expeditionStage = expedition.stageIndex;
    checkpoint.seed = seed;
    checkpoint.pendingLoot = expedition.pendingLoot;
    checkpoint.pendingDiscoveries = expedition.pendingDiscoveries;
    checkpoint.bag = expedition.bag;
    checkpoint.battlesWon = expedition.battlesWon;
    checkpoint.routeProgress = expedition.routeProgress;
    checkpoint.stageDiscoveryAwarded = stageDiscoveryAwarded;
    checkpoint.pendingSiteAccessUpdates = expedition.pendingSiteAccessUpdates;
    checkpoint.pendingRegionCompletions = expedition.pendingRegionCompletions;
    checkpoint.pendingRecruitCandidateIds = expedition.pendingRecruitCandidateIds;
    for (const Unit& unit : partyUnits) checkpoint.partyUnits.push_back({unit.id, unit.currentHp});
    return checkpoint;
}

int bulkAdvanceSecuredSites(ExpeditionState& expedition, const BaseState& baseState, const GameData& data) {
    if (!expedition.routeProgress) return 0;
    int passed = 0;
    // Mirrors continueExpedition()'s own guard ("don't advance once the
    // expedition is already complete"): stop the instant the site just
    // marked resolved is the last one before the Exit, WITHOUT calling
    // advanceExpeditionRouteToNextSite() - that call would move
    // currentNodeId to the Exit node itself, which breaks
    // computeExpeditionComplete()'s invariant that currentNodeId always
    // names the last *resolved Site*, not the Exit.
    while (computeCurrentStage(expedition, data).contentImplemented &&
          computeCurrentSiteAccess(expedition, baseState, data) == SiteAccessState::Secured) {
        expedition.routeProgress->resolvedNodeIds.insert(expedition.routeProgress->currentNodeId);
        expedition.routeProgress->safelyPassedNodeIds.insert(expedition.routeProgress->currentNodeId);
        expedition.battlesWon += 1;
        ++passed;
        if (computeExpeditionComplete(expedition, data)) break;
        if (!advanceExpeditionRouteToNextSite(expedition, baseState, data)) break; // defensive: shouldn't happen
    }
    return passed;
}

} // namespace jf
