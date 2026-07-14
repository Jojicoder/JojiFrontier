#include "jf/battle/SkillCharges.hpp"

namespace jf {

namespace {
int startingUses(SkillUsageType type) {
    switch (type) {
        case SkillUsageType::PerTurn:
        case SkillUsageType::OncePerBattle:
        case SkillUsageType::OncePerPhase:
            return 1;
        case SkillUsageType::Cooldown2:
        case SkillUsageType::Always:
            // Cooldown2 tracks readiness via cooldownPhasesRemaining instead;
            // Always is passive and never consumed.
            return 0;
    }
    return 0;
}
} // namespace

void initializeSkillCharges(Unit& unit) {
    for (SkillSlotState& slot : unit.skillSlots) {
        const SkillDefinition* def = slot.skillId.empty() ? nullptr : findSkill(slot.skillId);
        slot.usesRemaining = def ? startingUses(def->usageType) : 0;
        slot.cooldownPhasesRemaining = 0;
    }
}

void refreshSkillChargesOnPhaseStart(BattleState& battle, Team team) {
    for (Unit& unit : battle.units()) {
        if (unit.team != team) continue;
        for (SkillSlotState& slot : unit.skillSlots) {
            if (slot.skillId.empty()) continue;
            const SkillDefinition* def = findSkill(slot.skillId);
            if (!def) continue;
            switch (def->usageType) {
                case SkillUsageType::PerTurn:
                case SkillUsageType::OncePerPhase:
                    slot.usesRemaining = 1;
                    break;
                case SkillUsageType::Cooldown2:
                    if (slot.cooldownPhasesRemaining > 0) --slot.cooldownPhasesRemaining;
                    break;
                case SkillUsageType::OncePerBattle:
                case SkillUsageType::Always:
                    break;
            }
        }
    }
}

bool skillSlotAvailable(const Unit& unit, int slotIndex) {
    if (slotIndex < 0 || slotIndex >= static_cast<int>(unit.skillSlots.size())) return false;
    const SkillSlotState& slot = unit.skillSlots[slotIndex];
    if (slot.skillId.empty()) return false;
    const SkillDefinition* def = findSkill(slot.skillId);
    if (!def) return false;
    if (def->usageType == SkillUsageType::Always) return true;
    if (def->usageType == SkillUsageType::Cooldown2) return slot.cooldownPhasesRemaining <= 0;
    return slot.usesRemaining > 0;
}

bool consumeSkillCharge(Unit& unit, int slotIndex) {
    if (!skillSlotAvailable(unit, slotIndex)) return false;
    SkillSlotState& slot = unit.skillSlots[slotIndex];
    const SkillDefinition* def = findSkill(slot.skillId);
    if (def->usageType == SkillUsageType::Always) return true;
    if (def->usageType == SkillUsageType::Cooldown2) slot.cooldownPhasesRemaining = 2;
    else slot.usesRemaining = 0;
    return true;
}

std::vector<SkillAvailability> availableSkills(const Unit& unit) {
    std::vector<SkillAvailability> result;
    for (int i = 0; i < static_cast<int>(unit.skillSlots.size()); ++i) {
        const SkillSlotState& slot = unit.skillSlots[i];
        SkillAvailability entry;
        entry.slotIndex = i;
        entry.skillId = slot.skillId;
        if (slot.skillId.empty()) {
            result.push_back(entry);
            continue;
        }
        const SkillDefinition* def = findSkill(slot.skillId);
        if (!def) {
            entry.reasonEn = "Unknown skill";
            entry.reasonJa = "不明なスキル";
            result.push_back(entry);
            continue;
        }
        entry.available = skillSlotAvailable(unit, i);
        if (!entry.available) {
            if (def->usageType == SkillUsageType::Cooldown2) {
                entry.reasonEn = "Cooldown: " + std::to_string(slot.cooldownPhasesRemaining) + " Phase(s)";
                entry.reasonJa = "クールダウン: あと" + std::to_string(slot.cooldownPhasesRemaining) + "Phase";
            } else {
                entry.reasonEn = "No uses remaining this battle";
                entry.reasonJa = "この戦闘では使用済み";
            }
        }
        result.push_back(entry);
    }
    return result;
}

} // namespace jf
