# Battle Object共通仕様

## 状態

共通仕様確定。倒木、柵、扉、装置、物資箱、増援口、踏査地点、脱出口など、UnitでもTerrainでも
ない盤面要素を扱う正本。地域書はObject Definition ID、初期状態、配置、Objectiveとの接続だけを
定義する。

## 責務の境界

- Terrainはマス固有の移動コストと行動終了効果を持つ
- Unitは陣営、能力、HP、行動状態を持つ
- Battle Objectは任務対象、障害物、装置、容器、入口・出口を表す
- UI装飾や床パネルはBattle Objectにしない
- 報酬はObject自身が付与せず、Objective完了後にMissionFlowがPendingへ変換する

## 種別

```cpp
enum class BattleObjectKind {
    Marker,
    Barrier,
    Device,
    Container,
    SpawnPoint,
    ExitPoint,
};
```

| 種別 | 用途 | 例 |
|---|---|---|
| `Marker` | 占有可能な地点・調査対象 | 踏査地点、薬草調査地点、封鎖杭設置地点 |
| `Barrier` | 通路や攻撃を妨げる耐久物 | 倒木、柵、扉、支柱、封鎖扉 |
| `Device` | 操作で戦場状態を変える | 信号機、水門、冷却弁、警鐘 |
| `Container` | 保全・回収対象 | 物資箱、記録箱、写本箱 |
| `SpawnPoint` | 増援入口 | 破孔、門、坑道口 |
| `ExitPoint` | 離脱・護送・帰還地点 | 退避路、帰還基点、盤外出口 |

地域固有名ごとにEnumを増やさず、安定したDefinition IDとTagで差分を表す。

## データモデル

```cpp
using BattleObjectId = std::string;
using BattleObjectDefinitionId = std::string;

enum class BattleObjectTeam { Neutral, Player, Enemy };
enum class BattleObjectStateKind { Active, Disabled, Opened, Destroyed };

struct BattleObjectDefinition {
    BattleObjectDefinitionId definitionId;
    BattleObjectKind kind;
    int maxDurability;
    int defense;
    int resistance;
    bool canOccupy;
    bool blocksMovement;
    bool blocksStopping;
    bool blocksDeployment;
    bool blocksProjectiles;
    bool canBeAttacked;
    bool canBeRepaired;
    std::unordered_set<std::string> tags;
};

struct BattleObjectState {
    BattleObjectId id;
    BattleObjectDefinitionId definitionId;
    GridPos position;
    BattleObjectTeam team;
    BattleObjectStateKind state;
    int durability;
    int interactionCount;
};
```

Definitionは戦闘中不変、Stateだけを変更する。表示名、説明、操作文はLocalization Keyから取得する。

## 占有と通行

- 初期実装のObjectは1マス占有
- `canOccupy = true`のMarker・SpawnPoint・ExitだけUnitと同じマスに存在可能
- `blocksMovement = true`なら通過不可
- `blocksStopping = true`なら停止不可。通過可・停止不可の入口表現に使用できる
- `blocksDeployment = true`なら初期配置不可
- Object同士は同一マスへ配置しない
- Unit初期配置、必須Objective、唯一の通路へランダムBarrierを重ねない
- Unitが占有中のMarkerは消えず、Unit描画の下へ表示する

`blocksProjectiles`は射線を使う遠距離攻撃だけを遮る。射線は攻撃元と対象マスの中心を結ぶ
supercover lineで通過する中間マスを固定順に列挙し、1つでも遮断Objectがあれば対象不可とする。
攻撃元と対象マス自身は遮断判定から除く。隣接攻撃、射線無視Tagを持つ魔法・地形効果、Bossの
予告済み突進には適用しない。

Definition検証では次を不正とする。

- `canOccupy`と`blocksMovement`が同時にtrue
- `maxDurability = 0`かつ`canBeAttacked = true`
- `canBeRepaired = true`かつ`maxDurability = 0`

`blocksMovement = true`の場合は停止も不可能として扱い、`blocksStopping`の値を参照しない。

## 耐久とダメージ

- `maxDurability = 0`は耐久を持たず、通常攻撃対象にできない
- 耐久Objectへの物理ダメージは`max(STR + Weapon Might - defense, 1)`
- 魔法ダメージは`max(MAG + Weapon Might - resistance, 1)`
- 固定Objectダメージを持つSkillは通常式を使わず、効果文の値を適用
- 毒、炎上、回復、状態異常はObjectへ適用しない
- Critical、反撃、追撃はObjectへ発生しない
- 耐久は0未満にせず、0到達時に`Destroyed`へ一度だけ遷移
- `ObjectDestroyedEvent`はObject IDごとに1回だけ発行

通常武器は`canBeAttacked = true`のObjectをUnitと同様に対象選択できる。任務上破壊不可の装置や
Containerは`canBeAttacked = false`とし、対象予測に理由を表示する。

## 破壊後の状態

- Barrierは破壊後に原則として通行・停止可能になる
- Deviceは機能停止し、操作不可になる
- Containerは内容を自動取得せず、保全Objectiveを失敗させる
- SpawnPointは破壊または封鎖された時、接続Waveを`Prevented`へ解決する
- MarkerとExitは通常破壊不可
- 破壊後の残骸がTerrainを変える場合は、地域書に変換先Terrain IDを明記する

Object消滅ではなく`Destroyed`状態を保持し、描画、Objective、Saveが同じObject IDを参照できるようにする。

## 操作

操作は攻撃とは別の`Interact`コマンドで、行動を終了する。

```cpp
struct ObjectInteractionDefinition {
    std::string interactionId;
    int range;
    std::unordered_set<UnitClass> allowedClasses;
    BattleObjectStateKind requiredState;
    int maxUses;
};
```

- 基本射程は隣接1マス。Markerは同一マスで行動終了を条件にできる
- 兵種制限がない操作は全Unitが使用可能
- 工兵、衛生兵などの専用操作は対象選択時に必要兵種を表示
- 操作完了後に`ObjectStateChangedEvent`、最後に`ActionResolvedEvent`を発行
- 同じ一回限り操作を連打してObjectiveを増やさない
- 敵が操作可能かはInteraction Definitionで明示し、野生動物には人工装置操作を付与しない

## 修理

- `canBeRepaired = true`かつ耐久1以上のObjectだけ修理可能
- 初期共通修理は辺境工兵の専用Skillからのみ行う
- 1回の修理量は4、最大耐久を超えない
- `Destroyed`から戦闘中に復元しない
- 修理は行動を終了し、`ObjectStateChangedEvent`を発行
- 修理によって既に失敗した「無傷で保全」Objectiveを再達成させない

## ランダム生成

1. Terrain 24マスを生成
2. Unit初期配置保証マスを予約
3. Objective、Spawn、Exit、必須装置の固定・候補マスを予約
4. 必須経路が存在することを検証
5. 任意BarrierとContainerを地域別生成率で配置
6. Boss予告経路と最低1本の攻略経路を再検証
7. 不正ならObjectだけ再抽選し、上限到達後は安全な固定配置へ戻す

Terrain全体は再抽選しない。Object抽選結果、候補順、生成器バージョンをBattle Snapshotへ保存する。

## EventとObjective

Object Resolverは次を生成する。

- `ObjectStateChangedEvent`
- `ObjectDestroyedEvent`
- `TerrainCollisionEvent`
- `ActionResolvedEvent`

ObjectiveTrackerはEventを読むだけでObjectを変更しない。破壊、保全、操作、耐久閾値、指定地点確保は
安定したObject IDまたはTagで判定し、表示名で判定しない。

## Save

中断Saveへ全Objectの次を保存する。

- Object ID、Definition ID、位置、Team
- State、現在耐久、操作回数
- ランダム配置結果と生成器バージョン
- 発行済み破壊EventとObjective消費台帳

Definitionの数値は最新版を使用するが、現在耐久、状態、位置はSnapshotを優先する。削除された
Definition IDは互換表で移行し、復元不能なら直前の探索Checkpointへ戻す。

## UI

- Object名、耐久、操作、破壊可能・不可、通行・射線効果をTooltipに表示
- Objective対象は専用アイコンと外枠を付け、色だけに依存しない
- 選択時に攻撃、操作、修理の有効範囲を区別する
- 耐久警告は100%、50%、25%、0%の閾値を跨いだ時だけ通知
- SpawnPointは増援方向と残りRoundを表示する
- 非公開Objectは兆候だけを表示し、発見後に正式名へ切り替える

## 受入条件

- Barrierを破壊すると同じAction Batch完了後に通行可能になる
- Object破壊Eventと報酬が二重発火しない
- Marker上で指定兵種が行動終了した時だけ調査Objectiveが進む
- 壊れたContainerを修理して保全Objectiveを復活できない
- SpawnPoint封鎖が必須増援を`Prevented`へ変更する
- ランダムObjectが初期配置、必須Objective、唯一の経路を塞がない
- Save復元で位置、耐久、状態、操作回数が一致する
