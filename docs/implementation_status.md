# JOJIFrontier 実装状況まとめ

文書種別: **進捗記録**
仕様索引: [`README.md`](README.md)
この文書は現行コードとの差を記録し、正本を上書きしない。

更新日: 2026-07-22

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

## M6-D 地域締め 実装詳細(2026-07)

### 地域の最低保証報酬

`docs/regions/cinderwatch_gate.md`「地域の最低保証報酬」の表(鉄材5、石材3、旧軍備3、
信号機の中核部品1、偵察資料1、野戦医療記録1、信号技術資料1、軍旗記録1)を実装。
`BaseState::cinderwatchMaterialsEarned`(`unordered_map<string,int>`)が、地域が
`completedRegionIds`へ入るまでの間、Cinderwatchの各サイトが安全帰還のたびに実際に
持ち帰った素材を延べで積算する(消費可能な`storage`とは別カウンタ - `storage`は工房消費で
減るため代用できない)。`ExpeditionService.cpp`の`applyExpeditionReturnToBase()`で、
今回の帰還が地域を完了させる場合(`pendingRegionCompletions`にCinderwatchGateを含む)、
不足分をfloorとの差分だけ`materialAdds`へ上乗せしてから既存のcap/overflow計算
(`fitPlan`/`overflowPlan`)へ合流させ、新しい適用経路を作らずに済ませた。同じタイミングで、
未取得のキーDiscovery4種(`kCinderwatchReconDiscovery`/`kFieldMedicineDiscovery`/
`kReturnSignalDiscovery`/新設`kBannerRecordsDiscovery`)を`discoveryRegistry`へ直接
補充する。軍旗記録(`last_signal_banner_records`)は「2人以上撤退・降伏」トリガー自体が
M6-C項目3で意図的保留のままのため、この最低保証の直接付与が唯一の取得経路になる
(仕様の「最低保証だけで次地域解放と基礎施設解放が可能」と整合)。

### 灰鉄採石場(最小プレースホルダー地域)の解放

`RegionId::AshironQuarry`を追加し、`regionUnlocked()`へ
`completedRegionIds.count(CinderwatchGate) > 0`の1ケースを追加。中身は
`ashironQuarryRegion(data)`(`ashboughForestRegion()`と同型の組み立て関数)が
`data/regions.json`の新規プレースホルダー1地点(`ashiron_quarry_outpost`、
`ash_road`地形プロファイル流用、Bandit2体、鉄材1・石材1のみ)を返すだけの最小構成 -
M6-A着手前の旧`cinderwatch_outpost`と同じ役割。本格的な5地点コンテンツは引き続きM9
(残り8地域)の担当。`regionSummaries()`(拠点画面の地域選択)がRouteGraph未使用の単一
地点region.stagesをそのまま扱えたため、Route Graph登録は不要だった。

**副次修正**: 3地域目の追加で、拠点画面の「未解放」ツールチップが常に固定文言
「灰枝の森を攻略すると解放されます」(`exploration.region_locked_ashbough_forest`
キー)を出していたことが判明 - CinderwatchGateにしか正しくない文言だった。
`RegionSummary`へ`lockedByDisplayNameEn`/`lockedByDisplayNameJa`(ロック解除に必要な
直前地域名、`computeRegionSummaries()`内の`predecessor()`が算出)を追加し、
`exploration.region_locked`(`{region}を攻略すると解放されます`のプレースホルダー付き
キー)へ置き換えて地域ごとに正しい文言を出すよう修正した。

### 4〜6周の実戦計測

`tools/forest_balance.cpp`をAshboughForest専用のハードコードから`--region=<id>`引数
対応へ最小拡張(`regionIdFromStringStrict()`を利用、未指定時は従来どおり
AshboughForestで出力形式も不変)。「fresh party per site」ループはstage数に依存しない
既存ロジックのため無改修で動作。「3-site expedition」の連続周回ループは
`std::array<int,3>`だった`reached`集計を`std::vector<int>`化し、AshboughForest固有の
「Territory入場時HP%」計測は`regionId == AshboughForest`の時だけ行うようガードした。

実測結果(Cinderwatch、30 Seed、初期4人パーティ、消耗品・手動配置・Camp撤退なしの
worst case): fresh-party単体win率はOuter Gate 40%、Ashroad Watch 100%、
Ironwatch Stores 16.7%、Old Barracks 3.3%、Signal Tower/Last Signal 0%
(Direct policy)。6-site通し周回のwin率は0%。[[jf_forest_balance worst-case numbers]]
と同じ理由で、これは「消耗品・撤退・戦術的HP管理を一切行わない」worst caseであり、
加えてSignal Tower/Last Signalの0%は本ツールのDirect/Tactical AIがDevice操作
(`OperateObject`)を一切扱えないという計測ツール側の既知の欠落も混じっている
可能性が高い(このツールにDevice/Interact操作のAIロジックは存在しない)。Ashbough
Forestの同条件でのwin率(fresh party 80-100%、3-site通し0-50%)と比べてCinderwatchが
明確に厳しい傾向にあることは記録するが、[[jf_forest_balance worst-case numbers]]の
教訓どおり、実際のバランス調整は行わず、実測結果として記録するに留めた。実際のプレイ
(消耗品・Camp撤退あり)での確認は今後の課題として残す。

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
追加した。加入候補の表示名・兵種は`data/units.json`の`recruits`配列へ移し、
`confirmRecruitJoin()`、拠点UI、Save復元が同じ定義を参照する。残り2兵種
(辺境猟兵・戦闘魔導士)の加入条件配線は今後のSliceで`recruits`定義と加入候補付与条件を
追加する形で拡張する。

### 項目2続き 加入経路(辺境工兵・伝令騎兵・旗手、M7-2)

`data/units.json`の`recruits`配列へ`engineer_recruit`(オレン、FrontierEngineer)・
`cavalry_recruit`(カエル、MessengerCavalry)・`banner_recruit`(レッサ、BannerBearer)を
追加しただけで、既にデータ駆動化済みの`confirmRecruitJoin()`/拠点UI/Save復元がそのまま
機能した(新規コード不要)。残るは各候補の「付与条件」の配線だけ。

正本(`docs/regions/cinderwatch_gate.md`「報酬と加入」)では辺境工兵・伝令騎兵の候補
条件は「工作兵生存」「伝令兵脱出」という護衛NPCの生存だが、この護衛NPCを一時的に
プレイヤー操作/AI保護する仕組み(ProtectUnit・脱出護衛)はM6-B/Cで意図的に未実装のまま
保留されたサブシステムのため、今回も実装しなかった。相談の結果、`ironwatch_stores`/
`old_barracks`の通常勝利をそれぞれの候補付与条件として近似した
(`GameApp::proceedToCamp()`、`heavy_recruit`が`brokenwood_territory`勝利をそのまま
トリガーにしたのと同じ簡略化パターン)。旗手(`banner_recruit`)は正本どおり「軍旗記録
registered」= 地域攻略後の安全帰還そのものが条件で、これはM6-Dの最低保証報酬コミットで
既に保証済みのため護衛NPCのようなギャップがなく、`ExpeditionService.cpp`の
CinderwatchGate完了top-upブロック内へ`baseState.joinReadyCandidateIds.insert(
"banner_recruit")`を1行追加するだけで実装した(Pendingを経由せず、同じ安全帰還
Transaction内で直接恒久化)。

副次的に見つかったギャップも埋めた: `docs/roster_design.md`「受け入れ枠」の専門区画
(11人)は「野戦工作記録を安全帰還」が条件だが、この`野戦工作記録`Discovery自体が
コード上どこにも存在しなかった(`ironwatch_stores`の`工具庫`ルート/`CollapsedSidePath`は
戦闘効果のみ実装済みで、医療区画ルート側の`ironwatch_field_medicine_records`と非対称
だった)。新規安定ID`ironwatch_field_construction_records`を`CollapsedSidePath`の
`routeDiscoveries`へ追加し、`BaseState::recruitCapacity()`へ専門区画(11人)の第3段階を
実装した。これが無いと初期6人+heavy_recruit+engineer_recruit+cavalry_recruit+
banner_recruitの組み合わせが宿舎増築I止まりの8人枠にすぐ収まらなくなり、正本の想定
タイミングで実際に加入できなくなるため、M7-2の範囲に含めた。

### 項目2続き 加入経路(辺境猟兵)

`ranger_recruit`(ヴェイラ、FrontierRanger)を`recruits`配列へ追加。正本
(`docs/roster_design.md`「加入タイミング」)の候補条件「森の踏査記録、安全帰還」は、
既存の`kAshboughForestSurveyCompleteDiscovery`(灰枝の森地域完了時にPendingへ積まれ
安全帰還で恒久化される既存Discovery)そのものだったため、護衛NPCのような未実装
ギャップが無く、`banner_recruit`と同型の「地域完了そのものが条件」パターンで実装
できた。`GameApp::proceedToCamp()`のAshboughForest地域完了判定ブロック(この
Discoveryを`pendingDiscoveries`へPushしている箇所)へ、同じ`if`内で
`expedition_.pendingRecruitCandidateIds.insert("ranger_recruit")`を1行追加するだけ
(banner_recruitとは異なりPending経由 - 安全帰還まで確定させず、敗北/中断で失う
`heavy_recruit`と同じ扱い)。受け入れ枠は既存の宿舎増築I(8人、AshboughForest完了と
同一条件)がそのまま適用され、新しい段階は不要だった。

### 項目3 ユニットページと装備共有(武器重複装備の防止・装備スキル2枠UI)

正本(`docs/character_progression.md`)が定めるユニットページ本体(一覧・詳細・
比較パネル・クールダウン表示・連携戦術・探索能力表示)は依然未着手 - 現在の
`drawUnitScreen()`(`src/ui_facilities.cpp`)は名前・クラス・ステータス表示のみで、
装備切替UIもSpearman専用のまま、スキル・特性はUIに全く露出されていない。今回は
M7完了Gateが明記する正確性要件「同じ武器1本を複数人へ同時装備できない」だけを
実装した。

正本(`docs/item_system.md`「武器と特性の共有」)は武器を共有倉庫の実物本数で管理し、
`craft_long_spear`等の製作のたびに1本生産される想定だが、現在の`craft_long_spear`等は
`FacilityNode`の一回限りレシピ解除(`unlockFacilityNode()`)として実装されており、
解除後は無制限に装備可能(所有本数という概念がコード上どこにも存在しない)。本格仕様
どおりの実装には新しい製作アクションと武器在庫フィールドの新設が要る大きめの作業に
なるため、相談の結果、新しい在庫モデルは作らず**軽量近似**(`weaponOverrides_`内で
同じ`weaponId`を別ユニットへ重複割当てすることだけ拒否する)で実装した。基本武器
`iron_spear`(全Spearmanの既定武器、正本上も加入時に各自1本支給される)はこの
重複チェックの対象外。

`GameApp::equipWeaponForUnit()`(`src/core/GameApp.cpp`)の非`iron_spear`分岐へ、
`weaponOverrides_`を走査して他ユニットが同じ`weaponId`を既に持っていれば拒否する
チェックを追加。`GameApp::applySaveData()`の武器復元ループも同様に無条件で
Save内容を復元していたため、同じ制約を追加(手編集や旧Save由来の壊れた重複状態を
弾く) - `unordered_map`の走査順は非決定的なため、`unitId`でソートしてから先着順に
確定させ、どのSaveを読んでも同じ結果になるようにした。UI(`drawForgeEquipmentPanel()`、
`src/ui_facilities.cpp`)側は戻り値を見ておらず、他ユニットが既に持っている武器の
ボタンが押せてしまう(クリックしても状態が変わらないだけ)見た目の改善は、
ユニットページ本体のSliceへ送った。

### 項目3続き 装備スキル2枠UI(全兵種)

`GameApp::equipSkillForUnit(unitId, slotIndex, skillId)`(`src/core/GameApp.cpp:598`)は
元々クラス非依存の汎用実装(スキル側の`unitClass`一致と`requiredTrainingNodeIdFor()`の
解放チェックのみ)だったが、これを呼ぶUIがコード全体でどこにも存在せず、スキルは
加入時のTier1自動装備のまま変更不可能だった。`drawUnitScreen()`
(`src/ui_facilities.cpp`)へ`drawSkillEquipmentPanel()`を新設し、武器/特性カードの
Spearman専用`if`分岐の外側(両分岐共通)へ配置 - 初めて全兵種共通の装備UIになった。
`skillsForClass(unit.classId)`でクラスの3スキル(Tier1〜3)を取得し、2枠それぞれへ
選択ボタン+解除ボタンを表示する。

実装中に判明した誤認: 当初「Tier1は加入時に無条件付与されるため、再装備時も
`requiredTrainingNodeIdFor()`の解放チェックを経由しない」と想定していたが、実際の
`equipSkillForUnit()`はTier1を含む全Tierで訓練所解放を要求する(無条件付与は
`confirmRecruitJoin()`の加入直後の1回だけで、`equippedSkills_`へ直接書き込む別経路)。
UIのボタン有効化条件をTierに関わらず`trainingUnlocked`一本に修正して対応した。
2枠間の重複防止(同じスキルを両枠に同時装備できないようにする)は`equipSkillForUnit()`
自体を変更せず、UI側でボタンを無効化するだけの対応にとどめた(装備UIが1つしか
存在しない現状ではUI側のガードで十分と判断)。

### 項目3続き 探索能力表示

正本(`docs/exploration_system.md`「兵種による探索能力」)を確認したところ、探索能力が
明記されているのは12兵種中5兵種(辺境斥候・辺境工兵・辺境猟兵・暁の衛生兵・重装兵)
だけで、残り7兵種(戦闘特化兵種)には記述が無いと判明。既存の「Spearmanだけ武器分岐を
持つ」と同じ「正本に無いものは作らない」方針に従い、`explorationAbilityFor(UnitClass)`
(`src/ui_shared.cpp`)を新設し、該当5兵種は対応Locale Keyを返し、残り7兵種は空文字列を
返すswitchとして実装。`drawUnitScreen()`(`src/ui_facilities.cpp`)のidentityカードへ、
空でない場合だけ「探索能力」セクションを追加する(該当しない兵種はセクション自体を
出さない)。新規Locale Keyは`tr()`経由の通常Keyのため`allJapaneseGlyphText()`が自動収集し、
手動Glyph登録は不要だった。`explorationAbilityFor()`は`ui_shared.cpp`にあり
(`CMakeLists.txt`の`jf_lib`ソース一覧から意図的に除外されている、raylib型に依存する
UI層のファイル)、`jf_battle_tests`からはリンクされないため専用の単体テストは追加せず、
既存の`jf_locale_tests`/`check_localization`によるLocale Key整合性検証とビルド成功を
検証手段とした(同ファイルの他のUI Helperと同じ既存の検証パターン)。

### 項目3続き 比較パネル・一覧の加入候補非表示ロジック確認

正本(`docs/character_progression.md`「ユニットページ」一覧)の「比較対象を1人固定
できる」を実装。`ui_shared.hpp`の`BaseScreenState`(`viewedUnitId`と同じ置き場所)へ
`comparisonUnitId`(Save対象外、画面遷移で消えて構わない一時的な選択状態)を追加し、
`drawBasePartyRoster()`(`src/ui_base.cpp`)の各行へ既存の「詳細」ボタンと並べて
「比較」ボタン(トグル式)を追加、`drawUnitScreen()`(`src/ui_facilities.cpp`)の
identityカード下部へ、選択中と異なるUnitが比較対象に設定されていれば名前・兵種・
能力値を並べて表示する小さなカードを追加した。正本の「能力値差を緑赤だけで表現
しない」を満たすため、色分けはせず「HP 20 (比較: 24)」のように数値を並べるだけに
とどめた。比較対象のUnitが後から`roster()`から消えた場合(通常は起きないが、防御的に)
静かに`comparisonUnitId`を解除する。`GameApp`/Saveの変更は一切不要だった。

「一覧の加入候補非表示ロジック」(正本: 未加入者の名前・兵種を表示せず「加入候補
あり」とだけ表示)は調査の結果、対応不要と判明した - `drawBasePartyRoster()`は
`app.roster()`のみを列挙しており、`roster()`は加入済みUnitのみを含む(未加入の
加入候補は`joinReadyCandidateIds`にあるだけでrosterには入らない)。見せる名前自体が
まだ存在しないため、正本の要件はすでに満たされている。

## M9 残り8地域 実装詳細(2026-07)

### M9-A 灰鉄採石場: 地域骨格 + 地点1(崩落した搬入口)

M6-Dで追加した最小プレースホルダー(1地点、汎用Bandit2体)を、正本
(`docs/regions/ashiron_quarry.md`)どおりの5地点構成の骨格へ拡張した。M6-A方式
(最初は1地点だけ実コンテンツ、残りはプレースホルダーのまま次Sliceへ)を踏襲。

`src/core/RouteGraph.cpp`の`ashironQuarryGraph()`を、
`entrance → quarry_entrance → quarry_terrace → キャンプI →
BranchGroup(quarry_old_mine | quarry_hoist_works) → ashiron_vein → キャンプII →
quarry_collapse_core → exit`へ拡張。Cinderwatchの唯一のBranchGroup
(`cinderwatch_stores_barracks`)が`AllMembers`(両方確保必須)だったのに対し、
正本の地点3分岐は「どちらか1つで進行可」- 既存の`BranchCompletion::AnyMember`
列挙値は宣言されていたが実際には一度も使われておらず、分岐解決ロジック
`findNextUnresolvedBranchMember()`(`src/core/ExpeditionService.cpp`)は
`branch.branchCompletion`を一切参照せず常に「全メンバー解決」を要求する実装のまま
だった。今回`AnyMember`の場合は「メンバーのいずれか1つがこの遠征内で解決済み、
または既に恒久Secured済みなら分岐完了」という判定を追加し、`AllMembers`の既存挙動は
そのまま維持した(Cinderwatchの回帰テストが無改修で通ることを確認済み)。

地点1(`quarry_entrance`、崩落した搬入口)は正本の探索3択(標準/斜面ルート/
`[重装兵]`ルート)・敵編成(斧兵2・弓兵1・槍兵1)・報酬・副目標(搬入口標識確保)を
そのまま実装。`[重装兵]`ルートの「小型瓦礫1個を除去」(ObjectPlacementRuleの
バリア数削減)は、このSliceでは地点自体にバリア配置ルールを設けていないため
未実装のまま(地形生成・破壊可能Object本格導入は正本「実装順」項目2で、次以降の
Slice担当)。「標識確保で採石場旧図面を入手」も、既存コードには「探索副目標成功時
限定のDiscovery付与」という汎用の仕組みが無く(`surveyBonusLoot`はLoot専用)、
新しい仕組みを作るには早いと判断し、このSliceでは見送った。

地点2〜5(`quarry_terrace`/`quarry_old_mine`/`quarry_hoist_works`/`ashiron_vein`/
`quarry_collapse_core`)はM6-D当時の`ashiron_quarry_outpost`と同じ役割の最小
プレースホルダー(汎用Bandit編成、少量の素材報酬)として新設。`jf_forest_balance
--region=ashiron_quarry`(100 Seed)で実測記録: 地点1(崩落した搬入口)のfresh-party
win率はDirect 37%/Tactical 35%で、Cinderwatchの外門(40%前後)と近い難度感。
[[jf_forest_balance worst-case numbers]]の教訓どおり、実測記録のみで数値調整は
行わない。

戦闘魔導士(`mage_recruit`)の加入経路配線は、正本上「灰鉄採石場攻略後」(地域完了、
異常鉱脈記録registered)が条件のため、地域が本格的に完了できるようになる後続Slice
(地点2-5が実コンテンツ化され、`ashiron_quarry_secured`の複合完了条件を実装する段階)
まで持ち越す。

### M9-B 灰鉄採石場: 地点2(砕石段丘)

正本(`docs/regions/ashiron_quarry.md`「2. 砕石段丘」)を確認したところ、2つの新規
サブシステム(遅延地形ハザードの「落石予告2列」、オブジェクト喪失による新しい敗北
条件「鉱石箱2個を両方失うと敗北」)を要求すると判明。どちらも現行エンジンに一切
存在せず、相談の結果、今回はこの2つを見送り、地形・報酬の差分だけで近似する方針で
実装した。

主目的のOR条件(敵全滅、または搬出標識で行動終了)は、Cinderwatchの`ashroad_watch`
(灰道の監視所)が導入した`primaryHoldTileAlternative`(既定`primary`グループを
`Any`へ広げ、`HoldTile`Objectiveを追加するパターン)と全く同じ形の
`primarySecureTileAlternative`(`ObjectiveKind::SecureTile`版)を新設して実装した。
タイル選定は既存の`chooseHoldTile()`(Kind非依存の汎用ゾーン内タイル選定、
`src/battle/BattleFactory.cpp`)をそのまま流用でき、新しいアルゴリズムは不要だった。
`StageDescriptor`/`StageContentData`は`HoldTileMissionRule`型を再利用し
(`requiredHoldRounds`フィールドはSecureTileでは単に無視される)、専用型は増やして
いない。

「荷車固定ルートで槍兵1追加」は新しい「ルートで敵を増やす」機能を作らず、
`enemyRoster`へ最初から槍兵を含めておき(4体+槍兵=5体を基本形)、他の2ルート
(回避/工兵)側で既存の`enemiesRemoved:1`により槍兵だけを除いて4体にする、という
既存フィールドの向きを逆手に取った実装で対応した。「鉱石箱2個のうち一部確保で
ボーナス」は`ironwatch_stores`(アイアンウォッチ物資庫)の「物資箱2個確保」と全く
同じ形(`surveyObjectiveId`+`surveyTileCount:2`+`surveyTileObjectDefinitionId`)で
実装(「持ち去られると失う」の敗北条件部分だけを省略した近似)。新素材
`combustion_oil`(燃焼油、工兵ルート報酬)を`materialNameFor()`の`known`セット+
Localeへ追加。

`jf_forest_balance --region=ashiron_quarry`(100 Seed)の実測: 地点2
(砕石段丘)のfresh-party win率はDirect 82%で、地点1(崩落した搬入口)の37%より
大幅に高い - SecureTile代替経路が単純なDirect/Tactical方策でも「タイルへ辿り着く
だけ」で勝てる分、敵全滅要求より易しいことを反映していると見られる。
[[jf_forest_balance worst-case numbers]]の教訓どおり、実測記録のみで数値調整は
行わない。

### M9-C 灰鉄採石場: 地点3B(巻上機区画)

地点3分岐(旧採掘坑/巻上機区画、`AnyMember`)のうち、3A(旧採掘坑)は主目的自体が
「作業員を護衛して脱出させる」というプレイヤー操作可能な一時的ゲストユニットを前提とし、
このサブシステムは現行エンジンに一切存在しない(Cinderwatchの`old_barracks`
「伝令兵脱出」も同じ理由で未実装のまま)。相談の結果、3Bを先に実装し3Aは保留する
方針とした。

3B(`docs/regions/ashiron_quarry.md`「3B. 巻上機区画」)の主目的はOperateObjectで、
Cinderwatchの`signal_tower`(信号塔下層)が導入した「`objectPlacementRules`で
`operateObjectiveId`を設定したDeviceを配置すると、`BattleFactory.cpp`の
`assembleScenario()`が既定の`eliminate_enemies` primaryを1回だけ取り除き、配置された
Object各1体につき`ObjectiveKind::OperateObject`を追加する」既存パターンをそのまま
1台のDevice版として流用した。このロジックは配置数に依存しない汎用実装(コードを
読んで確認済み)なので、Device1台の3Bと2台のsignal_towerで全く同じコードパスが動作し、
**コード変更は一切不要**、`data/regions.json`へのコンテンツ追加のみで実装した。

調査の結果、`ObjectiveKind::OperateObject`の完了判定(`ObjectiveTracker.cpp`)は
`interactionCount > 0`(1回操作)で即完了する実装で、「N回操作」という閾値判定は
存在しない。正本の「巻上機を2回操作して停止」「敵全滅後は1回操作で可」は、この
既存の1回操作判定へ近似した(新しい閾値カウント機構は作らない)。同様に「巻上機耐久
6以上残す」副目標ボーナスと「巻上機破壊で敗北」は、Object破壊による敗北条件自体が
既存に無い(M6-Cの主信号機と同じギャップ)ため見送った。工兵ルートの「操作回数2から
1」「耐久+3」ボーナスも、操作回数の閾値自体が上記近似で1回固定になるため意味が
薄れ、実装していない。

敵編成は斧兵2・槍兵1・弓兵2(5体、`signal_tower`と近い規模)。探索3択は
`quarry_terrace`(M9-B)と同型の`routeOutcomes`/`routeVictoryLootDelta`で実装:
標準ルート「巻上機を奪還する」、`enemiesRemoved:1`+鉄鉱石-1の「鉱石箱を先に落とす」、
`scoutRouteRequiredClass: FrontierEngineer`+燃焼油+1の「補助動力をつなぐ」。報酬は
勝利で鉄鉱石2・木材1。恒久成果(`quarry_hoist_restored`相当)は既存の一般機構
(勝利+`siteAccess`昇格)がそのまま処理する。

テストは`tests/test_battle.cpp`へ3件追加: (1) `AnyMember`分岐が`quarry_hoist_works`
単独の解決でも先へ進める(M9-Aで追加した`quarry_old_mine`単独版と対のテスト)。
RouteGraphの`branchMembers`順序上、`chooseExplorationRoute()`による通常の探索フローは
常に先頭メンバー(`quarry_old_mine`)から提示されるため、`quarry_hoist_works`の解決は
`siteAccess`を直接`Secured`にして再現した。(2) Device操作で勝利すること、
(3) 3ルートの敵数・報酬差分。(2)(3)はいずれも同じ理由(分岐が常に`quarry_old_mine`を
先に提示する)で、`createScenarioBattle()`を`quarry_hoist_works`のStageDescriptorへ
直接渡す形で検証した(`primarySecureTileAlternative`の低レベルテストと同じ手法)。

`jf_forest_balance --region=ashiron_quarry`(500 Seed)の実測: Hoist Works
(巻上機区画)のfresh-party win率はDirect/Tactical双方0%(timeoutあり)。これは
Signal Tower/Last Signalで既に記録済みの既知の欠落と同じ理由 - 本ツールのDirect/
Tactical AIはDevice操作(`OperateObject`)を一切扱えない([[jf_forest_balance
worst-case numbers]]参照) - によるもので、実際のプレイでの挙動を表す数値ではない。
[[jf_forest_balance worst-case numbers]]の教訓どおり、実測記録のみで数値調整は
行わない。

### M9-D 灰鉄採石場: 地点5(崩落核)/ 地域ボス「灰殻穿岩虫」

地点4「灰鉄鉱脈」は正本上、地点3A(旧採掘坑)と同じ「操作可能なゲストユニット」
(戦闘魔導士イリエン、HP0で死亡せず撤退)を主目的の前提としており、現行エンジンに
未実装と判明(相談の結果確認済み)。3A同様に保留し、地点5「崩落核」(地域ボス
「灰殻穿岩虫」)を先に実装した。

ボス戦の実装前例として、Ashbough Forestの「灰角大猪」(`UnitClass::AshenhornBoar`、
`EnemyAI.cpp`の`takeBoarBossTurn()`)が既に存在し、テレグラフ攻撃(`BossTelegraph`/
`TelegraphState`、`BossRuntime.hpp`)・HP50%閾値での状態変化・退場理由の分岐
(`UnitExitReason::ScriptedWithdrawal`、`ObjectiveTracker.cpp`)はいずれもUnit/
BattleState上の汎用フィールドとして実装済みだった。新規`UnitClass::AshironGrubworm`
(`data/classes.json`、正本の基本値どおりHP56/STR9/DEF8/RES2/MOV3)を追加し、
`takeGrubwormBossTurn()`を`takeBoarBossTurn()`と同じ構造で新規実装:

- **潜行突進**: `BossTelegraph`/`chargeTelegraphed`/`chargeDirection`/
  `chargeCooldownActions`/`bossRuntime`(いずれもボア専用ではない、Unit上の汎用
  フィールド)をそのまま流用し、`executeBoarCharge()`と同型の
  `executeGrubwormCharge()`(直線移動、通過ユニットへSTR+3ダメージ、Barrier系
  Objectへ衝突で停止)を実装。
- **岩殻防御**: 新規`bool bossChargeRecoveryPending`フィールドで「潜行から出た
  直後の1アクションだけDEF+2ボーナスを失う」を表現(既定はDEF+2=10、直後の1
  アクションだけDEF8)。
- **崩落誘発**: 新規`bool bossCollapseUsed`フィールド(灰角大猪の`bossEnraged`と
  同じ役割の1回限りフラグ)。HP50%以下になった最初のターンで、`BattleState::
  setTerrain()`(プレイヤーSkillの地形変更で既に実行時利用実績あり)を使い、盤面の
  空きマス(自身の位置・ユニット占有・他Object占有を除く、行優先の決定的走査で先頭
  2マス)を`TerrainType::Rubble`(通行不可)へ即座に変換する。正本の「1ラウンド前に
  予告」は、潜行突進のような繰り返し発生する攻撃テレグラフとは性質が異なる1回限りの
  盤面変化のため、即時発動へ近似した(予告表示は実装していない)。

正本の主目的「灰殻穿岩虫のHPを0にして撤退させる AND 封鎖杭2箇所を設置し AND
耐久1以上で守り抜く」というAND合成を単一Battleの主目的として組む機構は現行に無い
(`primaryHoldTileAlternative`/`primarySecureTileAlternative`はOR代替のみ、
`operateObjectiveId`は既定Primaryを丸ごと置き換える動作でAND合成ではない)。新しい
AND合成の汎用機構を1地点のためだけに作るのは過剰実装と判断し、主目的は標準の
`EliminateTeam`(ボス含む敵全滅)のみとし、「封鎖杭2箇所」は`ironwatch_stores`/
`quarry_terrace`が使う既存の`surveyObjectiveId`+`surveyTileCount:2`+
`surveyTileObjectDefinitionId`(2箇所確保でボーナス報酬)パターンへ近似した
(`quality_iron`1個)。「支柱2本」「封鎖杭の耐久を保つ」「両杭永久崩落で敗北」は、
Objectの破壊が敗北条件をトリガーする機構が既存に無い(M6-C主信号機・M9-C巻上機と
同じ既知のギャップ)ため見送った。探索3択の第3(`[戦闘魔導士]`、イリエン加入候補
確定が条件)は、`BattleMage`クラス自体は実装済みだが「加入候補確定」フラグが未配線
(既存の既定方針どおり)のため`scoutRouteDisabled: true`で無効化(`brokenwood_
territory`/`cinderwatch_outer_gate`と同じ前例)。第2ルート「古い火割り溝」の
「燃焼油1消費でボスHP-6」は、アイテム消費によるルート選択ゲート機構もルート別の
敵初期HP減算機構も存在しないため、`partyDamage:1`+木材+1という単純な報酬差分のみで
近似した。

`ObjectiveTracker.cpp`のScriptedWithdrawal分岐へ`AshironGrubworm`を追加。新素材
`ashiron_shell`(穿岩殻、勝利報酬)を`materialNameFor()`の`known`セット+Localeへ
追加。敵編成は灰殻穿岩虫1体+雑魚2体(`brokenwood_territory`のボス+Wolf1体という
規模に倣った)。

`jf_forest_balance --region=ashiron_quarry`(500 Seed)の実測: Collapse Core
(崩落核)のfresh-party win率はDirect 98.8%/Tactical 89.8%(平均11ラウンド前後)。
主目的が標準`EliminateTeam`のみ(Device操作不要)のため、Hoist Works/Signal Tower
のような「本ツールのAIがDevice操作を扱えない」ことによる0%張り付きが発生せず、
ボスAI自体は両ポリシーで機能することを確認できた。[[jf_forest_balance worst-case
numbers]]の教訓どおり、実測記録のみで数値調整は行わない。

### M9-E 黒水低湿地(第4地域): 地域骨格 + 地点1(灰水の沈み道)

灰鉄採石場は地点5(ボス)まで実コンテンツ化済みとなったため、次の地域「黒水低湿地」
(第4地域、`docs/regions/blackwater_lowlands.md`)へ着手した。M6/M9の確立済み
パターン(地域骨格を1度作り、以後1地点ずつ本格化する)を踏襲し、今回のSliceは
RouteGraph骨格(7地点+3キャンプ)+地点1「灰水の沈み道」の実コンテンツだけに絞った。

事前調査の結果、地点1に必要な要素はほぼ全て既存機構で賄えた:

- **毒状態異常**(`StatusEffectType::Poison`)・**浅瀬地形**(`TerrainType::
  Shallows`、Ashbough Forestの薬草の沢由来)はいずれも既にフル実装済みで、
  今回は一切コード変更なし。地点1自身の敵(湿地の毒蜘蛛)は正本の行動記述に毒攻撃が
  無く(沼蛇の「毒噛み」は地点2以降)、`Weapon::onHitStatuses`(これも既存)も
  今回は未使用。深泥地形は地点1では表示演出としてのみ言及され機能要件ではないため、
  新規`TerrainType`の追加を見送り、必要になった地点のSliceへ先送りした。
- **副目標「右側の標識地点で行動終了」**は`ashroad_watch_fixture`と同型の
  `surveyObjectiveId`(`surveyTileCount`/`surveyTileObjectDefinitionId`は未設定、
  素のタイルのみ)で実装。
- **地点3「薬草洲」/4「樹脂林」の「両方必須・順不同」分岐**はCinderwatchの
  `ironwatch_stores`/`old_barracks`が使う`BranchCompletion::AllMembers`をそのまま
  流用(`src/core/RouteGraph.cpp`)。
- **湿地の毒蜘蛛**は新規`UnitClass`を作らず、Wolf相当のステータスを`enemyRoster`の
  `name`だけ差し替えて再利用した(Ashiron Quarryの「Rock Borer」がBanditを再利用
  した前例と同じパターン)。

新規`RegionId::BlackwaterLowlands`を追加し、`Region.cpp`(`regionDescriptor()`/
`toString()`/`regionIdFromString[Strict]()`/`regionCleared()`)・`RouteGraph.cpp`
(`blackwaterLowlandsGraph()`、`usesRouteGraph()`/`regionRouteGraph()`ディスパッチ)・
`ExpeditionService.cpp`(地域リスト・前地域マップ)を`AshironQuarry`と同じ形で配線
した。`jf_forest_balance --region=`は既に文字列ベースの汎用ルックアップ
(`regionIdFromStringStrict()`)のためコード変更不要だった。地点1「灰水の沈み道」の
探索3択は、既存の`cinderwatchOutcome()`デフォルト(rush=`partyDamage`2+
`enemiesRemoved`1、scout=自由配置左3列)が正本の数値とそのまま一致したため
`routeOutcomes`の上書きは不要で、`routeVictoryLootDelta`(薬草報酬なし/湿地樹脂+1)
だけで表現した。新素材`wetland_resin`(湿地樹脂)を`materialNameFor()`の`known`
セット+Localeへ追加。

地域/地点の日本語表示名(`RegionDescriptor::displayNameJa`/`StageDescriptor`の
`missionNameJa`)は`tr()`を経由しない直接埋め込み文字列のため、[[JA glyph coverage /
no ID-collision on JA text]]の慣習どおり`loadAppFont()`のcharsetSourceへ手動登録した
(`src/ui_shared.cpp`)。

地点2〜7はプレースホルダー(汎用Bandit/WatchArcher編成)のまま、M6-B/C方式で
1地点ずつ本格化する。

`jf_forest_balance --region=blackwater_lowlands`(500 Seed)の実測: 地点1
(灰水の沈み道)のfresh-party win率はDirect 100%/Tactical 100%。
[[jf_forest_balance worst-case numbers]]の教訓どおり、実測記録のみで数値調整は
行わない。

### M9-F 黒水低湿地: 地点2(葦原の分岐)

`docs/regions/blackwater_lowlands.md`「2. 葦原の分岐」を確認したところ、2つの
新規サブシステム(ルートごとの地形上書き「茂み4マスを通常床化」、敵配置の視認性
制御「敵配置非公開」)を要求すると判明。いずれも現行エンジンに存在せず、1地点の
ためだけに新設するのは過剰実装と判断し、相談の結果、敵数・報酬差分だけで近似する
方針で実装した(M9-Bが「落石予告」「鉱石箱喪失敗北」を見送ったのと同じ判断)。

主目的のOR条件(葦原出口を確保、または敵全滅後に出口で行動終了)は、Ashroad Watch/
Rubble Terraceが確立した`primarySecureTileAlternative`(既定`primary`グループを
`Any`へ広げ`SecureTile`を追加するパターン)をそのまま流用した。「敵全滅後」という
順序条件は、この既存パターン自体が元々厳密な順序を強制しない(ashroad_watchも同様)
ため、その近似を踏襲している。

敵編成は「葦を刈って見通す」ルート相当の5体(湿地の毒蜘蛛4+沼蛇1)を基本形とし、
他2ルートは`enemiesRemoved`(1/2)で減らす反転トリック(M9-Bで確立済み)を使用。
沼蛇は正本上「毒噛み優先」を持つが、この地点の主目的・勝敗に毒の有無が関わらない
ため、地点1の湿地の毒蜘蛛と同じ理由(新規`UnitClass`を避ける)でBanditのステータスを
名前だけ差し替えて再利用した(「Marsh Viper」)。毒攻撃武器(`Weapon::
onHitStatuses`)の実装は、それが主目的の一部になる地点まで見送っている。副目標
「古い道標を2個調査」は`surveyObjectiveId`+`surveyTileCount:2`(既存パターンの
2箇所版)で実装、報酬は正本の「湿地踏査記録」相当を単純な素材ボーナス(薬草+1)へ
近似した(重複防止台帳を持つ本格的なDiscovery機構はまだ無い)。

コード変更は無し(`primarySecureTileAlternative`/`surveyObjectiveId`/
`enemiesRemoved`/`routeVictoryLootDelta`という既存機構だけで表現できるため)。
新素材`poison_material`(毒素材)・敵表示名`character.marsh_viper`をLocaleへ追加。

`jf_forest_balance --region=blackwater_lowlands`(500 Seed)の実測: 地点2
(葦原の分岐)のfresh-party win率はDirect 99.2%/Tactical 98.4%。
`primarySecureTileAlternative`のSecureTile代替経路により、単純なDirect/Tactical
方策でも高いwin率になる傾向は既存地点(Rubble Terrace)と同じ。
[[jf_forest_balance worst-case numbers]]の教訓どおり、実測記録のみで数値調整は
行わない。

### M9-G 黒水低湿地: 地点3(薬草洲)

主目的「3ラウンド防衛、または敵全滅」を実装するため、`primaryHoldTileAlternative`/
`primarySecureTileAlternative`と同じ「既定`primary`グループをAnyへ広げ、代替
Objectiveを追加する」パターンを`primarySurviveRoundsAlternative`として新設した
(`include/jf/data/GameData.hpp`/`include/jf/core/Region.hpp`/`src/data/
GameData.cpp`/`src/core/Region.cpp`/`src/battle/BattleFactory.cpp`)。
`ObjectiveKind::SurviveRounds`自体は`battle.round() > target.surviveUntilRound`で
判定する既存実装(`ObjectiveTracker.cpp`)で、今回新設したのはOR合成の配線のみ。
`HoldTileMissionRule`はタイル/ゾーンの概念を持つため流用せず、`id`+
`surviveUntilRound`だけの新しい構造体にした。

副目標「採取者を撤退させない」+ 敗北条件「採取者の撤退」は調査の結果、
Cinderwatchの`old_barracks`「伝令兵脱出」・Ashiron Quarry地点3A/4と同じ
「プレイヤー操作外のNPCユニットを配置し生死を追跡する」ゲスト/護衛ユニット系
サブシステムの一種と判明した。`ObjectiveKind::ProtectUnit`はEnum+
Active→Failed追跡ロジックまで存在するが(1)そのFailedを実際の敗北条件へ結びつける
配線が無く、(2)そもそも「プレイヤー操作外の中立ユニット」を配置するTeam/生成手段
自体が存在しない(`Team`はPlayer/Enemyのみ、`BattleObjectTeam::Neutral`はUnitでは
なくObject用)。1地点のためだけに新しいUnit Team/中立ユニット配置の仕組みを作るのは
過剰実装と判断し、この副目標・敗北条件は見送った(地点3A/4/`old_barracks`と同じ
既知のギャップ)。

副目標「薬草地点2個を調査」は既存の`surveyObjectiveId`+`surveyTileCount:2`
(`sunken_path`/`reedway_fork`と同型)。「奥まで採取」ルートの2ラウンド目増援
(毒蜘蛛2体)は既存の`timedReinforcement`フィールド(Herbwater Hollowの狼増援と
同型)をそのままルート限定(`enableReinforcementWave`)で使用。`[暁の衛生兵]`ルート
+`marsh_emergency_medicine`Discoveryは`routeDiscoveries`(既存機構)で実装したが、
調査の結果「今回スキップしても地点7で代替取得できる」という重複防止つき代替取得
機構自体はAshiron Quarryの「採掘技術記録」と同じく実際にはコード化されておらず
(`RewardRule`の`RewardGrantId`重複防止は`region_mission_data_contract.md`側の
将来仕様として明記済みの未実装)、正本の記述は設計意図のプローズと判断し、地点3・
地点7それぞれで独立にDiscoveryを付与する(相互の重複防止ロジックは作らない)方針で
統一した。

`jf_forest_balance --region=blackwater_lowlands`(500 Seed)の実測: 地点3
(薬草洲)のfresh-party win率はDirect 100%/Tactical 99.8%。SurviveRounds/
EliminateTeamどちらの経路でも単純なDirect/Tactical方策が機能することを確認できた。
[[jf_forest_balance worst-case numbers]]の教訓どおり、実測記録のみで数値調整は
行わない。

### M9-H 黒水低湿地: 地点4(樹脂林)

`docs/regions/blackwater_lowlands.md`「4. 樹脂林」を確認したところ、主目的「樹脂箱
2個のうち1個以上を確保」自体は`surveyObjectiveId`+`surveyTileCount:2`が既に
`ObjectiveGroupRule::Any`(N個のうち1個で成功、`BattleFactory.cpp:554`、
`GameApp.cpp`の`surveySucceeded`判定コメントに明記)として実装済みと判明したが、
これは常に「勝利後のボーナス報酬」経路として配線されており、`EliminateTeam`なしでも
勝利できる主目的そのもの(OR代替)として使うには`groupId`を`"primary"`へ向ける新しい
配線が必要だった。正本の報酬表を確認したところ、「勝利: 湿地樹脂2、毒素材1」は
主目的達成時の通常報酬であり、クレート確保だけに紐づく追加報酬は無い(「罠2個処理」
「共同退路」の2つだけが個別報酬)。クレートのOR代替という主目的の形を実際に実装しても
報酬面での違いが無いと判断し、今回は標準`EliminateTeam`のみで近似し、この
OR性質(全滅なしでも1個確保だけで勝利できる)は見送った。

副目標「毒罠2個を解除または破壊」も、対応する「N個破壊で報酬」を汎用的に判定する
機構が無く(`logCollisionBonusLoot`/`noCasualtiesBonusLoot`はいずれも特定boss/状態
専用のチェック)、1地点のためだけに新しい報酬計算Hookを作るのは過剰実装と判断して
見送った(機能しないObjectを置くだけの意味がないため、罠Object自体も配置していない)。
副目標「採取者を撤退させない」+`[行軍隊長]`ルートの「回収者が中立護衛対象」は、M9-G
(地点3)で確認済みのゲストユニット系ギャップと同じ理由で見送り。敗北条件「樹脂箱を
両方失う」もObject破壊による敗北条件自体が既存に無い(M6-C/M9-C/M9-Dと同じ既知の
ギャップ)ため見送った。探索3択の敵編成差(「野生生物」vs「樹脂回収者」という敵種別の
差し替え)もルートごとに全く別の敵編成へ差し替える機構が無いため、地点1〜3と同じ湿地の
毒蜘蛛/沼蛇の使い回しで統一し、頭数差だけを`enemiesRemoved`で表現した。

結果として、このSliceは`data/regions.json`のコンテンツ追加のみで完結し
(M9-Fと同じ規模)、コード変更は一切無い。地点3「薬草洲」・地点4「樹脂林」の両方が
確保されたことで`AllMembers`分岐が解決し、Camp II以降へ進行可能になった。

`jf_forest_balance --region=blackwater_lowlands`(500 Seed)の実測: 地点4
(樹脂林)のfresh-party win率はDirect 98.6%/Tactical 93.6%。
[[jf_forest_balance worst-case numbers]]の教訓どおり、実測記録のみで数値調整は
行わない。

### M9-I 黒水低湿地: 地点5(黒水渡し)+ ゲストユニット護衛サブシステム新設

`docs/regions/blackwater_lowlands.md`「5. 黒水渡し」の主目的「荷運び役2人のうち1人
以上を右端へ脱出」自体が、これまで灰鉄採石場地点3A「旧採掘坑」・地点4「灰鉄鉱脈」・
Cinderwatchの`old_barracks`・黒水低湿地地点3/4で繰り返し意図的に見送ってきた
「ゲストユニット(プレイヤー操作外/一時加入NPC)」ギャップに依存していると判明した。
過去の地点ではこのギャップが副目標止まりだったため見送りで済んだが、地点5では主目的
そのものだったため、今回はこのSliceでサブシステムを新規実装した。

追加した型/関数: `Unit::isGuest`(表示専用フラグ。team/AI/選択ロジックは通常の
`Team::Player`ユニットと不変、常設ロースターには加わらない一時ユニットであることの
区別のみに使う)、`BattleMissionState::guestUnitIds`、`BattleState::allGuestsLost()`
(`allPlayersDefeated()`と同じ形。`evaluateBattleOutcome()`の敗北ゲートへ
`allPlayersDefeated()`の直後に追加し、`guestUnitIds`全員の`isPresent()`が`false`に
なった時点で部隊全滅とは独立に敗北とする)、`StageDescriptor::GuestUnitData`/
`guestUnits`(`BattleFactory.cpp`の`assembleScenario`が`Team::Player`・
`isGuest=true`で通常ユニットと同じ`units`ベクタへスポーンし、生成したidを
`guestUnitIds`へ登録)、`StageDescriptor::primaryEscapeUnitsAlternative`
(`primaryDefeatUnitId`と同じ「主目的を置換する、追加ではない」パターンで
`ObjectiveKind::EscapeUnits`を主目的化。脱出タイルの選定は既存`chooseHoldTile()`を
そのまま流用)。

`ObjectiveTracker.cpp`の`EscapeUnits`クレジット判定は、`requiredEscapeCount`到達で
Completed確定した後も`creditedTargetIds`への追記を続けるようガードを緩和した
(副目標「2人とも脱出」の判定に`creditedTargetIds.size()>=2`を使うために必要。
`SecureTile`側の判定・挙動は無変更)。

地点5の実コンテンツは`blackwaterCrossingStage()`として`Region.cpp`にC++直接記述
(`guestUnits`/`primaryEscapeUnitsAlternative`はJSONスキーマ未対応のため
`data/regions.json`側の旧プレースホルダーエントリは未使用のまま残置、`resin_grove`等
既存の「JSON側は最小のまま・実体はC++」パターンと同じ)。沼蛇=Bandit・毒蜘蛛=Wolf
再利用は地点1〜4の前例を踏襲。荷運び役2人は低戦闘ステータスの既存クラスを流用し
表示名のみ差し替え。荷物箱の副目標は地点4「樹脂林」と同じ`surveyObjectiveId`+
`SurveySuccess` RewardRuleパターンを1個用に流用。`GameApp::proceedToCamp()`へ
地点固有の副目標報酬分岐(2人脱出→高品質薬草1、荷物箱確保→毒素材1)を、既存の
`mergeLoot`ボーナスパターン(`logCollisionBonusLoot`等と同じ形)で追加した。

以下は該当インフラが無いため、M9-H等と同じ理由で意図的に見送った:
「荷物を減らして渡る」選択の持込品一時封印(持込品の戦闘限定封印/復元機構が無い)、
`[伝令騎兵]`ルートの護衛対象MOV+1・増援位置公開(ルート別のユニット別ステータス
修正/増援可視化機構が無い)。

`tests/test_battle.cpp`へ4件追加: 単体脱出(1人のみ)によるVictory成立(EliminateTeam
非依存で単独評価されることの確認)、ゲスト2人全滅による部隊生存下でのDefeat成立
(`allGuestsLost()`が部隊全滅ゲートと独立に効くことの確認)、両者脱出時の
高品質薬草1ボーナス、荷物箱確保時の毒素材1ボーナス。既存4テストスイート
(`jf_battle_tests`/`jf_locale_tests`/`jf_content_tests`/`check_localization`)含め
全成功。

`jf_forest_balance --region=blackwater_lowlands`(500 Seed)実測: Blackwater
Crossing(地点5)win率はDirect 50.6%・Tactical 50.6%で、他地点(93〜100%)より
大幅に低い。ただしHP残量はDirect 77.5%・Tactical 68.6%と高く、敗因はKOではなく
timeout(Direct 26/500、Tactical 132/500、Tactical側は`rounds`平均も12.09と他地点の
2〜3倍)に偏っている。`tools/forest_balance.cpp`の自動プレイAIは`ObjectiveKind`を
一切参照せず(grep 0件)、主目的が`EliminateTeam`である他地点を前提に敵殲滅だけを
行動基準にしていると見られる。地点5の主目的は`EscapeUnits`(脱出タイルへの到達)で
あり、この自動プレイAIには「脱出タイルへ荷運び役を誘導する」判断が組み込まれていない
ため、win率低下はゲームバランスの不具合ではなくbalanceツール側の未対応と考えられる。
[[jf_forest_balance worst-case numbers]]の教訓どおり実プレイでの確認が先であり、
現時点でこの数値を根拠にした地点5側の数値調整・balanceツール改修のどちらも行っていない。

### M9-J 黒水低湿地: 地点6(沈没水門)

`docs/regions/blackwater_lowlands.md`「6. 沈没水門」を確認したところ、主目的「水門を
操作し、2ラウンド防衛」のうち「水門を操作」部分は、signal_tower(灰道の監視所の
姉妹地点、2枚の信号盤)が既に証明済みの`objectPlacementRules`+`operateObjectiveId`
機構(`BattleFactory.cpp`が最初のOperateObject Objectiveを配置した時点でデフォルトの
`eliminate_enemies`primaryを置換し、`groupId: "primary"`のAll規則はそのまま維持する
「置換、拡張ではない」パターン)にそのまま収まると判明したため、水門の制御輪1個を
`sluice_gate_wheel` Deviceとして配置し、これ単体を主目的として`data/regions.json`へ
JSON実装した(他の兄弟地点と同じ経路、C++直接記述はしていない)。

以下は該当インフラが無いため見送った:

- 探索3択間の「操作2回」/「操作1回」の差: `ObjectiveKind::OperateObject`の
  Live評価は`interactionCount > 0`固定判定で閾値を持たず
  (`docs/implementation_status.md:63`)、3択とも同一のObject1個を共有し、回数差は
  数値としては機能しないフレーバー扱いとした
- ルート2「水位を一気に下げる」の「次Roundに敵味方の浅瀬4マスが深泥化」: 戦闘途中で
  地形を書き換える機構が無く、地点2の「地形上書き」見送りと同じ理由
- ルート3`[辺境工兵]`の「工具部品1消費」: ルート選択に紐づく消費アイテムコスト機構が
  無い。`FrontierEngineer`クラス自体はM7-2で実装済みのため
  `scoutRouteRequiredClass: FrontierEngineer`のクラスゲート自体は機能する
- 主目的の「水門操作」と「2ラウンド防衛」のAND結合: `primarySurviveRoundsAlternative`
  は常に`primary`グループをAny(OR)へ拡張したうえでデフォルトの`EliminateTeam`
  メンバーも残す(`BattleFactory.cpp`)ため、ここでは形が合わない(水門を操作した
  瞬間に敵が残っていても即勝利してしまう)。OperateObjectへ第2のKindをAllのまま
  追加する専用配線は存在しないため、1地点のために新規インフラを作らず主目的を
  OperateObjectのみで近似した(見送りを明記する、既存の判断方針どおり)
- 副目標「制御輪2個を保全」「毒罠3個を処理」: いずれもObject破壊・トラップ処理を
  汎用的に判定する機構が無い、M9-H地点4「樹脂林」の同形ギャップと同じ理由。トラップ
  Objectは機能しないため配置していない。紐づく`薬学記録`/`罠技術記録`の付与も
  対応する副目標が無いため見送った
- 敗北条件「水門本体の耐久0」: Object破壊による敗北条件自体が既存に無い
  (M6-C/M9-C/M9-Dと同じ既知のギャップ)

敵編成は罠師=Bandit、弓兵=WatchArcher、毒蜘蛛=Wolf(Marsh Poison Spider)の
名前のみ差し替えで、地点1〜5と同じ地域内再利用の慣例に従った。恒久成果
`sunken_sluice_restored`は他の全地点と同じ既存の一般機構(勝利+安全帰還で
`siteAccess::Secured`)がそのまま処理する。

結果として、このSliceは`data/regions.json`のコンテンツ追加のみで完結し
(M9-Fと同じ規模)、コード変更は`Region.cpp`のコメント更新のみで実質的なC++変更は
無い。`tests/test_battle.cpp`への新規テスト追加は無し(既存のOperateObject/
objectPlacementRulesの一般機構のテストでカバー済みと判断、signal_tower実装時と同じ
判断)。既存4テストスイート(`jf_battle_tests`/`jf_locale_tests`/`jf_content_tests`/
`check_localization`)含め全成功。

`jf_forest_balance --region=blackwater_lowlands`(500 Seed)の実測: 地点6
(沈没水門)のwin率はDirect 0.0%/Tactical 0.0%で、timeoutがDirect 327/500・
Tactical 405/500と大半を占める。`tools/forest_balance.cpp`の自動プレイAIは
`ObjectiveKind`を一切参照せず(grep 0件、M9-Iで確認済みの同じ制約)、
`OperateObject`をどのタイミングで実行すべきかという判断が組み込まれていないため、
制御輪へ一度も接触せずラウンド上限までさまよって引き分ける動きが大半と見られる。
これは地点5(黒水渡し、`EscapeUnits`)と同種のbalanceツール側の未対応であり、
[[jf_forest_balance worst-case numbers]]の教訓どおり実プレイでの確認が先であり、
現時点でこの数値を根拠にした地点6側の数値調整・balanceツール改修のどちらも行って
いない。

### M9-K 黒水低湿地: 地点7(深泥の水源)/ 地域ボス「沼牙の大蛇」/ 地域攻略

`docs/regions/blackwater_lowlands.md`「7. 深泥の水源」「地域ボス 沼牙の大蛇」および
「地域攻略と拠点接続」「最低保証報酬」を確認し、黒水低湿地の最終地点+地域ボス+
地域攻略を実装した。M9-Dの`takeGrubwormBossTurn()`を実装パターンの正本として踏襲。

新規`UnitClass::MarshFangSerpent`(`data/classes.json`、正本どおりHP60/STR9/DEF6/
RES3/MOV4、`serpent_fangs`武器)を追加し、`EnemyAI.cpp`へ`takeSerpentBossTurn()`を
新規実装。4つの行動:

- **毒牙**: 射程1・STR+3物理、命中時に未毒なら`applyPoison()`(既存機構、確定付与・
  重複なし)
- **水中潜行**: 既存の`chargeTelegraphed`/`bossRuntime.telegraph`/
  `chargeCooldownActions`(いずれもボア専用ではない汎用フィールド、M9-Dの前例どおり
  再利用)をそのまま流用し、`TelegraphShape::Area`+`lockedTiles`に移動先候補2マスを
  格納。実行時は先頭候補へ移動し隣接1体を攻撃する。「浅瀬が経路にない場合は使用
  しない」は、テレポート的な移動のため直線チャージのような経路走査機構が無く、
  「盤面のどこかにShallowsタイルが存在するか」への近似とした
- **締め付け**: `boarSweepTargets()`と同型の前方3マスパターン(STR+1、命中時に
  `moveDownActive`が未設定の対象だけへMove Down、重複なし)、隣接プレイヤー2体
  以上で発動
- **激しい身震い**: 新規`bool bossShudderUsed`(灰角大猪の`bossEnraged`/灰殻穿岩虫の
  `bossCollapseUsed`と同じ役割の1回限りフラグ)、HP50%以下で発火し、隣接4マスの
  ユニットへ既存`BattleState::applyKnockback()`(あらゆるノックバック源が使う共通
  機構)を適用

行動優先順位は正本の7項目を`takeGrubwormBossTurn()`と同じ「早期return連鎖」で実装。
ボス補正(毒1dmg/最大2回、炎上2dmg/最大2回、移動低下MOV-1、防御低下DEF-2、よろめき
次行動のみMOV-1、完全な行動不能無効)は既存の`isBoss`分岐する汎用関数
(`StatusEffect.hpp`の`statusPoisonDamage()`等)の数値がそのまま正本と一致していたため
無改修。ただし`Unit::isBoss`はAshenhornBoar/AshironGrubwormを含め現行のどのボスにも
`true`をセットする配線が存在しない(grep 0件の既存ギャップ、本Sliceの新規混入ではない)
- 3体とも実戦では非ボス補正のまま動いている。今回のスコープでは合わせて修正せず、
既知のギャップとして記録するに留めた。

以下は正本との差分・見送り(M9-Dの判断方針を踏襲、都度明記):

- 正本の主目的「沼牙の大蛇をHP0にして撤退させる AND 水源標識2個のうち1個以上で
  行動終了」というAND合成は、M9-Dが「封鎖杭2箇所」で下した判断と全く同じ理由
  (1地点のためだけの汎用AND合成機構を新設するのは過剰実装)で見送り、主目的は標準
  `EliminateTeam`のみとした。副目標「標識2個を両方確保」は既存の`surveyObjectiveId`
  +`surveyTileCount:2`+`surveyTileObjectDefinitionId`(`water_source_marker`)パターン
  (quarry_collapse_coreと同型)で近似した。「1個以上で行動終了」という主目的側の
  AND成分は、2個確保の達成が包含するため実質的に失われない
- `[辺境猟兵]`ルートの「潜行先1マスだけ表示」というルート限定のボスAI分岐は、
  route-conditional boss AI配線の前例がエンジンのどこにも無いため見送り、全ルート
  共通の2マス予告のままとした(`scoutRouteRequiredClass: FrontierRanger`のクラス
  ゲート自体は機能する)
- 「水源標識は押し出さず固定2ダメージ」「毒溜まりへ押し出しで毒付与」は、Object
  破壊・耐久値システム自体が存在しない既知のギャップ(M6-C/M9-C/M9-D/-Jと同じ)、
  および毒溜まりに相当する地形フレーバーが存在しない(Shallowsのみ)ため、双方とも
  「押し出されるが効果なし」に近似
- 敗北条件「水源標識2個破壊」「10ラウンド終了時に主目的未達成」は、Object破壊駆動の
  敗北条件・ラウンド上限敗北条件のどちらもエンジンに存在しない既知のギャップ
  (`Region.cpp`の`cinderwatchGateRegion()`コメントに明記済みの後者と同じ)
- 副目標「毒状態の味方0で戦闘終了」「薬草地点を使用せず勝利」は`GameApp.cpp`の
  `blackwater_crossing`と同じ直接チェック(ad-hocセカンダリボーナス)パターンで実装
  (`RewardRule::Condition`に該当する種類が無いため)。前者は
  `battle.units()`の毒状態走査、後者は既存`BattleState::collectedHerbPatches()`を
  そのまま使用

地域攻略・最低保証報酬はM6-Dの`cinderwatchMaterialsEarned`駆動フロアと完全に同じ形で
`blackwaterMaterialsEarned`(新規フィールド、`BaseState.hpp`/`SaveSystem.cpp`)を追加し、
`ExpeditionService.cpp`の`applyExpeditionReturnToBase()`へ同型のTop-upブロックを実装
(薬草8・高品質薬草1・毒素材4・湿地樹脂7のフロア、湿地踏査記録・薬学記録・罠技術
記録・緊急処置の4Discovery)。`blackwater_lowlands_secured`という安定IDそのものは
Ashiron Quarryの前例と同じく、コード上の実体は無く`RegionId::BlackwaterLowlands`が
`completedRegionIds`へ入ること(既存の`regionCleared()`/`computeWouldRegionBeCleared()`
汎用機構)がその実装である。診療所「薬学」(`pharmacology`)は新規`requiredDiscoveries:
{marsh_pharmacology_records}`を追加、工房へ新規`trapcraft`(罠技術)ノードを
`requiredDiscoveries: {marsh_trapcraft_records}`で追加(鉄杭・毒罠処理道具は正本の
どちらも具体的な`ItemType`を持たないため、既存の`pharmacology`の万能薬同様
`effectJa`のフレーバー記述のみ)。

風裂き高原(第5地域)は新規`RegionId::WindsweptHighland`+
`windswept_highland_outpost`(`data/regions.json`、Bandit2体の最小プレースホルダー)
で追加した。Ashiron Quarryが最初`ashiron_quarry_outpost`という同型の1地点だけの
プレースホルダーから始まった前例(M6-D)を完全に踏襲。`Region.cpp`の4箇所の
switch文・`regionUnlocked()`(BlackwaterLowlands完了で解放)・
`ExpeditionService.cpp`の`computeRegionSummaries()`(地域一覧・ロック済みTooltip)・
`SaveSystem.cpp`(`blackwaterMaterialsEarned`の保存/復元)を他地域と同じ形で配線した。

`tests/test_battle.cpp`へ5件追加: 毒牙(命中+毒付与)、締め付け(2体以上隣接での
発動条件・前方3マスのみ命中・Move Down重複なし)、激しい身震い(HP50%閾値・1回
限定・ノックバック)、地点7の勝利条件(HP0→ScriptedWithdrawal、`surveyObjectiveId`
設定確認)、風裂き高原の地域解放条件。既存4テストスイート
(`jf_battle_tests`/`jf_locale_tests`/`jf_content_tests`/`check_localization`)含め
全成功。

`jf_forest_balance --region=blackwater_lowlands`(500 Seed)の実測: Deep Mire
(深泥の水源)のfresh-party win率はDirect 99.8%/Tactical 99.2%(平均6〜8ラウンド)。
主目的が標準`EliminateTeam`のみ(M9-Dと同じ理由でDevice操作を含まない)のため、
本ツールの自動プレイAIがDevice操作やRound-limit判断を扱えないことによる0%張り付き
(地点5・6で観測済み)が発生せず、ボスAI自体は両ポリシーで機能することを確認できた。
7地点通しのRegion clear win率は0%(Direct/Tactical とも)だが、これは既に地点6
(沈没水門、`OperateObject`)側の`jf_forest_balance`未対応が原因と判明済み
(M9-J)であり、地点7自体のボス数値・AIが機能しないことを示すものではない。
[[jf_forest_balance worst-case numbers]]の教訓どおり、実測記録のみで数値調整は
行わない。

以上で黒水低湿地(第4地域)の全7地点が実コンテンツ化され、地域全体を安全帰還まで
攻略可能になった(地点6のOperateObject自動プレイ非対応を除き、実プレイでの
end-to-endクリアはエンジン機構としては揃っている)。風裂き高原(第5地域)は
選択可能な状態でBase画面に追加される。

### M9-L 風裂き高原(第5地域): 地域骨格 + 地点1(風下の登り口) + 強風地形

`docs/regions/windscar_plateau.md`を確認し、M6/M9の確立済みパターン(地域骨格を
1度作り、以後1地点ずつ本格化する)を踏襲して着手した。M9-Kが追加した
`RegionId::WindsweptHighland`+`windswept_highland_outpost`の1地点プレースホルダーを
土台に、本Sliceでスコープ全体(6地点+2キャンプ+地点3・4のどちらを先に攻略しても
よい分岐)+地点1「風下の登り口」の実コンテンツへ拡張した。

**命名の是正**: 正本の「内部地域IDは`windscar_plateau`」という明記に対し、M9-Kの
プレースホルダーは`RegionId::WindsweptHighland`/`"windswept_highland"`という暫定名の
ままだった。他の全地域(`CinderwatchGate`/`cinderwatch_gate`等)がenum名と
`toString()`文字列を1:1で一致させている既存規約に反していたため、本格実装の前に
`RegionId::WindscarPlateau`/`"windscar_plateau"`へ改名した(`BaseState.hpp`・
`Region.cpp`・`ExpeditionService.cpp`・`tests/test_battle.cpp`・`data/regions.json`)。
まだ1地点のプレースホルダーしか存在しない段階での改名のため、波及コストは最小。

**強風(新規地形機構)**: 正本の「実装順」1番目かつ地点1の探索ルート1「標準戦闘、
敵4体、強風は3Round目」が要求する機能のため、フレーバーとして見送らず実装した。
新規`TerrainType::WindGust`(移動コスト1、通行可能、それ自体に防御/回避ボーナスは
無い)を追加し、`BattleState`に戦闘ごと固定の`WindGustConfig{delta, triggerRound}`
(任意)を持たせ、`resolveWindGustRoundEnd()`という新規フリー関数で解決する。
呼び出し位置は`BattleController.cpp`の毒/炎上ステータスダメージ(
`processPhaseEndStatusEffects()`)と全く同じRound End地点 - 風で戦闘不能になった
ユニットが毒と同じ経路で撤退イベントに乗る。移動先が盤外・Unit・通行不能地形・
Objectで塞がれている場合は固定2ダメージ(正本どおり、既存`applyKnockback()`の
よろめきステータスとは意図的に別処理)。重装兵の重量装甲は`hasHeavyArmor()`
(`applyKnockback()`と同じ既存チェック)で移動もダメージも無効化する。地形生成は
新規`windscar_ascent`TerrainProfile(`data/terrain_profiles.json`、強風帯
`countBounds`min3/max6)で表現し、既存の重み付きランダム生成をそのまま流用した
(新規ジェネレータは不要)。

見送った部分(正本との差分、都度明記):

- 戦闘前の風向き・発生Round事前表示、発生1Round前の矢印テレグラフ - UI層の
  作業で、地点1の勝敗・数値には関わらないため後続Sliceへ先送り
- 分散配置(「実装順」2番目) - 地点1のルート2「2組の分散配置、敵3体」は
  `enemiesRemoved:1`の敵数差分だけで近似し、分散初期配置・合流経路保証の本格
  機構は未着手(既存機構が一切無い新規サブシステムのため、地点1単体のために
  新設するのは過剰実装、というM9-F/M9-Kと同じ判断)
- ルート2「織物-1」(消費型のルート効果コスト) - 消費アイテムコスト機構自体が
  存在しない既知のギャップ(M9-K自身のコメントが同じ理由を既に記録済み)
- ルート3「地形全公開」- エンジンにfog-of-war機構自体が無く常時全公開のため
  暗黙のno-op
- ルート3「強風帯2マス減少」- ルート単位の地形生成上書き機構が無い既知のギャップ
  (M9-Fの地形上書き見送りと同型)
- 「標識確保: 高原踏査進行」- 正本の安定ID表に無く、ナラティブラベルと判断して
  ループ/Discovery側の配線を追加しなかった

高原運び手(この地域の敵勢力)は新規`UnitClass`を作らず、Bandit4体を「Plateau
Runner」の表示名で再利用した(Blackwaterの「湿地の毒蜘蛛=Wolf」と同じ再利用
パターン)。副目標「登り口標識で行動終了」は`sunken_path_marker`と同型の
`surveyObjectiveId`(タイル数/Object指定なし)。勝利報酬(獣皮2、硬木1)・
斥候ルート報酬(織物1)は新規`victoryRewardRules`。新素材`hardwood`(硬木)・
`cloth`(織物)を`materialNameFor()`のknownセット+Localeへ追加(`material.hardwood`/
`material.cloth`)。地域/地点(全6地点)の日本語表示名・新規敵表示名
「高原運び手」・新規地形名「強風帯」は[[JA glyph coverage / no ID-collision on JA
text]]の慣習どおり`loadAppFont()`のcharsetSource+Locale双方へ登録した。

地点2〜6はプレースホルダー(汎用Bandit×2編成)のまま、M6-B/C方式で1地点ずつ
本格化する。地点3「風見台」/4「分断された輸送隊」の分岐は`RouteGraph.cpp`の
`windscarPlateauGraph()`でCinderwatch/Blackwaterと同じ`BranchCompletion::
AllMembers`を流用した。

`tests/test_battle.cpp`へ4件追加: 地域骨格(6地点+ルートグラフ+分岐の
`AllMembers`検証)、地点1の報酬・強風設定値、強風の押し出し/衝突ダメージ/重装兵
無効化を直接`BattleState`で検証。既存4テストスイート
(`jf_battle_tests`/`jf_locale_tests`/`jf_content_tests`/`check_localization`)含め
全成功。

`jf_forest_balance --region=windscar_plateau`(500 Seed)の実測: 地点1(風下の
登り口)のfresh-party win率はDirect 56.0%/HP残21.1%、Tactical 42.2%
(いずれも先行区域の地点1が軒並みDirect 99〜100%だったのと比べて明確に低い)。
Bandit4体の素の強さがBlackwaterの「湿地の毒蜘蛛」(Wolf再利用、より低ステータス)
より高いこと、および新規`windscar_ascent`TerrainProfileのBarrier比率(8%)・
強風の押し出し/衝突ダメージが合わさった結果と推測される。
[[jf_forest_balance worst-case numbers]]の教訓どおり実測記録のみに留め本Sliceでは
数値調整を行わないが、他地域の地点1と比べ明確に外れた値のため、次のSlice着手前に
実プレイでの確認とバランス調整を推奨する既知の要フォロー事項として記録する。

**[訂正・M9-Rで再検証済み]** 地点1のFrontalAdvanceルートは`routeOutcomes`上
`enemiesRemoved:0`(4体のまま)であり、これはツールの不具合ではなく地点1自身の
正本どおりのルート効果である。M9-Rで`jf_forest_balance`の別の不具合(Expedition
内の継続戦闘が`routeOutcomes`を適用していなかった点)を修正した後も、この
Direct 56.0%/Tactical 42.2%という数値は完全に不変だった。したがって上記の
「バランス調整を推奨」という所見は今も有効。詳細はM9-Rを参照。

**[追加調査・完了]** 上記「バランス調整を推奨」の前提(「先行区域の地点1が軒並み
Direct 99〜100%」)自体を検証したところ誤りだった。実測しなおすと、黒水低湿地
地点1(Wolf再利用、HP16/STR6/DEF2)は100.0%/100.0%だが、灰鉄採石場地点1
(Bandit・Bandit・WatchArcher・Spearmanの混成編成)はDirect 33.6%/Tactical
約35%で、正本の初出write-up(M9-A、「Direct 37%/Tactical 35%」)と一致する。
つまり既存の地点1のwin率は33.6%〜100%の幅があり、風裂き高原の56.0%/42.2%は
その範囲内に収まる、外れ値ではない数値だった。原因の内訳としては、Bandit
(HP22/STR9/DEF3)がWolf(HP16/STR6/DEF2)よりHP+37.5%/STR+50%/DEF+50%
明確に強く、これだけで大半のwin率差を説明できる。強風地形(`windscar_ascent`、
WindGust重み15・countBounds 3〜6/24マス)は最大2ダメージ・1回きりの対称効果
(敵味方問わず)であり、寄与は副次的と判断した。avg KO 3.05/4・timeout 2/500
(Direct)であり、行き詰まりではなく純粋な戦闘減耗によるもの。以上より、この
数値は地域ごとに新規メカニクス(強風)を試すという設計方針(「地域の役割」
節)の範囲内の想定内の難易度と判断し、**数値調整は行わない**。要フォロー事項
から除外する。

### M9-M 風裂き高原: 地点2(崩れた中継路)

`docs/regions/windscar_plateau.md`「2. 崩れた中継路」を本格実装した。M9-Lの
`windscarAscentStage()`と同じく`stageDescriptorFromContent()`+`data/regions.json`
ではなく`Region.cpp`内で手書きした - このSliceが要求する`StageDescriptor::
guestUnits`/`primaryEscapeUnitsAlternative`(M9-Iで黒水渡し向けに実装済みの
護衛サブシステム)がJSON Schemaに露出していないため、`blackwaterCrossingStage()`と
全く同じ理由・同じパターン。

**主目的のOR合成の近似**: 正本の主目的は「護衛対象を右端へ脱出、または敵全滅後に
橋を操作」という異なるObjective Kind同士のOR(EscapeUnits vs
EliminateTeam+OperateObject)で、これを合成する既存機構は無い(M9-D・M9-J
「地域ボス」「地点6」が自ら記録した「1地点のためにAND/OR合成インフラを新設しない」
判断と同型)。`primaryEscapeUnitsAlternative`(EscapeUnitsのみ)を主目的として
採用した。ルート1「吊り橋を一人ずつ渡る」だけが明示的に「護衛対象1人」を挙げており、
この地点の核が「人を守って渡す」であることが最も明確なため。「敵全滅後に橋を操作」の
代替経路は、そもそも橋Objectの耐久機構自体が未実装のため今回は見送った(下記参照)。

**護衛ユニットは全3ルート共通**: `StageDescriptor::guestUnits`はシナリオ構築時点で
固定され、選択した`ExplorationChoice`によって出し分けられない(M9-Iの既知の限界、
`blackwaterCrossingStage()`の`[伝令騎兵]`ルート自身のコメントが同じ制約を既に
記録済み)。ルート2「下の砕石道を進む」の正本テキストは「橋防衛なし」と明記するが、
この実装では護衛ユニット自体は3ルート共通で出現する(近似)。

**見送った部分(木橋Object耐久機構が丸ごと未実装、M6-C/M9-C/M9-D/M9-H/M9-J同型の
既知ギャップ)**:

- ルート3`[辺境工兵]`「橋索を補強する」の「木橋耐久+5」
- 副目標「木橋耐久を1以上残す」
- 敗北条件「木橋破壊後に代替路なし」
- 主目的の代替経路「敵全滅後に橋を操作」(操作対象のObjectが無い)

**見送らなかった部分**: ルート2「全員HP-2」は`ExplorationOutcome::partyDamage`
(既存フィールド、M9-D自身のコメントが記録済み)をそのまま流用した - 新規インフラ
不要。

敵は「槍兵2、弓兵2」の正本記述どおり、地点1の「高原運び手=Bandit再利用」パターンとは
変え、既存`UnitClass::Spearman`/`UnitClass::WatchArcher`をそのまま使用した(表示名も
未再スキン、Class名そのまま) - この2クラスは`loadAppFont()`のUnitClass一覧に
既に含まれているため追加のグリフ登録は不要。新素材`riding_gear`(騎具素材、勝利報酬)を
`materialNameFor()`のknownセット+`data/locales/{en,ja}.json`(`material.riding_gear`)へ
追加した - JA表示テキストは`loadAppFont()`の`allJapaneseGlyphText()`がLocale値を
自動収集する既存経路でカバーされるため、`charsetSource`の手動編集は不要だった
(地域/地点名は既にM9-L側が「崩れた中継路」まで含め手動リストへ登録済みで、
このSliceでの追加も無し)。キャンプIの解放自体はM9-Lが`RouteGraph.cpp`の
`windscarPlateauGraph()`で既に地点2後のノードとして配線済みのため、本Sliceでの
追加配線は不要だった。

`tests/test_battle.cpp`へ2件追加(`blackwater_crossing`の既存2テストと同型): 護衛
到達によるVictory単独成立、護衛全滅によるDefeat(部隊は無傷でも成立)を直接
`BattleState`で検証。既存4テストスイート含め全成功。

`jf_forest_balance --region=windscar_plateau`(500 Seed)の実測: 地点2(崩れた中継路)
のfresh-party win率はDirect 37.4%/HP残81.3%、Tactical 41.0%。ただしこの数値は
[[jf_forest_balance worst-case numbers]]・黒水渡し(地点5)自身の実測と同じ既知の
シミュレータ盲点を含む - 主目的がEscapeUnits(護衛対象の到達)であるにもかかわらず
このシミュレータは護衛対象を積極的に脱出させる行動を取らない(ヒューリスティックが
EliminateTeam前提のため)ため、win率が実プレイより低く出る構造的な偏りがある。
数値調整は行わない。

### M9-N 風裂き高原: 地点3(風見台)

`docs/regions/windscar_plateau.md`「3. 風見台」を本格実装した。この地点の主目的
「風見盤2個を操作」は`signal_tower`(信号塔下層、M6-C item2)が既に証明した
`objectPlacementRules`/`operateObjectiveId`の2-Object Schemaへそのまま収まる
(異なるObjective Kind同士の合成が要らない、`sunken_sluice`(M9-J)の単一Object版と
同型)ため、地点1・2とは異なり`Region.cpp`の手書きではなく
`stageDescriptorFromContent()`+`data/regions.json`のJSON側で完結させた
(`windwatch_panel_north`/`windwatch_panel_south`の2 Device、列0-3/4-7でゾーン分割、
`signal_tower`の`secondary_signal_panel`/`primary_signal_panel`と同じ形)。M9-K以来
プレースホルダーだった`windwatch_station`エントリ(`data/regions.json`、汎用Bandit×2)を
本コンテンツへ置き換えるだけで済み、`Region.cpp`側の`windscarPlateauRegion()`・
`RouteGraph.cpp`の配線は無改修。

敵は正本「弓兵2、槍兵2、伝令騎兵1」どおり既存`UnitClass::WatchArcher`/`Spearman`/
`MessengerCavalry`を再スキンなしでそのまま使用した(地点2のSpearman/WatchArcher
再利用パターンをそのまま踏襲、3クラスとも既に`loadAppFont()`のUnitClass一覧に
含まれているため追加グリフ登録は不要)。

**探索3択**: ルート1「正面階段を確保する」は無条件で敵5体・標準配置(routeOutcomes
上書き無し)。ルート2「二組で上下から入る」は、地点1のルート2「2組の分散配置」が
既に記録した「分散配置の本格機構は無い」判断(独自サブシステム新設は1地点のためには
過剰実装)をそのまま踏襲し、`enemiesRemoved:1`(5→4体)の敵数差分だけで近似した。
「風見盤2個を同時操作可能」は"simultaneous"を区別する機構自体が無く(`OperateObject`は
単に`interactionCount > 0`のLive評価で、Round内の同時性という概念が無い)、そもそも
2つの別Objectを別々のユニットが同時に操作すること自体は既存機構で元々可能なため、
このルート特有の効果としては暗黙のno-opとして扱った。ルート3`[監視弓兵]`「射線を
先に取る」は`scoutRouteRequiredClass = WatchArcher`で兵種ゲートのみ実装し、「稜線1マス
への自由配置」は見送った - `restrictedAutoSpawnMaxColumn`(薬草の沢/Ashironが
証明した既存機構)は列範囲でプレイヤー自動配置を絞れるだけで、地形種別(稜線)を
狙った配置は表現できない。地形種別に基づく配置ゾーニングの機構自体が存在しないため
(列ベースのゾーンのみ)、地点1ルート3の他の見送り項目と同型のギャップとして
no-opとした。「敵配置全公開」は地点1が既に記録したとおりfog-of-war自体が無いため
暗黙で常時真。

**見送った部分(既存の記録済みギャップと同型)**:

- OR条件「敵全滅後、残った風見盤を1個操作」: 異なるObjective Kind (EliminateTeam
  OR OperateObject)のOR合成インフラが無い、M9-D/M9-J/M9-Mが既に記録した同型の
  ギャップ。2 Objectの`operateObjectiveId`はいずれもデフォルトのAll(AND)primary
  groupへ加算されるだけで、代替経路は表現していない。
- 副目標「4ラウンド以内に両方操作」: `RewardRule::Condition`は`Always`/
  `RouteChoice`/`SurveySuccess`の3種のみで、Round数に基づく達成判定・報酬条件の
  機構が存在しない(`ObjectiveProgress`にも達成Roundを記録するタイムスタンプは無い)。
  一回限りの機構を1地点のために新設しない、という本プロジェクトの一貫した判断により
  見送り。従属する報酬`plateau_targeting_records`はこのSliceでは到達不能のため
  未配線(M9-Hの「到達不能な報酬は未宣言のまま残す」前例と同型)。
- 敗北条件「風見盤2個破壊」: Object耐久機構自体が丸ごと未実装、M6-C/M9-C/M9-D/
  M9-H/M9-J/M9-M以来記録済みの既知ギャップ。

`tests/test_battle.cpp`へ1件追加(`signal_tower`の2-Object OperateObjectテストと
同型): 敵全滅+片方の風見盤のみ操作ではVictoryが成立しないこと、両方操作して初めて
Victoryが成立することを直接`BattleState`で検証。地点3の敵構成(5体)・報酬
(硬木2/織物1)・`scoutRouteRequiredClass`も同テスト内でアサート。既存4テストスイート
含め全成功(`jf_content_tests`のRoute Graph到達可能性・3ルート全ての
`validateBattleMission()`検証も通過)。

`jf_forest_balance --region=windscar_plateau`(500 Seed)の実測: 地点3(風見台)の
fresh-party win率はDirect/Tactical共に0.0%(timeoutのみでKOなし)。これは
[[jf_forest_balance worst-case numbers]]・`sunken_sluice`(M9-J)自身の実測が既に
記録した既知のシミュレータ盲点そのもの - このシミュレータは`ObjectiveKind`を一切
認識せず、常にEliminateTeam前提のヒューリスティックで動くため、主目的が
OperateObjectのステージでは(敵を全滅させても勝利条件を満たさないため)必ずtimeoutで
敗北扱いになる。数値調整は行わない。

### M9-O 風裂き高原: 地点4(分断された輸送隊)

`docs/regions/windscar_plateau.md`「4. 分断された輸送隊」を本格実装した。
`windscarRelayStage()`(地点2)と同じく`Region.cpp`内で手書きした - この
Sliceが要求する`StageDescriptor::guestUnits`/`primaryEscapeUnitsAlternative`が
JSON Schemaに露出していないため、地点2・黒水渡しと全く同じ理由・同じパターン。

**主目的のOR合成の近似**: 正本の主目的は「負傷者1人以上を脱出、または荷物箱1個
以上を確保」で、異なるObjective Kind同士のOR(EscapeUnits vs SecureTile)。この
地点はルート1が負傷者2人を、ルート2が荷物箱2個を明示的に挙げており地点2の
荷運び役よりむしろ対称的だが、EscapeUnits(guest)を主目的として採用する判断は
M9-Mの前例をそのまま踏襲した - `primaryEscapeUnitsAlternative`+`guestUnits`は
黒水渡し・地点2で実証済みのend-to-end経路である一方、荷物箱側の
`surveyObjectiveId`はこのプロジェクトで常に「勝利へのボーナス報酬経路」として
のみ実証されており(`sunken_path_marker`/`blackwater_crossing_crate`等)、主目的
そのものとして機能させる配線(荷物箱確保単独で勝利させる、default primary
groupの置換)はまだどこにも存在しない。1地点のためにこの新しいprimary化を
新設するより実証済みのEscapeUnits経路を再利用する方が最小プラミングという
一貫した判断に合うため、こちらを採った。荷物箱自体はObject耐久機構が丸ごと
未実装(M6-C以来の既知ギャップ)のため今回は一切モデル化していない - ルート2の
「荷物箱2個」/副目標「荷物箱2個を保全」/敗北条件の荷物箱側/全保全報酬
`courier_route_chart`はすべて未配線のまま据え置いた(M9-Hの「到達不能な報酬は
未宣言のまま残す」前例と同型)。

**護衛ユニットは全3ルート共通**(M9-M/M9-Iの既知の限界の継続)。ルート2「荷車を
先に確保する」の正本テキストは負傷者を明示しないが、この実装では護衛ユニット
自体は3ルート共通で出現する(近似)。ルート2「防衛中に負傷者HP-3」は
`StageDescriptor::GuestUnitData`に開始前HPペナルティ用フィールドが無いため
(`partyDamage`はplayerParty専用)暗黙のno-opとして見送った。

**敵は既存クラスの再利用のみ**: 断崖の野盗(斧兵/弓兵/軽装剣士)に対応する
`UnitClass`(Axeman/LightSwordsman相当)は存在しない。弓兵は既存
`UnitClass::WatchArcher`をそのまま使い、斧兵・軽装剣士は新規Classを起こさず
`UnitClass::Bandit`を「Raider」表示名で再利用した - この表示名は
`data/regions.json`のsplit_convoyプレースホルダー自身が既に使っていた既存
ロケールキー(`character.raider`)のため追加のJAグリフ登録は不要。「騎兵ルート
では軽装剣士1追加」はenemyRosterへ5体目として常時含め、他2ルートで
`enemiesRemoved=1`により差し引く形で表現した(地点1の「敵4体→ルート2で敵3体」と
同じ加算後減算パターン)。ルート1「荷物報酬-1」はriding_gearを-1する
RewardRule::Condition::RouteChoiceルールで表現した(`computeStageVictoryLoot()`が
全ルールを合算し結果が正の分だけ残す既存機構をそのまま利用でき、M9-Lの
「織物-1」(消費型アイテムコスト)とは異なりこちらは単なる報酬減算のため新規
プラミング不要だった)。

**キャンプIIのゲート確認**: M9-Lが`RouteGraph.cpp`の`windscarPlateauGraph()`へ
既に配線済みの`windwatch_convoy_branch`(`BranchCompletion::AllMembers`、
メンバー`windwatch_station`/`split_convoy`)をそのまま確認し、地点3・4双方が
本格コンテンツになった今回も追加配線が不要であることをテストで検証した(下記)。

`tests/test_battle.cpp`へ5件追加: 護衛到達によるVictory単独成立、護衛全滅による
Defeat(部隊は無傷でも成立、地点2の2テストと同型)、騎兵ルートでの敵5体化検証、
ロケールキー・報酬ルール(FrontalAdvanceでriding_gear相殺)のアサート、
`windwatch_convoy_branch`が`AllMembers`のままであることの直接検証。既存4テスト
スイート含め全成功。

`jf_forest_balance --region=windscar_plateau`(500 Seed)の実測: 地点4(分断された
輸送隊)のfresh-party win率はDirect 67.0%/HP残83.4%、Tactical 65.2%。ただしこの
数値は[[jf_forest_balance worst-case numbers]]・地点2(崩れた中継路)自身の実測が
既に記録した既知のシミュレータ盲点を含む - 主目的がEscapeUnits(護衛対象の到達)
であるにもかかわらずこのシミュレータは護衛対象を積極的に脱出させる行動を
取らない(ヒューリスティックがEliminateTeam前提のため)ため、win率が実プレイより
低く出る構造的な偏り、および6地点通しExpeditionのReach数値(Split Convoy
0/500)が地点2以前の到達率の掛け算で説明できる縮小である点も同じ既知の性質。
数値調整は行わない。

### M9-P 風裂き高原: 地点5(断崖荷車道)

`docs/regions/windscar_plateau.md`「5. 断崖荷車道」を本格実装した。
`windscarConvoyStage()`(地点4)と同じく`Region.cpp`内で手書きした - このSliceが
要求する`StageDescriptor::guestUnits`/`primaryEscapeUnitsAlternative`が
JSON Schemaに露出していないため、これまでの3つのguest-escort地点と全く同じ
理由・同じパターン。

**荷車=Guestユニットとしてのモデル化**: 正本の「荷車耐久12」を、既存の
非戦闘Escortユニット(`GuestUnitData`)1体で近似した。荷車が右端タイルへ
到達すること自体は既存の`primaryEscapeUnitsAlternative`/`guestUnits`の
脱出判定と機能的に同一で、この地点はこれまでのどの地点よりも
EscapeUnitsとの一致度が高い(正本の主目的自体が「荷車または護衛対象1人以上」
というOR-of-サム、ルートに関わらず「何か」が到達すればよいため
`requiredEscapeCount=1`がまさにそのまま正しい)。ただし`UnitTemplate`に
HP上書きフィールドは無い(ステータスは`UnitClass`から決まる)ため、
「耐久12」という具体的な数値そのものは表現していない - 護衛クラス
(`DawnChirurgeon`再利用、既存の非戦闘Escortパターン)のベースHPで
近似するのみ。

**guestUnitsは全3ルート共通(固定)の既知の限界**: シナリオ構築時点で
固定されルート別に出し分けられない(M9-Iの既知の限界の継続)。正本は
ルート1/3が荷車1台、ルート2が護衛対象2人という異なる構成を明示するが、
このSliceでは荷車1体(Guest 1体)を全3ルート共通で採用した - 2/3ルートが
荷車を明示する多数派であり、`requiredEscapeCount=1`はどちらの構成でも
変わらないため。ルート2の「護衛対象2人」は未モデル化(1体のまま)として
記録する。

**ルート3`[重装兵]`「荷車への強風移動無効」は見送り**: `BattleState::
resolveWindGustRoundEnd()`のHeavyGuard免除判定(`hasHeavyArmor()`)は
`unit.unitClass`のみを見る汎用チェックで、プレイヤーか護衛かを区別しない
ため、荷車Guestの`UnitClass`を`HeavyInfantry`にすれば免除自体は
「タダで」発生することを確認した。しかし`HeavyInfantry`は実戦闘クラスで
あり、荷車という非戦闘物体に本物の重装歩兵ステータス(高STR/高DEF)を
与えるのは正本の意図と乖離する。`guestUnits`はルート別の出し分けも
できないため(上記の限界)、ルート3専用に別クラスを割り当てることも
できない。よって`windscarConvoyStage()`の「防衛中に負傷者HP-3」と同型の
暗黙のno-opとして見送り、コードコメントにのみ記録した(新しいギャップでは
なく、既存のguestUnits関連ギャップの一種)。

**敵は既存クラスの再利用のみ**: 正本の高原運び手(騎兵2/槍兵2/弓兵1)は
すべて既存`UnitClass`(`MessengerCavalry`/`Spearman`/`WatchArcher`)に
対応するため、`windscarRelayStage()`と同じく表示名の再利用なしで直接
使用した。ルート2「敵4体」はbase 5体ロースターから`enemiesRemoved=1`で
差し引く(`windscarAscentStage()`以来の加算後減算パターン)。ルート1
「騎具素材-1」ではなくルート2側の「騎具素材-1」を`RewardRule::
Condition::RouteChoice`で表現した(`computeStageVictoryLoot()`が全ルールを
合算し結果が正の分だけ残す既存機構、`split_convoy`と同じ新規プラミング
不要のパターン)。

**未実装(既知のギャップ、新規なし)**:
- 副目標「すべての荷物を保持」/「木橋を破壊されない」: Object耐久機構が
  丸ごと未実装(M6-C以来の既知ギャップ)。
- 敗北条件「全輸送対象の撤退」のうち荷物側: 上記と同じ理由で対象外 -
  護衛ユニット側は`allGuestsLost()`経由で自動配線される(実装済み)。
- 全荷物保持報酬`windscar_road_chart`: 副目標自体が未配線のため到達不能
  (M9-Hの「到達不能な報酬は未宣言のまま残す」前例と同型)。

`RouteGraph.cpp`の`windscarPlateauGraph()`は既にM9-Lが`cliff_cart_road`
ノードを配線済みで、このSliceでの追加配線は不要(コメントのみ「real
content」へ更新)。

`tests/test_battle.cpp`へ5件追加: 護衛(荷車)到達によるVictory単独成立、
護衛全滅によるDefeat(部隊は無傷でも成立)、ルート2での敵4体化検証、
ロケールキー・報酬ルール(CollapsedSidePathでriding_gear相殺)のアサート。
既存全テストスイート含め成功。

`jf_forest_balance --region=windscar_plateau`(500 Seed)の実測: 地点5
(断崖荷車道)のfresh-party win率はDirect 30.6%/HP残78.1%、Tactical
27.8%。ただしこの数値は[[jf_forest_balance worst-case numbers]]・地点2/4
自身の実測が既に記録した既知のシミュレータ盲点を含む - 主目的が
EscapeUnits(護衛対象=荷車の到達)であるにもかかわらずこのシミュレータは
護衛対象を積極的に脱出させる行動を取らない(ヒューリスティックが
EliminateTeam前提のため)ため、win率が実プレイより低く出る構造的な偏り。
数値調整は行わない。

### M9-Q 風裂き高原: 地点6(高原伝令所)/ 地域ボス「高原運び手の隊長」/ 地域攻略

`docs/regions/windscar_plateau.md`「6. 高原伝令所」「地域ボス 高原運び手の隊長」
「地域攻略と拠点接続」「最低保証報酬」を確認し、風裂き高原の最終地点+地域ボス+
地域攻略を実装した。M9-Kの`takeSerpentBossTurn()`を実装パターンの正本として踏襲。

新規`UnitClass::PlateauCourierCaptain`(`data/classes.json`、正本どおりHP40/STR9/
DEF6/RES4/MOV6、新規武器`road_sword`威力6射程1)を追加した。正本「兵種: 伝令騎兵型」
の言及どおりまず既存`MessengerCavalry`の再利用を検討したが、HP22/DEF4/RES3という
ベースクラスの数値がボスのHP40/DEF6/RES4と乖離しすぎるため、M9-D/M9-Kが自ボスで
下したのと同じ判断で新規Classを起こした。`EnemyAI.cpp`へ`takeCourierCaptainBossTurn()`
を新規実装。3つの固有行動:

- **通り抜け攻撃**: `computeReachableTiles()`から「隊長の現在位置と同じ行または列」
  かつ「移動距離2以上」かつ「対象へ隣接(射程1)」を満たすマスのうち最も低HPな対象を
  選び、`battle.moveUnit()`で直接そのマスへ移動して攻撃した後、対象から離れる方向へ
  最大2マス再移動する。正本の「ZoC・Unit・通行不能地形回避ルールを無視するが実際の
  通行可否は見る」は、`BattleState::moveUnit()`が元々「目的地の地形・Object・占有
  ユニットだけを見て経路上のZoC/Unitは一切見ない」という正本どおりの形をしている
  ため(沼牙の大蛇の水中潜行・灰殻穿岩虫のトンネルが同じ理由で直接
  `battle.moveUnit()`呼び出しを使っているのと同型)、新しい移動プリミティブは不要
  だった
- **迂回命令**: 戦闘中1回、`chargeTelegraphed`/`bossRuntime.telegraph`(いずれも
  ボア専用ではない汎用フィールド、M9-D/Kの前例どおり再利用)を使い、生存プレイヤーが
  多い方の半分の行(上下)を`TelegraphShape::Area`+`lockedTiles`で予告する。次のこの
  ボス自身の行動で解決: announced行に生存プレイヤーがいれば最も近い1体へ接近して
  攻撃、いなければ「通常AIへ戻り、無料の追加攻撃は発生しない」を文字どおり実装
  (turnを消費せず同じ行動内の後続の優先順位ステップへそのままフォールスルー)。
  正本の「騎兵1体と弓兵1体が...優先」は、このプロジェクトに他ユニットのAI判断へ
  影響する汎用のsquad間連携機構が一切無いため(全既存ボスの予告も自分自身の次行動
  にしか影響しない)、「隊長自身がannounced行のプレイヤーを優先する」へ縮小近似
  した - このSliceでの明示的な既知の縮小
- **退路確保**: HP50%以下で一度だけ(`bossShudderUsed`と同型の新規`bossEscapeRouteUsed`
  フラグ)、MOV+1/DEF-2を適用する。正本の「次の行動終了まで」は、他ボスのような
  ターンをまたぐ状態(`bossWeakenedFromStun`)ではなく、発動したその1行動の意思決定に
  だけ適用し関数を抜ける前に復帰する形で文字どおり実装した(「次の行動」=「発動した
  その行動自身」という最小解釈)。「伝令所から4マスを超えて離れない」という
  leash制約は、`plateau_relay`が`data/regions.json`側でJSON直書きのため戦闘に
  「伝令所」自身の盤上位置を運ぶ専用機構が存在しない(M9-K自身が沼牙の大蛇の
  「水源から3マス以内」leashについて記録した既知のギャップと全く同じ形)。1つの
  ボス能力のためだけに汎用Object位置プラミングを新設するのではなく、この地点の
  固定レイアウトが伝令所を置く想定の固定タイル(`kPlateauRelayStationTile`)を
  `EnemyAI.cpp`内に直接ハードコードする形で実装した - leash自体は機能する
  (退路確保が有効な間、移動先候補をそのタイルから距離4以内へ絞る)が、汎用Object
  位置トラッキング機構としては存在しない

行動優先順位は正本の6項目のうち、手順3「伝令箱へ到達可能な味方伝令を妨害」を除く
5項目を`takeGrubwormBossTurn()`/`takeSerpentBossTurn()`と同じ早期return連鎖で実装。
手順3は主目的の近似判断(下記)によりguest伝令ユニット自体がこのSliceに存在しないため
スコープ外とした。手順6「伝令所から3マス以内で側面位置を取る」も同じ
`kPlateauRelayStationTile`を目標に最近接マスへ寄る形で実装した。

以下は正本との差分・見送り(M9-D/M9-Kの判断方針を踏襲、都度明記):

- 正本の主目的「隊長を戦闘不能にして撤退させる OR 味方伝令を伝令箱へ到達させ
  伝令所を2ラウンド維持する」というOR合成は、M9-D/M9-J/M9-Mが繰り返し下した
  判断と同じ理由(1地点のためだけの汎用OR合成機構を新設するのは過剰実装)で
  見送り、主目的は標準`EliminateTeam`のみとした。ボスの撤退(HP0)は
  `ObjectiveTracker.cpp`の`emitUnitDefeatedEvents()`へ`PlateauCourierCaptain`を
  追加し、沼牙の大蛇等と同じ`UnitExitReason::ScriptedWithdrawal`扱いにした
- 探索ルート2「先に伝令を走らせる」の条件`windscar_road_chart`はDiscovery条件で
  あり、`StageContentData`/`StageDescriptor`のルートゲート機構は
  `scoutRouteRequiredClass`(UnitClass限定)と`scoutRouteDisabled`(3番目のルート
  専用の無効化フラグ)しか存在せず、Discoveryゲート・2番目のルート(CollapsedSidePath)
  向けの無効化のどちらも表現する機構が無い(このSlice時点でプロジェクト全体を
  通じて前例が無い新種のゲート形)。正本自身、この`windscar_road_chart`が
  地点1〜5のどこでも「保全報酬が未配線のため到達不能」(M9-H/M9-Pの記録参照)な
  ままだったため、実際にはこの時点で誰も所持できないルートでもある。よって
  ルート2は常時選択可能な無条件ルートとして近似し(ゲート自体は掛からない)、
  「味方伝令1人を追加」は主目的近似同様guestユニット自体を配線しないため
  no-opとした。「敵増援+1」だけは、`plateau_relay`のenemyRoster末尾に7人目
  (`plateau_relay_swordsman1`)を常時含めておき、ルート1・3側で
  `routeOutcomes`の`enemiesRemoved:1`により差し引く(`windscarConvoyStage()`
  以来の加算後減算パターン)形で表現した。「先行伝令ルートでは3ラウンド目に
  軽装剣士1増援」という正本のタイミング差分は、この加算後減算近似では
  即時出現になる(`timedReinforcement`は使わなかった)ことを既知の簡略化として
  記録する
- 探索ルート3`[行軍隊長]`「共同運用を提案」は`scoutRouteRequiredClass:
  MarchCaptain`で兵種ゲート自体は機能する。「敵2人がHP50%以下で撤退可能」は、
  `jf/battle/AiSystem.hpp`の`AiProfile::retreatHpPercent`(既存の汎用撤退閾値、
  デフォルト25%)が正本の「HP30%以下で撤退を評価」という高原運び手の敵勢力設定
  と機能的にほぼ同じ挙動を既に持っており、しかも`profileFor()`はUnitClass単位の
  グローバルな分岐(この地点・このルートに限定した閾値上書きの機構が無い)のため、
  ルート3固有の効果としての新規配線はしていない - 既存の汎用撤退閾値がこの地点
  でも既に(近似値で)機能しているとみなし、記録に留めた。「伝令所耐久+4」は
  Object耐久機構自体が丸ごと未実装(M6-C以来の既知ギャップ)のため見送り
- 副目標「伝令所耐久を8以上残す」(騎兵運用記録の従属報酬含む)はObject耐久機構
  自体が無いため丸ごと見送り(同上の既知ギャップ)
- 副目標「高原運び手を2人以上撤退・降伏させる」は、上記の既存汎用撤退機構
  (`EnemyAI.cpp`の`takeEnemyTurn()`が`AiCandidate::Retreat`経路で
  `exitReason = UnitExitReason::Retreated`を設定する既存配線)をそのまま利用し、
  `GameApp.cpp`に`blackwater_crossing`/`deep_mire`と同型のad-hocセカンダリ
  ボーナスとして実装した(`RewardRule::Condition`に該当する種類が無いため) -
  戦闘終了時に`Team::Enemy`かつ`exitReason==Retreated`のUnit数を数え、2以上かつ
  `windscar_road_chart`未取得なら`pendingDiscoveries`へ追加する
- 副目標「味方戦闘不能者0」は既存`noCasualtiesBonusLoot`(M9-D/M6以来の実証済み
  機構)をそのまま`plateau_relay`エントリへ宣言するだけで済んだ(新規コード不要)

地域攻略・最低保証報酬はM9-Kの`blackwaterMaterialsEarned`と完全に同じ形で
`windscarMaterialsEarned`(新規フィールド、`BaseState.hpp`/`SaveSystem.cpp`)を
追加し、`ExpeditionService.cpp`の`applyExpeditionReturnToBase()`へ同型のTop-up
ブロックを実装(獣皮5・織物7・硬木5・騎具素材4のフロア、高原街道図・騎兵運用
記録・標的指定記録・伝令路図の4Discovery、いずれも新規`k*Discovery`定数を
`BaseState.hpp`へ追加)。`windscar_plateau_secured`という安定IDそのものは
Ashiron Quarry/Blackwater Lowlandsの前例と同じく、コード上の実体は無く
`RegionId::WindscarPlateau`が`completedRegionIds`へ入ること(既存の
`regionCleared()`/`computeWouldRegionBeCleared()`汎用機構)がその実装である。

正本「解放効果は装備、スキル、情報、護衛支援へ限定する」の各項目について:
`plateau_targeting_records`で解放するとされる監視弓兵「標的指定」
(`mark_target`)、`courier_route_chart`で解放するとされる辺境斥候「緊急離脱」
(`emergency_withdrawal`)は、`Skill.cpp`の`SkillDefinition`にDiscovery条件で
スキルをゲートする機構自体が存在しない(全スキルは兵種+階層でのみ決まる)ため
既知のギャップとして見送った。騎兵運用記録の「機動訓練の上位候補と騎兵装備
レシピ」・騎具素材の「伝令騎兵の武器分岐」も同型(訓練所候補・武器分岐という
機構自体が未実装、`docs/implementation_roadmap.md`側の既存スコープ)。高原街道図
の「輸送情報と次地域の経路表示」はUI層のフレーバーで、地域攻略自体には影響しない
(正本も明記)ため対象外。

旧辺境集落(第6地域)は新規`RegionId::OldFrontierSettlement`+
`old_frontier_settlement_outpost`(`data/regions.json`、Bandit2体の最小
プレースホルダー)で追加した。Windscar Plateauが最初`windswept_highland_outpost`
という同型の1地点だけのプレースホルダーから始まった前例(M9-K)を完全に踏襲
(RouteGraphは使わない単一地点構成、`usesRouteGraph()`は無改修)。`Region.cpp`の
4箇所のswitch文・`regionUnlocked()`(WindscarPlateau完了で解放)・
`ExpeditionService.cpp`の地域リスト・`unitClassFromString()`(`GameData.cpp`)を
他地域と同じ形で配線した。

`tests/test_battle.cpp`へ6件追加: 通り抜け攻撃(2マス以上の直線移動を要求し
対象から離れて再移動)、退路確保(HP50%閾値・1回限定・同一行動内でMOV/DEF復帰)、
迂回命令(予告→次行動で解決、対象不在時のフォールスルー)、地点6の勝利条件
(HP0→ScriptedWithdrawal、`scoutRouteRequiredClass`/`noCasualtiesBonusLoot`
アサート)、旧辺境集落の地域解放条件。既存4テストスイート
(`jf_battle_tests`/`jf_locale_tests`/`jf_content_tests`/`check_localization`)
含め全成功。

`jf_forest_balance --region=windscar_plateau`(500 Seed、`500 --region=windscar_plateau`
という正しい引数順で実行 - `--seeds=`という引数形式は本ツールに存在せず誤って
未知のClass除外パーサへ吸われることを確認した上で修正)の実測: 地点6(高原伝令所)の
fresh-party win率はDirect/Tactical共に0.0%(avg KO 4.00、毎試行で部隊全滅)。
これは地点1〜5の既知のシミュレータ盲点(OperateObject/EscapeUnits主目的への
非対応)とは異なり、地点6は本Sliceの近似どおり純粋な`EliminateTeam`であるため、
シミュレータ側の既知の非対応では説明できない実測である。7体編成(隊長+伝令騎兵+
槍兵2+弓兵2、ルートによっては+1)がこのツールの想定する「新鮮な3人パーティ」に
対して単体でも相当に重いことが主因と推測されるが、[[jf_forest_balance worst-case
numbers]]の教訓どおりこれは後退なしの単一地点最悪ケース測定であり実プレイでの
確認に基づかない数値調整は行わない。ただし地点1(Downwind Ascent)がM9-L記録の
Direct 56.0%と一致した一方この地点だけ0.0%/全滅という際立った落差のため、次の
Slice着手前に実プレイでの確認とバランス調整を推奨する既知の要フォロー事項として
明記する。6地点通しのRegion clear win率は0%(Direct/Tacticalとも)だが、これは
既に地点3(Windwatch Station、`OperateObject`)側の`jf_forest_balance`未対応が
主要因と判明済み(M9-N)であり、地点6自体のボス数値・AIが機能しないことを
示すものではない(Reach: Plateau Relay Stationへの到達自体が0/500)。

**[訂正・M9-Rで再検証済み]** 上記の「地点6は0.0%」という実測は`jf_forest_balance`が
`plateau_relay`の`routeOutcomes`(FrontalAdvance: `enemiesRemoved:1`)を単発計測
(fresh-party per site)では既に正しく適用していたことがM9-Rで確認された - 表内の
「7体編成」という記述自体、実際には`enemiesRemoved:1`差し引き後の6体(隊長+5)を
指しており、当時の記述は誤解を招く書き方だったが数値自体は誤っていなかった。
M9-Rはこの計測ツールが持っていた別の実在バグ(6地点通しExpedition内の2地点目
以降の継続戦闘が`routeOutcomes`を一切適用していなかった)を修正したが、風裂き
高原のReachは地点6よりずっと手前(地点3のOperateObject非対応)で頭打ちのため、
この修正によって地点6の数値自体は変化しなかった。したがって地点6の0.0%は
ツールの不具合による過小評価ではなく、引き続き実プレイでの確認とバランス調整を
推奨する既知の要フォロー事項として有効である。詳細はM9-Rを参照。

以上で風裂き高原(第5地域)の全6地点が実コンテンツ化され、地域全体を安全帰還まで
攻略可能になった(地点3のOperateObject自動プレイ非対応を除き、実プレイでの
end-to-endクリアはエンジン機構としては揃っている)。旧辺境集落(第6地域)は
選択可能な状態でBase画面に追加される。

### M9-R jf_forest_balance: Expedition内継続戦闘へのroute効果未適用バグ修正

風裂き高原・地点6(高原伝令所)ボス戦の実測win率0.0%が「`jf_forest_balance`が
`StageDescriptor::routeOutcomes`/`ExplorationOutcome::enemiesRemoved`を一切
適用せず常に未加工の`enemyRoster`と戦っているのではないか」という疑いから
再調査した。

**調査結果**: 疑い自体は半分だけ正しかった。`tools/forest_balance.cpp`の
fresh-party単発計測ループ(各地点を`[Direct]`/`[Tactical]`の表として出力する
既存の主要な計測経路)は`createScenarioBattle()`へ`stageRouteOutcome(stage,
ExplorationChoice::FrontalAdvance)`を毎回明示的に渡しており、`routeOutcomes`/
`enemiesRemoved`は元から正しく適用されていた(`plateau_relay`のFrontalAdvance
ルートは`enemiesRemoved:1`のため、実際には隊長+5の6体編成で計測されている -
`buildEnemies()`を直接インスツルメントして`enemyCount=6`を確認した)。M9-L/M9-Qが
記録した地点1(Downwind Ascent)Direct 56.0%/Tactical 42.2%、地点6(Plateau Relay
Station)Direct/Tactical共に0.0%は、いずれもこの正しく機能していた経路の実測であり
不具合の産物ではない。

一方、実在のバグは別の場所にあった: 同ファイルの6地点通しExpedition計測ループ
(`Reach:`/`Region clear`を出力する経路)は、地点1(`stageIndex==0`)のみ
`stageRouteOutcome()`を渡し、地点2以降の継続戦闘(`createScenarioContinuationBattle()`)
へは`outcome`引数を一切渡していなかった - `BattleFactory.hpp`のデフォルト引数
`ExplorationOutcome outcome = {}`が暗黙に適用され、`enemiesRemoved`を含む全ての
route効果が2地点目以降で常に無視されていた。

**修正**: `tools/forest_balance.cpp`のExpeditionループにおける
`createScenarioContinuationBattle()`呼び出しへ、fresh-party単発計測ループと同じ
`stageRouteOutcome(forest.stages[stageIndex], ExplorationChoice::FrontalAdvance)`を
追加しただけの1行差分(サロゲートのFrontalAdvance固定は既存のfresh-party計測
ループ自身の慣習をそのまま踏襲、新しいルート選択ロジックは導入していない)。

**再計測**(500 Seed、`--region=windscar_plateau`): 地点1・地点6ともfresh-party
win率は修正前後で完全に不変(地点1: Direct 56.0%/Tactical 42.2%、地点6: Direct/
Tactical共に0.0%)。Expedition側の`Reach`/`Region clear`も見た目上は不変だったが、
これは風裂き高原の6地点通し到達率が既に地点3(Windwatch Station、`OperateObject`
未対応)で頭打ちになっており、修正対象だった地点2以降の継続戦闘のroute効果が
そもそも到達可能な範囲を超えて評価される機会が無かったため(Reach:地点4以降は
修正前後とも0/500)。

| | 修正前 | 修正後 |
|---|---|---|
| 地点1(Downwind Ascent)Direct win | 56.0% | 56.0%(不変) |
| 地点1(Downwind Ascent)Tactical win | 42.2% | 42.2%(不変) |
| 地点6(Plateau Relay Station)Direct win | 0.0% | 0.0%(不変) |
| 地点6(Plateau Relay Station)Tactical win | 0.0% | 0.0%(不変) |

他地域での健全な既存数値についても回帰が無いことを確認した:
Ashiron Quarry地域ボス(Collapse Core)Direct 98.8%/Tactical 89.8%、Blackwater
Lowlands地域ボス(Deep Mire)Direct 99.8%/Tactical 99.2%、Cinderwatch Gateの
全地点数値、いずれも修正前後で完全に一致。`ctest`の既存4スイート
(`jf_battle_tests`/`jf_locale_tests`/`jf_content_tests`/`check_localization`)は
全成功。

**結論**: 地点6ボス戦の0.0%は`jf_forest_balance`の不具合による過小評価では
なかった - fresh-party計測経路は元から`routeOutcomes`を正しく適用していた。
M9-L/M9-Qが既に記録した「実プレイでの確認とバランス調整を推奨する」という所見は
そのまま有効であり、本Sliceは計測ツール自体の別の(数値に影響しない)不具合を
修正したに留まる。数値調整は行わない。

### M9-S 風裂き高原: 地点6(高原伝令所)バランス調整(保守的)

M9-Rで確認された地点6のfresh-party win率0.0%(avg KO≈4.00, HP≈0%、全滅)に対し、
実プレイ相当の保守的なバランス調整を行った。比較対象の稼働中2地域ボス、Ashiron
QuarryのCollapse Core(grubworm+borer2体、win Direct 98.8%/Tactical 89.8%)と
Blackwater LowlandsのDeep Mire(serpent+spider3体、FrontalAdvanceで
`enemiesRemoved:1`適用後はserpent+spider2体、win Direct 99.8%/Tactical 99.2%)は
いずれも実戦闘時点でボス+2編成であるのに対し、地点6は`enemiesRemoved:1`適用後も
隊長+伝令騎兵+槍兵2+弓兵2の隊長+5編成のままだった。

まず`takeCourierCaptainBossTurn()`を`takeGrubwormBossTurn()`/
`takeSerpentBossTurn()`と比較したところ、構造的な差分を確認した: 他2ボスの
固有の突進/潜行行動はいずれも予告(telegraph)またはcooldownで頻度が絞られている
一方、隊長の「通り抜け攻撃」(対象へ隣接して攻撃した後、反撃の届かない位置へ
再移動する)はcooldownが一切無く、毎ターン無条件に発動できた - 事実上、毎ラウンド
反撃を受けない一方的な追加ダメージを与え続ける構造的優位であり、これがボス+5と
いう大きめの編成と相乗して地点1〜5と比べ際立って重い0.0%を生んでいたと判断した。

対応方針(小さい変更から順に検証、目標は40〜70%程度の妥当な範囲への改善、
他2ボスの90%台への一致は狙わない):
1. まず`data/regions.json`の`plateau_relay`エントリのみ(グローバルな`Spearman`/
   `WatchArcher`/`MessengerCavalry`クラスの基礎値は一切変更していない)敵数を
   隊長+4(弓兵2→1へ削減)へ絞って再計測 → Direct/Tactical共にほぼ変化なし
   (win 0.0%/0.2%)。編成数だけでは説明できない要因があると判明。
2. `PlateauCourierCaptain`のDEFを6→5へ試験的に下げて再計測(隊長専用クラスの
   ため他地点への影響は無い)→ こちらもほぼ変化なし(0.0%/0.2%)。数値上の
   deviationを正当化する効果が無かったため直ちに元のDEF6へ差し戻した -
   **正本の確定数値からの逸脱は行っていない**
3. 上記2つがいずれも不十分だったため、上で特定した構造的AI優位に対応する
   最小限の変更として、`EnemyAI.cpp`の「通り抜け攻撃」に他ボスと同型の
   1ターンcooldownを追加した(既存の汎用`chargeCooldownActions`フィールドを
   再利用、新規フィールド不要)。攻撃自体のダメージ・対象選択・再移動ロジックは
   一切変更していない、発動頻度のみを絞る変更。これも単独では効果が薄かった
   (0.0%/0.2%のまま)ため、(1)の敵数調整をさらに一段階進め、`plateau_relay`の
   編成を隊長+伝令騎兵+槍兵1(+ルート差分の襲撃剣士1)の隊長+2編成へ縮小し、
   cooldown修正と併用して再計測した。

**結果**(500 Seed、`jf_forest_balance 500 --region=windscar_plateau`):

| | 調整前 | 調整後 |
|---|---|---|
| 地点6 Direct win | 0.0% | 66.6% |
| 地点6 Tactical win | 0.0% | 58.4% |

40〜70%の目標範囲に収まったため、これ以上の反復調整は行わなかった(過剰な
チューニングを避けるという明示の方針どおり)。6地点通しのRegion clear/Reach
数値は従前どおり地点3(Windwatch Station、`OperateObject`未対応)で頭打ちのため
不変。

**正本との関係**: 最終的に適用したのは(1)`plateau_relay`の敵数削減
(隊長+5→隊長+2、`enemyRoster`配列からの2体削除)と(3)「通り抜け攻撃」への
1ターンcooldown追加の2点のみ。`PlateauCourierCaptain`のHP40/STR9/DEF6/RES4/MOV6
という正本の確定数値(`docs/regions/windscar_plateau.md`)は最終的に**一切変更
していない**(上記(2)のDEF試験は効果測定後に元へ差し戻し済み)。敵数調整は
M9-Q自身の結び「以後の敵数、風発生Round、報酬量は実戦テストに基づくバランス
調整として扱う」で明示的に許可されたレンジ内の変更である。一方、cooldown追加は
「固有行動」の挙動そのものへの変更のため、上記の結び文が明示的にカバーする
範囲(敵数・タイミング)を厳密には超える - ただし変更したのは発動頻度のみで、
ダメージ量・対象選択・移動先ロジックは正本記載どおりのまま維持しており、影響は
最小限に留めた。`tests/test_battle.cpp`の地点6関連アサーション(通り抜け攻撃・
退路確保・迂回命令・勝利条件)はいずれも敵数やcooldown回数を直接アサートして
おらず、既存4テストスイート(`jf_battle_tests`/`jf_locale_tests`/
`jf_content_tests`/`check_localization`)は全成功のまま(回帰無し)。

### M9-T 灰鉄採石場: 地点3A(旧採掘坑)/ 地点4(灰鉄鉱脈) - 地域完全化

M9-A/-D以来「ゲスト護衛サブシステムが未実装」を理由に据え置かれていた地点3A・4を、
M9-Iで新設・M9-K/-L/-O/-P/-Q/-Sで実証済みのguestUnits/primaryEscapeUnitsAlternative
を使って実装し、灰鉄採石場を全5地点+ボス+3A/3B双方実装済みへ完全化した。
`quarryOldMineStage()`/`ashironVeinStage()`として`blackwaterCrossingStage()`と同型に
`Region.cpp`へ手書きした(guestUnits/primaryEscapeUnitsAlternativeがJSON Schema未対応
のため、旧`quarry_old_mine`/`ashiron_vein`のJSONエントリはdead dataとして残置)。

**地点3A(旧採掘坑)**: 主目的は`primaryEscapeUnitsAlternative`(作業員1人以上を右端へ
脱出、requiredEscapeCount=1)- blackwater_crossingと完全に同型のクリーンな再利用。
穿岩獣=Bandit(M9-D「Rock Borer」前例)3体+回収団弓兵=WatchArcher1体、2ラウンド目に
穿岩獣1体の予告増援(TimedReinforcement)。ルート3`[古参守備兵]`「味方初期配置は左2列」
は`ExplorationOutcome::restrictedAutoSpawnMaxColumn`(Herbwater Hollowの衛生兵ルートが
既に証明済み)で実装 - 「配置ゾーン機構が既存に無いか」という懸念は杞憂で、既存
フィールドがそのまま使えた。副目標「作業員2人とも脱出」→採掘技術記録は
`GameApp::proceedToCamp()`の`creditedTargetIds.size()>=2`アドホックチェック
(blackwater_crossingの「2人とも脱出」と同型、Loot代わりにDiscoveryを付与する点だけ
異なる)。

**地点4(灰鉄鉱脈)**: 主目的AND(測定N箇所操作、イリエンを左側退路へ脱出)は
M9-D/-J/-M/-O/-Qが繰り返し見送った「異なるKind同士のAND合成」ギャップのため、
`primaryEscapeUnitsAlternative`(イリエンの脱出)を実際の主目的として採用し、
OperateObject側(測定N箇所)は丸ごと未配線のまま据え置いた。「左側退路」実装で
`PrimaryEscapeUnitsRule::zoneMinCol/zoneMaxCol`を初めて右端以外(左側)へ設定したところ、
`BattleFactory.cpp`の`chooseHoldTile()`に実在のバグを発見・修正した: WatchPost地形が
無いプロファイル(ash_road含む全非Cinderwatchプロファイル)ではリクエストされた
zoneMinCol/zoneMaxColを完全に無視して`chooseSurveyTile()`の固定右端ゾーンへ
フォールバックしていた(既存の全ゲスト護衛地点は偶然にも右端をリクエストしていたため
症状が出なかった)。`chooseHoldTile()`へリクエストされたゾーン内の素の通行可能マス探索
フォールバックを追加して修正。ただしゾーンを1列(col0のみ)にすると、その列自体が
味方部隊+ゲストのスポーンで埋まり再度右端へフォールバックする別の実地問題を確認した
ため、左ゾーンは味方スポーンゾーンと同じ3列(col0-2、`kLeftZoneMinCol`/
`kLeftZoneMaxCol`)へ広げて回避した - 単列ちょうどの左端ではないが、既存の全地点との
「右端固定」との対比では明確に左側という性質は保たれている。

副目標「イリエンを撤退させない」(`ObjectiveKind::ProtectUnit`、Objective.hppが「まだ
どの実コンテンツにも未接続」と記録していたKind)は、**今回もProtectUnit機構そのものは
配線しなかった**。理由: (1)Irien自身が`primaryEscapeUnitsAlternative`の対象でもあるため、
このステージのVictoryは既にIrien生存を含意しており、独立のProtectUnit Objective生成
(BattleFactoryへの新規プラミングが必要)は冗長、(2)1地点のためだけの新規インフラ追加を
避ける既存方針にも合う。代わりにblackwater_crossing以来の「Kind不一致はGameApp.cppの
ad-hoc isPresent()チェックで近似する」パターンをそのまま踏襲し、`proceedToCamp()`で
`ashiron_vein_irien`の生存を直接確認して`mage_recruit`をpendingRecruitCandidateIdsへ、
異常鉱脈記録(`kAnomalousVeinRecordsDiscovery`)をpendingDiscoveriesへ追加した。よって
`ObjectiveKind::ProtectUnit`の「reward-granting未接続」というギャップ自体は今回も
未解消のまま(Objective.hppのコメントは引き続き有効)。

戦闘魔導士イリエンは`data/units.json`の`recruits`へ`mage_recruit`(classId:
`BattleMage`)として新規登録した(既存の`heavy_recruit`等と同型)。ルート3
`[戦闘魔導士候補]`(条件: 地点3Aまたは3B確保)はステージ完了状態をゲートする機構が
存在しない(`scoutRouteRequiredClass`は現在の編成のみを見るクラスゲートで、しかも
このルートが要求するクラス自体をこの地点でしか入手できない循環依存があるため尚更
使えない)ため、地点5ルート3(`[戦闘魔導士]`、条件「イリエン加入候補確定」)が
M9-Dで下したのと全く同じ判断で`scoutRouteDisabled: true`とした。敵は穿岩獣4体+
大型穿岩獣1体(新規stat variantを起こさず、Rock Borerと同じBandit再利用の名前だけ
差し替え)、「異常反応」ルートのみ`enemiesRemoved`反転トリックで残す。副目標「鉱石箱
1個を確保」はblackwater_crossingの荷物箱と全く同じ`surveyObjectiveId`+
`surveyTileCount:1`+`SurveySuccess` RewardRuleパターンで実装(高品質鉄材1)。

**見送った部分(いずれも既存の同型ギャップ、新規判断なし)**:
- 地点3A: ルート2「作業員1人」(guestUnits固定、既知の限界)、両地点の「崩落予告」
  効果はCollapseWarning相当の地形種別が存在しないためno-op(M9-Bの「落石予告」ギャップ
  継続)、ルート3「増援なし」(timedReinforcementもルート別出し分け不可)、副目標
  「坑道支柱1本以上を保全」(Object耐久機構が丸ごと無い、M6-C/M9-C/M9-D同型)
- 地点4: OperateObject測定N箇所(上記)、敗北条件「全測定器破壊」(Object耐久機構、
  同上)。「採掘技術記録は失敗しても地点5の作業台帳から代替取得できる」という
  フォールバックはBlackwater/Windscarが持つ「地域完了フロアtop-up」に相当する仕組みが
  灰鉄採石場自体にまだ存在しないため未実装(新規ギャップとして記録 - 将来
  灰鉄採石場の地域完了top-upを実装するSliceの対象)

`tests/test_battle.cpp`へ6件追加: 地点3A単独脱出Victory、地点3A全作業員撤退Defeat、
地点4単独脱出Victory(脱出タイルが左側ゾーンにあることを直接検証)、地点4
イリエン生存→mage_recruit+異常鉱脈記録の付与(GameApp経由のフル遠征テスト)、既存の
`winCurrentBattle()`ヘルパーへEscapeUnits主目的地点(quarry_old_mine/ashiron_vein)を
自動的に解決するteleport-and-credit分岐を追加(既存の3A→4分岐到達済みテスト2件の
プレースホルダー名アサートを実名へ更新)。既存4テストスイート
(`jf_battle_tests`/`jf_locale_tests`/`jf_content_tests`/`check_localization`)含め
全成功。

`jf_forest_balance --region=ashiron_quarry`(500 Seed)実測: Old Mine Shaft win率
Direct 38.2%/Tactical 49.2%、Ashiron Vein win率Direct 23.8%/Tactical 27.2%。ただし
両地点とも主目的が`EscapeUnits`であり、このツールの自動プレイAIが`ObjectiveKind`を
一切参照しない(M9-I以来の既知のシミュレータ盲点、grep 0件)ため、脱出タイルへ誘導する
判断が組み込まれておらずwin率が実プレイより低く出る構造的な偏りが継続している。
6地点通しExpeditionのReach数値がOld Mine Shaft以降0に近いのも同じ理由による縮小と
見られる。Collapsed Entrance(地点1)のDirect win率が2.8%と大幅に低下している点は
今回のSliceの変更と無関係に見える(地点1はJSON定義のまま無改修)- 既存の別の
シミュレータ不具合の可能性があるが、本Sliceの検証範囲を超えるため未調査のまま記録
のみ残す。[[jf_forest_balance worst-case numbers]]の教訓どおり、実測記録のみで
数値調整は行わない。

### M9-U 旧辺境集落(第6地域): 地域骨格 + 地点1(風化した外柵)

`docs/regions/old_frontier_settlement.md`を確認し、M6/M9の確立済みパターン
(地域骨格を1度作り、以後1地点ずつ本格化する)を踏襲して着手した。M9-Qが追加した
`RegionId::OldFrontierSettlement`+`old_frontier_settlement_outpost`の1地点
プレースホルダーを土台に、本Sliceでスコープ全体(5地点+2キャンプ+地点3・4の
どちらを先に攻略してもよい分岐)+地点1「風化した外柵」の実コンテンツへ拡張した。
M9-Lの命名是正と異なりRegionId自体は既にM9-Qの時点で正本どおり
`RegionId::OldFrontierSettlement`/`"old_frontier_settlement"`だったため改名は不要
だった。

地点1「風化した外柵」は`windwatch_station`/`plateau_relay`と同じくJSON Schemaへ
直接収まったため`data/regions.json`のみで実コンテンツ化した(Region.cppの手書き
ステージ関数は不要)。ルート3`[辺境工兵]`「外柵を仮補修」の「防護柵1個追加」は
flavor/no-opではなく、`brokenwood_territory`(M6)以来実在する`extraBarrierCount`/
`scalesWithExtraBarrierOutcome`機構(`ExplorationOutcome::extraBarrierCount`+
`ObjectPlacementRule::scalesWithExtraBarrierOutcome`)がちょうど「特定ルートでのみ
Barrier Objectを1個追加」という効果に一致したため、これを使って実装した - 新規
`settlement_reinforced_barrier`Barrier定義(耐久8、DEF/RES3、`blocksMovement`)を
count=0のベースに`scalesWithExtraBarrierOutcome:true`で宣言し、ルート3の
`extraBarrierCount:1`で初めて盤上に現れる。durability自体はObject耐久機構が
未実装のため戦闘中に減少しないが、Barrierとして実際に通行を塞ぐ効果は本物。
ルート2「崩れた柵から入る」は`ExplorationOutcome::partyDamage`(既存、Windscar
site2以来の実証済みフィールド)で全員HP-2、`enemiesRemoved:1`で敵1体除外を表現。

新規材料`building_material`(建築材)/`food`(食料)を`materialNameFor()`のknownセット、
`data/locales/{en,ja}.json`の`material.building_material`/`material.food`キーへ
追加した。「建築材-1」(ルート2)/「建築材+1」(ルート3)は`riding_gear`の
負のRewardRule前例(windscarConvoyStage()/windscarCartRoadStage())と同じ
`routeVictoryLootDelta`(JSON側)の負quantityで表現 - 新規プラミング不要。

敵「灰道襲撃団」は正本が斧兵2・弓兵1・軽装剣士1(Axeman/LightSwordsman相当)と
明記するが、Windscar site4調査時と同様、現時点でもプロジェクト全体にAxeman/
LightSwordsman相当のUnitClassは存在しないため確認済み。弓兵はWatchArcherを
そのまま使い、斧兵・軽装剣士はBanditを「Raider」表示名で再利用した(split_convoy
と同じ再利用パターン、追加のJAグリフ登録も不要)。

新規`old_frontier_settlement`TerrainProfile(`data/terrain_profiles.json`)を追加し、
地点1を含む本Sliceの全5地点で共有した(windscarAscentStage()の`windscar_ascent`
プロファイルが後続の全プレースホルダーにも再利用された前例と同型)。正本の
地形生成表(集落道35〜50%/荒れた庭10〜20%/低い石垣10〜15%/家屋跡10〜20%/
共同広場5〜10%/崩れた家屋5〜10%)は新規TerrainTypeを追加せず既存5種で近似:
集落道+共同広場→Floor(50)、荒れた庭→Ash(15)、低い石垣→WatchPost(12、DEF+2が
「低い石垣」の防御ボーナスに一致)、家屋跡→Rubble(15)、崩れた家屋→Barrier(8、
通行不可という正本の第一選択肢に一致)。共同広場固有の「任務地点候補」という
役割は本Sliceでは配線していない(地点1自身が使わないため)。

地域骨格の残り4地点(`settlement_common_well`/`settlement_old_granary`/
`settlement_gathering_hall`/`settlement_dawn_defense`)はM6-B/C/M9-L方式の
Bandit x2最小プレースホルダー。地点3・4の`AllMembers`分岐は
`oldFrontierSettlementGraph()`(`RouteGraph.cpp`)でCinderwatch/Blackwater/Windscar
と同じ形で配線した。

見送った部分(正本との差分、都度明記):

- 副目標「外柵耐久を1以上残す」/報酬「外柵保全: 建築材+1」: Object耐久機構が
  丸ごと未実装(M6-C以来の既知ギャップ)のため、副目標自体を配線しておらず
  報酬側も到達不能のまま未宣言で残した(M9-H以来の「到達不能な報酬は未宣言のまま
  残す」前例と同型)。この地域は「複合防衛」テーマ上、後続の地点でもこのギャップに
  繰り返し当たる見込みが高い(タスク側の既知の予告どおり)
- ルート1「住民配置公開」: エンジンにfog-of-war機構が無く常時全公開のため
  Windscar site1のルート3と同型の暗黙no-op

`tests/test_battle.cpp`へ3件追加: 地域骨格(5地点+ルートグラフ+分岐の
`AllMembers`検証)、地点1の報酬・敵数・ルート別`enemiesRemoved`(FrontalAdvance
4体/CollapsedSidePath 3体)、地域解放条件(既存M9-Q相当のテストは温存)。既存4
テストスイート(`jf_battle_tests`/`jf_locale_tests`/`jf_content_tests`/
`check_localization`)含め全成功。`old_frontier_settlement`TerrainProfile追加時、
weightsの合計が100でないと`loadGameData()`が読み込み失敗する既存バリデーション
(「terrain weights must total 100」)に一度引っかかり、Floor weightを45→50へ
調整して解消した。

`jf_forest_balance --region=old_frontier_settlement`(500 Seed)の実測: 地点1
(風化した外柵)のfresh-party win率はDirect 60.6%/HP残19.6%、Tactical 53.6%/
HP残27.9%。既存地点1の実測レンジ(33.6%〜100%、M9-L訂正記事参照)の範囲内であり
外れ値ではない。[[jf_forest_balance worst-case numbers]]の教訓どおり実測記録の
みに留め、本Sliceでの数値調整は行わない。

以上で旧辺境集落(第6地域)の骨格(5地点+2キャンプ+分岐)が到達可能になり、地点1が
実コンテンツ化された。地点2〜5は次のSlice以降で1地点ずつ本格化する。

### M9-V 旧辺境集落(第6地域) 地点2(共同井戸)

M9-Uに続き、地点2「共同井戸」(`settlement_common_well`)を本格化した。
`guestUnits`(中立住民4人)が必要なため、地点1(JSON定義のみで済んだ)とは異なり
`windscarConvoyStage()`/`windscarCartRoadStage()`と同型のhand-authored
`settlementCommonWellStage()`(`src/core/Region.cpp`)として実装した。
`data/regions.json`の`settlement_common_well`プレースホルダーエントリは
blackwater_crossing以来の前例どおり死んだJSONとしてそのまま残した(参照されない)。

**主目的**: 「共同井戸を3ラウンド防衛」は`primarySurviveRoundsAlternative`
(blackwater_lowlands site3「薬草洲」M9-G以来証明済み)をそのまま再利用した。

**副目標「中立住民を全員避難」は近似ではなく実際の独立Secondary Objectiveとして
実装できた**。タスクの指示どおりObjective.hpp/BattleFactory.cppを読み、
`ObjectiveKind::EscapeUnits`自体がprimary/secondaryのどちらの文脈にも中立で、
`surveyObjectiveId`の副目標グループ追加パターン(新規`groupId`+`primary=false`)を
そのままEscapeUnits版へ転用できることを確認した。新規`StageDescriptor::
secondaryEscapeUnitsAlternative`フィールドを追加し、`BattleFactory.cpp`の
`assembleScenario()`へ`surveyObjectiveId`ブロックと同型の配線を追加、
`GameApp::proceedToCamp()`にも同じ「このgroupIdを持つdefinitionがCompletedかを
走査する」パターンで報酬(Discovery)付与を追加した。`primarySurviveRoundsAlternative`
(primaryグループ)とは完全に独立したグループのため、両者が衝突なく共存することを
テストで確認済み(下記)。これは`primaryEscapeUnitsAlternative`の「primaryを置換」
パターンとは別物 - 既にprimaryをSurviveRoundsが握っているこの地点では
`primaryEscapeUnitsAlternative`は使えないが、今回追加したsecondary版は使える。

**guestUnitsは4人固定(全3ルート共通)**: シナリオ構築時点で固定されルート別に
出し分けられない既知の限界(M9-I以来)。ルート1「両集団を退避」/ルート3
`[旗手]`「共同の防衛位置を示す」がともに4人(両集団)を明示する多数派のため4人を
採用し、ルート2「井戸だけを先に守る」の正本の2人は未モデル化(4人のまま近似、
`secondaryEscapeUnitsAlternative.requiredEscapeCount`も4人固定)。

**ルート3`[旗手]`「中立Unitが最寄り避難所へ自動移動」は見送り**: AI制御された
味方ユニットの自動経路探索機構自体が存在しないため(windscarConvoyStage()の
「防衛中に負傷者HP-3」等と同型の既知のギャップ)、暗黙のno-opとして記録。
`scoutRouteRequiredClass`は`UnitClass::BannerBearer`。

**敵**: 正本の斧兵1・弓兵2・軽装剣士1に対応するAxeman/LightSwordsman相当の
UnitClassは存在しない(M9-U自身の確認と同じ結論) - 弓兵は`WatchArcher`、
斧兵・軽装剣士は`Bandit`を「Raider」表示名で再利用(追加JAグリフ登録不要)。
「井戸優先ルート(ルート2)は斧兵1追加」は5体目としてベースロースターへ常時含め、
ルート1/3で`enemiesRemoved=1`により差し引く加算後減算パターン
(windscarAscentStage()以来)を採用した。

**見送った部分(正本との差分)**:
- 副目標「井戸耐久8以上」/報酬「食料+1」/敗北条件「井戸耐久0」: Object耐久機構が
  丸ごと未実装(M6-C以来の既知のギャップ)のため未配線。報酬は到達不能なまま
  未宣言で残した(M9-Hの前例と同型)
- 恒久成果`settlement_well_agreement_reached`: 地域単位の「5成果達成+安全帰還で
  恒久化」機構自体がまだ無く、M9-U自身も地点1の`settlement_outer_fence_opened`を
  未配線のままにしている - 同じ理由でこのSliceでも見送り、個別地点の恒久成果配線は
  地域全体の機構実装タイミングでまとめて対応する判断とした
- 「全員避難: 集落証言記録」用の新規Discovery id`settlement_communal_testimony_records`
  (`kSettlementCommunalTestimonyDiscovery`)は正本の「安定ID」表に記載が無いため
  `<region>_..._records`命名規則(`kMiningTechniqueRecordsDiscovery`等)に倣って
  選定した。ui_shared.cppの`discoveryNameFor()`には未登録のまま(表示名は
  raw idへフォールバック) - `kWindscarRoadChartDiscovery`/`kCourierRouteChartDiscovery`/
  `kMiningTechniqueRecordsDiscovery`/`kAnomalousVeinRecordsDiscovery`もすべて
  未登録の既存ギャップで、新規に導入したものではない

**キャンプI**: `RouteGraph.cpp`の`oldFrontierSettlementGraph()`はM9-Uの時点で
既に`settlement_common_well -> settlement_camp1 -> settlement_granary_hall_branch`
という配線が完了しており、本Sliceでの追加変更は不要だった(骨格構築時に先読みで
配線済み)。

`tests/test_battle.cpp`へ5件追加: 地点2の構成検証(敵5体ロースター・guestUnits4人・
primarySurviveRoundsAlternative/secondaryEscapeUnitsAlternativeの各id・ルート別
敵数と報酬)、中立住民全員撤退によるDefeat(`allGuestsLost()`)、独立Secondary
Objectiveとしての全員避難完了(primaryグループの状態に一切影響しないことを確認)、
GameApp経由のフル遠征テストでの`kSettlementCommunalTestimonyDiscovery`付与確認。
既存4テストスイート(`jf_battle_tests`/`jf_locale_tests`/`jf_content_tests`/
`check_localization`)含め全成功。テスト実行中、本Sliceと無関係な既存テスト
(Cinderwatch地点3の`stacked_crate`個数検証、乱数seed依存)が単発で1回だけ
flakyに失敗する既知の事象を確認したが、複数回の再実行では毎回成功しており
本Sliceの変更とは無関係と判断した(未調査のまま記録のみ)。

`jf_forest_balance --region=old_frontier_settlement`(500 Seed)の実測: 地点2
(共同井戸)のfresh-party win率はDirect 100.0%/HP残82.9%、Tactical 97.4%/
HP残78.3%(いずれも主目的がEliminateTeam ORなためシミュレータが素直に殲滅で
勝てている)。[[jf_forest_balance worst-case numbers]]の教訓どおり実測記録の
みに留め、本Sliceでの数値調整は行わない。

### M9-W 旧辺境集落(第6地域) 地点3(旧穀物庫)

M9-Vに続き、地点3「旧穀物庫」(`settlement_old_granary`)を本格化した。地点1
(`settlement_outer_fence`)と同じく`guestUnits`が不要なため、
`stageDescriptorFromContent()`+`data/regions.json`のJSON-authoredパスに収まり、
Region.cppの手書きステージ関数は不要だった。`data/regions.json`の
`settlement_old_granary`プレースホルダーエントリを実コンテンツへ差し替えた
(`RouteGraph.cpp`の`oldFrontierSettlementGraph()`はM9-Uの時点で既に
`settlement_old_granary`を参照済みのため、配線側の変更は不要)。

**主目的**: 「穀物庫を4ラウンド防衛、または敵全滅」は`primarySurviveRoundsAlternative`
(blackwater_lowlands site3「薬草洲」M9-G以来証明済み、M9-Vでも再利用済みの
パターン)を`surviveUntilRound: 4`でそのまま再利用した。

**副目標「食料箱2個を保持」**は`surveyObjectiveId`+`surveyTileCount: 2`+
`surveyTileObjectDefinitionId`(ironwatch_stores以来証明済みのcrateパターン)を
`food_crate`という新規definitionIdで再利用し、`surveyBonusLoot`で食料+2を配線した。

**敵**: 正本の斧兵2・弓兵2・軽装剣士1に対応するAxeman/LightSwordsman相当の
UnitClassは存在しない(M9-U/Vと同じ既知の結論) - 弓兵2体は`WatchArcher`、
斧兵2体・軽装剣士1体は`Bandit`を「Raider」表示名で再利用(追加JAグリフ登録不要)。
3ラウンド目の軽装剣士1体の予告増援は`TimedReinforcementData`
(`spawnRound: 3`、`announceRoundsBefore: 1`、herb_islet「薬草洲」以来証明済みの
機構)をそのまま再利用した。

**探索3択**: ルート1「備蓄を数えてから運ぶ」は無条件・敵5体で標準ロースターに
一致(`routeOutcomes`変更なし)。ルート2「住民へ先に配る」は`enemiesRemoved: 1`
(敵4体)+`routeVictoryLootDelta`で食料+1。ルート3`[古参守備兵]`「搬出口を封鎖」は
`scoutRouteRequiredClass: VeteranGuard`+`enemiesRemoved: 1`(敵4体)。

**ルート3の「敵増援なし」は見送り**: `TimedReinforcementData`はStage全体で1つの
固定フィールドで、`routeOutcomes`側にも特定ルートでのみ増援を無効化する機構は
存在しない(`guestUnits`がルート別に出し分けられないM9-I以来の限界と同型)。
コードベース全体を確認したが、既存地点にも「特定ルートで増援を止める」前例は
無かった(windscarConvoyStage()等の「防衛中に負傷者HP-3」と同じ、単に未接続の
既知ギャップ)。このため全ルート共通で3ラウンド目増援ありのままとし、ルート3の
この差分は暗黙のno-opとして記録する(次に同種のニーズが出た時点で
`routeOutcomes`側へ増援抑制フラグを追加する判断は保留)。

**見送った部分(正本との差分、Object耐久機構が丸ごと未実装というM6-C以来の
既知のギャップに起因)**:
- ルート2「穀物庫耐久-3」/ルート3「穀物庫耐久+2」: flavor/no-op(耐久という
  ベース値自体が存在しないため引き算・足し算のしようがない)
- 副目標「穀物庫耐久6以上」/敗北条件「穀物庫耐久0」: 未配線
- 恒久成果`settlement_granary_shared`: 地域単位の「恒久化」機構自体が
  まだ無い(M9-U/Vと同じ理由で見送り)

`tests/test_battle.cpp`へ1件追加: 地点3の構成検証(敵5体ロースター・
`scoutRouteRequiredClass: VeteranGuard`・`primarySurviveRoundsAlternative`/
`timedReinforcement`の各id、ルート別敵数と報酬、副目標成功時の食料+2上乗せ)、
ルート2の`enemiesRemoved`確認、SurviveRoundsで4ラウンド目まで生存してVictoryに
なることの確認(herb_islet「薬草洲」の第2テストと同型の
`beginEnemyPhase()`/`beginPlayerPhase()`ループ)。既存4テストスイート
(`jf_battle_tests`/`jf_locale_tests`/`jf_content_tests`/`check_localization`)
含め全成功(`cmake --build build -j10 --clean-first`+`ctest --test-dir build -j10`)。

`jf_forest_balance --region=old_frontier_settlement`(500 Seed)の実測: 地点3
(旧穀物庫)のfresh-party win率はDirect 69.4%/HP残13.6%、Tactical 80.6%/
HP残24.1%(主目的がSurviveRounds ORのためシミュレータが素直に殲滅または
4ラウンド生存で勝てている)。地点1(Direct 60.6%)と近いレンジで外れ値ではない。
[[jf_forest_balance worst-case numbers]]の教訓どおり実測記録のみに留め、本Sliceでの
数値調整は行わない。

### M9-X 旧辺境集落(第6地域) 地点4(集会家屋)

M9-Wに続き、地点4「集会家屋」(`settlement_gathering_hall`)を本格化した。地点1・3と
同じくguestUnitsが(全ルート共通では)不要なため、`stageDescriptorFromContent()`+
`data/regions.json`のJSON-authoredパスに収まった。`RouteGraph.cpp`の
`oldFrontierSettlementGraph()`はM9-Uの時点で既に`settlement_gathering_hall`を
`settlement_granary_hall_branch`(`BranchCompletion::AllMembers`)の一員として
参照済みのため、配線側の変更は不要だった。

**主目的「記録箱2個以上を集会家屋へ運ぶ」は真のN-of-M閾値では実装していない**。
タスクの指示どおり`surveyTileCount`の実際の意味を`BattleFactory.cpp`で確認したところ、
Nタイル配置時でもグループは常に`ObjectiveGroupRule::Any`(=配置されたN個のうち
「どれか1個」で群完了、AND側の「全部」もN-of-Mの閾値も無い)であることを再確認した
(既存の`settlement_old_granary`「食料箱2個保持」も同じ Any-of-2 の近似のまま実装
されている)。「2個以上」という閾値をそのまま表現する新規機構はM9-H「樹脂箱2個の
うち1個以上」(Blackwater地点4)の時点で既に見送り済みで、本SliceもM9-Hの前例に
倣い**主目的を標準EliminateTeam(既定値、StageDescriptorに何も設定しない)で近似**
した。都合の良いことに、正本のOR条件「敵全滅後、残る記録箱1個を操作」の
「敵全滅」側とこの近似は完全に一致するため、近似後の主目的は正本のOR条件の
半分をそのまま体現している(OperateObjectのOR-alternate自体はM9-D/J/M/O/Q/T
以来の「AND/OR混在Kind合成」ギャップのため引き続き見送り)。

**記録箱はsurveyObjectiveId経由で3個固定配置**: ルート別個数(3/2/3、ルート2のみ
2個)は、ルート別Object個数を出し分けられない既知の限界(Windscar地点系列以来)の
ため固定3個で近似した(`settlement_gathering_hall_ledger_crates`+
`surveyTileCount:3`+`surveyTileObjectDefinitionId:"settlement_ledger_crate"`、
food_crateパターンと同型)。副目標「3個すべて回収」も同じAny-of-3の近似のまま
専用のsurveyBonusLootは設定していない(正本の「記録箱3個: 集落台帳」報酬は
Discovery付与であり、`stage.discoveries`(無条件でVictory時に付与、既存の多くの
地点と同じ機構)で`settlement_command_ledger`(正本「安定ID」表の既存ID)を
そのまま採用した - Survey成功を条件にするより単純だが、正本の「3個すべて回収」
という副目標条件そのものへの厳密な紐付けは近似のまま残る)。

**guestUnitsはこの地点では実装しなかった**: 正本のルート1・2にはguest言及が
一切無く、ルート3`[行軍隊長]`のみ「中立住民2人が記録箱を運搬」を導入する非対称な
構成である。M9-V「共同井戸」の`[旗手]`ルートのように全ルート共通でguestが存在し
1ルートだけ数値が変わるケース(そちらは4人固定近似が妥当)とは異なり、本地点は
「guestが存在しないルートの方が多数派」であるため、4人固定方式と同じ理由付けで
2人固定を採用するのは正本の非対称性を歪める判断と考え、guestUnits自体を配線
しない選択をした。ルート3の「中立住民2人が記録箱を運搬」は未配線のflavor text
として記録し、対応する副目標「中立運搬人を撤退させない」・報酬「運搬人生存:
織物1」も同時に見送った。

**ルート2「防護柵1個追加」**は地点1「風化した外柵」以来実在する`extraBarrierCount`/
`scalesWithExtraBarrierOutcome`機構(`settlement_reinforced_barrier`Barrier定義を
再利用)でそのまま実装した - flavor/no-opではない。

**敵**: 正本の軽装剣士2・弓兵1・斧兵1に対応するAxeman/LightSwordsman相当の
UnitClassは存在しない(M9-U/V/Wと同じ既知の結論) - 弓兵は`WatchArcher`、
軽装剣士2体・斧兵1体は`Bandit`を「Raider」表示名で再利用(追加JAグリフ登録不要)。
ルート2「防衛準備を優先する」は`enemiesRemoved:1`(敵3体)、ルート3は
`scoutRouteRequiredClass:MarchCaptain`で敵数は基準の4体のまま(正本どおり)。

**見送った部分(正本との差分)**:
- 敗北条件「記録箱3個をすべて失う」: Object耐久機構が丸ごと未実装(M6-C以来の
  既知のギャップ)のため未配線
- 恒久成果`settlement_ledger_restored`: 地域単位の「恒久化」機構自体がまだ無い
  (M9-U/V/Wと同じ理由で見送り)
- guestUnits関連一式(上記の判断どおり): 中立住民2人の運搬・副目標「運搬人撤退
  させない」・報酬「織物1」

**キャンプII到達可能性を確認**: `RouteGraph.cpp`の`oldFrontierSettlementGraph()`は
`settlement_granary_hall_branch`(`BranchCompletion::AllMembers`、
`{"settlement_old_granary", "settlement_gathering_hall"}`)→`settlement_camp2`の
配線をM9-Uの時点で既に持っており、両地点(地点3・4)が本Sliceまでに実コンテンツ化
されたことで、キャンプIIは初めて実際に到達可能になった(骨格構築時のプレースホルダー
2地点は`jf_content_tests`のRoute Graph到達可能性検証を素通りしていただけで、実際の
実プレイでは中身が伴っていなかった)。

`tests/test_battle.cpp`へ1件追加: 地点4の構成検証(敵4体ロースター・
`scoutRouteRequiredClass:MarchCaptain`・`primarySurviveRoundsAlternative`が
未設定(既定EliminateTeam)・`surveyObjectiveId`/`surveyTileCount`・ルート別
`enemiesRemoved`/`extraBarrierCount`・Barrier Objectの実配置・
`computeStageDiscoveries()`が`settlement_command_ledger`を返すこと)、標準
EliminateTeamでのVictory確認。既存4テストスイート(`jf_battle_tests`/
`jf_locale_tests`/`jf_content_tests`/`check_localization`)含め全成功
(`cmake --build build -j10`+`ctest --test-dir build -j10`)。実装中、JSON側の
Object配置キー名を誤って`objectPlacements`(誤)と書いてしまい`objectPlacementRules`
(正)との取り違えでBarrierが盤上に現れない不具合に一度当たった - `definition`
ブロックへ`definitionId`を明示していなかった不備も併発しており、地点1の既存JSONを
再確認して両方修正した。

`jf_forest_balance --region=old_frontier_settlement`(500 Seed)の実測: 地点4
(集会家屋)のfresh-party win率はDirect 60.6%/HP残19.6%、Tactical 53.6%/HP残27.9%
(主目的が標準EliminateTeamのため、同じく敵4体基準ロースターの地点1「風化した
外柵」と数値が一致 - 同一TerrainProfile・近い敵構成であるため偶然の一致であり
外れ値ではない)。[[jf_forest_balance worst-case numbers]]の教訓どおり実測記録の
みに留め、本Sliceでの数値調整は行わない。

以上で旧辺境集落(第6地域)の地点1〜4が実コンテンツ化され、キャンプIIが実際に到達
可能になった。残るは地点5「夜明けの共同防衛」(地域ボス「襲撃団頭領」を含む最終
防衛戦)のみで、次のSlice以降で本格化する。

### M9-Y 旧辺境集落(第6地域): 地点5(夜明けの共同防衛)/ 強敵「襲撃団頭領」/ 地域攻略

`docs/regions/old_frontier_settlement.md`「5. 夜明けの共同防衛」「強敵 襲撃団頭領」
「地域攻略と拠点接続」「最低保証報酬」を確認し、旧辺境集落の最終地点+強敵+地域攻略を
実装した。地点5は正本自身が「頭領撃破ではなく共同防衛が目的、頭領は無視して5ラウンド
守ることもできる」と明記する非ボス構成のため、M9-K/M9-Qの真ボス実装(独自
`takeXBossTurn()`)をそのまま踏襲せず、まず生成AI+Profile調整だけで足りるかを検討した。

**主目的のAND合成(3サブ条件)の扱い**: `BattleFactory.cpp`を実際に読み、
`primarySurviveRoundsAlternative`が常に「defaultのprimary groupをAnyへ拡張し
SurviveRounds memberを足す」(EliminateTeamとのOR)動作で、「SurviveRoundsのみ、
OR拡張なし」という設定は存在しないことを確認した。タスク指示どおり「よりシンプルな方」
を採用する判断で、新規フィールドは追加せず`primarySurviveRoundsAlternative`
(`surviveUntilRound: 5`)をそのまま再利用した - このORが具体的に数値・勝敗判定を
壊す理由が見当たらないため。サブ条件2(井戸/穀物庫耐久1以上)はObject耐久機構が
丸ごと未実装(M6-C以来の既知のギャップ)のため見送り。サブ条件3(警鐘操作)は
完全な見送りにはせず、新設した`StageDescriptor::ObjectPlacementRule::
secondaryOperateObjectiveId`(既存の`operateObjectiveId`と同型だが「primaryを置換」
ではなく「独立追加」)経由で実際に機能する非primary Secondary Objectiveとして配線した -
進捗は追跡できるが主目的自体は左右しない。

**新規発見・修正した既存バグ**: `secondaryOperateObjectiveId`実装時、
`ObjectiveTracker.cpp`の`syncObjectiveProgress()`が内部で使う`primaryGroups()`が
`def.primary`でフィルタしており、primary=falseのOperateObjectは(SecureTile/
EscapeUnitsと異なりLive評価専用でイベント駆動パスを持たないため)このフィルタに
弾かれて永久にCompletedへ遷移できないことが判明した - 本Slice以前にはprimary=falseの
OperateObjectが存在しなかったため露見していなかった既存の潜在バグで、本Sliceが
初めて踏んだ。`validateBattleMission()`の「primary groupは1つのみ」検証は
`primaryGroups()`のprimary厳格フィルタに依存しているため、そちらは変更せず、
`syncObjectiveProgress()`専用の新規`liveEvalGroups()`(primary、またはKind==
OperateObjectを含む)を追加してこの1箇所だけ差し替えた。

**強敵「襲撃団頭領」はbespoke boss AIを実装しなかった**: 新規`UnitClass::RaidLeader`
(`data/classes.json`、正本どおりHP42/STR10/DEF7/RES3/MOV4、新規武器`heavy_axe`
威力7射程1)を追加したが、`EnemyAI.cpp`へ新しい`takeXBossTurn()`は書かなかった -
既存の`takeEnemyTurn()`/`generateAiCandidates()`パス(`AiSystem.cpp`の`profileFor()`)
にRaidLeader専用の分岐を1つ足し、`retreatHpPercent`を30(正本の「HP30%以下」)へ
調整しただけ。固有行動3つのうち「柵割り」(Object限定ダメージ+4)はObject耐久機構が
無いため無意味、「略奪指示」(次増援が穀物庫優先)はObject指定Targeting AIが無いため
no-op、「退路判断」(HP30%以下かつ4ラウンド目以降で撤退)はAiProfileにラウンド認識の
フックが無いため「4ラウンド目以降」のゲートだけ近似で省略(HP30%以下なら1ラウンド目
からでも撤退候補になる) - 3つとも既存の汎用機構の範囲に収まり、専用ターン関数を
書くほどの複雑さが無いと判断した。正本自身が「強敵は無視して勝てる」と明記する
non-boss構成であることが、この軽量アプローチを裏付けている。

**複数波増援のうち1波のみ実装**: `StageDescriptor::timedReinforcement`は
`std::optional`の単一フィールドで、正本が要求する2波(2ラウンド目:軽装剣士2、
4ラウンド目:斧兵1・弓兵1、いずれも1ラウンド前予告)を同時に表現できない - この
プロジェクトで複数`timedReinforcement`が同時に必要になったのはこの地点が初めてで、
既存の前例が無い新種の制約。タスク指示どおり「より影響の大きい方を採用、他方は
見送り明記」の判断で、5ラウンド防衛という主目的の中間地点により近い2ラウンド目の
波(軽装剣士2、Bandit「Raider」再利用)を実装し、4ラウンド目の波は見送った。

**探索3択**: ルート1「外柵を中心に守る」は地点1/4以来の`extraBarrierCount`/
`scalesWithExtraBarrierOutcome`機構(`settlement_reinforced_barrier`再利用)で
防護柵2個を実装、「上段増援が多い」はルート限定の増援配分機構が無いためno-op。
ルート2「住民を先に避難させる」は「敵1体追加」を`enemyRoster`6体目の常時包含+他
2ルートでの`enemiesRemoved:1`差し引き(加算後減算パターン)で実装、「避難所耐久-3」
はObject耐久機構未実装のためno-op、「中立Unitなし」はguestUnitsがルート別に
出し分けられない既知の限界(M9-I以来)のためno-op(4人のまま近似)。ルート3
`[旗手]`は`scoutRouteRequiredClass:BannerBearer`、「3組分散配置」は分散初期配置
機構が無いためno-op、「警鐘を開始時に1個操作済み」もSecondary Objectiveを開始前
からCompleted状態にする機構が無いため見送り(他2ルートでも通常操作で普通に
達成可能なので達成手段自体は失われない)。

**副目標・報酬**: 「中立住民を全員避難」は`secondaryEscapeUnitsAlternative`
(settlementCommonWellStage()以来の独立Secondary Objectiveパターン、4人固定)を
そのまま再利用、GameApp::proceedToCamp()から織物2を付与。「頭領撤退」は
`settlement_dawn_raid_leader`固定idの`exitReason==Retreated`を走査するad-hoc
ボーナス(windscar_plateauの「高原運び手を2人以上撤退」と同型)で鉄材1を付与。
「味方戦闘不能者0」は既存`noCasualtiesBonusLoot`(建築材1)をそのまま宣言。
「井戸・穀物庫保全: 集団防衛資料」はObject耐久機構未実装のため個別報酬としては
未配線(到達不能、M9-Hの前例と同型)だが、地域攻略の最低保証報酬フロア経由では
確実に取得できる(下記)。**実装中に見つけたバグ**: 最初`mergeLoot()`
(織物2・鉄材1の両方)を使ったが、この関数は`expedition_.pendingLoot`へ既に
挿入済みの`loot`ローカル変数にしか作用せず、以降の変更は反映されないことが
テストで判明した(このバグは`plateau_relay`等の既存Discovery系ad-hocブロックが
`pendingDiscoveries`へ直接pushしているのと同じ理由で回避されていた領域に、Loot系の
新規ブロックを初めて追加したために露見した) - `expedition_.pendingLoot.push_back()`
への直接pushへ修正した。

**地域攻略・最低保証報酬**: M9-Kの`blackwaterMaterialsEarned`と完全に同じ形で
`settlementMaterialsEarned`(新規フィールド、`BaseState.hpp`/`SaveSystem.cpp`)を
追加し、`ExpeditionService.cpp`の`applyExpeditionReturnToBase()`へ同型のTop-up
ブロックを実装(建築材6・鉄材3・食料7・織物2のフロア、`settlement_command_ledger`/
`collective_defense_records`の2 Discovery - 正本の「安定ID」表で「集落台帳・
援護命令」「集団防衛・不動の構え」がそれぞれ1つのidを共有すると明記されているため、
フロア表の4行はこの2 idへ集約される)。`old_frontier_settlement_secured`という
安定ID自体は前例どおりコード上の実体は無く、`RegionId::OldFrontierSettlement`が
`completedRegionIds`へ入ることがその実装。共同施設研究`collective_facility_methods`・
「宿舎交流区画研究可能化」・スキル解放(援護命令/不動の構え)は、Facilities.hppの
`SkillDefinition`にDiscovery条件でスキルをゲートする機構自体が存在しない(M9-Qが
`plateau_targeting_records`等で記録した既知のギャップと同型)、および「地域共同作業」
という施設研究の仕組み自体が存在しないため、Discovery id自体は付与されるがその
効果は解放されない(既知の据え置き)。なお`social_quarters`(交流区画)facility node
は元々`requiredDiscoveries`が空で常時研究可能なため、正本の要求は事実上満たされている。

**発見した既存の別バグ・修正**: `ExpeditionService.cpp`の`computeRegionSummaries()`
の地域一覧・predecessorラムダが`RegionId::WindscarPlateau`までしかカバーしておらず、
`RegionId::OldFrontierSettlement`自体がM9-U以来ずっとBase画面の地域一覧に
出現していなかった(pre-existing gap)。EmberRavine追加のためにこの一覧を拡張する
過程で発覚し、両地域を追加して解消した - この修正が無いと旧辺境集落もEmberRavineも
Base画面から選択できないままだったため、地域攻略の実プレイ確認に不可欠な修正として
本Sliceで対応した。

**燼火峡谷(第7地域)**: 新規`RegionId::EmberRavine`+`ember_ravine_outpost`
(`data/regions.json`、Bandit2体の最小プレースホルダー)で追加した。M9-K/Qの
`_outpost`プレースホルダー前例を踏襲(`old_frontier_settlement`TerrainProfileを
再利用)。`Region.cpp`の4箇所のswitch文・`regionUnlocked()`
(OldFrontierSettlement完了で解放)・`ExpeditionService.cpp`の地域リストを他地域と
同じ形で配線した。

`tests/test_battle.cpp`へ7件追加: 地点5の構成検証(敵6体ロースター・
primarySurviveRoundsAlternative(5ラウンド)・secondaryEscapeUnitsAlternative・
guestUnits4人・timedReinforcement・noCasualtiesBonusLoot・secondaryOperateObjectiveId・
ルート別敵数/防護柵2個)、SurviveRoundsで5ラウンド目まで生存してVictoryになることの
確認、中立住民全員撤退によるDefeat(`allGuestsLost()`)、警鐘の独立Secondary
Objective完了(primaryグループの状態に一切影響しないことを確認、interactionCount
Live評価)、GameApp経由のフル遠征テスト(頭領撤退で鉄材1・全員避難で織物2の
ad-hocボーナス)、地域攻略テスト(5地点完走→`old_frontier_settlement_secured`
相当の`completedRegionIds`確定→最低保証報酬フロア→EmberRavine解放→
`regionSummaries()`にEmberRavineが出現)。既存の地域一覧テスト
(`summaries.size() == 5`)は7地域(OldFrontierSettlement/EmberRavine追加)へ更新した。
既存4テストスイート(`jf_battle_tests`/`jf_locale_tests`/`jf_content_tests`/
`check_localization`)含め全成功。

`jf_forest_balance --region=old_frontier_settlement`(500 Seed)の実測: 地点5
(夜明けの共同防衛)のfresh-party win率はDirect 89.2%/HP残60.4%、Tactical 77.8%/
HP残51.6%(主目的がSurviveRounds ORのため、地点2・3同様シミュレータが素直に殲滅
または5ラウンド生存で勝てている - 正本の非ボス構成と`primarySurviveRoundsAlternative`
再利用が噛み合った結果と見られる)。5地点通しのRegion clear win率はDirect 0.6%/
Tactical 2.2%(いずれも極めて低い)だが、これは地点5自体の数値・AIの問題ではなく、
[[jf_forest_balance worst-case numbers]]が記録する既知の「guest-escort/survey地点は
自動プレイAIに過小評価されやすい」傾向どおり、Reachが地点2(共同井戸、guestUnits)
303/500・地点4(集会家屋、surveyTileCount)86/500と早い段階から目減りしている
ことに起因する(地点5自体のReachは4/500・18/500まで下がるが、これは前段の
目減りの複利)。実測記録のみに留め、本Sliceでの数値調整は行わない。

以上で旧辺境集落(第6地域)の全5地点が実コンテンツ化され、地域全体を安全帰還まで
攻略可能になった(guestUnits/surveyTileCountを伴う地点のOperateObject/EscapeUnits
自動プレイ非対応を除き、実プレイでのend-to-endクリアはエンジン機構としては揃って
いる - 部隊全滅を回避しつつ手動プレイすれば5ラウンド生存または敵全滅、全員避難、
警鐘操作をすべて達成でき、安全帰還でRegionId::OldFrontierSettlementが
completedRegionIdsへ入り、最低保証報酬フロアが適用され、燼火峡谷(第7地域)が
選択可能な状態でBase画面に追加される)。

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
- M1-E「M9前ブロッカー」項目3・4(静的部分)完了(2026-07): `loadGameData()`が起動時に
  全Route Graphの到達可能性(`validateRouteGraph()`)を検証するようになり、
  `jf_content_tests`が全Stage×`FrontalAdvance`/`CollapsedSidePath`/`ScoutRoute`の
  3ルートで`validateBattleMission()`のエラーが空であることをアサートするようになった
  (従来は戦闘組み立てのたびに呼ばれていたが結果を`stderr`へ出すだけでCTestを
  落とさなかった)。詳細: [`implementation_roadmap.md`](implementation_roadmap.md#m1-e-コンテンツ追加基盤)
- M1-E「M9前ブロッカー」項目2(RewardRule置換)完了(2026-07): `StageDescriptor`/
  `StageContentData`が並行して持っていた`baseVictoryLoot`/`routeVictoryLootDelta`/
  `surveyBonusLoot`の3フィールドを`std::vector<RewardRule>`(`jf/data/GameData.hpp`、
  `Condition::Always`/`RouteChoice`/`SurveySuccess`の3種)1本へ統一した。正本
  (`docs/region_mission_data_contract.md`)が定める本格的な報酬台帳
  (`RewardGrantId`二重付与防止、初回勝利/再調査の別表化)とは別物で、それはM9まで
  持ち越したまま(`implementation_roadmap.md`のM1-E Slice1コメントに明記済み)。
  `data/regions.json`のJSON key名・構造は無改修 - `GameData.cpp`のLoaderが同じ3つの
  JSON配列を読んで統一後の1配列へ詰め直すだけなので、既存コンテンツの数値には一切
  触れていない。`computeStageVictoryLoot()`(`src/core/Region.cpp`)を統一後リストの
  単純な1回走査へ書き換え、`stageDescriptorFromContent()`のフィールドコピーも3行から
  1行になった。`jf_forest_balance`(AshboughForest、200 Seed)の実測は移行前後で
  完全に同一(Byte-identical)。
- M1-E「M9前ブロッカー」項目1(Encounter生成ロジック自体のDefinition駆動化)は
  意図的に見送り(2026-07、調査済み): `BattleFactory.cpp`の`assembleScenario()`が持つ
  14ステップの生成手順は既に地域名・地点名による分岐を一切持たず、全て`stage.xxx`の
  有無チェックだけで分岐している。出荷済み7 Stage(灰枝の森3・沈黙した監視所群6・
  灰鉄採石場プレースホルダー)は全て同じ手順で問題なく生成できており、「この手順では
  表現できないStage」は現状一つも存在しない。正本(`docs/region_mission_data_contract.md`
  等)にも具体的なStep Schemaは無く、ロードマップの1行の目標記述に留まっていた。
  実装原則9「未実装の将来拡張を先に一般化しすぎない」との衝突を理由に、M9で実際に
  現行手順では表現できないStageが出た時点で着手する判断とした。M1-E「M9前ブロッカー」
  4項目はこれで全て対応済み(3項目完了・1項目意図的見送り)。

## M7項目3続き 武器分岐の全兵種一般化(2026-07)

`docs/implementation_roadmap.md`「M7項目3(残り)」の武器分岐部分を、槍兵専用実装から
全12兵種へ一般化した。仕様の数値・固有効果・製作費・レシピDiscoveryは
`docs/base_development.md`「初期6兵種の武器分岐仕様」「後半6兵種の武器分岐仕様」・
`docs/character_progression.md`「初期6兵種の武器レシピ」を正本のまま使用し、新しい
ゲームデザイン数値は追加していない。

実装した内容:

- `data/weapons.json`: 残り11兵種・33分岐武器の威力・射程・MOV補正を追加
- `include/jf/core/Facilities.hpp`: 兵種ごとの`{class}_forging`解放ノード11個と、
  `craft_*`分岐レシピ33個(製作費・Discovery条件・前提ノード付き)を追加。
  `FacilityNode`に`weaponBranchClass`フィールドを新設し、レシピがどの兵種に属するかを
  データから判定できるようにした(旧実装はSpearman固定のUIハードコードに依存していた)
- `src/core/GameApp.cpp`の`equipWeaponForUnit()`: 事前調査では「バックエンドは兵種非依存
  で変更不要」とされていたが、実際には`unit->classId != UnitClass::Spearman`および
  Spearman専用の`requiredRecipes`マップでハードコードされていたため、
  `FacilityNode::weaponBranchClass`を使った汎用実装へ書き換えた(この訂正は当初の
  確認済み事項からの逸脱)
- `src/ui_facilities.cpp`の3箇所のハードコード(`isWeaponRecipe`フィルタ、
  `craftClasses[6]`配列と「(未実装)」無効ボタン分岐、`drawUnitScreen()`の
  `unit->classId == Spearman`ゲート)を全て汎用化。`drawForgeEquipmentPanel()`自体も
  Spearman専用の候補武器配列で組まれていたため、クラスの基本武器+登録済み`craft_*`
  ノードからデータ駆動で候補一覧を組むよう書き換えた
- `data/locales/{en,ja}.json`に33武器分の`weapon.*`キーを追加(日本語グリフは
  `loadAppFont()`が`facilityNodeRegistry()`のJA文字列と`allJapaneseGlyphText()`を
  自動収集するため、手動でのcharset編集は不要だった)
- `tests/test_battle.cpp`: 11兵種分のレシピ解放+武器ステータス入替テスト
  (威力・射程・MOV補正)、およびノックバック系(圧進槌・追込弓)・状態異常付与系
  (制圧弓・拘束弓・残火焦点具が使う`Weapon::onHitStatuses`)の一般化済みエンジンフック
  を検証するテストを追加

エンジンに未接続の固有効果: `Weapon`構造体が汎用的に持つのは
`moveModifier`/`causesKnockback`/`onHitStatuses`の3種のみで、Spearmanの3分岐は
すべてこれでカバーできていた。しかし残り33分岐の大半(隊形範囲拡張、待機時DEF上昇、
条件付き追加ダメージ、Heal回復量変更、再移動、旗支援効果の付け替え等)は専用の
エンジン機構が無く、今回はレシピ・武器データ・`effectEn`/`effectJa`の説明文までは
実装したが、固有効果そのものの戦闘挙動は未接続。`causesKnockback`/`onHitStatuses`へ
素直に写像できた圧進槌・追込弓(ノックバック)、制圧弓・拘束弓(移動低下)、残火焦点具
(炎上)、および`moveModifier`のみで完結する武器(戦式焦点具・街道剣など)は実際に
機能する。残りは今回の変更でSpearmanと同じ「レシピは解放できるが固有効果は未接続」
という新規ギャップであり、既存の「ゲートは実装済みだが到達不能」パターンとは別種の
既知の未実装として次の優先候補へ追加する。

必要Discoveryが現状どの地域コンテンツからも付与されない(実装済みだが到達不能な)
レシピ: `cinderwatch_command_drills`・`quarry_combat_records`・
`settlement_command_ledger`・`cinderwatch_defense_manual`・`quarry_brace_records`・
`plateau_patrol_records`・`cinderwatch_watch_records`・`quarry_ambush_records`・
`sanctum_field_medicine`・`heavy_armor_method`・`demolition_forging`・
`impact_balance_record`・`field_construction_manual`・`controlled_demolition_notes`・
`structural_repair_guide`・`mounted_charge_drill`・`escort_signal_code`・
`snare_pattern_record`・`quarry_tracking_notes`・`herding_shot_method`・
`long_range_signal_code`・`vanguard_banner_record`・`protective_banner_rite`・
`arcane_resonance_record`・`battle_focus_formula`・`controlled_ember_formula`。
既に付与経路がある(到達可能な)もの: `plateau_targeting_records`・
`ashbough_forest_survey_complete`・`marsh_emergency_medicine`(=既存の
`kMarshEmergencyMedicineDiscovery`)・`courier_route_chart`・
`herb_thicket_grounds`(=既存の`kHerbThicketDiscovery`)。

同様に、後半6兵種の製作費が参照する`tack_material`(騎具素材)・`marsh_resin`
(湿地樹脂)・`ruin_fragment`(遺跡片)の3素材も、現状どの地域の戦利品テーブルにも
登場しないため収集手段が無い。Discoveryと同じ「ゲート実装済み・到達不能」扱いとして
記録する。

## M7項目3続き 武器分岐固有効果(基礎〜中程度Tier、2026-07)

前段(「M7項目3続き 武器分岐の全兵種一般化」)で洗い出した、データ・レシピ・UIは
揃っているがエンジン未接続の約24分岐のうち、コスト「安価〜中程度」に分類した17分岐
すべてを接続した。「高コスト(新規サブシステムが要る)」に分類した残りは対象外
(Command Sword/Guard Sword/Fortress Lance/Patrol Lance/Bulwark Maul/旗手3分岐/
Withdrawal Bladeの計8分岐)で、別タスクへ持ち越す。Patrol Lance/Bulwark Maul(待機
トリガーDEF)と旗手3分岐は既存機構との重なりが大きいことを確認済みだが、今回は
スコープに含めない。

### 実装した17分岐

1. **Guard Spear**(迎撃補正+2→+3): 実は前段の一般化作業で`weapon.braceBoost`が既に
   `combatDefenseBonus()`に接続済みだった - 本タスクでの変更なし、確認のみ。
2. **Charge Lance**(移動3+マスで+2): `weaponBranchBonusDamage()`(新規、
   CombatResolver.cpp)が`attacker.tilesMovedThisAction`を見る。
3. **Ambush Blade**(未行動の敵へ+2): `!target.hasActed`で判定。hasActedは
   所有チーム自身のPhase開始時(`beginPlayerPhase()`/`beginEnemyPhase()`)にしか
   リセットされないため、Player Phase中は常に「その敵はまだ今ラウンドのEnemy Phase
   を経ていない」=trueになる - ラウンド定義(Player Phase→Enemy Phaseの順)と整合する
   ため、この読み方をそのまま採用した。
4. **War Bow**(対象が最大HPで+2): `target.currentHp >= target.stats.maxHp`。
5. **Duel Sword**/6. **Quarry Bow**(対象の隣接に同チームの他ユニットがいなければ+2):
   共通ヘルパー`hasAdjacentAllyOfTarget()`で判定(両分岐とも「対象のチームメイトが
   隣接していない」という同一条件)。
7. **Mercy Staff**(Heal 8→12)/8. **Ward Staff**(Heal 6+RES+3)/9. **March Staff**
   (Heal 6+未行動ならMOV+1): `Weapon::healAmountOverride`/`healGrantsResistanceUp`/
   `healGrantsMoveUp`(新規フィールド)を`selectHealTarget()`で参照。RES/MOVバフは
   既存の`resistanceUpActive`/`moveUpActive`をそのまま再利用(ライフサイクルも同じ)。
10. **Repair Hammer**(野戦補修6→9): `Weapon::fieldRepairAmountOverride`(新規)を
    `field_repair`スキル解決コード(`kFieldRepairAmount`を上書き)で参照。
11. **Snare Bow**/12. **Driving Bow**/13. **Ember Focus**(既存onHit効果を戦闘1回に
    ガート): `Weapon::firstHitOnly`+`Unit::weaponFirstHitUsed`(いずれも新規、
    `arcaneOverflowUsed`と同じ「戦闘中1回、リセットしない」ライフサイクル)。
    `applyWeaponOnHitStatuses()`(StatusEffects.cpp)とconfirmAttack()のノックバック
    分岐の両方でこのゲートを見る - 1ユニットが同時に持てる武器は1本だけなので、
    onHitStatuses用/causesKnockback用で共有フラグにしても衝突しない。
14. **Resonant Focus**(戦闘1回目の命中で上下隣接へ固定2ダメージ): `Weapon::
    splashDamage`(新規)+同じfirstHitOnlyゲート。既存の`applyAdjacentSplashDamage()`
    ヘルパー(戦闘魔導士「魔力波及」用)をそのまま再利用。戦闘魔導士は常に
    `hasArcaneOverflow()`も真なので、この武器の初命中では魔力波及(固定3)と
    Resonant Focus本体(固定2)が両方発動し、同じ上下隣接マスへ計5ダメージになる
    (テストはこの合算値で検証)。
15. **Hook Lance**(射程2からの命中で1マス引き寄せ): `Weapon::pullsAtRangeTwo`
    (新規)+`BattleState::applyPull()`(新規、`applyKnockback()`の逆方向版)。
    Heavy Guardの常時無効化・`brace_for_impact`・Hide-Wrapped Gripの
    `knockbackNegatesRemaining`は同じく適用する(「強制的な位置操作を受けない」効果は
    押し出し/引き寄せ問わず一貫させるべきと判断)。ただし衝突時の処理は
    `applyKnockback()`と非対称: ノックバックは詰まった場合によろめきを付与するが、
    Hook Lanceのドキュメント文言「空いていれば」は罰則を含意しないため、Pullは単に
    no-opする。
16. **Trail Blade**(通過した灰地・浅瀬をそのPlayer Phase中だけ味方も移動コスト1):
    `Weapon::trailblazeOnMove`(新規)を`selectMoveTile()`(通常の移動確定)で参照し、
    辺境斥候`trailblaze`スキルと全く同じ`BattleState::markTrailblazed()`/
    `isTrailblazed()`機構を再利用 - 通常移動のたびに自動発火する点だけが違う。
17. **Escort Blade**(再移動終了時、隣接味方1人へDEF+2): `selectReMoveTarget()`
    (再移動の解決点)で、再移動後の位置に隣接する味方1人(複数いれば探索順で最初の
    1人、未規定のタイブレーク)へ`applyDefenseUp()`(行軍隊長`hold_formation`と同じ
    +2・次のEnemy Phase終了までのライフサイクル)を直接適用 - 専用フィールドは
    追加せず既存の`defenseUpActive`を再利用した。

### 実装の簡略化・保留した点

- Tier(a)の条件付きダメージ(2〜6)は、通常攻撃の確定パス(`confirmAttack()`/
  `pendingPreview()`)にのみ接続し、`counterthrust`(反撃準備)や`overwatch`(警戒
  射撃)がプレイヤーユニットの武器で攻撃するケース(`EnemyAI.cpp`)には接続していない
  - これらの武器はプレイヤー専用兵種にしか装備されないため理論上は反撃・警戒射撃でも
    条件が成立し得るが、影響範囲は小さいと判断し今回は見送った。
- **[訂正]** Ambush Bladeの「そのラウンドで未行動」判定は当初`hasActed`をそのまま
  読んでいたが、これは実際にはバグだった: `hasActed`は自チームの次のPhase開始まで
  リセットされないため、Player Phase中は敵の`hasActed`が前RoundのEnemy Phase終了
  時点の値=trueのまま残り続け、`!target.hasActed`が2Round目以降ずっとfalseになる
  (ボーナスが恒久的に不発になる)。`tests/test_battle.cpp`の回帰テストが実際に
  この不具合を再現・検出したため、`Unit::lastActedRound`(新規フィールド、
  `BattleState::markActed()`が現在のRoundを記録)+`weaponBranchBonusDamage()`への
  `round`引数追加で修正した。修正後は「対象敵の最終行動Round ≠ 現在Round」で正しく
  判定され、Player Phase中は(その敵がまだ今RoundのEnemy Phaseで行動していないため)
  常に条件を満たし、Enemy Phase中の反撃等で同一Round内に既に行動済みの敵に対しては
  正しく無効化される。「Player Phase中は事実上常に+2と等価」という性質そのものは
  そもそも正本の「そのRoundで未行動の敵」という文言どおりの挙動であり(Enemy Phaseは
  Player Phaseの後に来るため)、バグではなく仕様。実プレイでの駆け引き感覚の検証は
  引き続き推奨。

### 新規追加フィールド

`Weapon`(include/jf/core/Weapon.hpp): `healAmountOverride`/`healGrantsResistanceUp`/
`healGrantsMoveUp`/`fieldRepairAmountOverride`/`firstHitOnly`/`splashDamage`/
`pullsAtRangeTwo`/`trailblazeOnMove`。

`Unit`(include/jf/core/Unit.hpp): `weaponFirstHitUsed`(戦闘中1回、arcaneOverflowUsed
と同じライフサイクル)。

### テスト・ビルド

`tests/test_battle.cpp`に17分岐ぶんの固有効果テストを追加(スタット入れ替え・
レシピ解放は前段のテストで既にカバー済みのため対象外)。`ctest --test-dir build -j10`
は4/4 Pass。

## M7項目3続き 差分プレビュー(2026-07)

`docs/implementation_roadmap.md`「M7項目3(残り)」の差分プレビューを実装した。
`docs/character_progression.md`「ユニットページ/詳細」の「武器、特性、スキルを選ぶと、
右側へ「現在」「変更後」「変わる戦術」「失うもの」を表示する。長文は折り返し、
ホバーだけに必須情報を置かない」を正本のまま使用し、新しいゲームデザイン数値は
追加していない。

実装した内容:

- `src/ui_facilities.cpp`: `drawForgeEquipmentPanel()`/`drawSkillEquipmentPanel()`が
  候補ボタンへのホバーを検出し、`EquipmentHover`(現在ID/ホバー中IDと種別)を
  `drawUnitScreen()`へ返すよう変更。ホバー中は`drawEquipmentDiffPanel()`が
  ユニット詳細画面下部(比較対象カードと同じ領域)へ2x2グリッドで
  「現在」「変更後」「変わる戦術」「失うもの」を描画する(比較対象カードはホバー中
  ではない時のみ表示。両者は同じ画面領域を共有する一時的な表示補助という位置づけ)
- 武器の差分導出: `Weapon`構造体が既に持つ`might`/`minRange`・`maxRange`/
  `moveModifier`/`causesKnockback`/`braceBoost`/`onHitStatuses`を現在武器とホバー中
  武器で比較。数値差分(威力・射程・MOV補正、+/-符号付き)は「変わる戦術」へ、
  `causesKnockback`/`braceBoost`/`onHitStatuses`から得られる質的タグは、ホバー中側に
  だけあるものを「変わる戦術」の「得る効果」、現在側にだけあるものを「失うもの」へ
  振り分ける
- スキルの差分導出: `SkillDefinition`は数値フィールドを持たないため、`effectEn`/
  `effectJa`の説明文をそのまま「変わる戦術」へ表示し、`category`/`usageType`が
  現在スキルと異なる場合はその変化も追加する。現在スキルが空でなければ、
  「失うもの」へ現在スキルの効果を失う旨を表示する
- 特性(調整特性)は対象外: 現状のUIには`trait_hide_wrapped_grip`の単純な
  装備/解除トグルしかなく、比較対象となる複数の候補を選ぶUI自体が無い
  (`drawForgeEquipmentPanel()`の該当箇所を確認済み)。差分プレビューは「候補を選ぶと
  現在と変更後を比較する」機能のため、比較対象が1つしかない現状のトグルには
  適用対象が無く、新規に特性選択UIを作ることは本スライスの範囲外とした
- `data/locales/{en,ja}.json`に`ui.unit_screen.diff.*`・`skill.category.*`・
  `skill.usage.*`キーを追加(日本語グリフは`loadAppFont()`の
  `allJapaneseGlyphText()`自動収集経由で追加のcharset編集は不要だった)

目視確認: サンドボックス環境にディスプレイが無く(`raylib`起動時に
`GLFW: Failed to determine Monitor to center Window`で即終了)、実機でのGUI往復操作は
未確認。`cmake --build`と`ctest`(既存4スイート、ロケールキー整合性チェック含む)は
成功を確認した。ユニット詳細画面の描画コードはPure描画ロジックであり、既存の
`drawForgeEquipmentPanel()`等にも単体テストが無いため、新規テストは追加していない。

## M7項目3続き 武器分岐固有効果(高コストTier、2026-07)

「M7項目3続き 武器分岐固有効果(基礎〜中程度Tier)」で残した、新規サブシステムが
要る8分岐すべてを接続した: Command Sword(号令剣、隊形補正半径1→2かつ最も近い
2人限定)、Guard Sword(護衛剣、戦闘中1回・隣接味方の最初の被弾-3)、Fortress
Lance(城塞槍、ZoC新規進入で次の行動終了まで与ダメージ-2)、Patrol Lance
(巡回槍、待機で次のPlayer Phase開始までDEF+2)、Bulwark Maul(防壁槌、待機で
自身の次の行動終了までDEF+2、immovable_stance/brace_for_impactと同じ二重フラグ
方式)、Far/Valor/Warding Standard(旗手3分岐、既存`rallyingBannerActive`系の
戦旗補助を半径/対象ステータスでパラメータ化)、Withdrawal Blade(離脱刃、攻撃後
生存していれば対象から1マス離れる方向へ再移動、伝令騎兵`ride_through`の方向制約
版)。これで武器分岐(全12兵種33種)の固有効果はすべてエンジン接続済みとなった。

### 実装中に発見・修正した既存バグ

新規テストの作成過程で、`src/battle/AiSystem.cpp:182`(敵AIの移動候補生成)に
既存の未定義動作(符号付き整数オーバーフロー)を発見した。生存する`Team::Player`
ユニットが1体も存在しない瞬間(例: パーティが全滅した直後の残り敵AI計算)に
`nearest`が`std::numeric_limits<int>::max()`のままとなり、`-nearest *
profile.distanceWeight`の符号反転でオーバーフローしていた。UBSan
(`-fsanitize=address,undefined`)でこの一連のテスト不安定化(実行のたびに異なる
無関係な箇所でassertが失敗する現象)の原因として確認・再現し、`nearest`が
`INT_MAX`のまま(=対象なし)の場合はその移動候補生成自体をスキップするよう修正
した。この修正後、UBSanビルドおよび通常ビルドとも5回連続でtest成功を確認した。
今回の武器分岐機能そのものとは無関係な、既存コードの潜在バグだったため、
本Sliceの副産物として記録する。

### テストの前提ミス(実装ではなくテスト側)

新規テスト作成中に以下の思い込みミスを発見・修正した(実装側は正しかった):

- Command Swordの「最も近い2人」判定テストで、同着(距離2)のタイブレークを
  文字列比較の向きを逆に仮定していた("far3" < "near2"は辞書順で真、逆ではない)
  - 同着に依存しない設計へテストを修正
- Fortress Lanceのテストで、ZoC進入後の状態のまま「基準値」を測っていた
  (既にペナルティが乗った値をbaselineとして使ってしまっていた)
- Patrol Lance/Bulwark Mauleの待機確認は`selectMoveTile()`を経て
  `SelectAction`状態に入ってから`chooseWait()`を呼ぶ必要がある(既存の
  `immovable_stance`テストの手順と同じ)
- Bulwark Mauleは`immovable_stance`/`brace_for_impact`と同じく毎回のWaitで
  再付与される仕様のため、2回目のWaitでは解除を確認できない(別行動種別が必要)
- Withdrawal Bladeのテストで、1人しかいないPlayerユニットが行動を終えると
  Player Phaseがそのまま終了し`EnemyTurn`へ遷移することを見落としていた
  (`SelectUnit`のままという誤った期待だった)

### テスト・ビルド

`tests/test_battle.cpp`に8分岐ぶんの固有効果テストを追加。`ctest --test-dir
build -j10`は4/4 Pass(5回連続実行で安定を確認)、UBSan/ASanビルドでも
クリーン。`git diff --check`成功。

## M7項目3完了 連携作戦(2026-07)

`docs/character_progression.md`「連携作戦」を実装し、M7項目3を完了させた。

### 解放条件の近似

正本の解放条件「交流区画で対応する会話2件を読み、指定地域成果を安全帰還させる」は、
このコードベースに会話/交流区画システムが一切存在しないため、`heavy_recruit`/
`cavalry_recruit`の「加入候補確定」近似(M7項目2、`docs/implementation_status.md`
「M7 12兵種・仲間・会話 実装詳細」参照)と全く同じ簡略化パターンで、「対応する
会話2件を読み」節を落とし、指定地域の安全帰還完了(`BaseState::completedRegionIds`)
のみで判定する(`jf::isCooperationUnlocked()`、`src/battle/Cooperation.cpp`)。
`quarters_social_wing`施設や会話追跡の実装は今回のスコープ外。

| ID | 対応地域 | 到達可能性 |
|---|---|---|
| `paired_fallback_line` | シンダーウォッチ関門(`CinderwatchGate`) | 到達可能 |
| `paired_braced_breakthrough` | 灰鉄採石場(`AshironQuarry`) | 到達可能 |
| `paired_field_recovery` | 黒水低湿地(`BlackwaterLowlands`) - 地点3「薬草洲」単位ではなく地域全体の安全帰還へ近似(地点単位の実績追跡が既存コードに無いため) | 到達可能 |
| `paired_rapid_works` | 風裂き高原(`WindscarPlateau`) | 到達可能 |
| `paired_signal_ward` | 埋没聖堂(Buried Dawn Sanctum) | **到達不能** - `RegionId`自体がこのコードベースに存在せず、地域が全く未実装のため。効果自体は実装済みで、地域が実装され次第そのまま機能する |
| `paired_cross_observation` | 風裂き高原(解放条件のみ) | 実装対象外(下記) |

### `paired_cross_observation`の意図的な未実装

「敵1体を標的指定し、その敵の次回行動候補を同時公開」という効果は、このコード
ベースのどこにも前例が無い。`include/jf/core/Unit.hpp`の`quarryRevealed`
(辺境猟兵`read_quarry`)自身のコメントが、既存の出荷済みスキルでさえ敵AI行動
候補のプレイヤー露出を意図的にスコープ外にしたと明記しており、今回さらに大きい
同種の露出を新規実装するのはこのSliceの規模に見合わない。`jf::CooperationDefinition`
には`hasBattleEffect=false`で登録済み(ID自体は実在・選択可能)だが、
`BattleController::chooseCooperation()`は効果を持たないIDとしてno-opする
(`canUseCooperation()`も`hasBattleEffect`を見て常にfalseを返す)。UIの
連携作戦選択セレクター(下記)もこのIDを候補から除外している。

### 実装した5ペアの効果

- `paired_fallback_line`(帰還線): 両者+どちらかに隣接(距離1)する味方全員へ
  DEF+2、次のEnemy Phase終了まで。専用フラグ`Unit::pairedFallbackLineActive`
  (`rallyingBannerActive`と同型、数値が異なるため使い回さず新設)。
- `paired_signal_ward`(灯火の結界): どちらかのペアから距離2以内の味方全員へ
  RES+2、同じく次のEnemy Phase終了まで。専用フラグ`pairedSignalWardActive`。
- `paired_field_recovery`(野外救護): 距離2以内の味方1人を8回復し、毒か炎上の
  どちらか1つを解除。両方存在する場合はどちらを優先するか正本が指定していない
  ため、毒を優先する近似を採用(ドキュメント化済み)。既存のHeal解決パス
  (`canHeal()`/`selectHealTarget()`)とは別の専用コマンドとして実装(装備
  スキル2枠を消費しないため)。
- `paired_braced_breakthrough`(支え合う突破): 両者へ強制移動無効
  (`BattleState::applyKnockback()`/`applyPull()`/`resolveWindGustRoundEnd()`の
  既存`hasHeavyArmor()`/`braceForImpactActive`チェックへ新規フラグ
  `pairedBracedBreakthroughActive`を追加)と、突撃してきた敵(`tilesMovedThisAction
  >=2`)へのDEF+2(`BattleState::combatDefenseBonus()`の既存Brace系チェックと
  同じトリガー、独立加算)を付与、次のEnemy Phase終了まで。
- `paired_rapid_works`(迅速工作): 発動者から距離2以内の空きマスへ、既存の
  `rapid_barricade`Definition(辺境工兵「野戦補修」と共有、耐久6)をそのまま
  `BattleState::placeObject()`で戦闘中に新規配置。カエル(`cavalry_recruit`)が
  正本の指示どおり(発動者がどちらであっても)通常の再移動(予算2)を行える -
  既存の`SelectReMoveTarget`(`markActionResolved()`で行動済み化する)は再利用
  できない(相方が行動済みになってしまう)ため、専用の
  `BattleInputState::SelectCooperationCavalryReMoveTarget`
  /`selectCooperationCavalryReMoveTarget()`を新設し、移動のみ行い行動済み化
  しない。

### 保存データ・戦闘スコープ

`SaveData::equippedCooperationId`(squad-wide、単一スロット - `unitWeaponOverrides`
等の人物別mapとは別扱い)をSchema v2→v3として追加(`kCurrentSaveSchemaVersion`を
3へ、`migrateSave()`に空文字列defaultのv2→v3ステップを追加)。戦闘中の使用済み
判定は`BattleState::cooperationUsedThisBattle()`(`collectedHerbPatches_`等と同じ
battle-scoped、保存されない)。

### UI

- 戦闘: 既存のInteract用6番目Slotを共有する形で「連携作戦」ボタンを追加
  (`ui_battle.cpp`) - Interactが使える時はそちらを優先表示(同一ターンで両方
  使えるコンテンツが無いため実害なし)。対象選択が必要な2ペア
  (`paired_field_recovery`/`paired_rapid_works`)は専用のタイル強調表示、
  Backボタンで`cancelAttackSelection()`へ復帰できるよう同関数の許可Stateへ
  `SelectCooperationTarget`を追加。
- 遠征前準備: `ui_base.cpp`の`drawBaseBagAndExpedition()`にBag欄の下へ
  「連携作戦」1行セレクターを追加 - クリックで{未装備, 解放済みかつ
  `hasBattleEffect`なペア}を巡回選択する(選択肢が最大5+未装備の6択のため、
  専用ピッカー画面は今回作らなかった)。

### テスト・ビルド

`tests/test_battle.cpp`へ、5ペアの効果(帰還線のDEF+2波及、支え合う突破の
ノックバック無効+対Brace DEF+2、野外救護の回復+状態解除、迅速工作の
バリア設置+カエル専用再移動)・`paired_cross_observation`のno-op・
`isCooperationUnlocked()`の地域ゲーティングを検証するテストを追加。
`ctest --test-dir build -j10 --output-on-failure`は3回連続実行で4/4 Pass
(フレーキーなし)。`git diff --check`成功。

## 次の優先候補

1. Phase 3.5実装順7: 上記で実装済みのBase画面地域選択・Exploration画面分岐・安全路/
   再調査ボタンを、実機でのGUI往復操作（出発→勝利→帰還→再出発→安全通過/再調査）で
   目視確認する（サンドボックス環境のため未確認）
2. Phase 4手順6〜7: 薬草の沢の探索3択、薬草、増援、踏査、薬草マス回復
3. 状態異常・スキルを実際に付与する攻撃・スキル効果の実装（データ基盤のみ先行済み）
4. Web保存の同期完了状態と保存HUD、破損復旧の専用選択画面
5. Schema 2以降のスキーマ移行基盤
6. 施設ID、レシピ、装備の読込検証強化
