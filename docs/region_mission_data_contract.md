# 62地点・地域任務データ契約

文書種別: **正本**
正本範囲: Region・Site・Choice・Battle・Mission・Reward・Camp・地域完了の横断型

状態: 正式仕様。接続そのものは`campaign_route_graph.md`、地点内容は`docs/regions/`、
この文書は探索、戦闘、報酬、地域完了を同じデータへ接続する契約を定める。

## 地域Definition

```cpp
struct RegionDefinition {
    RegionId id;
    TextKey nameKey;
    RegionRouteId routeId;
    FieldProfileId fieldProfileId;
    RegionId previousRegionId;
    DiscoveryId completionDiscoveryId;
    std::vector<ObjectiveId> requiredRegionObjectiveIds;
    std::optional<UnitId> finalBossUnitId;
    std::vector<SiteId> siteIds;
};
```

全10地域の地点数は`3,6,5,7,6,5,8,6,7,9`、合計62とする。

## 地点Definition

```cpp
struct SiteDefinition {
    SiteId id;
    RegionId regionId;
    RouteNodeId routeNodeId;
    TextKey nameKey;
    TextKey situationKey;
    FieldProfileId fieldProfileId;
    std::vector<ExplorationChoiceId> explorationChoiceIds;
    BattleScenarioId battleScenarioId;
    MissionDefinitionId missionDefinitionId;
    RewardTableId firstClearRewardId;
    RewardTableId reconLootId;
    std::vector<ObjectiveId> secureRouteObjectiveIds;
    std::optional<CampId> campAfterVictory;
    std::vector<BattleMessageId> battleMessageIds;
};

各Siteは通常2択と兵種または探索道具による条件付き1択を持つ。主経路は条件付きChoiceなしでも
攻略可能にする。

## 探索Choice

```cpp
struct ExplorationChoiceDefinition {
    ExplorationChoiceId id;
    TextKey labelKey;
    TextKey resultKey;
    std::vector<ConditionDefinition> availability;
    ExplorationOutcomeDefinition outcome;
};

struct ExplorationOutcomeDefinition {
    int partyDamage;
    int enemiesRemoved;
    IntelLevel intelLevel;
    DeploymentRuleId deploymentRuleId;
    std::vector<BattleModifierId> battleModifierIds;
    std::vector<RewardEntry> conditionalRewards;
};
```

探索結果をBattleControllerへ直書きせず、BattleFactoryが`BattleModifier`へ変換する。

## Battle生成

`BattleScenarioDefinition`は次を参照する。

- 主・副Objective Group
- 敵RosterとAI Definition
- Field Profileと地形生成率
- 固定Object、配置禁止区域、増援Wave
- Boss Scriptと段階移行
- 勝利・敗北・撤退条件
- 開始時、増援、Boss予告、Objective更新のMessage

Random生成後も24 Tile Snapshotを保存し、再開時に再抽選しない。初期配置は通行不能地形の生成後に
合法位置へ補正し、地形封鎖によって味方4人または必須敵が配置不能にならないよう再抽選する。

## 報酬

- `firstClearRewardId`: 最初の勝利でだけ付与する通常素材、重要素材、Discovery候補
- `reconLootId`: 再調査で取得可能な通常素材だけ。初回通常素材の50〜70%
- `ObjectiveReward`: 対応Objective達成かつ戦闘勝利時だけ付与
- `BossReward`: `Defeated`または地域書で許可した`Withdrawn`を明示
- すべて`RewardGrantId`を持ち、Attempt内と恒久台帳で二重付与を防ぐ

安全通過はどの報酬表も実行しない。

## 安全通過条件

`SiteAccessState::Secured`へのPending昇格には次をすべて要求する。

1. Siteの戦闘または非戦闘主目的を達成
2. `secureRouteObjectiveIds`を達成
3. Siteを勝利状態で解決
4. 拠点へ安全帰還

単なる到達や敵全滅だけでは経路確保にしない。恒久化後は入口から連続する確保済み地点を一括通過できる。

## Camp

Camp Definitionは位置、1遠征1回効果、帰還可否、会話、次Node候補を持つ。
Camp利用とNode通過を分けて保存し、安全通過中に既知Campへ到達した場合は
「通過継続／Camp利用／拠点帰還」を選べる。

## 戦闘開始Message

各Siteに最低限、次をLocale Keyで定義する。

1. 地点名と任務状況
2. 主目的
3. 特殊地形またはObject効果
4. 増援またはBoss予告の有無
5. 探索Choiceによる戦闘補正

同じ内容を戦闘開始時に二重表示しない。Seed、内部ID、未発見情報は表示しない。

## 地域完了候補

```cpp
struct PendingRegionCompletion {
    RegionId regionId;
    RegionCompletionGrantId grantId;
    std::unordered_set<ObjectiveId> completedRequiredObjectives;
    BossResolution bossResolution;
};

enum class BossResolution { None, Defeated, Withdrawn, Escaped };
```

MissionFlowだけが最終地点解決後に候補を生成する。BattleControllerは地域名、次地域、報酬を知らない。

## 地域完了判定

1. 地域Definitionの必須Objectiveがすべて達成
2. 最終MissionがVictory
3. Boss条件がある場合、指定されたResolutionを満たす
4. 同じ`RegionCompletionGrantId`がAttempt台帳にない
5. `PendingRegionCompletion`を追加
6. 安全帰還Transactionで`completedRegionIds`とDiscovery Registryへ恒久登録
7. 次地域の解放条件を再評価
8. 完了報酬を1回だけ倉庫または受取保留へ追加

Bossが撤退しただけで完了できる地域は地域書で`Withdrawn`を許可する。指定がなければ
`Defeated`のみ有効。演出上の退却と戦闘データ上の撃破を混同しない。

## 二重付与防止

- Site、Objective、Boss、Regionごとに安定`RewardGrantId`を持つ
- Attempt台帳で同じ遠征中の結果再表示・Save再開を防ぐ
- Base台帳で帰還Transaction再試行・地域再訪を防ぐ
- 台帳へ記録するのは報酬と恒久進行のSaveが同時成功した時だけ
- 既に完了済みの地域を旧Save移行で補完しても完了報酬を生成しない

## 検証規則

1. 62 Site IDとRoute Node参照が一意かつ存在
2. 各Siteに3 Choice、Battle、Mission、初回報酬、再調査報酬、Messageが存在
3. 条件付きChoiceなしでも主経路を攻略可能
4. `reconLoot`にDiscovery、Key、Boss、Recruit、Region進行を含めない
5. Camp位置がRoute Graphと一致
6. 最終Siteから地域完了条件へ到達可能
7. Boss Resolution条件がBoss Scriptと一致
8. 全Grant IDが一意
9. 表示Keyが日本語辞書に存在
10. Graph、Mission、Rewardの参照循環や到達不能をCIで拒否

## 受入条件

- 各地域を同じLoaderとFactoryで開始でき、地域別の直書き分岐を追加しない
- 最終Boss撃破だけでは不足Objectiveのある地域を完了できない
- 最終戦後に敗北・撤退すると地域完了を恒久化しない
- 安全帰還で次地域が一度だけ解放される
- 再訪・再読込・結果画面再表示で報酬が増えない
- 未知Route/Nodeからの復旧でもBaseの恒久進行を失わない
