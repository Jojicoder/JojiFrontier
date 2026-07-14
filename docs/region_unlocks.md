# 地域解放・再訪仕様

文書種別: **正本**
正本範囲: 地域完了ID、次地域解放、安全帰還時の恒久化、再訪

## 状態

設定確定。本編10地域の解放、攻略完了、安全帰還、再訪を定める正本。地域内の地点条件は
`campaign_route_graph.md`、恒久報酬は`expedition_rewards.md`を参照する。

## 基本原則

- 新規ゲームで解放される遠征地は灰枝の森だけ
- 次地域は前地域の「地域攻略成果」を安全帰還で恒久化した時に解放する
- Boss撃破だけ、最終戦勝利だけ、必要素材の所持だけでは解放しない
- 施設建設、特定兵種、全副目標を次地域解放の必須条件にしない
- 解放済み地域は後の敗北、施設停止、素材消費で再ロックしない
- 攻略済み地域は素材回収、未達成副目標、会話、訓練のため再訪可能

## 恒久データ

```cpp
using RegionCompletionId = std::string;

struct BaseState {
    std::unordered_set<RegionId> completedRegionIds;
};
```

地域攻略は全Siteの`Surveyed`から推測せず、各地域の正式な地域ObjectiveをMissionFlowが完了した時に
Pendingへ追加する。安全帰還Transactionで`completedRegionIds`へ集合結合する。

`regionCleared()`は`completedRegionIds`を正本として判定する。Schema 2移行時だけ、既存の全Stageが
`Surveyed`以上なら対応地域を完了済みへ変換できる。

## 解放表

| # | 地域 | 地域攻略成果 | 次に解放する地域 |
|---:|---|---|---|
| 1 | 灰枝の森 | `ashbough_forest_survey_complete` | 沈黙した監視所群 |
| 2 | 沈黙した監視所群 | `cinderwatch_network_restored` | 灰鉄採石場 |
| 3 | 灰鉄採石場 | `ashiron_quarry_secured` | 黒水低湿地 |
| 4 | 黒水低湿地 | `blackwater_lowlands_secured` | 風裂き高原 |
| 5 | 風裂き高原 | `windscar_plateau_secured` | 旧辺境集落 |
| 6 | 旧辺境集落 | `old_frontier_settlement_secured` | 燼火峡谷 |
| 7 | 燼火峡谷 | `ember_ravine_secured` | 埋没聖堂 |
| 8 | 埋没聖堂 | `buried_dawn_sanctum_secured` | 破砕された前線砦 |
| 9 | 破砕された前線砦 | `shattered_march_fort_secured` | 地図外縁 |
| 10 | 地図外縁 | `mapped_edge_secured` | 本編完了、深層候補 |

表は各地域専用書で定義済みの正式IDを使用する。同義の地域完了IDを追加しない。灰枝の森の
`ashbough_forest_survey_complete`は重要発見としてDiscovery Registryへ登録すると同時に、同じ
安全帰還Transactionで`completedRegionIds`へ灰枝の森を追加する。IDが同じでも保存先と責務は
分離し、どちらか一方から他方を毎回推測しない。地図外縁では`mapped_edge_secured`に加えて
`main_campaign_completed`を本編完了フラグとして一度だけ登録する。

## 完了条件

地域Definition、Pending Region Completion、Boss退場理由、Grant台帳の実装契約は
[`region_mission_data_contract.md`](region_mission_data_contract.md)を正本とする。

共通条件:

1. 地域専用の最終主目的を達成
2. 必須の経路・設備・保全Objectiveを達成
3. Battleまたは最終EventがVictory
4. 地域攻略成果をPending Region Progressへ追加
5. 拠点へ安全帰還して恒久化

灰枝の森では灰角大猪を退かせるだけでなく、林縁と薬草の沢を通過可能にし、森の通行路を成立させる。
地図外縁では敵全滅ではなく、標識設置後に帰還基点へ脱出して安全帰還する。

## 地域選択UI

全10地域を順番に表示する。

- `未発見`: 名称を「不明地域」、詳細を非表示
- `判明・未解放`: 地域名と解放条件を表示、出発Buttonは無効
- `解放済み`: 既知情報、地点状態、取得候補を表示して出発可能
- `攻略済み`: 攻略済み表示と再訪目的を表示

初期状態では灰枝の森を`解放済み`、沈黙した監視所群を`判明・未解放`、それ以降を`未発見`とする。
司令所の地図室・偵察網は情報公開範囲を増やすが、地域解放そのものを代替しない。

## 再訪

- 攻略済み地域も入口から開始する
- 経路確保済み地点は安全通過可能
- Boss・地域最終強敵を必須再戦させない
- 初回Discovery、地域完了、加入候補、Key素材を再付与しない
- 地域専用書で許可した通常素材と未達成副目標だけを取得可能
- 再訪で次地域解放を取り消したり重複発火させない

## 失敗と例外

- 最終戦勝利後に敗北・手動撤退した場合、地域は未完了のまま
- 安全帰還Saveに失敗した場合、解放画面へ遷移せず同じTransactionを再試行
- 完了済み地域IDがあるのに前地域が未完了な旧・破損Saveは、後方互換のため完了済み地域を維持し、
  それ以前の地域も解放状態へ補完する。ただし完了報酬を追加しない
- 未知のRegion IDは無視して保存を上書きせず、復旧画面へ送る

## 受入条件

- 新規ゲームで灰枝の森以外へ出発できない
- 森のBoss勝利だけでは監視所群を解放しない
- 森攻略成果を安全帰還すると監視所群が一度だけ解放される
- 解放済み地域は施設停止や素材消費で再ロックしない
- 攻略済み地域を再訪しても完了報酬を再取得しない
- 地図外縁完了後も既存10地域へ再訪できる
