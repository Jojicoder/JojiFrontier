# 戦闘行動解決順

文書種別: **説明文書**
参照正本: [`battle_resolution_contract.md`](battle_resolution_contract.md)
この文書は処理順を読みやすく説明する。順序・例外が競合する場合は参照正本を優先する。

## 状態

JOJIFrontierの攻撃、スキル、反応、地形、状態異常、Objective、勝敗判定の解説。
実装と個別仕様は`battle_resolution_contract.md`の順序へ従う。

## 基本原則

- 1回の入力またはAI決定を1つの`RootAction`として最後まで解決する
- 反応攻撃は元行動の子処理であり、別のRoot Actionにしない
- Root Action中は演出の途中でObjectiveや勝敗を確定しない
- 状態変更を先に確定し、その結果を表すEventを後から巻き戻さない
- 同じ結果を描画、Objective、報酬側で再計算しない
- Event IDはBattleStateが単調増加で発行し、同じIDを二度消費しない

## Root Actionの共通順序

1. 行動者、対象、射程、コスト、使用回数を検証
2. 移動経路を1マスずつ解決
3. 移動中の警戒射撃、Zone of Control、侵入時マス効果を解決
4. 移動終了位置を確定
5. 能動コマンドの使用回数・Cooldown・Itemを消費
6. 攻撃、スキル、アイテム、待機、装置操作の主効果を解決
7. 主効果によるダメージまたは回復を適用
8. 命中時の能力補正・状態異常を適用
9. ノックバック、引き寄せなどの強制移動を解決
10. 衝突ダメージ、Object耐久、Object破壊を解決
11. 強制移動後の侵入時マス効果を解決
12. HP0、撤退、降伏、Object破壊を確定して対応Eventを生成
13. 条件を満たす反応スキルを優先順に最大1つ解決
14. 最終位置の行動終了時地形効果を解決
15. 行動終了時状態異常を解決
16. 追加の戦闘不能を確定
17. `ActionResolved`をRoot Actionにつき1回だけ生成
18. 全EventをID順で`ObjectiveTracker`へ1回渡す
19. 副目標、敗北条件、主目的の順に評価
20. 敗北を勝利より優先してBattle Outcomeを確定

移動だけ、待機、Item使用も同じRoot Actionを使う。存在しない段階を飛ばし、順番自体は変えない。

## 攻撃Effectの順序

```text
攻撃対象固定
-> ダメージ
-> 命中時状態
-> 強制移動
-> 衝突・侵入地形
-> 戦闘不能
-> 反応可否
```

基本必中のため通常攻撃に命中判定は置かない。将来回避を使用する攻撃でも、命中が確定した後の
順序は同じにする。茂みなどの回避補正は攻撃結果の確定前にだけ参照する。

ノックバック後の射程や位置を使う処理は、強制移動後の確定座標を参照する。押し出された対象が
衝突、地形、状態ダメージで戦闘不能になった場合は反応できない。

## 反応の解決

- 反応は元のRoot Actionと同じ`ActionId`を持ち、固有の`EffectStepId`を持つ
- 反応のダメージ、状態、強制移動、戦闘不能Eventも同じBatchへ追加する
- 反応から別の反応、警戒射撃、追撃、反撃を発生させない
- Root Action内で発生できる攻撃的反応は最大1回
- 複数の反応候補がある場合は、明示された優先度、Unit IDの順で1つだけ選ぶ
- 反応で攻撃者を倒しても、元行動のEventを削除しない

反応撃破による`UnitDefeated`もObjectiveへ通常どおり渡す。同じBatchで味方敗北条件と敵主目的が
同時成立した場合は敗北を優先する。

## 行動終了地形

行動終了地点では次の順序を使う。

1. 浅瀬などの解除効果
2. 薬草地点などの回復効果
3. 炎上床などの状態付与
4. 炎上など行動終了時状態ダメージ
5. 戦闘不能判定

強制移動で通過しただけの薬草地点から回復を得ない。侵入時発動と明記された罠・衝突だけは
強制移動直後に処理する。1つのActionで同じ一回限りマス効果を二度発動させない。

## Phase終了と開始

Phase終了:

1. 対象側Phase終了時の状態異常
2. 戦闘不能
3. 一時効果の残り時間更新・解除
4. `PhaseEnded`
5. Enemy Phase終了時は`RoundEnded`
6. Event BatchをObjective評価
7. 敗北優先で勝敗判定

次Phase開始:

1. Round・Phase状態を更新
2. 予告済み増援を解決
3. Bossの予告済みPhase処理を準備
4. Skill回数・Phase効果を更新
5. `PhaseStarted`
6. 開始BatchをObjective評価
7. 勝敗未成立なら入力またはAI行動へ進む

増援は`PhaseStarted`より前に状態へ追加し、`ReinforcementResolved`を同じ開始Batchへ含める。

## Event最小構成

```cpp
struct BattleEventHeader {
    BattleEventId eventId;
    ActionId rootActionId;
    EffectStepId effectStepId;
};
```

Eventには結果として確定したUnit ID、座標、HP差分、状態ID、退場理由を保持する。表示文はEventへ
保存せず、Localization IDと確定値からUIが組み立てる。

## 受入条件

- 通常攻撃でノックバック後に地形衝突し、戦闘不能、Objective、勝敗の順で処理される
- 反応で攻撃者を倒した場合も同じBatchで`UnitDefeated`が1回だけ処理される
- 同一Batchで敵全滅と味方全滅が成立すると敗北になる
- 浅瀬で炎上を解除したUnitはそのAction終了時の炎上ダメージを受けない
- `ActionResolved`は反応を含むRoot Action全体で1回だけ発行される
- 同じEvent IDを再投入してもObjective進捗が増えない
