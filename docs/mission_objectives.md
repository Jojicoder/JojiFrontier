# JOJIFrontier 任務目標・副目標システム

## 状態

共通仕様確定。`implementation_roadmap.md`のPhase 1前半を一部実装済み(`jf/battle/BattleEvents.hpp`、
`jf/battle/Objective.hpp`、`jf/battle/ObjectiveTracker.hpp`)。戦闘不能、敗北、撤退後の資産処理は
[`defeat_and_retreat.md`](defeat_and_retreat.md)を正本とする。この文書は戦闘内で何を
達成・失敗と判定し、どの報酬へ接続するかを定義する。
戦闘終了後の表示と遷移は[`battle_results_screen.md`](battle_results_screen.md)を正本とする。
遠征内のPending分類、安全帰還、報酬台帳の共通仕様は
[`expedition_rewards.md`](expedition_rewards.md)を正本とする。

実装済み: `ObjectiveKind`のうち`EliminateTeam`/`DefeatUnit`/`SecureTile`、AND/ORグループ
（1段のみ）、`BattleEvent`のうち`ActionResolved`/`UnitDefeated`/`PhaseStarted`/`PhaseEnded`/
`RoundEnded`、`evaluateBattleOutcome()`による「敗北を主目的より先に評価」。
`BattleController`は`allEnemiesDefeated()`/`allPlayersDefeated()`を直接見ず、この評価結果だけで
Victory/Defeatへ遷移する。デフォルトミッション(敵全滅のみ)は既存の挙動と完全互換。

- `SecureTile`は`target.securingTeam`と一致する側の`ActionResolved`だけを達成対象にする
- 存在しない`DefeatUnit`対象は未達成として扱う（自動勝利にしない）
- ORグループは実際に達成した目標だけを`completedPrimaryObjectives`へ入れ、未達側は
  `ObjectiveProgress.status`を`Superseded`にする（Activeのまま放置しない）
- `evaluateBattleOutcome()`が`EliminateTeam`/`DefeatUnit`達成時に`ObjectiveProgress.status`を
  `Completed`へ同期する
- `UnitDefeatedEvent`は`captureAliveSnapshot()`/`emitUnitDefeatedEvents()`により、戦闘・状態異常
  どちらが原因でも生存→戦闘不能の遷移ごとに1回だけ実発行する（`BattleController::confirmAttack()`、
  `finishPlayerAction()`、`EnemyAI`の攻撃・行動終了、Phase終了時状態異常のすべてから）
- Phase終了処理の順序を仕様どおりに修正: 状態異常ダメージ（と、それによる戦闘不能・
  `UnitDefeatedEvent`）を確定してから`PhaseEnded`/`RoundEnded`を発行する
- `syncObjectiveProgress()`/`evaluateBattleOutcome()`を分離した:
  前者だけが`ObjectiveProgress`を変更し、後者は確定済み状態を読むだけの純関数
- ORグループは同一Batchで複数条件が成立しても、Definition配列順で最初の1件だけを
  `Completed`にし、残りを`Superseded`にする（既に決着した後の再同期でも取り替えない）
- `validateBattleMission()`: ID重複、未参照Group、主目的Group数、`DefeatUnit`対象の
  存在・Team一致、`SecureTile`の盤内・通行可能・非占有、`ObjectiveProgress`の1:1初期化を
  検証する。`BattleFactory`から毎回呼び出し、問題があればstderrへ出力する
  （専用のミッション開始画面はまだ無いため、本番ビルドでもクラッシュさせず診断表示のみ）

未実装: 残り7種のObjectiveKind（`BattleObjectState`が必要）、隠し副目標の公開条件、
`MissionFlow`による報酬のPending変換と二重付与防止台帳、戦闘開始画面・HUD・結果画面、
`ActionKind`別の細かい対象条件、複数の主目的グループをまたぐAND/OR、検証失敗時の
専用リカバリ画面（現状はstderr診断のみ）。

## Phase 1確定仕様

以下はPhase 1実装の判断基準とし、地域別データで上書きしない。

### 初期3種の条件

- `EliminateTeam`は`target.team`の生存Unitが0になった時に達成する
- Phase 1では増援を扱わない。増援実装後は未解決の必須増援も0であることを追加条件とする
- `DefeatUnit`は指定Unitが戦闘開始時に存在し、生存から戦闘不能へ変化した時に達成する
- 指定Unit IDが存在しない、重複する、対象Teamが一致しない定義は戦闘生成エラーとする
- 定義エラーをObjective達成、通常勝利、対象なし戦闘へ読み替えない
- `SecureTile`のPhase 1既定許可陣営は`Team::Player`とする
- `SecureTile`は許可陣営の生存Unitが、攻撃、スキル、アイテム、待機を確定して行動終了した時に達成する
- 移動だけ、キャンセル、強制移動、敵の行動終了では達成しない
- Phase 1では兵種制限を付けない。専用調査は将来の`OperateObject`で表現する

### ORグループ

- `Any`は最初に達成したObjectiveだけを`Completed`として完了IDへ追加する
- 同じBatch内で複数条件が成立した場合は、Definition配列順で最初の1件を採用する
- 採用されなかった同グループの`Active` Objectiveは`Superseded`へ変更する
- `Superseded`は失敗ではなく、報酬、地域進行、達成数の対象外とする
- すでに`Completed`のObjectiveを後続イベントで置き換えない
- Phase 1の主目的グループは1つだけとする。複数主目的グループの直下論理は後続Phaseで追加する

### 進捗同期

- 勝敗評価の前に、成立したすべてのObjectiveを`Completed`へ同期する
- `evaluateBattleOutcome()`は状態を変更しない。進捗更新と勝敗評価を別処理に分ける
- 結果画面と報酬処理は再計算せず、確定済み`ObjectiveProgress.status`を参照する
- 完了IDは`Completed`だけから生成し、`Failed`、`Superseded`、`Hidden`を含めない

### イベントBatch

- 1回の行動、Phase終了処理、戦闘開始処理をそれぞれ1つのBatchとする
- Batch内では状態変更を先に完了し、その結果を表すイベントを発生順に並べる
- `UnitDefeated`は各Unitについて生存から戦闘不能へ変化した瞬間に1回だけ発行する
- 行動Batchでは戦闘不能などを発行した後、最後に`ActionResolved`を発行する
- Phase終了Batchでは状態異常、地形効果、戦闘不能を解決してから`PhaseEnded`を発行する
- Enemy Phase終了時だけ`PhaseEnded`の後に`RoundEnded`を発行する
- Objective更新と勝敗評価はBatch内の全イベント処理後に1回だけ行う
- 同じBatchで敗北と勝利が成立した場合は敗北を採用する
- 敗北時は同じBatchで副目標がCompletedになってもPending報酬を付与しない

### 戦闘開始時の検証

BattleFactoryは入力完了後、戦闘開始前に次を検証する。

1. Objective ID、Group ID、Unit IDが必要な範囲で一意
2. 全Objectiveが存在するGroupを参照している
3. 主目的Groupが1つ存在し、空ではない
4. `DefeatUnit`の対象Unitが存在し、Teamが一致する
5. `SecureTile`が盤面内、通行可能、初期配置で占有されない
6. `ObjectiveProgress`が全Definitionについて1件ずつ初期化されている

検証失敗時は戦闘を開始せず、開発ビルドではID付きエラーを表示する。製品ビルドでは安全な
既定戦闘へ自動変換せず、ミッション開始画面へ戻して日本語の読込エラーを表示する。

## 設計方針

- 主目的は戦闘を終わらせる条件、副目標は追加報酬や地域進行を得る条件とする
- 主目的と敗北条件は戦闘開始前からすべて公開する
- 副目標は発見済みのものを公開し、探索結果で秘匿されたものだけ`???`表示を許可する
- 副目標失敗だけでは戦闘を敗北にしない
- 条件達成の瞬間と報酬確定の瞬間を分離する
- 達成した報酬は戦闘勝利後にPendingへ入り、安全帰還まで恒久化しない
- 目標判定を地域固有のUIや`BattleController`へ直接埋め込まない

## 目標の種類

| 種類 | ID | 達成条件 | 主目的 | 副目標 |
|---|---|---|---:|---:|
| 敵全滅 | `EliminateTeam` | 指定Teamの生存ユニットと未解決の必須増援が0 | Yes | No |
| 指揮官撃破 | `DefeatUnit` | 指定Unit IDが戦闘不能または任務上の撤退 | Yes | Yes |
| 地点確保 | `SecureTile` | 条件を満たすユニットが指定マスで行動終了 | Yes | Yes |
| 複数地点確保 | `SecureTiles` | 指定数の異なる対象マスを確保 | Yes | Yes |
| 防衛 | `SurviveRounds` | 指定ラウンド終了まで敗北条件を回避 | Yes | Yes |
| 脱出 | `EscapeUnits` | 条件を満たす必要人数が脱出マスで行動終了 | Yes | Yes |
| 装置操作 | `OperateObject` | 指定ユニット・兵種が装置で専用行動を完了 | Yes | Yes |
| 設置物破壊 | `DestroyObject` | 指定Object IDまたはタグの耐久が0 | Yes | Yes |
| 対象保護 | `ProtectUnit` | 戦闘終了時に指定対象が撤退していない | No | Yes |
| 条件付き撃破 | `DefeatWithCondition` | 指定対象を地形衝突などの条件成立後に撃破 | No | Yes |

`SecureTile`は到達だけでは達成しない。対象マスで攻撃、スキル、アイテム、待機のいずれかを
確定して行動終了した時点で達成する。専用調査が必要な場合は`OperateObject`を使い、通常の
行動終了では達成させない。

## データモデル

```cpp
enum class ObjectiveKind {
    EliminateTeam,
    DefeatUnit,
    SecureTile,
    SecureTiles,
    SurviveRounds,
    EscapeUnits,
    OperateObject,
    DestroyObject,
    ProtectUnit,
    DefeatWithCondition
};

enum class ObjectiveStatus {
    Hidden,
    Active,
    Completed,
    Failed,
    Superseded
};

enum class ObjectiveGroupRule {
    All, // AND
    Any  // OR
};

struct ObjectiveDefinition {
    ObjectiveId id;
    ObjectiveKind kind;
    bool primary = false;
    bool required = false;
    bool hiddenUntilRevealed = false;
    ObjectiveGroupId groupId;
    ObjectiveTarget target;
    ObjectiveRequirement requirement;
    std::vector<RewardDefinition> rewards;
};

struct ObjectiveGroupDefinition {
    ObjectiveGroupId id;
    ObjectiveGroupRule rule = ObjectiveGroupRule::All;
    bool primaryGroup = false;
};

struct ObjectiveProgress {
    ObjectiveId id;
    ObjectiveStatus status = ObjectiveStatus::Active;
    int current = 0;
    int required = 1;
    std::unordered_set<std::string> creditedTargetIds;
};
```

- `ObjectiveDefinition`はミッションデータとして不変
- `ObjectiveProgress`だけを戦闘状態として更新する
- 文字列表示、報酬、地域進行をObjective IDの条件分岐でハードコードしない
- 同じ対象を複数回調査して`current`を増やせないよう`creditedTargetIds`を保持する
- 目標マスは地形生成後に有効候補から決定し、決定した座標を戦闘Seedと共に保存する

## 対象条件

`ObjectiveRequirement`は必要な条件だけを指定する。

- 許可Team
- 特定Unit ID
- 許可兵種または禁止兵種
- 生存状態
- 行動終了が必要か
- 専用操作コマンドが必要か
- 必要ラウンド数または必要人数
- 対象が初期配置済みか増援予定か
- 探索ルートによる有効化条件

条件を満たさないユニットが目標マスで行動終了しても、目標は進行しない。UIには
「暁の衛生兵が調査する必要があります」など、満たしていない理由を表示する。

## 判定イベント

目標システムは次の戦闘イベントを受け取って更新する。

| イベント | 主な用途 |
|---|---|
| `ActionResolved` | 地点確保、装置操作、脱出 |
| `UnitDefeated` | 敵全滅、指揮官撃破、護衛失敗 |
| `UnitRetreated` | 指揮官撃破、護衛失敗、脱出 |
| `ObjectDestroyed` | 障害物・装置目標、条件付き撃破 |
| `TerrainCollision` | 倒木衝突、罠誘導 |
| `ReinforcementResolved` | 必須増援の出現または出現不能確定 |
| `PhaseStarted` | 増援、時間制イベント |
| `PhaseEnded` | 防衛、占領維持 |
| `RoundEnded` | ターン制限、防衛ラウンド数 |

各イベントのダメージ、状態異常、強制移動、戦闘不能をすべて解決してから目標を評価する。

## 判定順序

1. 行動またはPhaseイベントを完全に解決
2. 戦闘不能、撤退、地形・設置物の変化を確定
3. 関連する副目標を更新
4. 敗北条件を評価
5. 主目的グループを評価
6. 敗北成立なら敗北を優先
7. 主目的成立なら勝利へ遷移
8. どちらも未成立ならPhase進行を続ける

同じイベントで主目的と副目標が達成された場合、副目標を`Completed`にした後で勝利する。
同じイベントで勝利と敗北が成立した場合は敗北とし、報酬をPendingへ追加しない。

## AND・OR

- 同じ`All`グループの目標はすべて達成するとグループ達成
- 同じ`Any`グループは1つ達成するとグループ達成
- 主目的グループが複数ある場合、ミッション直下の`All / Any`を別途指定する
- ORの別条件が達成されたら、未達側は`Failed`ではなく`Superseded`相当の終了表示にする
- 初期実装では入れ子を1段までとし、任意深度の論理木を作らない

例:

```text
Victory = DefeatBoss
       OR (OperateGate AND EscapeAtLeast3)
```

## 増援と敵全滅

予告、出現、封鎖、上限、中断Saveは[`reinforcement_rules.md`](reinforcement_rules.md)を正本とする。

- 必須増援が予定されている間、現在の敵が0でも`EliminateTeam`を達成しない
- 増援予定マスが塞がれ、代替候補もない場合は`ReinforcementResolved`で出現不能を確定する
- 任意増援、探索で無効化した増援、勝利後増援は全滅条件へ含めない
- HUDには現在の敵数と「増援予告あり」を別々に表示する
- 増援の存在を隠す場合も、敵全滅直後に理由なく勝利しない状態を作らない。最低限「敵影あり」を表示する

## 副目標報酬

```text
副目標達成
-> ObjectiveProgressをCompleted
-> 戦闘内HUDへ達成表示
-> 戦闘勝利
-> MissionFlowがCompleted Objectiveを評価
-> Loot / Discovery / RegionProgress / RecruitCandidateをPendingへ追加
-> 安全帰還で恒久化
```

- Objective達成時点ではStorageやDiscovery Registryへ直接追加しない
- 同じObjective IDの報酬は同じ`expeditionAttemptId`で1回だけ付与する
- 敗北、手動撤退ではCompletedだった副目標も報酬を失う
- 再挑戦ではObjectiveを未達へ戻し、地形と目標座標を新しい遠征Seedから生成する
- 恒久登録済みDiscoveryを再取得しても重複登録しない
- 通常素材の再獲得可否は地域・地点ごとの再訪報酬設定に従う

## 敵AI

敵AIは攻撃可能性だけでなく、任務目的へ次の優先度を持つ。

1. この行動で敵側勝利条件を達成できる
2. プレイヤーの次行動で主目的達成されるのを妨害できる
3. 護衛対象、操作中の装置、確保中の地点へ圧力をかける
4. 撃破可能な対象を攻撃する
5. 孤立、低HP、近距離、Unit IDの既定優先順位

- すべての敵が目標マスへ殺到しないよう、任務データでObjective担当数の上限を指定する
- 野生動物は人間の装置操作を理解せず、縄張り・孤立対象・群れAIを優先する
- 知的勢力だけが防衛地点、脱出路、装置を戦術目標として評価する
- 敵が目的を狙うことは戦闘開始文と行動予告で示す

## UI

イベントをプレイヤーへ通知する優先順位、重複防止、確認待ち条件は
[`battle_message_box.md`](battle_message_box.md)を正本とする。Objective側は表示文を生成せず、
安定IDと確定済み状態だけを通知層へ渡す。

### 戦闘開始画面

- 主目的
- 敗北条件
- 公開済み副目標
- ターン制限
- 護衛・防衛・脱出対象
- 探索結果による変更

### 戦闘HUD

- 主目的を常時1行表示
- 複数条件は`1 / 3`、`2ラウンド残り`など進捗表示
- 副目標は折りたたみ可能な一覧にする
- 目標マスは通常移動範囲・攻撃範囲と異なる輪郭アイコンで表示する
- 達成時は短い通知を1回だけ表示し、同じ文章を画面とボタンで重複表示しない
- 失敗時は理由を表示し、主目的を続行できる場合は入力を止めない

### 結果画面

- 主目的の達成方法
- 達成・失敗した副目標
- ObjectiveごとのPending報酬
- 未取得理由
- 新しいRegionProgress、Discovery、加入候補

## 地域固有設定の配置

共通仕様書には地域名、敵編成、Objective ID、固有報酬を置かない。各地域の地点別目標は
`docs/regions/`以下の専用ファイルを正本とする。

- 灰枝の森: [`regions/ashbough_forest.md`](regions/ashbough_forest.md#任務目標と副目標)

## 戦闘イベントの実装契約

Root Actionの完全解決順、同時発生、増援、Boss段階移行、反応Skillは
[`battle_resolution_contract.md`](battle_resolution_contract.md)を正本とする。

イベントは表示通知ではなく、戦闘状態を変更した事実をObjectiveへ伝える内部データとする。
イベント発行前に状態を変更し、Objective側が同じダメージや移動を再実行しないようにする。

### 安定ID

```cpp
using BattleEventId = std::uint64_t;
using ActionId = std::uint64_t;
using ObjectiveId = std::string;
using ObjectiveGroupId = std::string;
using BattleObjectId = std::string;
using RewardId = std::string;
```

- データ定義IDはASCIIの`lower_snake_case`
- Unit ID、Object ID、Objective IDは1戦闘内で一意
- 表示名、翻訳文、座標をIDとして使用しない
- ランダム配置されたObjectも定義IDと連番から安定IDを作る
- `BattleEventId`は戦闘開始時に1へ戻し、発行ごとに単調増加
- `ActionId`は移動から行動終了まで同じ値を使用する

### 型付きイベント

無関係な項目を大量に持つ汎用構造体は使わず、`std::variant`で有効なPayloadを限定する。

```cpp
struct ActionResolvedEvent {
    ActionId actionId;
    std::string actorUnitId;
    ActionKind actionKind;
    GridPos endPosition;
};

struct UnitDefeatedEvent {
    std::string unitId;
    Team team;
    std::optional<std::string> sourceUnitId;
    std::optional<BattleObjectId> sourceObjectId;
};

struct UnitRetreatedEvent {
    std::string unitId;
    Team team;
    GridPos exitPosition;
    RetreatReason reason;
};

struct ObjectStateChangedEvent {
    BattleObjectId objectId;
    int previousDurability;
    int currentDurability;
};

struct ObjectDestroyedEvent {
    BattleObjectId objectId;
    std::optional<std::string> sourceUnitId;
};

struct TerrainCollisionEvent {
    std::string movingUnitId;
    GridPos collisionPosition;
    std::optional<BattleObjectId> objectId;
};

struct ReinforcementResolvedEvent {
    std::string waveId;
    ReinforcementResult result; // Spawned / Prevented / Exhausted
    std::vector<std::string> spawnedUnitIds;
};

struct PhaseStartedEvent { Phase phase; int round; };
struct PhaseEndedEvent { Phase phase; int round; };
struct RoundEndedEvent { int round; };

using BattleEventPayload = std::variant<
    ActionResolvedEvent,
    UnitDefeatedEvent,
    UnitRetreatedEvent,
    ObjectStateChangedEvent,
    ObjectDestroyedEvent,
    TerrainCollisionEvent,
    ReinforcementResolvedEvent,
    PhaseStartedEvent,
    PhaseEndedEvent,
    RoundEndedEvent>;

struct BattleEvent {
    BattleEventId id;
    ActionId actionId; // 行動外イベントは0
    int round;
    Phase phase;
    BattleEventPayload payload;
};
```

ダメージ、回復、状態異常付与はObjective条件が必要になった時だけ専用イベントを追加する。
初期実装でObjectiveが参照しない表示専用の細粒度イベントまで作らない。

### Battle Object

装置、倒木、信号機、脱出口など、ユニット以外の任務対象を`BattleObjectState`で管理する。
Definition、State、占有、耐久、操作、修理、生成、Saveの完全なデータ契約は
[`battle_objects.md`](battle_objects.md)を正本とする。

- 耐久を持たない地点マーカーは`durability = maxDurability = 0`
- 破壊可能Objectは耐久0になった時に`ObjectDestroyed`を1回だけ発行
- 描画側はObjectを所有せず、`BattleState`の読取結果だけを表示
- ランダム配置後のObject IDと座標は中断セーブの盤面スナップショットへ含める

## 所有権と責務

| 所有者 | 責務 | 行わないこと |
|---|---|---|
| `BattleState` | Unit、Terrain、Object、Phase、Roundの現在状態 | 報酬確定、画面遷移 |
| Combat/Skill/Movement Resolver | 1処理を解決し、状態変更後のイベントを生成 | Objective判定、UI描画 |
| `BattleController` | 入力、AI行動、イベントBatchの確定、勝敗遷移 | 地域固有報酬の付与 |
| `ObjectiveTracker` | DefinitionとEventからProgressを更新し、勝敗候補を返す | BattleStateの再変更 |
| `MissionFlow` / `GameApp` | 勝利後にCompleted Objectiveの報酬をPendingへ変換 | 戦闘途中の恒久化 |
| `main.cpp` | 状態、目標、通知の描画 | 目標判定、報酬判定 |

`BattleState`は`BattleMissionState`を所有する。

```cpp
struct BattleMissionState {
    std::vector<ObjectiveDefinition> definitions;
    std::vector<ObjectiveGroupDefinition> groups;
    std::unordered_map<ObjectiveId, ObjectiveProgress> progress;
    std::unordered_set<BattleEventId> consumedEventIds;
};
```

Definitionは戦闘開始後に変更しない。探索結果による目標追加・非表示解除は、BattleFactoryが
戦闘開始前にDefinitionへ反映する。

## 1行動のイベント処理順

詳細なRoot Action、反応、地形、状態異常の順序は
[`combat_resolution_order.md`](combat_resolution_order.md)を正本とする。本節はObjectiveへEventを
渡す境界の要約であり、個別Resolverが独自順序を追加しない。

プレイヤー行動と敵行動は、途中で勝敗を確定せず、1つの`ActionId`に属するイベントをすべて
解決してからObjectiveを評価する。

1. 入力またはAI行動を検証
2. 移動を確定
3. 進入地形、薬草、罠を解決
4. 攻撃、スキル、アイテム、待機、装置操作を解決
5. ダメージ、回復、状態異常を確定
6. ノックバックなどの強制移動を解決
7. 地形衝突、Object耐久、Object破壊を確定
8. 戦闘不能、撤退、降伏を確定
9. `ActionResolved`を最後に追加
10. Event Batchを`ObjectiveTracker`へID順で1回渡す
11. 副目標、敗北条件、主目的の順に評価
12. 継続なら次の入力またはAIへ進む

Phase終了処理も1つのBatchとして扱う。

1. Phase終了時状態異常
2. その結果の戦闘不能
3. `PhaseEnded`
4. Enemy Phase終了時だけ`RoundEnded`
5. Objective評価
6. 勝敗未成立なら次Phase開始処理、増援、`PhaseStarted`
7. 増援BatchをObjective評価

毒などで敵味方が同時に全滅した場合も、Batch全体を処理してから敗北優先で決定する。

## 勝敗評価API

```cpp
enum class BattleOutcomeKind { Ongoing, Victory, Defeat };

struct BattleOutcome {
    BattleOutcomeKind kind;
    std::vector<ObjectiveId> completedPrimaryObjectives;
    std::vector<ObjectiveId> failedDefeatConditions;
};

BattleOutcome ObjectiveTracker::evaluateOutcome() const;
```

- 敗北条件成立なら、主目的成立の有無にかかわらず`Defeat`
- 敗北条件なし、主目的グループ成立なら`Victory`
- それ以外は`Ongoing`
- 旧`allEnemiesDefeated()`と`allPlayersDefeated()`はObjective評価の内部条件として残せるが、
  `BattleController`から直接Victory/Defeatへ遷移しない
- 勝敗確定後は追加イベントとObjective更新を受け付けない
- 状態異常解除など戦闘終了後の清掃はOutcome確定後に1回だけ行う

## 報酬定義と二重付与防止

```cpp
struct LootReward { LootId id; int quantity; };
struct DiscoveryReward { DiscoveryId id; };
struct RegionObjectiveReward { RegionId regionId; ObjectiveId objectiveId; };
struct RecruitCandidateReward { std::string recruitId; };

using RewardPayload = std::variant<
    LootReward,
    DiscoveryReward,
    RegionObjectiveReward,
    RecruitCandidateReward>;

struct RewardDefinition {
    RewardId id;
    RewardPayload payload;
};
```

MissionFlowは`expeditionAttemptId + battleId + objectiveId + rewardId`を報酬台帳キーとして保持する。

- Objective達成時は報酬を付与しない
- Battle OutcomeがVictoryになった後だけCompleted Objectiveを集計
- 台帳に存在しない報酬だけPendingへ追加
- 結果画面の再描画、キャンプ再読込、ボタン連打で二重付与しない
- 敗北・手動撤退では台帳と未確定報酬を遠征状態と共に破棄
- 恒久登録済みDiscoveryとRecruitはPending追加時にも重複を拒否

## 非表示目標の公開条件

初期実装で使用できる公開条件を次に限定する。

| 条件 | 公開タイミング |
|---|---|
| `BattleStart` | 戦闘開始画面から公開 |
| `ExplorationFlag` | 指定探索結果がある場合、戦闘開始時に公開 |
| `ObjectObserved` | 味方が対象Objectから距離2以内で行動終了 |
| `RoundReached` | 指定ラウンド開始時 |
| `EventTag` | 指定タグを持つイベント解決後 |

- 非表示中は完全非表示または`???`のどちらかをDefinitionで指定
- 敗北条件は非表示にしない
- 公開済み状態は同じ戦闘中に戻さない
- 未公開副目標を偶然達成した場合も内部では達成し、結果画面で公開する
- 探索で完全情報を得た場合はBattleFactoryが開始前に公開済みへ変換する

## イベントの重複防止

- Resolverは状態遷移1回につきイベントを1回だけ発行
- `ObjectiveTracker`は`consumedEventIds`へ追加済みのイベントを無視
- 同一Batch内のEvent ID重複は開発時Assertとテスト失敗
- `creditedTargetIds`により同じUnit、Object、Tileを複数回加算しない
- 戦闘再生成時は新しいBattle IDとEvent列を作る
- 中断セーブを将来戦闘中へ拡張する場合、次Event IDと消費済みIDも保存対象にする

## 確定テスト一覧

1. 敵全滅で勝利
2. 味方全滅で敗北
3. 同じBatchで敵味方全滅なら敗北
4. 必須増援未解決中は敵0でも勝利しない
5. 増援`Prevented`後は全滅目標を達成可能
6. 地点へ移動しただけでは確保しない
7. 地点上で行動終了すると1回だけ確保
8. OR目標達成時、未達側を`Superseded`にする
9. 副目標達成だけでは勝利しない
10. Object耐久0で破壊イベントを1回だけ発行
11. 同じEventを2回渡しても進捗を二重加算しない
12. 勝利後だけObjective報酬をPendingへ追加
13. 結果画面を複数回処理しても報酬を二重付与しない
14. 敗北・撤退でCompleted副目標の未確定報酬を失う
15. 非表示目標を指定条件で公開
16. 未公開のまま達成した副目標を結果画面で表示
17. 勝敗確定後のイベントを無視
18. 表示言語を変更してもObjective判定結果が変わらない

## 実装順

1. `ObjectiveDefinition`、`ObjectiveProgress`、AND/ORグループ
2. `UnitDefeated`と`ActionResolved`イベント
3. `EliminateTeam`、`DefeatUnit`、`SecureTile`
4. 勝利・敗北の共通評価順
5. 戦闘開始画面、HUD、結果画面
6. 最初の地域で地点確保目標を接続
7. 必須増援と兵種限定の装置操作を接続
8. 地形衝突を使う条件付き副目標を接続
9. 防衛、脱出、護衛、装置操作の汎用化

## 実装時の不変条件

- 副目標達成だけで戦闘を終了しない
- 戦闘勝利前に副目標報酬をPendingへ追加しない
- 敗北条件を主目的より後に評価して勝利で上書きしない
- 必須増援を残したまま敵全滅勝利にしない
- 目標座標を描画コードだけに保持しない
- 目標判定に表示言語の文字列を使用しない
- 同じObjective報酬を再描画や再読込で二重付与しない
- 野生動物に人間用の装置・防衛地点AIを適用しない
