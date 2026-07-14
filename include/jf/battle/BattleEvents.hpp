#pragma once

#include <cstdint>
#include <string>
#include <variant>

#include "jf/battle/BattleObject.hpp"
#include "jf/battle/Phase.hpp"
#include "jf/core/Grid.hpp"
#include "jf/core/UnitClass.hpp"

namespace jf {

// docs/mission_objectives.md "戦闘イベントの実装契約" / docs/battle_objects.md
// "EventとObjective". TerrainCollision/ReinforcementResolved still wait on
// the reinforcement-wave model (docs: 増援), which isn't built yet.
using BattleEventId = std::uint64_t;
using ActionId = std::uint64_t;

// Only the values SecureTile's "攻撃、スキル、アイテム、待機のいずれかを確定"
// eligibility needs to distinguish today.
enum class ActionKind { Move, Attack, Skill, Item, Wait };

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
                                        RoundEndedEvent, ObjectStateChangedEvent, ObjectDestroyedEvent>;

struct BattleEvent {
    BattleEventId id;
    ActionId actionId; // 0 for non-action events (Phase/Round events)
    BattleEventPayload payload;
};

} // namespace jf
