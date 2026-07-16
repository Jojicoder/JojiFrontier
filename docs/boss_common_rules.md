# Boss共通規則
## 状態

共通仕様確定。各地域Boss書は能力値、固有行動、探索差分、専用ギミックだけを定義し、状態異常、
強制移動、予告、Phase移行、退場理由は本書に従う。

実装済み(2026-07): `jf::UnitExitReason`(`jf/core/UnitExitReason.hpp`、本書の5値そのまま)と
`Unit::exitReason`、退場理由を含む`UnitDefeatedEvent`、`jf::BossStageChangedEvent`。
`BossRuntimeState`と`BossTelegraph`は段階Index、行動ID、形状、予告・実行Round、固定対象・固定範囲、
方向、状態を共通保持する。灰角大猪は
HP0時に`ScriptedWithdrawal`(撃破相当)を設定し、激昂時に`BossStageChangedEvent`を1回発行する
(`EnemyAI.cpp`)。大猪の突進予告は共通`BossTelegraph`へ固定し、予告・消費時に
`BossTelegraphChangedEvent`を発行する。Objective Definition側の許可退場理由Filterは未実装。

## 基本原則

- BossはHPだけが高い通常敵にしない
- 強力な範囲攻撃、突進、地形変化は原則1ラウンド前に予告する
- 予告後に対象、列、範囲を追尾変更しない
- 対処不能な即時大ダメージ、永久行動不能、無限増援を使用しない
- 探索、兵種、地形、Objectのうち最低1つに固有行動への対抗手段を用意する
- Boss撃破・撤退成果はPendingであり、安全帰還まで恒久化しない

## 状態異常

通常Bossには`status_effects.md`の共通補正を使う。

| 状態 | Boss補正 |
|---|---|
| 毒 | Phase終了時1ダメージ、最大2回 |
| 炎上 | 行動終了時2ダメージ、最大2回 |
| 移動低下 | MOV-1 |
| 防御低下 | DEF-2 |
| よろめき | 移動不可ではなく次の行動だけMOV-1 |

個別の完全無効は戦闘前情報と対象予測に表示する。専用ギミックによるスタン・弱体は通常状態と
別IDにし、万能薬や状態治療で解除できない。通常状態異常だけでBossを永久停止できない。

## 強制移動・進路制御・挑発

- 通常のノックバック、引き寄せ、位置交換ではBossを移動させない
- 地域固有ギミックが明示した場合だけ、指定距離・指定方向へ移動させる
- ノックバック無効でも衝突を前提とするBoss専用突進は通常どおり処理する
- Zone of Controlへ外から入った場合、その移動の残りを失う
- Zone of Control内から行動開始したBossは現在地から攻撃でき、隣接マスへの移動も禁止されない
- 複数のZone of ControlでBossを行動不能にしない
- 挑発は対象評価を上げるが、予告済み行動、任務Object、防衛地点、Phase移行を上書きしない

進路封鎖はBossの到達速度を落とす戦術であり、毎Phase何もできなくする戦術にはしない。

## 予告行動

予告時に次を固定する。

- 行動ID
- 実行Round・Phase
- 対象Unitまたは対象マス・行・範囲
- 通過経路
- 基本ダメージ式と付与状態
- 中断条件と失敗時処理

対象が移動しても追尾しない。実行時に経路が無効なら、地域書で定めた順に処理する。

1. 有効な部分まで実行して停止
2. 専用Objectとの衝突を処理
3. 完全に実行不能なら行動を取消し、攻撃を伴わない位置調整または待機

取消時に別対象へ即時通常攻撃しない。予告済み行動は挑発、低HP優先、AI再評価で変更しない。

## Phase移行

Boss Phaseは通常のPlayer/Enemy Phaseとは別の追加手番を意味せず、Boss内部の行動段階を表す。

1. Root Action Batchを最後まで解決
2. 戦闘不能と勝敗を評価
3. Bossが生存し戦闘継続中なら閾値を確認
4. 1回だけ`BossStageChanged`を発行
5. 能力、行動候補、地形予告を更新
6. 次の通常Enemy Phaseから新段階のAIを使用

HP50%など複数閾値を1Actionで越えても、定義順に状態だけ更新し、追加攻撃を得ない。Phase移行時の
地形変化は先に予告するか、即時ダメージを持たない変化に限定する。予告済み行動がある場合は地域書に
取消条件がない限り先に実行し、その後新段階へ移る。

## Bossの退場理由

```cpp
enum class UnitExitReason {
    Defeated,
    Retreated,
    Escaped,
    Surrendered,
    ScriptedWithdrawal,
};
```

- `Defeated`: HP0。通常の撃破Objectiveに加算
- `Retreated`: AI判断で盤外へ撤退。撃破には数えない
- `Escaped`: 任務上の脱出成功。阻止Objectiveは失敗
- `Surrendered`: 降伏条件成立。生存扱いだが地域書指定で無力化Objectiveへ加算可能
- `ScriptedWithdrawal`: HP0や専用条件後の演出退場。地域書が明示した場合だけ撃破相当

Boss素材、撃破Discovery、地域攻略成果はObjective Definitionが許可した退場理由でのみ付与する。
表示演出だけを見て報酬側が撃破と推測しない。灰角大猪はHP0後の
`ScriptedWithdrawal`を撃破相当として定義する。

## 範囲攻撃と反撃

Bossの範囲攻撃、突進通過ダメージ、地形ダメージには通常反撃しない。直接単体攻撃だけが
`counterattack_rules.md`の反撃準備を発動できる。Boss本人も反撃スキルを明示的に持たない限り
攻撃された際に反撃しない。

## 複数マスBoss

本編10地域の初期実装はすべて1マス占有で完成させる。複数マスBossはBattle Object、Footprint、
経路探索、範囲選択、Save Snapshotが完成した後の深層遠征で初導入する。

導入条件:

- 全占有マスを1つのUnit IDへ紐付ける
- 移動前後のFootprint全体で通行・Object衝突を検証する
- 攻撃対象はUnit単位で、占有マスごとの多重Hitを起こさない
- HP、状態、行動回数は1体分だけ持つ
- Objectiveと報酬は1回だけ発火する
- 中断Saveに基準座標、向き、Footprint IDを保存する

複数マス対応前に見た目だけ2マスへ広げない。

## 受入条件

- 通常状態異常へBoss補正を適用し、永久停止できない
- 通常ノックバックでは移動せず、専用ギミックだけがBoss位置を変える
- 挑発で予告済み攻撃列が変わらない
- 予告対象が移動しても攻撃が追尾しない
- HP閾値を越えても同じEnemy Phaseに追加行動しない
- `Retreated`をBoss撃破Objectiveへ加算しない
- 灰角大猪の専用撤退だけを撃破相当として扱う
- Boss範囲攻撃へ反撃準備が発動しない
