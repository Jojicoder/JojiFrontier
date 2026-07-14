# 遠征状態遷移・保存境界仕様

## 状態

設定確定。遠征準備から安全帰還・敗北までの画面状態、処理責務、Checkpoint、再開位置を定める正本。
資産分類は`expedition_rewards.md`、ルート進行は`route_graph_data.md`、結果表示は
`battle_results_screen.md`を参照する。

## 状態一覧

```cpp
enum class GameFlowState {
    Base,
    ExpeditionPreparation,
    RegionSelection,
    AtRouteNode,
    ExplorationChoice,
    ExplorationResolved,
    PreBattleDeployment,
    Battle,
    BattleResult,
    Camp,
    SafeReturnSummary,
    DefeatSummary,
};
```

raylibの表示ModalやTabは保存しない。`GameApp::Screen`は当面この状態をまとめて表現できるが、
分岐処理は`GameFlowState`相当の明示的な状態へ段階移行する。

## 正常遷移

```text
Base
-> ExpeditionPreparation
-> RegionSelection
-> AtRouteNode(Entrance)
-> ExplorationChoice
-> ExplorationResolved
-> PreBattleDeployment（必要な場合だけ）
-> Battle
-> BattleResult
-> Camp または AtRouteNode(Next)
-> SafeReturnSummary
-> Base
```

敗北:

```text
Battle -> DefeatSummary -> Base
```

安全通過:

```text
AtRouteNode(Entrance)
-> 連続する経路確保済みNodeを一括解決
-> 分岐 / 未解決Node / 条件未達Node / Campで停止
-> AtRouteNode(Next) または Camp
```

一括安全通過は通過履歴だけを更新し、HP、戦闘不能、Bag、Pending、装備、利用済みCamp効果を
変更しない。既知のCampでは「通過継続／Camp利用／帰還」を選べる。地点ごとの確認画面は出さない。

帰還信号弾:

```text
AtRouteNode（探索選択前） -> 帰還確認 -> SafeReturnSummary -> Base
```

## 状態ごとの責務

| 状態 | 入力 | 更新できる状態 | 離脱条件 |
|---|---|---|---|
| Base | 施設、会話、Storage | 恒久状態 | 準備開始 |
| ExpeditionPreparation | 4人、Loadout、6+2枠 | 準備Draftのみ | 検証成功 |
| RegionSelection | 解放済み地域 | 準備DraftのRegion | 遠征開始確定 |
| AtRouteNode | 次Node、安全通過、再調査、帰還信号弾 | Route Progress | 選択確定 |
| ExplorationChoice | 地点の通常2択・兵種1択 | 選択Draft | 選択確定 |
| ExplorationResolved | 固定済み結果 | HP、Item、Battle Modifier | 戦闘生成 |
| PreBattleDeployment | 許可範囲の配置 | 配置Snapshot | 全員配置確定 |
| Battle | 戦闘入力 | Battle State | Victory / Defeat |
| BattleResult | 表示と確認 | 更新不可 | 次状態保存成功 |
| Camp | Item、続行、帰還 | HP、Bag、Camp利用回数 | 選択確定 |
| SafeReturnSummary | 確定資産表示 | 恒久化Transactionのみ | 保存成功 |
| DefeatSummary | 損失表示 | 未使用品返却と遠征破棄 | 保存成功 |

## 遠征開始Transaction

1. 4人、Loadout、連携、消耗品6枠、探索道具2枠、地域解放を検証
2. `ExpeditionAttemptId`と遠征Seedを発行
3. 消耗品をStorageから遠征Bagへ移す
4. Unit、装備、Skill、特性をSnapshot化
5. `RouteProgressSnapshot.currentNodeId = entranceNodeId`
6. Checkpointを保存
7. 保存成功後だけ地域画面へ遷移

途中失敗時はStorage、Attempt、Checkpointをすべて開始前へ戻す。

## Checkpoint種別

```cpp
enum class ExpeditionCheckpointKind {
    AtRouteNode,
    ExplorationResolved,
    BattleResultResolved,
    Camp,
};
```

### AtRouteNode

- 次の探索選択前
- Route Progress、Party、Bag、Pending、使用済みCamp効果を保存
- 帰還信号弾を使える最後の安全状態

### ExplorationResolved

- 探索選択、事前ダメージ、Item消費、敵数補正、配置条件を適用した後
- 選択結果とBattle Seedを保存
- 再開時は探索選択をやり直さず、同じBattleを再生成
- 事前ダメージとItem消費を二重適用しない

### BattleResultResolved

- Outcome、Completed Objective、報酬台帳、Pending追加、Party HP、戦闘不能、Bag消費を保存した後
- 結果画面は保存済みSnapshotを読むだけ
- 再開時に報酬を再計算・再付与しない

### Camp

- Camp到着時とItem使用後
- 現在HP、戦闘不能、Bag、Pending、Route Progress、Camp利用回数を保存
- 再開時は同じCamp。次地点を自動開始しない

戦闘途中と配置途中は保存しない。終了時は直前の`ExplorationResolved`へ戻り、同じSeedとModifierから
再生成する。Battle中に消費したItemやHP変化はBattleResult確定前なら巻き戻る。

## 戦闘勝利Transaction

1. `BattleOutcome::Victory`を固定
2. Objective結果を固定
3. Reward Ledgerで未付与を確認
4. Pending 4分類へ追加
5. Party HP、戦闘不能、Bag消費を遠征Snapshotへ反映
6. Route Nodeを解決済みにする
7. 次Edgeを評価
8. `BattleResultResolved`を保存
9. 保存成功後に結果画面を表示

どこかで失敗した場合はTransaction全体を再試行し、部分的なPendingやNode解決を保存しない。

## Camp遷移

- Camp到着だけでHPを回復しない
- 使用したItemは即Checkpointへ反映する
- 「遠征を続ける」は次Nodeを決定し、`AtRouteNode`を保存してから遷移
- 「拠点へ帰還」は安全帰還Transactionへ進む
- Campから次回遠征を再開しない
- 1遠征1回の補給・回復は`usedCampEffects`で二重利用を防ぐ

## 安全帰還Transaction

1. Pending LootをStorageへ合算
2. Pending DiscoveriesをRegistryへ集合結合
3. Pending Region ProgressとSite Accessを恒久化
4. Pending Recruit Candidatesを加入可能候補へ移す
5. 地域完了を`completedRegionIds`へ追加し、次地域解放を再計算
6. 未使用Bag Itemと探索道具をStorageへ戻す
7. 全RosterのHPと戦闘不能を拠点状態へ回復
8. Attempt処理済みIDを記録
9. 恒久Saveを原子的に保存
10. 保存成功後にCheckpointを破棄してBaseへ遷移

保存失敗時はSummaryを閉じず、同じAttempt IDで再試行する。二重確定しない。

## 敗北・手動撤退

- Pending 4分類と未確定Route Progressを破棄
- 使用済み消耗品は戻さない
- 未使用Bag Itemと探索道具をStorageへ戻す
- 保護箱で保護済みの通常Lootだけ例外処理
- 全Rosterを拠点で完全回復
- Checkpointを破棄し、恒久Save成功後にBaseへ戻る
- 同じ遠征を再開せず、新しいAttemptは地域入口から開始する

## Save対象

```text
Attempt ID / Region ID / Seed
Checkpoint Kind
RouteProgressSnapshot
選択済みExploration Outcome
Pending 4分類 / Reward Ledger
Party Snapshot / Loadout Snapshot
Bag 6枠 / 探索道具2枠
使用済みCamp効果
Battle Result Snapshot（結果確定後のみ）
```

Hover、選択中Tab、Animation、Tooltip、Scroll位置は保存しない。

## 更新互換

- Graph変更はAliasとLegacy mappingを伴う
- Battle数値変更後も保存済みBattle Resultは再計算しない
- `ExplorationResolved`から再生成するBattleは保存済みModifierを使い、新しい探索定義で上書きしない
- Unit、Weapon、Skill IDが廃止された場合は明示的な置換表を使う
- 移行不能な遠征Checkpointだけを破棄し、恒久Base Saveは維持する

## 受入条件

1. 各状態から許可されていない操作を拒否する。
2. 探索事前ダメージとItem消費を再開時に二重適用しない。
3. 結果画面再表示で報酬を再付与しない。
4. Camp Item使用後の再開で使用前へ戻らない。
5. 安全帰還保存失敗時に一部資産だけ恒久化しない。
6. 敗北時に恒久Site Accessと過去の地域完了を失わない。
7. 戦闘途中終了は同じ探索結果・Seedで再生成する。
8. 新しい遠征は常に地域入口から開始する。
