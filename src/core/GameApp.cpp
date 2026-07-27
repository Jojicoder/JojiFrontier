#include "jf/core/GameApp.hpp"

#include "jf/battle/BattleFactory.hpp"
#include "jf/battle/SkillCharges.hpp"
#include "jf/core/Skill.hpp"

#include <algorithm>
#include <random>

namespace jf {

namespace {
std::uint32_t makeExpeditionSeed() {
    std::random_device device;
    return (static_cast<std::uint32_t>(device()) << 1u) ^ static_cast<std::uint32_t>(device());
}

// The Base screen always keeps a BattleController alive even though nothing
// is actually being played there yet; which region/stage backs that idle
// placeholder is arbitrary. Kept as its own function so the constructor and
// resetToBase() can't drift apart.
StageDescriptor idlePlaceholderStage(const GameData& data) {
    return regionDescriptor(RegionId::CinderwatchGate, data).stages.at(0);
}
} // namespace

GameApp::GameApp(GameData data) : data_(std::move(data)) {
    roster_ = data_.playerParty;
    roster_.insert(roster_.end(), data_.reserveRoster.begin(), data_.reserveRoster.end());
    for (const auto& unit : data_.playerParty) selectedPartyIds_.push_back(unit.id);
    expeditionSeed_ = makeExpeditionSeed();
    battleController_ = std::make_unique<BattleController>(
        createScenarioBattle(data_, idlePlaceholderStage(data_), expeditionSeed_));
}

void GameApp::update(float dt) {
    if (screen_ == Screen::Battle) {
        battleController_->update(dt);
    }
}

void GameApp::proceedToCamp() {
    // docs/region_mission_data_contract.md "二重付与防止": without this
    // guard, calling proceedToCamp() again after screen_ already transitioned
    // to Camp (inputState() stays Victory forever - nothing resets it) would
    // re-run the whole reward grant, doubling pendingLoot and battlesWon
    // every time. Every other screen-transition method already guards on
    // the screen it's leaving; this one was missing it.
    if (screen_ != Screen::Battle || battleController_->inputState() != BattleInputState::Victory) return;
    const StageDescriptor stage = currentStage();

    bool surveySucceeded = false;
    if (stage.surveyObjectiveId && !isReconnaissanceRun_) {
        // BattleFactory groups every survey-tile objective (one or several,
        // e.g. Herbwater Hollow's 2 HerbPatch tiles) under a group id equal
        // to surveyObjectiveId, so this succeeds if any of them completed -
        // whether there's 1 tile or several.
        const BattleMissionState& mission = battleController_->battle().missionState();
        for (const ObjectiveDefinition& def : mission.definitions) {
            if (def.groupId == *stage.surveyObjectiveId &&
                mission.progress.at(def.id).status == ObjectiveStatus::Completed) {
                surveySucceeded = true;
                break;
            }
        }
    }
    std::vector<LootStack> loot = computeStageVictoryLoot(stage, lastExplorationChoice_, surveySucceeded);
    // docs/regions/ashbough_forest.md "折れ木の縄張り"'s ad-hoc secondary
    // bonuses - merged by id (not just appended) so a bonus sharing an id
    // with the base reward (e.g. 獣皮) becomes one combined stack rather
    // than two separate same-id entries.
    auto mergeLoot = [&](const std::vector<LootStack>& extra) {
        for (const LootStack& stack : extra) {
            auto it = std::find_if(loot.begin(), loot.end(),
                                   [&](const LootStack& entry) { return entry.id == stack.id; });
            if (it == loot.end()) loot.push_back(stack);
            else it->quantity += stack.quantity;
        }
    };
    if (!isReconnaissanceRun_ && battleController_->battle().bossHasCollidedWithBarrier())
        mergeLoot(stage.logCollisionBonusLoot);
    if (!isReconnaissanceRun_ &&
        std::none_of(battleController_->battle().units().begin(), battleController_->battle().units().end(),
                    [](const Unit& unit) { return unit.team == Team::Player && !unit.isAlive(); }))
        mergeLoot(stage.noCasualtiesBonusLoot);
    // docs/regions/blackwater_lowlands.md「5. 黒水渡し」's "全員脱出: 高品質薬草1" -
    // "both guests escaped" isn't expressible through RewardRule's Condition
    // enum (SurveySuccess only reads a surveyObjectiveId group, not
    // creditedTargetIds' size), so it's checked directly here, same
    // ad-hoc-secondary-bonus pattern as logCollisionBonusLoot/
    // noCasualtiesBonusLoot above. The crate's "荷物箱保持: 毒素材1" doesn't
    // need a twin here - it's already an ordinary SurveySuccess RewardRule
    // (stage.victoryRewardRules), folded into `loot` above.
    if (!isReconnaissanceRun_ && stage.id == "blackwater_crossing") {
        const BattleMissionState& mission = battleController_->battle().missionState();
        for (const ObjectiveDefinition& def : mission.definitions) {
            if (def.id != "blackwater_crossing_escape") continue;
            if (mission.progress.at(def.id).creditedTargetIds.size() >= 2)
                mergeLoot({{"quality_herb", 1}});
            break;
        }
    }
    // docs/regions/blackwater_lowlands.md「7. 深泥の水源」の副目標「薬草地点を
    // 使用せず勝利」: same ad-hoc-secondary-bonus pattern as blackwater_crossing
    // above - no RewardRule::Condition reads BattleState::collectedHerbPatches().
    if (!isReconnaissanceRun_ && stage.id == "deep_mire" && battleController_->battle().collectedHerbPatches() == 0)
        mergeLoot({{"quality_herb", 1}});
    expedition_.pendingLoot.insert(expedition_.pendingLoot.end(), loot.begin(), loot.end());

    // docs/regions/buried_dawn_sanctum.md「2. 崩れた礼拝堂」の副目標「避難者
    // 全員脱出」→ 野戦救護記録: same ad-hoc creditedTargetIds.size()>=2 check
    // as blackwater_crossing's own "2人とも脱出" bonus above, granting a
    // Discovery instead of Loot (same shape as quarry_old_mine's own
    // kMiningTechniqueRecordsDiscovery check below).
    if (!isReconnaissanceRun_ && stage.id == "collapsed_nave") {
        const BattleMissionState& mission = battleController_->battle().missionState();
        for (const ObjectiveDefinition& def : mission.definitions) {
            if (def.id != "collapsed_nave_escape") continue;
            if (mission.progress.at(def.id).creditedTargetIds.size() >= 2 &&
                std::find(expedition_.pendingDiscoveries.begin(), expedition_.pendingDiscoveries.end(),
                         kFieldMedicalRecordsDiscovery) == expedition_.pendingDiscoveries.end())
                expedition_.pendingDiscoveries.push_back(kFieldMedicalRecordsDiscovery);
            break;
        }
    }
    // docs/regions/shattered_march_fort.md「旧兵舎」の副目標「負傷兵全員避難」
    // -> 集団救護記録: same ad-hoc creditedTargetIds.size()>=2 check as
    // blackwater_crossing's own "2人とも脱出" bonus above, granting a
    // Discovery instead of Loot (same shape as collapsed_nave's own
    // kFieldMedicalRecordsDiscovery check above).
    if (!isReconnaissanceRun_ && stage.id == "fort_old_barracks") {
        const BattleMissionState& mission = battleController_->battle().missionState();
        for (const ObjectiveDefinition& def : mission.definitions) {
            if (def.id != "fort_old_barracks_escape") continue;
            if (mission.progress.at(def.id).creditedTargetIds.size() >= 2 &&
                std::find(expedition_.pendingDiscoveries.begin(), expedition_.pendingDiscoveries.end(),
                         kGroupTriageRecordsDiscovery) == expedition_.pendingDiscoveries.end())
                expedition_.pendingDiscoveries.push_back(kGroupTriageRecordsDiscovery);
            break;
        }
    }
    // docs/regions/ashiron_quarry.md「3A. 旧採掘坑」の副目標「作業員2人とも
    // 脱出」→ 採掘技術記録: same ad-hoc creditedTargetIds.size()>=2 check as
    // blackwater_crossing's own "2人とも脱出" bonus above, but granting a
    // Discovery instead of Loot (RewardRule has no Discovery-granting shape).
    if (!isReconnaissanceRun_ && stage.id == "quarry_old_mine") {
        const BattleMissionState& mission = battleController_->battle().missionState();
        for (const ObjectiveDefinition& def : mission.definitions) {
            if (def.id != "quarry_old_mine_escape") continue;
            if (mission.progress.at(def.id).creditedTargetIds.size() >= 2 &&
                std::find(expedition_.pendingDiscoveries.begin(), expedition_.pendingDiscoveries.end(),
                         kMiningTechniqueRecordsDiscovery) == expedition_.pendingDiscoveries.end())
                expedition_.pendingDiscoveries.push_back(kMiningTechniqueRecordsDiscovery);
            break;
        }
    }
    // docs/regions/ashiron_quarry.md「4. 灰鉄鉱脈」の副目標「イリエンを撤退
    // させない」(ObjectiveKind::ProtectUnit's documented purpose, but NOT
    // wired through that Kind here - see ashironVeinStage()'s own comment
    // for why) combined with "測定完了": approximated as Irien (the
    // ashiron_vein_irien guest) still being present when the battle ends in
    // Victory - the same ad-hoc isPresent()-style check every other
    // Kind-mismatch secondary bonus in this function already uses.
    if (!isReconnaissanceRun_ && stage.id == "ashiron_vein") {
        const Unit* irien = battleController_->battle().findUnit("ashiron_vein_irien");
        if (irien != nullptr && irien->isPresent()) {
            expedition_.pendingRecruitCandidateIds.insert("mage_recruit");
            if (std::find(expedition_.pendingDiscoveries.begin(), expedition_.pendingDiscoveries.end(),
                         kAnomalousVeinRecordsDiscovery) == expedition_.pendingDiscoveries.end())
                expedition_.pendingDiscoveries.push_back(kAnomalousVeinRecordsDiscovery);
        }
    }

    // docs/regions/ember_ravine.md「6. 旧耐熱工房」の「記録箱2個: 特殊鍛造記録」:
    // surveyObjectiveId's group is always ObjectiveGroupRule::Any, so it
    // can't itself distinguish "1個以上"(primary-adjacent secondary, already
    // handled via surveySucceeded/RewardRule above) from "2個とも"(this
    // bonus). Scan every individual objective under the group id directly
    // and require them ALL to be Completed - kSpecialForgingRecordsDiscovery's
    // own comment explains why this needs the ad-hoc check instead of a
    // RewardRule::Condition.
    if (!isReconnaissanceRun_ && stage.id == "heatwork_shop") {
        const BattleMissionState& mission = battleController_->battle().missionState();
        bool anyInGroup = false;
        bool allCompleted = true;
        for (const ObjectiveDefinition& def : mission.definitions) {
            if (def.groupId != "heatwork_shop_crate") continue;
            anyInGroup = true;
            if (mission.progress.at(def.id).status != ObjectiveStatus::Completed) {
                allCompleted = false;
                break;
            }
        }
        if (anyInGroup && allCompleted &&
            std::find(expedition_.pendingDiscoveries.begin(), expedition_.pendingDiscoveries.end(),
                     kSpecialForgingRecordsDiscovery) == expedition_.pendingDiscoveries.end())
            expedition_.pendingDiscoveries.push_back(kSpecialForgingRecordsDiscovery);
    }

    // docs/regions/mapped_edge.md「9地点仕様」地点7「折れた見張台」の主目的
    // 報酬「地図外縁踏査記録」: heatwork_shopのkSpecialForgingRecordsDiscovery
    // と全く同じall-group-members-Completed ad-hocチェック(surveyObjectiveId
    // のgroupは常にObjectiveGroupRule::Anyのため、「記録箱2個とも回収」という
    // 厳密条件はこの直接スキャンでしか表現できない)。主目的自体は観測盤
    // (`operate_mapped_edge_broken_watchtower_panel`)のOperateObject-primary
    // 近似で、この記録箱グループはsecondary/bonus-reward側。
    if (!isReconnaissanceRun_ && stage.id == "mapped_edge_broken_watchtower") {
        const BattleMissionState& mission = battleController_->battle().missionState();
        bool anyInGroup = false;
        bool allCompleted = true;
        for (const ObjectiveDefinition& def : mission.definitions) {
            if (def.groupId != "mapped_edge_broken_watchtower_crate") continue;
            anyInGroup = true;
            if (mission.progress.at(def.id).status != ObjectiveStatus::Completed) {
                allCompleted = false;
                break;
            }
        }
        if (anyInGroup && allCompleted &&
            std::find(expedition_.pendingDiscoveries.begin(), expedition_.pendingDiscoveries.end(),
                     kMappedEdgeSurveyRecordsDiscovery) == expedition_.pendingDiscoveries.end())
            expedition_.pendingDiscoveries.push_back(kMappedEdgeSurveyRecordsDiscovery);
    }

    // docs/regions/shattered_march_fort.md「兵站庫」の「兵站箱全保全 -> 軍需管理
    // 記録」: same all-group-members-Completed ad-hoc check as heatwork_shop's
    // kSpecialForgingRecordsDiscovery above.
    if (!isReconnaissanceRun_ && stage.id == "fort_logistics_depot") {
        const BattleMissionState& mission = battleController_->battle().missionState();
        bool anyInGroup = false;
        bool allCompleted = true;
        for (const ObjectiveDefinition& def : mission.definitions) {
            if (def.groupId != "fort_logistics_depot_crate") continue;
            anyInGroup = true;
            if (mission.progress.at(def.id).status != ObjectiveStatus::Completed) {
                allCompleted = false;
                break;
            }
        }
        if (anyInGroup && allCompleted &&
            std::find(expedition_.pendingDiscoveries.begin(), expedition_.pendingDiscoveries.end(),
                     kLogisticsManagementRecordsDiscovery) == expedition_.pendingDiscoveries.end())
            expedition_.pendingDiscoveries.push_back(kLogisticsManagementRecordsDiscovery);
    }

    // docs/regions/ember_ravine.md「7. 灰封観測所」の「記録箱2個: 峡谷踏査記録、
    // 灰嵐以前の監視記録」: same all-group-members-Completed ad-hoc check as
    // heatwork_shop's kSpecialForgingRecordsDiscovery above, granting both
    // Discoveries when the "2個とも回収" bonus tier is met.
    if (!isReconnaissanceRun_ && stage.id == "ashsealed_observatory") {
        const BattleMissionState& mission = battleController_->battle().missionState();
        bool anyInGroup = false;
        bool allCompleted = true;
        for (const ObjectiveDefinition& def : mission.definitions) {
            if (def.groupId != "ashsealed_observatory_crate") continue;
            anyInGroup = true;
            if (mission.progress.at(def.id).status != ObjectiveStatus::Completed) {
                allCompleted = false;
                break;
            }
        }
        if (anyInGroup && allCompleted) {
            if (std::find(expedition_.pendingDiscoveries.begin(), expedition_.pendingDiscoveries.end(),
                         kEmberRavineSurveyRecordsDiscovery) == expedition_.pendingDiscoveries.end())
                expedition_.pendingDiscoveries.push_back(kEmberRavineSurveyRecordsDiscovery);
            if (std::find(expedition_.pendingDiscoveries.begin(), expedition_.pendingDiscoveries.end(),
                         kPreAshstormWatchRecordsDiscovery) == expedition_.pendingDiscoveries.end())
                expedition_.pendingDiscoveries.push_back(kPreAshstormWatchRecordsDiscovery);
        }
    }

    // docs/regions/ember_ravine.md「8. 赤熱裂け目」の副目標「冷却弁2個を両方
    // 操作」→ 耐熱加工記録(未取得なら): same all-group-members-Completed
    // ad-hoc check as heatwork_shop's own kSpecialForgingRecordsDiscovery
    // block above, this time over the `redheat_fissure_valves`
    // secondaryOperateObjectiveId group (BattleFactory.cpp places 2
    // independent OperateObject objectives sharing that group id, same shape
    // as ash_crystal_shelf's own `ash_crystal_shelf_gather_points` group).
    if (!isReconnaissanceRun_ && stage.id == "redheat_fissure") {
        const BattleMissionState& mission = battleController_->battle().missionState();
        bool anyInGroup = false;
        bool allCompleted = true;
        for (const ObjectiveDefinition& def : mission.definitions) {
            if (def.groupId != "redheat_fissure_valves") continue;
            anyInGroup = true;
            if (mission.progress.at(def.id).status != ObjectiveStatus::Completed) {
                allCompleted = false;
                break;
            }
        }
        if (anyInGroup && allCompleted && !baseState_.discoveryRegistry.count(kHeatResistantProcessingRecordsDiscovery) &&
            std::find(expedition_.pendingDiscoveries.begin(), expedition_.pendingDiscoveries.end(),
                     kHeatResistantProcessingRecordsDiscovery) == expedition_.pendingDiscoveries.end())
            expedition_.pendingDiscoveries.push_back(kHeatResistantProcessingRecordsDiscovery);
    }

    // docs/regions/buried_dawn_sanctum.md「地点4: 写本庫」の公開副目標「写本箱3個
    // 回収」-> 上位魔法研究記録: same all-group-members-Completed ad-hoc check as
    // heatwork_shop's own kSpecialForgingRecordsDiscovery above, over the
    // `sanctum_archive_crate` surveyObjectiveId group (surveyTileCount:3). The
    // primary's own "写本箱2個確保" is intentionally NOT modeled via this group
    // at all - it is approximated as standard EliminateTeam instead (see
    // kAdvancedMagicResearchRecordsDiscovery's own comment for why).
    if (!isReconnaissanceRun_ && stage.id == "sanctum_archive") {
        const BattleMissionState& mission = battleController_->battle().missionState();
        bool anyInGroup = false;
        bool allCompleted = true;
        for (const ObjectiveDefinition& def : mission.definitions) {
            if (def.groupId != "sanctum_archive_crate") continue;
            anyInGroup = true;
            if (mission.progress.at(def.id).status != ObjectiveStatus::Completed) {
                allCompleted = false;
                break;
            }
        }
        if (anyInGroup && allCompleted &&
            std::find(expedition_.pendingDiscoveries.begin(), expedition_.pendingDiscoveries.end(),
                     kAdvancedMagicResearchRecordsDiscovery) == expedition_.pendingDiscoveries.end())
            expedition_.pendingDiscoveries.push_back(kAdvancedMagicResearchRecordsDiscovery);
    }

    // docs/regions/shattered_march_fort.md「切離命令庫」の公開副目標「命令箱2個」
    // -> 砦指揮記録、切離命令断片: same all-group-members-Completed ad-hoc check
    // as ember_ravine's own ashsealed_observatory block above ("2個とも回収" ->
    // 2 Discoveries at once, M9-AF's own precedent this doc explicitly points
    // to), granting both Discoveries together over the
    // `fort_severance_order_archive_crate` surveyObjectiveId group
    // (surveyTileCount:2).
    if (!isReconnaissanceRun_ && stage.id == "fort_severance_order_archive") {
        const BattleMissionState& mission = battleController_->battle().missionState();
        bool anyInGroup = false;
        bool allCompleted = true;
        for (const ObjectiveDefinition& def : mission.definitions) {
            if (def.groupId != "fort_severance_order_archive_crate") continue;
            anyInGroup = true;
            if (mission.progress.at(def.id).status != ObjectiveStatus::Completed) {
                allCompleted = false;
                break;
            }
        }
        if (anyInGroup && allCompleted) {
            if (std::find(expedition_.pendingDiscoveries.begin(), expedition_.pendingDiscoveries.end(),
                         kFortCommandRecordsDiscovery) == expedition_.pendingDiscoveries.end())
                expedition_.pendingDiscoveries.push_back(kFortCommandRecordsDiscovery);
            if (std::find(expedition_.pendingDiscoveries.begin(), expedition_.pendingDiscoveries.end(),
                         kSeveranceOrderFragmentsDiscovery) == expedition_.pendingDiscoveries.end())
                expedition_.pendingDiscoveries.push_back(kSeveranceOrderFragmentsDiscovery);
        }
    }

    // docs/regions/old_frontier_settlement.md「2. 共同井戸」の副目標「中立住民を
    // 全員避難」→ 集落証言記録: unlike blackwater_crossing/quarry_old_mine's
    // ad-hoc creditedTargetIds.size()>=N check above, this reads a REAL
    // independent secondary Objective group (StageDescriptor::
    // secondaryEscapeUnitsAlternative, see settlementCommonWellStage()'s own
    // comment) - same "scan mission.definitions for this group id, check its
    // progress is Completed" pattern surveySucceeded uses above, just for a
    // non-survey group id. Grants a Discovery (not Loot) instead, same as
    // quarry_old_mine's own analogous block.
    if (!isReconnaissanceRun_ && stage.id == "settlement_common_well" &&
        stage.secondaryEscapeUnitsAlternative) {
        const BattleMissionState& mission = battleController_->battle().missionState();
        bool allEvacuated = false;
        for (const ObjectiveDefinition& def : mission.definitions) {
            if (def.groupId == stage.secondaryEscapeUnitsAlternative->id &&
                mission.progress.at(def.id).status == ObjectiveStatus::Completed) {
                allEvacuated = true;
                break;
            }
        }
        if (allEvacuated &&
            std::find(expedition_.pendingDiscoveries.begin(), expedition_.pendingDiscoveries.end(),
                     kSettlementCommunalTestimonyDiscovery) == expedition_.pendingDiscoveries.end())
            expedition_.pendingDiscoveries.push_back(kSettlementCommunalTestimonyDiscovery);
    }

    // docs/regions/windscar_plateau.md「6. 高原伝令所」の副目標「高原運び手を
    // 2人以上撤退・降伏させる」: reuses the existing generic enemy AI retreat
    // path (jf/battle/AiSystem.hpp's AiProfile::retreatHpPercent, already
    // ~30% for Human-derived profiles - see EnemyAI.cpp's takeEnemyTurn(),
    // which sets exitReason=Retreated on that path) rather than any new
    // per-faction retreat threshold override, same ad-hoc-secondary-bonus
    // pattern as blackwater_crossing/deep_mire above (no RewardRule::
    // Condition reads a retreat count).
    // docs/regions/old_frontier_settlement.md「5. 夜明けの共同防衛」の「全住民
    // 避難: 織物2」: same "scan mission.definitions for this group id, check
    // Completed" pattern as settlement_common_well's kSettlementCommunalTestimony
    // Discovery block above, but granting Loot instead of a Discovery (this
    // site's secondaryEscapeUnitsAlternative id differs from settlement_common_
    // well's own).
    // NOTE: `mergeLoot()` only affects `loot` (merged into
    // expedition_.pendingLoot at the single insert point above, near
    // computeStageVictoryLoot() - already executed by this point in the
    // function) - both blocks below push directly onto
    // expedition_.pendingLoot instead, same as every Discovery-granting
    // ad-hoc block below already does with pendingDiscoveries.
    if (!isReconnaissanceRun_ && stage.id == "settlement_dawn_defense" && stage.secondaryEscapeUnitsAlternative) {
        const BattleMissionState& mission = battleController_->battle().missionState();
        for (const ObjectiveDefinition& def : mission.definitions) {
            if (def.groupId == stage.secondaryEscapeUnitsAlternative->id &&
                mission.progress.at(def.id).status == ObjectiveStatus::Completed) {
                expedition_.pendingLoot.push_back({"cloth", 2});
                break;
            }
        }
    }
    // docs/regions/old_frontier_settlement.md「5. 夜明けの共同防衛」の「頭領撤退:
    // 鉄材1」: same generic enemy AI retreat path as windscar_plateau's own
    // "高原運び手を2人以上撤退" bonus (plateau_relay block below) - here it's a
    // single named unit (`settlement_dawn_raid_leader`) rather than a count
    // threshold, since this site has exactly one RaidLeader.
    if (!isReconnaissanceRun_ && stage.id == "settlement_dawn_defense") {
        const Unit* leader = battleController_->battle().findUnit("settlement_dawn_raid_leader");
        if (leader != nullptr && leader->exitReason == UnitExitReason::Retreated)
            expedition_.pendingLoot.push_back({"iron", 1});
    }

    // docs/regions/buried_dawn_sanctum.md「6. 夜明け祭壇」の公開副目標「団長を
    // 降伏させる: 聖堂器材+1。撃破は不要」: same single-named-unit generic
    // enemy AI retreat pattern as settlement_dawn_defense's own RaidLeader
    // bonus above (`AiSystem.cpp`'s `retreatHpPercent=25` for
    // `SanctumRetrievalLeader` sets `exitReason=Retreated` on that path).
    if (!isReconnaissanceRun_ && stage.id == "dawn_altar") {
        const Unit* leader = battleController_->battle().findUnit("dawn_altar_leader");
        if (leader != nullptr && leader->exitReason == UnitExitReason::Retreated)
            expedition_.pendingLoot.push_back({"sanctum_equipment", 1});
    }

    if (!isReconnaissanceRun_ && stage.id == "plateau_relay") {
        int retreatedEnemies = 0;
        for (const Unit& unit : battleController_->battle().units())
            if (unit.team == Team::Enemy && unit.exitReason == UnitExitReason::Retreated) ++retreatedEnemies;
        if (retreatedEnemies >= 2 && !baseState_.discoveryRegistry.count(kWindscarRoadChartDiscovery) &&
            std::find(expedition_.pendingDiscoveries.begin(), expedition_.pendingDiscoveries.end(),
                     kWindscarRoadChartDiscovery) == expedition_.pendingDiscoveries.end())
            expedition_.pendingDiscoveries.push_back(kWindscarRoadChartDiscovery);
    }

    if (!isReconnaissanceRun_) {
        std::size_t stageIndex = static_cast<std::size_t>(expedition_.stageIndex);
        if (stageIndex < stageDiscoveryAwarded_.size() && !stageDiscoveryAwarded_[stageIndex]) {
            for (const DiscoveryId& discovery : computeStageDiscoveries(stage, lastExplorationChoice_))
                expedition_.pendingDiscoveries.push_back(discovery);
            stageDiscoveryAwarded_[stageIndex] = true;
        }
        // docs/regions/blackwater_lowlands.md「7. 深泥の水源」の副目標「毒状態の
        // 味方0で戦闘終了」: grants marsh_emergency_medicine early if this run's
        // party ends the battle with nobody poisoned and it isn't already
        // registered/pending - same ad-hoc-secondary-bonus pattern as above.
        // Harmless if herb_islet's own ScoutRoute already granted it earlier in
        // the same expedition (discoveryRegistry/pendingDiscoveries dedupe).
        if (stage.id == "deep_mire" &&
            std::none_of(battleController_->battle().units().begin(), battleController_->battle().units().end(),
                        [](const Unit& unit) { return unit.team == Team::Player && unit.poisonRemainingProcs > 0; }) &&
            !baseState_.discoveryRegistry.count(kMarshEmergencyMedicineDiscovery) &&
            std::find(expedition_.pendingDiscoveries.begin(), expedition_.pendingDiscoveries.end(),
                     kMarshEmergencyMedicineDiscovery) == expedition_.pendingDiscoveries.end())
            expedition_.pendingDiscoveries.push_back(kMarshEmergencyMedicineDiscovery);
        // docs/regions/ember_ravine.md「8. 赤熱裂け目」の副目標「味方の炎上状態
        // 0で終了」→ 制御燃焼式: same ad-hoc-secondary-bonus pattern as
        // deep_mire's own "毒状態の味方0" -> kMarshEmergencyMedicineDiscovery
        // check directly above, scanning burnRemainingProcs instead of
        // poisonRemainingProcs.
        if (stage.id == "redheat_fissure" &&
            std::none_of(battleController_->battle().units().begin(), battleController_->battle().units().end(),
                        [](const Unit& unit) { return unit.team == Team::Player && unit.burnRemainingProcs > 0; }) &&
            !baseState_.discoveryRegistry.count(kControlledEmberFormulaDiscovery) &&
            std::find(expedition_.pendingDiscoveries.begin(), expedition_.pendingDiscoveries.end(),
                     kControlledEmberFormulaDiscovery) == expedition_.pendingDiscoveries.end())
            expedition_.pendingDiscoveries.push_back(kControlledEmberFormulaDiscovery);
        // docs/roster_design.md「加入段階」: 灰角大猪撃破(brokenwood_territory
        // 勝利)で重装兵の加入候補を記録する。安全帰還まではPending
        // (ExpeditionState::pendingRecruitCandidateIds)、敗北で失う。
        if (stage.id == "brokenwood_territory") expedition_.pendingRecruitCandidateIds.insert("heavy_recruit");
        // docs/regions/cinderwatch_gate.md「報酬と加入」: the real trigger is
        // the escort NPC surviving (工作兵生存/伝令兵脱出), which needs a
        // temporary player-controlled/protected-ally subsystem that doesn't
        // exist (M6-B/C's own deferred scope). Approximated here as the site's
        // ordinary victory, same simplification pattern as brokenwood_territory
        // above.
        if (stage.id == "ironwatch_stores") expedition_.pendingRecruitCandidateIds.insert("engineer_recruit");
        if (stage.id == "old_barracks") expedition_.pendingRecruitCandidateIds.insert("cavalry_recruit");
        SiteAccessState achieved = surveySucceeded ? SiteAccessState::Secured : SiteAccessState::Surveyed;
        queueExpeditionSiteAccessPromotion(expedition_, siteAccessKey(expedition_.regionId, stage.id), achieved,
                                           baseState_);

        // docs/regions/ashbough_forest.md "地域進行": once every site is
        // Surveyed+ (灰枝の林縁勝利、薬草の沢勝利、灰角大猪撃破), the win that
        // completes the last one queues the region's completion Discovery -
        // committed alongside completedRegionIds on the safe return that
        // follows (docs/region_mission_data_contract.md "地域完了判定").
        if (!baseState_.completedRegionIds.count(expedition_.regionId) &&
            !expedition_.pendingRegionCompletions.count(expedition_.regionId) &&
            computeWouldRegionBeCleared(expedition_.regionId, expedition_, baseState_, data_)) {
            expedition_.pendingRegionCompletions.insert(expedition_.regionId);
            if (expedition_.regionId == RegionId::AshboughForest) {
                expedition_.pendingDiscoveries.push_back(kAshboughForestSurveyCompleteDiscovery);
                // docs/roster_design.md「加入タイミング」: 辺境猟兵の加入候補条件は
                // 「森の踏査記録(=このDiscoveryそのもの)、安全帰還」- 地域完了と
                // 同時にPendingへ記録する(banner_recruitのCinderwatch完了時と同型)。
                expedition_.pendingRecruitCandidateIds.insert("ranger_recruit");
            }
        }
    }
    expedition_.battlesWon += 1;
    if (expedition_.routeProgress)
        expedition_.routeProgress->resolvedNodeIds.insert(expedition_.routeProgress->currentNodeId);
    syncPartySnapshotFromBattle();
    justSecuredLoot_ = false;
    screen_ = Screen::Camp;
    updateExpeditionCheckpoint(ExpeditionCheckpoint::Stage::Camp);
}

void GameApp::acknowledgeDefeat() {
    if (battleController_->inputState() != BattleInputState::Defeat) return;
    lastSecuredLoot_.clear();
    resetToBase();
}

bool GameApp::retireExpedition() {
    if (screen_ == Screen::Base) return false;
    lastSecuredLoot_.clear();
    resetToBase();
    return true;
}

void GameApp::continueExpedition() {
    if (screen_ != Screen::Camp || expeditionComplete()) return;
    syncPartySnapshotFromBattle();
    if (expedition_.routeProgress) {
        if (!advanceRouteToNextSite()) return;
        screen_ = Screen::Exploration;
        updateExpeditionCheckpoint(ExpeditionCheckpoint::Stage::Exploration);
        return;
    }
    std::vector<Unit> survivors = battleController_->battle().units();
    ++expedition_.stageIndex;
    battleController_ = std::make_unique<BattleController>(
        createScenarioContinuationBattle(data_, survivors, currentStage(), expeditionSeed_));
    applyEquipmentTraits(*battleController_);
    applyEquippedSkills(*battleController_);
    applyArmorBonus(*battleController_);
    // docs/character_progression.md「連携作戦」: squad-wide, so it's copied
    // onto the fresh BattleState at every real-battle entry point, same as
    // applyEquipmentTraits()/applyEquippedSkills() above.
    battleController_->battle().setEquippedCooperationId(equippedCooperationId_);
    screen_ = Screen::Battle;
}

bool GameApp::useBattleHealingItem(ItemType item) {
    if (screen_ != Screen::Battle || item == ItemType::FirstAidKit || expedition_.count(item) <= 0) return false;
    if (!battleController_->useHealingItem(healingAmount(item))) return false;
    return expedition_.consume(item);
}

bool GameApp::chooseNeutralBattleHealingItem(ItemType item) {
    if (screen_ != Screen::Battle || item == ItemType::FirstAidKit || expedition_.count(item) <= 0 ||
        healingAmount(item) <= 0) return false;
    if (!battleController_->chooseHealingItemTarget(healingAmount(item))) return false;
    pendingBattleItem_ = item;
    return true;
}

bool GameApp::selectNeutralBattleHealingTarget(GridPos pos) {
    if (!pendingBattleItem_ || !battleController_->selectHealingItemTarget(pos)) return false;
    const ItemType item = *pendingBattleItem_;
    pendingBattleItem_.reset();
    return expedition_.consume(item);
}

bool GameApp::chooseProtectiveBoard() {
    if (screen_ != Screen::Battle || expedition_.count(ItemType::ProtectiveBoard) <= 0) return false;
    battleController_->chooseProtectiveBoard();
    return battleController_->inputState() == BattleInputState::SelectBoardTarget;
}

bool GameApp::selectBoardTarget(GridPos pos) {
    if (!battleController_->selectBoardTarget(pos)) return false;
    return expedition_.consume(ItemType::ProtectiveBoard);
}

bool GameApp::useCampItem(ItemType item, const std::string& unitId) {
    if (screen_ != Screen::Camp || expedition_.count(item) <= 0) return false;
    auto& units = battleController_->battle().units();
    if (item == ItemType::CampRations) {
        bool changed = false;
        for (Unit& unit : units) {
            if (unit.team == Team::Player && unit.isAlive() && unit.currentHp < unit.stats.maxHp) {
                unit.currentHp = std::min(unit.currentHp + 5, unit.stats.maxHp);
                changed = true;
            }
        }
        if (!changed || !expedition_.consume(item)) return false;
        updateExpeditionCheckpoint(ExpeditionCheckpoint::Stage::Camp);
        return true;
    }
    if (item == ItemType::ReturnFlare) {
        return returnToBase();
    }
    Unit* target = battleController_->battle().findUnit(unitId);
    if (!target || target->team != Team::Player) return false;
    if (item == ItemType::RescuePack) {
        if (target->isAlive()) return false;
        target->currentHp = std::max(1, target->stats.maxHp / 4);
    } else {
        int amount = healingAmount(item);
        if (!target->isAlive() || target->currentHp >= target->stats.maxHp || amount <= 0) return false;
        target->currentHp = std::min(target->currentHp + amount, target->stats.maxHp);
    }
    if (!expedition_.consume(item)) return false;
    updateExpeditionCheckpoint(ExpeditionCheckpoint::Stage::Camp);
    return true;
}

std::string GameApp::currentMissionName() const {
    return currentStage().missionNameEn;
}

std::string GameApp::currentMissionNameJa() const {
    return currentStage().missionNameJa;
}

bool GameApp::expeditionComplete() const { return computeExpeditionComplete(expedition_, data_); }

std::optional<std::string> GameApp::nextMissionNameJa() const {
    return computeNextMissionNameJa(expedition_, data_);
}

std::optional<std::vector<std::string>> GameApp::nextSiteEnemyRosterNames() const {
    return computeNextSiteEnemyRosterNames(expedition_, data_, expeditionPartyUnits_);
}

void GameApp::syncPartySnapshotFromBattle() {
    if ((screen_ != Screen::Battle && screen_ != Screen::Camp) || !battleController_) return;
    std::vector<Unit> party;
    for (const Unit& unit : battleController_->battle().units())
        if (unit.team == Team::Player) party.push_back(unit);
    if (!party.empty()) expeditionPartyUnits_ = std::move(party);
}

bool GameApp::advanceRouteToNextSite() {
    return advanceExpeditionRouteToNextSite(expedition_, baseState_, data_);
}

bool GameApp::returnToBase() {
    if (screen_ != Screen::Camp) return false;
    ReturnToBaseResult result = applyExpeditionReturnToBase(expedition_, baseState_, returnGrantSequence_);
    if (!result.success) return false;
    justSecuredLoot_ = true;
    lastSecuredLoot_ = std::move(result.securedLootIds);
    resetToBase();
    justSecuredLoot_ = true;
    markPersistentStateChanged();
    return true;
}

bool GameApp::discardStorage(const LootId& id, int quantity) {
    if (quantity <= 0) return false;
    if (baseState_.materialStorageCap(id) == BaseState::kKeyMaterialStorageCap) return false;
    if (!baseState_.consumeStorage(id, quantity)) return false;
    markPersistentStateChanged();
    return true;
}

bool GameApp::discardItemStorage(ItemType type, int quantity) {
    if (quantity <= 0) return false;
    if (!baseState_.consumeItemStorage(type, quantity)) return false;
    markPersistentStateChanged();
    return true;
}

bool GameApp::discardOverflowStack(std::size_t index, int quantity) {
    auto& stacks = baseState_.rewardOverflow.stacks;
    if (index >= stacks.size() || quantity <= 0 || stacks[index].quantity < quantity) return false;
    stacks[index].quantity -= quantity;
    if (stacks[index].quantity == 0) stacks.erase(stacks.begin() + static_cast<std::ptrdiff_t>(index));
    markPersistentStateChanged();
    return true;
}

void GameApp::acknowledgeLootSecured() {
    justSecuredLoot_ = false;
}

bool GameApp::togglePartyMember(const std::string& unitId) {
    if (screen_ != Screen::Base) return false;
    auto it = std::find(selectedPartyIds_.begin(), selectedPartyIds_.end(), unitId);
    if (it != selectedPartyIds_.end()) {
        selectedPartyIds_.erase(it);
        markPersistentStateChanged();
        return true;
    }
    if (selectedPartyIds_.size() >= 4) return false;
    if (std::none_of(roster_.begin(), roster_.end(), [&](const auto& unit) { return unit.id == unitId; })) return false;
    selectedPartyIds_.push_back(unitId);
    markPersistentStateChanged();
    return true;
}

bool GameApp::craftItem(ItemType type) {
    if (screen_ != Screen::Base) return false;
    if (baseState_.ownedItemCount(type) >= BaseState::kItemStorageCap) return false;
    const std::vector<ItemCraftCost> cost = itemCraftCost(type);
    for (const ItemCraftCost& line : cost)
        if (baseState_.storageCount(line.materialId) < line.quantity) return false;
    for (const ItemCraftCost& line : cost) baseState_.consumeStorage(line.materialId, line.quantity);
    baseState_.addItemStorage(type, 1);
    markPersistentStateChanged();
    return true;
}

bool GameApp::addPreparedItem(ItemType item) {
    if (screen_ != Screen::Base || preparedBag_.size() >= ExpeditionState::kBagCapacity) return false;
    if (!baseState_.consumeItemStorage(item, 1)) return false;
    preparedBag_.push_back(item);
    markPersistentStateChanged();
    return true;
}

void GameApp::removePreparedItem(std::size_t index) {
    if (screen_ != Screen::Base || index >= preparedBag_.size()) return;
    baseState_.addItemStorage(preparedBag_[index], 1);
    preparedBag_.erase(preparedBag_.begin() + index);
    markPersistentStateChanged();
}

std::vector<GameApp::RegionSummary> GameApp::regionSummaries() const {
    return computeRegionSummaries(data_, baseState_);
}

bool GameApp::startExpedition(RegionId regionId) {
    if (screen_ != Screen::Base || selectedPartyIds_.size() != 4 || !isRegionUnlocked(regionId)) return false;
    activeExpeditionData_ = data_;
    // M10-A (docs/deep_layers.md「1Lvあたりの数値」: "武器はLvごとに攻撃力
    // +1"): applied once here (rather than per-instantiateUnit call) since
    // every player-side Unit for this expedition is built from
    // activeExpeditionData_.weaponsById (base weapon lookup and weaponOverrides_
    // resolution both go through it) - a single mutation up front covers the
    // whole run, including continuation battles that reuse expeditionPartyUnits_.
    for (const auto& [weaponId, level] : baseState_.weaponLevels) {
        auto weaponIt = activeExpeditionData_.weaponsById.find(weaponId);
        if (weaponIt != activeExpeditionData_.weaponsById.end()) weaponIt->second.might += (level - 1);
    }
    activeExpeditionData_.playerParty.clear();
    for (const std::string& id : selectedPartyIds_) {
        auto it = std::find_if(roster_.begin(), roster_.end(), [&](const auto& unit) { return unit.id == id; });
        if (it != roster_.end()) activeExpeditionData_.playerParty.push_back(*it);
    }
    if (activeExpeditionData_.playerParty.size() != 4) return false;
    expedition_ = ExpeditionState{};
    expedition_.regionId = regionId;
    if (usesRouteGraph(regionId)) expedition_.routeProgress = initialRouteProgress(regionId);
    expedition_.bag = preparedBag_;
    expeditionSeed_ = makeExpeditionSeed();
    isReconnaissanceRun_ = false;
    stageDiscoveryAwarded_.assign(regionDescriptor(regionId, data_).stages.size(), false);
    expeditionPartyUnits_.clear();
    for (const UnitTemplate& unitTemplate : activeExpeditionData_.playerParty)
        expeditionPartyUnits_.push_back(
            instantiateUnit(activeExpeditionData_, unitTemplate, Team::Player, GridPos{0, 0}, &weaponOverrides_));
    screen_ = Screen::Exploration;
    justSecuredLoot_ = false;
    updateExpeditionCheckpoint(ExpeditionCheckpoint::Stage::Exploration);
    return true;
}

bool GameApp::partyHasFrontierScout() const {
    for (const UnitTemplate& unit : activeExpeditionData_.playerParty) {
        if (unit.classId == UnitClass::FrontierScout) return true;
    }
    return false;
}

bool GameApp::partyHasClass(UnitClass unitClass) const {
    for (const UnitTemplate& unit : activeExpeditionData_.playerParty) {
        if (unit.classId == unitClass) return true;
    }
    return false;
}

SiteAccessState GameApp::currentSiteAccess() const {
    return computeCurrentSiteAccess(expedition_, baseState_, data_);
}

bool GameApp::chooseSafePassage() {
    if (screen_ != Screen::Exploration || currentSiteAccess() != SiteAccessState::Secured) return false;
    const StageDescriptor stage = currentStage();
    lastExplorationChoice_ = ExplorationChoice::FrontalAdvance;
    isReconnaissanceRun_ = false;
    if (expeditionPartyUnits_.empty()) {
        // No prior battle exists this run yet - a fresh, full-HP party is
        // correct here (this is the expedition's starting state, not a
        // free heal).
        battleController_ = std::make_unique<BattleController>(createScenarioBattle(
            activeExpeditionData_, stage, expeditionSeed_, cinderwatchOutcome(ExplorationChoice::FrontalAdvance),
            nullptr, &weaponOverrides_));
    } else {
        // Preserve the party's current HP/status exactly like
        // continueExpedition() does - safe passage must not silently heal
        // the party for free once a region has more than one stage.
        battleController_ = std::make_unique<BattleController>(
            createScenarioContinuationBattle(activeExpeditionData_, expeditionPartyUnits_, stage, expeditionSeed_));
    }
    applyEquipmentTraits(*battleController_);
    applyEquippedSkills(*battleController_);
    applyArmorBonus(*battleController_);
    // docs/character_progression.md「連携作戦」: squad-wide, so it's copied
    // onto the fresh BattleState at every real-battle entry point, same as
    // applyEquipmentTraits()/applyEquippedSkills() above.
    battleController_->battle().setEquippedCooperationId(equippedCooperationId_);
    // No battle fought - straight to Camp with no loot/discoveries (docs/
    // regions/ashbough_forest.md: "狼戦と探索3択を省略。報酬なし").
    if (expedition_.routeProgress) {
        expedition_.routeProgress->resolvedNodeIds.insert(expedition_.routeProgress->currentNodeId);
        expedition_.routeProgress->safelyPassedNodeIds.insert(expedition_.routeProgress->currentNodeId);
    }
    justSecuredLoot_ = false;
    screen_ = Screen::Camp;
    updateExpeditionCheckpoint(ExpeditionCheckpoint::Stage::Camp);
    return true;
}

bool GameApp::chooseReconnaissance() {
    if (screen_ != Screen::Exploration || currentSiteAccess() != SiteAccessState::Secured) return false;
    const StageDescriptor stage = currentStage();
    lastExplorationChoice_ = ExplorationChoice::FrontalAdvance;
    isReconnaissanceRun_ = true;
    if (expeditionPartyUnits_.empty()) {
        battleController_ = std::make_unique<BattleController>(createScenarioBattle(
            activeExpeditionData_, stage, expeditionSeed_, cinderwatchOutcome(ExplorationChoice::FrontalAdvance),
            nullptr, &weaponOverrides_));
    } else {
        battleController_ = std::make_unique<BattleController>(
            createScenarioContinuationBattle(activeExpeditionData_, expeditionPartyUnits_, stage, expeditionSeed_));
    }
    applyEquipmentTraits(*battleController_);
    applyEquippedSkills(*battleController_);
    applyArmorBonus(*battleController_);
    // docs/character_progression.md「連携作戦」: squad-wide, so it's copied
    // onto the fresh BattleState at every real-battle entry point, same as
    // applyEquipmentTraits()/applyEquippedSkills() above.
    battleController_->battle().setEquippedCooperationId(equippedCooperationId_);
    screen_ = Screen::Battle;
    return true;
}

int GameApp::bulkPassSecuredSites() {
    if (screen_ != Screen::Exploration || !expedition_.routeProgress) return 0;
    int passed = bulkAdvanceSecuredSites(expedition_, baseState_, data_);
    if (passed == 0) return 0;

    // Same "no battle fought, just need a party-state container for the
    // stopping screen" rebuild as chooseSafePassage(), done once at the end
    // rather than per intermediate site. currentNodeId is always a real
    // Site here (see above), so currentStage() is always valid.
    const StageDescriptor stage = currentStage();
    if (expeditionPartyUnits_.empty()) {
        battleController_ = std::make_unique<BattleController>(createScenarioBattle(
            activeExpeditionData_, stage, expeditionSeed_, cinderwatchOutcome(ExplorationChoice::FrontalAdvance),
            nullptr, &weaponOverrides_));
    } else {
        battleController_ = std::make_unique<BattleController>(
            createScenarioContinuationBattle(activeExpeditionData_, expeditionPartyUnits_, stage, expeditionSeed_));
    }
    applyEquipmentTraits(*battleController_);
    applyEquippedSkills(*battleController_);
    applyArmorBonus(*battleController_);
    // docs/character_progression.md「連携作戦」: squad-wide, so it's copied
    // onto the fresh BattleState at every real-battle entry point, same as
    // applyEquipmentTraits()/applyEquippedSkills() above.
    battleController_->battle().setEquippedCooperationId(equippedCooperationId_);
    lastExplorationChoice_ = ExplorationChoice::FrontalAdvance;
    isReconnaissanceRun_ = false;
    justSecuredLoot_ = false;

    if (expeditionComplete()) {
        screen_ = Screen::Camp;
        updateExpeditionCheckpoint(ExpeditionCheckpoint::Stage::Camp);
    } else {
        screen_ = Screen::Exploration;
        updateExpeditionCheckpoint(ExpeditionCheckpoint::Stage::Exploration);
    }
    return passed;
}

bool GameApp::chooseExplorationRoute(ExplorationChoice choice) {
    if (screen_ != Screen::Exploration || !currentStage().contentImplemented) return false;
    if (!expedition_.routeProgress && expedition_.stageIndex != 0) return false;
    const StageDescriptor stage = currentStage();
    if (choice == ExplorationChoice::ScoutRoute && stage.scoutRouteDisabled) return false;
    if (choice == ExplorationChoice::ScoutRoute &&
        !partyHasClass(stage.scoutRouteRequiredClass.value_or(UnitClass::FrontierScout)))
        return false;
    if (currentSiteAccess() == SiteAccessState::Secured) return false;

    isReconnaissanceRun_ = false;
    lastExplorationChoice_ = choice;
    ExplorationOutcome outcome = stageRouteOutcome(stage, choice);
    if (outcome.enableFreeDeployment) {
        deploymentOutcome_ = outcome;
        deploymentTerrain_ = generateFieldTerrain(activeExpeditionData_.terrainProfile(stage.terrainProfileId),
                                                  expeditionSeed_);
        deploymentPlayers_.clear();
        for (const Unit& snapshot : expeditionPartyUnits_) {
            if (snapshot.team != Team::Player || !snapshot.isAlive()) continue;
            Unit unit = snapshot;
            unit.position = {0, 0};
            deploymentPlayers_.push_back(std::move(unit));
        }
        deploymentPlaced_.assign(deploymentPlayers_.size(), false);
        deploymentEnemyPreview_ = previewEnemies(activeExpeditionData_, stage, expeditionSeed_, outcome,
                                                 static_cast<int>(deploymentPlayers_.size()));
        for (const Unit& enemy : deploymentEnemyPreview_) {
            const int key = enemy.position.row * kGridCols + enemy.position.col;
            if (!isPassable(deploymentTerrain_[key])) deploymentTerrain_[key] = TerrainType::Floor;
        }
        screen_ = Screen::PreBattleDeployment;
        return true;
    }

    battleController_ = std::make_unique<BattleController>(createScenarioContinuationBattle(
        activeExpeditionData_, expeditionPartyUnits_, stage, expeditionSeed_, outcome));
    applyEquipmentTraits(*battleController_);
    applyEquippedSkills(*battleController_);
    applyArmorBonus(*battleController_);
    // docs/character_progression.md「連携作戦」: squad-wide, so it's copied
    // onto the fresh BattleState at every real-battle entry point, same as
    // applyEquipmentTraits()/applyEquippedSkills() above.
    battleController_->battle().setEquippedCooperationId(equippedCooperationId_);
    screen_ = Screen::Battle;
    return true;
}

bool GameApp::placeDeploymentUnit(std::size_t partyIndex, GridPos pos) {
    if (screen_ != Screen::PreBattleDeployment || partyIndex >= deploymentPlayers_.size()) return false;
    if (pos.row < 0 || pos.row >= kGridRows) return false;
    if (pos.col < 0 || pos.col > deploymentOutcome_.deploymentMaxColumn) return false;
    for (std::size_t i = 0; i < deploymentPlayers_.size(); ++i) {
        if (i != partyIndex && deploymentPlaced_[i] && deploymentPlayers_[i].position == pos) return false;
    }
    deploymentPlayers_[partyIndex].position = pos;
    deploymentPlaced_[partyIndex] = true;
    return true;
}

bool GameApp::allDeploymentUnitsPlaced() const {
    if (deploymentPlaced_.empty()) return false;
    return std::all_of(deploymentPlaced_.begin(), deploymentPlaced_.end(), [](bool placed) { return placed; });
}

bool GameApp::isDeploymentUnitPlaced(std::size_t partyIndex) const {
    return partyIndex < deploymentPlaced_.size() && deploymentPlaced_[partyIndex];
}

bool GameApp::confirmDeployment() {
    if (screen_ != Screen::PreBattleDeployment || !allDeploymentUnitsPlaced()) return false;
    std::vector<GridPos> positions;
    for (const Unit& unit : deploymentPlayers_) positions.push_back(unit.position);
    battleController_ = std::make_unique<BattleController>(createScenarioContinuationBattle(
        activeExpeditionData_, expeditionPartyUnits_, currentStage(), expeditionSeed_, deploymentOutcome_, &positions));
    applyEquipmentTraits(*battleController_);
    applyEquippedSkills(*battleController_);
    applyArmorBonus(*battleController_);
    // docs/character_progression.md「連携作戦」: squad-wide, so it's copied
    // onto the fresh BattleState at every real-battle entry point, same as
    // applyEquipmentTraits()/applyEquippedSkills() above.
    battleController_->battle().setEquippedCooperationId(equippedCooperationId_);
    screen_ = Screen::Battle;
    deploymentPlayers_.clear();
    deploymentPlaced_.clear();
    return true;
}

void GameApp::cancelDeployment() {
    if (screen_ != Screen::PreBattleDeployment) return;
    deploymentPlayers_.clear();
    deploymentPlaced_.clear();
    screen_ = Screen::Exploration;
}

bool GameApp::advanceOutpostStage() {
    if (screen_ != Screen::Base) return false;
    auto next = static_cast<OutpostStage>(static_cast<int>(baseState_.outpostStage) + 1);
    if (!eligibleForOutpostStage(baseState_, next)) return false;
    baseState_.outpostStage = next;
    markPersistentStateChanged();
    return true;
}

void GameApp::applyEquipmentTraits(BattleController& controller) {
    for (Unit& unit : controller.battle().units()) {
        if (unit.team != Team::Player) continue;
        auto it = equippedTraits_.find(unit.id);
        unit.knockbackNegatesRemaining =
            (it != equippedTraits_.end() && it->second == TuningTraitId::HideWrappedGrip) ? 1 : 0;
    }
}

void GameApp::applyArmorBonus(BattleController& controller) {
    for (Unit& unit : controller.battle().units()) {
        if (unit.team != Team::Player) continue;
        auto overrideIt = armorOverrides_.find(unit.id);
        if (overrideIt != armorOverrides_.end()) {
            if (const ArmorDefinition* armor = findArmorDefinition(overrideIt->second)) {
                int level = baseState_.armorLevel(armor->id);
                unit.stats.defense += armorDefBonusAtLevel(*armor, level);
                unit.stats.resistance += armorResBonusAtLevel(*armor, level);
            }
        }
        auto traitIt = equippedArmorTraits_.find(unit.id);
        unit.firstStatusNegatesRemaining =
            (traitIt != equippedArmorTraits_.end() && traitIt->second == ArmorTuningTraitId::WardStep) ? 1 : 0;
    }
}

void GameApp::applyEquippedSkills(BattleController& controller) {
    for (Unit& unit : controller.battle().units()) {
        if (unit.team != Team::Player) continue;
        auto it = equippedSkills_.find(unit.id);
        for (std::size_t slot = 0; slot < unit.skillSlots.size(); ++slot) {
            unit.skillSlots[slot].skillId =
                it != equippedSkills_.end() ? it->second.equippedSkillIds[slot] : std::string{};
        }
        // BattleController's constructor already ran initializeSkillCharges()
        // against these slots while they were still empty; re-run it now
        // that the real equipped ids are in place.
        initializeSkillCharges(unit);
    }
}

bool GameApp::equipSkillForUnit(const std::string& unitId, int slotIndex, const std::string& skillId) {
    if (screen_ != Screen::Base || slotIndex < 0 || slotIndex > 1) return false;
    auto unit = std::find_if(roster_.begin(), roster_.end(),
                              [&](const UnitTemplate& candidate) { return candidate.id == unitId; });
    if (unit == roster_.end()) return false;

    if (skillId.empty()) {
        equippedSkills_[unitId].equippedSkillIds[static_cast<std::size_t>(slotIndex)].clear();
        markPersistentStateChanged();
        return true;
    }

    const SkillDefinition* definition = findSkill(skillId);
    if (!definition || definition->unitClass != unit->classId) return false;
    std::string requiredNode = requiredTrainingNodeIdFor(unit->classId);
    if (requiredNode.empty() || !baseState_.unlockedNodeIds.count(requiredNode)) return false;

    equippedSkills_[unitId].equippedSkillIds[static_cast<std::size_t>(slotIndex)] = skillId;
    markPersistentStateChanged();
    return true;
}

bool GameApp::equipCooperation(const std::string& cooperationId) {
    if (screen_ != Screen::Base) return false;
    if (cooperationId.empty()) {
        equippedCooperationId_.clear();
        markPersistentStateChanged();
        return true;
    }
    const CooperationDefinition* definition = findCooperationDefinition(cooperationId);
    if (!definition || !isCooperationUnlocked(cooperationId, baseState_)) return false;
    equippedCooperationId_ = cooperationId;
    markPersistentStateChanged();
    return true;
}

bool GameApp::confirmRecruitJoin(const std::string& candidateId) {
    if (screen_ != Screen::Base) return false;
    if (!baseState_.joinReadyCandidateIds.count(candidateId)) return false;
    if (baseState_.joinedRecruitIds.count(candidateId)) return false; // already joined - idempotent no-op
    if (static_cast<int>(roster_.size()) >= recruitCapacity()) return false; // 枠不足: 候補は消さない

    const UnitTemplate* recruit = data_.recruitDefinition(candidateId);
    if (!recruit) return false;

    // docs/roster_design.md「兵種加入時の付与」手順1-5: Unit IDをRosterへ追加
    // (基本武器はUnit生成時にclassDefinition().weaponIdから解決されるため
    // UnitTemplate自体には持たせない)、Tier1スキルを第1枠へ装備、第2枠は空のまま。
    roster_.push_back(*recruit);
    for (const SkillDefinition* skill : skillsForClass(recruit->classId)) {
        if (skill->unlockTier == 1) {
            equippedSkills_[candidateId].equippedSkillIds[0] = skill->id;
            break;
        }
    }
    baseState_.joinedRecruitIds.insert(candidateId);
    markPersistentStateChanged();
    return true;
}

bool GameApp::unlockFacilityNode(const std::string& nodeId) {
    if (screen_ != Screen::Base) return false;
    const FacilityNode* node = findFacilityNode(nodeId);
    if (!node || !facilityNodeEligible(baseState_, *node)) return false;

    // Eligibility checks every cost first, so this consumption cannot leave a
    // partially-paid node when one of several materials is missing.
    for (const LootStack& cost : node->materialCosts) {
        if (!baseState_.consumeStorage(cost.id, cost.quantity)) return false;
    }
    baseState_.unlockedNodeIds.insert(nodeId);
    if (node->occupiesFacilitySlot) baseState_.constructedFacilityIds.insert(nodeId);
    markPersistentStateChanged();
    return true;
}

bool GameApp::equipWeaponForUnit(const std::string& unitId, const std::string& weaponId) {
    if (screen_ != Screen::Base) return false;
    if (!baseState_.constructedFacilityIds.count("simple_forge")) return false;
    auto unit = std::find_if(roster_.begin(), roster_.end(), [&](const UnitTemplate& candidate) {
        return candidate.id == unitId;
    });
    if (unit == roster_.end()) return false;
    if (weaponId.empty()) {
        weaponOverrides_.erase(unitId);
        markPersistentStateChanged();
        return true;
    }
    if (data_.weaponsById.find(weaponId) == data_.weaponsById.end()) return false;
    // Weapon-branch generalization to all 12 classes (docs/implementation_
    // roadmap.md "M7項目3(残り) ...特性・武器分岐の他兵種一般化"): the
    // recipe->weapon link is data-driven via FacilityNode::weaponBranchClass
    // instead of a Spearman-only lookup table. A "craft_*" node's id is
    // always "craft_" + its weapon id by construction.
    const std::string baseWeaponId = data_.classDefinition(unit->classId).weaponId;
    if (weaponId != baseWeaponId) {
        const FacilityNode* recipe = findFacilityNode("craft_" + weaponId);
        if (recipe == nullptr || recipe->weaponBranchClass != unit->classId ||
            !baseState_.unlockedNodeIds.count(recipe->id)) {
            return false;
        }
        // docs/item_system.md「武器と特性の共有」: a crafted branch weapon is a single
        // shared-warehouse copy - reject if another unit already has it equipped.
        // The class's base weapon is exempt (every unit is issued their own copy at
        // join, per docs/roster_design.md「加入人物には対応する基本武器を1本支給する」).
        for (const auto& [otherUnitId, otherWeaponId] : weaponOverrides_) {
            if (otherUnitId != unitId && otherWeaponId == weaponId) return false;
        }
    }
    weaponOverrides_[unitId] = weaponId;
    markPersistentStateChanged();
    return true;
}

bool GameApp::equipTuningTraitForUnit(const std::string& unitId, TuningTraitId traitId) {
    if (screen_ != Screen::Base) return false;
    if (!baseState_.constructedFacilityIds.count("simple_forge")) return false;
    auto unit = std::find_if(roster_.begin(), roster_.end(), [&](const UnitTemplate& candidate) {
        return candidate.id == unitId;
    });
    if (unit == roster_.end() || unit->classId != UnitClass::Spearman) return false;
    if (traitId == TuningTraitId::None) {
        equippedTraits_.erase(unitId);
        markPersistentStateChanged();
        return true;
    }
    if (traitId != TuningTraitId::HideWrappedGrip) return false;
    if (!baseState_.unlockedNodeIds.count("trait_hide_wrapped_grip")) return false;
    equippedTraits_[unitId] = traitId;
    markPersistentStateChanged();
    return true;
}

bool GameApp::strengthenWeapon(const std::string& weaponId) {
    if (screen_ != Screen::Base) return false;
    if (!baseState_.constructedFacilityIds.count("simple_forge")) return false;
    // A Lv is meaningless for a weapon nobody has crafted yet - the class
    // base weapon (no "craft_" node) isn't Lv-eligible in this Slice either
    // (see jf::weaponLevelEligibleWeapons()'s own doc comment).
    if (!baseState_.unlockedNodeIds.count("craft_" + weaponId)) return false;
    int currentLevel = baseState_.weaponLevel(weaponId);
    if (currentLevel >= BaseState::kMaxWeaponLevel) return false;
    std::vector<LootStack> cost = weaponLevelUpCost(weaponId, currentLevel + 1);
    if (cost.empty()) return false; // Lv6+ or a not-yet-wired weapon id
    for (const LootStack& stack : cost) {
        if (baseState_.storageCount(stack.id) < stack.quantity) return false;
    }
    for (const LootStack& stack : cost) baseState_.consumeStorage(stack.id, stack.quantity);
    baseState_.weaponLevels[weaponId] = currentLevel + 1;
    markPersistentStateChanged();
    return true;
}

bool GameApp::equipArmorForUnit(const std::string& unitId, const std::string& armorId) {
    if (screen_ != Screen::Base) return false;
    if (!baseState_.constructedFacilityIds.count("simple_forge")) return false;
    auto unit = std::find_if(roster_.begin(), roster_.end(),
                             [&](const UnitTemplate& candidate) { return candidate.id == unitId; });
    if (unit == roster_.end()) return false;
    if (armorId.empty()) {
        armorOverrides_.erase(unitId);
        markPersistentStateChanged();
        return true;
    }
    const ArmorDefinition* armor = findArmorDefinition(armorId);
    if (!armor || armor->unitClass != unit->classId) return false;
    if (!baseState_.unlockedNodeIds.count("craft_" + armorId)) return false;
    // docs/deep_layers.md「スロットと基本ルール」: same shared-warehouse
    // "only one unit at a time" rule as equipWeaponForUnit() - armor has no
    // "base item" exemption (there is no default armor issued at join).
    for (const auto& [otherUnitId, otherArmorId] : armorOverrides_) {
        if (otherUnitId != unitId && otherArmorId == armorId) return false;
    }
    armorOverrides_[unitId] = armorId;
    markPersistentStateChanged();
    return true;
}

bool GameApp::strengthenArmor(const std::string& armorId) {
    if (screen_ != Screen::Base) return false;
    if (!baseState_.constructedFacilityIds.count("simple_forge")) return false;
    if (!baseState_.unlockedNodeIds.count("craft_" + armorId)) return false;
    const FacilityNode* recipe = findFacilityNode("craft_" + armorId);
    if (!recipe || recipe->materialCosts.empty()) return false;
    int currentLevel = baseState_.armorLevel(armorId);
    if (currentLevel >= BaseState::kMaxArmorLevel) return false;
    std::vector<LootStack> cost = armorLevelUpCost(armorId, recipe->materialCosts[0].id, currentLevel + 1);
    if (cost.empty()) return false; // Lv6+ or a not-yet-wired armor id
    for (const LootStack& stack : cost) {
        if (baseState_.storageCount(stack.id) < stack.quantity) return false;
    }
    for (const LootStack& stack : cost) baseState_.consumeStorage(stack.id, stack.quantity);
    baseState_.armorLevels[armorId] = currentLevel + 1;
    markPersistentStateChanged();
    return true;
}

bool GameApp::equipArmorTraitForUnit(const std::string& unitId, ArmorTuningTraitId traitId) {
    if (screen_ != Screen::Base) return false;
    if (!baseState_.constructedFacilityIds.count("simple_forge")) return false;
    auto unit = std::find_if(roster_.begin(), roster_.end(),
                             [&](const UnitTemplate& candidate) { return candidate.id == unitId; });
    if (unit == roster_.end()) return false;
    if (traitId == ArmorTuningTraitId::None) {
        equippedArmorTraits_.erase(unitId);
        markPersistentStateChanged();
        return true;
    }
    if (traitId != ArmorTuningTraitId::WardStep) return false;
    if (!baseState_.unlockedNodeIds.count("armor_trait_ward_step")) return false;
    equippedArmorTraits_[unitId] = traitId;
    markPersistentStateChanged();
    return true;
}

SaveData GameApp::createSaveData(const std::string& language) const {
    SaveData save;
    save.base = baseState_;
    save.selectedPartyIds = selectedPartyIds_;
    save.unitWeaponOverrides = weaponOverrides_;
    for (const auto& [unitId, traitId] : equippedTraits_) {
        std::string traitString = tuningTraitIdToString(traitId);
        if (!traitString.empty()) save.unitEquippedTraits[unitId] = traitString;
    }
    save.unitArmorOverrides = armorOverrides_;
    for (const auto& [unitId, traitId] : equippedArmorTraits_) {
        std::string traitString = armorTuningTraitIdToString(traitId);
        if (!traitString.empty()) save.unitEquippedArmorTraits[unitId] = traitString;
    }
    for (const auto& [unitId, loadout] : equippedSkills_) {
        if (!loadout.equippedSkillIds[0].empty()) save.unitEquippedSkillsSlot0[unitId] = loadout.equippedSkillIds[0];
        if (!loadout.equippedSkillIds[1].empty()) save.unitEquippedSkillsSlot1[unitId] = loadout.equippedSkillIds[1];
    }
    save.language = language;
    save.expedition = expeditionCheckpoint_;
    save.equippedCooperationId = equippedCooperationId_;
    return save;
}

bool GameApp::applySaveData(const SaveData& save) {
    if (screen_ != Screen::Base || save.schemaVersion < 1 || save.schemaVersion > kCurrentSaveSchemaVersion) return false;

    BaseState loadedBase = save.base;
    // docs/roster_design.md「兵種加入時の付与」: roster_ only ever starts as
    // data_.playerParty/reserveRoster (GameApp::GameApp()) - a recruit joined
    // via confirmRecruitJoin() during a previous session must be re-added here
    // before the roster-dependent validation below (loadedParty/weapons/
    // traits/skills) can see them. Unknown ids are skipped the same way
    // confirmRecruitJoin() refuses candidates with no data definition.
    for (const std::string& candidateId : loadedBase.joinedRecruitIds) {
        if (std::any_of(roster_.begin(), roster_.end(), [&](const UnitTemplate& u) { return u.id == candidateId; }))
            continue;
        if (const UnitTemplate* recruit = data_.recruitDefinition(candidateId)) roster_.push_back(*recruit);
    }
    // Defensive self-consistency only (docs/base_development.md: built
    // facilities never expire, so there's no capacity to violate) - a
    // constructedFacilityIds entry that isn't actually an occupiesFacilitySlot node, or
    // whose unlock record is missing, indicates corrupt/foreign save data.
    for (auto it = loadedBase.constructedFacilityIds.begin(); it != loadedBase.constructedFacilityIds.end();) {
        const FacilityNode* node = findFacilityNode(*it);
        if (!node || !node->occupiesFacilitySlot || !loadedBase.unlockedNodeIds.count(*it))
            it = loadedBase.constructedFacilityIds.erase(it);
        else ++it;
    }

    std::vector<std::string> loadedParty;
    for (const std::string& id : save.selectedPartyIds) {
        bool exists = std::any_of(roster_.begin(), roster_.end(), [&](const UnitTemplate& unit) { return unit.id == id; });
        if (exists && std::find(loadedParty.begin(), loadedParty.end(), id) == loadedParty.end()) loadedParty.push_back(id);
    }
    if (loadedParty.size() != 4) loadedParty = selectedPartyIds_;

    std::unordered_map<std::string, std::string> requestedWeapons = save.unitWeaponOverrides;
    if (save.schemaVersion == 1) {
        for (const auto& [unitClass, weaponId] : save.weaponOverrides)
            for (const UnitTemplate& unit : roster_)
                if (unit.classId == unitClass) requestedWeapons[unit.id] = weaponId;
    }
    std::unordered_map<std::string, std::string> loadedWeapons;
    {
        // docs/item_system.md「武器と特性の共有」: a crafted branch weapon is a
        // single shared-warehouse copy - a Save (hand-edited or from an older
        // schema) claiming it for 2+ units must only restore it to one of them.
        // Sort by unitId first so the winner is deterministic rather than
        // depending on unordered_map's iteration order.
        std::vector<std::pair<std::string, std::string>> sortedRequests(requestedWeapons.begin(),
                                                                         requestedWeapons.end());
        std::sort(sortedRequests.begin(), sortedRequests.end());
        std::unordered_set<std::string> claimedBranchWeapons;
        const std::unordered_map<std::string, std::string> recipes = {
            {"long_spear", "craft_long_spear"}, {"heavy_spear", "craft_heavy_spear"},
            {"guard_spear", "craft_guard_spear"},
        };
        for (const auto& [unitId, weaponId] : sortedRequests) {
            auto unit = std::find_if(roster_.begin(), roster_.end(), [&](const UnitTemplate& candidate) {
                return candidate.id == unitId;
            });
            if (unit == roster_.end() || unit->classId != UnitClass::Spearman || !data_.weaponsById.count(weaponId))
                continue;
            if (weaponId == "iron_spear") {
                loadedWeapons[unitId] = weaponId;
                continue;
            }
            auto recipe = recipes.find(weaponId);
            if (recipe == recipes.end() || !loadedBase.unlockedNodeIds.count(recipe->second)) continue;
            if (!claimedBranchWeapons.insert(weaponId).second) continue; // already claimed by an earlier unitId
            loadedWeapons[unitId] = weaponId;
        }
    }
    std::unordered_map<std::string, std::string> requestedTraits = save.unitEquippedTraits;
    if (save.schemaVersion == 1) {
        for (const auto& [unitClass, traitString] : save.equippedTraits)
            for (const UnitTemplate& unit : roster_)
                if (unit.classId == unitClass) requestedTraits[unit.id] = traitString;
    }
    std::unordered_map<std::string, TuningTraitId> loadedTraits;
    for (const auto& [unitId, traitString] : requestedTraits) {
        TuningTraitId traitId = tuningTraitIdFromString(traitString);
        auto unit = std::find_if(roster_.begin(), roster_.end(), [&](const UnitTemplate& candidate) {
            return candidate.id == unitId;
        });
        if (unit != roster_.end() && unit->classId == UnitClass::Spearman &&
            traitId == TuningTraitId::HideWrappedGrip &&
            loadedBase.unlockedNodeIds.count("trait_hide_wrapped_grip"))
            loadedTraits[unitId] = traitId;
    }

    // M10-B: armor overrides, mirroring the weapon dedup shape above but
    // data-driven off jf::armorRegistry()/unit->classId instead of a
    // hardcoded per-class recipe table (armor has no legacy schemaVersion==1
    // form to migrate - it's new in this Slice).
    std::unordered_map<std::string, std::string> loadedArmors;
    {
        std::vector<std::pair<std::string, std::string>> sortedRequests(save.unitArmorOverrides.begin(),
                                                                         save.unitArmorOverrides.end());
        std::sort(sortedRequests.begin(), sortedRequests.end());
        std::unordered_set<std::string> claimedArmors;
        for (const auto& [unitId, armorId] : sortedRequests) {
            auto unit = std::find_if(roster_.begin(), roster_.end(), [&](const UnitTemplate& candidate) {
                return candidate.id == unitId;
            });
            if (unit == roster_.end()) continue;
            const ArmorDefinition* armor = findArmorDefinition(armorId);
            if (!armor || armor->unitClass != unit->classId) continue;
            if (!loadedBase.unlockedNodeIds.count("craft_" + armorId)) continue;
            if (!claimedArmors.insert(armorId).second) continue; // already claimed by an earlier unitId
            loadedArmors[unitId] = armorId;
        }
    }
    std::unordered_map<std::string, ArmorTuningTraitId> loadedArmorTraits;
    for (const auto& [unitId, traitString] : save.unitEquippedArmorTraits) {
        ArmorTuningTraitId traitId = armorTuningTraitIdFromString(traitString);
        auto unit = std::find_if(roster_.begin(), roster_.end(), [&](const UnitTemplate& candidate) {
            return candidate.id == unitId;
        });
        if (unit != roster_.end() && traitId == ArmorTuningTraitId::WardStep &&
            loadedBase.unlockedNodeIds.count("armor_trait_ward_step"))
            loadedArmorTraits[unitId] = traitId;
    }

    std::unordered_map<std::string, UnitSkillLoadout> loadedSkills;
    auto loadSkillSlot = [&](const std::unordered_map<std::string, std::string>& requested, std::size_t slotIndex) {
        for (const auto& [unitId, skillId] : requested) {
            if (skillId.empty()) continue;
            auto unit = std::find_if(roster_.begin(), roster_.end(),
                                     [&](const UnitTemplate& candidate) { return candidate.id == unitId; });
            if (unit == roster_.end()) continue;
            const SkillDefinition* definition = findSkill(skillId);
            if (!definition || definition->unitClass != unit->classId) continue;
            // docs/roster_design.md「兵種加入時の付与」: the Tier-1 skill granted
            // at join time (GameApp::confirmRecruitJoin()) is unconditional, not
            // gated by requiredTrainingNodeIdFor() - a reload must not drop it
            // just because the training facility isn't unlocked yet.
            bool isUnconditionalJoinGrant = definition->unlockTier == 1 && loadedBase.joinedRecruitIds.count(unitId);
            if (!isUnconditionalJoinGrant) {
                std::string requiredNode = requiredTrainingNodeIdFor(unit->classId);
                if (requiredNode.empty() || !loadedBase.unlockedNodeIds.count(requiredNode)) continue;
            }
            loadedSkills[unitId].equippedSkillIds[slotIndex] = skillId;
        }
    };
    loadSkillSlot(save.unitEquippedSkillsSlot0, 0);
    loadSkillSlot(save.unitEquippedSkillsSlot1, 1);

    // docs/character_progression.md「連携作戦」: re-validate against
    // loadedBase (not baseState_, still the pre-load value here) the same
    // way loadedTraits/loadedSkills above do - a save from before its region
    // was completed, or a hand-edited/foreign save, must not resurrect an
    // id that isn't actually unlocked.
    std::string loadedCooperationId;
    if (!save.equippedCooperationId.empty() && findCooperationDefinition(save.equippedCooperationId) &&
        isCooperationUnlocked(save.equippedCooperationId, loadedBase))
        loadedCooperationId = save.equippedCooperationId;

    baseState_ = std::move(loadedBase);
    selectedPartyIds_ = std::move(loadedParty);
    weaponOverrides_ = std::move(loadedWeapons);
    equippedTraits_ = std::move(loadedTraits);
    armorOverrides_ = std::move(loadedArmors);
    equippedArmorTraits_ = std::move(loadedArmorTraits);
    equippedSkills_ = std::move(loadedSkills);
    equippedCooperationId_ = std::move(loadedCooperationId);
    persistentRevision_ = 0;
    expeditionCheckpoint_.reset();
    expeditionRevision_ = 0;

    if (save.expedition && save.expedition->partyUnits.size() <= 4) {
        activeExpeditionData_ = data_;
        activeExpeditionData_.playerParty.clear();
        for (const std::string& id : selectedPartyIds_) {
            auto unit = std::find_if(roster_.begin(), roster_.end(), [&](const auto& candidate) { return candidate.id == id; });
            if (unit != roster_.end()) activeExpeditionData_.playerParty.push_back(*unit);
        }
        const ExpeditionCheckpoint& checkpoint = *save.expedition;
        RegionDescriptor checkpointRegion = regionDescriptor(checkpoint.regionId, data_);
        std::optional<RouteProgressSnapshot> restoredRoute = checkpoint.routeProgress;
        bool routeValid = true;
        if (usesRouteGraph(checkpoint.regionId)) {
            if (!restoredRoute) restoredRoute = initialRouteProgress(checkpoint.regionId);
            const RegionRouteGraph& graph = regionRouteGraph(checkpoint.regionId);
            const RouteNodeDefinition* node = findRouteNode(graph, restoredRoute->currentNodeId);
            routeValid = restoredRoute->routeId == graph.routeId && node && node->kind == RouteNodeKind::Site &&
                         node->stageId.has_value();
        } else if (restoredRoute) {
            routeValid = false;
        }
        const bool stageIndexValid =
            checkpoint.expeditionStage >= 0 &&
            static_cast<std::size_t>(checkpoint.expeditionStage) < checkpointRegion.stages.size();
        if (activeExpeditionData_.playerParty.size() == 4 && stageIndexValid && routeValid) {
            expedition_ = ExpeditionState{};
            expedition_.regionId = checkpoint.regionId;
            expedition_.pendingLoot = checkpoint.pendingLoot;
            expedition_.pendingDiscoveries = checkpoint.pendingDiscoveries;
            expedition_.bag = checkpoint.bag;
            expedition_.battlesWon = checkpoint.battlesWon;
            expedition_.stageIndex = checkpoint.expeditionStage;
            expedition_.routeProgress = std::move(restoredRoute);
            expedition_.pendingSiteAccessUpdates = checkpoint.pendingSiteAccessUpdates;
            expedition_.pendingRegionCompletions = checkpoint.pendingRegionCompletions;
            expedition_.pendingRecruitCandidateIds = checkpoint.pendingRecruitCandidateIds;
            expeditionSeed_ = checkpoint.seed;
            stageDiscoveryAwarded_ = checkpoint.stageDiscoveryAwarded;
            stageDiscoveryAwarded_.resize(checkpointRegion.stages.size(), false);
            justSecuredLoot_ = false;
            isReconnaissanceRun_ = false;

            expeditionPartyUnits_.clear();
            for (const UnitTemplate& unitTemplate : activeExpeditionData_.playerParty) {
                Unit unit = instantiateUnit(activeExpeditionData_, unitTemplate, Team::Player, GridPos{0, 0},
                                            &weaponOverrides_);
                auto snapshot = std::find_if(checkpoint.partyUnits.begin(), checkpoint.partyUnits.end(),
                                             [&](const auto& entry) { return entry.id == unit.id; });
                unit.currentHp = snapshot != checkpoint.partyUnits.end() ? snapshot->currentHp : unit.stats.maxHp;
                expeditionPartyUnits_.push_back(unit);
            }

            if (checkpoint.stage == ExpeditionCheckpoint::Stage::Camp) {
                battleController_ = std::make_unique<BattleController>(createScenarioContinuationBattle(
                    activeExpeditionData_, expeditionPartyUnits_, currentStage(), expeditionSeed_));
                applyEquipmentTraits(*battleController_);
                applyEquippedSkills(*battleController_);
                applyArmorBonus(*battleController_);
                screen_ = Screen::Camp;
            } else {
                screen_ = Screen::Exploration;
            }
            ExpeditionCheckpoint normalized = checkpoint;
            normalized.routeProgress = expedition_.routeProgress;
            normalized.partyUnits.clear();
            for (const Unit& unit : expeditionPartyUnits_)
                normalized.partyUnits.push_back({unit.id, unit.currentHp});
            expeditionCheckpoint_ = std::move(normalized);
        } else if (activeExpeditionData_.playerParty.size() == 4) {
            // docs/expedition_recovery.md "更新後の復旧" 優先順位4: the
            // region/party are fine, but the specific Node/Stage this
            // checkpoint pointed to isn't (e.g. after a content change) -
            // fall back to the region's entrance rather than silently
            // discarding the whole expedition. "入口退避ではHP、戦闘不能、
            // Bag、Pendingを巻き戻さない" - only the route/stage position
            // resets to the start; Pending rewards/discoveries/site-access/
            // bag and party HP all survive.
            expedition_ = ExpeditionState{};
            expedition_.regionId = checkpoint.regionId;
            expedition_.pendingLoot = checkpoint.pendingLoot;
            expedition_.pendingDiscoveries = checkpoint.pendingDiscoveries;
            expedition_.bag = checkpoint.bag;
            expedition_.pendingSiteAccessUpdates = checkpoint.pendingSiteAccessUpdates;
            expedition_.pendingRegionCompletions = checkpoint.pendingRegionCompletions;
            expedition_.pendingRecruitCandidateIds = checkpoint.pendingRecruitCandidateIds;
            if (usesRouteGraph(checkpoint.regionId))
                expedition_.routeProgress = initialRouteProgress(checkpoint.regionId);
            expeditionSeed_ = checkpoint.seed;
            stageDiscoveryAwarded_.assign(checkpointRegion.stages.size(), false);
            justSecuredLoot_ = false;
            isReconnaissanceRun_ = false;

            expeditionPartyUnits_.clear();
            for (const UnitTemplate& unitTemplate : activeExpeditionData_.playerParty) {
                Unit unit = instantiateUnit(activeExpeditionData_, unitTemplate, Team::Player, GridPos{0, 0},
                                            &weaponOverrides_);
                auto snapshot = std::find_if(checkpoint.partyUnits.begin(), checkpoint.partyUnits.end(),
                                             [&](const auto& entry) { return entry.id == unit.id; });
                unit.currentHp = snapshot != checkpoint.partyUnits.end() ? snapshot->currentHp : unit.stats.maxHp;
                expeditionPartyUnits_.push_back(unit);
            }
            screen_ = Screen::Exploration;
            updateExpeditionCheckpoint(ExpeditionCheckpoint::Stage::Exploration);
        }
    }
    return true;
}

void GameApp::resetToBase() {
    // docs/item_system.md "未使用消耗品は帰還・敗北のどちらでも倉庫へ戻る。
    // 使用済み消耗品は敗北しても戻らない。" - expedition_.bag only ever holds
    // what hasn't been consumed yet (useCampItem/useBattleHealingItem/etc.
    // remove consumed items via ExpeditionState::consume()), so whatever
    // remains here at the end of ANY exit path (safe return, defeat, or a
    // harsh retireExpedition()) goes back to owned storage unconditionally.
    for (ItemType item : expedition_.bag) baseState_.addItemStorage(item, 1);
    expedition_ = ExpeditionState{};
    stageDiscoveryAwarded_ = {};
    expeditionPartyUnits_.clear();
    preparedBag_.clear();
    expeditionSeed_ = makeExpeditionSeed();
    battleController_ = std::make_unique<BattleController>(
        createScenarioBattle(data_, idlePlaceholderStage(data_), expeditionSeed_));
    screen_ = Screen::Base;
    expeditionCheckpoint_.reset();
    ++expeditionRevision_;
}

void GameApp::updateExpeditionCheckpoint(ExpeditionCheckpoint::Stage stage) {
    if (stage == ExpeditionCheckpoint::Stage::Camp) syncPartySnapshotFromBattle();
    expeditionCheckpoint_ =
        buildExpeditionCheckpoint(stage, expedition_, expeditionSeed_, stageDiscoveryAwarded_, expeditionPartyUnits_);
    ++expeditionRevision_;
}

} // namespace jf
