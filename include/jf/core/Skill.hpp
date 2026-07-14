#pragma once

#include <array>
#include <string>
#include <vector>

#include "jf/core/UnitClass.hpp"

namespace jf {

// docs/skill_system.md "スキル分類". Exploration abilities (斥候の偵察など) are
// not part of this - they attach to the class directly and never consume an
// equip slot, per the doc.
enum class SkillCategory {
    Active,   // 能動: chosen from the battle "スキル" command
    Passive,  // 受動: applies automatically once its condition holds
    Reactive  // 反応: auto-triggers off an enemy action
};

// docs/skill_system.md "使用制限". Recharge timing only - see
// jf/battle/SkillCharges.hpp for the actual bookkeeping.
enum class SkillUsageType {
    PerTurn,       // 毎ターン: 1 use, refills at the user's own next Phase start
    OncePerBattle, // 戦闘1回: 1 use for the whole battle, refills only at battle start
    Cooldown2,     // クールダウン2ターン: after use, needs 2 of the user's own Phase starts
    OncePerPhase,  // 1フェーズ1回: 1 use, refills at the user's own next Phase start
    Always         // 常時: passive, never consumed
};

// One equippable skill's metadata (docs/skill_system.md's per-class tables).
// This is descriptive data only, mirroring FacilityNode's effectEn/effectJa
// convention - it does not attach any executable battle effect. Only the 6
// shipped classes' skills are registered; the doc's "後半6兵種" classes
// (Heavy Guard, Engineer, Courier, Ranger, Standard-Bearer, Battle Mage)
// have no UnitClass value yet, so their skills aren't representable here.
struct SkillDefinition {
    std::string id;
    UnitClass unitClass{};
    std::string nameEn;
    std::string nameJa;
    SkillCategory category = SkillCategory::Active;
    SkillUsageType usageType = SkillUsageType::Always;
    std::string effectEn;
    std::string effectJa;
    // 1 = unlocked as soon as the class is available, 2/3 = later unlock
    // tiers (docs/skill_system.md "初期スキルの解放順"). The doc leaves the
    // exact Discovery IDs gating tiers 2/3 unspecified, so eligibility here
    // only checks the class's training branch, not a specific Discovery -
    // see requiredTrainingNodeIdFor() and the note in docs/skill_system.md.
    int unlockTier = 1;
};

const std::vector<SkillDefinition>& skillRegistry();
const SkillDefinition* findSkill(const std::string& id);
std::vector<const SkillDefinition*> skillsForClass(UnitClass unitClass);

// The docs/base_development.md training-ground branch node that must be
// built before any of this class's equip-skill tiers can be equipped
// (docs/skill_system.md "対応分岐"). Returns an empty string for a class
// with no mapped branch yet.
std::string requiredTrainingNodeIdFor(UnitClass unitClass);

// Persistent per-unit equip loadout (docs/skill_system.md "保存データ" -
// the `equippedSkills` half of that doc's UnitLoadout struct; weapon and
// tuning-trait persistence already exist separately as
// GameApp::weaponOverrides_/equippedTraits_).
struct UnitSkillLoadout {
    std::array<std::string, 2> equippedSkillIds{};
};

} // namespace jf
