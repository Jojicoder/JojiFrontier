#include "jf/core/Skill.hpp"

namespace jf {

const std::vector<SkillDefinition>& skillRegistry() {
    static const std::vector<SkillDefinition> skills = {
        // 行軍隊長 (March Captain)
        {"hold_formation", UnitClass::MarchCaptain, "Hold Formation", "隊形維持", SkillCategory::Active,
         SkillUsageType::Cooldown2, "Self and adjacent allies DEF+2 until the next Enemy Phase ends.",
         "自身と隣接する味方のDEF+2。次のEnemy Phase終了まで。", 1},
        {"advance_order", UnitClass::MarchCaptain, "Advance Order", "前進命令", SkillCategory::Active,
         SkillUsageType::OncePerBattle, "An adjacent unacted ally's MOV+1 until this Player Phase ends.",
         "隣接する未行動の味方1人のMOV+1。このPlayer Phase終了まで。", 2},
        {"support_order", UnitClass::MarchCaptain, "Support Order", "援護命令", SkillCategory::Reactive,
         SkillUsageType::OncePerPhase, "An adjacent ally takes 3 less damage from an attack.",
         "隣接味方が攻撃で受けるダメージを3軽減。", 3},

        // 古参守備兵 (Veteran Guard)
        {"provoke", UnitClass::VeteranGuard, "Provoke", "挑発", SkillCategory::Active, SkillUsageType::Cooldown2,
         "One enemy is provoked: next Enemy Phase it prioritizes attacking the user if able.",
         "敵1体へ挑発を付与。次のEnemy Phase、使用者を攻撃可能なら最優先する。", 1},
        {"extended_lockdown", UnitClass::VeteranGuard, "Extended Lockdown", "封鎖強化", SkillCategory::Active,
         SkillUsageType::OncePerBattle, "Zone of Control extends to Manhattan distance 2 until the next Enemy Phase ends.",
         "次のEnemy Phase終了までZone of Controlをマンハッタン距離2へ拡張。", 2},
        {"immovable_stance", UnitClass::VeteranGuard, "Immovable Stance", "不動の構え", SkillCategory::Passive,
         SkillUsageType::Always, "Triggers only on confirming Wait: DEF+3 until the user's next action ends, no movement on that next action.",
         "待機確定時のみ発動。次の自分の行動終了までDEF+3、次の自分の行動では移動不可。", 3},

        // 監視弓兵 (Watch Archer)
        {"suppressing_shot", UnitClass::WatchArcher, "Suppressing Shot", "制圧射撃", SkillCategory::Active,
         SkillUsageType::Cooldown2, "A normal attack; a hit also applies Move Down.",
         "通常攻撃を行い、命中した敵へ移動低下を付与。", 1},
        {"overwatch", UnitClass::WatchArcher, "Overwatch", "警戒射撃", SkillCategory::Active,
         SkillUsageType::OncePerBattle, "Next Enemy Phase, attacks the first enemy to enter the equipped weapon's range.",
         "次のEnemy Phase、最初に装備武器の射程へ入った敵を1回攻撃。", 2},
        {"mark_target", UnitClass::WatchArcher, "Mark Target", "標的指定", SkillCategory::Active,
         SkillUsageType::Cooldown2, "Marks one enemy: the next ally attack against it deals +2 damage, then the mark clears.",
         "敵1体へ標的を付与。次に味方から受ける攻撃ダメージ+2、その攻撃後に解除。", 3},

        // 辺境斥候 (Frontier Scout)
        {"trailblaze", UnitClass::FrontierScout, "Trailblaze", "道拓き", SkillCategory::Active, SkillUsageType::Cooldown2,
         "Moves up to normal MOV; Ash/Shallows tiles crossed cost allies 1 movement for this Player Phase.",
         "最大通常MOVで移動する。通過した灰地・浅瀬はそのPlayer Phase中、味方の移動コスト1。", 1},
        {"ambush", UnitClass::FrontierScout, "Ambush", "奇襲", SkillCategory::Active, SkillUsageType::OncePerBattle,
         "A normal attack (weapon range) against an enemy that hasn't acted this round, dealing +3 damage.",
         "そのラウンドで未行動の敵へ通常攻撃し、与ダメージ+3。", 2},
        {"emergency_withdrawal", UnitClass::FrontierScout, "Emergency Withdrawal", "緊急離脱", SkillCategory::Active,
         SkillUsageType::Cooldown2, "Moves up to 3 tiles without attacking; may start from a tile adjacent to an enemy.",
         "攻撃せず最大3マス移動。敵隣接マスから移動を開始可能。", 3},

        // 槍兵 (Spearman)
        {"spear_wall", UnitClass::Spearman, "Spear Wall", "槍壁", SkillCategory::Active, SkillUsageType::Cooldown2,
         "Self and one adjacent ally get DEF+2 against attackers who moved 2+ tiles, until the next Enemy Phase ends.",
         "自身と隣接味方1人へ、2マス以上移動した敵から攻撃される際のDEF+2を付与。次のEnemy Phase終了まで。", 1},
        {"halting_thrust", UnitClass::Spearman, "Halting Thrust", "足止め突き", SkillCategory::Active,
         SkillUsageType::Cooldown2, "A normal attack (weapon range); a hit also applies Move Down.",
         "通常攻撃を行い、命中した敵へ移動低下を付与。", 2},
        {"counterthrust", UnitClass::Spearman, "Counterthrust", "反撃準備", SkillCategory::Reactive,
         SkillUsageType::OncePerBattle, "Survive an enemy's attack while it's in weapon range: counter it immediately once.",
         "敵の攻撃を受けて生存し、その敵が武器射程内なら即座に1回反撃。", 3},

        // 暁の衛生兵 (Dawn Chirurgeon) - basic Heal stays an innate ability
        // outside these 2 equip slots, per docs/skill_system.md.
        {"cleanse", UnitClass::DawnChirurgeon, "Cleanse", "状態治療", SkillCategory::Active, SkillUsageType::Cooldown2,
         "Clears every status effect from self or one adjacent ally.",
         "自身または隣接味方1人の状態異常をすべて解除。", 1},
        {"protective_treatment", UnitClass::DawnChirurgeon, "Protective Treatment", "守護処置", SkillCategory::Active,
         SkillUsageType::Cooldown2, "Self or one adjacent ally gets RES+3 until the next Enemy Phase ends.",
         "自身または隣接味方1人のRES+3。次のEnemy Phase終了まで。", 2},
        {"emergency_treatment", UnitClass::DawnChirurgeon, "Emergency Treatment", "緊急処置", SkillCategory::Active,
         SkillUsageType::OncePerBattle, "Heals one ally at or below 50% max HP for 12.",
         "現在HPが最大HPの50%以下の味方1人を12回復。", 3},
    };
    return skills;
}

const SkillDefinition* findSkill(const std::string& id) {
    for (const SkillDefinition& skill : skillRegistry()) {
        if (skill.id == id) return &skill;
    }
    return nullptr;
}

std::vector<const SkillDefinition*> skillsForClass(UnitClass unitClass) {
    std::vector<const SkillDefinition*> result;
    for (const SkillDefinition& skill : skillRegistry()) {
        if (skill.unitClass == unitClass) result.push_back(&skill);
    }
    return result;
}

std::string requiredTrainingNodeIdFor(UnitClass unitClass) {
    switch (unitClass) {
        case UnitClass::VeteranGuard:
        case UnitClass::Spearman:
            return "vanguard_training";
        case UnitClass::WatchArcher:
        case UnitClass::FrontierScout:
            return "mobility_training";
        case UnitClass::MarchCaptain:
        case UnitClass::DawnChirurgeon:
            return "specialist_training";
        case UnitClass::Bandit:
        case UnitClass::Wolf:
        case UnitClass::AshenhornBoar:
            return "";
    }
    return "";
}

} // namespace jf
