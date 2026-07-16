#pragma once

#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "jf/core/Grid.hpp"
#include "jf/core/UnitClass.hpp"

namespace jf {

// docs/battle_objects.md: board elements that are neither Unit nor Terrain -
// fallen trees, survey markers, devices, containers, spawn/exit points.
// M1-C "Battle Object最小実装" scope: Marker (踏査地点), Barrier (倒木), and
// ExitPoint's data model + occupancy/passability/durability rules. Devices,
// Containers, and SpawnPoints are declared for completeness (per the doc's
// full enum) but have no resolver logic yet - that's later Slices' work
// (reinforcements, region-specific devices).
enum class BattleObjectKind {
    Marker,
    Barrier,
    Device,
    Container,
    SpawnPoint,
    ExitPoint,
};

enum class BattleObjectTeam { Neutral, Player, Enemy };
enum class BattleObjectStateKind { Active, Disabled, Opened, Destroyed };

using BattleObjectId = std::string;
using BattleObjectDefinitionId = std::string;

// docs/battle_objects.md "操作": Interact is a separate command from
// Attack that ends the actor's turn. Basic range is 1 (adjacent); a Marker
// may instead require ending the move on the same tile (range 0).
struct ObjectInteractionDefinition {
    std::string interactionId;
    int range = 1;
    std::unordered_set<UnitClass> allowedClasses; // empty == any class
    BattleObjectStateKind requiredState = BattleObjectStateKind::Active;
    int maxUses = 1;
};

// Immutable for the life of the battle - only BattleObjectState changes.
struct BattleObjectDefinition {
    BattleObjectDefinitionId definitionId;
    BattleObjectKind kind = BattleObjectKind::Marker;
    int maxDurability = 0;
    int defense = 0;
    int resistance = 0;
    bool canOccupy = false;
    bool blocksMovement = false;
    bool blocksStopping = false;
    bool blocksDeployment = false;
    bool blocksProjectiles = false;
    bool canBeAttacked = false;
    bool canBeRepaired = false;
    std::unordered_set<std::string> tags;
    // Battle Object統合(Interact配線): 未設定ならこのDefinitionはInteract不可
    // (踏査地点や倒木のように、専用の操作を持たないObjectの既定)。設定時は
    // resolveObjectInteraction()へそのまま渡し、成功したら遷移先として
    // interactionResultStateを使う。
    std::optional<ObjectInteractionDefinition> interaction;
    BattleObjectStateKind interactionResultState = BattleObjectStateKind::Opened;
};

struct BattleObjectState {
    BattleObjectId id;
    BattleObjectDefinitionId definitionId;
    GridPos position;
    BattleObjectTeam team = BattleObjectTeam::Neutral;
    BattleObjectStateKind state = BattleObjectStateKind::Active;
    int durability = 0;
    int interactionCount = 0;
};

// docs/battle_objects.md "Definition検証": a Definition is invalid if it
// combines occupancy with blocking movement (a Marker/SpawnPoint/Exit must
// stay steppable), claims durability-based attackability with zero
// durability, or claims repairability with zero durability. Appends one
// message per problem found; returns true iff none were found.
inline bool validateObjectDefinition(const BattleObjectDefinition& def, std::vector<std::string>* errors) {
    bool valid = true;
    auto fail = [&](const std::string& message) {
        valid = false;
        if (errors) errors->push_back(message);
    };
    if (def.canOccupy && def.blocksMovement) {
        fail("object definition '" + def.definitionId + "': canOccupy and blocksMovement can't both be true");
    }
    if (def.maxDurability == 0 && def.canBeAttacked) {
        fail("object definition '" + def.definitionId + "': maxDurability == 0 can't be canBeAttacked");
    }
    if (def.maxDurability == 0 && def.canBeRepaired) {
        fail("object definition '" + def.definitionId + "': maxDurability == 0 can't be canBeRepaired");
    }
    return valid;
}

} // namespace jf
