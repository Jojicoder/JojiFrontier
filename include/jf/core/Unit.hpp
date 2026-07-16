#pragma once

#include <algorithm>
#include <array>
#include <string>

#include "jf/core/Grid.hpp"
#include "jf/core/Stats.hpp"
#include "jf/core/StatusEffect.hpp"
#include "jf/core/UnitClass.hpp"
#include "jf/core/UnitExitReason.hpp"
#include "jf/core/Weapon.hpp"
#include "jf/battle/BossRuntime.hpp"

namespace jf {

// docs/boss_common_rules.md "Bossの退場理由": meaningful once a unit stops
// being alive - only `Defeated` and `ScriptedWithdrawal` are ever set today
// (no shipped boss retreats, escapes, or surrenders yet). The other 3
// values exist so Objective/reward code introduced for a future boss can
// already assume this full set exists, rather than needing a breaking enum
// change later. A non-boss unit's defeat is always `Defeated`.
// Battle-scoped runtime state for one of a unit's 2 equipped-skill slots
// (docs/skill_system.md). `skillId` is set from the unit's persistent
// UnitSkillLoadout when a battle is created; empty means nothing equipped
// there. See jf/battle/SkillCharges.hpp for how these are initialized,
// refreshed, and consumed.
struct SkillSlotState {
    std::string skillId;
    int usesRemaining = 0;
    int cooldownPhasesRemaining = 0;
};

struct Unit {
    std::string id;
    std::string name;
    UnitClass unitClass = UnitClass::MarchCaptain;
    Team team = Team::Player;

    Stats stats;
    int currentHp = 1;
    Weapon weapon;

    GridPos position{};
    bool hasActed = false;
    int tilesMovedThisAction = 0;

    // Hide-Wrapped Grip tuning trait: negates this many knockbacks for the
    // rest of the battle (set once at battle start, decremented on use).
    int knockbackNegatesRemaining = 0;

    // Scales down status-effect magnitude/duration instead of granting full
    // immunity (docs/status_effects.md "ボス補正"). Set once when a boss
    // encounter is instantiated; no shipped content sets this yet.
    bool isBoss = false;

    // 状態異常 (docs/status_effects.md). Applied/ticked/cleared through
    // jf::applyPoison()/applyBurn()/.../processActionEndStatusEffects()/
    // processPhaseEndStatusEffects() (jf/battle/StatusEffects.hpp) rather
    // than mutated directly, so the reapplication/immunity rules stay in
    // one place. All of it is battle-scoped: never saved, always cleared at
    // battle end.
    int poisonRemainingProcs = 0;   // 0 = not poisoned
    int burnRemainingProcs = 0;     // 0 = not burning
    bool moveDownActive = false;    // MOV penalty until this unit's side's next phase ends
    bool defenseDownActive = false; // DEF penalty until this unit's side's next phase ends
    bool staggerActive = false;     // no movement on this unit's next action
    bool staggerImmune = false;     // cannot be re-staggered until this unit's side's next phase ends
    // 暁の衛生兵`protective_treatment` (docs/initial_skill_effects.md): RES+3
    // until the next Enemy Phase ends. See jf/battle/StatusEffects.hpp's
    // applyResistanceUp()/clearSkillBuffsAtEnemyPhaseEnd() for why this
    // clears differently from the debuff flags above.
    bool resistanceUpActive = false;
    // 行軍隊長`hold_formation` (docs/initial_skill_effects.md): DEF+2 until
    // the next Enemy Phase ends, same clearing timing as resistanceUpActive
    // above (see clearSkillBuffsAtEnemyPhaseEnd()).
    bool defenseUpActive = false;
    // 古参守備兵`extended_lockdown` (docs/initial_skill_effects.md): extends
    // this unit's own Zone of Control from range 1 to range 2 until the next
    // Enemy Phase ends - same clearing timing as the two buffs above. Only
    // meaningful on a unit with hasZoneOfControl(unitClass) already true.
    bool zocRangeExtended = false;
    // 槍兵`spear_wall` (docs/initial_skill_effects.md): grants the same
    // conditional DEF+2 as the Spearman class's baseline Brace trait
    // (jf::hasBrace(), BattleState::combatDefenseBonus() - only applies
    // against an attacker who moved 2+ tiles this action) to a unit that
    // doesn't already have it, until the next Enemy Phase ends - same
    // clearing timing as the three buffs above. Consulted directly in
    // combatDefenseBonus() rather than effectiveDefense(), since unlike
    // those buffs this one is conditional on the attacker, not flat.
    bool braceSkillActive = false;
    // 古参守備兵`provoke`(挑発) (docs/initial_skill_effects.md): id of the
    // unit that provoked this one, empty = not provoked. Consulted by
    // EnemyAI.cpp's takeEnemyTurn() to override normal target selection for
    // the next Enemy Phase only (Boss AI is untouched - "Boss予告は変更
    // しない"). Cleared alongside the other Enemy-Phase-end buffs (see
    // clearSkillBuffsAtEnemyPhaseEnd()) even though this is set on the
    // provoked ENEMY rather than the caster.
    std::string provokedByUnitId;
    // 監視弓兵`overwatch`(警戒射撃) (docs/initial_skill_effects.md): once set
    // by chooseSkill(), stays armed - unlike provokedByUnitId above, this
    // has no "next Enemy Phase only" wording, so it persists across
    // multiple Enemy Phases (if none carries an enemy into range) until it
    // actually fires. Consulted by EnemyAI.cpp's triggerOverwatch(), which
    // clears it back to false the moment it fires (matching the skill's
    // own 戦闘1回 cost - it only ever ambushes once per battle regardless
    // of how many Enemy Phases pass before something wanders into range).
    // Currently only wired for the generic (non-Wolf/non-Boss) enemy AI
    // path - see takeEnemyTurn()'s comment for why.
    bool overwatchActive = false;
    // 監視弓兵`mark_target`(positive, on an enemy)/行軍隊長`support_order`
    // (negative, a damage-reduction shield on an ally) (docs/
    // initial_skill_effects.md): 0 = no effect. Adds this (signed) amount to
    // the next successful hit this unit takes from any attacker
    // (computeDamage() only reads it - stays pure for previewAttack() -
    // resolveAttack() clears it back to 0 once a real attack actually lands,
    // "その後解除").
    int markedBonusDamage = 0;
    // 行軍隊長`advance_order` (docs/initial_skill_effects.md): MOV+1 until
    // THIS Player Phase ends (not the next Enemy Phase end, unlike every
    // other buff flag above) - see jf/battle/StatusEffects.hpp's
    // applyMoveUp()/clearMoveUpAtPlayerPhaseEnd().
    bool moveUpActive = false;
    // 古参守備兵`immovable_stance` (docs/initial_skill_effects.md): a Passive
    // skill (no charge, no chooseSkill() target step - it auto-triggers the
    // instant this unit confirms Wait). DEF+3 and no movement, lasting until
    // the end of THIS unit's own next action (not a phase boundary at all).
    // `immovableStanceJustGranted` distinguishes "this is the very Wait
    // action that granted it" (don't clear yet) from "this is the next
    // action" (clear at its end) - see BattleController::finishPlayerAction().
    bool immovableStanceActive = false;
    bool immovableStanceJustGranted = false;

    // The 2 equipped-skill slots (docs/skill_system.md). See
    // jf/battle/SkillCharges.hpp for lifecycle management.
    std::array<SkillSlotState, 2> skillSlots{};

    // 灰角大猪 (docs/regions/ashbough_forest.md "灰角大猪") boss-only
    // transient state - meaningless for any other unit. Kept directly on
    // Unit rather than a parallel structure since mid-battle state is never
    // saved anyway (matches how the generic status-effect fields above are
    // already modeled). Charge always travels along the boar's own current
    // row (docs: "同じ行を...進む") and it can't move between telegraphing
    // and executing (execution is checked before any repositioning). The
    // row stays fixed while chargeDirection stores left/right travel.
    bool bossEnraged = false;               // HP<=50%, triggers once
    bool chargeTelegraphed = false;         // executes next turn, then clears
    int chargeDirection = -1;               // -1 = left, +1 = right; fixed when telegraphed
    int chargeCooldownActions = 0;          // intervening aggressive actions before another telegraph
    int chargesExecuted = 0;                // at most 2 per battle
    bool bossStunnedNextEnemyPhase = false; // set on log collision; skips one turn
    bool bossWeakenedFromStun = false;      // DEF/RES overridden low while true
    BossRuntimeState bossRuntime;

    // docs/boss_common_rules.md "Bossの退場理由": set once, the moment this
    // unit's HP first reaches 0 (see ObjectiveTracker.cpp's
    // emitUnitDefeatedEvents(), the one place that currently sets it -
    // AshenhornBoar gets ScriptedWithdrawal, everything else Defeated).
    // Meaningless while still alive; never reset, since a unit never comes
    // back mid-battle.
    UnitExitReason exitReason = UnitExitReason::Defeated;
    // docs/enemy_ai_rules.md "撤退と降伏": set once a unit reaches an Exit
    // tile via a Retreat AiCandidate (EnemyAI.cpp) - it left the field
    // alive, so `isAlive()` (HP-based) intentionally stays true; use
    // isPresent() wherever "is this unit still a threat/target on the
    // battlefield" is the actual question (unitAt(), AI targeting,
    // EliminateTeam). exitReason is set to Retreated alongside this.
    bool hasExited = false;

    bool isAlive() const { return currentHp > 0; }
    bool isPresent() const { return isAlive() && !hasExited; }

    int attackPower() const {
        return weapon.damageType == DamageType::Physical ? stats.strength : stats.magic;
    }

    int minimumAttackRange() const {
        return unitClass == UnitClass::WatchArcher && weapon.minRange < 2 ? 2 : weapon.minRange;
    }

    // Move budget after よろめき/移動低下 (docs/status_effects.md). Stagger on
    // a normal unit means "no movement" outright (0, not merely low); on a
    // boss it substitutes a MOV-1 penalty instead of a full lock.
    int effectiveMove() const {
        if (immovableStanceActive) return 0; // 古参守備兵`immovable_stance`'s next action
        if (staggerActive && !isBoss) return 0;
        int mov = stats.move;
        if (moveUpActive) mov += 1; // 行軍隊長`advance_order`
        if (staggerActive) mov = std::max(mov - 1, 0);
        if (moveDownActive) mov = std::max(mov - statusMoveDownAmount(isBoss), 1);
        return mov;
    }

    // Defense after 防御低下 (docs/status_effects.md - RES is never lowered
    // by it).
    int effectiveDefense() const {
        int def = stats.defense;
        if (defenseUpActive) def += 2;
        if (immovableStanceActive) def += 3; // 古参守備兵`immovable_stance`
        if (defenseDownActive) def = std::max(def - statusDefenseDownAmount(isBoss), 0);
        return def;
    }

    // RES after protective_treatment's buff (docs/initial_skill_effects.md -
    // a flat +3, no boss scaling specified, unlike the debuff amounts above).
    int effectiveResistance() const {
        return resistanceUpActive ? stats.resistance + 3 : stats.resistance;
    }
};

} // namespace jf
