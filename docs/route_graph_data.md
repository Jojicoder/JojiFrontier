# 地域ルートグラフ・データ仕様

文書種別: **正本**
正本範囲: Route Node、Edge、Condition、進行Snapshot、Graph検証、移行

## 状態

設定確定。`campaign_route_graph.md`に定義された10地域62地点を、C++、Save、UIで共通利用するための
データ契約を定める。地点内の戦闘内容は地域専用書、地域の解放順は`region_unlocks.md`、画面遷移と
保存境界は`expedition_flow.md`を正本とする。

実装状況(2026-07-13): 灰枝の森の直線経路に必要な最小部分(`Entrance`/`Site`/`Camp`/`Exit`、
無条件Edge、遠征Snapshot、Save)を実装済み。`BranchGroup`、条件付きEdge、Variant、Camp効果、
旧ID Aliasは、それらを使う次地域を実装する時に追加する。以下の完全モデルが引き続き最終契約である。

## 安定ID

```cpp
using RegionRouteId = std::string;
using RouteNodeId = std::string;
using SiteId = std::string;
using CampId = std::string;
using BranchGroupId = std::string;
using RouteVariantId = std::string;
using RouteConditionId = std::string;
```

- IDはASCIIの`snake_case`で、表示名、順番、座標を含めない
- `RouteNodeId`は地域内で一意。Save上は`regionId + nodeId`で識別する
- 地点表示名変更で`SiteId`を変えない
- 廃止IDは`routeIdAliases`へ残し、最低2つのSave Schema世代を移行可能にする
- `stageIndex`は移行専用とし、新規Saveと進行判定へ使用しない

## 定義モデル

```cpp
enum class RouteNodeKind {
    Entrance,
    Site,
    BranchGroup,
    Camp,
    Exit,
};

enum class BranchCompletion {
    AnyMember,
    AllMembers,
};

struct RouteNodeDefinition {
    RouteNodeId id;
    RouteNodeKind kind;
    TextKey nameKey;
    std::optional<SiteId> siteId;
    std::optional<CampId> campId;
    std::optional<BranchGroupId> branchGroupId;
    std::vector<RouteNodeId> branchMembers;
    BranchCompletion branchCompletion = BranchCompletion::AllMembers;
    bool allowsReturnFlareBeforeEntry = false;
    bool repeatableAfterRegionClear = false;
};

enum class RouteConditionKind {
    Always,
    NodeResolvedThisAttempt,
    AllNodesResolvedThisAttempt,
    AnyNodeResolvedThisAttempt,
    SiteAccessAtLeast,
    ObjectiveCompletedThisAttempt,
    DiscoveryRegistered,
    RegionCompleted,
};

struct RouteCondition {
    RouteConditionId id;
    RouteConditionKind kind;
    std::vector<std::string> targetIds;
    SiteAccessState minimumAccess = SiteAccessState::Unknown;
};

struct RouteEdgeDefinition {
    RouteNodeId from;
    RouteNodeId to;
    std::vector<RouteConditionId> allOf;
    std::vector<RouteConditionId> anyOf;
};

struct RegionRouteGraph {
    RegionId regionId;
    RegionRouteId routeId;
    RouteNodeId entranceNodeId;
    RouteNodeId exitNodeId;
    std::vector<RouteNodeDefinition> nodes;
    std::vector<RouteCondition> conditions;
    std::vector<RouteEdgeDefinition> edges;
    std::unordered_map<int, RouteNodeId> legacyStageIndexMapping;
    std::unordered_map<RouteNodeId, RouteNodeId> routeIdAliases;
};
```

`TextKey`は`localization.md`のLocale Key。表示文をGraphへ直書きしない。

## 分岐と合流

地点3・4を任意順で攻略する地域は`BranchGroup`を使う。

```text
branch_1
  members: [site_3, site_4]
  completion: AllMembers
```

1. Branchへ入ると、現在の遠征で未解決のMemberを選択する
2. Member解決後は同じBranchへ戻る
3. `AllMembers`成立後だけ後続Edgeを有効にする
4. 片方だけ解決した状態で直前Campへ戻り、安全帰還できる
5. 次の遠征では恒久`SiteAccessState`を読み、確保済みMemberを安全通過として解決できる

灰鉄採石場の旧採掘坑／巻上機区画は別Siteではない。同じSite Nodeの`RouteVariantId`として扱い、
取得済みVariantだけを恒久集合へ保存する。

## 遠征中の進行Snapshot

```cpp
struct RouteProgressSnapshot {
    RegionRouteId routeId;
    RouteNodeId currentNodeId;
    RouteNodeId lastCheckpointNodeId;
    std::unordered_set<RouteNodeId> resolvedNodeIds;
    std::unordered_set<RouteNodeId> safelyPassedNodeIds;
    std::unordered_set<RouteVariantId> resolvedVariantIds;
    std::unordered_set<CampId> usedCampEffects;
    std::vector<RouteNodeId> traversalHistory;
};
```

- `resolvedNodeIds`は現在の遠征だけ。敗北・撤退で破棄する
- 恒久進行は`BaseState::siteAccess`と`completedRegionIds`へ分離する
- `safelyPassedNodeIds`は報酬禁止とUI履歴のために保持する
- `usedCampEffects`は1遠征1回の補給・回復を二重利用させない
- `traversalHistory`は診断と結果表示用。進行条件の正本にはしない
- 現在Nodeを単純な整数へ戻さない

## Node解決

```cpp
enum class NodeResolutionKind {
    BattleVictory,
    ObjectiveResolution,
    SafePassage,
    NonCombatEvent,
};

struct NodeResolution {
    RouteNodeId nodeId;
    NodeResolutionKind kind;
    std::unordered_set<ObjectiveId> completedObjectives;
};
```

1. Node開始時に到達条件を検証する
2. 探索結果を固定して戦闘または非戦闘処理を実行する
3. MissionFlowがObjective、報酬、Pending進行を一度だけ解決する
4. `resolvedNodeIds`へ追加する
5. Edge条件を再評価し、選択可能な次Nodeを提示する

`SafePassage`は戦闘勝利数へ加算しない。報酬、Discovery、Objective、加入候補も発生させない。
補給Nodeの利用はNode解決と`usedCampEffects`を別に記録する。

Site、探索Choice、Battle、Mission、報酬、安全通過条件、Camp、戦闘Messageの横断契約は
[`region_mission_data_contract.md`](region_mission_data_contract.md)を正本とする。

## Graph検証

起動時とTestで次を検証し、不正Graphでは遠征開始を拒否する。

1. Node ID、Condition IDが地域内で重複しない
2. EntranceとExitが各1つ存在する
3. Edgeの参照先、Condition、Branch Memberが存在する
4. 全SiteがEntranceから到達可能で、地域攻略条件からExitへ到達可能
5. 条件なしの無限Cycleがない
6. Branch MemberからBranchへ戻れる
7. Camp数とSite数が`campaign_route_graph.md`の正式数と一致する
8. 全10地域のSite数が`3,6,5,7,6,5,8,6,7,9`、合計62
9. 最終地点を初回から安全通過できない
10. Return Flare不可区間が戦闘・強制追跡をまたいで開かない

## Saveと移行

正式実装はSchema 3へ含める。施設の建設済み／稼働中移行もSchema 3で同時に行い、別のSchema 3を
作らない。

追加項目:

- `ExpeditionCheckpoint.routeProgress`
- `BaseState.completedRegionIds`
- `BaseState.securedRouteVariantIds`

Schema 2の`expeditionStage`は`legacyStageIndexMapping`で`currentNodeId`へ変換する。対応表がない、
Node Aliasでも解決できない、Graph検証に失敗する場合はCheckpointを自動継続しない。恒久Saveを維持した
まま「遠征データを破棄して拠点へ戻る」を提示し、Pendingを恒久化しない。Release前に全公開Versionの
移行Fixtureを通し、この復旧が通常更新で発生しないことを保証する。

## 受入条件

- 灰枝の森3地点を同じGraphで順に進める
- 地点2後に帰還し、次回は入口から地点1・2を安全通過できる
- 2地点Branchを任意順で解決し、片方だけ持ち帰れる
- Save復元後も現在Node、Branch進捗、Camp利用回数が一致する
- 安全通過が戦闘勝利数と報酬を増やさない
- 同じ基盤へ沈黙した監視所群6地点をデータ追加できる
