#pragma once

#include <cstdint>
#include <string>
#include <variant>

#include "jf/battle/BattleObject.hpp"
#include "jf/battle/Phase.hpp"
#include "jf/battle/Reinforcement.hpp"
#include "jf/core/Grid.hpp"
#include "jf/core/UnitClass.hpp"
#include "jf/core/UnitExitReason.hpp"

namespace jf {

// docs/mission_objectives.md "戦闘イベントの実装契約" / docs/battle_objects.md
// "EventとObjective". TerrainCollision/ReinforcementResolved still wait on
// the reinforcement-wave model (docs: 増援), which isn't built yet.
using BattleEventId = std::uint64_t;
using ActionId = std::uint64_t;

// Only the values SecureTile's "攻撃、スキル、アイテム、待機のいずれかを確定"
// eligibility needs to distinguish today.
enum class ActionKind { Move, Attack, Skill, Item, Wait, Interact };

struct ActionResolvedEvent {
    ActionId actionId;
    std::string actorUnitId;
    Team actorTeam;
    ActionKind actionKind;
    GridPos endPosition;
};

struct UnitDefeatedEvent {
    std::string unitId;
    Team team;
    UnitExitReason exitReason = UnitExitReason::Defeated;
};

// docs/boss_common_rules.md "Phase移行": fired exactly once per real Boss
// stage transition (e.g. the Ashenhorn Boar's enrage at HP<=50%), never
// more than once for the same Action Batch even if multiple thresholds are
// crossed at once. `stageIndex` is 0 at battle start and increments once
// per transition - a boss with only one transition (today, every shipped
// boss) only ever reaches 1.
struct BossStageChangedEvent {
    std::string unitId;
    int stageIndex;
};

struct BossTelegraphChangedEvent {
    std::string unitId;
    std::string actionId;
    bool announced;
};

struct ReinforcementAnnouncedEvent {
    std::string waveId;
    int spawnRound;
};

struct ReinforcementResolvedEvent {
    std::string waveId;
    ReinforcementResult result;
};

struct PhaseStartedEvent {
    Phase phase;
    int round;
};

struct PhaseEndedEvent {
    Phase phase;
    int round;
};

struct RoundEndedEvent {
    int round;
};

// docs/battle_objects.md "操作": fired once per real state transition (not
// once per Interact attempt - a no-op interact, e.g. hitting maxUses,
// fires nothing).
struct ObjectStateChangedEvent {
    BattleObjectId objectId;
    BattleObjectStateKind newState;
};

// docs/battle_objects.md "耐久とダメージ": fired exactly once per Object,
// the moment its durability reaches 0.
struct ObjectDestroyedEvent {
    BattleObjectId objectId;
};

using BattleEventPayload = std::variant<ActionResolvedEvent, UnitDefeatedEvent, PhaseStartedEvent, PhaseEndedEvent,
                                        RoundEndedEvent, ObjectStateChangedEvent, ObjectDestroyedEvent,
                                        BossStageChangedEvent, BossTelegraphChangedEvent,
                                        ReinforcementAnnouncedEvent, ReinforcementResolvedEvent>;

struct BattleEvent {
    BattleEventId id;
    ActionId actionId; // 0 for non-action events (Phase/Round events)
    BattleEventPayload payload;
};

} // namespace jf
