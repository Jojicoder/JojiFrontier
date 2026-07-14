# 初期6兵種スキル実効果契約

文書種別: **正本**
正本範囲: 初期6兵種18 Skillの対象、射程、Cost、効果、AI評価、予測表示

状態: 正式仕様。名称・解放順は`skill_system.md`を正本とし、この文書は実行データとAI評価を定める。

## 共通Definition

```cpp
struct SkillDefinition {
    SkillId id;
    UnitClassId classId;
    SkillActivation activation;
    TargetRule target;
    RangeRule range;
    SkillCost cost;
    std::vector<EffectDefinition> effects;
    AiSkillWeights ai;
    std::vector<PreviewToken> preview;
};
```

`SkillCost`は`Cooldown`または`BattleCharges`。通常攻撃後に追加Skillを使えず、Skill使用で行動終了する。
反応Skillだけは敵Root Action内で発動する。

## 実効果表

| 兵種 | Skill ID / 名称 | 対象・射程 | 回数 | 確定効果 | AI基準 |
|---|---|---|---|---|---:|
| 行軍隊長 | `hold_formation` 隊形維持 | 自身と隣接味方 | CD2 | 次のEnemy Phase終了までDEF+2。本人を含む | 2人以上対象+500、3人以上+800 |
| 行軍隊長 | `advance_order` 前進命令 | 隣接する未行動味方1人 | 戦闘1回 | このPlayer Phase終了までMOV+1 | Objective到達+900、攻撃可能化+600 |
| 行軍隊長 | `support_order` 援護命令 | 隣接味方への単体攻撃 | 1 Phase 1回 | 受けるDamageを3軽減。毒・炎上・地形には反応しない | 撃破回避+1200、軽減3なら+350 |
| 古参守備兵 | `provoke` 挑発 | 敵1体、射程2 | CD2 | 次Enemy Phase、使用者を攻撃可能なら対象評価で最優先。Boss予告は変更しない | 防衛線から誘導+650、後衛保護+500 |
| 古参守備兵 | `extended_lockdown` 封鎖強化 | 自身 | 戦闘1回 | 次Enemy Phase終了までZone of Controlを距離2へ拡張 | 狭路+700、Objective防衛+900 |
| 古参守備兵 | `immovable_stance` 不動の構え | 待機時の自身 | 待機ごと | 次の自分の行動終了までDEF+3。次行動は移動不可 | 2回以上被攻撃予測+700 |
| 監視弓兵 | `suppressing_shot` 制圧射撃 | 敵1体、武器射程 | CD2 | 通常攻撃し移動低下を付与 | 接近阻止+500、Objective遅延+700 |
| 監視弓兵 | `overwatch` 警戒射撃 | 装備武器の射程 | 戦闘1回 | 次Enemy Phase、最初に射程へ入った敵へ通常攻撃1回 | 侵入予測+650、撃破可能+1000 |
| 監視弓兵 | `mark_target` 標的指定 | 敵1体、武器射程 | CD2 | Damageなし。次に味方から受ける攻撃Damage+2、その後解除 | 味方2人以上攻撃可能+550 |
| 辺境斥候 | `trailblaze` 道拓き | 仮移動で通過した灰地・浅瀬 | CD2 | このPlayer Phase中だけ味方の移動Costを1にして行動終了 | 2人以上短縮+500、Objective到達+700 |
| 辺境斥候 | `ambush` 奇襲 | 未行動の敵1体、武器射程 | 戦闘1回 | 通常攻撃しDamage+3 | 撃破+1500、未行動強敵+450 |
| 辺境斥候 | `emergency_withdrawal` 緊急離脱 | 自身、最大3 Tile | CD2 | 攻撃せず移動。敵隣接から開始可能。通常占有規則を守る | 危険低下+700、Objective到達+600 |
| 槍兵 | `spear_wall` 槍壁 | 自身と隣接味方1人 | CD2 | 2 Tile以上移動した敵から攻撃される際DEF+2。次Enemy Phase終了まで | 対象2人+550、突進予測+800 |
| 槍兵 | `halting_thrust` 足止め突き | 敵1体、武器射程 | CD2 | 通常攻撃し移動低下を付与 | 突撃敵+650、Objective遅延+700 |
| 槍兵 | `counterthrust` 反撃準備 | 攻撃者、武器射程 | 戦闘1回 | 単体武器攻撃を受け生存時、攻撃者へ通常攻撃1回 | 予測Damageと撃破で自動判断 |
| 暁の衛生兵 | `cleanse` 状態治療 | 自身か隣接味方 | CD2 | 毒、炎上、移動低下、防御低下、よろめきを全解除 | 2状態+700、炎上致死回避+1000 |
| 暁の衛生兵 | `protective_treatment` 守護処置 | 自身か隣接味方 | CD2 | 次Enemy Phase終了までRES+3 | 魔法攻撃2回予測+650 |
| 暁の衛生兵 | `emergency_treatment` 緊急処置 | HP50%以下の味方、射程2 | 戦闘1回 | HP12回復。戦闘不能は対象外 | 撃破圏脱出+1100、回復量1ごと+25 |

## 攻撃予測表示

- Damage範囲または確定値
- 対象範囲と最小・最大射程
- 付与・解除する状態と残り期間
- 移動、押出し、再移動の着地点
- Cooldownまたは残り回数
- 反応条件と「反応への反応なし」
- Boss補正後の実際の値

## AI

表のAI基準値を`enemy_ai_rules.md`の共通Scoreへ加算する。即時Objective達成、敗北防止、撃破の
優先順位を越えて上書きしない。AIは未公開罠やプレイヤーの未確定移動を参照しない。

## 受入条件

- 全Skillに対象、射程、Cost、Effect、AI Weight、Previewが存在
- 使用不能理由が日本語表示される
- Cancel、対象消失、射程外でCostを消費しない
- 同じ強化の上限と状態異常共通規則を守る
- 反撃準備が範囲・地形・状態Damageへ反応しない
- Previewと実解決結果が一致する
