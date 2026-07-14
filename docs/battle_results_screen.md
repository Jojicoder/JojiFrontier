# 戦闘結果画面仕様
## 目的

戦闘の成否、達成内容、遠征へ持ち越す損耗と成果を一画面で確認する。結果画面は表示専用であり、
報酬計算、Pending追加、地域進行の更新を行わない。報酬の正本は
[`expedition_rewards.md`](expedition_rewards.md)とする。

## 表示データ

MissionFlowがOutcome確定と報酬付与を一度だけ完了した後、次のSnapshotを生成する。

```cpp
struct BattleResultViewModel {
    BattleOutcome outcome;
    TextKey missionNameKey;
    TextKey outcomeReasonKey;
    std::vector<ObjectiveResultView> primaryObjectives;
    std::vector<ObjectiveResultView> secondaryObjectives;
    std::vector<LootDeltaView> pendingLootAdded;
    std::vector<DiscoveryId> discoveriesAdded;
    std::vector<RegionProgressDeltaView> regionProgressAdded;
    std::vector<RecruitId> recruitCandidatesAdded;
    std::vector<UnitResultView> partyResults;
    std::vector<ItemUsageView> itemUsage;
    ResultTransition transition;
};
```

結果画面の再表示、ボタン連打、Save復元でSnapshotを再生成しても報酬を再付与しない。

## 勝利画面

表示順は次とする。

1. 「勝利」、戦闘地点名、勝利理由
2. 主目的の達成状態
3. 副目標の達成・未達成状態と追加成果
4. 未確定戦利品、重要発見、地域成果、加入候補の今回増加分
5. 各Unitの現在HP、戦闘不能、今回の消耗品使用数
6. 地域内の踏査、経路確保、Boss撃破などの進行差分
7. 次の遷移ボタン

増加分と遠征全体の合計を混同させない。「今回獲得」と「遠征中合計」を別見出しにする。報酬がない
分類は大きな空枠を出さず、省略または「なし」を1行表示する。

## 敗北・撤退画面

- 敗北理由と失敗した主目的
- 戦闘不能になったUnit
- 失われるPending 4分類
- 使用済みで戻らない消耗品
- Storageへ戻る未使用持込品
- 拠点帰還後にHPと戦闘不能が完全回復すること

敗北時の唯一の遷移は「拠点へ帰還」。通常撤退は安全帰還ではなく、敗北と同じPending処理を明示する。
帰還信号弾による安全帰還結果だけは、確定予定資産を表示して「帰還を確定」とする。

## 続行・帰還への遷移

- 通常勝利: 「キャンプへ進む」だけを表示する
- キャンプ: 「遠征を続ける」「拠点へ帰還」の判断を行う
- 地域最終地点: 「キャンプへ進む」。深層がなければキャンプで続行を無効化する
- 敗北・撤退: 「拠点へ帰還」だけを表示する

結果画面とキャンプの両方に同じ続行・帰還選択を置かない。タイトル下の説明とボタンに同じ文を二重表示
しない。

## 操作とレイアウト

- 戦場を暗くした上へ情報Panelを1枚だけ表示する
- Objectiveと成果が多い場合は本文だけをScrollし、見出しと遷移ボタンを固定する
- 色だけで成否を表さず、達成、未達成、失敗を文字とIconで示す
- 日本語・英語とも省略せず読める可変高さを使う
- 内部ID、Seed、報酬台帳Keyは表示しない
- 遷移確定中は二重入力を無効化する
- Checkpoint保存に失敗した場合は画面を閉じず「再試行」を表示する

全表示文は[`localization.md`](localization.md)の`TextKey`を使う。

## 受入条件

1. 主目的と副目標が異なる区分で表示される。
2. Pendingの今回増加分と遠征中合計を判別できる。
3. 戦闘不能、現在HP、使用済み消耗品を次地点へ進む前に確認できる。
4. 結果画面を再表示しても報酬が増えない。
5. 勝利はキャンプ、敗北は拠点へ正しく遷移する。
6. 保存失敗時に遷移せず、再試行で同じ報酬台帳を維持する。
7. 日本語・英語、Window最大化、最小対応解像度で文字が見切れない。
