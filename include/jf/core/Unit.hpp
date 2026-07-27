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
    // 奇襲刃(ambush_blade)「そのラウンドで未行動の敵」判定専用。`hasActed`は
    // 自チームの次のPhase開始(beginPlayerPhase()/beginEnemyPhase())まで
    // リセットされないため、Player Phase中は敵の`hasActed`が前Roundの
    // Enemy Phase終了時点の値(=true)のまま残り続け、「未行動」判定に使えない
    // (2Round目以降ずっとfalseにならないバグになる)。`markActed()`が呼ばれた
    // 時点のBattleState::round()を記録し、`target.lastActedRound !=
    // battle.round()`で「今Roundではまだ行動していない」を正しく判定する。
    int lastActedRound = 0;
    int tilesMovedThisAction = 0;

    // Hide-Wrapped Grip tuning trait: negates this many knockbacks for the
    // rest of the battle (set once at battle start, decremented on use).
    int knockbackNegatesRemaining = 0;

    // docs/regions/ember_ravine.md 敵勢力「岩蜥蜴」: "各戦闘最初の炎上付与だけ
    // 無効" - same "negate N of X for the rest of the battle, set once at
    // battle start, decremented on use" shape as knockbackNegatesRemaining
    // above. Consumed by jf::applyBurn() (jf/battle/StatusEffects.hpp)
    // regardless of Burn's source (weapon on-hit or a terrain tile like
    // FireFloor), since that's the single application entry point.
    int firstBurnNegatesRemaining = 0;

    // M10-B (docs/deep_layers.md「防具用調整特性」`ward_step` armor tuning
    // trait): "戦闘ごとに最初の状態異常を1回無効" - generic across every
    // StatusEffectType (unlike firstBurnNegatesRemaining above, which only
    // covers Burn), consumed at jf::applyStatusEffect()'s single choke point
    // (jf/battle/StatusEffects.hpp) rather than duplicated per-effect. Same
    // "set once at battle start, decremented on use" lifecycle.
    int firstStatusNegatesRemaining = 0;

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
    // 戦闘魔導士`ward_break`(魔防破砕、docs/class_reference.md「後半6兵種」):
    // same "read-only for previewAttack(), cleared by resolveAttack() on a
    // real hit" lifecycle as markedBonusDamage above, but only added when
    // `attacker.weapon.damageType == DamageType::Magical`(魔法攻撃限定) -
    // kept as its own field rather than folding into markedBonusDamage since
    // that one applies to any damage type.
    int magicMarkedBonusDamage = 0;
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
    // 重装兵`brace_for_impact` (docs/class_reference.md「後半6兵種」/
    // docs/skill_system.md「重装兵」): same "auto-triggers on Wait, lasts
    // through this unit's own next action" shape as immovable_stance above,
    // but DEF+3 and full forced-movement negation instead of DEF+3-and-no-
    // movement - so effectiveMove() is untouched, only effectiveDefense()
    // and BattleState::applyKnockback() consult this.
    bool braceForImpactActive = false;
    bool braceForImpactJustGranted = false;
    // 辺境工兵「野戦工作」(docs/class_reference.md「後半6兵種」): 戦闘中1回の
    // Active固有能力(canHeal()と同じく2スキル枠の外)。canHeal()同様この
    // フィールドで一度使用済みかを追跡する(clearAllStatusEffectsでは戻さない
    // -戦闘スコープの1回きり)。
    bool fieldFortificationUsed = false;
    // 伝令騎兵「再移動」(docs/class_reference.md「後半6兵種」): consumed by
    // BattleController::finishPlayerAction() the moment it computes this
    // action's re-move budget (2, or 4 if this flag is set) - always cleared
    // there regardless of whether a re-move tile ends up chosen, since
    // `ride_through`'s boost is spent on "this action" either way.
    bool rideThroughBudgetActive = false;
    // 伝令騎兵`urgent_dispatch`(緊急伝令): same "until THIS Player Phase ends"
    // lifecycle as moveUpActive above (行軍隊長`advance_order`), but +2
    // instead of +1 - kept as its own flag so advance_order's existing
    // behavior/tests stay untouched. Cleared by the same
    // clearMoveUpAtPlayerPhaseEnd().
    bool urgentDispatchActive = false;
    // 辺境猟兵「簡易罠」(docs/class_reference.md「後半6兵種」): 戦闘中1回の
    // Active固有能力(canHeal()/canFieldFortify()と同じく2スキル枠の外)。
    // fieldFortificationUsed同様この1フィールドで一度使用済みかを追跡する。
    bool simpleTrapUsed = false;
    // 辺境猟兵`read_quarry`(獲物を読む): 純粋に情報表示的なスキル(行動を強制
    // しない) - 既存エンジンに敵AIの行動予測を保持・表示する仕組みが無いため、
    // 今回はこのデータフラグのみ実装する(実際のプレビューUIは対象外)。
    // overwatchActive等と同じ「次のEnemy Phase終了まで」ライフサイクルで
    // clearSkillBuffsAtEnemyPhaseEnd()が解除する。
    bool quarryRevealed = false;
    // 旗手`rallying_banner`(奮起の旗): 距離2以内の味方(自身含む)にDEF+1/RES+1を
    // 付与、次のEnemy Phase終了まで。既存のdefenseUpActive(+2)/resistanceUpActive
    // (+3)とは数値が異なるため使い回さず専用フィールドにする。
    // clearSkillBuffsAtEnemyPhaseEnd()が解除する。
    bool rallyingBannerActive = false;
    // 戦闘魔導士「魔力波及」(docs/class_reference.md「後半6兵種」): 戦闘中1回の
    // 固有能力(canHeal()/canFieldFortify()と異なり専用コマンドではなく、通常攻撃
    // 命中時にBattleController::confirmAttack()が自動判定する)。
    bool arcaneOverflowUsed = false;
    // Snare Bow/Driving Bow/Ember Focus/Resonant Focus (docs/
    // base_development.md, M7項目3続き 武器分岐固有効果): 戦闘中1回だけ、この
    // 武器のonHitStatuses/causesKnockback/splashDamageのうちどれか1つを許可
    // する(weapon.firstHitOnly)。arcaneOverflowUsed同様、clearAllStatusEffects
    // では戻さない(戦闘スコープの1回きり)。
    bool weaponFirstHitUsed = false;
    // Guard Sword(護衛剣, docs/base_development.md「M7項目3続き 武器分岐固有効果
    // (高コストTier)」): 戦闘中1回、隣接味方が受ける最初の攻撃ダメージを3軽減する。
    // weaponFirstHitUsed同様「戦闘スコープの1回きり」だが、この武器はここでしか
    // 使わない独立フラグにした(weaponFirstHitUsedは同じユニットが同時に持てる
    // onHitStatuses/causesKnockback/splashDamageと排他共有する前提のため)。
    // 消費側はBattleState::combatDefenseBonus()(参照/プレビュー含む)、確定側は
    // CombatResolver.cppのresolveAttack()。
    bool guardSwordShieldUsed = false;
    // Fortress Lance(城塞槍): このユニットのZone of Controlへ新規に入った敵は、
    // 次の行動が終わるまで与ダメージ-2。BattleState::moveUnit()が入域を検出して
    // 立て、BattleState::markActed()がその「次の行動」の終わりでクリアする
    // (Waitトリガー系のJustGranted二重フラグは不要 - 付与そのものが移動であって
    // 行動の確定ではないため、「次の行動」は必ずこのフラグが立った後の初回行動)。
    bool zocEntryDamageDownActive = false;
    // Patrol Lance(巡回槍): 待機で行動終了した場合、次のPlayer Phase開始まで
    // DEF+2。immovable_stance/brace_for_impactの「自身の次の行動まで」とは違い
    // 「次のPlayer Phase開始まで」なのでJustGranted二重フラグは不要 - Player
    // Phaseの間だけ生き続け、BattleState::beginPlayerPhase()がクリアする。
    bool patrolLanceReadyDefenseActive = false;
    // Bulwark Maul(防壁槌): 待機時、次の自分の行動開始まで(=自身の次の行動が
    // 終わるまで)DEF+2。immovable_stance/brace_for_impactと全く同じ「Waitで
    // 自動発動、JustGrantedがそのWait自身と次の行動を区別する」ライフサイクル。
    bool bulwarkMaulActive = false;
    bool bulwarkMaulJustGranted = false;

    // 連携作戦(docs/character_progression.md「連携作戦」) - each pair gets its
    // own dedicated flag rather than reusing an existing one whose magnitude
    // doesn't match (same reasoning as rallyingBannerActive's own comment
    // above: e.g. defenseUpActive is +2 but resistanceUpActive is +3, neither
    // matches this Slice's +2/+2 shape exactly, and overloading them would
    // couple two independently-tuned effects together).
    // `paired_fallback_line`(帰還線, ライダン+マエラ): DEF+2, granted to both
    // paired units AND every ally within radius 1 of either paired unit.
    // "次のEnemy Phase終了まで" - same lifecycle as rallyingBannerActive,
    // cleared by clearSkillBuffsAtEnemyPhaseEnd().
    bool pairedFallbackLineActive = false;
    // `paired_signal_ward`(灯火の結界, レッサ+イリエン): RES+2 to every ally
    // within distance 2 of either paired unit. Same "次のEnemy Phase終了まで"
    // lifecycle as pairedFallbackLineActive above. Currently unreachable in
    // play (see jf::isCooperationUnlocked()'s own comment - its region,
    // 埋没聖堂/Buried Dawn Sanctum, isn't implemented) but the flag/effect
    // exist so nothing else needs revisiting once that region ships.
    bool pairedSignalWardActive = false;
    // `paired_braced_breakthrough`(支え合う突破, コレン+ハドリク): grants both
    // paired units forced-move immunity (consulted by the same
    // hasHeavyArmor()/braceForImpactActive checks in BattleState::
    // applyKnockback()/applyPull()/resolveWindGustRoundEnd()) AND DEF+2
    // specifically against an attacker who moved 2+ tiles this action
    // (consumed by BattleState::combatDefenseBonus(), the same "Brace"-style
    // tilesMovedThisAction>=2 check hasBrace()/weapon.braceBoost/
    // braceSkillActive already use - kept as its own flag since its
    // lifecycle, "次のEnemy Phase終了まで", differs from braceSkillActive's
    // own single-action lifecycle). Same clearSkillBuffsAtEnemyPhaseEnd()
    // lifecycle as the two flags above.
    bool pairedBracedBreakthroughActive = false;


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

    // 灰殻穿岩虫 (docs/regions/ashiron_quarry.md "灰殻穿岩虫") boss-only: HP<=50%
    // one-time "崩落誘発" trigger, mirrors bossEnraged's role for a different boss.
    bool bossCollapseUsed = false;
    // 灰殻穿岩虫「岩殻防御」: true for exactly the action right after a charge,
    // during which the ability's DEF+2 bonus is lost.
    bool bossChargeRecoveryPending = false;

    // 沼牙の大蛇 (docs/regions/blackwater_lowlands.md「地域ボス 沼牙の大蛇」)
    // boss-only: HP<=50% one-time "激しい身震い" trigger, same role as
    // bossEnraged/bossCollapseUsed above. 水中潜行's own telegraph reuses the
    // generic chargeTelegraphed/bossRuntime.telegraph/chargeCooldownActions
    // fields above (M9-D's own note: "いずれもボア専用ではない、Unit上の汎用
    // フィールド") rather than adding new ones.
    bool bossShudderUsed = false;

    // 高原運び手の隊長 (docs/regions/windscar_plateau.md「地域ボス 高原運び手の
    // 隊長」) boss-only: 迂回命令 is a once-per-battle telegraph (reuses the
    // generic chargeTelegraphed/bossRuntime.telegraph fields above, same
    // reasoning as bossShudderUsed's own comment); 退路確保 is an HP<=50%
    // one-time trigger, same role as bossEnraged/bossCollapseUsed/
    // bossShudderUsed above.
    bool bossFlankUsed = false;
    bool bossEscapeRouteUsed = false;

    // 赤背の大蜥蜴 (docs/regions/ember_ravine.md「地域ボス 赤背の大蜥蜴」)
    // boss-only: HP<=50% one-time "噴気誘導" trigger, same role as
    // bossEnraged/bossCollapseUsed/bossShudderUsed above. 熱砂突進's own
    // telegraph reuses the generic chargeTelegraphed/bossRuntime.telegraph/
    // chargeCooldownActions fields above, same reasoning as those bosses'
    // own comments.
    bool bossFumeLureUsed = false;

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

    // docs/regions/blackwater_lowlands.md "5. 黒水渡し"の荷運び役のような
    // 戦闘限定ゲストユニット(常設ロースターに加わらない)であることを示す表示専用
    // フラグ。team/AI/選択ロジックは通常のTeam::Playerユニットと変わらない -
    // 見た目(名札・肖像枠など)の区別のためだけに存在する。
    bool isGuest = false;

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
        if (urgentDispatchActive) mov += 2; // 伝令騎兵`urgent_dispatch`
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
        if (braceForImpactActive) def += 3; // 重装兵`brace_for_impact`
        if (rallyingBannerActive) def += 1; // 旗手`rallying_banner`
        if (patrolLanceReadyDefenseActive) def += 2; // 巡回槍
        if (bulwarkMaulActive) def += 2; // 防壁槌
        if (pairedFallbackLineActive) def += 2; // 連携作戦`paired_fallback_line`
        if (defenseDownActive) def = std::max(def - statusDefenseDownAmount(isBoss), 0);
        return def;
    }

    // RES after protective_treatment's buff (docs/initial_skill_effects.md -
    // a flat +3, no boss scaling specified, unlike the debuff amounts above).
    int effectiveResistance() const {
        int res = stats.resistance;
        if (resistanceUpActive) res += 3;
        if (rallyingBannerActive) res += 1; // 旗手`rallying_banner`
        if (pairedSignalWardActive) res += 2; // 連携作戦`paired_signal_ward`
        return res;
    }
};

} // namespace jf
