# 戦闘イベント・解決順契約

文書種別: **正本**
正本範囲: Root Action、Event順、同時発生、増援、Boss段階移行、反応処理

状態: 正式仕様。Object個別規則は`battle_objects.md`、Objective条件は`mission_objectives.md`を参照する。

## Root Action

プレイヤー入力、敵AI行動、増援出現、Phase開始効果をそれぞれ1つの`RootActionId`で囲む。
Root Action中に発生したイベントをBatchとして最後まで解決してから、勝敗と次入力へ進む。

```cpp
struct BattleEventEnvelope {
    BattleEventId eventId;
    RootActionId rootActionId;
    int sequence;
    BattleEventPayload payload;
};
```

同じ`eventId`は一度だけ適用する。反応行動は同じRoot Action内に子Actionとして入り、別の
プレイヤー入力機会を作らない。

## 完全解決順

1. 行動合法性とCostを再検証
2. 使用回数・Itemを予約
3. 移動と停止マスを確定
4. 攻撃対象・範囲・予告固定対象を確定
5. Damageと回復を同時Snapshotから計算
6. Damage、回復、Barrierを適用
7. Knockback、引寄せ、衝突Damageを適用
8. Object耐久、破壊、地形変化を適用
9. 状態異常・一時強化・解除を適用
10. 停止マス地形効果を適用
11. HP0、撤退、降伏、脱出を退場理由付きで確定
12. 反応Skillを安定優先順で最大1回ずつ解決
13. Objective TrackerへBatchを渡す
14. Boss段階移行条件を評価
15. 敗北を先、勝利を後に評価
16. Cost予約を確定してAction終了Eventを発行

途中で対象が倒れてもRoot Actionを中断せず、既に確定した範囲効果とObject破壊を解決する。
ただし倒れたUnitがまだ開始していない反応Skillは実行しない。

## 同時発生

- 同一範囲攻撃の全対象Damageは攻撃前Snapshotから計算する
- 同時撃破はUnit ID順にEvent化するが、Objective判定はBatch全体を見て行う
- 同じBatchで敵味方全滅なら敗北を優先する
- 主目的と副目標が同時達成した場合は両方を記録してから勝利する
- Object破壊とSecure Tile達成が同時なら、破壊後のTile状態を使う
- Boss HP閾値到達と撃破が同時なら撃破を優先し、段階移行演出を発生させない

## Battle Object

- Unitとは別Containerで管理し、Object ID、陣営、占有Tile、耐久、通行、相互作用を持つ
- Object Damageは対応Tagを持つActionだけが与えられる
- 破壊Eventは1回だけ発行し、残骸への置換後にObjectiveを評価する
- 護衛対象がObjectかUnitかをObjective Definitionで明示する

## 増援

```cpp
struct ReinforcementWaveDefinition {
    ReinforcementWaveId id;
    int warningRounds;
    SpawnTiming timing;
    std::vector<TilePos> spawnTiles;
    std::vector<UnitSpawnDefinition> units;
    bool actsImmediately = false;
    bool countsForElimination = true;
    int maxDelayRounds = 2;
};
```

- 原則1 Round前に出現地点と敵カテゴリを予告する。Boss増援は2 Round前でもよい
- 出現はEnemy Phase開始時、通常敵行動前
- 初期版は出現直後に行動しない。次のEnemy Phaseから行動する
- Spawn Tileが塞がれていれば定義順で代替Tileを探す
- 全候補が塞がれていれば最大2 Round延期し、その後は出現失敗としてObjectiveへ通知する
- Unitを押し出したり重ねたりして強制出現させない
- `countsForElimination`の予告済み・待機中Waveは敵全滅勝利を保留する
- 1戦闘の増援上限は初期配置を除き8体。地域固有書はこれ以下を指定する
- SaveはWave ID、予告済み、残りRound、出現済みUnit ID、延期回数を保存する

## Objective追加種別

- `DefendObject`: 指定Round終了まで対象が生存
- `EscortUnit`: 対象が指定Exitへ到達し生存
- `EscapeUnits`: 必要人数がExitで行動終了
- `DestroyObject`: 指定Objectを破壊
- `ActivateObjects`: 指定数のObjectを操作
- `SurviveRounds`: 指定Round終了まで敗北しない
- `PreventEscape`: 指定対象の脱出を阻止

各Objectiveは退場理由を区別し、撃破、撤退、降伏、脱出を暗黙に同一視しない。

## Boss段階移行

1. Root Action解決後、勝敗評価前に閾値を検査
2. 撃破済みなら移行しない
3. 1 Root Actionで複数閾値を越えても次の未処理段階へ1回だけ移行
4. 段階移行時に予定行動をCancelするか維持するかBoss Definitionへ明記
5. 新しい予告行動は移行Event後に固定し、同じRoot Action中には実行しない
6. 段階移行はHPを回復しない。例外はUIで事前公開する固有能力だけ
7. Boss撤退は`Withdrawn`、撃破は`Defeated`として別Event・別報酬条件にする

## 反撃

通常攻撃への自動反撃はない。`反撃準備`など明示的な反応Skillだけが反撃する。
反応は1 Root Actionにつき各Unit最大1回、反応への反応は不可。範囲攻撃、地形Damage、状態Damageは
Skill Definitionが明示しない限り反撃Triggerにならない。

## 受入条件

- 同一Root ActionとSeedでEvent順が一致
- 同時全滅で敗北が優先される
- 反応で最後の敵を倒してもObjectiveと報酬が1回だけ処理される
- 塞がれた増援が延期・失敗し、Unitを重ねない
- 待機中増援を含む全滅条件が早期勝利しない
- Boss撃破時に不要な段階移行が発生しない
