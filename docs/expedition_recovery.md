# 地域内セーブ・再開・復旧仕様

文書種別: **正本**
正本範囲: 地域内Checkpoint、復元順、Node変更時の退避、不正Route隔離

状態: 正式仕様。保存媒体、Schema、Exportは`save_system.md`、通常遷移は`expedition_flow.md`を参照する。

## 保存可能地点

- 探索地点へ到着し、選択前の`AtRouteNode`
- 探索結果確定後かつ戦闘生成完了後の`ExplorationResolved`
- 戦闘勝利と報酬解決後の`BattleVictoryResolved`
- Campで各操作完了後の`Camp`

戦闘途中、敵Phase途中、配置操作途中、確認Dialog表示中は保存しない。終了時は最後の完全な
Checkpointへ戻る。

## Checkpoint内容

- Attempt ID、Region ID、Route ID、Current Node ID、Checkpoint Kind
- Seed、生成器Version、生成済み24 Tile
- Party Snapshot、HP、戦闘不能、装備、Skill回数
- Bag、探索道具、使用済みItem、保護対象
- Pending 4分類とReward Grant Ledger
- Route Progress、分岐、解決済みNode、安全通過履歴、利用済みCamp
- 確定済み探索結果、敵編成、初期配置、増援Schedule

一時UI状態、Hover、Animation Frame、AI予約は保存しない。

## 復元順

1. Save Schemaを移行
2. Region Alias、Route Alias、Node Aliasを解決
3. Route Graphを検証
4. Inventory、Party、Pendingを検証
5. Current NodeとCheckpoint Kindを検証
6. 生成器Versionが一致すればSnapshotを使用。再抽選しない
7. 一致しなくても保存済み24 Tileと敵Snapshotが完全ならそのまま使用
8. UI状態をCheckpoint Kindから再構築

## 更新後の復旧

優先順位:

1. 正式IDが存在する: その地点へ復元
2. Node Aliasが存在する: 新IDへ変換して復元
3. `lastCheckpointNodeId`が有効: そこへ退避
4. RegionとRouteが有効: 同じAttemptのPendingと消耗を維持して地域入口へ退避
5. Regionが無効: Attemptを復旧保留にし、拠点へ戻す。ただしPendingを恒久化しない

入口退避ではHP、戦闘不能、Bag、Pendingを巻き戻さない。解決済みNodeはGraph上に存在するIDだけ
保持し、未知IDを診断ログへ残す。入口退避で報酬、回復、安全通過状態を追加しない。

## 不正Route ID

- 恒久Base Saveは維持する
- 該当Checkpointだけを`QuarantinedExpedition`へ移す
- 「遠征を破棄して拠点へ戻る／Importから復旧」を表示する
- 遠征破棄ではPendingを失い、未使用持込品を倉庫へ返す
- 自動的に地域完了やDiscoveryへ変換しない
- 未知IDをCinderwatchなど別地域へ暗黙変換しない

## 受入条件

- 全Checkpoint種別でHP、戦闘不能、Bag、Pending、Current Nodeが一致
- 再読込で地形と増援が再抽選されない
- Node改名Aliasで同じ地点へ復元できる
- Node削除時に最後の有効地点、次に入口へ安全退避できる
- 不正RouteでBase Saveを失わない
- 復旧を繰り返して報酬や未使用品が増えない
