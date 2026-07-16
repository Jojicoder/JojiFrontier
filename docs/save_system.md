# JOJIFrontier セーブシステム

状態: Phase 1実装済み。恒久拠点状態と言語設定を保存する。遠征中断セーブも簡略版として実装済み。
現在のSchemaは2。Export / Import、Schema移行骨格、保存状態HUD、復旧UIは実装済み
(2026-07)。Web同期完了待ち・GitHub Pages更新継続試験は未実装(実ブラウザ/Pages公開
環境が無い環境で作業したため対象外のまま)。

施設の正式仕様変更によりSchema 3を予定する。現行Schema 2の`builtNodeIds`は破棄せず、
`constructedFacilityIds`へ移行し、旧稼働状態は読み捨てる。具体的規則は
[`base_development.md`](base_development.md#建設と研究)を正本とする。
Schema 3には同時に`route_graph_data.md`のRoute Progressと`region_unlocks.md`の
`completedRegionIds`を追加する。施設用とRoute用に別々のSchema 3を作らない。遠征Checkpointの
正式な保存境界は[`expedition_flow.md`](expedition_flow.md)を正本とする。

## 現在の実装状況

実装済み:

- `SaveData`と`schemaVersion = 2`
- `BaseState`、倉庫、Discovery、拠点段階のJSON保存
- 解放済み技術、建設済み施設、ユニット単位の武器変更・調整特性の保存
- Schema 1の兵種単位装備を、該当兵種の各ユニットへ移行
- 4人編成と表示言語の保存
- デスクトップ版の`joji_frontier_save.json`
- 一時ファイルからの置換と直前データの`.bak`保持
- 最新データ破損時のバックアップ読込
- 未対応の将来スキーマを拒否し、自動上書きを停止
- Web版の`/joji-save`へのIDBFSマウントとIndexedDB同期
- 帰還、拠点発展、施設操作、装備変更、編成変更、言語変更後のオートセーブ
- 遠征中断セーブ（`ExpeditionCheckpoint`、下記「遠征中断セーブの実装範囲」参照）
- Export / Import画面（`exports/`/`imports/`フォルダ経由、`.preimport.bak`退避。
  以前ここに「未実装」と記載していたが実装済みだった、2026-07訂正）
- `v1 -> v2`移行処理（`jf::migrateSave()`、現行は`deserializeSave()`の既定値埋めで
  実質No-opだが、移行前に`.schema-vN.bak`へ退避する骨格まで実装。Schema 3で
  実差分が生じた時にこの関数へ変換ロジックを足すだけで済む）
- 保存中、成功、失敗のHUD表示（`SaveHudState{Idle,Saving,Saved,Failed}`、自動再試行
  最大3回・0.5/1/2秒、手動再試行は回数制限後も利用可能）
- 破損時の復旧選択画面（`Restore Backup`/`Import Save`/`Start New`。Restore Backupは
  `load()`自身が試さない`.preimport.bak`/`.schema-vN.bak`を追加で試す。Start Newは
  破損ファイルを`.corrupt-YYYYMMDD-HHMMSS.json`へ退避してから新規開始）

未実装:

- C++側が`FS.syncfs`完了を待つ非同期状態管理
- Emscripten実機とGitHub Pages更新前後の継続試験
- Schema 2施設データからSchema 3建設済み・稼働中施設への移行

## 遠征中断セーブの実装範囲

全地域共通のCheckpoint復元順、Node変更時の退避、不正Route隔離は
[`expedition_recovery.md`](expedition_recovery.md)を正本とする。

下記「後期実装の確定仕様 > 遠征中断セーブ」で定義した3チェックポイント
（`ExplorationResolved`/`BattleVictoryResolved`/`Camp`）のうち、実装は次の2つに単純化した
`ExpeditionCheckpoint`（`include/jf/core/SaveSystem.hpp`）として存在する。

- `Exploration`: `startExpedition()`直後（ステージ0、探索ルート未選択、パーティとバッグ確定済み）
- `Camp`: 戦闘勝利報酬確定後のキャンプ画面。キャンプでアイテムを使うたびに更新する

戦闘中・配置中（`Screen::Battle`、`Screen::PreBattleDeployment`）はチェックポイントを更新しない。
その間にアプリを終了した場合は、直前の`Exploration`または`Camp`チェックポイントから再開し、
プレイヤーはルート選択またはContinue Expeditionをやり直す。同じ`expeditionSeed`から地形・敵編成が
決定論的に再生成されるため、これは仕様上のロールバックであり、乱数結果が変わる不具合ではない。

`Camp`チェックポイントは、生存パーティの各ユニットHP（`id`と`currentHp`のみ）を保存する。復元時は
`selectedPartyIds`とロースターからユニットを再構築し、保存されたHPを当てはめてから
`createScenarioContinuationBattle`で戦闘状態を作り直す（撃破済みユニットは`currentHp <= 0`のまま渡され、
既存の生存者フィルタで自然に除外される）。

拠点への安全帰還・敗北・遠征リタイアはすべて`resetToBase()`を通るため、そのタイミングで
チェックポイントは破棄される（次のオートセーブで`"expedition": null`に戻る）。

## 目的

ブラウザ版を更新してJavaScript、WebAssembly、データ、画像が差し替わっても、既存の
プレイヤー進行を削除せず継続利用できるようにする。

Web版はIndexedDB、デスクトップ版はローカルファイルを使用する。保存形式と移行処理は
両者で共通化する。

## 保存構成

```text
C++ SaveData
-> JSONへシリアライズ
-> Web: Emscripten IDBFS / IndexedDB
-> Desktop: ローカルJSONファイル
```

Web版ではセーブ用ディレクトリをIDBFSへマウントする。起動時にIndexedDBから読み込み、
保存時にはファイル書き込み後に`FS.syncfs(false, ...)`の完了を待つ。

セーブ読込が完了するまでは新規ゲームデータを自動保存しない。起動直後の初期値で既存
セーブを上書きする事故を防ぐ。

## セーブ形式

ルートに必ずスキーマバージョンを持たせる。

```json
{
  "schemaVersion": 1,
  "gameVersion": "0.1.0",
  "base": {},
  "settings": {},
  "expedition": null
}
```

- `schemaVersion`: JSON構造の移行判定に使う整数
- `gameVersion`: 診断と不具合報告用。互換性判定の主キーにはしない
- `base`: 恒久進行
- `settings`: 言語、音量などのユーザー設定
- `expedition`: 遠征中断セーブ。未採用期間は`null`

## 保存対象

恒久状態:

- `BaseState`
- 倉庫の`LootStack`
- Discovery Registry
- 拠点開拓段階
- 解放済み施設ノードID
- 建設済み施設IDと稼働中施設ID（Schema 3以降）
- 加入済み仲間
- 解放済み兵種、アイテム、武器を導出するための恒久情報

設定:

- 日本語／英語
- 音量
- 将来追加する表示・操作設定

遠征中断セーブを採用する場合:

- `ExpeditionState`
- Pending Loot
- Pending Discoveries
- 遠征バッグ
- パーティHPと戦闘不能状態
- 現在ノードと選択済み探索結果
- 戦闘中断を許可する場合はBattleStateとターン状態

通常のオートセーブでは、キャッシュや表示アニメーションなど再生成可能な一時状態は
保存しない。

## スキーマ移行

アップデート時に古いセーブを削除しない。読み込み時に現在のスキーマへ順番に移行する。

```cpp
SaveData migrateSave(SaveData save) {
    if (save.schemaVersion == 1) {
        // 新しい項目へ互換性のある初期値を設定する。
        save.schemaVersion = 2;
    }
    return save;
}
```

ルール:

- 移行は`v1 -> v2 -> v3`のように一段ずつ行う
- 新項目にはゲーム進行を壊さない初期値を設定する
- 不明な新バージョンを古いゲームで上書きしない
- 移行前データのバックアップを保持する
- 移行成功後にだけ最新版を保存する
- 各旧バージョンからの移行を自動テストする

## 保存タイミング

最低限のオートセーブ地点:

- 拠点へ安全帰還し、LootとDiscoveriesを確定した直後
- 施設ノード解放直後
- 仲間加入または恒久編成変更後
- 言語などの設定変更後
- 新しい遠征を開始する直前

遠征中断セーブを採用する場合は、探索選択後、戦闘勝利後、キャンプ到着時にも保存する。
戦闘中に毎フレーム保存はしない。

## 破損対策

- 一時ファイルへ書いてから本セーブへ置き換える
- 最新セーブと直前バックアップを保持する
- JSON解析、必須項目、値範囲を検証する
- 読み込み失敗時に破損データを即時上書きしない
- 復旧できない場合は新規開始、バックアップ読込、Importを選択可能にする

## Export / Import

IndexedDBだけでは、サイトデータ削除、端末変更、公開URL変更に対応できない。そのため、
セーブJSONのExportとImportを用意する。

- Export: 現在のセーブをJSONファイルとして保存
- Import: JSONを検証・移行し、内容確認後に適用
- Import前に現在のセーブをバックアップ
- 将来スキーマを古いゲームへImportする操作は拒否

クラウド同期は初期実装の対象外。Export／Importを最初の端末間移行手段とする。

## Web公開時の条件

IndexedDBはサイトのorigin単位で分離される。ゲーム更新後もセーブを維持するには、原則
として同じプロトコル、ドメイン、ポートで公開し続ける。

GitHub Pagesでは次を維持する。

- 同じアカウントまたはOrganization
- 同じPagesドメイン
- 同じゲーム公開パス

リポジトリ名、カスタムドメイン、公開パスを変更すると、ブラウザからは別サイトとして
扱われる可能性がある。移行時は旧サイトでExportを案内し、新サイトでImportできるように
する。

次の場合は永続性を保証できない。

- ユーザーがブラウザのサイトデータを削除した
- シークレット／プライベートモードを使用した
- ブラウザやOSがストレージを消去した
- 公開originを変更した

## 実装順

1. `SaveData`、`schemaVersion`、保存エラー型を定義
2. `BaseState`と設定のJSON変換
3. JSONラウンドトリップと不正データのテスト
4. デスクトップ版のファイル保存・読込
5. Web版のIDBFS初期化、読込、同期
6. 帰還時などのオートセーブ接続
7. `v1 -> v2`の移行テスト用サンプルを追加
8. Export／Import画面を追加
9. GitHub Pages上で更新前後の継続テスト

最初の実装範囲は恒久`BaseState`と言語設定までとする。遠征中断セーブは、探索・キャンプ・
戦闘の画面遷移が固まった後に追加する。

## 後期実装の確定仕様

以下は実装を後回しにするが、実装時に再検討で停止しないための確定仕様とする。

### 遠征中断セーブ

戦闘途中のセーブは採用しない。保存可能なチェックポイントは次の3種類に限定する。

1. `ExplorationResolved`: 探索選択を確定し、戦闘開始前の補正と配置を生成した直後
2. `BattleVictoryResolved`: 勝利判定と報酬追加が完了し、キャンプへ遷移する直前
3. `Camp`: キャンプ画面でアイテム使用などの処理が完了した直後

敵ターン途中、味方行動途中、攻撃確認中、配置操作途中は保存しない。アプリを閉じた場合は
最後に完了したチェックポイントから再開する。これにより、ターン処理、アニメーション、
選択中ポインタを保存対象から除外する。

遠征チェックポイントには次を保存する。

- 地域ID、地点ID、遠征ステージ、チェックポイント種別
- 遠征シードと盤面生成器バージョン
- 確定済み`ExplorationOutcome`
- 生成済み地形24マスのスナップショット
- 戦闘開始時の敵ID、兵種、武器、配置
- 自由配置を使った場合の味方配置
- パーティID、現在HP、戦闘不能状態
- 遠征バッグと残数
- Pending Loot、Pending Discoveries
- `RegionProgress`
- 取得済み報酬を二重付与しないための地点完了ID

乱数生成器の内部メモリ状態は保存しない。再現に必要なシードと生成器バージョンを保存し、
生成済み地形と配置はスナップショットを優先する。将来生成アルゴリズムが変わっても、進行中
遠征の盤面が変化しないようにする。

アップデート後は安定IDでデータを再接続する。武器、兵種、アイテムの数値は原則として最新
データを使用するが、地形、配置、HP、消耗品、Pending資産はセーブを維持する。削除されたIDが
ある場合は次の順で縮退する。

1. 互換IDへの移行定義を適用
2. 同カテゴリの基本データへ置換
3. `ExplorationResolved`なら同地点の探索画面へ戻す
4. 復旧不能なら直前キャンプへ戻す

縮退時もPending資産を勝手に確定・削除しない。報酬付与済み地点IDを保持し、再戦による報酬の
二重取得を防ぐ。

### Export / Import

設定画面に`Save Data / セーブデータ`区画を追加し、`Export`と`Import`を配置する。拠点画面
以外でもExportは可能とする。Importは遠征中データを置換するため、拠点画面でのみ実行可能と
する。遠征中にImportを選んだ場合は「拠点へ戻ってから実行してください」と表示する。

Exportファイル名:

```text
JOJIFrontier-save-YYYYMMDD-HHMMSS.json
```

Export JSONには通常セーブと同じSchema、ゲームバージョン、作成日時を含める。秘密情報や端末
固有パスは含めない。

Import手順:

1. ファイルを選択
2. JSON解析、Schema移行、必須項目、値範囲、安定IDを検証
3. 拠点段階、仲間数、Discovery数、最終保存日時の概要を表示
4. 「現在のデータを置き換える」確認を要求
5. 現在セーブを`.preimport.bak`へ保存
6. Importデータを通常セーブへ書き込み
7. 再読込に成功した場合だけゲーム状態へ適用

Importは置換のみとし、倉庫、Discovery、仲間などのマージは行わない。重複Lootは読込時に同じ
IDへ合算し、重複Discoveryと施設IDは集合として1件へ正規化する。不正JSON、移行不能データ、
現在より新しいSchemaは拒否し、現在のセーブを変更しない。

Web版はブラウザのファイルダウンロードと`<input type="file">`を使用する。デスクトップ版は
OSファイル選択ダイアログを抽象化して使用し、利用できない環境では実行ファイル横の`exports/`
をExport先、`imports/`をImport候補場所とする。

### 保存状態HUD

画面右下に小さな保存状態を表示する。戦闘コマンド、施設ボタン、設定ボタンとは重ならない
固定余白を確保する。

状態遷移:

```text
Idle
-> Saving / 保存中
-> Saved / 保存済み
-> Idle

Saving
-> Failed / 保存失敗
-> Retrying / 再試行中
-> Saved または FailedPermanent
```

- `Saving`: 保存要求開始からストレージ同期完了まで表示
- `Saved`: 成功後1.5秒表示
- `Failed`: 失敗理由の短い要約と再試行回数を表示
- `FailedPermanent`: 自動で消さず、設定画面への導線とExportボタンを表示

保存要求が連続した場合、実行中の保存は中断しない。最新状態を`dirty`として1件だけ保持し、
現在の保存完了後にもう一度保存する。各フレームや各クリックを個別キューへ積まない。

自動再試行は最大3回。待機時間は0.5秒、1秒、2秒とする。手動の「再試行」は回数制限後も
利用可能にする。表示文は日本語と英語を必須とする。

### 破損復旧画面

通常セーブの解析または検証に失敗した場合、自動で新規セーブを上書きしない。バックアップが
正常なら復旧画面で推奨選択として表示する。

選択肢:

- `Restore Backup / バックアップから復旧`
- `Import Save / セーブを読み込む`
- `Start New / 新しく始める`

新規開始は二段階確認とし、「破損データを保持したまま新規開始」と表示する。破損ファイルは
日時付きの`.corrupt-YYYYMMDD-HHMMSS.json`へ移動し、即時削除しない。

バックアップ復旧時は、復旧データを一度読み直して検証してから通常セーブへ反映する。復旧前の
破損通常セーブを`.corrupt-*`へ保持し、復旧成功後のデータを新しい通常セーブにする。直前の正常
バックアップは、次の正常なオートセーブが完了するまで上書きしない。

### Schema移行

Schema移行はJSON状態で一段ずつ適用する。

```cpp
while (version < kCurrentSaveSchemaVersion) {
    json = migrateOneVersion(version, json);
    ++version;
}
```

各`vN -> vN+1`移行は独立関数とし、入力Schema、出力Schema、追加した既定値、名称変更したIDを
テストする。移行前に元ファイルを`.schema-vN.bak`へ保存する。全段階の移行と最新版としての
再読込に成功した場合だけ通常セーブを更新する。

移行失敗時は元データを保持して復旧画面へ進む。現在より新しいSchemaは読込も自動移行もせず、
「新しいゲームバージョンが必要」と表示する。古いゲームから将来Schemaを上書きしない。

Schema 2は現行実装として確定済みで、ユニット単位装備と簡略版`ExpeditionCheckpoint`を含む。
次のSchema 3では施設の建設済み・稼働中分離、Route Progress、`completedRegionIds`、正式な
遠征Checkpointを同時に追加する。具体的な変換規則と旧SchemaサンプルはSchema 3実装時に
固定し、施設・Route・遠征ごとに別Schema番号を消費しない。

### Web同期完了待ち

JavaScript側に非同期保存関数を1つ用意し、C++へ次の結果コードをコールバックする。

```text
Pending / Success / Failed / Unavailable
```

C++側はファイル書き込みだけで保存成功と判定せず、`FS.syncfs(false)`のコールバックが
`Success`を返した時点で保存完了とする。同期中に新しい保存要求が来た場合は`dirty = true`だけを
設定し、同期完了後に最新状態を再シリアライズして保存する。

`visibilitychange`と`pagehide`では未保存状態があれば同期を要求するが、ブラウザ終了時の非同期
完了は保証できない。そのため重要チェックポイントでは画面遷移直後に保存し、終了イベントだけに
依存しない。

IndexedDBまたはIDBFSが利用できない場合、ゲームはセッション中のみ動作可能な一時保存へ切り替え、
常時警告を表示する。自動的に永続保存できたように見せず、Exportを推奨する。利用可能状態へ戻った
場合はユーザー確認なしに一時データで既存永続セーブを上書きしない。

### GitHub Pages更新継続試験

最初は手動リリースチェックリストとして運用し、Webビルドと保存APIが安定した後に自動化する。

対象ブラウザ:

- 最新Chrome
- 最新Safari
- 最新Firefox

リリース前試験:

1. 公開中ビルドで専用テストセーブを作成
2. 倉庫、Discovery、施設、編成、言語、遠征チェックポイントを記録
3. 新ビルドを同じGitHub Pages originと公開パスへ配置
4. キャッシュを更新して再読込
5. Schema移行または同一Schema読込が成功することを確認
6. 記録した全項目とPending資産が一致することを確認
7. オートセーブ後に再読込して再度一致を確認
8. Exportした旧セーブを新ビルドへImportできることを確認
9. 将来Schema、破損通常セーブ、正常バックアップの復旧試験を実行

GitHub Pagesではアカウント、ドメイン、プロトコル、リポジトリ名、公開パスを原則変更しない。
変更が必要な場合は旧URLでExport告知期間を設け、新URLでImportできることを公開前に確認する。
