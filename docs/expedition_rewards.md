# JOJIFrontier 遠征状態・報酬仕様

文書種別: **正本**
正本範囲: Pending分類、報酬所有権、安全帰還、敗北ロスト、Grant台帳

報酬確定を含む画面・Transaction順序は[`expedition_flow.md`](expedition_flow.md)、地域攻略成果から
次地域を解放する規則は[`region_unlocks.md`](region_unlocks.md)を参照する。

## 状態

共通仕様確定。遠征中の資産、消耗、地域進行、加入候補、安全帰還、敗北ロスト、報酬の
二重付与防止については本書を正本とする。戦闘内の達成判定は
[`mission_objectives.md`](mission_objectives.md)、戦闘不能と撤退は
[`defeat_and_retreat.md`](defeat_and_retreat.md)、保存媒体とSchemaは
[`save_system.md`](save_system.md)、結果表示は
[`battle_results_screen.md`](battle_results_screen.md)を参照する。

この仕様はJOJIFrontier固有のゲーム処理であり、World Bibleの共有正史を変更しない。

## 所有状態の分離

| 状態 | 所有者 | 内容 | 敗北 | 安全帰還 |
|---|---|---|---|---|
| 恒久状態 | `BaseState` | Storage、Discovery Registry、地域進行、加入済み仲間、施設 | 維持 | 更新 |
| 遠征準備 | `ExpeditionLoadout` | 選択した4人、消耗品6枠、探索道具2枠、装備 | 未使用品と探索道具を返却 | 未使用品と探索道具を返却 |
| 未確定状態 | `ExpeditionState` | Loot、Discovery、地域進行、加入候補、報酬台帳 | 破棄 | 恒久状態へ確定 |
| 戦闘状態 | `BattleState` | HP、戦闘不能、地形、Unit、Objective | 遠征終了 | 次地点へ引継ぎ |

恒久状態と未確定状態を同じコンテナへ入れない。画面表示用の合算値を保存データとして
書き戻さない。

## 遠征ID

```cpp
using ExpeditionAttemptId = std::string;
using BattleId = std::string;
using RewardLedgerKey = std::string;
```

- `expeditionAttemptId`は遠征開始を確定した時に新規発行し、帰還または敗北まで変更しない
- 同じ地域へ再出発した場合は新しいAttempt IDを発行する
- アプリ終了から同じ遠征を復元する場合は同じAttempt IDを使用する
- `battleId`は`attemptId + siteId + visitSequence`から決定論的に作る
- 表示名、言語、座標、乱数SeedだけをIDとして使わない
- Attempt IDとBattle IDは通常UIへ表示しない

## データモデル

```cpp
struct PendingRegionProgress {
    RegionId regionId;
    std::unordered_set<ObjectiveId> completedObjectives;
    std::unordered_set<SiteId> surveyedSites;
    std::unordered_set<SiteId> securedRoutes;
    bool bossDefeated = false;
};

struct ExpeditionState {
    ExpeditionAttemptId attemptId;
    RegionId regionId;
    std::uint64_t expeditionSeed = 0;
    std::vector<LootStack> pendingLoot;
    std::unordered_set<DiscoveryId> pendingDiscoveries;
    std::unordered_map<RegionId, PendingRegionProgress> pendingRegionProgress;
    std::unordered_set<RecruitId> pendingRecruitCandidates;
    std::unordered_set<RewardLedgerKey> rewardLedger;
    std::vector<ItemType> bag;
    std::vector<UnitExpeditionSnapshot> party;
    SiteId currentSiteId;
    int battlesWon = 0;
    int visitSequence = 0;
};
```

- `pendingLoot`は同じLoot IDを追加時に合算し、数量0以下を保存しない
- Discovery、Objective、地点、加入候補、台帳は集合で保持し、重複を許可しない
- パーティSnapshotはUnit ID、現在HP、戦闘不能状態を保持する
- 装備は遠征中に変更しないため、開始時Loadoutの参照またはSnapshotで固定する
- UIアニメーション、選択中ボタン、ホバー状態は遠征状態へ含めない

## Pendingの4分類

### Pending Loot

木材、鉄材、薬草、獣皮、消耗品、装備、キー素材など、数量を持つ資産。安全帰還時にStorageへ
同一IDごとに加算する。キー素材も数量資産でありDiscoveryへ入れない。

### Pending Discoveries

偵察資料、医療記録、技術資料、地域踏査記録など、消費・売却しない発見。安全帰還時に
Discovery Registryへ登録する。登録済みIDはPending追加時点で拒否する。

### Pending Region Progress

踏査済み地点、経路確保、地域Objective、ボス撃破など。戦闘中のObjective達成から直接恒久化せず、
戦闘勝利後にPendingへ追加し、安全帰還時に恒久RegionProgressへ集合結合する。

### Pending Recruit Candidates

救出、会話、護衛などの条件を満たした加入候補。安全帰還時に`BaseState`の恒久的な
`availableRecruitCandidates`へ移し、集会所の加入会話後に正式加入する。候補状態では編成、
施設配置、装備変更の対象にしない。安全帰還済み候補は後の遠征敗北で失わない。

## 持込品

- 遠征バッグは6枠。同一アイテムも1個で1枠を使う
- 探索道具は消耗品と分離した2枠。恒久取得済みの異なる道具を装備する
- 消耗品と探索道具の正式カタログは[`item_system.md`](item_system.md)を正本とする
- 遠征開始確定時に選択品を恒久在庫からバッグへ移す
- 在庫数を超える選択、7枠以上、未解放品は開始前検証で拒否する
- 使用したアイテムは即座にバッグから削除し、以後の戦闘でも補充しない
- 未使用品は安全帰還、敗北、手動撤退のどの場合もStorageへ戻す
- 使用済み品は敗北や撤退でも戻らない
- 戦利品として得た消耗品はPending Lootへ入り、同じ遠征中のバッグへ自動追加しない
- 地点固有の補給効果だけが例外としてバッグへ追加でき、6枠上限と補給回数を地点仕様で明示する
- 拠点帰還時にバッグを空にし、次の遠征では改めて6枠を選ぶ

## 報酬発生と台帳

```cpp
struct RewardLedgerEntry {
    ExpeditionAttemptId attemptId;
    BattleId battleId;
    ObjectiveId objectiveId;
    RewardId rewardId;
};
```

台帳キーは`attemptId / battleId / objectiveId / rewardId`の4要素で作る。地点のルート基本報酬は
`objectiveId`の代わりに予約ID`site_completion`を使用する。

1. 探索選択と戦闘開始時には報酬を追加しない
2. Objective達成時はProgressだけを更新する
3. Battle OutcomeがVictoryになった後、MissionFlowがCompleted Objectiveを収集する
4. 報酬キーが台帳にないことを確認する
5. 報酬を対応するPending分類へ追加する
6. 追加成功後に同じトランザクションで台帳キーを記録する
7. 結果画面は追加済み結果を読むだけで、報酬処理を再実行しない

同じキーが存在する場合は成功済みとして何も追加しない。ボタン連打、結果画面再表示、キャンプ
復元で報酬を二重付与しない。

## 戦闘後の処理

### 勝利

```text
Outcome確定
-> Completed Objectiveを固定
-> MissionFlowが報酬をPendingへ追加
-> HP・戦闘不能・バッグ消費を遠征状態へ反映
-> Battle結果をresolvedにする
-> キャンプまたは次の地点へ遷移
-> チェックポイント保存
```

- 戦闘勝利だけではStorageや恒久進行を更新しない
- 戦闘終了時に状態異常と一時効果を解除する
- HPと戦闘不能は同じ遠征の次戦闘へ引き継ぐ
- 地点報酬の表示後にアプリを終了しても、同じ報酬を再追加しない

### 敗北・手動撤退

- Pending 4分類と報酬台帳を破棄する
- 未使用のバッグ品だけStorageへ返却する
- 消費済みアイテムは返却しない
- 恒久状態を変更しない
- 拠点へ戻し、全員のHPと戦闘不能を完全回復する
- 同じ遠征の再開はできず、次回は入口から新しいAttemptとして開始する

### 安全帰還

安全帰還はキャンプの「拠点へ帰還」、地域終了後の帰還、または
[`item_system.md`](item_system.md)で許可された地点間・探索選択前・キャンプで使用する帰還信号弾だけ。
戦闘メニューの撤退を安全帰還へ変換しない。

```text
Pending Loot -> Storageへ合算
Pending Discoveries -> Registryへ集合結合
Pending Region Progress -> 恒久RegionProgressへ集合結合
Pending Recruit Candidates -> 恒久的な加入可能候補へ登録
未使用バッグ品 -> Storageへ返却
全員 -> HP全回復・戦闘不能解除
遠征チェックポイント -> 破棄
恒久セーブ -> 1回のトランザクションで保存
```

- 4分類すべての検証に成功してから恒久状態を更新する
- 途中の1分類だけ確定した状態を保存しない
- 同じAttemptの帰還処理済みIDを恒久側に保持し、クラッシュ復旧時の二重確定を防ぐ
- 帰還保存に失敗した場合は結果画面を閉じず、同じAttemptで再試行する

## 周回と再訪

- ゲーム上の新しい遠征は常に地域入口から始まる
- アプリ中断からの復元は新しい周回ではなく、同じ遠征の続行として扱う
- 安全通過では報酬を付与しない
- 危険区域の再調査では地点仕様が許可した通常Lootだけ再取得できる
- Discovery、加入、ボスキー素材、初回地域Objectiveは恒久取得後に再付与しない
- 敗北した遠征の未確定踏査や経路確保は次回へ引き継がない
- 以前の安全帰還で確定済みの地点状態は敗北しても失わない

## UI表示

- 日本語UIでは「未確定戦利品」「重要発見」「地域成果」「加入候補」と分類して表示する
- 「荷物」は持込品だけを表示し、未確定戦利品と混ぜない
- キャンプでは現在HP、戦闘不能、バッグ残数、Pending 4分類、次地点情報を表示する
- 安全帰還確認では確定する全資産を表示する
- 撤退確認では失うPendingと返却される未使用品を分けて表示する
- 敗北画面では失った資産と、拠点で全回復したことを表示する
- 内部ID、Attempt ID、Battle ID、乱数Seedを通常UIへ表示しない
- 戦闘結果画面は報酬処理を行わず、追加済みSnapshotだけを表示する

## 保存対象

中断チェックポイントへ最低限次を保存する。

- Attempt ID、地域ID、地点ID、訪問番号、遠征Seed
- Pending 4分類と報酬台帳
- バッグ内容
- パーティのHPと戦闘不能
- 戦闘勝利数
- 選択済み探索結果と生成に必要なBattle Seed
- チェックポイント種別とActive Battle Marker

現行の簡略チェックポイントに存在しないAttempt ID、Pending Region Progress、加入候補、報酬台帳は
追加時にSave Schemaを更新する。古いセーブには空集合と新しいAttempt IDを安全な初期値として与える。

## 検証条件

1. Objective達成だけでは報酬を得ない
2. 勝利後だけ各Pending分類へ追加される
3. 同じ勝利結果を2回処理しても数量が増えない
4. 同じReward IDでもBattle IDが異なれば再訪許可報酬を追加できる
5. 登録済みDiscoveryと加入済み仲間をPendingへ重複追加しない
6. 敗北と撤退でPending 4分類をすべて失う
7. 未使用持込品は敗北時も戻り、使用済み品は戻らない
8. 安全帰還で4分類が一度だけ恒久化される
9. 帰還保存の再試行で資産が二重化しない
10. 安全通過で報酬を得ない
11. 中断復元後もAttempt ID、Pending、台帳が一致する
12. 新しい遠征は入口から始まり、新しいAttempt IDを持つ

## 実装境界

`BattleController`は報酬を知らない。`ObjectiveTracker`は達成状態だけを返す。MissionFlowが
Objective報酬をPendingへ変換し、GameAppが遠征遷移と安全帰還を管理する。描画コードは状態を
読むだけで、資産追加、破棄、確定を行わない。
