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
    auto isMemberResolved = [&](const std::string& memberId) {
        if (progress.resolvedNodeIds.count(memberId)) return true;
        const RouteNodeDefinition* member = findRouteNode(graph, memberId);
        if (member && member->stageId) {
            auto access = baseState.siteAccess.find(siteAccessKey(regionId, *member->stageId));
            if (access != baseState.siteAccess.end() && access->second >= SiteAccessState::Secured) return true;
        }
        return false;
    };
    // docs/regions/ashiron_quarry.md「地点構成」: unlike Cinderwatch's only
    // BranchGroup (AllMembers - clear every member), AnyMember means the
    // branch is done as soon as ONE member is resolved (this expedition or
    // permanently), regardless of the others' state.
    if (branch.branchCompletion == BranchCompletion::AnyMember) {
        for (const std::string& memberId : branch.branchMembers)
            if (isMemberResolved(memberId)) return nullptr;
    }
    for (const std::string& memberId : branch.branchMembers) {
        if (isMemberResolved(memberId)) continue;
        return findRouteNode(graph, memberId);
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
    // regionUnlocked()'s own single-predecessor gate per region, mirrored
    // here only for the locked-tooltip's "unlocks after clearing X" text -
    // AshboughForest has no predecessor (always unlocked).
    auto predecessor = [](RegionId id) -> std::optional<RegionId> {
        if (id == RegionId::CinderwatchGate) return RegionId::AshboughForest;
        if (id == RegionId::AshironQuarry) return RegionId::CinderwatchGate;
        if (id == RegionId::BlackwaterLowlands) return RegionId::AshironQuarry;
        if (id == RegionId::WindscarPlateau) return RegionId::BlackwaterLowlands;
        // docs/regions/old_frontier_settlement.md「燼火峡谷を遠征先へ追加」(M9-Y):
        // OldFrontierSettlement/EmberRavine were both missing from this
        // lambda AND the summaries loop below - a pre-existing gap dating to
        // M9-U (OldFrontierSettlement's own region-skeleton Slice never added
        // itself here). Fixed as part of this Slice's region-clear wiring,
        // since without it neither region can ever appear on the Base
        // screen at all, regardless of regionUnlocked()'s own (correct)
        // completedRegionIds check.
        if (id == RegionId::OldFrontierSettlement) return RegionId::WindscarPlateau;
        if (id == RegionId::EmberRavine) return RegionId::OldFrontierSettlement;
        return std::nullopt;
    };

    std::vector<RegionSummary> summaries;
    for (RegionId id : {RegionId::AshboughForest, RegionId::CinderwatchGate, RegionId::AshironQuarry,
                        RegionId::BlackwaterLowlands, RegionId::WindscarPlateau, RegionId::OldFrontierSettlement,
                        RegionId::EmberRavine}) {
        RegionDescriptor region = regionDescriptor(id, data);
        bool unlocked = regionUnlocked(id, baseState, data);
        RegionSummary summary{id, region.displayNameEn, region.displayNameJa, unlocked, "", ""};
        if (!unlocked) {
            if (std::optional<RegionId> prior = predecessor(id)) {
                RegionDescriptor priorRegion = regionDescriptor(*prior, data);
                summary.lockedByDisplayNameEn = priorRegion.displayNameEn;
                summary.lockedByDisplayNameJa = priorRegion.displayNameJa;
            }
        }
        summaries.push_back(summary);
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

    // docs/regions/cinderwatch_gate.md「地域の最低保証報酬」: track this run's
    // Cinderwatch materials into the region-wide running tally (only while the
    // region hasn't completed yet - once it has, the floor no longer applies).
    const bool cinderwatchStillOpen =
        expedition.regionId == RegionId::CinderwatchGate && !baseState.completedRegionIds.count(RegionId::CinderwatchGate);
    if (cinderwatchStillOpen)
        for (const auto& [id, quantity] : materialAdds) baseState.cinderwatchMaterialsEarned[id] += quantity;

    // If this return completes CinderwatchGate, top up any shortfall against
    // the floor table before cap/overflow is computed below, so the top-up
    // gets the exact same capacity-safe treatment as normal loot.
    std::vector<DiscoveryId> discoveriesThisReturn = expedition.pendingDiscoveries;
    if (cinderwatchStillOpen && expedition.pendingRegionCompletions.count(RegionId::CinderwatchGate)) {
        static const std::unordered_map<LootId, int> kCinderwatchMaterialFloor = {
            {"iron", 5}, {"stone", 3}, {"old_gear", 3}, {"signal_core", 1},
        };
        for (const auto& [id, floor] : kCinderwatchMaterialFloor) {
            const int earned = baseState.cinderwatchMaterialsEarned[id];
            if (earned < floor) materialAdds[id] += floor - earned;
        }
        static const std::vector<DiscoveryId> kCinderwatchKeyDiscoveries = {
            kCinderwatchReconDiscovery, kFieldMedicineDiscovery, kReturnSignalDiscovery, kBannerRecordsDiscovery,
        };
        for (const DiscoveryId& discovery : kCinderwatchKeyDiscoveries) {
            const bool alreadyHave = baseState.discoveryRegistry.count(discovery) ||
                                      std::find(discoveriesThisReturn.begin(), discoveriesThisReturn.end(), discovery) !=
                                          discoveriesThisReturn.end();
            if (!alreadyHave) discoveriesThisReturn.push_back(discovery);
        }
        // docs/roster_design.md「加入タイミング」: 旗手の加入候補は「軍旗記録
        // registered」= 地域攻略後の安全帰還が条件そのものなので、Pending経由
        // せずこの完了Transaction内で直接恒久化する。
        baseState.joinReadyCandidateIds.insert("banner_recruit");
    }

    // docs/regions/blackwater_lowlands.md「最低保証報酬」: same mechanism as
    // Cinderwatch's floor top-up above, tracked/applied independently.
    const bool blackwaterStillOpen = expedition.regionId == RegionId::BlackwaterLowlands &&
                                     !baseState.completedRegionIds.count(RegionId::BlackwaterLowlands);
    if (blackwaterStillOpen)
        for (const auto& [id, quantity] : materialAdds) baseState.blackwaterMaterialsEarned[id] += quantity;

    if (blackwaterStillOpen && expedition.pendingRegionCompletions.count(RegionId::BlackwaterLowlands)) {
        static const std::unordered_map<LootId, int> kBlackwaterMaterialFloor = {
            {"herb", 8}, {"quality_herb", 1}, {"poison_material", 4}, {"wetland_resin", 7},
        };
        for (const auto& [id, floor] : kBlackwaterMaterialFloor) {
            const int earned = baseState.blackwaterMaterialsEarned[id];
            if (earned < floor) materialAdds[id] += floor - earned;
        }
        static const std::vector<DiscoveryId> kBlackwaterKeyDiscoveries = {
            kBlackwaterSurveyDiscovery, kMarshPharmacologyDiscovery, kMarshTrapcraftDiscovery,
            kMarshEmergencyMedicineDiscovery,
        };
        for (const DiscoveryId& discovery : kBlackwaterKeyDiscoveries) {
            const bool alreadyHave = baseState.discoveryRegistry.count(discovery) ||
                                      std::find(discoveriesThisReturn.begin(), discoveriesThisReturn.end(), discovery) !=
                                          discoveriesThisReturn.end();
            if (!alreadyHave) discoveriesThisReturn.push_back(discovery);
        }
    }

    // docs/regions/windscar_plateau.md「最低保証報酬」: same mechanism as
    // Cinderwatch/Blackwater's floor top-up above, tracked/applied
    // independently.
    const bool windscarStillOpen = expedition.regionId == RegionId::WindscarPlateau &&
                                   !baseState.completedRegionIds.count(RegionId::WindscarPlateau);
    if (windscarStillOpen)
        for (const auto& [id, quantity] : materialAdds) baseState.windscarMaterialsEarned[id] += quantity;

    if (windscarStillOpen && expedition.pendingRegionCompletions.count(RegionId::WindscarPlateau)) {
        static const std::unordered_map<LootId, int> kWindscarMaterialFloor = {
            {"hide", 5}, {"cloth", 7}, {"hardwood", 5}, {"riding_gear", 4},
        };
        for (const auto& [id, floor] : kWindscarMaterialFloor) {
            const int earned = baseState.windscarMaterialsEarned[id];
            if (earned < floor) materialAdds[id] += floor - earned;
        }
        static const std::vector<DiscoveryId> kWindscarKeyDiscoveries = {
            kWindscarRoadChartDiscovery, kCavalryOperationRecordsDiscovery, kPlateauTargetingRecordsDiscovery,
            kCourierRouteChartDiscovery,
        };
        for (const DiscoveryId& discovery : kWindscarKeyDiscoveries) {
            const bool alreadyHave = baseState.discoveryRegistry.count(discovery) ||
                                      std::find(discoveriesThisReturn.begin(), discoveriesThisReturn.end(), discovery) !=
                                          discoveriesThisReturn.end();
            if (!alreadyHave) discoveriesThisReturn.push_back(discovery);
        }
    }

    // docs/regions/old_frontier_settlement.md「最低保証報酬」: same mechanism as
    // Cinderwatch/Blackwater/Windscar's floor top-up above, tracked/applied
    // independently.
    const bool settlementStillOpen = expedition.regionId == RegionId::OldFrontierSettlement &&
                                     !baseState.completedRegionIds.count(RegionId::OldFrontierSettlement);
    if (settlementStillOpen)
        for (const auto& [id, quantity] : materialAdds) baseState.settlementMaterialsEarned[id] += quantity;

    if (settlementStillOpen && expedition.pendingRegionCompletions.count(RegionId::OldFrontierSettlement)) {
        static const std::unordered_map<LootId, int> kSettlementMaterialFloor = {
            {"building_material", 6}, {"iron", 3}, {"food", 7}, {"cloth", 2},
        };
        for (const auto& [id, floor] : kSettlementMaterialFloor) {
            const int earned = baseState.settlementMaterialsEarned[id];
            if (earned < floor) materialAdds[id] += floor - earned;
        }
        // docs/regions/old_frontier_settlement.md「安定ID」table: 集落台帳・
        // 援護命令 and 集団防衛・不動の構え each share 1 id -
        // kSettlementCommandLedgerDiscovery/kCollectiveDefenseRecordsDiscovery's
        // own comments explain why the doc's 4-row floor table collapses to
        // these 2 ids.
        static const std::vector<DiscoveryId> kSettlementKeyDiscoveries = {
            kSettlementCommandLedgerDiscovery, kCollectiveDefenseRecordsDiscovery,
        };
        for (const DiscoveryId& discovery : kSettlementKeyDiscoveries) {
            const bool alreadyHave = baseState.discoveryRegistry.count(discovery) ||
                                      std::find(discoveriesThisReturn.begin(), discoveriesThisReturn.end(), discovery) !=
                                          discoveriesThisReturn.end();
            if (!alreadyHave) discoveriesThisReturn.push_back(discovery);
        }
    }

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

    for (const DiscoveryId& discovery : discoveriesThisReturn) baseState.discoveryRegistry.insert(discovery);
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
