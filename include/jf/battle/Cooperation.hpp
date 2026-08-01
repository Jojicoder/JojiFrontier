#pragma once

#include <string>
#include <vector>

#include "jf/core/BaseState.hpp"

namespace jf {

// docs/character_progression.md「連携作戦」: the 6 documented pairs. Every id
// is registered here (including `paired_cross_observation`) so save data /
// UI enumeration always sees the full set, even though `hasBattleEffect`
// is false for the one pair whose effect is deliberately deferred (see its
// own comment below).
struct CooperationDefinition {
    std::string id;
    std::string unitAId;
    std::string unitBId;
    // Every pair has a real battle effect now - `paired_cross_observation`
    // used to be the one exception (deferred: exposing enemy-AI action
    // candidates had no existing UI hook, unlike 辺境猟兵`read_quarry`'s
    // purely-cosmetic quarryRevealed flag). It's implemented via a
    // throwaway BattleState clone instead of a dedicated prediction system -
    // see CooperationRevealResult's own comment in BattleState.hpp and
    // BattleController::selectCooperationTarget().
    bool hasBattleEffect = true;
};

const std::vector<CooperationDefinition>& cooperationDefinitions();
const CooperationDefinition* findCooperationDefinition(const std::string& id);

// docs/character_progression.md「解放条件」, approximated the same way as
// heavy_recruit/cavalry_recruit's own "加入候補確定" (see
// docs/implementation_status.md M7項目2): dropping the "対応する会話2件を
// 読み" clause entirely and gating purely on the named region's safe-return
// completion (BaseState::completedRegionIds). `paired_signal_ward`'s region
// (埋没聖堂/Buried Dawn Sanctum) has no RegionId/region implementation at all
// yet - unlike the other 4 pairs' regions, which all exist - so it always
// returns false here until that region ships (same "unreachable until
// region X ships" documented-gap pattern used elsewhere in this project).
bool isCooperationUnlocked(const std::string& id, const BaseState& base);

} // namespace jf
