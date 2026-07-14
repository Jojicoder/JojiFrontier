# 敵増援共通仕様

## 状態

共通仕様確定。地域固有書は増援の編成、Round、入口、阻止条件だけを定義し、出現処理、
Objective、上限、Save復元は本書に従う。

## データモデル

```cpp
enum class ReinforcementState {
    Scheduled,
    Announced,
    Spawned,
    Prevented,
    Cancelled,
};

struct ReinforcementWave {
    ReinforcementWaveId id;
    Team team;
    int spawnRound;
    Phase spawnPhase;
    bool requiredForElimination;
    int announceRoundsBefore = 1;
    std::vector<UnitSpawnDefinition> units;
    std::vector<GridPosition> orderedSpawnCandidates;
    ReinforcementState state;
};
```

ID、編成、候補マス順、出現Roundは戦闘開始時に固定する。ランダム候補を使う場合もBattle Seedから
開始時に確定し、中断復元時に再抽選しない。

## 予告

- 原則として出現の1ラウンド前に予告する
- 強力な増援、複数方向、Boss連携は2ラウンド前予告を地域書で指定できる
- 予告には出現Round、方向、予定マスを表示する
- 偵察不足で兵種を隠しても「敵影あり」と方向は隠さない
- Eventや装置操作で増援が起動した場合、最短でも次の該当Enemy Phaseに出現し、同じAction中には出さない
- 予告なしで出現して即攻撃する増援は使用しない

`Scheduled -> Announced`でメッセージを1回だけ表示する。Save復元後に同じ予告を初回扱いで再表示しない。

## 出現処理

増援は指定Phase開始処理で、`PhaseStarted`の直前に解決する。

1. 阻止・取消条件を確認
2. `orderedSpawnCandidates`を順番に調べる
3. 通行可能、Unit非占有、非Objectiveマスへ配置。対応する`SpawnPoint`との同居は許可し、
   それ以外のObjectがある候補は使用しない
4. 配置したUnitをそのPhaseは行動済みにする
5. `ReinforcementResolved(Spawned)`を発行
6. 次の同Team Phaseから通常行動可能にする

出現直後の移動、攻撃、スキル使用は行わない。これにより予告マスを塞ぐ、離れる、迎撃姿勢を
整える判断を保証する。

## 出現地点が塞がれた場合

- 同じ入口グループの候補マスを定義順に使用する
- 別の行、別の入口へ無断で変更しない
- 全候補が塞がれていればそのWaveを`Prevented`として確定する
- 次Roundへ自動延期しない
- `ReinforcementResolved(Prevented)`を発行する
- 一時的なUnit占有でも阻止成立とする。ただし候補マスは戦闘前に公開する

地域書で「封鎖不可」とする物語上の侵入口は、通常Unitが停止できないBattle Objectとして最初から
表現する。出現時だけ既存Unitを押し退けたり消去したりしない。

## 全滅Objective

- `requiredForElimination = true`のWaveは`Scheduled`または`Announced`中も未解決敵として数える
- `Spawned`後は出現Unitの生存数で判定する
- `Prevented`または`Cancelled`になれば未解決数から除外する
- 任意増援、探索で無効化済みのWave、勝利後演出は全滅条件へ含めない
- 現在の敵が0でも必須Waveが未解決なら勝利にしない
- 未公開増援が勝利を止める場合もHUDへ「敵影あり」を表示する

## 上限

- 1Wave最大4体
- 通常戦闘1回の増援予定は合計8体まで
- 防衛任務だけ地域書の明示で合計12体まで拡張可能
- 同時に盤面上へ存在できるUnit数は、初期配置・増援を含めて通行可能マス数を超えない
- 無限増援は本編で使用しない
- 上限超過はデータ読込時エラーとし、実行中に黙って切り捨てない

## Save復元

中断Saveには次を保存する。

- Wave IDと`ReinforcementState`
- 出現Round、Phase、必須・任意区分
- 確定済み編成と候補マス順
- 予告表示済みフラグ
- 出現済みUnitの安定ID、HP、状態、位置、行動済み状態
- `ReinforcementResolved`のEvent IDまたは消費台帳

盤面スナップショットを正本として復元し、生成率や最新乱数から増援を再生成しない。Schema更新で
Unit IDが変わった場合はSave移行表を使い、復元不能なら直前の探索Checkpointへ戻す。

## 地域固有差分

地域書で変更できるもの:

- Wave編成
- 出現RoundとPhase
- 1または2ラウンド前予告
- 入口候補マス
- 必須・任意区分
- 装置、探索選択、副目標による阻止・取消

出現直後行動、全滅Objectiveの判定方式、Save形式、最大12体のHard Limitは変更できない。

## 受入条件

- 1ラウンド前に予告され、出現Phaseには行動しない
- 第1候補が塞がれていると第2候補へ出現する
- 全候補封鎖で`Prevented`となり、必須増援待ちが解除される
- 必須増援予告中は現在敵0でも勝利しない
- 任意増援は敵全滅勝利を停止しない
- Save復元後にWaveが二重出現せず、予告も重複通知されない
- 上限超過データを開始前に拒否する
