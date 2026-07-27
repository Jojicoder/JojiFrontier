#pragma once

#include <string>

namespace jf {

enum class UnitClass {
    MarchCaptain,
    VeteranGuard,
    WatchArcher,
    FrontierScout,
    Spearman,
    DawnChirurgeon,
    // docs/class_reference.md「後半6兵種」/M7項目1: 固有能力「重量装甲」
    // (hasHeavyArmor())。加入経路(M7項目2)は未実装のため、まだ
    // playerParty/reserveRosterには登場しない - Classとしてのみ完全に有効。
    HeavyInfantry,
    // docs/class_reference.md「後半6兵種」/M7項目1: 固有能力「野戦工作」
    // (canFieldFortify())。加入経路(M7項目2)は未実装のため、まだ
    // playerParty/reserveRosterには登場しない - Classとしてのみ完全に有効。
    FrontierEngineer,
    // docs/class_reference.md「後半6兵種」/M7項目1: 固有能力「再移動」
    // (canReMove())。加入経路(M7項目2)は未実装のため、まだ
    // playerParty/reserveRosterには登場しない - Classとしてのみ完全に有効。
    MessengerCavalry,
    // docs/class_reference.md「後半6兵種」/M7項目1: 固有能力「簡易罠」
    // (canSetSimpleTrap())。加入経路(M7項目2)は未実装のため、まだ
    // playerParty/reserveRosterには登場しない - Classとしてのみ完全に有効。
    FrontierRanger,
    // docs/class_reference.md「後半6兵種」/M7項目1: 固有能力「戦旗」
    // (hasBannerAura())。加入経路(M7項目2)は未実装のため、まだ
    // playerParty/reserveRosterには登場しない - Classとしてのみ完全に有効。
    BannerBearer,
    // docs/class_reference.md「後半6兵種」/M7項目1: 固有能力「魔力波及」
    // (hasArcaneOverflow())。加入経路(M7項目2、希少な名前付き加入イベント)は
    // 未実装のため、まだplayerParty/reserveRosterには登場しない -
    // Classとしてのみ完全に有効。
    BattleMage,
    Bandit,
    // docs/regions/ashbough_forest.md 灰枝の林縁: an ordinary large predator,
    // not a monster (per World Bible flora_fauna.md framing) - no special
    // combat rule of its own, just a distinct enemy roster entry.
    Wolf,
    // docs/regions/ashbough_forest.md "灰角大猪" (折れ木の縄張り's boss,
    // individual name "折れ木の主"). Own AI (jf/battle/EnemyAI.hpp's
    // takeBoarBossTurn()) and boss-only transient Unit fields.
    AshenhornBoar,
    // docs/regions/ashiron_quarry.md "灰殻穿岩虫" (崩落核's boss). Own AI
    // (jf/battle/EnemyAI.hpp's takeGrubwormBossTurn()), reuses AshenhornBoar's
    // generic boss-runtime Unit fields (bossEnraged/chargeTelegraphed/etc).
    AshironGrubworm,
    // docs/regions/blackwater_lowlands.md "沼牙の大蛇" (深泥の水源's boss). Own
    // AI (jf/battle/EnemyAI.hpp's takeSerpentBossTurn()), reuses the same
    // generic boss-runtime Unit fields as the boar/grubworm above.
    MarshFangSerpent,
    // docs/regions/windscar_plateau.md "高原運び手の隊長" (高原伝令所's boss).
    // Own AI (jf/battle/EnemyAI.hpp's takeCourierCaptainBossTurn()), reuses
    // the same generic boss-runtime Unit fields as the boar/grubworm/serpent
    // above. Stat shape starts from MessengerCavalry per the doc's own
    // "兵種: 伝令騎兵型" wording, but the boss's HP40/DEF6/RES4 diverge too
    // far from the base class (HP22/DEF4/RES3) to reuse it directly - a new
    // UnitClass with its own baseStats entry, same decision M9-D/M9-K made
    // for their own bosses.
    PlateauCourierCaptain,
    // docs/regions/old_frontier_settlement.md "強敵 襲撃団頭領" (夜明けの共同
    //防衛's elite enemy). Unlike the true bosses above (Boar/Grubworm/
    // Serpent/CourierCaptain, each with their own takeXBossTurn()), the doc
    // is explicit this is NOT a scripted-withdrawal boss - it's optional to
    // defeat, and the region's real win condition is surviving 5 rounds
    // regardless of whether this unit is ever engaged. No bespoke EnemyAI.cpp
    // turn function was written for it: it goes through the same generic
    // takeEnemyTurn()/generateAiCandidates() path as any other enemy, with
    // AiSystem.cpp's profileFor() giving it a tuned retreatHpPercent (30,
    // matching the doc's "HP30%以下...撤退") - see that function's own
    // comment for what's approximated away (the doc's added "かつ4ラウンド目
    // 以降" round-gate has no round-aware hook in AiProfile, so this unit
    // considers retreating at HP30% from round 1, not just round 4+).
    RaidLeader,
    // docs/regions/ember_ravine.md "赤背の大蜥蜴" (赤熱裂け目's boss, region 7's
    // final/required boss - unlike RaidLeader above, the doc lists defeating
    // this unit as sub-condition 1 of an AND-primary, matching the "true
    // boss" shape of AshenhornBoar/AshironGrubworm/MarshFangSerpent/
    // PlateauCourierCaptain, not RaidLeader's optional-elite shape). Own AI
    // (jf/battle/EnemyAI.hpp's takeRedbackLizardBossTurn()), reuses the same
    // generic boss-runtime Unit fields as the 4 true bosses above.
    RedbackLizard,
    // docs/regions/buried_dawn_sanctum.md "聖堂回収団長" (夜明け祭壇's elite
    // enemy). Same optional-elite shape as RaidLeader above (the doc is
    // explicit "強敵撃破は最終攻略に撃破不要" - the primary is a
    // SurviveRounds/crate-preservation objective the party can complete
    // whether or not this unit is ever engaged): no bespoke EnemyAI.cpp
    // turn function, just the generic takeEnemyTurn()/generateAiCandidates()
    // path with AiSystem.cpp's profileFor() giving it a tuned
    // retreatHpPercent (25, matching the doc's "HP25%以下...降伏する") - the
    // doc's additional retreat sub-conditions ("記録箱2個保全、退路あり") have
    // no round/Object-aware hook in AiProfile, so only the HP threshold is
    // modeled, same simplification RaidLeader's own comment already made.
    SanctumRetrievalLeader,
    // docs/regions/shattered_march_fort.md "残留砦隊長" (切離命令庫の最終強敵).
    // Same optional-elite shape as RaidLeader/SanctumRetrievalLeader above
    // (正本「隊長撃破を必須にしない」/不変条件「隊長撃破を必須にしない」): no
    // bespoke EnemyAI.cpp turn function, just the generic
    // takeEnemyTurn()/generateAiCandidates() path with AiSystem.cpp's
    // profileFor() giving it a tuned retreatHpPercent (25, matching the doc's
    // "HP25%以下...撤退"). The doc's 3 bespoke abilities (交互防衛/壁際指揮/
    // 記録封鎖) and the retreat condition's additional "命令箱2個保全" clause
    // are all deferred - see AiSystem.cpp's own comment for this class.
    FortGarrisonCaptain,
    // docs/regions/mapped_edge.md「最終戦「地図外縁」」's 環境波「普通の大型獣
    // 1体」(HP58/STR10/DEF7/RES3/MOV4, "直線突進を1Round前に予告するが、撃破は
    // 不要"). No existing UnitClass matches this exact stat line (checked
    // data/classes.json), and this project has no per-instance stat-override
    // mechanism on top of a shared UnitClass (checked BattleFactory.cpp/
    // Region.cpp), so this is a new class rather than a restat of
    // AshenhornBoar/AshironGrubworm/MarshFangSerpent/RedbackLizard. Chose the
    // TRUE-scripted-boss shape (own takeXBossTurn() in jf/battle/EnemyAI.hpp,
    // reusing the same generic chargeTelegraphed/bossRuntime.telegraph/
    // chargeCooldownActions Unit fields as the 5 bosses above) over the
    // optional-elite shape (RaidLeader/SanctumRetrievalLeader/
    // FortGarrisonCaptain's tuned retreatHpPercent, no bespoke turn function)
    // because the doc is explicit about a repeating TELEGRAPHED STRAIGHT-LINE
    // CHARGE, a mechanic the optional-elite shape has no hook for at all
    // (AiProfile only tunes retreat/aggression, never telegraphs anything) -
    // "撃破は不要" only means the primary objective doesn't require its
    // death, which this project already gets for free here since the site's
    // primary is composed entirely of OperateObject+EscapeUnits members (no
    // EliminateTeam member at all - see mappedEdgeFinalStage()), not a
    // property of the AI shape. Unlike the 5 true bosses above, this beast
    // has no enrage stage, no secondary attack, and no scripted-withdrawal
    // flavor ("普通の大型獣" - an ordinary beast, not a named individual) -
    // its own takeFrontierBeastBossTurn() is deliberately the simplest of
    // the 6, a single-mode telegraph-charge-or-melee loop with no stage
    // transitions.
    FrontierBeast,
};

enum class Team {
    Player,
    Enemy
};

std::string toString(UnitClass unitClass);
bool providesFormationBonus(UnitClass unitClass);
bool hasZoneOfControl(UnitClass unitClass);
bool ignoresAshPenalty(UnitClass unitClass);
bool hasBrace(UnitClass unitClass);
bool canHeal(UnitClass unitClass);
// docs/class_reference.md「野戦工作」: same "own dedicated command outside
// the 2 equip slots" shape as canHeal() above -戦闘中1回、隣接空きマスへ
// 防護板を設置(BattleController::chooseFieldFortification()).
bool canFieldFortify(UnitClass unitClass);
int passiveEvasionBonus(UnitClass unitClass);
// docs/class_reference.md「重量装甲」: this engine's knockback is always
// exactly 1 tile (BattleState::applyKnockback), so "reduce distance by 1,
// floor 0" collapses to "never gets knocked back" - consulted directly
// there, unconditionally (unlike the consumable knockbackNegatesRemaining).
bool hasHeavyArmor(UnitClass unitClass);
// docs/class_reference.md「再移動」: 攻撃・スキル・アイテム行動後、生存していれば
// 最大2マス(`ride_through`使用時4マス)移動して行動終了 - BattleController::
// finishPlayerAction()が消費し、SelectReMoveTargetへの遷移を判定する。
bool canReMove(UnitClass unitClass);
// docs/class_reference.md「簡易罠」: same "own dedicated command outside the
// 2 equip slots" shape as canHeal()/canFieldFortify() above -戦闘中1回、
// 隣接空きマスへ罠を設置(BattleController::chooseSimpleTrap())。設置物
// Definitionは`snare_trap`スキルと共有する("ranger_trap")。
bool canSetSimpleTrap(UnitClass unitClass);
// docs/class_reference.md「戦旗」: マンハッタン距離2以内の味方のSTR/MAGを+1する
// 常時Aura(旗手自身は対象外、複数いても重複しない) - CombatResolver.cppの
// bannerAuraBonus()とStatusEffects.cppのconsumeUnyieldingSignalIfAvailable()
// (`unyielding_signal`)が参照する。
bool hasBannerAura(UnitClass unitClass);
// docs/class_reference.md「魔力波及」: 戦闘中1回、通常魔法攻撃後に対象と上下隣接
// する敵へ固定ダメージ(DEF/RES無視、反応攻撃なし) - BattleController::
// confirmAttack()がUnit::arcaneOverflowUsedと合わせて判定する。
bool hasArcaneOverflow(UnitClass unitClass);

} // namespace jf
