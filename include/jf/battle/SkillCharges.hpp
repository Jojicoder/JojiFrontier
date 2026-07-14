#pragma once

#include <string>
#include <vector>

#include "jf/battle/BattleState.hpp"
#include "jf/core/Skill.hpp"

namespace jf {

// Sets both of `unit`'s skill slots to "fully available" for their usage
// type - call once per unit when a battle is created (fresh or
// continuation), after its equipped skill ids are already set on
// unit.skillSlots[i].skillId.
void initializeSkillCharges(Unit& unit);

// Call once at the start of `team`'s Phase (the natural counterpart to
// BattleState::beginPlayerPhase()/beginEnemyPhase()): refills PerTurn/
// OncePerPhase slots and ticks a Cooldown2 slot's remaining phases down by
// one. OncePerBattle only ever refills via initializeSkillCharges(), and
// Always is never consumed.
void refreshSkillChargesOnPhaseStart(BattleState& battle, Team team);

bool skillSlotAvailable(const Unit& unit, int slotIndex);

// Marks the slot used (docs/skill_system.md: "スキル結果が確定した時点で回数を
// 消費する" - call this only once an activated skill's effect actually
// resolves, never while the player is still choosing a target). No-op
// (returns false) if the slot is out of range, empty, or already
// unavailable.
bool consumeSkillCharge(Unit& unit, int slotIndex);

struct SkillAvailability {
    int slotIndex = 0;
    std::string skillId; // empty = no skill equipped in this slot
    bool available = false;
    std::string reasonEn; // only meaningful when !available
    std::string reasonJa;
};

// Building block for a future skill-menu UI (docs/skill_system.md: "使用不能
// スキルは非表示にせず、理由付きで無効表示") - always reports both slots,
// with a human-readable reason attached to whichever are currently
// unusable. Does not know about anything outside charge/cooldown state
// (e.g. "already acted this turn" is a BattleController-level concern).
std::vector<SkillAvailability> availableSkills(const Unit& unit);

} // namespace jf
