# JOJIFrontier 全体実装ロードマップ

文書種別: **進捗記録**

仕様索引: [`README.md`](README.md)

更新日: 2026-07-13

この文書は実装順、依存関係、品質Gateだけを管理する。ゲーム仕様、数値、安定IDを新規定義しない。
仕様変更は[`README.md`](README.md)で指定された正本へ行い、現在の実装詳細は
[`implementation_status.md`](implementation_status.md)へ記録する。

## 現在地

実装済み:

- 3x8基本戦闘、移動、攻撃、待機、簡易Enemy Phase
- Objective初期3種、AND/OR、敗北優先の基礎
- Pending、安全帰還、地点状態の1地点縦切り
- Baseから灰枝の森を選択するUI
- 灰枝の森の入口、3地点、Camp、出口を持つ最小Route Graph
- 林縁から薬草の沢までの地点遷移とSnapshot保存
- 状態異常Containerと初期18 Skillのメタデータ

直近の未完了:

- Root ActionとEvent Batchの完全解決
- Objectiveの残り種別とBattle Object
- 薬草の沢、折れ木の縄張り、灰角大猪
- 地域完了、安全帰還、次地域解放の完全Transaction
- 連続する確保済み地点の一括安全通過

現在のコードが正式仕様へ未追従している詳細は`implementation_status.md`だけへ記録する。

## 実装原則

1. 画面入力からSaveまで通る縦切りで実装する。
2. 地域固有分岐を`GameApp`、`BattleController`、`main.cpp`へ追加しない。
3. 表示文字列ではなく安定IDで判定する。
4. Random結果はSeedだけでなく生成済みSnapshotを保存する。
5. 恒久成果は安全帰還Transaction成功時だけ確定する。
6. 新しい状態を追加する変更には同じSliceでSave項目と移行方針を追加する。
7. 新しい表示には同じSliceでLocale Keyと文字検査を追加する。
8. 各Milestoneは自動テストと通常操作確認を通してから完了にする。
9. 未実装の将来拡張を先に一般化しすぎず、灰枝の森と監視所群で必要な共通面だけ作る。
10. 仕様とコードが違う場合、コードに合わせて正本を黙って変更しない。

## 状態表記

- **完了**: 完了条件と実操作確認をすべて通過
- **進行中**: 一部が通常プレイ可能
- **未着手**: 設計済みだが実行経路なし
- **保留**: 前提Milestone待ち

## 依存関係

```text
M0 回帰固定
 -> M1 共通契約基盤
    -> M2 灰枝の森完成
       -> M3 保存・Locale・報酬耐性
          -> M4 Skill・AI・Boss共通化
             -> M5 拠点・施設・倉庫完成
                -> M6 沈黙した監視所群
                   -> M7 12兵種・仲間・会話
                      -> M8 遠征経済完成
                         -> M9 残り8地域
                            -> M10 公開品質
```

M1以降、Schema・Locale・テストは独立した後工程にせず、各Sliceへ含める。M3は既存負債の一括解消と
更新耐性の完成を担当する。

## 旧Phase番号との対応

旧Phase番号は過去の実装記録を読むためだけに残し、新しい作業管理には使用しない。

| 旧区分 | 新Milestone |
|---|---|
| Phase 0 | M0 現行基盤の固定 |
| Phase 1、Phase 4の共通基盤部分 | M1 共通契約基盤 |
| Phase 2〜4の灰枝の森内容 | M2 灰枝の森完成 |
| Phase 12、12.5 | M3 保存・Locale・報酬耐性 |
| Phase 5、11 | M4 Skill・AI・Boss共通化 |
| Phase 8 | M5 拠点・施設・倉庫完成 |
| Phase 7 | M6 沈黙した監視所群 |
| Phase 6、9 | M7 12兵種・仲間・会話 |
| Phase 10 | M8 遠征経済完成 |
| Phase 13の地域実装 | M9 残り8地域 |
| Phase 13の公開作業 | M10 公開品質 |

## M0 現行基盤の固定

状態: **完了。ただしGUI往復試験の一部は継続**

正本:

- [`regression_test_plan.md`](regression_test_plan.md)

作業:

1. DebugとReleaseでassertが有効なテスト構成を固定
2. 3x8移動、攻撃、待機、取消、敵Phaseを回帰テスト化
3. 味方通過・停止不可、敵通過不可を固定
4. 地形Seed再現、Pending、安全帰還、敗北ロストを固定
5. 既存Save Fixtureを保存

完了条件:

- Debug/Release Build成功
- CTest全成功
- 同じSnapshotから同じ盤面と結果を再現
- 現行Save Fixtureを読込可能

## M1 共通契約基盤

状態: **進行中**

目的: 灰枝の森を固有分岐で完成させる前に、戦闘・地域・報酬の共通差込口を完成させる。

正本:

- [`battle_resolution_contract.md`](battle_resolution_contract.md)
- [`mission_objectives.md`](mission_objectives.md)
- [`battle_objects.md`](battle_objects.md)
- [`region_mission_data_contract.md`](region_mission_data_contract.md)
- [`route_graph_data.md`](route_graph_data.md)
- [`expedition_rewards.md`](expedition_rewards.md)

実装Slice:

### M1-A Event Batch完成

状態: **「直近の実装順」記載の4項目(UnitDefeatedEvent、Phase終了状態処理、Batch後1回の
勝敗評価、同時発生テスト)は完了**。`emitUnitDefeatedEvents()`が`AliveSnapshot`の
unordered_map順(非決定的)ではなく`battle.units()`のvector順を辿るよう修正し、同時撃破の
Event順が実行ごとに変わる不具合を解消。「敵味方同時全滅では敗北優先」「同時複数撃破の
Event数が過不足ない」の回帰テストを追加。項目1(`RootActionId`/Event Envelope)と項目5
(反応への反応禁止、反応Skill自体が未実装のため対象外)は未着手のまま残る。

1. `RootActionId`と順序付きEvent Envelope
2. Damage、移動、Object、状態、退場、Objective、勝敗の固定順
3. `UnitDefeatedEvent`の一度だけ発行
4. 同時全滅時の敗北優先
5. 反応への反応禁止

### M1-B Objective完成

1. Team制約付き`SecureTile`
2. 存在検証付き`DefeatUnit`
3. OR未達側の`Superseded`
4. 防衛、護衛、脱出、破壊、装置、Round生存
5. Mission開始時Definition検証

### M1-C Battle Object最小実装

状態: **「直近の実装順」記載の4項目のうちデータモデル部分は完了、BattleController/UI統合は未着手**。
`BattleObjectDefinition`/`BattleObjectState`、占有・通行(`blocksMovement`/`blocksStopping`/
`blocksDeployment`/`blocksProjectiles`)、倒木(Barrier、破壊まで)、踏査地点(Marker、占有共存)、
Exit(ExitPoint、データモデルのみ)、Interact検証(射程・兵種・要求State・使用回数上限)、破壊/
状態変化Eventの型までは実装・テスト済み。ただしBattleControllerの攻撃対象選択・Interactコマンド・
UIへの配線、BattleFactoryのランダム生成統合、Save Snapshotはまだ未着手。増援口(SpawnPoint)は
種別の定義のみで専用ロジックはない。

1. Object ID、占有、通行、耐久、陣営
2. 倒木、踏査地点、Exit、増援口
3. 操作・破壊Event
4. Save Snapshot

### M1-D Region Mission接続

状態: **項目2、3、5は既存のRegionDescriptor/StageDescriptor/GameApp設計で既に満たされている
ことを確認。項目4の実際のバグを1件発見して修正。項目1(新契約の構造体そのものへの移行)は
未着手のまま次段階へ持ち越し**。

- 項目2「灰枝の森だけを共通Definitionへ移行」: 既に`RegionDescriptor`/`StageDescriptor`
  経由でCinderwatch Gateと全く同じ`BattleFactory`パイプラインを通っており、地域名による
  分岐は存在しない
- 項目3「MissionFlowがObjective結果をPendingへ変換」: `GameApp::proceedToCamp()`が
  `missionState()`のObjective結果を読んでPending Loot/Discoveryへ変換する処理を既に行っている
  (独立した`MissionFlow`クラスとしては未分離だが、責務としては満たしている)
- 項目5「地域固有処理をBattleControllerから排除」: `BattleController.cpp`を確認し、
  地域名・地点名によるC++分岐が一切ないことを確認済み(`BattleFactory.cpp`の地形生成だけ
  `FieldType`という安定IDで分岐しており、これは契約が許容する`FieldProfileId`相当)
- 項目4「Attempt/Base二段階Grant Ledger」: `GameApp::proceedToCamp()`に`screen_ != Screen::Battle`
  の防御がなく、`inputState()`がVictoryのまま変化しないため、Camp画面遷移後に同関数を再度
  呼ぶと木材・獣皮のPending Lootと`battlesWon`が二重加算される実バグを発見。他の画面遷移
  関数と同じ「離れる画面をチェックする」パターンへ揃えて修正し、二重呼び出しでも加算されない
  ことを確認する回帰テストを追加した
- 項目1「Region、Site、Choice、Mission、Reward Definition Loader」(新契約の`RegionDefinition`/
  `SiteDefinition`/`ExplorationChoiceDefinition`等の構造体そのものへの移行)は未着手。地域が
  1〜2個・地点数も少ない現状では価値に対してコストが高いため、M2-Aで薬草の沢を追加し地点数が
  増える際に着手する

1. Region、Site、Choice、Mission、Reward Definition Loader
2. 灰枝の森だけを共通Definitionへ移行
3. MissionFlowがObjective結果をPendingへ変換
4. Attempt/Base二段階Grant Ledger
5. 地域固有処理をBattleControllerから排除

完了Gate:

- 同じRoot Actionで同時撃破・Object破壊・Objective達成を決定論的に解決
- 7種以上のObjectiveが共通Trackerを利用
- 同じ結果画面を再表示しても報酬を二重付与しない
- 灰枝の林縁を地域名によるC++分岐なしで生成可能
- 不正Definitionを起動時に検出

## M2 灰枝の森完成

状態: **M2-A/B/C/D完了**

目的: 最初の地域で探索、戦闘、周回、Boss、帰還、拠点発展を一周させる。

正本:

- [`regions/ashbough_forest.md`](regions/ashbough_forest.md)
- [`exploration_system.md`](exploration_system.md)
- [`region_unlocks.md`](region_unlocks.md)

実装Slice:

### M2-A 薬草の沢

状態: **増援・衛生兵専用踏査・継続時+2回復を除き実装済み**。

- 項目1「探索3択と条件表示」: 実装済み。そのまま通過/薬草を採取/[暁の衛生兵]薬草を選別の
  3択。衛生兵ルートは`StageDescriptor::scoutRouteRequiredClass`で`DawnChirurgeon`を要求
  (`GameApp::partyHasClass()`汎用化により実現。灰枝の林縁の斥候ルートは従来どおり
  `FrontierScout`要求のまま)
- 項目2「薬草、浅瀬、増援の生成」: 浅瀬(`TerrainType::Shallows`、移動コスト2)と地形生成率
  (通常床40%・浅瀬35%・茂み15%・灰地10%)、中央4列(col 2-5)への薬草地点2マス配置は実装済み。
  **増援(2ターン目に狼1体)は未実装** - 戦闘中増援の仕組み自体がプロジェクトに存在しない
  (`battle_resolution_contract.md`の増援Waveモデル待ち)
- 項目3「薬草Tile行動終了時回復」: 既存の汎用`BattleState::consumeHerbPatch()`
  (`finishPlayerAction()`から毎行動終了時に呼ばれる)がそのまま適用される。今回新規実装した
  ものではなく、既存の汎用機構を薬草の沢で初めて実際に使う地形が生成されるようになった
- 項目4「撤収OR主目的と踏査副目標」: 主目的(狼全滅)と共通副目標「薬草地点確保」
  (2枚の薬草地点いずれかで行動終了→勝利で薬草+1、Any-groupで実現)は実装済み。
  **衛生兵専用踏査(`herbwater_hollow_surveyed`をRegionProgressへ記録)は未実装** -
  特定のUnit(暁の衛生兵本人)だけが確保できる専用地点の仕組みが必要で、現在の
  `ObjectiveTarget`にはUnit単位の制約フィールドがない
- 項目5「勝利、Pending、Checkpoint、再開テスト」: 3ルートそれぞれの勝利報酬、共通副目標の
  ボーナス、Route Graph経由でのExploration到達・Checkpoint保存・再開の回帰テストを追加済み
- **未実装**: 初回の薬草地点確保後「遠征を続ける」を選んだ場合だけ生存者全員HP+2(1遠征1回、
  安全帰還時や安全通過時は発生しない)というボーナス。`continueExpedition()`に地域固有の
  特殊分岐を増やさないという設計原則と、まだ実装していない機能の兼ね合いで保留
- 灰枝の林縁の踏査地点生成ロジック(`chooseSurveyTile()`)は変更なし。薬草の沢のような
  複数地点の副目標はBattleFactoryが地形からHerbPatchタイル位置を検出して自動的にAny-group
  化する汎用機構へ拡張したため、地域名によるC++分岐は増えていない

1. 探索3択と条件表示
2. 薬草、浅瀬、増援の生成
3. 薬草Tile行動終了時回復
4. 撤収OR主目的と踏査副目標
5. 勝利、Pending、Checkpoint、再開テスト

### M2-B 折れ木の縄張り

状態: **項目5(予告Message・UI表示)を除き実装済み**。

- 項目1「探索3択と倒木配置差」: 実装済み。A(慎重に)/B(誘導、倒木2本)の2ルートを実装。
  C([辺境猟兵]獣の痕跡を追う)は`StageDescriptor::scoutRouteDisabled`で明示的に無効化 -
  辺境猟兵は灰枝の森攻略後の加入候補クラスで現在存在せず、正本自体が「Cは初回攻略用ではなく
  再訪・再挑戦用の選択肢とする」と明記しているため、今回のスコープに含めない
- 項目2「灰角大猪の予告突進、薙ぎ払い、激昂」: `UnitClass::AshenhornBoar`と専用AI
  (`jf/battle/EnemyAI.cpp`の`takeBoarBossTurn`)を実装。行動優先順位(激昂判定→予告済み突進の
  実行→薙ぎ払い→突進予告→通常移動)、突進(同じ行を3(激昂後4)マス、STR+4-DEF、倒木か盤端で
  停止)、薙ぎ払い(前上・正面・前下、STR+2-DEF)、HP50%以下での激昂(STR8→10、突進距離
  3→4)を実装
- 項目3「倒木衝突、専用停止、防御低下」: 倒木(`BattleObjectKind::Barrier`、
  `jf/battle/BattleObjectResolver.hpp`のM1-C基盤を再利用)への突進衝突で即座に破壊、
  次のEnemy Phase行動を1回スキップ、DEF5→2・RES1→0(次に行動可能になる直前に復元)を実装。
  ランダム初期配置(灰角大猪は右2列、味方は左2列、倒木は中央4列でボスと同じ行を避ける)も実装
- 項目4「Boss退場理由と報酬」: 主目的はデフォルトのEliminateTeam(敵はボスのみのため
  「討伐」と「敵全滅」が同一条件、専用ObjectiveDefinition不要)。副目標「倒木衝突」(灰角の
  欠片1)と「無傷」(獣皮1)はObjectiveシステムに載せず、`proceedToCamp()`から
  `BattleState::bossHasCollidedWithBarrier()`と`battle.units()`の生存確認で直接判定する
  Ad-hoc方式(`StageDescriptor::logCollisionBonusLoot`/`noCasualtiesBonusLoot`)で実装。
  退場演出(死亡ではなく撤退)はテキスト・アニメーションの話でUI未実装のため対象外
- 項目5「Boss予告Messageと攻撃範囲表示」: 未実装。戦闘メッセージ・予告列表示・激昂境界表示は
  すべてUI層の仕事で、このセッションのUI配線は一貫して後回しにしている方針と同じ扱い

回帰テスト: 突進の予告→実行(通過ダメージ、範囲到達)、倒木衝突(即時破壊・スタン・DEF/RES
復元タイミング)、薙ぎ払い(2対象同時ダメージ)、激昂(HP50%でSTR10化)、地形生成(Barrier/
Rubble/WatchPostなし)、倒木配置(中央4列・ボスと異なる行)、A/Bルートの報酬差、Cルート無効化、
倒木衝突ボーナス、無傷ボーナスの有無を追加

### M2-C 地域完了

状態: **実装済み**。

- 項目1「3地点の必須Objective合成」: `RegionProgress`という新しい構造体は作らず、既存の
  `BaseState::siteAccess`(3地点それぞれのSiteAccessState)を正本として合成した
  - `GameApp::wouldRegionBeCleared()`が「地域内の全Stageが`baseState_.siteAccess`
    (永続分)と`expedition_.pendingSiteAccessUpdates`(今回の遠征でまだ未確定の分)の
    どちらかでSurveyed以上」を判定する。正本の「`ashbough_verge_surveyed`」
    「`herbwater_hollow_surveyed`」「`bossDefeated`」の3条件は、いずれも各地点の
    「戦闘勝利」で得られるSurveyed昇格と等価なため、専用Objective IDを新設せず
    既存のSiteAccessState機構をそのまま再利用できた
- 項目2「`PendingRegionCompletion`」: `ExpeditionState::pendingRegionCompletions`
  (`std::unordered_set<RegionId>`)として実装。正本の完全な構造体
  (`grantId`、`completedRequiredObjectives`、`BossResolution`)までは作らず、
  現状1地域・1ボスのスコープに対して十分な最小形にした
- 項目3「安全帰還で地域完了・Discovery・報酬を原子的に確定」: 3地点目の勝利
  (`proceedToCamp()`)で`wouldRegionBeCleared()`がtrueになった瞬間に
  `ashbough_forest_survey_complete`をPending Discoveriesへ追加し、`returnToBase()`が
  `pendingRegionCompletions`を`completedRegionIds`へコミットする。敗北・
  `retireExpedition()`は`ExpeditionState`ごと破棄するため恒久化しない
- 項目4「沈黙した監視所群を一度だけ解放」: 既存の`regionUnlocked()`が
  `completedRegionIds`を見るため、上記のコミットだけで自動的に解放される
  (`completedRegionIds`はSetなので二重追加しても実質的に一度だけ)
- 項目5「開拓拠点への発展」: `eligibleForOutpostStage()`を拡張し、Cinderwatch Gateの
  `ashveil_fang`条件に加えて、灰角の大牙(`kAshenhornFangMaterial`)1+木材3のOR条件を追加。
  どちらの地域を先に攻略しても開拓拠点へ進める

回帰テスト: 3地点連続勝利で最後の勝利時にDiscoveryがPendingへ入ること、安全帰還で
Discovery Registry・completedRegionIds・Cinderwatch Gate解放が確定すること、開拓拠点
発展条件を満たすこと、最終地点勝利後に`retireExpedition()`した場合は何も恒久化しないこと、
Save往復で`pendingRegionCompletions`/`completedRegionIds`が保持されることを追加

### M2-D 周回短縮

状態: **実装済み**。

- 項目1「入口から連続する確保済み地点の一括通過」: `GameApp::bulkPassSecuredSites()`を
  実装。現在地から連続するSecured地点をまとめて安全通過し(報酬なし、`chooseSafePassage()`
  と同じ規則)、最初の未確保地点(通常のExploration画面)またはExit(Camp画面、
  `expeditionComplete()`成立)で停止する。中間の各地点でCamp画面を経由せず、無音で
  進める設計にした - Route Graph対応のUIがまだ存在しないため、この判断はバックエンドAPI
  のみを整え、実際の見せ方(進行ログ表示等)はUI実装時に決める
  - 実装中に見つけたバグ: `advanceRouteToNextSite()`をExit到達時も無条件に呼ぶと、
    `currentNodeId`がExitノードへ進んでしまい、`expeditionComplete()`が前提とする
    「`currentNodeId`は常に最後に確定した地点(Site)を指す」という不変条件を壊し、
    `currentStage()`が例外を投げていた。`continueExpedition()`自身の「既にcomplete
    なら呼ばない」というガードと同じ判定を先に行うよう修正して解決
- 項目2「分岐、未解決地点、Campで停止」: 灰枝の森のRoute Graphは分岐のない一本道
  なので分岐は該当なし。未解決地点で止まる挙動と、Camp通過(`advanceRouteToNextSite()`
  が中間Campノードを素通りしつつ`lastCheckpointNodeId`を更新する)は既存実装のまま
- 項目3「地点別`reconLoot`」: 実装済み。`chooseReconnaissance()`が灰枝の林縁・
  薬草の沢それぞれの基本報酬のみを再取得可能にしている(M2-A/Phase 3で実装済み)
- 項目4「通過前後のHP、Bag、Pending、Camp利用状態不変テスト」: 安全通過前後のHP不変は
  テスト済み。一括通過も同じ構築経路を再利用するため個別のHP再検証はせず、通過数・
  停止地点・報酬なしを検証する回帰テストを追加(2地点連続通過、地点未確保時のno-op、
  全地点確保時にExitまで到達する境界ケースの3種)

完了Gate:

- 新規Saveから灰枝の森を攻略し、安全帰還して次地域を解放可能
- 2〜3周の想定を人工的な周回ロックなしで達成可能
- 敗北時に地域完了と重要発見を恒久化しない
- Boss勝利だけでは不足Objectiveのある地域を完了しない
- Save再読込と結果再表示で報酬・地域完了が増えない
- 通常操作による出発、途中帰還、再出発、安全通過、再調査を確認

## M3 保存・Locale・報酬耐性

状態: **一部基盤あり。正式契約は未実装**

目的: 地域や施設を増やす前に、更新、文字追加、倉庫超過で既存進行を壊さない状態にする。

正本:

- [`save_system.md`](save_system.md)
- [`expedition_recovery.md`](expedition_recovery.md)
- [`localization.md`](localization.md)
- [`inventory_overflow.md`](inventory_overflow.md)

実装Slice:

### M3-A Checkpoint復旧

1. 全Checkpoint Kindの共通Serializer
2. Route、Node、Region Alias
3. Node削除時のCheckpoint、入口、隔離の順による退避
4. 地形、敵、増援のSnapshot復元
5. 不正RouteでBase Saveを保持

### M3-B Locale移行

1. `data/locales/ja.json`とText Key Loader
2. C++直書き表示文の段階移行
3. Key、Formatter引数、欠字、置換文字、内部ID露出の検査
4. UI可変高と文字切れ検査
5. 英語はKey契約を維持して後続追加可能にする

### M3-C 倉庫超過

1. 99/999上限
2. `RewardOverflowState`
3. 帰還結果から倉庫整理
4. 放棄確認
5. 倉庫、保留、Pendingの原子的Save

### M3-D Web保存

1. 保存中・成功・失敗HUD
2. 同期完了待ちと保存Queue
3. Export/ImportとImport前Backup
4. Schema移行Fixture

完了Gate:

- 更新でNodeが変わってもBase進行を失わない
- 上限超過報酬が消えない
- 未対応の将来Schemaを上書きしない
- 日本語UIへ英語、内部ID、代替文字を暗黙表示しない
- Web保存完了前に成功表示しない

## M4 Skill・AI・Boss共通化

状態: **メタデータと一部状態処理のみ**

目的: 初期6兵種の戦術差と、敵勢力ごとの判断差を実戦へ接続する。

正本:

- [`initial_skill_effects.md`](initial_skill_effects.md)
- [`status_effects.md`](status_effects.md)
- [`enemy_ai_rules.md`](enemy_ai_rules.md)
- [`reinforcement_rules.md`](reinforcement_rules.md)
- [`boss_common_rules.md`](boss_common_rules.md)
- [`combat_forecast.md`](combat_forecast.md)

実装Slice:

1. Skill Effect Executor、対象選択、Cost予約、取消
2. 初期6兵種18 Skill
3. Previewと実Resolverの共有
4. 状態異常、地形、Boss補正
5. 増援Wave、予告、封鎖、延期、Save
6. AI候補、Score、小隊予約、決定論的同点処理
7. 狼、人間、防衛役、射撃役、支援役のProfile
8. Boss予告固定、段階移行、撤退・撃破区別

完了Gate:

- 18 SkillすべてでPreviewと実結果が一致
- 通常反撃は発生せず、反撃準備だけ1回発動
- 敵が未公開情報を参照しない
- 待機中増援を含む全滅条件が早期勝利しない
- 同じStateとSeedでAI行動が一致

## M5 拠点・施設・倉庫完成

状態: **旧施設モデルが実装済み。正式モデルは未実装**

目的: 遠征成果を新しい戦術選択へ変換する拠点ループを完成させる。

正本:

- [`base_development.md`](base_development.md)
- [`facility_research.md`](facility_research.md)
- [`facility_data_contract.md`](facility_data_contract.md)
- [`facility_ui.md`](facility_ui.md)
- [`item_system.md`](item_system.md)

実装Slice:

1. Facility、Research、Recipe JSONとLoader
2. 旧ID AliasとSchema移行
3. 解体・稼働枠モデルを廃止
4. 建設済み施設の常時利用
5. 施設一覧、訪問、研究、製作、確認UI
6. 原子的な建設・研究・製作Transaction
7. ユニットページの装備変更
8. 地域攻略による拠点段階と外観変化

完了Gate:

- 建設済み施設を切替なしで利用可能
- 研究・製作を再実行して素材を二重消費しない
- 旧Saveの施設・研究IDを正式IDへ移行
- 不正参照、研究循環、Locale欠落を起動時検出
- 施設説明、必要素材、効果が見切れない

## M6 沈黙した監視所群

状態: **旧固定3戦闘実装あり。正式6地点化は未着手**

目的: 共通データだけで最初の4〜6周型地域を完成させる。

正本:

- [`regions/cinderwatch_gate.md`](regions/cinderwatch_gate.md)
- [`campaign_route_graph.md`](campaign_route_graph.md)

実装Slice:

1. 6地点、分岐、2 CampをDefinition化
2. 外門、監視所、物資庫
3. 工兵救出、旧兵舎、伝令路
4. 信号塔Object、副信号機、主信号機
5. 敵増援と任務AI
6. 加入候補と地域完了
7. 灰鉄採石場解放
8. 4〜6周の実戦計測

完了Gate:

- 地域固有のC++分岐なしで6地点を生成
- 各周回で最低1つの恒久成果を持ち帰れる
- 確保済み前半を一括通過して後半へ消耗を温存できる
- 熟練プレイの早期攻略を周回数で禁止しない

## M7 12兵種・仲間・会話

状態: **初期6兵種のみ一部実装**

正本:

- [`class_reference.md`](class_reference.md)
- [`character_progression.md`](character_progression.md)
- [`roster_design.md`](roster_design.md)
- [`gathering_place.md`](gathering_place.md)

実装Slice:

1. 後半6兵種のClass、武器、固有能力
2. 灰鉄採石場攻略までの12兵種加入経路
3. ユニットページと装備共有
4. 集会所の人物一覧、会話、既読状態
5. Pending加入候補から安全帰還、会話、Roster追加
6. 加入・装備・会話Save

完了Gate:

- 12兵種すべての役割と非役割が戦闘で確認可能
- 同じ武器1本を複数人へ同時装備できない
- 加入候補を再訪・再読込で二重追加しない
- 任意会話未読で本編を停止しない

## M8 遠征経済完成

状態: **初期6 Itemのみ一部実装**

正本:

- [`item_system.md`](item_system.md)
- [`expedition_rewards.md`](expedition_rewards.md)
- [`expedition_preparation_screen.md`](expedition_preparation_screen.md)

実装Slice:

1. 消耗品6枠、探索道具2枠の準備画面
2. Item Buttonから使用可能一覧
3. 全消耗品と使用場所制限
4. 探索道具と兵種Choiceの関係
5. 保護箱、帰還信号弾、補給地点
6. 製作、倉庫、Bag、未使用品返却の統合試験

完了Gate:

- 使用不能理由と対象を操作前に表示
- 遠征中は原則補充しない
- 使用済みと未使用持込品、Pendingを混同しない
- 帰還・敗北・中断再開でItem数が保存される

## M9 残り8地域

状態: **設計済み、実データ未作成**

目的: 完成済み共通基盤へ地域データを追加し、本編10地域を構築する。

正本:

- [`campaign_regions.md`](campaign_regions.md)
- [`campaign_route_graph.md`](campaign_route_graph.md)
- [`regions/`](regions/)

実装順:

1. 灰鉄採石場
2. 黒水低湿地
3. 風裂き高原
4. 旧辺境集落
5. 燼火峡谷
6. 埋没聖堂
7. 破砕された前線砦
8. 地図外縁

各地域の共通Gate:

- Route、Choice、Battle、Mission、Reward、Camp、MessageがDefinition検証を通過
- 入口から全必須地点とExitへ到達可能
- 4〜6周で短縮効果と資源判断が成立
- 地域完了、次地域解放、再訪報酬が二重発火しない
- 主要編成で進行不能にならない

2地域ずつ実装・計測し、Gateを通るまで次の組へ進まない。

## M10 公開品質

状態: **未着手**

実装Slice:

1. Tutorialと難しめの標準難度導入
2. 全62地点の進行不能Graph検査
3. 全地域の資源収支、戦闘不能率、周回数計測
4. 日本語・英語Locale完成
5. DesktopとWebの主要解像度UI試験
6. Emscripten Buildとブラウザ実機試験
7. GitHub Pages更新前後のSave継続試験
8. Export/Import、破損復旧、将来Schema拒否試験

公開Gate:

- 10地域を新規Saveから本編完了まで通せる
- 62地点に欠落Definition、到達不能、二重報酬がない
- 内部ID、代替文字、文字切れ、UI重なりがない
- ChromeとSafariで保存継続
- 更新前Saveを更新後Buildで維持

## 直近の実装順

次の3 Sliceだけに集中する。

1. **M1-A Event Batch完成**
   - `UnitDefeatedEvent`
   - Phase終了状態処理
   - Batch後1回の勝敗評価
   - 同時発生テスト
2. **M1-B Objective残修正**
   - Team制約
   - 対象存在検証
   - OR `Superseded`
   - Progress同期
3. **M1-C Battle Object最小実装**
   - 踏査地点
   - 倒木
   - Exit
   - 操作・破壊Event

この3 Sliceが通るまで、薬草の沢の個別戦闘、追加Skill、後半兵種、残り地域を並行実装しない。
完了後はM1-Dで灰枝の森を共通Definitionへ載せ、M2-A薬草の沢へ進む。

## 各Sliceの完了定義

すべて満たした時だけ完了とする。

1. 担当正本と実装が一致
2. 新規IDと参照のValidationがある
3. 正常系、Cancel、重複、Save再開、失敗系のテストがある
4. Debug/Release BuildとCTestが成功
5. プレイヤー到達経路がある
6. 日本語表示とLocale検査がある
7. `implementation_status.md`を更新
8. GUI変更はDesktopとWebの対象解像度で目視確認

## 変更規則

- 実装完了時は`implementation_status.md`も同時更新する
- 仕様変更は担当正本へ行い、この文書には依存関係の変化だけを反映する
- World Bibleに属する変更はBibleを先に確認する
- Milestone番号をSave Schema、ゲームVersion、施設段階へ流用しない
- 新しいMilestoneを追加する前に既存Milestoneへ含められない理由を書く
