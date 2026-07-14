# JOJIFrontier 実装状況まとめ

文書種別: **進捗記録**
仕様索引: [`README.md`](README.md)
この文書は現行コードとの差を記録し、正本を上書きしない。

更新日: 2026-07-13

この文書は、企画上の確定仕様ではなく、現在のコードに入っている機能と残作業を記録する。
詳細な設計判断は各仕様書を正とする。

現在のロードマップ位置は**M1 共通契約基盤**。本文に残る`Phase 1`〜`Phase 4`表記は、
再編前の実装履歴を識別するための旧番号であり、新しい実装順には使用しない。対応表は
[`implementation_roadmap.md`](implementation_roadmap.md#旧phase番号との対応)を参照する。

プロジェクト全体の実装順、依存関係、Phase完了条件は
[`implementation_roadmap.md`](implementation_roadmap.md)を正本とする。

## 戦闘

実装済み:

- C++20、raylibによる固定2D戦闘画面
- 3行x8列の論理グリッド
- Player Phase / Enemy Phaseのターン進行
- ユニット選択、移動、攻撃、待機、キャンセル
- 味方マスは通過可能・停止不可、敵マスは通過・停止不可
- 移動可能範囲、攻撃可能範囲、実際の攻撃対象マスの色分け
- 移動確定後に移動範囲を消去
- 地形、移動コスト、ランダム地形生成
- 通常命中100%の物理・魔法ダメージ（茂み上の防御側のみ回避+20%）
- Formation Bonus、Zone of Control、迎撃姿勢など初期兵種能力
- Heavy Spearの直交1マスノックバック
- Hide-Wrapped Gripによる各戦闘最初のノックバック無効
- 状態異常5種(毒・炎上・移動低下・防御低下・よろめき)のデータ基盤・効果処理・地形処理順
  (`jf/core/StatusEffect.hpp`、`jf/battle/StatusEffects.hpp`)。付与する攻撃・スキルはまだ存在しない
- スキル共通基盤: 初期6兵種18スキルのメタデータ、装備2枠、使用回数・クールダウン管理
  (`jf/core/Skill.hpp`、`jf/battle/SkillCharges.hpp`)。個別スキルの戦闘効果は未実装
- 任務目標・戦闘イベント基盤: `EliminateTeam`/`DefeatUnit`/`SecureTile`、AND/ORグループ、
  `UnitDefeatedEvent`を含む戦闘イベント、勝敗評価(`jf/battle/ObjectiveTracker.hpp`)。
  `BattleController`はこの評価結果だけでVictory/Defeatへ遷移する
- M1-A Event Batch完成(`battle_resolution_contract.md`の直近スコープ4項目): `emitUnitDefeatedEvents()`
  が`AliveSnapshot`(unordered_map)自体ではなく`battle.units()`(固定順のvector)を辿るよう修正し、
  同時撃破のEvent発行順が実行ごとに変わっていた不具合(ハッシュ順依存の非決定性)を解消した。
  Phase終了状態処理は`PhaseEnded`/`RoundEnded`発行前に解決済み、勝敗評価は1Batchにつき
  `syncObjectiveProgress()`+`evaluateBattleOutcome()`を1回だけ呼ぶ構成まで確認済み。
  「同一Batchで敵味方同時全滅なら敗北優先」と「同時複数撃破が過不足なくEvent化される」の
  回帰テストを追加。`RootActionId`/`BattleEventEnvelope`による正式なRoot Action分解と
  反応Skillの禁止規則(反応する対象自体が未実装のため対象外)はまだ実装していない
- M1-C Battle Object最小実装(`battle_objects.md`の直近スコープ4項目: 踏査地点、倒木、Exit、
  操作・破壊Event): `jf/battle/BattleObject.hpp`に`BattleObjectDefinition`/`BattleObjectState`/
  `BattleObjectKind`(Marker/Barrier/Device/Container/SpawnPoint/ExitPoint)のデータモデルと
  `validateObjectDefinition()`(不正な組み合わせ3種を検出)を追加。`BattleState`にObject
  登録・配置・検索・`blocksMovement`/`blocksStopping`/`blocksDeployment`/`blocksProjectiles`
  照会を追加し、`Movement.cpp`の到達可能マス計算(経路展開・停止先の両方)と
  `BattleState::moveUnit()`をObjectの通行規則へ対応させた。`jf/battle/BattleObjectResolver.hpp`に
  `resolveObjectAttack()`(物理=STR+Might-防御、魔法=MAG+Might-魔防、下限1、耐久0到達時に
  一度だけDestroyedへ遷移)と`resolveObjectInteraction()`(射程・兵種制限・要求State・
  使用回数上限を検証してから状態遷移)を実装。`BattleEvents.hpp`へ`ObjectStateChangedEvent`/
  `ObjectDestroyedEvent`を追加(Payload variantに追加しただけで、まだ何もこれらを発行する
  呼び出し元は存在しない)。倒木のブロック/破壊、踏査地点の占有共存、Interactの検証規則の
  回帰テストを追加
- M1-D Region Mission接続: 灰枝の森の共通Definition移行・MissionFlowのPending変換・
  BattleControllerの地域非依存は既存設計(`RegionDescriptor`/`StageDescriptor`/
  `GameApp::proceedToCamp()`)で既に満たされていることを確認。`GameApp::proceedToCamp()`に
  `screen_ != Screen::Battle`の防御が欠けており、Camp画面遷移後に再度呼ぶと木材・獣皮の
  Pending Lootと`battlesWon`が二重加算される実バグを発見して修正(他の画面遷移関数と同じ
  「離れる画面をチェックする」パターンへ統一)。二重呼び出し回帰テストを追加
- M2-A 薬草の沢(`herbwater_hollow`、Route Graph2番目の地点)を実装: 探索3択(そのまま通過/
  薬草を採取/[暁の衛生兵]薬草を選別)、`TerrainType::Shallows`(移動コスト2)、地形生成率
  (通常床40%・浅瀬35%・茂み15%・灰地10%)、中央4列への薬草地点2マス自動配置、共通副目標
  「薬草地点確保」(2枚のいずれかで行動終了→勝利で薬草+1、SecureTileのAny-group化を
  1地点固定から複数地点対応へ汎用化)。`StageDescriptor`に`routeOutcomes`(ステージ別の
  探索ルート効果上書き)と`scoutRouteRequiredClass`(Cルートの必須兵種、既定は
  `FrontierScout`、薬草の沢は`DawnChirurgeon`)を追加し、`GameApp::partyHasFrontierScout()`を
  汎用の`partyHasClass()`へ拡張。`ExplorationOutcome`に`restrictedAutoSpawnMaxColumn`
  (自由配置ではなく乱数配置のまま列数だけ絞る効果)を追加。増援(2ターン目の狼1体)、
  暁の衛生兵専用踏査(`herbwater_hollow_surveyed`)、初回薬草地点確保後の継続時+2回復は
  未実装(詳細はimplementation_roadmap.mdのM2-A状態を参照)
- M2-B 折れ木の縄張り(`brokenwood_territory`、灰角大猪ボス)を実装: `UnitClass::AshenhornBoar`
  と専用AI(`takeBoarBossTurn`、`jf/battle/EnemyAI.cpp`)。予告→突進(3/激昂後4マス、
  STR+4-DEF、倒木か盤端で停止)、薙ぎ払い(前上・正面・前下、STR+2-DEF)、HP50%以下での
  激昂(STR8→10)。倒木衝突は`jf/battle/BattleObjectResolver.hpp`(M1-C)のBarrierを再利用し、
  即時破壊・次のEnemy Phase1回休み・DEF5→2とRES1→0(復帰時に元へ戻す)を実装。
  ランダム初期配置(ボスは右2列、味方は左2列、倒木は中央4列でボスと異なる行)、探索2択
  (慎重に/誘導、Cルートは`scoutRouteDisabled`で明示的に無効化)、副目標「倒木衝突」
  (灰角の欠片1)と「無傷」(獣皮1)をAd-hoc方式(Objectiveシステムを介さず
  `BattleState::bossHasCollidedWithBarrier()`と`battle.units()`の生存確認で直接判定)で
  実装。Boss予告Message・攻撃範囲表示等のUIは未配線(このセッション全体の一貫した方針どおり)
- M2-C 地域完了を実装: 新しい`RegionProgress`構造体は作らず、既存の`BaseState::siteAccess`を
  正本として`GameApp::wouldRegionBeCleared()`で3地点(灰枝の林縁・薬草の沢・折れ木の縄張り)の
  Surveyed以上を合成判定する。`ExpeditionState::pendingRegionCompletions`
  (`std::unordered_set<RegionId>`)に地域完了をPendingとして積み、3地点目の勝利時に
  `ashbough_forest_survey_complete`をPending Discoveriesへ追加、`returnToBase()`で
  `completedRegionIds`へ確定する(敗北・撤退では恒久化しない)。これにより既存の
  `regionUnlocked()`が自動的に沈黙した監視所群を解放する。`eligibleForOutpostStage()`を
  拡張し、灰角の大牙1+木材3(Cinderwatch Gateの`ashveil_fang`とOR条件)でも開拓拠点へ
  進めるようにした。Save Checkpointにも`pendingRegionCompletions`を追加
- M2-D 周回短縮を実装: `GameApp::bulkPassSecuredSites()`が現在地から連続するSecured地点を
  無音でまとめて安全通過し、最初の未確保地点またはExitで止まる(灰枝の森はM2で完成)。
  実装中、Exit到達時に`advanceRouteToNextSite()`を無条件に呼ぶと`currentNodeId`がExit
  ノードへ進み、`expeditionComplete()`が前提とする「`currentNodeId`は常に最後に確定した
  Siteを指す」という不変条件を壊して`currentStage()`が例外を投げる実バグを発見・修正した

設定済み・未実装:

- `RootActionId`と順序付き`BattleEventEnvelope`による正式なRoot Action分解(M1-A実装Sliceの
  残り項目。現状は個別のBattleController呼び出し順で同等の効果を得ているのみで、契約が
  定めるデータ構造そのものはまだ存在しない)
- Battle ObjectのBattleController/UI統合: 攻撃対象選択がUnitしか選べず、Objectを対象にできない
  (`resolveObjectAttack()`は関数として存在するが呼び出し元がない)。Interactコマンドの
  BattleController API・UIボタンも未配線。`ObjectStateChangedEvent`/`ObjectDestroyedEvent`を
  実際に発行する呼び出し元も存在しない
- Battle ObjectのBattleFactoryランダム生成統合(`battle_objects.md`「ランダム生成」の手順1〜7)。
  地域書がObject配置を指定してもまだ反映されない
- Battle ObjectのSave Snapshot(Object ID、Definition ID、位置、Team、State、耐久、操作回数、
  乱数配置結果の保存・復元)
- `region_mission_data_contract.md`の`RegionDefinition`/`SiteDefinition`/
  `ExplorationChoiceDefinition`/`RewardGrantId`等の構造体そのものへの移行(M1-D項目1)。
  灰枝の森3地点(薬草の沢・折れ木の縄張り含む)追加後も`StageDescriptor`の軽量拡張
  (`routeOutcomes`、`scoutRouteDisabled`、Ad-hoc副目標フィールド等)で対応できており、
  まだ本格的な移行コストに見合っていない。次地域(灰鉄採石場、5地点)を追加するM9系まで
  持ち越す想定に変更
- 戦闘中増援Wave(`battle_resolution_contract.md`「増援」)。薬草の沢の「薬草を採取」ルートで
  2ターン目に狼1体が増援する仕様が未実装のまま
- 特定Unit限定の踏査Objective(暁の衛生兵専用踏査など)。`ObjectiveTarget`にUnit単位の制約が
  なく、`herbwater_hollow_surveyed`のようなRegionProgress記録の仕組みも未実装
- 遠征継続時だけの1回限りボーナス回復(薬草の沢の「薬草地点確保後、継続時+2」)。
  `continueExpedition()`に地域固有分岐を増やさないという設計原則との兼ね合いで保留
- 薬草の沢「薬草確保後の撤収」をOR主目的として許可する仕様
  (`docs/regions/ashbough_forest.md`「地域共通の勝敗条件」)。現状の主目的は狼全滅の
  EliminateTeamのみで、撤収による代替勝利条件は未実装(ExitPoint/EscapeUnits系の
  Objectiveが必要で、M1-Cでは配置のみ・ゲームプレイ未接続のまま)
- Boss共通基盤(`battle_resolution_contract.md`「Boss段階移行」)。灰角大猪の激昂・突進・
  薙ぎ払いは今回この個体専用のAI関数として実装しており、複数Bossで再利用できる汎用Boss
  Definitionモデルへの一般化はM4「Skill・AI・Boss共通化」の対象
- Boss予告Message・突進予告列・激昂境界の戦闘UI表示(`main.cpp`は未配線)
- Device/Container/SpawnPointの実際の挙動(修理、Wave接続、破壊後Terrain変換)。データモデルの
  種別だけ定義済みで、専用ロジックは未着手
- Root Action単位の完全な行動解決順、同時発生規則のうち増援・Boss段階移行・反応Skill部分
- 通常反撃なし。槍兵の反撃準備だけが行う反応攻撃
- 予告、封鎖、出現直後行動不可、全滅条件、Saveを含む増援Wave
- 状態異常補正、予告固定、段階移行、退場理由を含むBoss共通処理
- 本編後の深層遠征向け複数マスBoss Footprint
- 倒木、装置、Container、増援口、Exitを統合するBattle Object
- 実Resolverと同じ計算を使う攻撃予測・危険予告Overlay
- 任務、役割、小隊予約、撤退、情報制限を含む敵AI共通評価
- 初期6兵種18スキルの対象、射程、Cost、実効果、AI評価、予測表示

正本は[`battle_resolution_contract.md`](battle_resolution_contract.md)、
[`initial_skill_effects.md`](initial_skill_effects.md)、[`enemy_ai_rules.md`](enemy_ai_rules.md)。

## データ契約

設定済み・未実装:

- Facility、Research、RecipeをJSONへ分離する定義型、所有状態、旧ID Alias、起動時検証
- 倉庫上限超過時の受取保留、倉庫整理、放棄確認、原子的な帰還Transaction
- 全地域共通Checkpoint、Node変更時の退避、不正Routeの隔離復旧
- 62地点のSite、Choice、Battle、Mission、Reward、Camp、Messageの横断参照
- 必須ObjectiveとBoss退場理由からPending Region Completionを作り、安全帰還で恒久化する処理
- Attempt/BaseのReward Grant Ledgerによる二重付与防止

正本は[`facility_data_contract.md`](facility_data_contract.md)、
[`inventory_overflow.md`](inventory_overflow.md)、[`expedition_recovery.md`](expedition_recovery.md)、
[`region_mission_data_contract.md`](region_mission_data_contract.md)。

表示:

- 横長の床パネルを使った固定サイドビュー
- プレイヤーは青、敵は赤の円形プレースホルダー
- ユニット名を上、HPバーを下へ表示
- 選択ユニットの黄色い円形アウトライン
- HUDを画面端へ集約

## 共通UI

実装済み:

- 日本語対応Fontの描画と全画面共通の可読性Scale
- 共通Panel、Button、配色
- UTF-8文字境界を壊さない省略と自動折り返し
- Tooltipを論理解像度内へ自動配置
- Tooltip本文を最大幅560pxで折り返し、実描画Font高から枠の高さを自動計算
- 施設概要、施設Node効果、必要素材、不足理由の可変高Tooltip

未実装:

- `UiTheme`、`UiButton`、`TextLayout`、`ScrollPanel`の`main.cpp`外へのComponent分離
- 無効Buttonの理由Tooltipを全画面へ統一適用
- 遠征準備画面の4人・Loadout・消耗品6枠・探索道具2枠・地域情報Tab
- 62地点の地域ルート画面
- 正式な戦闘結果画面

## 兵種と武器

現在のデータにある主要兵種:

- March Captain
- Veteran Guard
- Watch Archer
- Frontier Scout
- Spearman
- Dawn Chirurgeon

武器:

- 兵種の基本武器
- Iron Spear
- Long Spear
- Heavy Spear
- Guard Spear
- 武器耐久は不採用
- Spearman向け分岐武器と調整特性を拠点で管理

## 遠征と探索

実装済み:

- 拠点準備、4人編成、6枠の遠征バッグ
- `jf::RegionId`/`StageDescriptor`/`RegionDescriptor`による軽量地域データ基盤
  (`jf/core/Region.hpp`)。Cinderwatch Gateの旧stage直書き実装をデータへ完全移行済み
  （既存挙動を回帰テストで確認）
- Cinderwatch Gateの探索A/Bルート
- Frontier Scout編成時のCルートと戦闘前自由配置
- 左3列、通行可能地形、同一マス不可の配置制限
- 探索結果を`ExplorationOutcome`として戦闘生成へ渡す構造
- Pending LootとPending Discoveriesの分離
- 勝利後キャンプ、続行、帰還、敗北時ロスト
- 帰還時だけ恒久倉庫とDiscovery Registryへ確定
- 遠征中のHP、消耗品、戦闘不能の引き継ぎ
- 灰枝の森・灰枝の林縁(1地点目、campaign_regions.mdの第1地域)を実装: 狼4体
  (急行ルートのみ3体)、群れAI(`jf/battle/EnemyAI.cpp`)、探索3択とルート別報酬、
  踏査地点(SecureTile副目標)のランダム有効配置と成功報酬。
  `GameApp::startExpedition(RegionId::AshboughForest)`からフル遠征が動作する
- 地域アンロック(docs/region_unlocks.md正本): 灰枝の森(第1地域)は初期解放、
  沈黙した監視所群(`RegionId::CinderwatchGate`、第2地域、表示名も正式な
  「沈黙した監視所群」へ修正済み)は`BaseState::completedRegionIds`に
  `RegionId::AshboughForest`が入るまで`GameApp::startExpedition()`が拒否する
  (`jf::regionUnlocked()`、`GameApp::isRegionUnlocked()`/`regionSummaries()`)。
  `completedRegionIds`は地域専用の最終Objective達成＋安全帰還でのみ追加される想定
  だが、その仕組み(灰角大猪撃破＋3地点確保)はPhase 4未実装のため、現状は
  `completedRegionIds`へ何も追加されず沈黙した監視所群は恒久的に未解放のまま。
  灰枝の林縁1戦の勝利・安全帰還だけでは絶対に解放されない(以前の実装は
  `SiteAccessState`から地域解放を推測しており、1地点だけで解放される誤りが
  あったため修正した。`regionCleared()`は将来のSchema移行専用ヘルパーとして残置)。
  `startExpedition()`の既定値もロック中の`CinderwatchGate`から、常に解放済みの
  `AshboughForest`へ変更した(旧既定値は新規ゲームで必ず失敗する地雷だった)
- Phase 3「周回・地域経路の開拓」の縦切り(灰枝の林縁のみ): `jf::SiteAccessState`
  (未踏/踏査済み/経路確保済み、`BaseState::siteAccess`に`Region::siteAccessKey()`で
  永続化)。昇格は`ExpeditionState::pendingSiteAccessUpdates`にPendingとして積み、
  `GameApp::returnToBase()`の安全帰還時だけ確定（昇格のみ、格下げなし）。敗北・
  `retireExpedition()`は`ExpeditionState`ごと破棄するため恒久化しない。経路確保済み
  以降は`GameApp::chooseExplorationRoute()`が使えなくなり、代わりに
  `chooseSafePassage()`（戦闘・探索3択・報酬すべて省略して即Campへ）と
  `chooseReconnaissance()`（新しい盤面で通常戦闘、通常素材の基本報酬のみ・
  踏査副目標の再取得なし）を使う。セーブデータにも`siteAccess`
  （`BaseState`側、恒久）と`pendingSiteAccessUpdates`（`ExpeditionCheckpoint`側、
  遠征中断セーブ）を追加済み。schemaVersionは据え置き(2)のまま両フィールドとも
  省略可能な追加項目として読み込み、これらのキーを含まない旧セーブJSONの
  読込テストも追加済み（欠落時は空/Unknownへ安全にデフォルト）
- 灰枝の林縁の地形生成を正式値の通常床65%・茂み15%・灰地20%へ統一。茂みはSeedごとに
  2〜4マスへ正規化し、100 Seedの決定論・個数回帰テストを追加
- Phase 3.5「プレイヤー到達性の縦切り」実装順1〜6: Base画面に遠征先選択欄を追加し
  (`GameApp::regionSummaries()`/`isRegionUnlocked()`)、未解放地域は理由付きの
  無効ボタンで表示。Exploration画面を`app.expedition().regionId`で分岐して
  灰枝の林縁専用のA/B/C文言・状況説明を表示し、地点状態(未踏/踏査済み/経路確保済み)
  を常時表示。経路確保済み時は3択の代わりに「安全路を進む」/「危険区域を再調査」
  ボタンへ切り替わる。新規追加した日本語文言はすべてフォントのグリフ収集リストへ登録済み
- `GameApp::chooseSafePassage()`の修正: 旧実装は`stageIndex`に関わらず常に新規
  フルHPパーティで戦闘を再生成しており、複数地点地域が実装されると無料全回復に
  なるバグがあった。`stageIndex == 0`のみ新規生成、それ以外は`continueExpedition()`
  と同じ`createScenarioContinuationBattle()`で現在の生存者HPを引き継ぐよう修正。
  現状は灰枝の森が1地点のみのため実挙動への影響はまだないが、Phase 4での複数地点化
  に備えた修正
- Phase 4手順1〜5: `RegionRouteGraph`/`RouteProgressSnapshot`を実装し、灰枝の森の
  入口→灰枝の林縁→薬草の沢→キャンプ→折れ木の縄張り→出口を登録。灰枝の森では
  `currentNodeId`を進行正本とし、勝利・安全通過後の続行は次地点のExplorationへ戻る。
  HP・戦闘不能・Bag・Pending・通過履歴をExploration/Camp両Checkpointへ保存し、薬草の沢で
  再読込しても林縁へ戻らない。安全通過は戦闘・報酬・勝利数・回復を発生させない。
  薬草の沢と折れ木の縄張りは地点登録のみで、未実装の林縁戦を誤起動しない到達画面を表示する
- `regionIdFromStringStrict()`: 不明な地域ID文字列を`CinderwatchGate`へ黙って
  変換していた挙動を修正。`BaseState.completedRegions`内の不明IDはSave全体を
  読込失敗にし(恒久データのため)、`ExpeditionCheckpoint.regionId`内の不明IDは
  その中断セーブだけを破棄する(中断セーブは再生成可能なため)

未実装:

- Phase 3.5実装順7: 通常操作(Base選択→出発→勝利→帰還→再出発→安全通過/再調査)の
  実機往復試験。サンドボックス環境のためGUIスクリーンショットでの目視確認ができておらず、
  ビルド成功・回帰テスト通過・文字コードカバレッジ確認のみで代替している
- 灰枝の森の残り2地点（薬草の沢、折れ木の縄張り）の探索・戦闘内容と灰角大猪。
  複数地点の順序制御と到達画面は実装済み
- 灰角大猪の突進予告、薙ぎ払い、激昂、倒木衝突
- `RegionProgress`による3地点合成の地域目標・Discovery管理
- `campaign_route_graph.md`で62地点の接続は設計済み。実行時モデルは灰枝の森の直線経路のみ実装済みで、
  他地域の分岐・合流・条件判定は未実装
- 正式仕様へ追加した、地域入口から連続する経路確保済み地点の一括通過、既知Campでの停止選択、
  地点別`reconLoot`（初回通常素材の50〜70%）は未実装。現行UIは地点ごとに安全通過を選ぶ
- Pending加入候補、候補重複防止、安全帰還後の候補登録、集会所加入
- 残り7種のObjectiveKind（`BattleObjectState`が必要な装置・破壊・防衛・脱出系）
- 戦闘開始画面・HUD・結果画面のUI、`MissionFlow`の報酬台帳

設定済み・未実装の接続仕様:

- [`route_graph_data.md`](route_graph_data.md): 62地点を実行するGraphデータ、Branch、Condition、Save
- [`region_unlocks.md`](region_unlocks.md): 10地域の解放、攻略完了、安全帰還、再訪
- [`expedition_flow.md`](expedition_flow.md): 遠征準備から結果、Camp、安全帰還、敗北までの状態遷移
- [`battle_objects.md`](battle_objects.md): 任務Object、耐久、操作、生成、Save
- [`combat_forecast.md`](combat_forecast.md): 攻撃予測、危険範囲、情報公開
- [`enemy_ai_rules.md`](enemy_ai_rules.md): 敵候補評価、役割連携、撤退、決定論

## アイテム

現行実装（効果・使用場所・行動コストの正本は
[`battle_system.md`](battle_system.md#expedition-item-specification)）:

- 救急セット
- 野戦治療キット
- 救命包
- 野営食
- 帰還信号弾
- 防護板

遠征開始時に持ち込みを選び、遠征中は原則として自動補充しない。

設計のみ・未実装:

- 万能薬
- 投擲火炎壺、煙幕筒、鉄杭、閃光筒
- 探索道具と保護箱

## 拠点と施設

実装済み:

- 野営地から開拓都市までの`OutpostStage`
- 作戦テントと共同テントを常設基礎設備として扱う
- 開拓拠点の有効施設枠2
- 訓練場、簡易鍛冶台、救護テント、工作台
- 施設ノードの段階、Discovery、前提ノード、素材条件
- 建設、解体、無料再建
- 解体時に各建設素材の50%を端数切り捨てで返却
- 解放済み技術は解体後も保持
- 施設費用の複数素材対応と一括検証後の消費
- 鍛冶台稼働中のみ装備・調整変更可能
- 分岐武器に対応レシピの解放を要求

上記の建設・解体・再建と「鍛冶台稼働中のみ装備変更」は現行実装であり、正式仕様では廃止予定。
正式仕様は[`base_development.md`](base_development.md)の「建設済み施設は常時利用可能」で、未実装差分は次。

- `constructedFacilityIds`への一本化
- 解体、50%返却、再建費の廃止
- 稼働・停止・施設枠UIの廃止
- 所有済み装備・スキル・特性・持込品を常に準備可能
- Schema 2の`builtNodeIds`からSchema 3への移行
- 建設・研究・所有の3状態を別データとして保持
- 研究済み分岐は恒久利用可能とし、追加の分岐有効化枠を設けない
- 所有済み装備・スキル・特性・道具の装備と持込を常に許可
- 施設Lv表示を廃止し、開拓段階・建設状態・研究済み分岐数を表示

確定済み初期費用:

全施設研究ノードの安定ID・効果・Discovery・素材・前提・解放地域・種別は
[`facility_research.md`](facility_research.md)で設定完了。共同研究4系統も設定済みだがコード未実装。
製作・所持ルールは[`item_system.md`](item_system.md)で設定完了し、消耗品1個製作、各99個上限、
武器と特性の実物個数制、探索道具の恒久一意所有を採用する。現行コードのバッグ直結方式とは未接続。
施設一覧、施設詳細、建設確認、研究詳細・確認、製作画面は
[`facility_ui.md`](facility_ui.md)で設定完了。現行UIの施設Lv、施設枠、解体・再建操作は正式仕様では
廃止予定であり、新UIへの置換はPhase 8で行う。

| 施設 | 費用 | 追加条件 |
|---|---|---|
| 救護テント | 木材2、薬草2 | 薬草群生地Discovery |
| 訓練場 | 木材3、獣皮2 | なし |
| 工作台 | 木材3、獣皮1 | なし |
| 簡易鍛冶台 | 木材2、獣皮1 | なし |

## セーブ

実装済み:

- Schema 2のJSONセーブ
- Schema 1の兵種単位装備からSchema 2のユニット単位装備への移行
- 拠点、倉庫、Discovery、施設、編成、武器、特性、言語
- デスクトップのローカルファイル
- 一時ファイル、通常セーブ、バックアップ
- 破損時のバックアップ読込
- Web IDBFS / IndexedDB接続
- 恒久状態変更時のオートセーブ
- 探索開始時とキャンプ時の簡略版遠征中断セーブ（`regionId`含む）
- Export / Import（`exports/`/`imports/`フォルダ経由、`.preimport.bak`退避）

未実装:

- 保存状態HUDと破損復旧の専用選択画面（破損時のバックアップ自動読込は実装済み）
- Schema 2以降のスキーマ移行処理
- Web同期完了を待つ処理
- Emscripten実ビルドとブラウザ更新試験

## ローカライズ

正式仕様: [`localization.md`](localization.md)

実装済み:

- 英語と日本語を切り替え可能
- 選択言語をセーブ対象に含める
- 施設名と主要HUD文言を両言語で表示

未実装:

- `data/locales/ja.json`と`en.json`を使うLocale正本
- C++内の`kJa...`、`pick(en, ja)`、表示文直書きの撤去
- Key集合、引数、欠字、`??`、U+FFFD、文字化けを検出する`check_localization`
- 全兵種、素材、施設、状態異常、地域名のLocale Key統一

## 未実装の主要画面仕様

- [`battle_results_screen.md`](battle_results_screen.md): Objective、Pending差分、損耗、地域進行を表示する
  読取専用の戦闘結果画面
- [`expedition_preparation_screen.md`](expedition_preparation_screen.md): 4人、装備、Skill、特性、連携作戦、
  消耗品6枠、探索道具2枠、地域情報と警告を確定する遠征準備画面
- Tutorialと難度選択は後工程。現時点は難しめの標準難度だけを正本とする

## 検証状況

- デスクトップ通常ビルド成功
- Debugビルド成功
- 戦闘、施設、装備、セーブJSON、破損復旧テスト成功
- `git diff --check`成功
- Emscripten環境がローカルにないためWeb実ビルドは未確認
- 修正済み: デフォルトビルド type(RelWithDebInfo)が`NDEBUG`を付与し、`jf_battle_tests`内の
  `assert()`（副作用を持つ呼び出しを含む）が無効化されていた。`CMakeLists.txt`の
  `jf_battle_tests`ターゲットへ`-UNDEBUG`を追加して修正し、それまで検証されていなかった
  既存テスト4件の不具合も合わせて修正した
- `tests/test_battle.cpp`は`NDEBUG`が残っていればCompile errorにし、CMakeはClang/GCCの
  `-UNDEBUG`とMSVCの`/UNDEBUG`を切り替える。修正前のTest成功記録は回帰保証に使用しない

## 次の優先候補

1. Phase 3.5実装順7: 上記で実装済みのBase画面地域選択・Exploration画面分岐・安全路/
   再調査ボタンを、実機でのGUI往復操作（出発→勝利→帰還→再出発→安全通過/再調査）で
   目視確認する（サンドボックス環境のため未確認）
2. Phase 4手順6〜7: 薬草の沢の探索3択、薬草、増援、踏査、薬草マス回復
3. 状態異常・スキルを実際に付与する攻撃・スキル効果の実装（データ基盤のみ先行済み）
4. Web保存の同期完了状態と保存HUD、破損復旧の専用選択画面
5. Schema 2以降のスキーマ移行基盤
6. 施設ID、レシピ、装備の読込検証強化
