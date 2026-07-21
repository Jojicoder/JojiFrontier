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
- 任務目標・戦闘イベント基盤: `EliminateTeam`/`DefeatUnit`/`SecureTile`/`DestroyObject`/
  `SurviveRounds`/`EscapeUnits`、AND/ORグループ、`UnitDefeatedEvent`を含む戦闘イベント、
  勝敗評価(`jf/battle/ObjectiveTracker.hpp`)。`BattleController`はこの評価結果だけで
  Victory/Defeatへ遷移する。`DestroyObject`(2026-07追加)は`DefeatUnit`と同じ「対象
  BattleStateを都度評価するLive評価」パターンで、対象ObjectのStateが`Destroyed`かを見るだけ
  (`ObjectDestroyedEvent`自体は消費しない)。未知Object IDや`canBeAttacked=false`なObjectへの
  参照は`validateBattleMission()`が起動時に拒否する。`SurviveRounds`(2026-07続き)も同じ
  Live評価で`battle.round() > target.surviveUntilRound`を見るだけ(「指定ラウンドの終了まで」
  なので到達ではなく超過を要求)。`surviveUntilRound < 1`(round_の初期値1に対し即座に
  満たされてしまう)は起動時検証で拒否。`EscapeUnits`(2026-07続き)は`SecureTile`と同じ
  `ActionResolvedEvent`credit機構を再利用しつつ、`creditedTargetIds`(既存のSet)が
  `requiredEscapeCount`件の異なるUnit IDに達するまでCompletedにしない点だけ拡張した
  (同一Unitが複数回行動終了しても1件としてしかカウントしない)。`docs/mission_objectives.md`
  が定める`UnitRetreated`Event経由の脱出(ExitPointへ実際に離脱するAI/撤退駆動の経路)は
  対象外 - ExitPointの実挙動が前提のため別Slice。`ProtectUnit`(2026-07続き)は常に
  副目標専用(`docs/mission_objectives.md`の表通りprimary=No固定)で、他Kindと構造が逆:
  「満たされている」がデフォルト状態(戦闘開始時から真)で、崩れた瞬間だけ捕まえる
  「立ち下がりEdge」の検出になる。既存の`syncObjectiveProgress()`のPrimary Group走査
  (満たされた瞬間にCompletedへ遷移する「立ち上がりEdge」前提)へ混ぜるとSync 1回目で
  即Completedになってしまうため、専用の別Passを新設し`Active→Failed`のみを行う
  (`Completed`には自分からは決して遷移しない - 勝利時に「Activeのまま残っていれば護衛成功」
  という規約は、これを消費する側(まだ未接続)に委ねる設計)。ProtectUnitがprimary=trueに
  誤指定された場合は`validateBattleMission()`が拒否する(誤ってPrimary Group側の
  立ち上がりEdge評価に混入するのを防ぐため)。`OperateObject`(2026-07続き)は
  `DestroyObject`と同じLive評価で、対象Objectの`interactionCount > 0`を見るだけ
  (Event経由の配線は不要)。`docs/mission_objectives.md`の「指定ユニット・兵種が」という
  兵種制限は`resolveObjectInteraction()`の`ObjectInteractionDefinition::allowedClasses`が
  Interact成功可否そのものを既に制限しているため、`interactionCount`が進んでいる時点で
  兵種制限は自動的に満たされている - Objective側で二重チェックする必要がない。
  対象Objectに`interaction`が設定されていない(例: 通常のBarrier)場合は
  `validateBattleMission()`が拒否する。5種ともまだ出荷済みコンテンツからは
  未接続(地域書側の`surveyObjectiveId`相当の配線は次のSlice)
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
- Battle Objectの攻撃対象化(2026-07続き): 上記`resolveObjectAttack()`に呼び出し元を追加。
  `BattleController`へ`ConfirmObjectAttack`(`ConfirmSkillAttack`と同じ並行状態パターン)・
  `objectTargetableTiles_`・`pendingObjectPreview()`・`confirmObjectAttack()`を追加し、
  `chooseAttack()`/`selectTargetTile()`がUnitに加えてcanBeAttackedなObjectも対象にできる
  ようにした。`main.cpp`へConfirm/Cancelボタン・専用Preview Popup・Tileハイライト・
  ヒット/破壊バナー(`battle.object_hit_message`/`battle.object_destroyed_message`)を追加。
  出荷済みの唯一のBattle Object(Brokenwood Territoryの`fallen_log`)を`canBeAttacked=true`/
  `maxDurability=16`へ変更し実際に攻撃可能にした(従来は`blocksMovement`専用登録で、
  配線後も対象外のままだった)。攻撃による破壊は`ObjectDestroyedEvent`のみ発行する設計とした
  - 通常攻撃でObjectの状態が変わるのは「破壊」の1パターンだけのため
- Battle ObjectのInteractコマンド配線(2026-07続き): `resolveObjectInteraction()`自体は既に
  完成していたが、どのDefinitionがInteract可能かを表す場所が無く呼び出し元もUIも無かった。
  `BattleObjectDefinition`へ`std::optional<ObjectInteractionDefinition> interaction`と
  結果State`interactionResultState`を追加し、`BattleController`へ`SelectInteractTarget`
  (Preview/Confirmを挟まず`chooseHeal()`と同じ即時解決パターン)・`objectInteractableTiles_`・
  `canInteract()`(UIのButton表示可否を読み取り専用で判定)・`chooseInteract()`・
  `selectInteractTarget()`(成功時のみ`ObjectStateChangedEvent`を発行)を追加。`ActionKind`へ
  `Interact`を新設。`main.cpp`はSelectAction画面の固定5Slotが埋まっているため、
  `canInteract()`がtrueの時だけ現れる6個目のButtonと専用Tileハイライト色を追加した。
  出荷済みコンテンツにInteract可能なObjectがまだ1つも無いため(`fallen_log`は攻撃対象のみ)、
  通常プレイでこのButtonは現状表示されない - 次にDeviceコンテンツが追加された時点で
  自動的に有効になる
- Battle ObjectのBattleFactoryランダム生成統合(2026-07、`battle_objects.md`「ランダム生成」の
  手順3・5・6一部): 折れ木の縄張りの`fallen_log`専用だった`if (stage.terrainProfileId == ...)`
  Ad-hocブロックを、`StageDescriptor::ObjectPlacementRule`(`BattleObjectDefinition`本体・
  配置数・Route B用`extraBarrierCount`加算・配置列範囲・「最初の生存Enemyと同じ行を避ける」
  フラグを1件にまとめたデータ)を読む汎用`BattleFactory.cpp`の`placeRandomObjects()`へ一般化。
  地域書(`Region.cpp`)側は`brokenwood.objectPlacementRules`へ1件登録するだけになり、次の
  地域がBarrier/Containerを乱数配置したくなった時に`BattleFactory.cpp`へ新しい
  `if`分岐を足す必要がない。`blocksMovement`なRuleは配置前に
  `hasRouteAcrossWithBlockedTiles()`(`hasRouteAcross()`のBattleState版、Object込みで
  「盤面を横断する経路が最低1本残るか」を確認)で検証し、唯一の経路を塞ぐ候補は次の
  シャッフル候補へ回す(手順7の「安全な固定配置へ戻す」フォールバックは未実装 - 現状の
  地域は候補さえあれば経路を塞がずに収まるため)。既存の`fallen_log`回帰テスト(列2-5固定、
  ボスと同じ行に出ない、Route Aで1本・Route Bで2本)はテキスト変更なしで全て通過。
  リファクタ中、`jf_forest_balance`のBrokenwood勝率が89.7%→13.7%まで急落する事象を検知して
  調査した結果、原因は今回のリファクタではなく前Slice(Battle Objectの攻撃対象化)由来の
  潜在バグと判明: `tools/forest_balance.cpp`の`attackIfPossible()`が、射程内にUnitが
  1体もおらずcanBeAttackedなObject(`fallen_log`)だけが対象という新しいケースで
  `chooseAttack()`が`SelectTarget`へ遷移したまま`cancelAttackSelection()`を呼ばずに
  `false`を返し、以降の全Policy関数が期待する`SelectAction`と食い違ってその戦闘が
  永久停止(Round上限までTimeout)していた。`attackIfPossible()`にUnit対象が見つからない
  場合の`cancelAttackSelection()`呼び出しを追加して修正(実プレイの`main.cpp`は
  Cancelボタンがあるため無関係 - このBotだけの潜在バグだった)。修正後
  `jf_forest_balance`はBrokenwood 89.7%/87.7%、Timeout一桁まで回復したことを確認済み
- M1-C完了(2026-07): Battle ObjectのSave Snapshotは対象外と判明したため実装不要と結論。
  `include/jf/core/SaveSystem.hpp`の`ExpeditionCheckpoint`冒頭コメントが既に明記する通り、
  「戦闘中・配置中の状態はそもそも一切保存しない」設計(中断したら最後のExploration/Camp
  チェックポイントへ戻り、同じSeedから戦闘を決定論的に再生成する)がこの調査前から
  確立しており、増援Wave状態を保存しない判断と同じ理由で対象外
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
- 頭数連動の護衛狼増援(2026-07、`campaign_balance.md`「Skill実装後の実測と護衛狼の頭数連動」
  参照): `StageDescriptor::understaffedReinforcement`/`understaffedThreshold`を新設し、
  Verge/Hollowでの戦闘不能により4人未満でTerritoryへ突入した場合、護衛狼をもう1体追加する
  (`buildEnemies()`に`livingPlayerCount`引数を追加)。大猪自身の能力値は変更なし。
  `jf_forest_balance`実測(500 seed)で3地点連続の全滅率が32%前後→39.8%(目標35-45%の中央)
  に是正されたことを確認済み。Camp次地点プレビュー(`nextSiteEnemyRosterNames()`)と
  PreBattleDeployment(`previewEnemies()`)もこの追加reinforcementを反映するよう更新
- Boss共通型の抽出(2026-07、M4項目8): `boss_common_rules.md`の「Bossの退場理由」を
  `jf::UnitExitReason`(`jf/core/Unit.hpp`)+`Unit::exitReason`として新設し、灰角大猪の
  HP0を`ScriptedWithdrawal`(撃破相当)として設定(`ObjectiveTracker.cpp`の
  `emitUnitDefeatedEvents()`)。「Phase移行」を`jf::BossStageChangedEvent`
  (`jf/battle/BattleEvents.hpp`)として新設し、灰角大猪の激昂時に1回発行。続けて
  `jf::BossRuntimeState`/`jf::BossTelegraph`(`jf/battle/BossRuntime.hpp`)へ予告行動
  (行動ID・形状・予告/実行Round・対象・固定Tile・方向)を汎用化し、灰角大猪の突進予告を
  `Unit::bossRuntime.telegraph`経由に移行、`jf::BossTelegraphChangedEvent`を発行。
  Objective側の退場理由Filter(許可した退場理由でのみBoss素材を付与)は、`Retreated`等を
  実際に使うBoss・敵がまだいないため未着手
- Boss突進予告のUI表示(2026-07続き): `BossTelegraphChangedEvent`も増援Wave予告と同じ
  制約(`consumedEventIds`がペイロードを保持しない書き込み専用の重複排除セット)を持つため、
  `main.cpp`は`Unit::bossRuntime.telegraph.state`を毎フレームポーリングする方式
  (`reinforcementUiStates()`と同じ手法、`bossTelegraphUiStates()`として追加)で
  `None→Announced`遷移を検知し`pushBattleMessage()`でバナーを表示する。
  `BossTelegraph::lockedTiles`(「攻撃列」の空間情報)は定義されていたが
  `EnemyAI.cpp`の3箇所の予告生成コードがいずれも`{}`のまま一度も設定していなかったため、
  `executeBoarCharge()`の実行時Walk(行固定・盤面端または遮蔽Objectで打ち切り)を
  副作用なしで再現する`computeBoarChargeTiles()`を新設して予告時に実際に埋めるよう
  修正した。これにより`main.cpp`はイベントを介さずこの確定済みフィールドを読むだけで
  済み、`objectTargetableTiles()`等と同じ`containsTile`+`DrawRectangleRec`パターンで
  警告色のTileハイライトを表示する。既存の灰角大猪突進テスト3件に、予告直後の
  `lockedTiles`が実行時に実際に通過するTile列と一致することの確認を追加(意図的に
  `computeBoarChargeTiles()`を空配列へ差し替えて失敗することを確認した上で復元)。
  ヘッドレス環境のため実機(raylibウィンドウ)上での目視確認は未実施
- 増援Wave(2026-07、M4項目5): `jf::ReinforcementWave`(`jf/battle/Reinforcement.hpp`、
  `ReinforcementState{Scheduled,Announced,Spawned,Prevented,Cancelled}`)、
  `validateReinforcementWaves()`、`BattleState::addReinforcementWave()`/
  `announceReinforcements()`/`resolveReinforcementsForPhase()`(Player/Enemy Phase開始時に
  呼ばれ、予告→出現→全候補封鎖ならPrevented、を処理)、`hasPendingRequiredEnemyReinforcements()`
  (必須Wave未解決の間`EliminateTeam`を早期成立させない)を実装。地域接続は薬草の沢の採取
  ルート(`StageDescriptor::timedReinforcement`、2ラウンド目に狼1体)のみ - 以前「増援の
  仕組み自体が未実装」として保留していたHollowの既知ギャップが解消された
- 増援Wave予告のUI表示(2026-07、M4項目5続き): `ReinforcementAnnouncedEvent`は
  `consumedEventIds`という書き込み専用の重複排除セットにしか痕跡が残らずペイロードを
  読み出せないため、`src/main.cpp`に`reinforcementUiStates()`(Wave id→直近状態のMap)を
  新設し、既存の`lastSeenAttackEvent`/`lowHpWarnedUnits()`と同じ「毎フレームポーリングし
  差分検出」方式で`Scheduled→Announced`遷移を検知、`pushBattleMessage()`で
  `battle.reinforcement_announced`バナーを1回だけ表示するようにした。表示は`spawnRound`
  のみ(実出現Tileは非表示のまま)。Boss予告UI(`BossTelegraphChangedEvent`)は今回の対象外で
  依然未配線
- AI候補/Score/小隊予約/兵種別Profile(2026-07、M4項目6・7、簡略版): `enemy_ai_rules.md`の
  完全仕様(8 Role、6 Faction、撤退/降伏、Object操作候補)より小さい、`jf::AiCandidate`/
  `jf::AiProfile`(Wolf/Human/Defender/Ranged/Support/Banditの6種)/`generateAiCandidates()`/
  `chooseBestAiCandidate()`+`candidateLess()`(7段の決定論的同点処理)を実装し、Boss
  (`AshenhornBoar`)以外の全Enemy(Wolf含む - 専用群れAI関数は完全にデッドコード化していた
  ため削除)をこの経路へ接続。`jf::AiSquadReservations`(`BattleController::
  enemyReservations_`)でEnemy Phase中の停止マス・予約Damage・支援対象の小隊予約も実装。
  Bandit Profileは低HP優先を強め・追跡制限を短くして「野盗」のFaction差分を一部反映
  (Loot Container/離脱経路評価はObject/Exit認識が無いため保留)。**`jf_forest_balance`実測で
  この変更により3地点連続全滅率が39.8%→59.2%(目標35-45%を超過)まで悪化したことを確認**
- 兵種DEF調整(2026-07): 上記の実測を掘り下げた結果、古参守備兵1人を欠くと灰枝の林縁単体でも
  勝率100%→56.6%まで崩壊する(他クラス1人欠けでは崩壊しない)極端な依存が判明。狼の攻撃力11に
  対し古参守備兵DEF10は被ダメージ1(下限)、監視弓兵DEF3/暁の衛生兵DEF2は8-9という差が原因
  だった。`data/classes.json`のDEFを古参守備兵10→8、監視弓兵3→5、暁の衛生兵2→4へ調整
  (`class_reference.md`の正本テーブルも同期)。古参守備兵抜きの灰枝の林縁単体勝率は
  56.6%→81.4%まで改善。3地点連続(無補給)の全滅率はやや悪化(59.2%→60.0%)したが、これは
  撤退前提の最悪ケース指標のため許容(詳細は`campaign_balance.md`「古参守備兵への過度な依存と
  DEF調整」参照)
- 撤退実装(2026-07、M4項目6・7継続課題の一部): `enemy_ai_rules.md`「撤退と降伏」の撤退部分を
  実装。`Unit::hasExited`+`Unit::isPresent()`(`isAlive() && !hasExited`)を追加し、「まだ盤面上の
  脅威/対象か」を問う箇所(`unitAt()`、`Movement.cpp`の各判定、`allEnemiesDefeated()`、
  `EliminateTeam`)を`isAlive()`から置き換え。`isAlive()`自体はHPのみの判定のまま不変
  (`DefeatUnit`は意図的にこのまま)。`AiActionType::Retreat`+`AiProfile::retreatHpPercent`
  (既定25、Wolf20)で撤退候補を生成し、盤面右端列(Exit)へ退場させる。実装中に
  `BattleState::isTeamDone()`/`BattleController::nextUnactedEnemy()`の`isAlive()`残存という
  致命的な潜在バグ(撤退後Enemy Phaseが永久停止する)を発見・修正、revert検証済み。新規テスト
  5ブロック追加、全て通過。`jf_forest_balance`でAshbough Forestへの数値影響は皆無(Tactical
  3地点連続 40.0%/60.0%で不変) - 狼のHP16・DEF2に対し現行の与ダメージが大きく撤退閾値到達前に
  ほぼ即死するため。降伏候補・Object操作候補は未着手
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
- `region_mission_data_contract.md`の`RegionDefinition`/`SiteDefinition`/
  `ExplorationChoiceDefinition`/`RewardGrantId`等の構造体そのものへの移行(M1-D項目1)。
  灰枝の森3地点(薬草の沢・折れ木の縄張り含む)追加後も`StageDescriptor`の軽量拡張
  (`routeOutcomes`、`scoutRouteDisabled`、Ad-hoc副目標フィールド等)で対応できており、
  まだ本格的な移行コストに見合っていない。次地域(灰鉄採石場、5地点)を追加するM9系まで
  持ち越す想定に変更
- 薬草の沢の「薬草を採取」ルートはRound 2の狼1体Waveと予告バナー表示まで実装済み。
  未実装なのは他地域のEncounter Definitionから同じ共通Waveを生成するデータ移行
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
- 激昂境界の戦闘UI表示(HP50%閾値の表示自体は未配線、`bossRuntime.stageIndex`は
  データとしては保持済み)。突進予告Message・予告列のハイライトは2026-07に配線完了
  (下記「Boss突進予告のUI表示」参照)
- Device/Container/SpawnPointの実際の挙動(修理、Wave接続、破壊後Terrain変換)。データモデルの
  種別だけ定義済みで、専用ロジックは未着手
- Root Action単位の完全な行動解決順、同時発生規則のうち増援・Boss段階移行・反応Skill部分
- 通常反撃なし。槍兵の反撃準備だけが行う反応攻撃
- Encounter/地域Definitionから増援Waveを生成する接続(現状は薬草の沢1件のみ)。Wave
  Runtimeの予告、封鎖、出現直後行動不可、必須全滅条件、Checkpointからの決定論的再生成、
  予告のUI表示(バナー)は実装済み
- Boss Objectiveの許可退場理由Filter。状態異常補正、予告固定、段階移行、退場理由Eventは実装済み
- 本編後の深層遠征向け複数マスBoss Footprint
- 倒木、装置、Container、増援口、Exitを統合するBattle Object
- 実Resolverと同じ計算を使う攻撃予測・危険予告Overlay
- Objective/Object操作、撤退・降伏、情報公開範囲を含むAI候補。Damage/位置/役割Profile、
  小隊予約、決定論的同点処理は実装済み
- 初期6兵種18スキルの対象、射程、Cost、実効果、AI評価、予測表示

正本は[`battle_resolution_contract.md`](battle_resolution_contract.md)、
[`initial_skill_effects.md`](initial_skill_effects.md)、[`enemy_ai_rules.md`](enemy_ai_rules.md)。

## データ契約

実装済み:

- `data/terrain_profiles.json`を正本とする`TerrainProfile` Loader
- 重み付き地形、特徴地形、列ごとの障害物上限、水平通路保証、特定地形の最低/最大数を
  共通生成器で処理
- 現行6フィールドをProfileへ移行し、`FieldType`列挙と地形生成率の地点別分岐を削除
- 重複Profile ID、未知Terrain、重み合計100以外、不正な個数上下限、現行参照Profile欠落を
  読込時に拒否
- 新しい地形構成を`TerrainProfile`追加だけで生成できる回帰テスト
- 薬草の沢で旧コードと正本が食い違っていた地形率を、通常床40%・浅瀬35%・茂み15%・
  灰地10%へ修正
- 倉庫上限超過時の受取保留・倉庫整理・放棄確認・原子的な帰還Transaction(M3-C、
  詳細は`implementation_roadmap.md`「M3-C 倉庫超過」)
- `data/regions.json`を正本とする`StageDescriptor`部分Loader(2026-07、M1-E Slice1続き):
  `StageDescriptor`自体をJSON化対象にする方針を採り、JSON化可能な安定Subsetを
  `GameData::StageContentData`として切り出した(`id`/`terrainProfileId`/`enemyRoster`/
  `baseVictoryLoot`/`routeVictoryLootDelta`/`surveyObjectiveId`/`surveyBonusLoot`/
  `discoveries`/`missionNameEn`/`missionNameJa`)。`GameData::loadGameData()`が
  重複ID・存在しないTerrain Profile参照・未知`ExplorationChoice`文字列を起動時に拒否する。
  `Region.cpp`の`stageDescriptorFromContent()`がこのSubsetから`StageDescriptor`の共通部分を
  組み立て、灰枝の林縁(構成が最も単純なStage)をC++直書きから完全移行した実証済み。
  倒木・薬草地点の乱数配置(`ObjectPlacementRule`/`HerbPatchGenerationRule`)は元々
  `StageDescriptor`側のC++フィールドとして先に一般化済みのため、今回のJSON化対象には
  未だ含めていない。続けて`StageContentData`へ`routeOutcomes`/`scoutRouteRequiredClass`/
  `scoutRouteDisabled`/`timedReinforcement`/`herbPatchGeneration`を追加し、増援Wave・
  衛生兵限定ルートを持つ薬草の沢も完全移行した。続けて`objectPlacementRules`
  (`BattleObjectDefinition`をそのまま埋め込み)・`enemyCountOverride`・`boostedFirstEnemy`・
  `understaffedReinforcement`/`understaffedThreshold`・`logCollisionBonusLoot`/
  `noCasualtiesBonusLoot`もSchemaへ追加し、折れ木の縄張り(出荷済みの中で最も複雑なStage)と
  沈黙した監視所群3地点(`enemyRoster`をJSON側で意図的に省略 - 空Rosterは既存の
  「`GameData::enemyRoster`という共有Rosterを使う」という意味を保持)も完全移行した。
  これで出荷済み6 Stage全てが`data/regions.json`駆動になり、`Region.cpp`から地点固有の
  StageDescriptor直書きは無くなった。`jf_forest_balance`実測は全て移行前後で完全に同一
  (Byte-identical)
- `BattleFactory.cpp`最後の名前分岐を除去(2026-07): 折れ木の縄張り専用の
  `stage.terrainProfileId == kBrokenwoodTerritoryTerrain`(敵生成列を右2列に絞る)を
  `StageDescriptor::enemyZoneWidth`(既定`nullopt`=3列)として一般化。`assembleScenario()`/
  `buildEnemies()`は`stage.id`/`stage.terrainProfileId`の値そのものを条件分岐に一切使わなく
  なった(`terrainProfileId`は`data.terrainProfile()`を引くためのKeyとしてのみ使用)
- コンテンツ構造検証`jf_content_tests`を新設(2026-07、M1-E Slice7一部): `jf_locale_tests`と
  同様CTest登録済みで`ctest`実行のたびに自動検証する。全Region×全Stageを100 Seedずつ
  実際に生成し、Unit配置の盤内・通行可能・非重複、敵数の一致、Object配置数・列範囲・
  非重複、HerbPatch枚数の一致、盤面左右端間の経路存在(無制限BFS)、同一Seedからの
  決定論的再生成を検証する。Route Graph到達可能性・Objective達成可能性の静的検査は
  対象外(M9で分岐Routeが増えてから着手)
- `UnitClass` switch解消(表示名2箇所、2026-07、M1-E Slice5一部): `main.cpp`の
  `classNameFor()`/`classRoleFor()`が`UnitClass`の`switch`文だったのを、`ClassDefinition`
  (`data/classes.json`)へ`nameKey`/`roleKey`の2フィールドを追加してそこから引く形へ移行。
  実装前に本物のブロッカーを発見: 両関数は`loadAppFont()`(Font Glyph収集、`GameData`
  読込より前に実行)からも呼ばれており、素朴に`GameData`引数を追加するだけでは起動順序が
  壊れる状態だった。調べた結果この呼び出し自体が既に冗長と判明(両関数は`tr()`のラッパーに
  過ぎず、その和文は`allJapaneseGlyphText()`がLocale Table全体から自動収集済み)。冗長な
  収集行を削除しただけで依存自体が消え、起動順序変更なしで済んだ。残る呼び出し元
  (main.cppのUI描画関数9箇所)は全て`jf::GameApp& app`を引数に持っていたため
  `app.gameData()`をそのまま渡すだけで済んだ。ビルド・`ctest`4種・実機起動確認
  (フォントロード完了までクラッシュなしを確認)とも通過。兵種パッシブ自体
  (`UnitClass.cpp`の`hasBrace`/`hasZoneOfControl`等)は元々`switch`ではなく単発の等値比較
  関数群で、新兵種追加で既存コードを壊さない形に既になっていたため対象外と判断
- 薬草地点・倒木の乱数配置を`StageDescriptor::HerbPatchGenerationRule`/
  `ObjectPlacementRule`という地域書側のデータへ一般化(2026-07)。`if (stage.terrainProfileId
  == ...)`というAd-hoc分岐だった折れ木の縄張りの倒木・薬草の沢の薬草地点の両方をこの形へ
  移行済み。踏査地点(`chooseSurveyTile()`)は元々地点名で分岐しない共通関数のため対象外と
  判断(詳細は`implementation_roadmap.md`「M1-E コンテンツ追加基盤」Slice3)

設定済み・未実装:

- Encounter、Site、Region、AI ProfileのJSON Definitionと横断Validation(`StageDescriptor`の
  一部フィールドは出荷済み6 Stage全てでJSON化済み。Encounter生成ロジック自体・AI Profile・
  `region_mission_data_contract.md`が定めるフルSchemaへの全面移行はM9まで持ち越し)
- 敵生成列をPlacement Ruleへ移す処理(薬草地点・倒木は完了、詳細は上記)
- Boss専用一時状態を`Unit`からRuntime Stateへ分離する処理
- Facility、Research、RecipeをJSONへ分離する定義型、所有状態、旧ID Alias、起動時検証
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

装備スキル(M4-A、`docs/initial_skill_effects.md`):

- 実装済み: 装備選択・Save往復・戦闘開始時Charge初期化・Charge/Cooldown管理(既存)、
  戦闘中にスキルを選んで発動するExecutor経路(`BattleController::chooseSkill()`/
  `selectSkillTarget()`、新設)。**Skill IDごとのif分岐ではなく、4つの再利用可能な
  「形状テーブル」(`healSkillShapes`/`isCleanseShape`/`attackSkillShapes`/
  `buffSkillShapes`/`markSkillShapes`、いずれも`BattleController.cpp`の匿名namespace)を
  検索する形へリファクタ済み**。同じ形状のSkillはテーブルへ1行足すだけで実装できる(実例:
  `ambush`と`extended_lockdown`はどちらもテーブル1行のみで新規コード無し/最小追加で
  実装)。**初期6兵種18 Skill全てに実効果あり**:
  暁の衛生兵`emergency_treatment`(Heal形状。HP50%以下の味方を射程2から12回復、戦闘1回)、
  `cleanse`(状態解除形状。自身または隣接味方1人の毒・炎上・移動低下・防御低下・よろめきを
  全解除、CD2)、`protective_treatment`(バフ形状・単体。RES+3、次のEnemy Phase終了まで、
  CD2)、監視弓兵`suppressing_shot`(攻撃形状。敵1体・武器射程・通常攻撃+移動低下付与、
  CD2)、行軍隊長`hold_formation`(バフ形状・AoE即時。自身と隣接味方全員DEF+2、次のEnemy
  Phase終了まで、CD2)、槍兵`halting_thrust`(攻撃形状。`suppressing_shot`と同じテーブル行を
  共有)、辺境斥候`ambush`(攻撃形状。Damage+3、未行動の敵限定、戦闘1回)、古参守備兵
  `extended_lockdown`(バフ形状・自身のみ即時。Zone of Control範囲を距離1→2、次のEnemy
  Phase終了まで、CD2。`Movement.cpp`の`isStoppedByZoneOfControl()`が`Unit::
  zocRangeExtended`を直接参照)。バフ形状は`BuffKind{Resistance,Defense,ZocRange}`という
  enumと`Unit::resistanceUpActive`/`defenseUpActive`/`zocRangeExtended`、
  `effectiveResistance()`/`effectiveDefense()`拡張、専用の`clearSkillBuffsAtEnemyPhaseEnd()`
  (通常のmoveDown/defenseDownは「対象自身の陣営の次Phase終了」で切れるが、この3つは常に
  「次のEnemy Phase終了」固定)が必要だった。監視弓兵`mark_target`(Mark形状。敵1体・武器
  射程、Damageなし、次にこの敵が受ける攻撃へDamage+2・命中時に消費、CD2)は攻撃せず
  `Unit::markedBonusDamage`を設定するだけの新形状で、`CombatResolver.cpp`の
  `computeDamage()`は読むだけ(`previewAttack()`用に純関数のまま維持)、`resolveAttack()`
  側で命中時だけ0へ戻す。行軍隊長`support_order`(Mark形状。隣接味方1人・自身除く、
  Damage-3の被ダメージ軽減シールド、1 Phase 1回)は`markedBonusDamage`を符号付きにして
  `targetsAlly`フラグを足しただけで、新規フィールド0個・`mark_target`と全く同じ消費経路を
  再利用して実装できた。行軍隊長`advance_order`(隣接する未行動味方1人・自身除く、MOV+1、
  このPlayer Phase終了まで、戦闘1回)は形状テーブルへ押し込まず`isCleanseShape`と同じ
  素朴な専用分岐にした - 既存3バフが全て「次のEnemy Phase終了」固定で切れるのに対し、
  これだけ「今の」Player Phase終了で切れるため。`Unit::moveUpActive`/`effectiveMove()`
  拡張、`applyMoveUp()`/`clearMoveUpAtPlayerPhaseEnd()`(Player Phase終了処理へ追加)が
  必要だった。古参守備兵`immovable_stance`(不動の構え)は他11 SkillがすべてActiveなのに対し
  唯一のPassive(`SkillCategory::Passive`、`SkillUsageType::Always`)で、`chooseSkill()`を
  経由せず`chooseWait()`確定時に自動発動する(装備中は毎回、Chargeなし)。「次の自分の行動
  終了までDEF+3・移動不可」を`Unit::immovableStanceActive`/`immovableStanceJustGranted`の
  2段階フラグで表現し、発動元のWait自体を「次の行動」に数えないようにした。Waitを繰り返すと
  その都度再発動する(1回だけの資源ではない)。`effectiveMove()`/`effectiveDefense()`を
  拡張。戦闘HUD「スキル」メニューでの装備スキル表示(使用不能理由付き)も追加。辺境斥候
  `emergency_withdrawal`(緊急離脱。自身、最大3 Tile、CD2、攻撃せず移動、敵隣接から開始
  可能、通常占有規則を守る)は対象が「空きTile」という初めてのパターンで、既存5形状のどれも
  前提が合わず新設の専用関数`computeEmergencyWithdrawalTiles()`(MOV/地形コストを無視した
  固定3マス予算、Zone of Controlは完全無視)で実装した。ただし「通常占有規則を守る」は
  `Movement.cpp`の`computeReachableTilesImpl()`と同じ規則(経路上の敵Unitは通行不可、
  味方Unitは通過可だが着地不可)に従う必要があり、実装当初はBFS展開中に占有を見ておらず
  最終候補リストでしか占有判定していないバグを作り込んだ(回帰テストでこのチェックを
  意図的に外して失敗することを確認した上で修正)。`selectSkillTarget()`にも対象Unit取得を
  前提とする既存の共有ガードより前に、空きTileへ直接`battle_.moveUnit()`する専用の早期
  分岐が必要だった。槍兵`spear_wall`(槍壁。自身と隣接味方1人、次のEnemy Phase終了まで、
  Spearman兵種の基礎特性Brace(`hasBrace()`/`BattleState::combatDefenseBonus()`、攻撃者が
  2 Tile以上移動していた場合のみDEF+2)と同じ条件付きDEF+2を、まだ持っていないユニットへも
  一時的に付与、CD2)はバフ形状テーブルを再利用しつつ、既存の1体選択バフ(選んだ1人だけが
  受け取る)と違い「自身と選んだ隣接味方1人の両方」が受け取る初めてのケースのため、
  `BuffSkillShape`へ`alsoSelf`フラグを追加した(true時は自身を対象選択リストから除外し、
  選んだ対象へ適用した後もう一度自身にも適用する)。効果自体は常時+固定値ではなく攻撃者の
  行動に依存する条件付きのため、`effectiveDefense()`ではなく`BattleState::
  combatDefenseBonus()`に新設の`Unit::braceSkillActive`を直接参照するチェックを追加した。
  古参守備兵`provoke`(挑発。敵1体・射程2、Damageなし、CD2、次Enemy Phase、使用者を攻撃
  可能なら対象評価で最優先。Boss予告は変更しない)はMark形状に構造が似ているが、書き込む
  値が符号付き整数ではなく発動者のUnit id(`Unit::provokedByUnitId`)で、この効果を消費
  するのが`BattleController`ではなく`EnemyAI.cpp`の`takeEnemyTurn()`という別コードパスの
  ため専用分岐にした。`takeEnemyTurn()`は通常`findNearestPlayer()`で最寄りのプレイヤーを
  対象にするが、`provokedByUnitId`が設定されていれば発動者を最優先の対象に差し替える
  (Wolf/Boar Boss専用AIより手前の通常AI経路のみ)。実装時、`attackIfPossible()`の既存
  フォールバック(優先対象が射程外なら別の射程内Unitを代わりに攻撃する)が挑発を素通り
  させてしまう問題を回帰テストで実際に再現した上で、`onlyPreferred`引数を追加して
  (挑発中はフォールバックせず何もしない)修正した。槍兵`counterthrust`(反撃準備。
  攻撃者・武器射程、戦闘1回、単体武器攻撃を受け生存時、攻撃者へ通常攻撃1回)は
  `SkillCategory::Reactive`が実際に使われた初めてのSkillで、`chooseSkill()`/
  `selectSkillTarget()`を一切経由しない - 装備しているだけで、`EnemyAI.cpp`の
  `attackIfPossible()`(`takeEnemyTurn()`と`takeWolfPackTurn()`が共有する、実際に
  `resolveAttack()`を呼ぶ2箇所)から新設の`tryCounterthrust()`を呼び、装備者が攻撃を
  受けて生存していれば、攻撃者が"装備者自身の"武器射程内にいる場合のみ即座に1回反撃して
  Chargeを消費する(攻撃者の射程ではなく防御側の射程を見る - 弓兵に射程2から攻撃されても
  近接射程1のみの装備者は反撃できない)。灰角大猪Bossのsweep/chargeは`resolveAttack()`を
  経由せずHPを直接減らす専用実装のため、この2箇所へのフックだけで自然に対象外になる。
  監視弓兵`overwatch`(警戒射撃。装備武器の射程、戦闘1回、次Enemy Phase、最初に射程へ
  入った敵へ通常攻撃1回)は`chooseSkill()`側は`hold_formation`/`extended_lockdown`と同じ
  「自身のみ、対象選択なしで即座に解決」パターンだが、書き込む先がBuffKindではなく専用の
  `Unit::overwatchActive`。効果自体は`provoke`同様`EnemyAI.cpp`側で消費する - 新設の
  `triggerOverwatch()`を`takeEnemyTurn()`の2箇所(その敵が行動する直前、および移動直後)
  から呼び、`overwatchActive`な自軍Unitのうち、その敵が"監視兵自身の"武器射程内に入って
  いれば即座に1回攻撃してから`overwatchActive`を解除する(Chargeはキャスト時点で既に
  消費済み)。`provokedByUnitId`と違い「次Enemy Phase」限定の文言がないため、
  `clearSkillBuffsAtEnemyPhaseEnd()`では解除せず、実際に発動するまで複数Enemy Phaseを
  またいで持続する。現状は通常AI経路(Wolf/Boarを除く)のみに配線済み - Wolf pack/Boar
  bossの専用AI関数への複製はスコープ外として据え置いた。辺境斥候`trailblaze`(道拓き。
  仮移動で通過した灰地・浅瀬、CD2、このPlayer Phase中だけ味方の移動Costを1にして行動
  終了)は18 Skill中で唯一、既存のどの機構にも「実際に通過した経路そのもの」が残っていない
  効果だったため、`Movement.cpp`の`computeReachableTilesImpl()`へ親ポインタ追跡
  (`parentOut`)を追加し、新設の`computeMovementPath(battle, mover, destination)`がそれを
  辿って経路を逆算する(起点は除き終点は含む)。`BattleController::selectMoveTile()`が
  実際の移動より前に(`mover.position`がまだ起点のうちに)経路を`lastMovementPath_`へ
  キャプチャしておく必要があった。`chooseSkill()`は`overwatch`と同じ「自身のみ、対象選択
  なしで即座に解決」パターンで、経路上の灰地・浅瀬Tileだけを`BattleState::
  markTrailblazed()`で記録する(平地は無視)。新設した`costOverrideAt`引数
  (`BattleState::isTrailblazed()`)が、そのTileのコストを地形本来のコストや移動する側の
  兵種に関係なく常に1へ上書きする。`Unit::moveUpActive`と同じ「このPlayer Phase終了まで」
  なので`clearMoveUpAtPlayerPhaseEnd()`の隣で`BattleState::clearTrailblazedTiles()`を
  呼ぶ形にした
- **完了**: 初期6兵種18 Skill全てに実効果あり。項目1の残作業は今後追加される兵種/Skillが
  出た時の形状テーブル拡張のみ
- M4項目3(Preview/Resolverの一致)は**攻撃形状3 Skill(制圧射撃・足止め突き・奇襲)のみ
  完了**: 通常攻撃と対称な`ConfirmAttack`→`ConfirmSkillAttack`の2段階フローを追加
  (新設`BattleController::pendingSkillPreview()`/`confirmSkillAttack()`)。Previewと
  実際の解決が同じ`computeDamage()`を通るため、両者が食い違うことはコードの構造上
  あり得ない。Heal/バフ/Mark等Damageを予測しないSkillは対象外(即座解決のまま) -
  これらにPreviewする数値自体が無いための意図的なスコープ限定。UI側(`main.cpp`)の
  変更はビルド成功と回帰テストのみで検証、実機での目視確認は未実施(ヘッドレス環境)。
  実装直後の評価で`confirmSkillAttack()`が`weapon.causesKnockback`を見ておらず
  `applyKnockback()`も呼んでいないことが判明(他の全攻撃経路は呼んでいる)。重槍装備の
  槍兵が`halting_thrust`を使うとノックバックだけ発生しない実在のバグで、修正して
  回帰テストを追加済み
- 状態異常の実付与(2026-07、M4項目4): 移動低下は既にM4-Aで配線済み(制圧射撃・
  足止め突き)。よろめきの主な発生源「障害物へのノックバック衝突」を新規実装
  (`BattleState::applyKnockback()`がノックバック先を塞ぐもの一式(範囲外・他Unit・不可通行
  地形・Battle Object)を検出すると`applyStagger()`を呼ぶ)。実装中に既存バグを発見:
  `applyKnockback()`はBattle Objectを一切見ておらず、倒木などのBarrierを無視してノックバック
  が素通りしていた。同じ修正で解消。毒・炎上・自Skill経由の防御低下は対応する敵・アイテム・
  武器・クラスが未実装のため保留(倒木衝突経由の防御低下はBoss専用の別IDで実装済み)
- M4共通基盤(2026-07): 増援Wave・予告・封鎖・必須全滅条件、AI候補/Score/小隊予約、
  Wolf/Human/Defender/Ranged/Support Profile、Boss共通Telegraphを実装。未接続なのは地域Definition、
  増援HUD、Objective/Object操作・撤退候補、Boss退場理由Filter
  固定・段階移行・撤退区別の汎用化

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
- 沈黙した監視所群(M6-A/B/C項目1・2・3、`implementation_roadmap.md`詳細):
  全6地点(地点1シンダーウォッチ外門・地点2灰道の監視所・地点3Aアイアンウォッチ
  物資庫・地点3B旧兵舎・地点5信号塔下層・地点6最後の信号)が実コンテンツ、
  `RouteGraph`による地点3A/3Bの分岐(`BranchGroup`/`AllMembers`)・キャンプI/II
  まで実装済み。地点3Aは専用地形・正本敵編成・探索2択(弓兵除外+障害物2個/鉄材+1)・
  「物資箱2個のうち1個以上を確保」副目標(`surveyTileCount`+`chooseSurveyTiles()`、
  地形を変更しない汎用N枚選択、`supply_crate` Containerマーカー表示)まで実装。
  工作兵護衛・3つ目の探索選択・状態条件付き増援・加入候補は未実装
  (controllable-NPCサブシステムと`FrontierEngineer`クラス自体が存在しないため)。
  地点5は専用の2操作可能Device(副信号機・主信号機、JSON側で`interaction`を宣言する
  経路を新設)がprimary目的そのもの(デフォルトのEliminateTeamメンバーを置き換え、
  `validateBattleMission()`の「primaryグループはちょうど1つ」制約に対応)、
  軍旗保管箱の副目標、ラウンド2固定近似の増援まで実装。6ラウンド制限・3つ目の
  探索選択・軍旗記録discoveryは未実装(ラウンド超過での敗北条件がコード上どこにも
  存在しないため)。地点6(元守備隊長ボス)は`MarchCaptain`+`boostedFirstEnemy`
  (新設`strengthBonus`含む)、`ObjectiveKind::DefeatUnit`をJSON側でスキーマ化した
  `primaryDefeatUnitId`(同じくEliminateTeam置き換え方式)、既存
  `noCasualtiesBonusLoot`を再利用した「味方戦闘不能者0」副目標、実在クラス
  (`MarchCaptain`)ゆえ無効化しなかった`[行軍隊長]`探索選択まで実装。地域完了
  (`completedRegionIds`追加)は既存の`wouldRegionBeCleared()`汎用機構がそのまま
  機能し、地点6実装だけで自然に達成された。ボス固有行動3種(射線命令・防衛隊形・
  信号封鎖)・主信号機の耐久/破壊敗北条件・「元守備兵2人以上撤退」副目標・
  軍旗記録discovery・地域の最低保証報酬(取り逃し分の最終地点回収)は未実装

未実装:

- Phase 3.5実装順7: 通常操作(Base選択→出発→勝利→帰還→再出発→安全通過/再調査)の
  実機往復試験。サンドボックス環境のためGUIスクリーンショットでの目視確認ができておらず、
  ビルド成功・回帰テスト通過・文字コードカバレッジ確認のみで代替している
- 灰枝の森の残り2地点（薬草の沢、折れ木の縄張り）の探索・戦闘内容と灰角大猪。
  複数地点の順序制御と到達画面は実装済み
- 灰角大猪の突進予告、薙ぎ払い、激昂、倒木衝突
- `RegionProgress`による3地点合成の地域目標・Discovery管理
- `campaign_route_graph.md`で62地点の接続は設計済み。実行時モデルは灰枝の森の直線経路と、
  沈黙した監視所群の地点3A/3B分岐(`RouteNodeKind::BranchGroup`、`AllMembers`、
  順不同解決)のみ実装済みで、他地域の分岐・合流・Condition・Variant・旧ID Aliasは未実装
- 正式仕様へ追加した、地域入口から連続する経路確保済み地点の一括通過、既知Campでの停止選択、
  地点別`reconLoot`（初回通常素材の50〜70%）は未実装。現行UIは地点ごとに安全通過を選ぶ
- Pending加入候補、候補重複防止、安全帰還後の候補登録、集会所加入
- 残り2種のObjectiveKind（`DestroyObject`/`SurviveRounds`/`EscapeUnits`/`ProtectUnit`/
  `OperateObject`は2026-07に実装済み。複数地点確保・条件付き撃破が残る）
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
- 訓練場、簡易鍛冶台、救護テント、工作台
- 施設ノードの段階、Discovery、前提ノード、素材条件
- 施設費用の複数素材対応と一括検証後の消費
- 分岐武器に対応レシピの解放を要求
- **M5(2026-07)**: 解体・稼働枠・再建モデルを完全に廃止(`docs/base_development.md`
  「解体、素材返却、再建費は採用しない」に対応)。`BaseState::facilitySlotCapacity()`
  (段階ごとの同時稼働枠2/4/6)、`GameApp::dismantleFacilityNode()`(素材50%返却)、
  `GameApp::rebuildFacilityNode()`(空き枠があれば無料再建)を削除し、
  `facilityNodeEligible()`/`applySaveData()`の稼働枠チェックも削除。
  `unlockFacilityNode()`が元々unlockedNodeIds/builtNodeIds両方へ一括挿入していた
  ため、削除だけで「一度建設した施設は恒久的に利用可能」というモデルへ一致した。
  UIもDismantle/Rebuildボタンを削除し、施設Lv表示・施設枠表示を「解放済み分岐N」・
  開拓段階名の表示へ置き換えた(`base_development.md`完了条件#12に対応)
- **M5レビュー対応(2026-07)**: コードレビューで指摘された3点を修正。
  (1) `BaseState::builtNodeIds`を`facility_data_contract.md`が定める正式フィールド名
  `constructedFacilityIds`へ改名(`BaseState.hpp`/`Facilities.hpp`/`GameApp.cpp`/
  `SaveSystem.cpp`/`main.cpp`/`test_battle.cpp`全箇所。JSON側のキー名`"builtNodes"`は
  変更していないため、既存セーブへの互換影響はゼロ)。
  (2) `main.cpp`の`facilityIsActive()`が稼働中/未稼働(ACTIVE/INACTIVE)というトグル
  状態を表示しており「施設は稼働・停止状態を持たない」という正式仕様と矛盾していた
  ため、`facilityIsConstructed()`に改名し表示文言も建設済み/未建設
  (`ui.facility_node.constructed`/`ui.facility_node.not_constructed`)へ変更した。
  内部ロジック自体(恒久建設済みかどうかの判定)は変更していない
  (3) `implementation_roadmap.md`の段階4残り+段階5の記述にあった「解体/再建」という
  古いUI言及に「※解体・再建UIは後にM5で廃止」の注記を追加

正式仕様は[`base_development.md`](base_development.md)。今回のSliceでは意図的に対象外とした
未実装差分:

- `FacilityDefinition`/`ResearchNodeDefinition`/`RecipeDefinition`のJSON化と汎用Loader
  (`data/facilities.json`等)。現状は`include/jf/core/Facilities.hpp`のハードコード
  C++配列のまま
- 旧ID Alias(`facility_research.md`の18件のID移行表)とSchema 3への移行
- 鍛冶台建設済みのみ装備・調整変更可能というチェック自体は残っている(`constructedFacilityIds`が
  永続化されたので実質的に「一度建設すれば恒久的に許可」になっているが、
  `equipWeaponForUnit()`等のガード文自体は変更していない)
- 全12兵種の武器分岐(現状Spearmanの3分岐のみ)、10地域にまたがる施設研究ツリー
  拡張、共同施設研究、拠点段階Encampment→PioneerOutpost以降の進行・外観変化

確定済み初期費用:

全施設研究ノードの安定ID・効果・Discovery・素材・前提・解放地域・種別は
[`facility_research.md`](facility_research.md)で設定完了。共同研究4系統も設定済みだがコード未実装。
製作・所持ルールは[`item_system.md`](item_system.md)で設定完了し、消耗品1個製作、各99個上限、
武器と特性の実物個数制、探索道具の恒久一意所有を採用する。**消耗品6種は接続済み**
(`GameApp::craftItem()`が木材・獣皮・薬草を消費して`BaseState::itemStorage`(ID毎99上限)へ
1個製作し、`addPreparedItem()`/`removePreparedItem()`が所持数とバッグ間で個数を移動、
未使用分は`resetToBase()`で帰還・敗北・遠征リタイアいずれの経路でも所持数へ戻る)。
武器・調整特性・探索道具の実物個数制/恒久所有は未接続のまま。

消耗品6種の製作レシピ(このセッションで新規設定、素材は灰枝の森産の基本素材のみ):

| 消耗品 | 材料 |
|---|---|
| 救急セット | 薬草2、獣皮1 |
| 野戦治療キット | 薬草1、木材1 |
| 救命包 | 薬草2、獣皮2 |
| 野営食 | 木材2、薬草1 |
| 防護板 | 木材3 |
| 帰還信号弾 | 木材2、獣皮2 |
施設一覧、施設詳細、建設確認、研究詳細・確認、製作画面は
[`facility_ui.md`](facility_ui.md)で設定完了。現行UIの施設Lv・施設枠・解体・再建操作は
M5(2026-07)で廃止済み(上記「拠点と施設」参照)。JSON Definition駆動の画面への
本格的な置換は未着手のまま。

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
- Export / Import（`exports/`/`imports/`フォルダ経由、`.preimport.bak`退避） -
  以前ここに「未実装」と記載していたが実際には実装済みだった(2026-07訂正)
- M3-A(最小スライス): `applySaveData()`がCheckpointの`routeProgress`不正Node ID
  や`expeditionStage`範囲外を検出した際、旧実装は当該遠征を警告なく完全破棄して
  いたバグを修正。region/partyが有効な場合はPending Loot/Discoveries/Bag/
  Site Access更新/地域完了フラグとパーティHPを保持したまま地域入口へ退避する
  ようにした（`docs/expedition_recovery.md`「更新後の復旧」優先順位4に対応）
- M3-D(2026-07): `jf::migrateSave()`(`vN -> vN+1`を一段ずつ適用、現行`v1->v2`は
  デシリアライザの既定値埋めにより実質No-opだが移行前に`.schema-vN.bak`退避まで
  実施)、保存状態HUD(`Idle/Saving/Saved/Failed`、自動再試行最大3回+手動再試行、
  `main.cpp`の`drawSaveStatusHud()`)、破損復旧画面(`Restore Backup`/
  `Import Save`/`Start New`、`SaveStore::restoreFromBackup()`/
  `quarantineCorruptSave()`、`drawSaveRecoveryScreen()`)を実装。Web同期完了待ち・
  GitHub Pages更新継続試験は実ブラウザ/Pages公開環境が無いため対象外のまま

未実装:

- Web同期完了を待つ処理
- Emscripten実ビルドとブラウザ更新試験
- M3-Aの残り: Attempt ID/Checkpoint Kindの正式な型、Route/Node/Region Alias
  解決、`QuarantinedExpedition`（自動退避不能な矛盾Saveの隔離状態）、地形・敵・
  増援のgenerator-version対応Snapshot再利用。Route Graphが現状1本（Ashbough
  Forest）しかなく改版されたことがないため、実要件が生じるまで据え置き

## ローカライズ

正式仕様: [`localization.md`](localization.md)

実装済み:

- 英語と日本語を切り替え可能
- 選択言語をセーブ対象に含める
- 施設名と主要HUD文言を両言語で表示
- M3-B Slice 1(2026-07): `data/locales/ja.json`/`en.json`(17 Key)+
  `jf::loadLocales()`/`jf::tr(key, japanese[, args])`(`include/jf/core/Locale.hpp`、
  `src/core/Locale.cpp`)を新設。`GameData.cpp`の`readJsonFile()`と同じ規約(開く→
  Parse→失敗時cerr+nullopt)、`en`/`ja`のKey集合不一致は起動失敗として検出、未解決
  Keyは`[[MISSING:key]]`マーカーで可視化(黙ったフォールバックをしない)。main.cpp
  既存の`gLanguage`グローバルはそのまま再利用し、`tr(key)`という1行ラッパー経由で
  接続。共通ボタン(決定/キャンセル/戻る/待機/ターン終了/閉じる/つづける/追加/削除)、
  フェイズ見出し、設定/言語ラベル、勝利/遠征失敗見出し、データ読込失敗・4人選択
  検証文言の計17個の`kJa*`定数を削除し`tr()`呼び出しへ移行。`loadAppFont()`の
  Glyph Atlas登録も`jf::allJapaneseGlyphText()`(Locale全体の日本語Valueを自動連結)
  経由へ一部移行し、今後Key化する文字列は手動charset編集なしで網羅されるように
  した。新規`jf_locale_tests`(`tests/test_locale.cpp`)でKey集合検証・両言語の
  `tr()`戻り値・未解決Keyマーカー・Formatter・破損Locale検出を回帰テスト化
  (実データではなく一時スクラッチコピーを壊す方式 - assert失敗時のリストア漏れで
  実ファイルが壊れたまま残る事故を避けるため)
- 段階3全体(2026-07): 兵種名・役割、アイテム名・説明、素材名、武器名、
  キャラクター名、地形名、Battle Object名、拠点段階名、Discovery名、施設名・役割、
  状態異常バッジ(計約128 Key)を移行
- 段階4〜5(2026-07、完了): 戦闘ログメッセージ(Formatter初実戦投入)、戦闘HUD操作
  文言、戦闘予測ポップアップ、遠征準備・キャンプ・設定/Export/Import・探索3択・
  PreBattleDeployment・拠点/Facilities/Forge/Unit各画面を移行。`main.cpp`の
  `kJa*`定数は421個→**0個**(意図的に残す`kJaJapaneseNative`1個のみ)。呼び出し元
  0件だった`drawUnitInfo()`(デッドコード)を発見・削除。残る`pick()`呼び出しは
  `SkillDefinition`/`FacilityNode`等、データ層が最初からEn/Ja両方を持つフィールド
  を選ぶ既存パターンのみで、直書き債務ではない。詳細は`implementation_roadmap.md`
  「M3-B Locale移行」参照
- 段階6の実行可能な範囲(2026-07): `tools/check_localization.sh`(CTest名
  `check_localization`)で`kJa*`定数の残数を検査。Key集合一致・Formatter・破損
  Locale検出は`jf_locale_tests`が担当

未実装:

- 共通UI表の「主目的/副目標/敗北条件/地域成果/加入候補」(段階2、該当UIが無いため対象外)
- `check_localization`のCI化、Glyph網羅の自動検査、画像ベースの目視検査(spec項目7・8)

## 未実装の主要画面仕様

- [`battle_results_screen.md`](battle_results_screen.md): Objective、Pending差分、損耗、地域進行を表示する
  読取専用の戦闘結果画面
- [`expedition_preparation_screen.md`](expedition_preparation_screen.md): 4人、装備、Skill、特性、連携作戦、
  消耗品6枠、探索道具2枠、地域情報と警告を確定する遠征準備画面
- Tutorialと難度選択は後工程。現時点は難しめの標準難度だけを正本とする

## M6 沈黙した監視所群 実装詳細(2026-07)

ロードマップ本体には状態の一行要約だけを残し、技術的な実装判断はここへ集約する。

コンポーネント分離の進行状況:

- `ExpeditionService`着手済み(2026-07、2Slice): `jf/core/ExpeditionService.hpp`/`.cpp`。
  既存コードベースの流儀(`Region.hpp`の`computeStageVictoryLoot()`、`RouteGraph.hpp`の
  `findRouteNode()`)に合わせ、ステートフルな"Service"クラスではなく自由関数群として実装。
  第1弾は`BattleController`/`Screen`に一切触れない純粋な状態照会ロジック
  (`computeCurrentStage`/`computeExpeditionComplete`/`advanceExpeditionRouteToNextSite`等
  10個)、第2弾は`returnToBase()`の帰還処理・`updateExpeditionCheckpoint()`のスナップショット
  組み立て・`bulkPassSecuredSites()`のRoute前進ループを追加抽出した
  (`applyExpeditionReturnToBase`/`buildExpeditionCheckpoint`/`bulkAdvanceSecuredSites`)。
  `GameApp`の公開APIは一切変更していない(既存テスト・UIは無修正でそのまま通る純粋
  リファクタ)。意図的に対象外: `chooseExplorationRoute`/`continueExpedition`/
  `chooseSafePassage`/`chooseReconnaissance`/`confirmDeployment`/`placeDeploymentUnit`関連/
  `resetToBase`/`startExpedition`/`acknowledgeDefeat`/`retireExpedition`は
  `battleController_`の構築・差し替えと`screen_`遷移が大半を占め、これ以上切り離すと
  可変参照を大量に渡すだけで結合度が下がらないため見送った
- UI Screen State構造体化、第1弾着手済み(2026-07): `g`接頭辞グローバル約30個のうち、
  他ファイル未参照のファイルローカル状態と、複数ファイルが共有する横断状態のうち着手
  可能な範囲を構造体化した: `ui_camp.cpp`の`CampScreenState`、`ui_battle.cpp`の
  `BattleScreenState`、`ui_shared.hpp`の`BaseScreenState`。`gSettingsOpen`/`gSaveStore`系/
  `gSaveHudState`系/`gWarehouseCleanupOpen`/`gPendingImport`系はアプリ全体のライフサイクル
  状態で「画面State」の概念に当てはまらないため見送った
- Audio: 見送り(2026-07、検討済み)。`assets/`にAudio素材が無く`PlaySound()`呼び出しも
  皆無、既存演出(`ui_battle.cpp`の戦闘バナー/アタックランジ)は`BattleState`を毎フレーム
  ポーリングする実装で「横断イベント口」の生きた前例が無いため、実要件が具体化するまで
  `AudioManager`設計を見送った

### M6-A 地点1(外門)・地点2(監視所)・キャンプI

新しい`ObjectiveKind::HoldTile`(地点維持、`SecureTile`の単発接触/`SurviveRounds`の全体
生存のどちらにも一致しない「指定マスをNラウンド連続保持」)を`mission_objectives.md`へ
追加・実装。地点1の3つ目の選択肢`[重装兵]`は当時HeavyInfantry未実装のため無効化。
新素材`iron`/`stone`/`old_gear`を追加。想定外に大きかった発見:
`GameApp::continueExpedition()`はRoute Graph未対応の地域では地点0以降Explorationを一切
経由せず直接Battleへ進む仕様だったため、Cinderwatchを`usesRouteGraph()`へ登録して解決
(分岐自体はM6-Bで導入)。

### M6-B 地点3A/3B分岐・キャンプII・地点3B(旧兵舎)

`docs/route_graph_data.md`「分岐と合流」の`BranchGroup`/`AllMembers`モデルを、この1分岐に
必要な範囲だけ実装。`RouteNodeKind::BranchGroup`+`branchMembers`/`branchCompletion`を追加し、
各Memberの唯一の出Edgeを分岐Nodeへ戻す設計にすることで、既存の単一後続ノード探索
(`advanceRouteToNextSite()`)がBranchを何度でも再訪できるようにした。新設
`nextUnresolvedBranchMember()`が今回の遠征でまだ未解決、かつ恒久的にもSecured済みでない
Memberを選ぶ。地点3B「旧兵舎」を正本の地形・敵編成で新規実装、地点3Aは引き続き旧
プレースホルダーのまま。工作兵生存/伝令兵脱出の加入候補報酬は、Pending加入候補という
仕組み自体が当時コード上どこにも存在しなかったため保留(M7項目5が正式に担当)。

### M6-C項目1 地点3A(物資庫)本仕様コンテンツ(一部)

`ironwatch_stores`を専用地形プロファイルへ置き換え、正本敵編成へ変更。副目標「物資箱2個の
うち1個以上を確保」を`surveyObjectiveId`+`ObjectiveGroupRule::Any`で実装、新設
`surveyTileCount`+`chooseSurveyTiles()`(地形を変更しないN枚選択)へ一般化した。医療区画
choiceの`discoveries`付与を無条件からルート限定へ変更(新設`routeDiscoveries`)。意図的に
保留: 「工作兵を撤退させずに勝利」(`ProtectUnit`)はAIユニットを一時的にプレイヤー操作へ
切り替える仕組みが無く未実装。3つ目の探索選択`[辺境工兵]`はクラス未実装のため無効化。
状態条件付き増援トリガーは既存の増援機構が選択ベースのみのため未実装。

### M6-C項目2 地点5(信号塔下層)本仕様コンテンツ

`RouteGraph.cpp`に`last_signal`Siteノードを新規追加して地点6を切り出した(M6-Aの地点1/2
分割と同じ手順)。`OperateObject`Objective自体は実装済みだったが、JSON側でDevice+
Interactionを宣言する経路が無かったため新規追加。`validateBattleMission()`が
「primaryグループはちょうど1つ」を要求するため、primaryを`EliminateTeam`から2つの
`OperateObject`へ置き換える方式にした。「軍旗保管箱を確保」は3Aの物資箱と同じ
`surveyObjectiveId`+`surveyTileCount`(=1)で実装(データのみ)。UIにDevice/Containerの
描画分岐を`ui_battle.cpp`へ追加。意図的に保留: 6ラウンド制限は`mission_objectives.md`の
データモデルにも存在せず未実装。3つ目の探索選択`[辺境工兵]`は無効化。軍旗記録discoveryは
加入候補システムと登録先施設が無いため未作成。

### M6-C項目3 地点6(最後の信号、元守備隊長ボス)

`last_signal`を正本の敵編成へ置き換え。ボスは既存`MarchCaptain`クラス+
`boostedFirstEnemy`(HP+10・DEF+2)+新設`strengthBonus`(STR+2)。主目的「元守備隊長を
戦闘不能にして撤退させる」は新設`primaryDefeatUnitId`経由の`ObjectiveKind::DefeatUnit`
(既存機構をJSONスキーマ化しただけ)。「撤退」は敵の自発的撤退AIが無いため戦闘不能(HP0)の
みで判定。3つ目の探索選択`[行軍隊長]`は`MarchCaptain`が実在するクラスのため
`scoutRouteRequiredClass`で実装。新規素材「信号機の中核部品」(`signal_core`)・
「高品質鉄材」(`quality_iron`)を追加。M6完了Gateの「地域固有のC++分岐なしで6地点を生成」を、
6地点通しの遠征で安全帰還すると`completedRegionIds`へ`CinderwatchGate`が追加されることを
直接検証するテストで確認。意図的に保留: ボスの固有行動3種、主信号機の耐久/破壊敗北条件、
「元守備兵2人以上撤退」副目標、軍旗記録discoveryは未実装(理由は各対応する地点3A/5と同型)。

## M7 12兵種・仲間・会話 実装詳細(2026-07)

### 項目1 後半6兵種のClass・武器・固有能力・スキル

1兵種ずつ実装する方針で全6兵種を完了。重装兵(HeavyInfantry)の固有能力「重量装甲」・
スキル3種(装甲前進/衝撃防御/障害物破砕)、辺境工兵(FrontierEngineer)の「野戦工作」・
スキル3種(野戦補修/瓦礫爆破/即席防壁、戦闘中に動的生成するBattle Object配置を初めて
使用)、伝令騎兵(MessengerCavalry)の「再移動」・スキル3種(緊急伝令/駆け抜け/救援搬送)、
辺境猟兵(FrontierRanger)の「簡易罠」・スキル3種(拘束罠/獲物を読む/追い込み射撃)、
旗手(BannerBearer)の「戦旗」・スキル3種(奮起の旗/行軍の律動/不退の合図)、戦闘魔導士
(BattleMage)の「魔力波及」・スキル3種(連鎖魔弾/地表灼熱/魔防破砕)。

再移動(攻撃・スキル・アイテム行動後、生存していれば最大2マス移動して行動終了)は
`BattleController::finishPlayerAction()`を`bool`返却化し、17箇所の呼び出し元すべてに
委譲ガードを追加する形で実装した。簡易罠/拘束罠と地表灼熱は「ユニットが踏んだ/一定時間
経過した瞬間に自動発動する」新規メカニクスで、前者は`EnemyAI.cpp`の敵自発移動4箇所へ
トリガー判定を追加し、後者は`rapid_barricade`と同じBattle Object期限切れパターンを
再利用した。戦旗(距離2以内の味方STR/MAG+1)は`computeDamage()`/`previewAttack()`/
`resolveAttack()`へデフォルト値0付きの`attackerBonusPower`引数を追加し既存呼び出し元を
無改修のまま保った。不退の合図(距離2以内の味方が受ける最初の移動低下/よろめきを無効化)は
`applyMoveDown()`/`applyStagger()`/`applyStatusEffect()`/`applyWeaponOnHitStatuses()`/
`resolveAttack()`へ`BattleState&`引数を追加する形で実装。魔防破砕(次に受ける魔法攻撃限定の
ダメージ+3)は既存の`markedBonusDamage`と並列の`magicMarkedBonusDamage`フィールド追加のみ。
`read_quarry`(獲物を読む)のみ、既存エンジンに敵AI行動予測を保持・表示する仕組みが無い
ためデータフラグ(`Unit::quarryRevealed`)のみの実装(プレビューUIは対象外)。

### 項目2 加入経路(最小の縦切り: 加入基盤+重装兵1体)

`ExpeditionState::pendingRecruitCandidateIds`(遠征中は保留、敗北で破棄)→
`BaseState::joinReadyCandidateIds`(安全帰還で恒久化、以後は別遠征の敗北でも失わない)→
`GameApp::confirmRecruitJoin()`(`BaseState::joinedRecruitIds`へ恒久化しRoster追加+Tier1
スキル自動装備)という3段階のパイプラインを、既存の`pendingDiscoveries`→
`discoveryRegistry`パターンと`BaseState`の単調増加集合パターンをそのまま踏襲する形で新設。
灰角大猪(`brokenwood_territory`)撃破で重装兵(`heavy_recruit`、表示名「ハドリク」)の加入
候補が付与される。受け入れ枠は`BaseState::recruitCapacity()`として共同テント6人/宿舎増築I
後8人の2段階のみ実装(専門区画11人・遠征別棟12人は対応するDiscovery/地域完了判定が
未実装のため対象外)。UIは拠点画面に最小限の「加入可能: ハドリク」ボタンのみを追加し、
`docs/gathering_place.md`が定義する本格的な会話ツリー・既読状態・立ち絵UIは項目4として
対象外のまま。Save/Loadは`joinReadyCandidateIds`/`joinedRecruitIds`をシリアライズし、
`GameApp::applySaveData()`に`roster_`再構築ループを追加(`roster_`は起動時に静的データから
一度だけ構築されるため、セーブ経由で加入したUnitを再ロード時に個別に復元する必要が
あった)。同じ理由で、Tier1スキルの自動装備は`requiredTrainingNodeIdFor()`の施設解放
チェックを経由しない無条件付与のため、`applySaveData()`のスキル復元ロジックも
`joinedRecruitIds`に含まれるUnitのTier1スキルだけそのチェックを迂回するよう分岐を
追加した。残り5兵種(辺境工兵・伝令騎兵・辺境猟兵・旗手・戦闘魔導士)の加入条件配線は
今後のSliceで`confirmRecruitJoin()`のクラス分岐を1件ずつ拡張する形で追加する。

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
