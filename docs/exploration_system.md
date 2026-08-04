# JOJIFrontier 探索システム

状態: JOJIFrontier固有のゲームシステム案。地域名、人物、歴史へ接続する場合は
`../JojiWorldBible` を確認する。

遠征資産の所有権、Pending分類、安全帰還、敗北ロスト、報酬台帳は
[`expedition_rewards.md`](expedition_rewards.md)を正本とする。

## 基本方針

探索に自由移動マップは使用しない。遠征ルート上のノードを進み、各探索ノードで
状況文と選択肢を提示する。単なるノベル分岐ではなく、選択結果を直後の戦闘、消耗、
戦利品、発見物へ反映する。

本編10地域の順序、地点数、報酬カテゴリ、拠点発展の節目は
[`campaign_regions.md`](campaign_regions.md)を参照する。地点固有設定は`docs/regions/`以下へ
分離し、この共通仕様書へ重複記載しない。全地点の接続と遷移条件は
[`campaign_route_graph.md`](campaign_route_graph.md)を参照する。
Graphのデータ構造は[`route_graph_data.md`](route_graph_data.md)、地域解放は
[`region_unlocks.md`](region_unlocks.md)、画面遷移とCheckpointは
[`expedition_flow.md`](expedition_flow.md)を正本とする。

基本画面遷移:

```text
BASE
-> ROUTE NODE
-> EXPLORATION EVENT
-> RESULT
-> BATTLE
-> LOOT
-> CAMP
-> NEXT NODE または BASE
```

基本ループ:

```text
拠点
-> 探索ノード（状況文＋選択肢）
-> 戦闘
-> 戦利品
-> キャンプ（続行／帰還）
-> 次の探索ノード
```

## 周回と地域経路の開拓

遠征は常に地域入口から開始する。キャンプは遠征中の続行・帰還判断に使う場所であり、
次回遠征の開始地点にはしない。第2地域以降は標準4〜6周で主要な地点、重要発見、加入、
地域ボスへ到達する長さを基準とする。3地点だけの灰枝の森は導入地域として例外にする。

周回で味方の基礎能力や敵能力を自動補正しない。前回までに持ち帰った地域知識、開通路、
設備、現地協力者によって、入口から後半へ到達するまでの時間と消耗を減らす。

### 地点の恒久状態

```cpp
enum class SiteAccessState {
    Unknown,
    Surveyed,
    Secured,
};
```

| 状態 | 意味 | 次回以降の扱い |
|---|---|---|
| 未踏 | 安全な経路も脅威も不明 | 通常探索と必須戦闘 |
| 踏査済み | 地形、敵、退路を把握 | リスクと敵編成を事前表示。短縮選択肢を追加 |
| 経路確保済み | 街道、橋、門、監視所などを恒久確保 | 必須戦闘を短い通過イベントへ置換可能 |

状態の昇格は該当目的を達成し、安全帰還した時だけ確定する。敗北した遠征中の踏査や
工作は恒久化しない。

### 確保済み地点の通過

- 地域画面上では地点を省略せず、入口から順番に通る
- 地域入口で「確保済み経路を進む」を選ぶと、入口から連続する経路確保済み地点を一括解決する
- 地点ごとの確認ボタンは出さず、通過履歴を地域図と短い要約へまとめる
- 分岐、未解決地点、通行条件未達地点へ到達した時点で一括通過を停止する
- 既知のキャンプを通過する場合だけ「通過を続ける／キャンプを利用する／拠点へ帰還」を表示する
- 安全通過では戦闘、探索3択、ランダムイベント、回復、補充、報酬を発生させない
- 初回報酬、重要発見、加入候補は再取得できない
- 通常素材を再び狙う場合は「危険区域を再調査」を選び、再生成された戦闘を行う
- 地域固有の襲撃や天候で安全路が一時封鎖される場合は、遠征開始前に予告する
- 一時封鎖で`Secured`状態そのものは失わない

安全通過中も次の遠征状態をそのまま維持する。

### 灰枝の森の確保済み地点からの深層ショートカット

灰枝の森自身のSecured地点画面には「深層へ行く」「最深層へ行く」ボタンがあり、
拠点の地域一覧を経由せず直接深層遠征を開始できる(`GameApp::
startDeepLayerExpeditionFromSecuredSite()`)。2026-08-02までは`isRegionUnlocked()`
だけで出し分けており、この地点を通るたび(この画面を再訪するたび)毎回表示されて
いた。**一度でもその深層地域へ入ったら、以降は二度と表示しない**よう変更した
(`BaseState::deepLayerRegionsEntered`、`GameApp::startExpedition()`内で該当
RegionIdなら記録、このショートカット経由でも拠点の地域一覧経由でもどちらでも
記録される)。一度入った後は拠点の地域一覧から通常通り選ぶ。セーブ/ロードでも
維持される(`SaveSystem.cpp`の`deepLayerRegionsEntered`)。

- 現在HPと戦闘不能状態
- 遠征Bagと各消耗品の残数
- Pending Loot、Pending Discoveries、Pending地域進行、保護対象
- 編成、武器、Skill、特性
- 利用済みCamp効果と補給回数
- 選択済み分岐、解決済み地点、現在の経路位置

安全通過では`battlesWon`を増やさず、HP回復、戦闘不能復帰、消耗品補充、Camp利用回数の
復元を行わない。通過前後で遠征Snapshotの消耗状態が変わらないことを不変条件とする。

### 危険区域の再調査

- 経路確保済み地点で任意に選択する再戦闘とする
- 盤面、通常敵、通常地形は地域規則に従って再生成する
- 地点ごとに`reconLoot`を定義し、初回通常素材報酬の概ね50〜70%を基準とする
- 初回報酬、Discovery、重要素材、Boss固有素材、加入候補、地域進行は付与しない
- 副目標の初回達成報酬も再付与しない
- 再調査中の消耗、戦闘不能、敗北、帰還は通常遠征と同じ規則を使う
- 地域攻略後も通常素材を狙う任意手段として残すが、周回の必須作業にはしない

### 周回で解放する軽量化ギミック

| 恒久成果 | 次回以降の効果 |
|---|---|
| 踏査記録 | 敵編成、地形傾向、危険選択の結果を事前表示 |
| 開通路 | 対応地点の通常戦闘を省略して安全通過 |
| 補給地点 | 1遠征1回、キャンプで指定消耗品1個またはHP小回復を受け取る |
| 現地協力者 | 特定の障害除去、撤退路確保、選択肢追加 |
| 復旧設備 | 増援停止、障害物操作、ボス予告追加などの地域固有効果 |
| 施設技術 | 新兵種、探索能力、持込選択肢を増やすが、基礎能力を直接上げない |

### 標準4〜6周の役割

1. 第1周: 情報収集。最初のキャンプまで進み、踏査記録を持ち帰る
2. 第2周: 前半確保。踏査済み地点を安全路へ変え、最初の施設資料を持ち帰る
3. 第3周: 中盤開拓。仲間、補給地点、現地協力者の条件を達成して帰還する
4. 第4周: 後半進出。復旧設備とボス対策の重要発見を確保する
5. 第5周: 地域攻略。開拓済み経路を通り、HPとアイテムを残してボスへ挑む
6. 第6周: 任意の回収。未達成の副目標、追加素材、重要発見を回収する

第4〜5周で地域攻略できる構成を標準とし、第6周を必須にはしない。上手い編成や探索能力で
早期攻略できてもよい。同じ通常戦闘を進行条件として毎周繰り返させない。各周回では最低1つ、
次回を明確に楽にする恒久成果を持ち帰れるようにする。

4〜6周は強制回数ではなく、初見かつ標準的な判断での目安とする。熟練した編成は3〜4周、
慎重な進行は5〜6周で攻略してよい。3地点の灰枝の森だけは2〜3周を目安とする。

### 遠征開始位置と再開

- 新しい遠征Attemptは必ず地域入口から開始する
- 拠点帰還、敗北、任意撤退後の次Attemptも地域入口から開始する
- CampはそのAttempt内の判断地点であり、次回遠征の恒久再開地点にはならない
- 中断Saveからの再開だけは、保存済みAttemptの現在Nodeと全Snapshotを復元する
- アップデートで現在Nodeが無効になった場合は、遠征を破棄せず地域入口へ安全にフォールバックする
- 地域入口から確保済み連続区間を一括通過することで、入口開始の意味を保ちながら反復操作を減らす

## 選択肢の構成

テスト版では各地点に次の3択を用意する。

- 通常選択肢2つ
- 編成中の兵種によって使用可能になる専用選択肢1つ

兵種専用選択肢は常に最善にはしない。安全性、情報、戦利品、消耗、時間などの
トレードオフを持たせる。選択前には「安全」「危険」「物資消費」「戦闘有利」など、
結果の性質をある程度予告する。

## 選択結果が変更できる要素

- 敵の数と種類
- 敵味方の初期配置
- 地形と障害物
- 戦闘開始時HP
- アイテム消費
- 追加戦利品
- 施設解放に使う発見物
- 戦闘回避
- ボス能力や増援条件

戦闘回避を常に最適解にしない。戦闘を避けた場合は戦利品、発見物、地域確保などを
得られない場合がある。

## 兵種による探索能力

| 兵種系統 | 探索能力 |
|---|---|
| 辺境斥候 | 偵察、隠し道、高所からの観測 |
| 辺境工兵 | 障害物、機械、橋、遺跡装置 |
| 辺境猟兵 | 追跡、狩猟、野生生物への対応 |
| 暁の衛生兵 | 負傷者、医療記録、衛生上の危険 |
| 重装兵 | 瓦礫除去、強行突破、重量物運搬 |

兵種の価値を戦闘能力だけでなく、遠征ルートと資源管理にも持たせる。

## 地域専用設定

地点名、選択肢、戦闘補正、報酬、重要発見は地域ごとの専用ファイルで管理する。
共通探索書へ地域固有の数値を重複記載しない。

- 第1地域: [灰枝の森](regions/ashbough_forest.md)
- 第2地域: [沈黙した監視所群](regions/cinderwatch_gate.md)
- 全地域構成: [遠征地域構成](campaign_regions.md)

## 結果データ

探索結果は共通データへ変換し、イベント固有処理を戦闘コードへ直接書かない。

```cpp
struct ExplorationOutcome {
    int partyDamage = 0;
    std::vector<ItemType> consumedItems;
    std::vector<std::string> pendingLoot;
    std::vector<std::string> pendingDiscoveries;
    BattleModifier battleModifier;
    bool skipBattle = false;
};
```

`BattleModifier` は敵数、初期配置、地形、ボス能力、増援条件などを保持する。
探索結果画面と戦闘HUDでは、適用された効果を明示する。

例:

- 「斥候成功: 敵編成を確認」
- 「有利な初期配置」
- 「工兵工作: Barrierを除去」
- 「信号停止: 増援なし」

## 戦利品と発見物の分離

通常戦利品と進行用の重要発見は内部データを分離する。

`PendingLoot`:

- 鉄材
- 武器
- 消耗品
- 通常素材

`PendingDiscoveries`:

- 偵察資料
- 野戦医療記録
- 信号技術資料
- 地域ボス由来の重要発見

安全帰還時の処理:

```text
PendingLoot
-> 倉庫・所持品へ確定

PendingDiscoveries
-> Discovery Registryへ恒久登録
-> 対応する拠点ノードを解放可能にする
```

重要発見は売却、装備、消費の対象にしない。敗北時には通常戦利品と同様、まだ帰還して
いないPending Discoveriesを失う。

## 日本語表示と内部ID

探索システムのゲーム内表示は日本語を正規表示とする。

- 地点名
- 状況文
- 選択肢名と説明
- 条件タグと選択不可理由
- リスク／効果の予告
- 探索結果
- 戦闘へ適用された補正
- 戦利品名と発見物名

イベントロジックでは表示文字列を判定キーに使わない。イベント、選択肢、発見物には
言語非依存の安定IDを持たせ、表示文だけを日本語の言語データから取得する。

例:

```text
choice id: cinderwatch_high_ground_scout
ja: 【斥候】高所から偵察
```

英語対応を将来追加しても、日本語UIへ内部IDや英語文を暗黙表示しない。日本語文の欠落は
開発時に検出する。

## 探索テンプレート(2026-08-02実装、Phase 1)

`docs/prompts/exploration_system_improvement_prompt.md`の調査で、本編62地点のうち
`routeOutcomes`(選択肢ごとの個別効果)を実際に定義しているのは15地点だけで、残り47地点は
単一の`cinderwatchOutcome()`へフォールバックしていたことが判明した(地域・地形の違いが
探索結果へ一切反映されない)。この単一フォールバックを、地形テーマ別の8種類
(`ExplorationTemplate`: Forest/Mountain/Mine/Marsh/Ruins/Settlement/Fortification/
OpenField、`include/jf/core/Exploration.hpp`)へ置き換えた。

- `stage.routeOutcomes`に該当choiceの個別設定があれば従来どおりそれを最優先する。
- 無ければ`stage.id`の部分一致(`quarry`→Mine、`blackwater`→Marsh等、
  `explorationTemplateForStageId()`、`Region.cpp`)からテンプレートを推定し、
  そのテンプレートの既定`ExplorationOutcome`を適用する。
- 既存の`ExplorationOutcome`の7フィールドのみを使用し、スキーマ拡張はしていない
  (Phase 2以降で検討: 敵編成入れ替え・報酬/発見物分岐・UI自動生成等)。

`ash_road`のように複数地域で使い回されている汎用`terrainProfileId`は地形の手がかりに
ならないため、`stage.id`側の命名(既に地域・用途を反映した命名規則)をキーにした。

## 兵種専用選択肢の一般化(2026-08-02実装、Phase 2)

調査で、UI側(`ui_exploration.cpp`)がルートC(斥候ルート)のボタン有効/無効を常に
`partyHasFrontierScout()`で判定していた実バグが見つかった。`StageDescriptor::
scoutRouteRequiredClass`で個別に他兵種を要求する15地点では、実際のクリック処理
(`GameApp::chooseExplorationRoute()`)は正しい兵種をチェックしていたのに、ボタンの
表示だけは辺境斥候の有無で決まっていた(誤って有効/無効に見えるボタンが存在した)。

- UIを`app.currentStageScoutRouteRequiredClass()`(実際のゲート判定と同じ値)へ修正。
- ロック時の案内文も固定の「編成に辺境斥候が必要」から、実際に必要な兵種名を
  埋め込む形(`exploration.scout_route_locked`、`{class}`プレースホルダ)へ変更。
- `scoutRouteRequiredClass`未設定の地点(大多数)についても、単純にFrontierScout
  既定にせず、`ExplorationTemplate`ごとに`docs/exploration_system.md`「兵種による
  探索能力」の対応表に沿った既定兵種を割り当てた
  (`explorationTemplateDefaultClass()`、`Exploration.hpp`; Mine/Ruins→辺境工兵、
  Marsh→辺境猟兵、Settlement→暁の衛生兵、OpenField→重装兵、Forest/Mountain/
  Fortification→辺境斥候のまま)。地点固有の`scoutRouteRequiredClass`は引き続き
  最優先。

## UI文言のOutcomeからの自動生成(2026-08-02実装、Phase 2続き)

47地点で使い回されていた固定の効果説明文言(`exploration.frontal_effect`等)を、実際の
`ExplorationOutcome`から組み立てる要約文へ置き換えた(`buildExplorationEffectSummary()`、
`ui_shared.cpp`)。`partyDamage`/`enemiesRemoved`/`enableFreeDeployment`/
`restrictedAutoSpawnMaxColumn`/`extraBarrierCount`/`startingHeatLevel`/
`enableReinforcementWave`の各フィールドのうち既定値でないものだけを「・」区切りで表示し、
どれも既定値なら「戦闘条件に変化なし」を明示する(空Outcomeが未実装なのか意図的な
無効果なのか区別できなかった問題への簡易対応)。灰枝の森は既存の物語的な説明文言に
実際の効果を括弧書きで併記する形にし、それ以外の地点は要約文をそのまま表示する。
`GameApp::explorationOutcomeForChoice(choice)`(新設)がUIから「選ぶとどうなるか」を
プレビューする唯一の窓口。

### テスト可能性の向上(2026-08-02追加)

`buildExplorationEffectSummary()`自体は`ui_shared.cpp`(raylib依存のUI層)にあり、
ヘッドレスな`jf_battle_tests`からは直接検証できない。そこで「どのフィールドが
非デフォルトか」「危険度(tone: Neutral/Benefit/Caution/Danger)」を判定する部分だけを
`jf::summarizeExplorationOutcome()`(`include/jf/core/Exploration.hpp`、jf_lib側の
純粋ロジック)として切り出した。`ExplorationEffectSummary{hasEffect, tokens}`を返し、
各`ExplorationEffectToken{kind, tone, amount}`は文字列もColorも持たない。UI側の
`buildExplorationEffectSummary()`はこのtokenをLocale Keyへ機械的に変換して結合する
だけの薄いラッパーになった。toneは`partyDamage>=3`ならDanger、`enableReinforcementWave`
は常にDanger、敵削減/自由配置はBenefit、拘束配置/開始熱量はCaution、障害物追加は
Neutralという固定ルール(今はまだ文字色には反映していない - 色分け表示は
`docs/prompts/exploration_effect_summary_improvement_prompt.md`の項目2として未着手)。
`tests/test_battle.cpp`にkind/tone/hasEffectの単体・組み合わせテストを追加済み。

### tone→文字色の反映(2026-08-02追加、項目2)

上記の`tone`を実際の描画色へ反映した。`explorationEffectToneColor(ExplorationEffectTone)`
(`ui_shared.cpp`)がBenefit=緑系、Caution=黄橙系、Danger=赤系、Neutral=既存の
`kColorTextMuted`へ変換し、`buildExplorationEffectDisplayTokens()`が各tokenを
`{text, color}`のペアへ変換、`drawExplorationEffectTokens()`が「・」区切り(常に
`kColorTextMuted`)を挟みつつ各tokenをそれぞれの色で描画する。灰枝の森は既存の
物語文言の後に括弧書きで色付きtokenを続ける(`ui_exploration.cpp`の`drawRouteEffect`
ラムダ)。危険度判定ロジック自体(tone決定)は`jf_lib`側にあるため既存テストで
検証済みで、この描画関数自体はraylib依存のためヘッドレステスト対象外(目視未確認)。

## 探索選択肢のランダム化(2026-08-02実装)

`docs/prompts/exploration_randomization_prompt.md`(2026-08-02設計)に基づき、
テンプレートフォールバック地点(本編62地点中47地点、`routeOutcomes`未定義)の
3択が、その地点に到達するたび(厳密には現在のstage idがキャッシュと変わるたび)
再抽選されるようにした。**既存15地点の手作り`routeOutcomes`は対象外**で、常に
固定のまま。

- `jf::ExplorationChoiceOption{id, labelKey, outcome, requiredClass}`
  (`Exploration.hpp`) - 1候補の中身。
- `jf::explorationOptionPool(ExplorationTemplate)` - 各テンプレートの4候補
  (既存の正面/脇道/斥候3つ + 新規候補1つ)。新規候補は8つ追加
  (`exploration.option.*`のLocale Key、`data/locales/{ja,en}.json`)。
- `jf::rollExplorationChoices(tmpl, rng)` - 4候補から2候補を実際にコイントスで
  入れ替える、**枠の役割を固定した**ロジック。スロットA(正面)は常に`pool[0]`
  (兵種ゲートなし)。新規候補が兵種ゲートを持つ場合のみスロットC(斥候)と
  入れ替わり候補になり(かつ`pool[2]`と同じ兵種を要求するよう設計- 例:
  Ruins/Settlementの新規候補は本来の第一候補と別の兵種にしていたが、後で
  同じ兵種に統一)、持たない場合のみスロットB(脇道)と入れ替わる。
  **これによりスロットCの必要兵種は再抽選をまたいで絶対に変わらない** -
  `stage.scoutRouteDisabled`や全既存テストが前提とする「斥候ルートは特定
  兵種でゲートされる」という不変条件を壊さないための設計上の制約。
  - 検討時の失敗: 最初は4候補から3候補を選んでスロット順もシャッフルする
    設計だったが、`ashroad_watch`(砦テンプレート)のテストが約1/2の確率で
    失敗するようになった - 斥候ルートの効果(自由配置)がスロットCから
    別スロットへ移動してしまい、「斥候ルート=自由配置」という各所の前提が
    崩れたため。今の「役割固定・コイントス1回」方式に縮小して解決。
- `GameApp::rolledOptionForChoice(choice)`(private) - 現在のstage idごとに
  キャッシュし、hand-authoredな地点(`stage.routeOutcomes`が空でない)では
  常に`nullptr`を返す。`explorationOutcomeForChoice()`/
  `requiredClassForChoice()`/`explorationChoiceLabelKey()`が全てこれ経由で
  「ロール済みならそれを使う、無ければ既存の固定ロジックにフォールバック」
  という形に一般化された。
- `ui_exploration.cpp`は3スロット全てに`requiredClassForChoice()`による
  ゲート判定を一般化(以前はスロットCだけがゲート対象だった)。

既知の制約: 戦利品/発見物(`computeStageVictoryLoot()`等)は`ExplorationChoice`
(スロット識別子)自体をキーにしており、ロールされた中身とは連動しない - つまり
スロットBの中身が「脇道(HP-2)」と「茂みを盾に(障害物+2)」のどちらであっても、
そのスロットの戦利品テーブルは変わらない。これは今回のスコープ(`ExplorationOutcome`
は変更しない)を守った結果の副作用で、表示された効果と得られる戦利品の結びつきが
今後どこまで必要かは未検討。

## 実装順

1. Exploration Event、Choice、Outcomeのデータモデル
2. PendingLootとPendingDiscoveriesの分離
3. 日本語の探索画面と結果画面
4. 兵種条件と選択不可理由の表示
5. `ExplorationOutcome`から`BattleModifier`への変換
6. 事前ダメージ、敵数、情報公開、初期配置補正
7. 戦闘勝利で地域固有の重要発見をPendingDiscoveriesへ追加
8. 帰還時にDiscovery Registryへ登録
9. 地域専用データとの接続

最初は灰枝の森の3地点を直線で接続し、「誰を編成したかが探索と戦闘の両方を
変えるか」を検証する。その後、地域ごとに地点数と分岐を増やす。
