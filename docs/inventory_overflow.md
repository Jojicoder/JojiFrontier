# 倉庫上限・報酬超過仕様

文書種別: **正本**
正本範囲: 倉庫上限、受取保留、整理、放棄、帰還時の超過Transaction

状態: 正式仕様。通常の所有権とPending処理は`expedition_rewards.md`を正本とする。

## 上限

- 消耗品、武器、特性部品: IDごとに99
- 通常素材: IDごとに999
- 探索道具、キー素材: IDごとに1
- Discoveryと地域完了は集合で管理し数量を持たない

上限超過分を自動破棄、自動売却、別素材へ変換しない。

## 受取保留

安全帰還Transactionで倉庫へ入らない分は、恒久Save内の`RewardOverflowState`へ移す。

```cpp
struct OverflowStack {
    RewardGrantId grantId;
    ItemId itemId;
    int quantity;
    ExpeditionAttemptId sourceAttemptId;
};

struct RewardOverflowState {
    std::vector<OverflowStack> stacks;
};
```

- 保留上限は合計200 Stack
- 同じGrant、Itemは1 Stackへ統合する
- 受取保留は敗北ロスト対象ではない
- 保留品から倉庫へ移した数量だけStackを減らす
- 保留中のキー素材・地域進行・Discoveryは存在させない。これらは重複除去して直接恒久化する
- 200 Stackを超える帰還は確定せず、倉庫整理を要求する

## 帰還処理

1. Pendingを報酬台帳で重複検証
2. 倉庫へ入る数量を計算
3. 超過分を受取保留へ計算
4. 保留200 Stack以内か検証
5. 倉庫、保留、Discovery、地域進行を1 Transactionで保存
6. 成功後にPendingを破棄して帰還結果画面を表示

帰還結果画面からは保存成功後に離れられる。超過がある場合も保留保存済みなら拠点へ進める。
保存失敗時は同じ画面に留まり、Pendingを消さず再試行する。

## 倉庫整理画面

- 倉庫、装備中、遠征準備中、受取保留を区別して表示
- 上限、現在数、受取可能数を表示
- 初期版では武器・素材の売却や分解を実装しない
- 消耗品と通常素材だけ任意放棄可能
- 装備中、唯一の探索道具、キー素材、Discoveryは放棄不可

## 保留品の放棄

- Stack単位または数量指定で放棄可能
- 「品名、数量、再取得できない可能性」を表示する確認を必須とする
- 長押しや二段階確認ではなく、明示的な確認Buttonを1回使う
- 放棄TransactionのSave成功後だけ一覧から削除する
- 重要素材として定義されたItemは放棄不可

## 受入条件

- 素材998所持で5取得すると999を倉庫、4を保留へ保存する
- 消耗品99所持でも帰還報酬が消えない
- 同じ帰還結果を再表示しても保留が二重付与されない
- 保留200 Stack超過時にPendingを失わない
- 放棄確認をCancelすると数量が変わらない
- Save失敗時に倉庫・保留・Pendingが部分更新されない
