# JOJIFrontier 全体実装ロードマップ

文書種別: **進捗記録**

仕様索引: [`README.md`](README.md)

更新日: 2026-07-23

この文書は実装順、依存関係、品質Gateだけを管理する。ゲーム仕様、数値、安定IDを新規定義しない。
仕様変更は[`README.md`](README.md)で指定された正本へ行い、現在の実装詳細は
[`implementation_status.md`](implementation_status.md)へ記録する。

## 現在地

Milestone進行(M0〜M6は完了/概ね完了、M8-Aは未実装、詳細は各章参照):

- M6 沈黙した監視所群: 完了(M6-A/B/C/D)。6地点すべて実コンテンツ化済み、最低保証報酬・
  灰鉄採石場(最小プレースホルダー)解放・4〜6周実測まで完了
- M7 12兵種・仲間・会話: 項目1(後半6兵種)完了。項目2(加入経路)は5/6兵種
  (重装兵・辺境工兵・伝令騎兵・旗手・辺境猟兵)完了、残り1兵種(戦闘魔導士、灰鉄採石場の
  本格攻略に依存)は未着手。項目3(ユニットページと装備共有)は武器重複装備の防止+
  装備スキル2枠UI(全兵種)+探索能力表示+比較パネル+差分プレビューまで完了、
  一覧(加入候補非表示は既存実装で対応済みと確認)/特性・武器分岐の他兵種拡張
  (全12兵種・33分岐、固有効果エンジン接続含め2026-07完了)も完了。残るは連携作戦
  (新規戦闘メカニクス)のみ
- M9 残り8地域: 灰鉄採石場(第3地域)は**完了(M9-A/B/C/D/T)**。全5地点+地域ボス+
  地点3A/3B双方が実コンテンツ化済み。地点3A「旧採掘坑」・地点4「灰鉄鉱脈」は
  M9-Iで新設したゲストユニット護衛サブシステムを使いM9-Tで実装した(戦闘魔導士
  イリエンの加入経路`mage_recruit`も同時に配線済み、M7項目2の残り1兵種が解消)。
  黒水低湿地(第4地域)着手、
  M9-E(骨格7地点+地点1「灰水の沈み道」)・M9-F(地点2「葦原の分岐」)・M9-G
  (地点3「薬草洲」、`primarySurviveRoundsAlternative`を新規実装)・M9-H(地点4
  「樹脂林」、コード変更無し)完了。地点3/4の`AllMembers`分岐は両方確保済みで解決、
  キャンプII以降へ進行可能。「採取者を撤退させない」副目標/敗北条件は、地点3A/4と
  同じゲストユニット系ギャップとして意図的に見送り。M9-I(地点5「黒水渡し」)は主目的
  そのものがこのギャップに依存していたため、今回はゲストユニット護衛サブシステム
  (`Unit::isGuest`、`BattleMissionState::guestUnitIds`、`BattleState::allGuestsLost()`、
  `StageDescriptor::guestUnits`/`primaryEscapeUnitsAlternative`)を新規実装したうえで
  地点5を正本どおり実コンテンツ化(完了、2026-07)。詳細は`implementation_status.md`
  参照。「荷物を減らして渡る」の持込品一時封印、`[伝令騎兵]`ルートの護衛対象MOV+1・
  増援位置公開は該当インフラが無いため引き続き見送り。M9-J(地点6「沈没水門」)は
  signal_tower実装済みの`objectPlacementRules`+`operateObjectiveId`機構がそのまま
  収まったため`data/regions.json`のみで実コンテンツ化(完了、2026-07)。主目的の
  「水門操作」と「2ラウンド防衛」のAND結合、副目標(制御輪保全/毒罠処理)、
  「水門本体の耐久0」敗北条件は該当インフラが無いため引き続き見送り。M9-K(地点7
  「深泥の水源」/地域ボス「沼牙の大蛇」)で黒水低湿地(第4地域)の全7地点が完了
  (完了、2026-07)。M9-Dの`takeGrubwormBossTurn()`と同じ構造で新規`UnitClass::
  MarshFangSerpent`のボスAIを実装、主目的のAND合成はM9-Dと同じ理由で
  `EliminateTeam`のみへ近似。地域の最低保証報酬(M6-Dと同型の`blackwaterMaterials
  Earned`フロア)・診療所「薬学」/工房「罠技術」の解放条件配線・風裂き高原(第5地域、
  当時`RegionId::WindsweptHighland`、最小プレースホルダー)の追加まで完了。M9-Lで
  `RegionId::WindscarPlateau`/`"windscar_plateau"`へ改名(他地域のenum名/toString()
  一致規約に合わせる是正)のうえ、地域骨格(6地点+2キャンプ+地点3・4のどちらを
  先に攻略してもよい分岐)+地点1「風下の登り口」を実コンテンツ化し、正本が
  「実装順」1番目に挙げる新規地形機構「強風」(`TerrainType::WindGust`+
  `BattleState::resolveWindGustRoundEnd()`)を実装(完了、2026-07)。地点1の
  fresh-party win率(Direct 56.0%)が先行区域の地点1(軒並み99〜100%)より明確に
  低い実測が出ており、次のSlice着手前にバランス調整の要フォローとして記録済み。
  詳細は`implementation_status.md`参照。M9-M(地点2「崩れた中継路」)で
  `StageDescriptor::guestUnits`/`primaryEscapeUnitsAlternative`(M9-Iで実装済みの
  護衛サブシステム)を流用し実コンテンツ化(完了、2026-07)。主目的「護衛対象を
  右端へ脱出、または敵全滅後に橋を操作」はEscapeUnits単独へ近似、木橋耐久系
  (副目標/工兵ルート効果/敗北条件の一部)はObject耐久機構が未実装のため引き続き
  見送り。新素材`riding_gear`(騎具素材)追加。詳細は`implementation_status.md`参照。
  M9-N(地点3「風見台」)はsignal_tower実装済みの`objectPlacementRules`+
  `operateObjectiveId`機構(2 Object版)がそのまま収まったため`data/regions.json`
  のみで実コンテンツ化(完了、2026-07)。敵は`WatchArcher`/`Spearman`/
  `MessengerCavalry`を再スキンなしで使用。OR条件(敵全滅後の代替操作)・
  4ラウンド以内副目標(Round数に基づく報酬条件の機構が無い)・風見盤破壊による
  敗北条件(Object耐久機構未実装)は該当インフラが無いため引き続き見送り。
  詳細は`implementation_status.md`参照。M9-O(地点4「分断された輸送隊」)は
  地点2「崩れた中継路」と同じ`StageDescriptor::guestUnits`/
  `primaryEscapeUnitsAlternative`パターンで実コンテンツ化(完了、2026-07)。
  主目的「負傷者1人以上を脱出、または荷物箱1個以上を確保」はEscapeUnits単独へ
  近似(荷物箱側はObject耐久機構未実装のため未モデル化)、敵は既存
  `WatchArcher`+`Bandit`(「Raider」表示名)を再利用。地点3・4の両方が本格
  コンテンツになったことで、`windwatch_convoy_branch`(`AllMembers`)経由の
  キャンプII解放ゲートが正しく機能する状態になったことをテストで確認。
  詳細は`implementation_status.md`参照。M9-P(地点5「断崖荷車道」)は地点2/4と
  同じ`StageDescriptor::guestUnits`/`primaryEscapeUnitsAlternative`パターンで
  実コンテンツ化(完了、2026-07)。主目的「荷車または護衛対象1人以上を右端へ
  脱出」は正本のOR自体がEscapeUnits単独と機能的に一致するためこれまでで
  最も近似度が高く実装できた。荷車は既存GuestUnitData1体で近似(耐久12という
  数値自体は未モデル化)、ルート3の強風移動無効は`hasHeavyArmor()`が
  ユニット種別を問わない汎用判定であるため技術的には成立しうるが、荷車に
  実戦闘クラスのステータスを与える副作用を避けて見送り。敵は既存
  `MessengerCavalry`/`Spearman`/`WatchArcher`を再スキンなしで使用。
  詳細は`implementation_status.md`参照。M9-Q(地点6「高原伝令所」/地域ボス
  「高原運び手の隊長」/地域攻略)で風裂き高原(第5地域)の全6地点が完了
  (完了、2026-07)。M9-Kの`takeSerpentBossTurn()`と同じ構造で新規`UnitClass::
  PlateauCourierCaptain`のボスAIを実装(通り抜け攻撃・迂回命令・退路確保の
  3固有行動)、主目的のOR合成はM9-D/Kと同じ理由で`EliminateTeam`のみへ近似
  (ScriptedWithdrawal撤退)。地域の最低保証報酬(M9-Kと同型の
  `windscarMaterialsEarned`フロア)・旧辺境集落(第6地域、最小プレースホルダー)
  の追加まで完了。地点6のfresh-party win率(Direct/Tactical共に0.0%、avg KO
  4.00)は他地点の既知のシミュレータ盲点(OperateObject/EscapeUnits非対応)とは
  異なり純粋な`EliminateTeam`地点での実測のため、次のSlice着手前にバランス調整の
  要フォローとして記録済み。詳細は`implementation_status.md`参照。M9-U〜Xで
  旧辺境集落(第6地域)の地域骨格+地点1〜4(風化した外柵/共同井戸/旧穀物庫/
  集会家屋)を実コンテンツ化した後、M9-Y(地点5「夜明けの共同防衛」/強敵
  「襲撃団頭領」/地域攻略)で**旧辺境集落(第6地域)は完了**(全5地点、2026-07)。
  正本自身が「頭領撃破ではなく共同防衛が目的」と明記する非ボス構成のため、
  bespoke boss AIを実装せず既存`AiProfile`調整のみで対応(M9-K/Qの真ボスより
  軽量な初めてのケース)。地域の最低保証報酬(`settlementMaterialsEarned`フロア)・
  燼火峡谷(第7地域、`RegionId::EmberRavine`、最小プレースホルダー)の追加まで
  完了。実装過程で`ObjectiveTracker.cpp`(primary=false・Live評価の
  OperateObjectが永久にCompletedへ遷移できない)・`ExpeditionService.cpp`
  (`computeRegionSummaries()`がOldFrontierSettlementを一覧から欠いていた)の
  2件の既存バグを発見・修正した。詳細は`implementation_status.md`参照

M9-Z(燼火峡谷・第7地域: 地域骨格 + 地点1「焼け石の入口」 + 炎上床/冷却床/
噴気予告床/戦場熱量)完了(2026-07)。8地点+3キャンプ+地点3・4のどちらを先に
攻略してもよい分岐が到達可能な状態になり、地点1が実コンテンツ化された
(`data/regions.json`のJSON定義のみで完結、Region.cpp側の手書き関数は不要)。
この地域の新規機構「戦場熱量」(`BattleState::heatLevel()`、戦闘ごと0〜3の
共有値)と新規地形6種(焼け石床/熱砂/炎上床/冷却床/噴気予告床/灰煙床)を
「実装順」1番目どおり最優先で実装し、フレーバーとして見送らなかった
(Windscarの強風地形と同じ「新地域+新規地形/戦闘状態メカニクスを同じSliceで
実装する」前例を踏襲)。敵勢力「岩蜥蜴」の「各戦闘最初の炎上付与だけ無効」は
新規`Unit::firstBurnNegatesRemaining`(新規UnitClassは追加しない)で表現した。
詳細は`implementation_status.md`のM9-Z参照。

M9-AA(燼火峡谷・第7地域: 地点2「熱風の棚道」)完了(2026-07)。護衛対象を
右端へ脱出させる主目的(`guestUnits`/`primaryEscapeUnitsAlternative`、
windscarRelayStage()前例のRegion.cpp手書き)、敵勢力「熱地採取団」
(Bandit/WatchArcher再利用reskin)、新素材`sulfur`(硫黄)を実装。
「冷却床2マス追加」(ルート単位地形上書き未実装)とキャンプIの
炎上解除演出(キャンプ到達時ステータス書き換えフック未実装)は既知ギャップ
として見送り。詳細は`implementation_status.md`のM9-AA参照。M9-AB(地点3
「硫黄窪地」)完了。`StageDescriptor::secondaryProtectUnitAlternative`を新設し
`ObjectiveKind::ProtectUnit`を初めて実際に生成・トラッキングするようにした
(副目標「採取者を撤退させない」) - 黒水低湿地site3/4・灰鉄採石場site4
「灰鉄鉱脈」で繰り返し見送られてきた「ゲストユニット系ギャップ」をこの地点に
限り解消した。詳細は`implementation_status.md`のM9-AB参照。M9-AC(地点4
「破損冷却水路」)完了。`sunken_sluice`(M9-J)と同型のOperateObject単独近似
(`data/regions.json`のJSON定義のみで完結、Region.cpp側の手書き関数は不要)。
主目的のSurviveRounds(2)とのAND合成・副目標「水路耐久8以上」・敗北条件
「水路耐久0」はObject耐久追跡機構の欠如により見送り。地点3・4の両方が完了した
ことで`ember_ravine_camp2`(キャンプII)が実際に到達可能になった。詳細は
`implementation_status.md`のM9-AC参照。M9-AD(地点5「灰晶採取棚」)完了。
主目的「灰晶箱1個以上を確保」はM9-H(黒水低湿地地点4)と同型のcrate-primary
近似(標準EliminateTeam+`surveyObjectiveId`のsecondary/bonus-reward)、副目標
「採取地点2個を操作」は`ObjectPlacementRule::secondaryOperateObjectiveId`
(M9-Yで新設・Region.cpp手書き専用だった)を初めてJSON Schema
(`GameData.hpp`/`GameData.cpp`)へ露出させて配線した。新素材`ash_crystal`
(灰晶)追加。深部ルート専用の「大型個体」はper-unit stat補正機構が無いため
同stat5体目の追加で近似、噴気予告1回無効(ルート3)は前例のないper-route
無効化機構のため見送り(no-op)。詳細は`implementation_status.md`のM9-AD参照。
M9-AE(地点6「旧耐熱工房」+ キャンプIII到達可能化)完了。主目的「冷却炉を
停止し、記録箱1個以上を確保」はM9-AC/`sunken_sluice`と同型のOperateObject
単独近似、「記録箱2個: 特殊鍛造記録」ボーナス階層はBlackwater地点5(M9-I)の
`creditedTargetIds.size()>=2`と同型のad-hocチェック(crateはSecureTile系の
ためグループ完了スキャンへ読み替え)で新規Discovery
`kSpecialForgingRecordsDiscovery`を付与。地点3・4・6が実装済みとなった
ことで`ember_ravine_camp3`(キャンプIII)が実際に到達可能になった。
キャンプIIIの「以後の戦闘開始熱量を1下げる」効果は、既存キャンプがいずれも
「到達ゲート」か「1回きりの回復」止まりで「遠征スコープで持続する数値修飾」を
保持する仕組みがこのプロジェクトに存在しないため、新規ギャップカテゴリとして
見送り。詳細は`implementation_status.md`のM9-AE参照。
M9-AF(地点7「灰封観測所」)完了。主目的「観測記録箱2個のうち1個以上を左端へ
運ぶ」は`ash_crystal_shelf`/`heatwork_shop`と同型のEliminateTeam-primary +
crate secondary近似(「運ぶ」は未実装、「確保」のみモデル化)、「2個とも
回収」ボーナス階層はM9-AEと同型のad-hocグループ完了スキャンで、新規
Discovery`kEmberRavineSurveyRecordsDiscovery`/`kPreAshstormWatchRecordsDiscovery`
の2件を同時付与。6ラウンド制限・ルート別記録箱数変更はいずれも既知ギャップ
(round-limit-as-defeat機構・per-route surveyTileCount変更機構がこのプロジェクト
に存在しない)として見送り。詳細は`implementation_status.md`のM9-AF参照。

M9-AG(燼火峡谷・第7地域: 地点8「赤熱裂け目」/地域ボス「赤背の大蜥蜴」/地域攻略)
完了(2026-07)。地点1〜7に続き最終地点+地域ボスを本格実装し、燼火峡谷(第7
地域)は全8地点が実コンテンツ化された。ボスはM9-D/K/Qと同じbespoke
`takeRedbackLizardBossTurn()`(熱砂突進/尾払い/噴気誘導)、主目的は標準
`EliminateTeam`近似+ScriptedWithdrawal、副目標「冷却弁2個両方操作」は
`secondaryOperateObjectiveId`で実装、「炎上状態0」は`deep_mire`と同型の
ad-hocボーナスで`controlled_ember_formula`を付与。地域攻略・最低保証報酬フロア
(`emberRavineMaterialsEarned`)・鍛冶場/工房の3研究ノード追加・埋没聖堂
(第8地域、`RegionId::BuriedDawnSanctum`、最小プレースホルダー)の追加まで
完了。これにより`paired_signal_ward`が到達可能になった。詳細は
`implementation_status.md`のM9-AG参照。

M9-AH(埋没聖堂・第8地域: 地域骨格 + 地点1「埋没参道」)完了(2026-07)。新規
戦闘メカニクスを導入しない地域という正本自身の明記どおり、M9-U(旧辺境集落)を
最も近い前例として踏襲した。6地点+2キャンプ+地点3・4「順序選択」(救護室/写本庫、
どちらを先に攻略してもよいが両方必須)を`RouteGraph.cpp`へ既存の
`BranchCompletion::AllMembers`で配線、地点1「埋没参道」を`data/regions.json`のみで
実コンテンツ化(聖堂回収団4体、建築材2/石材1、ルート2「全員HP-2で迂回」は
`partyDamage`、ルート3`[重装兵]`「梁を支える」は`scoutRouteRequiredClass:
HeavyInfantry`)。新規`buried_dawn_sanctum`TerrainProfileを追加、正本の6地形を
既存5 TerrainTypeで近似(石床→Floor、崩土→Rubble、礼拝床→WatchPost(RES+2の
代わりにDEF+2、地形単位のRES加算機構自体が未実装のためM9-Uの「低い石垣」近似と
同型)、薬草保管床→HerbPatch(既存の「行動終了時HP5回復、各マス1回」機構がそのまま
一致)、封鎖床/崩壁→Barrier(「装置操作まで通行不可」はObject耐久/装置連動機構
自体が未実装のため、単純な通行不可Barrierへ近似))。材料は`building_material`/
`stone`とも既存(Old Frontier Settlement/Ashiron Quarryで登録済み)のため新規登録
不要。副目標「参道支柱2本保全」→建築材+1はObject耐久機構未実装のため見送り
(M9-U以来の既知ギャップと同型)。地点2〜6は旧`buried_dawn_sanctum_outpost`
単一地点スタブに代わるBandit x2(-3)最小プレースホルダー。詳細は
`implementation_status.md`のM9-AH参照。

直近の未完了(優先度順):

1. 埋没聖堂(第8地域)の残り地点(2〜6)の本格実装(M6-B/C・M9-A〜AHと同じ
   「骨格→1地点ずつ」方式)。地域骨格+地点1「埋没参道」はM9-AHで完了済み。
3. M7項目3 完了(2026-07)。連携作戦(新規戦闘メカニクス、
   `docs/character_progression.md`)を実装 - 6ペア中5ペアに実戦闘効果を実装
   (`paired_fallback_line`/`paired_signal_ward`/`paired_field_recovery`/
   `paired_braced_breakthrough`/`paired_rapid_works`)、`paired_cross_observation`
   のみ意図的に未実装のまま登録(理由は`implementation_status.md`「M7項目3完了
   連携作戦」参照)。解放条件は「対応する会話2件を読み」節を落とし、指定地域の
   安全帰還完了へ近似(heavy_recruit/cavalry_recruitの「加入候補確定」近似と
   同型)。`paired_signal_ward`は対応地域(埋没聖堂)自体が当時未実装のため
   当時は到達不能だったが、M9-AGが`RegionId::BuriedDawnSanctum`を追加したことで
   到達可能になった(詳細はM9-AG参照)。特性・武器分岐の他兵種一般化(2026-07完了)・
   差分プレビュー(2026-07
   完了)・武器分岐の固有効果エンジン接続(全33分岐、2026-07完了)と合わせて
   M7項目3は全面完了。
4. 風裂き高原の残バランス課題: 地点2・4・5は主目的がシミュレータの`ObjectiveKind`
   盲点[EscapeUnits]に、地点3・6(OperateObject補助部分)は[OperateObject]に
   該当するため、実測win率が実プレイより低く出る既知の構造的偏り(地点1・地点6
   ボス本体は2026-07に調査・対応済み、詳細はM9-R/S参照)

M9-U(旧辺境集落・第6地域: 地域骨格 + 地点1「風化した外柵」)完了(2026-07)。
5地点+2キャンプ+地点3・4のどちらを先に攻略してもよい分岐が到達可能な状態になり、
地点1が`extraBarrierCount`/`scalesWithExtraBarrierOutcome`機構(M6以来実在、
本地域で初めて`[辺境工兵]`ルートの「防護柵1個追加」に使用)で実コンテンツ化
された。新素材`building_material`(建築材)/`food`(食料)追加、新規
`old_frontier_settlement`TerrainProfile追加(既存5 TerrainTypeで正本の地形生成表を
近似)。外柵耐久系の副目標・保全報酬はObject耐久機構未実装のため引き続き見送り。
詳細は`implementation_status.md`参照。地点2〜5(共同井戸/旧穀物庫/集会家屋/
夜明けの共同防衛)は引き続きプレースホルダー。

M9-V(旧辺境集落・第6地域 地点2「共同井戸」)完了(2026-07)。主目的は
`primarySurviveRoundsAlternative`(3ラウンド防衛)の再利用、副目標「中立住民を
全員避難」は新規`StageDescriptor::secondaryEscapeUnitsAlternative`により
近似ではなく実際の独立Secondary Objective(primaryグループと衝突しない別
ObjectiveGroup)として実装できた。guestUnits(中立住民)は全3ルート共通4人固定
(既知の限界、ルート2の正本2人は未モデル化)。敵は`Bandit`/`WatchArcher`の
既存クラス再利用、井戸優先ルートのみ斧兵+1(加算後減算パターン)。井戸耐久系の
副目標・敗北条件・恒久成果`settlement_well_agreement_reached`はObject耐久機構
未実装/地域完了恒久化機構未実装のため引き続き見送り。詳細は
`implementation_status.md`参照。地点3〜5(旧穀物庫/集会家屋/夜明けの共同防衛)は
引き続きプレースホルダー。

M9-W(旧辺境集落・第6地域 地点3「旧穀物庫」)完了(2026-07)。地点1と同じく
`guestUnits`不要のため`data/regions.json`のみのJSON-authoredで実コンテンツ化
(Region.cppの手書きステージ関数は不要)。主目的は`primarySurviveRoundsAlternative`
(4ラウンド防衛)、副目標「食料箱2個を保持」は`surveyObjectiveId`+`surveyTileCount`
のcrateパターン(ironwatch_stores以来)を再利用、3ラウンド目の軽装剣士1体の予告
増援は`TimedReinforcementData`(herb_islet以来)を再利用。ルート3「敵増援なし」は
`TimedReinforcementData`がStage全体固定でルート別に無効化する機構が無いため見送り
(既知の限界、既存地点にも前例なし)。穀物庫耐久系の探索効果・副目標・敗北条件・
恒久成果`settlement_granary_shared`はObject耐久機構/地域完了恒久化機構が未実装の
ため引き続き見送り。詳細は`implementation_status.md`参照。地点4〜5(集会家屋/
夜明けの共同防衛)は引き続きプレースホルダー。

M9-X(旧辺境集落・第6地域 地点4「集会家屋」)完了(2026-07)。地点1・3と同じく
`guestUnits`不要のためJSON-authoredで実コンテンツ化。主目的「記録箱2個以上」は
`surveyTileCount`が真のN-of-M閾値ではなくAny-of-N(M9-H「樹脂箱」前例と同型の
既知の限界)なため、標準EliminateTeamで近似(正本のOR条件「敵全滅」と自然に一致)。
記録箱はルート別個数(3/2/3)を出し分けられない既知の限界により固定3個で近似、
Discovery報酬`settlement_command_ledger`は`stage.discoveries`で無条件付与。
guestUnitsは正本のルート1・2に一切無くルート3のみ導入する非対称構成のため、
M9-Vの「全ルート共通4人固定」近似とは異なり、本地点では実装しない判断とした
(ルート3の「中立住民2人が記録箱を運搬」・副目標「運搬人撤退させない」・報酬
「織物1」は未配線のflavor textとして記録)。ルート2「防護柵1個追加」は
`extraBarrierCount`機構で実装(flavor/no-opではない)。地点3・4がともに実
コンテンツ化されたことで、`RouteGraph.cpp`の`settlement_granary_hall_branch`
(`BranchCompletion::AllMembers`)経由のキャンプIIが実際に到達可能になった。
記録箱耐久系の敗北条件・恒久成果`settlement_ledger_restored`はObject耐久機構/
地域完了恒久化機構が未実装のため引き続き見送り。詳細は`implementation_status.md`
参照。地点5(夜明けの共同防衛+地域ボス「襲撃団頭領」)のみ引き続きプレースホルダー。

M9-Y(旧辺境集落・第6地域 地点5「夜明けの共同防衛」/ 強敵「襲撃団頭領」/ 地域攻略)
完了(2026-07)。地域の最終地点+地域攻略で、旧辺境集落(第6地域)の全5地点が
実コンテンツ化された。正本の主目的AND合成(3サブ条件)は`primarySurviveRoundsAlternative`
(5ラウンド防衛)をそのまま再利用、井戸/穀物庫耐久サブ条件はObject耐久機構未実装
のため見送り、警鐘操作サブ条件は新規`ObjectPlacementRule::
secondaryOperateObjectiveId`により独立Secondary Objectiveとして実装(主目的は
左右しない)。実装中に`ObjectiveTracker.cpp`の既存バグ(primary=false・
Live評価のOperateObjectが永久にCompletedへ遷移できない)を発見・修正した。強敵
「襲撃団頭領」(新規`UnitClass::RaidLeader`)は正本が明記する非ボス構成のため
bespoke boss AIを実装せず、既存`AiProfile`のretreatHpPercent調整のみで対応
(M9-K/Q系の真ボスより軽量)。2波の増援のうち1波のみ`timedReinforcement`で実装
(単一`optional`フィールドの制約、新規制限として記録)。副目標「中立住民を全員避難」
(織物2)・「頭領撤退」(鉄材1)・「味方戦闘不能者0」(建築材1)はいずれも実装、
地域の最低保証報酬フロア(`settlementMaterialsEarned`)・燼火峡谷(第7地域、
`RegionId::EmberRavine`)の追加まで完了。実装過程で`ExpeditionService.cpp`の
`computeRegionSummaries()`がOldFrontierSettlementを欠いていた既存バグ(M9-U以来、
Base画面の地域一覧に一度も出現していなかった)も発見・修正した。詳細は
`implementation_status.md`参照。

M1-E「M9前ブロッカー」4項目は全て対応済み(3項目完了・1項目は現状ニーズ無しと判断し
意図的に見送り)のため、M9(残り8地域)へいつでも着手できる状態。

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
11. 既存Mechanicの組み合わせで作れるユニット、地点、地域はDefinition追加だけで実装し、
    `GameApp`、`BattleController`、`main.cpp`へコンテンツ名による分岐を増やさない。
12. 新しいMechanicだけをC++で実装し、追加時には安定ID、Definition Schema、Validation、
    Seed再現テストを同じSliceで用意する。

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
                      -> M8-A 商店
                         -> M8 遠征経済完成
                            -> M9 残り8地域
                               -> M10 公開品質
```

M1以降、Schema・Locale・テストは独立した後工程にせず、各Sliceへ含める。M3は既存負債の一括解消と
更新耐性の完成を担当する。

M1-Eは一度に置き換えない。Definition Schema、Validation、地形・Encounter移行はM1の共通契約完成後に
着手し、AI ProfileとBoss Runtime State移行はM4の共通化を利用する。M6では新しい地点を可能な範囲で
M1-E経由に載せ、M9開始前にM1-Eの全完了Gateを通す。

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
| 新規 | M8-A 商店 |
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

状態: **当時の「直近の実装順」記載の4項目(UnitDefeatedEvent、Phase終了状態処理、Batch後1回の
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

状態: **項目1〜5全て完了。項目4は`DestroyObject`/`SurviveRounds`/`EscapeUnits`/
`ProtectUnit`/`OperateObject`(破壊・防衛・脱出・護衛・装置操作、いずれも2026-07)が
出揃った(M1-B完了 - `mission_objectives.md`が定める残り2種SecureTiles/
DefeatWithConditionはM1-Bの当初スコープ外)**

1. Team制約付き`SecureTile`
2. 存在検証付き`DefeatUnit`
3. OR未達側の`Superseded`
4. 防衛、護衛、脱出、破壊、装置、Round生存
   - **完了(2026-07)**: 破壊 = `ObjectiveKind::DestroyObject`を追加。`DefeatUnit`と同じ
     Live評価パターン(対象`BattleObjectState`が`Destroyed`かを都度確認、Event消費なし)。
     未知Object ID・`canBeAttacked=false`なObjectへの参照は`validateBattleMission()`が
     起動時に拒否。出荷済み地域書側の接続(既存の`fallen_log`等を対象にした実際のMission)は
     まだ無く、`jf/battle/Objective.hpp`/`ObjectiveTracker.cpp`のAPIとしてのみ存在する
   - **完了(2026-07続き)**: 防衛/Round生存 = `ObjectiveKind::SurviveRounds`を追加。
     `docs/mission_objectives.md`の定義「指定ラウンド終了まで敗北条件を回避」通り、
     `battle.round() > target.surviveUntilRound`(到達ではなく超過)で判定するLive評価。
     「敗北条件を回避」の部分は追加ロジック不要 - `evaluateBattleOutcome()`が
     `allPlayersDefeated()`をどのPrimary Groupよりも先に評価するため、敗北していれば
     このObjectiveの充足有無自体が意味を持たない。`surviveUntilRound < 1`
     (`round_`の初期値1に対し即座に満たされてしまう)は起動時検証で拒否。
   - **完了(2026-07続き)**: 脱出 = `ObjectiveKind::EscapeUnits`を追加。`SecureTile`と同じ
     `ActionResolvedEvent`credit機構(handleObjectiveEvent())を再利用し、既存の
     `ObjectiveProgress::creditedTargetIds`(Set)が`target.requiredEscapeCount`件の
     異なるUnit IDに達した時だけCompletedにする拡張のみで実装できた(同一Unitが複数回
     行動終了しても1件のまま)。`docs/mission_objectives.md`が挙げるもう一方の経路
     (`UnitRetreated`Event経由 - AI/撤退駆動でExitPointから実際に離脱する場合)は対象外とし、
     「脱出マスで行動終了」という主定義のみ実装した。
   - **完了(2026-07続き)**: 護衛 = `ObjectiveKind::ProtectUnit`を追加。他Kindと違い常に
     副目標専用(`docs/mission_objectives.md`の表通りprimary=No固定)で、「満たされている」が
     戦闘開始時からのデフォルト状態、崩れた瞬間だけ捕まえる「立ち下がりEdge」という構造が
     他Kindと逆になる点が実装の要点だった。既存の`syncObjectiveProgress()`のPrimary Group
     走査は「満たされた瞬間にCompletedへ遷移する立ち上がりEdge」を前提にしているため、
     素朴にそこへ混ぜるとSync 1回目で即Completedになってしまうバグを生む。専用の別Passを
     新設し、`Active→Failed`のみを行う一方向遷移(対象がisPresent()でなくなった瞬間に
     Failedへロックインし、以後isPresent()に戻っても復帰しない)として実装。`Completed`
     には自分からは決して遷移させない設計とし、「勝利時にActiveのまま残っていれば護衛成功」
     という規約はこれを消費する側(まだ未接続)に委ねた。ProtectUnitがprimary=trueに
     誤指定された場合(Primary Group側の立ち上がりEdge評価に混入する実害がある)は
     `validateBattleMission()`が拒否する。
   - **完了(2026-07続き)**: 装置操作 = `ObjectiveKind::OperateObject`を追加。`DestroyObject`と
     同じLive評価パターンで、対象`BattleObjectState::interactionCount > 0`を見るだけ
     (Event経由の配線は不要)。`docs/mission_objectives.md`の「指定ユニット・兵種が」という
     兵種制限は`resolveObjectInteraction()`の`allowedClasses`がInteract成功可否そのものを
     既に制限しているため、`interactionCount`が進んでいる時点で兵種制限は自動的に満たされて
     おり、Objective側で二重チェックする必要がなかった。対象Objectに`interaction`が
     設定されていない場合は`validateBattleMission()`が拒否する。M1-B項目4はこれで
     完了(防衛・護衛・脱出・破壊・装置の5種すべて実装済み。「Round生存」は防衛に統合)
5. Mission開始時Definition検証

### M1-C Battle Object最小実装

状態: **完了**。当時の「直近の実装順」記載の4項目のうち、データモデル部分、BattleController/UI統合の
「攻撃対象化」「Interactコマンドの配線」、BattleFactoryのランダム生成統合が完了。
Save Snapshotは「戦闘中は一切保存しない」既存設計により対象外と判明(詳細は項目4)。
`BattleObjectDefinition`/`BattleObjectState`、占有・通行(`blocksMovement`/`blocksStopping`/
`blocksDeployment`/`blocksProjectiles`)、倒木(Barrier、破壊まで)、踏査地点(Marker、占有共存)、
Exit(ExitPoint、データモデルのみ)、Interact検証(射程・兵種・要求State・使用回数上限)、破壊/
状態変化Eventの型までは実装・テスト済み。増援口(SpawnPoint)は種別の定義のみで専用ロジックはない。

- Battle Objectの攻撃対象化(2026-07): `resolveObjectAttack()`(`BattleObjectResolver.hpp/.cpp`)
  自体は既に完成していたが呼び出し元が無く、`ObjectStateChangedEvent`/`ObjectDestroyedEvent`も
  発行元が無い状態だった。`BattleController`に`ConfirmObjectAttack`(`ConfirmSkillAttack`と
  同じ並行状態パターン)・`pendingObjectTarget_`/`objectTargetableTiles_`・
  `pendingObjectPreview()`(命中率/反撃/状態異常の無い簡易Preview)・`confirmObjectAttack()`
  (`resolveObjectAttack()`を呼び、破壊時のみ`ObjectDestroyedEvent`を発行)を追加。
  `chooseAttack()`は射程内のcanBeAttackedなObjectを別Vectorとして収集し、
  `selectTargetTile()`がUnit/Objectのどちらを狙ったか判定する。`main.cpp`側は
  `ConfirmAttack`と同じConfirm/Cancelボタン・Tileハイライトのパターンを複製し、専用の
  `drawObjectAttackPreviewPopup()`(Damageと耐久のみ表示)とヒット/破壊バナー
  (`battle.object_hit_message`/`battle.object_destroyed_message`)を追加した。
  出荷済みコンテンツで唯一存在するBrokenwood Territoryの`fallen_log` Barrierも
  `canBeAttacked=true`/`maxDurability=16`/`defense=resistance=3`へ更新し、実際に攻撃可能な
  対象にした(従来は`blocksMovement`専用で登録されており、コントローラー側を配線しても
  対象外のままだった)。ヘッドレス環境のため実機(raylibウィンドウ)上での目視確認は未実施
  (`jf_battle_tests`の新規回帰テスト2ブロック+ビルド成功で検証、既存のrevert-verify規約に
  従い意図的に`resolveObjectAttack()`呼び出しを外して失敗することを確認した上で復元)
- Battle ObjectのInteractコマンド配線(2026-07続き): `resolveObjectInteraction()`自体は
  M1-C当初から完成していたが、どのDefinitionがInteract可能かを表す場所が無く
  (`ObjectInteractionDefinition`が独立した構造体のまま宙に浮いていた)、呼び出し元も
  UIボタンも存在しなかった。`BattleObjectDefinition`へ`std::optional<ObjectInteractionDefinition>
  interaction`と、成功時の遷移先`interactionResultState`を追加し、Definition側でInteract可否
  ・射程・兵種制限・要求State・使用回数上限・結果Stateを一体で表現できるようにした。
  `BattleController`に`SelectInteractTarget`(`ConfirmObjectAttack`と異なりPreview/Confirmを
  挟まず、`chooseHeal()`/`selectHealTarget()`と同じ「対象Tile選択で即時解決」パターン)・
  `objectInteractableTiles_`・`canInteract()`(UIがButtonを表示すべきかを判定する読み取り専用
  Query)・`chooseInteract()`・`selectInteractTarget()`(`resolveObjectInteraction()`を呼び、
  成功時のみ`ObjectStateChangedEvent`を発行)を追加。候補Tile収集
  (`computeInteractableObjectTiles()`)は`resolveObjectInteraction()`が確認毎回検証している
  requiredState/maxUses/allowedClasses/rangeと全く同じ判定を(状態変更を伴わずに)再現し、
  候補として出たTileは確定時に必ず受理される。`ActionKind`へ`Interact`を追加(既存の
  `Move/Attack/Skill/Item/Wait`のどれとも意味が異なるため専用値にした。既存コードに
  `ActionKind`の網羅的switchは無く、追加は非破壊)。`main.cpp`は`SelectAction`の固定5Slot
  (攻撃/スキル/アイテム/待機/戻る)が既に埋まっているため、`canInteract()`がtrueの時だけ
  現れる6個目のButtonをその左に追加し、Tileハイライト色も新設(紫系、既存の攻撃/回復/
  スキル色と衝突しない)。出荷済みコンテンツには現状Interact可能なObjectが1つも無いため
  (`fallen_log`は攻撃対象のみ)、実プレイでこのButtonが表示されることはまだ無い -
  次の地域・装置コンテンツが`interaction`を設定した時点で自動的に有効になる設計。
  `BattleController`経由の配線を検証する新規回帰テスト1ブロック(誤った兵種では候補Tileに
  現れないこと、正しい兵種での確定がObjectState/interactionCount/Eventを正しく更新すること、
  対象外Tileの選択が無視されること)を追加
- BattleFactoryランダム生成統合(2026-07続き): `battle_objects.md`「ランダム生成」手順1〜7の
  うち3(必須Objective/Spawn/Exit予約)・5(任意Barrier/Container配置)・6一部(経路の再検証)を
  一般化。従来はBrokenwood Territory専用の`if (stage.terrainProfileId == kBrokenwoodTerritoryTerrain)`
  Ad-hocブロックが`fallen_log`のDefinition登録から配置まで丸ごとハードコードしていたため、
  次の地域が別のBarrier/Containerを乱数配置したくなるたびに同じ形のブロックが増える構造
  だった。`StageDescriptor::ObjectPlacementRule`(`BattleObjectDefinition`本体・配置数・Route
  B用`extraBarrierCount`加算フラグ・配置列範囲・「最初の生存Enemyの行を避ける」フラグ)を
  新設し、`BattleFactory.cpp`の`placeRandomObjects()`が`stage.objectPlacementRules`を読んで
  Definition登録・候補収集(行優先重複回避→残り埋めの2パス、既存ロジックのまま)・配置まで
  汎用的に行う。地域書側(`Region.cpp`)はBrokenwood向けに1件の`ObjectPlacementRule`を
  登録するだけになった。`blocksMovement`なRuleは配置前に`hasRouteAcrossWithBlockedTiles()`
  (`hasRouteAcross()`のBattleState版、Objectで塞がれるTileを渡せる)で「盤面横断経路が
  最低1本残るか」を検証し、塞ぐ候補は次のシャッフル候補へ回す - 手順7の「上限到達後は
  安全な固定配置へ戻す」フォールバックは未実装のまま(候補が尽きたら単にその分は配置しない。
  現状の地域は候補さえあれば経路を塞がずに収まるため優先度を落とした)。
  リファクタ直後、`jf_forest_balance`のBrokenwood勝率が89.7%→13.7%へ急落する事象を検知して
  原因調査した結果、今回のリファクタ自体のバグではなく、前Slice「Battle Objectの攻撃対象化」
  由来の潜在バグ(`tools/forest_balance.cpp`の`attackIfPossible()`が、射程内にUnitが0体で
  canBeAttackedなObjectだけがある新ケースで`chooseAttack()`が`SelectTarget`へ遷移したまま
  `cancelAttackSelection()`を呼ばずに`false`を返し、以降のPolicy関数が期待する`SelectAction`
  と食い違って戦闘がRound上限までTimeoutし続けていた)と判明。実プレイの`main.cpp`は
  Cancelボタンがあるため無関係で、このBalance-testing Botだけの潜在バグだった。
  `attackIfPossible()`にUnit対象なしの場合の`cancelAttackSelection()`呼び出しを追加して修正し、
  Brokenwood 89.7%/87.7%・Timeout一桁台まで回復したことを確認済み

1. Object ID、占有、通行、耐久、陣営
2. 倒木、踏査地点、Exit、増援口
3. 操作・破壊Event(攻撃による破壊、Interact操作による`ObjectStateChangedEvent`とも配線完了)
4. Save Snapshot
   - **対象外と判明(2026-07)**: `include/jf/core/SaveSystem.hpp`の`ExpeditionCheckpoint`
     冒頭コメントが既に明記する通り、「戦闘中・配置中の状態はそもそも一切保存しない」設計が
     この方針決定前から確立している - 中断したら最後のExploration/Campチェックポイントへ
     戻り、同じSeedから戦闘を決定論的に再生成する(増援Waveの状態を保存しない判断と同じ
     理由)。`battle_objects.md`の「Save」節が想定する「現在耐久・State・操作回数を
     中断Saveへ保存する」仕組みは、そもそも存在しない中断セーブポイントに対して作ることに
     なるため、実装不要と判断した。M1-C完了Gateからこの項目を除外する

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

### M1-E コンテンツ追加基盤

状態: **地形Profile Slice完了。既存6 Stage(灰枝の森3地点+沈黙した監視所群3地点)すべてを
`data/regions.json`駆動へ移行済み(`StageDescriptor`の一部フィールド)、`BattleFactory.cpp`から
地域名・地点名による分岐を除去済み、CTest登録済みのコンテンツ構造検証(`jf_content_tests`)を
新設済み。`UnitClass` switch分散のうち表示名2箇所を解消(2026-07)。M9前ブロッカー
4項目すべて対応完了(2026-07、うちEncounter生成ロジック自体のDefinition駆動化は
調査の結果、現状ニーズが無いと判断し意図的に見送り)。AI・Boss状態のデータ化、
兵種パッシブの能力ID化は未着手。M9着手前の必須基盤は完了**

目的: 新しい兵種、一般敵、地形構成、地点、地域を、既存Mechanicの組み合わせであれば
C++の列挙・分岐追加なしにDefinitionだけで追加できるようにする。独自Boss行動や新しい地形効果など、
新Mechanicの実装までデータだけで表現することは目標にしない。

**M9前ブロッカー**(M9着手前に必ず解消する項目、他は未着手のままでもM9へ進める):

1. 残りRegion/Site/Encounterの完全定義化(実装Slice1、Encounter生成ロジック自体の
   Definition駆動化) - **調査済み・意図的に見送り(2026-07)**。`BattleFactory.cpp`の
   `assembleScenario()`が持つ14ステップの生成手順は既に地域名・地点名による分岐を
   一切持たず、全て`stage.xxx`の有無チェックだけで分岐している。出荷済み7 Stage
   (灰枝の森3・沈黙した監視所群6・灰鉄採石場プレースホルダー)は全て同じ手順で問題なく
   生成できており、「この手順では表現できないStage」は現状一つも存在しない。正本
   (`docs/region_mission_data_contract.md`等)にも具体的なStep Schemaは無く、この項目は
   実装原則9「未実装の将来拡張を先に一般化しすぎない」と衝突する予防的設計と判断し、
   M9で実際にこの手順で表現できないStageが出た時点で着手する
2. `RewardRule`への置換(実装Slice4) - **完了(2026-07)**。`StageDescriptor`/
   `StageContentData`の`baseVictoryLoot`/`routeVictoryLootDelta`/`surveyBonusLoot`
   3フィールドを`std::vector<RewardRule>`(Always/RouteChoice/SurveySuccessの3条件)
   1本へ統一。`data/regions.json`のJSON key名・構造は無改修、`jf_forest_balance`実測は
   完全に同一(Byte-identical)。本格報酬台帳(`RewardGrantId`二重付与防止等)はM9まで
   持ち越しのまま
3. Route Graph到達可能性検査(実装Slice7) - **完了(2026-07)**。`loadGameData()`が起動時に
   全Route Graphへ`validateRouteGraph()`を適用する
4. Objective達成可能性検査(実装Slice7) - **静的な参照ID検証は完了(2026-07)**。
   `validateBattleMission()`を`jf_content_tests`が全Stage×全ExplorationChoiceで
   アサートするようになった。「実際に達成可能か」のMonte Carloシミュレーションは未着手のまま

残る移行対象:

- `UnitClass`列挙と複数の`switch`に分散した兵種ID、表示、パッシブ
- `Region.cpp`へ直書きされた敵編成、探索結果、報酬、地点名
- 薬草地点、倒木数、敵生成列などの`terrainProfileId`固有分岐
- 狼・灰角大猪の専用AI選択と、`Unit`へ直接追加されたBoss専用一時状態

実装Slice:

1. **Definition Schemaを固定**
   - `UnitDefinition`: 安定Unit ID、Class ID、表示Locale Key、所属候補、基本Loadout、AI Profile ID
   - `ClassDefinition`: 基礎能力、移動Profile、パッシブ能力ID、装備可能カテゴリ
   - `TerrainProfile`: Tileごとの重み、最低/最大数、特徴Tile、通路保証、生成Version
   - `EncounterDefinition`: 敵Wave、生成範囲、Battle Object配置、Objective、増援、AI Profile
   - `SiteDefinition`: Choice、Encounter、Reward、Camp、Message、解放・安全通過条件
   - `RegionDefinition`: Route Graph、入口、Exit、Site参照、完了条件、次地域解放
   - **一部着手(2026-07)**: `region_mission_data_contract.md`が定めるフル`SiteDefinition`/
     `RegionDefinition`(Reward Grant Ledger、PendingRegionCompletion等、今は存在しない仕組み
     まで含む)への全面移行は依然M9まで持ち越し(`implementation_status.md`記載の判断を維持)。
     代わりに`StageDescriptor`(`jf/core/Region.hpp`)自体を「段階的にJSON化」する方針を採り、
     JSON化できるフィールドの安定Subsetを`GameData::StageContentData`
     (`jf/data/GameData.hpp`)として切り出した。`id`/`terrainProfileId`/`enemyRoster`/
     `baseVictoryLoot`/`routeVictoryLootDelta`/`surveyObjectiveId`/`surveyBonusLoot`/
     `discoveries`/`missionNameEn`/`missionNameJa`が対象(`objectPlacementRules`/
     `herbPatchGeneration`/`routeOutcomes`/`timedReinforcement`等、より複雑な地点固有Ruleは
     未対応のままC++直書き)。`StageDescriptor`が`Region.hpp`(`GameData.hpp`を包含)にあるため
     `GameData.hpp`が`StageDescriptor`を直接持てない(循環include - `RegionId`が`BaseState.hpp`
     に置かれている既存理由と同じ制約)。`data/regions.json`+`GameData::loadGameData()`の
     Loader(重複ID・存在しないTerrain Profile参照・未知`ExplorationChoice`文字列を起動時に
     拒否)を追加し、`Region.cpp`の`stageDescriptorFromContent()`が`StageContentData`から
     `StageDescriptor`の共通部分を組み立てる。実証として灰枝の林縁(構成が最も単純)を
     JSONへ完全移行し、`Region.cpp`のC++直書きから削除した。`jf_forest_balance`実測は
     移行前後で完全に同一の数値(Byte-identical)であることを確認済み。回帰テストは
     Loaderが正しい値を読むことの直接検証、`regionDescriptor()`出力の内容一致、および
     不正`regions.json`(存在しないTerrain Profile参照)がLoad全体を拒否することの3点を追加
   - **Schema拡張・薬草の沢を移行(2026-07続き)**: `StageContentData`へ
     `routeOutcomes`/`scoutRouteRequiredClass`/`scoutRouteDisabled`/`timedReinforcement`
     (`Phase`・`GridPos`一覧を含む)/`herbPatchGeneration`を追加(`TimedReinforcementData`は
     `StageDescriptor::TimedReinforcement`と同じ形の別型 - 循環include制約のため直接共有できず、
     `stageDescriptorFromContent()`が1:1変換する)。薬草の沢(灰枝の林縁より複雑 - 増援Wave、
     衛生兵限定ルート、薬草地点生成ルールを持つ)をこの拡張Schemaへ完全移行し、`Region.cpp`の
     C++直書きから削除した。`jf_forest_balance`実測はここでも移行前後で完全に同一。
   - **残り4 Stage完全移行(2026-07続き)**: `StageContentData`へ`objectPlacementRules`
     (`BattleObjectDefinition`をそのまま埋め込み - `BattleObject.hpp`は`GameData.hpp`を
     参照し返さないため循環include制約に触れない)・`enemyCountOverride`・`boostedFirstEnemy`・
     `understaffedReinforcement`/`understaffedThreshold`・`logCollisionBonusLoot`/
     `noCasualtiesBonusLoot`を追加。折れ木の縄張り(ここまでで最も複雑なStage - Roster、
     ルート別報酬・結果、Scout Route無効化、倒木のObjectPlacementRule、頭数連動増援、
     Ad-hoc副目標2種を全て持つ)と沈黙した監視所群3地点(`enemyRoster`をJSON側で意図的に
     省略 - 空Rosterは「`GameData::enemyRoster`という共有Rosterを使う」という既存の意味を
     保持したまま)を完全移行し、`Region.cpp`の`cinderwatchGateRegion()`/
     `ashboughForestRegion()`から該当ブロックを削除した。これで出荷済み6 Stage全てが
     `data/regions.json`駆動になった。`jf_forest_balance`実測はここでも完全に同一
     (Byte-identical)。回帰テストは折れ木の縄張り・沈黙した監視所群3地点それぞれについて
     Loaderの出力と`regionDescriptor()`出力の内容一致を追加
2. **LoaderとValidationを実装**
   - 重複ID、未知参照、負数、確率合計、配置不能、到達不能、循環前提を起動時に検出
   - Definition読込失敗時は不完全な地域を開始可能にせず、内部ID付き診断を出す
   - Locale Key、Save Alias、生成Versionを必須項目として検査
3. **地形生成をProfile駆動へ移行**
   - **完了(2026-07-14)**: `terrain_profiles.json`、`TerrainProfile` Loader、Validation、
     共通重み付き生成器へ現行6フィールドを移行。`FieldType`列挙と地点別生成率の`if`連鎖を削除
   - **完了(2026-07-14)**: 重複ID、未知Terrain、重み合計、個数上下限、現行参照Profile欠落を検出
   - **完了(2026-07-14)**: 任意のテストProfileをC++分岐追加なしで生成する回帰テストを追加
   - **修正(2026-07-14)**: 薬草の沢の旧閾値が正本より灰地+5ポイント・通常床-5ポイントだった
     不一致を解消し、通常床40%・浅瀬35%・茂み15%・灰地10%へ統一
   - 現在の6 `FieldType`を同値の`TerrainProfile`へ置換
   - 地形率、茂み上下限、障害物列制限、特徴Tile、通路保証を共通生成器で処理
   - **完了(2026-07続き)**: 倒木は前項(M1-C)の`StageDescriptor::ObjectPlacementRule`で対応済み。
     薬草地点も同じ「地域書がRuleを持ち、Factoryは`stage.terrainProfileId`を見ない」形へ
     `StageDescriptor::HerbPatchGenerationRule`(配置数・列範囲)として一般化した
     (`if (stage.terrainProfileId == kHerbwaterHollowTerrain)`だったブロックを
     `if (stage.herbPatchGeneration)`へ置換、`kHerbwaterHollowTerrain`はTerrainProfile ID
     指定以外の用途では未参照になった)。踏査地点(`chooseSurveyTile()`)はそもそも地点名で
     分岐しておらず($kRightZoneMinCol$〜$kRightZoneMaxCol$の共通列範囲を使う汎用関数)、この
     項目の対象外と判断。Object化(実際の`BattleObjectState`のMarkerにする)は保留 - 現状の
     `ObjectiveDefinition::target.tile`直接参照で「地点名で判定しない」目的は既に満たしており、
     Object化はSecureTile評価ロジック側の変更を伴う別スコープのため
4. **Encounter生成をDefinition駆動へ移行**
   - 敵数、敵編成、能力補正、初期配置列、自由配置、増援、Objectiveを共通Factoryで解決
   - `StageDescriptor`の個別報酬欄を汎用`RewardRule`へ置換
   - **完了(2026-07)**: 地域名・地点名・`FieldType`による`BattleFactory`分岐を除去。
     残っていた唯一の名前分岐(`stage.terrainProfileId == kBrokenwoodTerritoryTerrain`で
     敵生成列を右2列に絞る折れ木の縄張り固有ルール)を`StageDescriptor::enemyZoneWidth`
     (既定`nullopt`は通常の3列)として一般化し、`data/regions.json`側のデータへ移した。
     `BattleFactory.cpp`の`assembleScenario()`/`buildEnemies()`はこれで`stage.id`/
     `stage.terrainProfileId`の値そのものを一切参照しない(`TerrainProfile`はID経由で
     `data.terrainProfile(stage.terrainProfileId)`を引くためのKeyとしてのみ使う)。
     `jf_forest_balance`実測は完全に同一。項目1(敵数・編成等を「共通Factoryで解決」、
     つまりEncounter生成ロジック自体をC++コードとしてもDefinitionから動的合成する高階の
     汎用化)はまだ未着手 - 現状は「値の置き場所がJSONになった」段階で、生成の手順自体は
     BattleFactory.cppのC++のまま。項目2(`RewardRule`置換)は**完了(2026-07)**、
     詳細は上記M9前ブロッカー項目2を参照
5. **ユニットとAIの追加契約を分離**
   - 通常追撃、低HP優先、射程維持、防衛、撤退を再利用可能な`AIProfile`として定義
   - 兵種パッシブは能力IDの一覧から解決し、既存兵種追加で`switch`を増やさない
     - **表示名2箇所は完了(2026-07)**: `main.cpp`の`classNameFor()`/`classRoleFor()`が
       `UnitClass`の`switch`文だったのを、`ClassDefinition`(`data/classes.json`、
       既にJSON化済み)へ`nameKey`/`roleKey`の2フィールドを追加してそこから引く形へ
       移行した。実装前に本物のブロッカーを発見: `classNameFor`/`classRoleFor`は
       `loadAppFont()`(フォントGlyph収集、`GameData`読込より前に実行)からも
       呼ばれており、素朴に`GameData`引数を追加するだけでは起動順序が壊れる状態だった。
       調べた結果、この呼び出し自体が既に冗長と判明(両関数は`tr()`のラッパーに過ぎず、
       その和文はLocale Table全体を自動収集する`allJapaneseGlyphText()`が既にカバー
       済み - `loadAppFont()`冒頭のコメントが元々そう説明していた)。冗長な収集行を
       削除しただけで依存自体が消え、起動順序変更なしで済んだ。残る呼び出し元
       (main.cppのUI描画関数群、9箇所)は全て`jf::GameApp& app`を引数に持っていたため
       `app.gameData()`をそのまま渡すだけで済み、ビルド・`ctest`4種・実機起動確認
       (フォントロード完了までクラッシュなしを確認)とも通過。兵種パッシブ自体
       (`UnitClass.cpp`の`hasBrace`/`hasZoneOfControl`等)は元々`switch`ではなく
       単発の等値比較関数群で、新兵種追加で既存コードを壊さない形に既になっていたため
       対象外と判断。Boss固有状態の分離、AIProfileの拡張はまだ未着手
   - Boss固有状態は`Unit`本体から能力ごとのRuntime Stateへ移し、新Boss追加で専用Fieldを増やさない
   - 独自Boss行動はC++ Ability/AI Handlerとして実装し、Definitionから安定IDで参照する
6. **既存コンテンツを無挙動変更で移行**
   - 灰枝の森3地点と沈黙した監視所群3地点を新Definitionへ移行
   - 移行前後で同じSeed、Choice、編成から地形、敵、Objective、報酬が一致するFixtureを作る
   - 旧ID Aliasと生成Versionを残し、既存Saveを入口へ強制リセットしない
7. **コンテンツ検査ツールを追加**
   - 全Region/Site/Encounter参照の一括検証
   - Route Graph到達可能性、初期配置可能数、通路保証、Objective達成可能性の静的検査
   - 指定Seed群で盤面生成を繰り返し、配置不能・進行不能・範囲外生成を検出
   - **部分完了(2026-07)**: `tests/test_content.cpp`(`jf_content_tests`、`jf_locale_tests`と
     同じくCTest登録済み・`ctest`実行のたびに自動検証)を新設。読み込んだ全Region×全Stageを
     100 Seedずつ`createScenarioBattle()`で実際に生成し、(1)全Unitが盤内・通行可能Tile・
     互いに非重複、(2)敵数が`enemyCountOverride`/Roster数と一致、(3)`ObjectPlacementRule`の
     配置数・列範囲が一致しObject同士・UnitとObjectが非重複、(4)`herbPatchGeneration`の
     HerbPatch枚数が一致、(5)盤面左端から右端まで実際に到達可能な経路が最低1本存在、
     (6)同一Seedからの2回生成が地形・Unit位置とも完全に一致(決定論)を検証する。(5)の経路検証は
     `BattleFactory.cpp`内部の`hasRouteAcross()`系(非公開)と同じ「地形通行可否+
     blocksMovementなObjectのみを見る無制限BFS」を`test_content.cpp`側に再実装したもの -
     実装当初`computeReachableTiles()`(MOV値で制限される、Unit占有も見る)で代用しようとして
     即座に失敗(8列の盤で移動力4のUnitが1手で右端へ届かないのは経路不備ではなく正常な仕様)し、
     モードの違いを確認した上で無制限BFSへ書き直した。全7 Stage(灰枝の森3+沈黙した監視所群6の
     一部が既に完了済みでAshironQuarryのプレースホルダー1を含む)×100 Seedで現状パス。
   - **Route Graph到達可能性・Objective静的参照ID検証完了(2026-07)**: 「Route Graphは
     灰枝の森の一直線経路のみで分岐が無く」という上記の見送り理由は、M6-BでCinderwatchが
     `BranchGroup`付き分岐Route Graphを獲得した時点で既に古くなっていた。`validateRouteGraph()`
     (entrance→exit到達性、全SiteNode到達性、`src/core/RouteGraph.cpp`に既存実装済みだが
     どこからも呼ばれていなかった)を`GameData.cpp`の`loadGameData()`終端へ配線し、他の全
     Loader検証と同じ「起動時に拒否する」扱いにした。Objective側は、`BattleFactory.cpp`が
     戦闘組み立てのたびに呼んでいた`validateBattleMission()`の戻り値が`stderr`出力のみで
     ビルドを落とさなかった点を修正: `test_content.cpp`の`checkStage()`がこの戻り値を
     `assert(errors.empty())`するようになった。合わせて`checkStage()`は`FrontalAdvance`
     (従来どおり100 Seed)に加え`CollapsedSidePath`/`ScoutRoute`(各20 Seed、`routeOutcomes`で
     主目的自体が差し替わる地点の検証漏れを塞ぐための軽量チェック)も検証するようになり、
     敵数・Object配置数のassertionも`ExplorationOutcome`の`enemiesRemoved`/
     `extraBarrierCount`を考慮する式へ一般化した。「実際に達成可能か」のMonte Carlo的な
     実達成可能性検査(現在の板面・敵AIで本当に勝てるか)は依然未着手で、これはM9で
     必要になった時点で着手する判断のまま

M1-E完了Gate:

- 既存Mechanicだけを使う一般ユニットをDefinition追加だけで戦闘へ参加させられる
- 地形率、配置物、敵編成、Objectiveが異なる新地点をDefinition追加だけで生成できる
- 新地域を追加しても`GameApp`、`BattleController`、`main.cpp`を変更しない
- 灰枝の森と沈黙した監視所群のSeed Fixture、報酬、Save再開が移行前後で一致する
- 未知ID、不正参照、到達不能Graph、配置不能EncounterをBuild/CTestで検出する
- M9の地域データはこの基盤以外の独自Factoryを作らない

M1全体の完了Gate(2026-07時点の再評価):

- 同じRoot Actionで同時撃破・Object破壊・Objective達成を決定論的に解決 - **形式上は未達**。
  `RootActionId`/`BattleEventEnvelope`(`sequence`付き)という正式なデータ構造そのものは
  まだ存在しない(既存の`BattleEvent{id, actionId, payload}`はEvent単体のIDと「どの行動に
  属するか」は持つが、Player入力/敵AI行動/増援出現/Phase開始効果を一律に1つのRoot Actionで
  囲む仕組みではなく、`actionId`はPlayer/Enemyの個別行動解決時にしか振られない=非Action
  Event側は`actionId=0`のまま)。ただし**実質的な効果(同時発生時の決定論)は既存テストで
  確認済み** - 「同一Batchで敵味方同時全滅なら敗北優先」「同時複数撃破のEvent数が過不足ない」
  (M1-A)の回帰テストが通っている。正式化はコストが大きく(Player入力・敵AI・増援・
  Phase開始の全Event発行箇所を再配線する必要があり、影響範囲が広い)、ここまでの实装で
  露呈した実害は無いため、着手判断は保留中
- 7種以上のObjectiveが共通Trackerを利用 - **達成**。8種
  (EliminateTeam/DefeatUnit/SecureTile/DestroyObject/SurviveRounds/EscapeUnits/
  ProtectUnit/OperateObject)が`ObjectiveTracker`を利用する(2026-07完了)
- 同じ結果画面を再表示しても報酬を二重付与しない - **達成**。`GameApp::proceedToCamp()`が
  `screen_ != Screen::Battle`を確認してから処理する(以前の実バグ修正済み、M1-D参照)
- 灰枝の林縁を地域名によるC++分岐なしで生成可能 - **達成**。`data/regions.json`駆動へ
  移行済み、`BattleFactory.cpp`から地域名分岐を全除去(2026-07完了、M1-E参照)
- 不正Definitionを起動時に検出 - **達成**。`GameData::loadGameData()`が重複ID・未知参照等を
  拒否し、`jf_content_tests`が生成結果の構造的妥当性もCTestで継続検証する

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

状態: **M3-A最小スライス完了(残りは意図的に保留)。M3-Bは完了。M3-Cはメモリ上の上限・保留・整理・放棄と
帰還事前検証が完了(保存I/O成功後にPendingを破棄する永続化TransactionはM3-D Web同期と合わせて未実装)。M3-Dはデスクトップ
完結範囲が完了(Web同期待ち・GitHub Pages継続試験はブラウザ/Pages公開環境が無いため
対象外)。M3-Bは段階1〜5(`main.cpp`の`kJa*`定数421個を全廃、残るのは意図的な1件
`kJaJapaneseNative`のみ)+段階6の実行可能な範囲(`check_localization`のKey集合・
Formatter・`kJa*`残数検査)まで完了**

目的: 地域や施設を増やす前に、更新、文字追加、倉庫超過で既存進行を壊さない状態にする。

正本:

- [`save_system.md`](save_system.md)
- [`expedition_recovery.md`](expedition_recovery.md)
- [`localization.md`](localization.md)
- [`inventory_overflow.md`](inventory_overflow.md)

実装Slice:

### M3-A Checkpoint復旧

状態: **最小スライス完了(入口退避のみ)。Alias/隔離/Snapshot復元は未実装**

正本の完全な仕様(Attempt ID、Checkpoint Kind列挙、Node/Route/Region Alias解決、
generator-version対応のSnapshot再利用、`QuarantinedExpedition`状態)は、現状
Route Graphが1本(Ashbough Forest)しかなく一度も改版されていないため、実際の
Alias/バージョニング要件がまだ発生していないと判断し、後回しにした。代わりに
`applySaveData()`にあった具体的なデータ損失バグ1件のみを修正:

- **修正済み**: Checkpointの`routeProgress`が指す Node IDが存在しない、または
  `expeditionStage`が範囲外だった場合、旧実装は該当Checkpointを警告なく完全に
  破棄していた(Pendingの報酬・発見・Site Access更新・地域完了・Bag・HPが全て
  消える)。正本の「更新後の復旧」優先順位4(region/partyが有効ならPendingと
  消耗を維持して地域入口へ退避)に基づき、region/partyが有効な場合は
  Pending Loot/Discoveries/Bag/pendingSiteAccessUpdates/pendingRegionCompletions
  とパーティHPを保持したまま、地域の入口(Route Graph地域は
  `initialRouteProgress()`、旧式のCinderwatchはstageIndex=0)へ退避するように
  変更した。
- **未実装(意図的に対象外)**: Attempt ID/Checkpoint Kindの正式な型、
  Route/Node/Region Aliasテーブルによる「リネームされたが意味的には同じ」
  ノードの解決、`QuarantinedExpedition`(自動退避もできない矛盾したSaveの
  隔離状態)、地形・敵・増援のgenerator-version対応Snapshot再利用。これらは
  複数のRoute Graphやその改版が実在するようになってから、実際の要件に合わせて
  設計するのが妥当と判断した。
- 回帰テスト2件追加(`tests/test_battle.cpp`): 実在するCheckpointの
  `routeProgress.currentNodeId`を存在しないIDに書き換えても、Pending Loot/Bag/
  HPが保持されたまま地域入口(灰枝の林縁)へ復帰すること。旧式地域
  (Cinderwatch Gate)で`expeditionStage`を範囲外にしても同様に例外なく
  Explorationへ復帰すること。

### M3-B Locale移行

状態: **段階1〜5完了。段階6はKey集合・Formatter・`kJa*`残数を自動検査する実行可能範囲まで完了**

`src/main.cpp`(3165行)には`pick(en, ja)`呼び出し242箇所・`kJa`定数421個という規模で
表示文が直書きされており、一度に全件移行するのは検証不能なため、`localization.md`
自身が定義する6段階の移行順のうち**段階1(Loader基盤)+段階2の一部(共通ボタン・
フェイズ見出し・汎用エラー/検証文言)**だけを本Sliceの対象とした:

- `data/locales/ja.json`/`en.json`(`ui.phase.player`のような安定ID、17 Key)と
  `jf::loadLocales()`/`jf::tr(key, japanese)`/`jf::tr(key, japanese, args)`
  (`include/jf/core/Locale.hpp`、`src/core/Locale.cpp`)を新設。`GameData.cpp`の
  `readJsonFile()`と同じ「開く→Parse→失敗時はcerrへログしnullopt」規約に揃え、
  `loadLocales()`は`en`/`ja`のKey集合が完全一致しない限り起動失敗として扱う
  (`localization.md`「欠落時に... 黙ってフォールバックしない」)。未解決Keyは
  `"[[MISSING:key]]"`という目立つマーカーを返す
  - Formatter版(`{name}`置換)は現時点でどの移行済みKeyも使わないが、段階3以降の
    テンプレート付き文言(HP表示等)へ備えて実装し、専用テストで直接検証した
- `main.cpp`側は既存の`gLanguage`/`Language` enumをそのまま再利用し、`tr(key)`という
  1行ラッパー(`gLanguage`を見て`jf::tr()`を呼ぶ)経由で接続 - `Locale.cpp`はmain.cppの
  言語切替Globalを一切知らない
- 移行対象(共通UI 10項目のうち実装済みの3つ+汎用ボタン+フェイズ見出し+汎用エラー/
  検証文言、計17 Key): フェイズ見出し(プレイヤー/敵フェイズ)、決定/キャンセル/戻る/
  待機/ターン終了/閉じる/つづける/追加/削除ボタン、設定/言語ラベル、勝利/遠征失敗の
  見出し、データ読込失敗・4人選択の検証文言。該当する17個の`kJa*`定数は削除し、
  `button()`/`disabledButton()`の呼び出し元をそれぞれ`tr(key)`へ置換した
  (`button()`に、既存の英日2文字列版と別に1Key版のオーバーロードを追加)
- 兵種名・素材名・施設名・状態異常名・地域名・戦闘メッセージ・結果画面・遠征準備の
  個別文言(残り400個超の`kJa*`定数)は**本Sliceでは変更していない** - 段階3・4の対象
  として残す
- `loadAppFont()`の`charsetSource`(Glyph Atlas登録、[[feedback_ja_glyph_coverage]]
  参照)を、個別`kJa*`定数の手動列挙から`jf::allJapaneseGlyphText()`(Locale全体の
  日本語Valueを自動連結)+残る`kJa*`定数の列挙という2本立てへ変更 - 段階3以降で
  Key化する文字列は今後この1箇所の変更なしに自動的にGlyph網羅される
- 新規`tests/test_locale.cpp`(`jf_locale_tests`、`jf_battle_tests`とは別のCTest
  実行ファイル): Key集合の完全一致検証、両言語の`tr()`が期待通りの文字列を返すこと、
  未解決Keyのマーカー表示、Formatter、破損したJa.jsonペアを検出すること(検証は
  実ファイルではなく一時スクラッチディレクトリへコピーしたものを壊す方式にした -
  実データファイルを壊した状態でassertが落ちるとリストア行を実行せず本物の
  `data/locales/ja.json`が壊れたまま残るため)を検証。`loadLocales()`のKey集合
  検証部分を意図的に無効化して本テストが実際に失敗することを確認した上で復元
- クリーンビルド後、`ctest`を20回連続実行して全通過を確認
- 実機確認: `build/JOJIFrontier`を実際に起動し、遠征準備画面で移行済みの
  「Settings」ボタンが文字化けなく描画されることをスクリーンショットで確認。
  日本語モードでの目視確認は、この環境の画面共有state上の制約(スクリーンショットの
  たびに無関係な別ウィンドウが前面に来る)により断念し、代わりに`tr()`が両言語で
  正確な文字列を返すことをテストで直接(文字列完全一致)検証する形で代替した
- **段階3の一部(2026-07、続き)**: 兵種名・役割説明(`classNameFor`/`classRoleFor`、
  9兵種×2)、アイテム名・説明(`itemFullNameFor`/`itemDescriptionFor`、6種×2)、
  素材名(`materialNameFor`、13 ID)、武器名(`weaponNameFor`、12 ID)、
  キャラクター名(`unitDisplayNameFor`、10エントリ)、地形名(`terrainNameFor`、
  8種)、Battle Object名(`battleObjectNameFor`、6種)を`class.*`/`item.*.name`/
  `item.*.description`/`material.*`/`weapon.*`/`character.*`/`terrain.*`/
  `battle_object.*`という約90 Keyへ移行。`materialNameFor`/`weaponNameFor`は
  「既知IDならKey化、未知IDはそのまま返す」という既存の後方互換フォールバックを
  維持しつつ、内部実装を個別`unordered_map<string,string>`から
  `unordered_set`+`tr("prefix." + id)`へ整理(素材/武器IDそのものが安定IDのため、
  Key名の大部分をID文字列から機械的に導出できた)。対応する17個の`kJa*`定数
  (`kJaFallenLog`等、施設Battle Object名とRescuePack/CampRations/ReturnFlareの
  アイテム名)を削除し、`charsetSource`からも除去(段階1で`jf::
  allJapaneseGlyphText()`経由に切り替え済みのため、Key化した文字列は追加の
  手当てなしで自動的にGlyph網羅される)
- **段階3の残り(2026-07、続き)**: 拠点段階名・短縮名(`outpostStageNameFor`/
  `outpostStageShortNameFor`、4段階×2)、Discovery名(`discoveryNameFor`、5件)、
  施設名・役割説明(`facilityIdNameFor`/`facilityRoleFor`、6施設×2)、状態異常
  バッジ(`activeStatusBadges`のGlyph/ラベル、5種×2+「残り」「このPhase終了まで」
  「次の行動は移動不可」の3共通語)を`outpost_stage.*`/`discovery.*`/`facility.*`/
  `status.*`という約38 Keyへ移行。対応する17個の`kJa*`定数を削除
- **段階4の一部(2026-07、続き)**: 戦闘ログの命中/外れ/瀕死/戦闘不能メッセージ
  (`hitMessageText`/`missMessageText`/`lowHpMessageText`/`fallenMessageText`)を
  `battle.hit_message`等4 Keyへ移行 - このSlice最初の`jf::tr(key, japanese, args)`
  Formatter実戦投入(`{attacker}`/`{target}`/`{damage}`の名前付き引数)。main.cpp側に
  `tr(key, args)`という2引数ローカルラッパーを追加(既存の1引数`tr(key)`と対になる)。
  スキル/施設ノードの名前・効果文言(`SkillDefinition`/`FacilityNode`の
  `nameEn`/`nameJa`フィールド)は、Skill/Facility Definitionという別のデータ層が
  既に持つ正規のバイリンガルフィールドを`pick()`する既存パターンであり、
  `kJa*`定数の直書き債務ではないため対象外と判断した
- **段階4の続き(2026-07、続き)**: 戦闘HUD操作文言(行動選択の状態ラベル、Attack/
  Skills/Items/Heal/Wait/Back各ボタン、対象選択の状態別プロンプト、使用可能スキル/
  アイテム無しの表示)、戦闘予測ポップアップ(Damage/Hit/Evasion)、遠征準備画面
  (タイトル・パーティ見出し・物資見出し・バッグ見出し・空き枠・遠征開始ボタン)、
  キャンプ画面(タイトル・戦利品確保・パーティHP・保留戦利品・勝利数・遠征継続/
  拠点帰還/キャンプ進行ボタン)、設定画面(ウィンドウ操作・遠征リタイア・Export/
  Import一式のボタンとステータス文言)を約115 Keyへ移行。この過程で`drawUnitInfo()`
  という戦闘中ユニット情報表示関数が呼び出し元0件の完全なデッドコードだったことを
  発見し削除した(HP/STR/MAG/DEF/RESの表示に`gLanguage`を無視した常時バイリンガル
  混在文字列を出す既存のおかしな実装だったが、どこからも呼ばれておらず実害はなかった)。
  Export/Import状態表示(`setSaveStatus()`)は英日を別々に確定させる既存シグネチャの
  ため、呼び出し側で`jf::tr(key, false)`/`jf::tr(key, true)`を明示的に両方評価して
  渡す形にし、表示中に言語を切り替えても`pick()`が正しく再解決する既存の動的挙動を
  保った。`main.cpp`の`pick()`/`kJa*`残数は433個→199個まで減少
- **段階4残り+段階5(2026-07、完了)**: 探索3択(正面突破/側道/斥候ルートとその効果、
  灰枝の森・シンダーウォッチ両地域分)、PreBattleDeployment画面、拠点画面(拠点発展・
  Discovery一覧)、Facilities/Forge/Unit各画面(施設Unlock/Build/解体/再建 ※解体・再建
  UIは後にM5で廃止、素材不足理由、調整特性装備、兵種別レシピ、装備変更)、Camp画面の
  Next Field情報パネル(灰枝の森
  ルートとCinderwatch Gate旧地域の両方の分岐文言)を移行し、`main.cpp`に残る
  `kJa*`定数を421個から**0個**(意図的に残す`kJaJapaneseNative`1個を除く)にした。
  `siteAccessLabel()`/`facilityNodeBlockedReason()`等、switch文や早期returnで組み
  立てていた関数は構造を変えずcase内の`pick()`を`tr()`へ置換するだけで済んだ。
  タイル情報ツールチップ(通行不可/移動コスト/HP回復)、Forge鍛冶画面の見出し群、
  Unit画面の兵種別装備状態も同時に移行
  - 移行後も残る`pick()`呼び出しは全て、`SkillDefinition`/`FacilityNode`の
    `nameEn`/`nameJa`・`effectEn`/`effectJa`、地域一覧`RegionSummary`の
    `displayNameEn`/`displayNameJa`、`GameApp::currentMissionName()`/
    `currentMissionNameJa()`、`SkillAvailability::reasonEn`/`reasonJa`、
    Export/Import結果の`gSaveStatusMessage`/`gSaveStatusMessageJa`など、
    **データ層が最初からEn/Jaの両方を保持しているフィールドをpick()する**
    既存パターンのみで、`kJa*`定数の直書き債務ではないため意図的に対象外とした
- **段階6の実行可能な範囲(2026-07、完了)**: `tools/check_localization.sh`
  (`CMakeLists.txt`に`check_localization`という名前でCTest登録)を新設し、
  `main.cpp`の`^const std::string kJa`定義が`kJaJapaneseNative`1個以外に
  存在しないことを検査する。ダミーの`kJa*`定数を追加して本チェックが実際に
  失敗することを確認した上で復元。Key集合一致・Formatter・破損Locale検出は
  既存の`jf_locale_tests`が担当。画像ベースの目視検査(spec項目8)はこの環境の
  スクリーンショット制約により対象外のまま
- **継続課題**: 共通UI表の「主目的/副目標/敗北条件/地域成果/加入候補」(段階2、
  現在のUIに該当する表示箇所が無いため対象外のまま)、CI化(spec項目6のAST的な
  厳密検査・項目7のGlyph網羅自動検査・項目8の画像検査)

### M3-C 倉庫超過

状態: **完了**

`docs/inventory_overflow.md`を実装:

- `jf::OverflowStack`/`jf::RewardOverflowState`(`include/jf/core/BaseState.hpp`)、
  `regionKeyMaterialIds()`(既存の`kAshveilFangMaterial`/`kAshenhornFangMaterial`を
  列挙、探索道具・キー素材の1個上限に対応)
- `GameApp::returnToBase()`を`bool`返却へ変更し、帰還前に「倉庫へ入る数量」と
  「上限超過分」(通常素材999、キー素材1)を計算してから確定するTransactionへ書き換え。
  200 Stack超過時は何もコミットせずfalseを返す(呼び出し元の`main.cpp`は倉庫整理
  画面へ誘導)。キー素材の上限超過分は保留へ積まず`docs/inventory_overflow.md`
  「重複除去して直接恒久化する」通り静かに切り捨てる。二重付与防止は正式なGrant
  Ledger(M1-D項目1、未実装のまま)を新設せず、既存の`screen_ != Screen::Camp`
  ガード(`proceedToCamp()`の二重付与バグを直した時と同じパターン)を再利用
- `GameApp::discardStorage()`/`discardItemStorage()`/`discardOverflowStack()`
  (キー素材は放棄不可)と、倉庫整理UI(`main.cpp`の`drawWarehouseCleanupOverlay()`、
  Camp画面で帰還がブロックされた時に自動表示、Base画面からも手動で開ける)。
  放棄は明示的な確認Button1回、確定後にのみ一覧から消える
- `RewardOverflowState`をSave JSONへ追加(`SaveSystem.cpp`)
- 回帰テスト6件(素材999超過分の保留計上、キー素材の重複除去、200 Stack超過時の
  完全不変、放棄の成功/失敗/Cancel相当、同一帰還の二重呼び出し防止)、意図的に
  上限チェック・キー素材除外ロジックを外して失敗確認後に復元

### M3-D Web保存

状態: **デスクトップ完結範囲は完了。Web同期完了待ち・GitHub Pages継続試験は対象外
(実ブラウザ/Pages公開環境がこの環境に無く検証不能なため)**

- **Export/Import**: 実装済みだったことが判明(`jf::exportSaveData()`/
  `importFrom()`/`listImportCandidates()`/`loadImportCandidate()`、
  `drawSettingsOverlay()`のUI) - `save_system.md`/本書の「未実装」記載が古かった
  だけで、ファイル名規則・`.preimport.bak`退避・Schema範囲検証まで仕様通り
- **Schema移行**: `jf::migrateSave()`(`SaveSystem.cpp`、`vN -> vN+1`を一段ずつ適用
  するループ骨格)を新設。現行`v1 -> v2`は`deserializeSave()`が既に新項目を安全な
  既定値で埋めているため実質No-opだが、`SaveStore::load()`が移行前に
  `.schema-vN.bak`へ退避してから適用するようになり、Schema 3(施設再設計、
  save_system.md予告済み)が来た時に実差分だけ足せる状態にした
- **保存状態HUD**: `SaveHudState{Idle,Saving,Saved,Failed}`(`main.cpp`)を新設し、
  画面右下固定位置に表示。自動再試行最大3回(0.5/1/2秒)、手動「再試行」は回数
  制限後も利用可能。デスクトップの同期I/Oでは`Saving`は実質1フレームのみ表示
- **破損復旧画面**: 起動時のSave読込失敗(`SaveStore::load()`が自身の`.bak`
  自動フォールバックも含めて何も読めなかった場合のみ)に、`Restore Backup`/
  `Import Save`/`Start New`の3択画面(`drawSaveRecoveryScreen()`)を表示。
  `SaveStore::restoreFromBackup()`は`load()`自身が試さない`.preimport.bak`/
  `.schema-vN.bak`(新しいバージョン優先)を追加で試す。`quarantineCorruptSave()`
  は破損ファイルを`.corrupt-YYYYMMDD-HHMMSS.json`へ退避(即削除しない)
- 回帰テスト7件(v1→v2移行、`SaveStore::load()`の自動バックアップ+移行、
  `restoreFromBackup()`成功、`quarantineCorruptSave()`の退避+no-op)、意図的に
  移行ロジック(バージョン番号を上げない改変で無限ループも実際に確認)・
  バックアップ書き込みを外して失敗確認後に復元

完了Gate:

- 更新でNodeが変わってもBase進行を失わない
- 上限超過報酬が消えない
- 未対応の将来Schemaを上書きしない
- 日本語UIへ英語、内部ID、代替文字を暗黙表示しない
- Web保存完了前に成功表示しない

## M4 Skill・AI・Boss共通化

状態: **項目1・2(Skill Effect Executor、初期6兵種18 Skill)が完了。項目3(Preview/
Resolverの一致)はDamageを予測する3つの攻撃形状Skill(制圧射撃・足止め突き・奇襲)のみ完了。
項目4(状態異常・地形・Boss補正)は既存コンテンツで可能な範囲(移動低下・よろめき)が完了、
毒・炎上・自Skill防御低下は対応コンテンツ未実装で保留。項目5(増援Wave)はデータモデルと
戦闘内接続が完了、地域接続は薬草の沢のみ。項目6・7(AI候補/Score/小隊予約/兵種別Profile)は
`enemy_ai_rules.md`の完全仕様より小さい5 Profileの簡略版を実装済み(Wolf/Boss以外の全Enemy
経路に接続)。項目8(Boss予告固定・段階移行・撤退区別)は`UnitExitReason`/
`BossStageChangedEvent`/`BossRuntimeState`/`BossTelegraph`を新設し灰角大猪の実装をそこまで
汎用化。**AI強化によりForest全滅率が39.8%→59.2%まで悪化した件は、実プレイ確認の上
「無補給3連戦という撤退前提の最悪ケース指標であり通常プレイには影響しない」と判断し、
数値調整不要のまま様子見で確定済み(`campaign_balance.md`「AI強化後の実測」参照。その後の
DEF調整・撤退実装後も59.2%→60.0%でほぼ不変)**(詳細は各項目の節を参照)**

目的: 初期6兵種の戦術差と、敵勢力ごとの判断差を実戦へ接続する。

正本:

- [`initial_skill_effects.md`](initial_skill_effects.md)
- [`status_effects.md`](status_effects.md)
- [`enemy_ai_rules.md`](enemy_ai_rules.md)
- [`reinforcement_rules.md`](reinforcement_rules.md)
- [`boss_common_rules.md`](boss_common_rules.md)
- [`combat_forecast.md`](combat_forecast.md)

実装Slice:

### M4-A Skill Effect Executor(最小実装)

状態: **完了(18/18 Skill、初期6兵種の全Skillに実効果あり)**

装備スキルの永続選択・Save往復・戦闘開始時のCharge初期化(`initializeSkillCharges`)、
Charge/Cooldown管理(`SkillCharges.hpp`の`skillSlotAvailable`/`consumeSkillCharge`/
`refreshSkillChargesOnPhaseStart`/`availableSkills`)は既にこのMilestone着手前から実装済み
だったが、実際に戦闘中スキルを選んで効果を発動する経路(Executor)が存在しなかった。今回追加:

- `BattleController::chooseSkill(int slotIndex)`/`selectSkillTarget(GridPos)`: 既存の
  `chooseHeal()`/`selectHealTarget()`と同じ形の入力状態遷移(新設`SelectSkillTarget`)を追加し、
  対象決定後に`consumeSkillCharge()`とAttack/Heal同様の`finishPlayerAction()`を呼ぶ
- **リファクタ(ユーザー要望「スキル使いまわせる形式に」)**: 当初はSkillごとにif分岐を
  1つずつ書き足していたが、`BattleController.cpp`の匿名namespaceへ4つの「形状テーブル」
  (`healSkillShapes()`/`attackSkillShapes()`/`buffSkillShapes()`、`cleanse`だけは
  パラメータが無いため`isCleanseShape()`)を切り出し、`chooseSkill()`/`selectSkillTarget()`
  はSkill IDごとの分岐ではなく形状テーブルの検索だけで動くよう書き換えた。同じ形状に
  当てはまる新しいSkillはテーブルへ1行足すだけで実装でき、実際に`ambush`(下記)を
  新規コード0行(テーブル1行のみ)で追加してこれを実証した。この書き換え自体は既存7
  Skillの挙動を変えないリファクタで、既存回帰テストが全てそのまま通ることを確認済み
- 実装済みの4形状と該当Skill:
  - **Heal形状**(`healSkillShapes`): 対象・射程・回復量・HP条件をパラメータ化。
    暁の衛生兵`emergency_treatment`(HP50%以下の味方、射程2、12回復、戦闘1回)
  - **状態解除形状**(`isCleanseShape`、パラメータなし1種のみ): 暁の衛生兵`cleanse`
    (自身または隣接味方1人の毒・炎上・移動低下・防御低下・よろめきを全解除、CD2)
  - **攻撃形状**(`attackSkillShapes`): `confirmAttack()`と同じ攻撃解決
    (`rollAttackHit`/`resolveAttack`/`emitUnitDefeatedEvents`)+追加ダメージ/移動低下付与/
    未行動限定をパラメータ化。監視弓兵`suppressing_shot`・槍兵`halting_thrust`(どちらも
    移動低下付与)、辺境斥候`ambush`(Damage+3、未行動の敵限定。**このリファクタ後に
    テーブル1行だけで追加した最初のSkill**)
  - **バフ形状**(`buffSkillShapes`): `BuffKind{Resistance,Defense,ZocRange}`のどれを
    上げるか・自身+隣接全員(AoE即時解決)/自身のみ(即時解決)/1体選択かをパラメータ化。
    暁の衛生兵`protective_treatment`(RES+3、単体選択)、行軍隊長`hold_formation`(DEF+2、
    自身+隣接全員へ即時)、古参守備兵`extended_lockdown`(自身のZone of Control範囲を
    距離1→2、自身のみ即時。**バフ形状をBool2種からenum+`selfOnly`へ拡張して追加した
    Skill**、`Movement.cpp`の`isStoppedByZoneOfControl()`が`Unit::zocRangeExtended`を
    直接参照するよう修正)。バフは新しい仕組みも必要で、`Unit::resistanceUpActive`/
    `defenseUpActive`/`zocRangeExtended`と`effectiveResistance()`/`effectiveDefense()`
    拡張、専用の`clearSkillBuffsAtEnemyPhaseEnd()`(通常のmoveDown/defenseDownが「対象
    自身の陣営の次Phase終了」で切れるのに対し、この3つは常に「次のEnemy Phase終了」固定で
    切れるため別関数にした)を追加した。槍兵`spear_wall`(槍壁。自身と隣接味方1人、次の
    Enemy Phase終了まで、Spearman兵種の基礎特性Brace(2 Tile以上移動した敵から攻撃される際
    DEF+2、`hasBrace()`/`BattleState::combatDefenseBonus()`)と同じ条件付きDEF+2を、
    まだ持っていないユニットへも一時的に付与)は、既存の1体選択バフ(選んだ1人だけが
    受け取る)と違い「自身と選んだ隣接味方1人の両方」が受け取る初めてのケースだったため、
    `BuffSkillShape`へ`alsoSelf`フラグを追加した(true時は対象選択リストから自身を除外し、
    解決時に選んだ対象へ適用した後もう一度自身にも適用する)。この効果はDEF+2が常時ではなく
    「攻撃者が2 Tile以上移動していた場合のみ」という条件付きのため、`effectiveDefense()`
    ではなく`BattleState::combatDefenseBonus()`側に`Unit::braceSkillActive`を直接
    参照するチェックを追加した(既存3バフの「常時+固定値」パターンとは異なる)
  - **Mark形状**(`markSkillShapes`): 攻撃せず対象へ`Unit::markedBonusDamage`(符号付き)を
    設定するだけ。監視弓兵`mark_target`(敵1体・武器射程、Damageなし、次にこの敵が受ける
    攻撃へDamage+2、その後解除)、行軍隊長`support_order`(隣接味方1人・自身除く、
    Damage-3の被ダメージ軽減シールド。**`targetsAlly`フラグ追加だけでMark形状を再利用し、
    新規フィールド0個で追加したSkill**)。`markedBonusDamage`は`CombatResolver.cpp`の
    `computeDamage()`で読むだけ(`previewAttack()`が副作用を起こさないよう純関数のまま
    維持)、実際に命中した`resolveAttack()`側で0へ戻して消費する - 外れた攻撃では
    消費されない
  - **専用分岐**(形状テーブル化しなかったもの): 行軍隊長`advance_order`(前進命令。
    隣接する未行動味方1人・自身除く、MOV+1、このPlayer Phase終了まで、戦闘1回)。射程外・
    未行動限定・自身除外という組み合わせ、かつ既存3バフが全て「次のEnemy Phase終了」固定で
    切れるのに対しこれだけ「今の」Player Phase終了で切れるという点が既存テーブルの前提と
    食い違うため、`buffSkillShapes`へパラメータを足して押し込むより`isCleanseShape`と同じ
    素朴なid判定+専用分岐にした方が読みやすいと判断した。`Unit::moveUpActive`/
    `effectiveMove()`拡張、`applyMoveUp()`/`clearMoveUpAtPlayerPhaseEnd()`
    (`processPhaseEndStatusEffects(battle_, Team::Player)`の呼び出し箇所に追加)が必要
  - **Passive/自動発動**: 古参守備兵`immovable_stance`(不動の構え)。他11 Skillは全て
    Active(Skillメニューから選んで使う)だが、これは`SkillCategory::Passive`かつ
    `SkillUsageType::Always`で、`chooseSkill()`を一切経由せず`chooseWait()`確定時に
    自動発動する(装備されていれば毎回、Chargeなし)。効果は「次の自分の行動終了まで
    DEF+3、その行動は移動不可」で、発動direct元のWait自体を「次の行動」に数えないよう
    `Unit::immovableStanceActive`/`immovableStanceJustGranted`の2段階フラグにした
    (`chooseWait()`で発動時に両方true、`finishPlayerAction()`で
    `justGranted`をfalseにするだけの回と、`immovableStanceActive`自体をfalseにする回を
    分けている)。`effectiveMove()`/`effectiveDefense()`を拡張。Waitを繰り返すとその都度
    再発動し続ける(1回限りの資源ではない)点に注意
  - **自己移動形状**(専用分岐、形状テーブル化しなかったもの): 辺境斥候
    `emergency_withdrawal`(緊急離脱。自身、最大3 Tile、CD2、攻撃せず移動、敵隣接から
    開始可能、通常占有規則を守る)。対象が「Unit」ではなく「空きTile」という、既存5形状
    どれも前提にしていない初めてのパターンのため、専用の`computeEmergencyWithdrawalTiles()`
    (`BattleController.cpp`匿名namespace)を新設した。通常移動の`computeReachableTiles()`
    (`Movement.cpp`)とは意図的に別実装にしてある: MOV/地形コストを無視した固定3マス予算、
    かつZone of Controlを完全無視(スキルの狙い「敵隣接から開始可能」そのもの)。ただし
    「通常占有規則を守る」の部分は`computeReachableTilesImpl()`と同じ規則
    (敵Unitは経路の途中を含め通行不可、味方Unitは通過はできるが着地不可)を踏襲する必要が
    あり、実装時に一度「最終候補リストでの占有判定のみで、BFS展開中は占有を見ていない」
    というバグを作り込んだ(味方どころか敵Unitが経路上に立っていても素通りできてしまう
    状態)。回帰テストで意図的にこのチェックを外して失敗することを確認した上で
    `Movement.cpp`と同じ「展開時に敵Unitなら経路自体を打ち切る」チェックを追加して修正。
    `selectSkillTarget()`にも、対象Unitの取得を前提とする既存の共有ガード
    (`battle_.unitAt(pos)`が無ければfalse)より前に、空きTileへの`battle_.moveUnit()`を
    直接呼ぶ専用の早期分岐を追加する必要があった(このSkill特有の構造変更)
  - **AI誘導形状**(専用分岐、形状テーブル化しなかったもの): 古参守備兵`provoke`
    (挑発。敵1体・射程2、Damageなし、CD2、次Enemy Phase、使用者を攻撃可能なら対象評価で
    最優先)。Mark形状(敵1体対象・Damageなし)に構造は似ているが、書き込む値が
    `markedBonusDamage`という符号付き整数ではなく「このSkillを使ったUnitのid」
    (`Unit::provokedByUnitId`)で、しかもこの効果を消費するのは`BattleController`側では
    なく`EnemyAI.cpp`の`takeEnemyTurn()`という別ファイルの別コードパスのため、Mark形状へ
    押し込まず専用分岐にした。`takeEnemyTurn()`は通常`findNearestPlayer()`で最寄りの
    プレイヤーUnitを対象にするが、`provokedByUnitId`が設定されていればその発動者を
    最優先の対象として差し替える(Wolf/Boar Bossの専用AIより手前の通常AI経路のみ、
    「Boss予告は変更しない」と一致)。実装時、`attackIfPossible()`の既存フォールバック
    (優先対象が射程外なら別の射程内Unitを代わりに攻撃する)がこの効果を素通りしてしまう
    という問題を見つけた - 挑発された敵がたまたま射程内の別Unitへ攻撃してしまい、
    「最優先」のはずの発動者が無視されるケース。回帰テストで実際にこの問題を再現させた
    上で、`attackIfPossible()`へ`onlyPreferred`引数を追加し(挑発中は優先対象が射程外なら
    フォールバックせず何もしない)、`takeEnemyTurn()`から挑発中は`onlyPreferred=true`で
    呼ぶよう修正した。`clearSkillBuffsAtEnemyPhaseEnd()`で他のバフと同じ「次のEnemy Phase
    終了」で`provokedByUnitId`をクリアする(挑発を受けるのはプレイヤー側ではなく敵Unit
    自身だが、タイミングは同じ関数を再利用)
  - **Reactive(反応)**: 槍兵`counterthrust`(反撃準備。攻撃者・武器射程、戦闘1回、
    単体武器攻撃を受け生存時、攻撃者へ通常攻撃1回)。`SkillCategory::Reactive`が実際に
    使われた初めてのSkillで、これまでの15 Skillと違い`chooseSkill()`/
    `selectSkillTarget()`を一切経由しない - 装備しているだけで、`EnemyAI.cpp`の
    `attackIfPossible()`(`takeEnemyTurn()`と`takeWolfPackTurn()`の両方が共有する、実際に
    `resolveAttack()`を呼ぶ2箇所)から新設の`tryCounterthrust(battle, defender, attacker)`
    を呼び、装備者が攻撃を受けて(命中/外れ問わず「受け」)生存していれば、攻撃者が
    "装備者自身の"武器射程内にいる場合のみ即座に1回反撃してChargeを消費する(攻撃者の
    射程ではなく防御側の射程を見る点に注意 - 弓兵に射程2から攻撃されても、装備者の武器が
    近接射程1のみなら反撃できない)。灰角大猪Bossのsweep/chargeは`resolveAttack()`を
    経由せずHPを直接減らす専用実装のため、この2箇所にフックするだけで自然に対象外になる
    (「単体武器攻撃」の趣旨とも一致)。immovable_stance(Passive)と同様、Reactiveも
    `availableSkills()`のスキルメニュー表示自体はフィルタしていない(装備欄には出るが
    `chooseSkill()`で選んでも他の未実装Skillと同じno-opになるだけで、実際の発動は完全に
    自動)
  - **AI予約形状**(専用分岐、形状テーブル化しなかったもの): 監視弓兵`overwatch`
    (警戒射撃。装備武器の射程、戦闘1回、次Enemy Phase、最初に射程へ入った敵へ通常攻撃
    1回)。`chooseSkill()`側は`hold_formation`/`extended_lockdown`と同じ「自身のみ、対象
    選択なしで即座に解決」パターンだが、書き込む先がBuffKindではなく専用の`Unit::
    overwatchActive`のため、その自己解決分岐に`isOverwatchShape`の特別扱いを1つ足す形に
    した。効果自体は`provoke`と同様`EnemyAI.cpp`側で消費する - 新設の
    `triggerOverwatch(battle, enemy)`を`takeEnemyTurn()`の2箇所(その敵が行動する直前、
    および移動直後)から呼び、`overwatchActive`な自軍Unitのうち、その敵が"監視兵自身の"
    武器射程内に入っていれば即座に1回攻撃してから`overwatchActive`を解除する(Chargeは
    キャスト時点で既に消費済みなので、`triggerOverwatch()`側では消費しない)。
    `provokedByUnitId`と違い「次Enemy Phase」限定の文言がないため、`clearSkillBuffsAt
    EnemyPhaseEnd()`では解除せず、実際に発動するまで複数Enemy Phaseをまたいで持続する
    (`clearAllStatusEffects(BattleState&)`でのみ戦闘終了時にクリア)。現状は通常AI経路
    (Wolf/Boarを除く)のみに配線済み - `provoke`の「Boss予告は変更しない」のような明示的な
    仕様記載はないが、Wolf pack/Boar bossの専用AI関数へも同じフックを複製する作業は
    スコープ外として据え置いた(将来の拡張候補として明記)
  - **仮移動経路記録**(専用分岐・新設のMovement.cpp関数、形状テーブル化しなかったもの):
    辺境斥候`trailblaze`(道拓き。仮移動で通過した灰地・浅瀬、CD2、このPlayer Phase中だけ
    味方の移動Costを1にして行動終了)。これが18 Skill中で唯一、既存のどの機構(移動距離の
    合計を返す`Unit::tilesMovedThisAction`含む)にも「実際に通過した経路そのもの」が
    残っていない効果だったため、`Movement.cpp`の`computeReachableTilesImpl()`へ親ポインタ
    追跡(`parentOut`引数)を追加した上で、新設の`computeMovementPath(battle, mover,
    destination)`がそれを辿って経路を逆算する(起点は除き終点は含む)。`chooseSkill()`側は
    `overwatch`と同じ「自身のみ、対象選択なしで即座に解決」パターンだが、
    `BattleController::selectMoveTile()`が実際の移動(`battle_.moveUnit()`)より前に
    (`mover.position`がまだ起点のうちに)経路を`lastMovementPath_`へキャプチャしておく
    必要があった。解決時、経路上の灰地・浅瀬Tileだけを`BattleState::markTrailblazed()`で
    記録し(平地は無視)、`computeReachableTilesImpl()`へ新設した`costOverrideAt`引数
    (`BattleState::isTrailblazed()`を渡す)が、そのTileのコストを常に1へ上書きする -
    地形本来のコストや、移動する側の兵種に関係なく効く(道拓きの狙いそのもの)。
    `Unit::moveUpActive`と同じ「このPlayer Phase終了まで」なので、
    `clearMoveUpAtPlayerPhaseEnd()`の隣で`BattleState::clearTrailblazedTiles()`を呼ぶ形に
    した。これで18 Skill全てに実効果が入った
- UI: 戦闘HUDの「スキル」メニューが、今まで暁の衛生兵の生得Heal(`canHeal()`、Tier未満の
  クラス専用能力で装備スキルとは別物)しか出していなかったのを、装備中の2スキル枠も
  `BattleController::selectedUnitSkills()`(`SkillAvailability`)経由で表示するよう拡張。
  使用不能スキルは`docs/skill_system.md`「使用不能スキルは非表示にせず、理由付きで無効表示」
  通りグレーアウト+ホバー理由表示にした
- 回帰テスト: 18 Skillそれぞれの対象判定・効果・Cost消費に加え、`ambush`の未行動限定
  フィルタと`Damage+3`の加算、`extended_lockdown`のZoC範囲拡張(`computeReachableTiles`
  レベルでの検証)、`mark_target`のDamageなし付与と命中時のみ消費(外れでは消費しない)、
  `support_order`の自身除外・射程外除外とシールドによる被ダメージ軽減(下限1)、
  `advance_order`の未行動限定・自身除外と`clearMoveUpAtPlayerPhaseEnd`が
  `clearSkillBuffsAtEnemyPhaseEnd`から独立して切れること、`immovable_stance`が
  Enemy Phaseを跨いで持続し、Wait以外の行動(防護板設置)でだけ実際に解除されることを追加
  (Waitの連続では解除されず再発動する境界も別途検証)。`emergency_withdrawal`は固定3マス
  予算(MOV設定と無関係)・ZoC無視(同じ距離3の唯一経路上にVeteranGuardのZoCを置き、通常
  `computeReachableTiles()`では止まる一方このSkillでは通れることを対比)・占有規則
  (経路上の敵Unitは通行不可、味方Unitは通過可だが着地不可)の3点を検証。`spear_wall`は
  「自身が対象選択リストに含まれない」「選んだ隣接味方と自身の両方が`braceSkillActive`を
  受け取る」ことに加え、`combatDefenseBonus()`で攻撃者が2 Tile以上移動した場合のみ
  DEF+2が乗り、移動していなければ乗らないことを検証。`provoke`は対象選択(射程2の敵のみ、
  味方は対象外)、`provokedByUnitId`の設定に加え、`jf::takeEnemyTurn()`を直接呼んで
  「挑発した発動者より近い別のプレイヤーUnitが射程内に既にいても、挑発後は発動者の方を
  攻撃する」ことを検証(意図的に`onlyPreferred`を外して失敗することを確認した上で実装を
  修正 - `attackIfPossible()`の既存フォールバックが挑発を素通りさせるバグを実際に再現)。
  `counterthrust`は近接同士なら反撃が発生すること、攻撃者が防御側自身の武器射程外
  (弓兵が射程2から攻撃、防御側は近接射程1のみ)だと反撃が発生せずChargeも消費されない
  こと、戦闘1回消費後は別の敵から再度攻撃されても反撃しないことの3点を`jf::
  takeEnemyTurn()`を直接呼んで検証(意図的に`tryCounterthrust()`の呼び出しを外して失敗
  することを確認した上で実装が正しくフックされていることを確認)。`overwatch`は
  `chooseSkill()`が即座に解決してChargeも即消費すること、敵がその敵自身の行動開始時点で
  既に監視兵の武器射程内にいれば行動前に撃破されて自分の攻撃は一度も発生しないこと
  (`attacked == nullptr`かつ監視兵側のHPが無傷)、MOVを丁度射程の上限に収まるよう調整した
  敵が移動後に射程へ入って同様に行動前撃破されること、戦闘1回消費後は別の敵が同じ射程に
  入っても発動しないことを検証(意図的に両方の`triggerOverwatch()`呼び出しを外して失敗
  することを確認した上で実装が正しくフックされていることを確認)。`computeMovementPath()`
  単体の経路復元(直線移動、起点除く終点含む、移動なしなら空)、`trailblaze`が経路上の
  灰地・浅瀬だけを記録し平地は記録しないこと、記録後は他のUnitの`computeReachableTiles()`
  でそのTileのコストが実際に1へ下がって従来届かなかったTileへ届くようになること、
  「このPlayer Phase中だけ」の通り最後のPlayer Unitが行動を終えてPhaseが切り替わると
  クリアされることを検証(意図的に`costOverrideAt`のコスト上書きを外して失敗することを
  確認した上で実装が正しくフックされていることを確認)。既存分のテストが
  リファクタ後も無変更で通ることを確認
- **完了**: 初期6兵種18 Skill全てに実効果あり。項目1の残作業は今後追加される兵種/Skillが
  出た時の形状テーブル拡張のみ

### M4項目3 Preview/Resolverの一致(部分実装)

状態: **攻撃形状3 Skill(制圧射撃・足止め突き・奇襲)のみ完了。他Skillは対象外(下記参照)**

通常攻撃は元々`ConfirmAttack`(Target選択 → Preview表示 → Confirmで確定)という2段階
フローだったが、攻撃形状Skillだけは`selectSkillTarget()`が対象Tileを選んだ瞬間に
即座に解決していて、Previewを見せる機会が無かった。これは`initial_skill_effects.md`の
Gate「18 SkillすべてでPreviewと実結果が一致」に反する状態だったため、通常攻撃と対称な
2段階フローを追加した:

- 新設`BattleInputState::ConfirmSkillAttack`: `selectSkillTarget()`は攻撃形状Skill
  (`attackSkillShapes()`に一致するもの)だけ即座に解決せず、この状態へ遷移して
  `pendingTarget_`をセットするだけに変更(他の形状は従来通り即座解決のまま)
- `BattleController::pendingSkillPreview()`(新設、`pendingPreview()`と対になる):
  `ConfirmAttack`用の`previewAttack()`/`computeDamage()`をそのまま再利用し、
  攻撃形状の`bonusDamage`(`ambush`のDamage+3等)があればPreviewの`damage`/
  `targetHpAfter`へ加算する。**PreviewとConfirm後の実際の結果が同じ`computeDamage()`を
  通るため、両者が食い違うことはあり得ない**(Gate要件をコードの構造で保証)
- `BattleController::confirmSkillAttack()`(新設): 実際に`resolveAttack()`を呼び、
  Cost/Charge消費・moveDown付与・`finishPlayerAction()`を行う - 中身は従来
  `selectSkillTarget()`内にあった攻撃形状解決ロジックをそのまま移設したもの
- `cancelAttackSelection()`が`ConfirmSkillAttack`もキャンセル可能な状態として扱うよう拡張
- `main.cpp`: `ConfirmAttack`と同じConfirm/Cancelボタン・Previewポップアップを
  `ConfirmSkillAttack`にも配線(`drawBattleHud()`/メインループのPreview描画分岐)
- 回帰テスト: 制圧射撃・足止め突き・奇襲の3テストを更新し、`selectSkillTarget()`後に
  `ConfirmSkillAttack`へ遷移すること、`pendingSkillPreview()`が返す`damage`と
  `confirmSkillAttack()`後の実際のHP減少量が一致すること(奇襲は`normalDamage + 3`との
  一致も検証)を追加。意図的に`confirmSkillAttack()`のbonusDamage加算を外して失敗する
  ことを確認した上で実装が正しいことを確認
- **対象外(意図的なスコープ限定)**: Heal(`emergency_treatment`)・バフ・状態解除・
  Mark・AI誘導・自己移動などDamageを予測しないSkillは、これまで通り対象Tile選択と同時に
  即座解決する。これらはPreviewする数値(Damage)自体が存在しないため、Gate文言
  「Previewと実結果が一致」の対象外と判断した。将来Heal量などのPreview表示が必要になれば
  同様のConfirmステップを追加できる
- **未検証**: この変更はUI(main.cpp)の入力状態機械に手を入れているが、実機(raylib
  ウィンドウ)上での目視確認は行っていない(ヘッドレス環境のため) - `jf_battle_tests`の
  回帰テストとビルド成功のみで検証した
- **修正済みバグ**: この変更を実装した直後の評価で、`confirmSkillAttack()`が
  `weapon.causesKnockback`を一切見ておらず`battle_.applyKnockback()`も呼んでいない
  ことが判明した(`confirmAttack()`/`EnemyAI.cpp`の全攻撃経路は呼んでいるのに、ここだけ
  抜けていた)。重槍(Heavy Spear、現状唯一`causesKnockback: true`を持つ装備武器)を
  持った槍兵が`halting_thrust`を使うと、命中・移動低下は起きるのにノックバックだけ
  発生しない実在のバグだった。`resolveAttack()`直後に他の攻撃経路と同じ
  `causesKnockback`チェックを追加し、Heavy Spear装備での`halting_thrust`使用時に
  実際に1マス押し出されることを検証する回帰テストを追加(意図的にチェックを外して
  失敗することを確認した上で修正)

### M4項目4 状態異常・地形・Boss補正(現状のコンテンツで可能な範囲は完了)

状態: **既存コンテンツ(装備Skill・武器特性・Battle Object)から実際に呼べる分は配線完了。
毒・炎上・自Skill経由の防御低下は対応する武器・敵・地形コンテンツ自体が未実装のため保留**

`status_effects.md`は5種の状態異常のデータ基盤(`Unit`フィールド、`applyX()`、Phase処理、
UI)が完成済みだったが、実際にそれを付与する攻撃・スキルが1つも無い状態だった(同ドキュメント
自身の記載)。調査の結果:

- 移動低下は既にM4-Aで配線済みだった(監視弓兵`suppressing_shot`・槍兵`halting_thrust`が
  命中時に`applyMoveDown()`を呼ぶ) - `status_effects.md`の「未実装」記載が古くなっていたので
  今回訂正した
- よろめきの主な発生源「障害物へのノックバック衝突」を新規実装: `BattleState::
  applyKnockback()`が、ノックバック先が範囲外・他Unit占有・不可通行地形・Battle Object
  (倒木などのBarrier)のいずれかで塞がれている場合に`applyStagger()`を呼ぶよう変更。実装中に
  **既存バグ**を発見した - `applyKnockback()`はBattle Objectの占有を一切見ておらず、
  Barrier(倒木)を無視してノックバックが素通りできてしまっていた。同じ修正でこれも解消
  (`objectBlocksMovementAt()`/`objectBlocksStoppingAt()`を`BattleState::moveUnit()`と
  同じ形でチェックするよう追加)
- 毒(獣・魔物の攻撃が主な発生源)・炎上(投擲火炎壺/火系魔法/炎上床)・自Skill経由の防御低下
  (武器・工兵スキル)は、対応する敵・アイテム・武器・クラス自体がまだ実装されていないため
  保留。「倒木衝突」経由の防御低下はBoss専用の別ID(`bossWeakenedFromStun`)で既に実装済みで、
  本書の「通常のdefenseDownActiveとは意図的に別管理」方針とも一致する
- 副次的な整理: 暁の衛生兵`cleanse`が5フィールドを直接リセットしていたのを
  `clearAllStatusEffects(Unit&)`呼び出しへ統一(重複コードの削減、挙動は変更なし)
- 回帰テスト: ノックバックがUnit占有で塞がれた場合とBattle Objectで塞がれた場合の両方で
  よろめきが付与されることを検証(意図的にstagger付与のチェックを外して失敗することを
  確認した上で実装が正しいことを確認)。`cleanse`のリファクタも既存テストが無変更で通ることを
  確認

### M4項目8 Boss予告固定・段階移行・撤退区別(部分実装)

状態: **`UnitExitReason`(退場理由)と`BossStageChangedEvent`(段階移行)を新設し、灰角大猪の
既存実装をこの2点だけ汎用型へ抽出した。突進予告のバナー・Tileハイライト表示も完了。
予告構造体自体の汎用化は次のBossが実装されるまで保留**

`boss_common_rules.md`は全Boss共通の退場理由・Phase移行・予告固定ルールを定義しているが、
現状は灰角大猪1体分の実装しかなく、その全てがこのBoss専用のアドホックなフィールド
(`chargeTelegraphed`/`chargeDirection`/`bossEnraged`等)で表現されていた。今回:

- `jf::UnitExitReason`(`Defeated`/`Retreated`/`Escaped`/`Surrendered`/`ScriptedWithdrawal`、
  `jf/core/Unit.hpp`)を本書の定義通りに新設し、`Unit::exitReason`フィールドを追加。
  `ObjectiveTracker.cpp`の`emitUnitDefeatedEvents()`(HP0を検知する唯一の箇所)で、
  対象が灰角大猪なら`ScriptedWithdrawal`、それ以外は`Defeated`をその場で設定する
- `jf::BossStageChangedEvent`(`jf/battle/BattleEvents.hpp`、`BattleEventPayload`variantへ追加)
  を新設し、灰角大猪の激昂(`bossEnraged`がfalse→trueへ切り替わる瞬間)で1回だけ発行するよう
  `EnemyAI.cpp`の`takeBoarBossTurn()`を変更
- 回帰テスト: 灰角大猪のHP0で`exitReason`が`ScriptedWithdrawal`になること、通常Unitの
  HP0では`Defeated`のままであること、激昂の瞬間に`BattleMissionState::consumedEventIds`が
  増える(=`BossStageChangedEvent`が実際に発行されている)ことを検証。意図的に`exitReason`の
  設定を外して失敗することを確認した上で実装が正しいことを確認
- **上記の後に追加実装**: `jf::BossRuntimeState`/`jf::BossTelegraph`
  (`jf/battle/BossRuntime.hpp`、`TelegraphShape`/`TelegraphState`込み)を新設し、予告行動
  (行動ID・形状・予告/実行Round・対象・固定Tile・方向)を1つの構造体へ汎用化。灰角大猪の
  突進は`Unit::bossRuntime.telegraph`を実行時の正本として使うよう移行し、予告・消費の
  タイミングで`jf::BossTelegraphChangedEvent`を発行する。これにより「2体目のBossまで保留」
  としていた汎用化は完了扱いとする
- **突進予告のUI表示(2026-07)**: `BossTelegraphChangedEvent`も`consumedEventIds`の
  書き込み専用制約(増援Wave予告と同じ)を持つため、`main.cpp`は
  `Unit::bossRuntime.telegraph.state`を毎フレームポーリングする方式
  (`reinforcementUiStates()`と同じ手法)で`None→Announced`遷移を検知し
  `pushBattleMessage()`でバナーを表示する。`BossTelegraph::lockedTiles`は
  「攻撃列」の空間情報として定義されていたが、`EnemyAI.cpp`の3箇所の予告生成コードが
  いずれも`{}`のまま一度も設定していなかったため、`executeBoarCharge()`の実行時Walk
  (行固定・盤面端または遮蔽Objectで打ち切り)を副作用なしで再現する
  `computeBoarChargeTiles()`を新設し、予告時に実際に埋めるよう修正した。これにより
  `main.cpp`はイベントを介さずこの確定済みフィールドを読むだけで済み、
  `objectTargetableTiles()`と同じ`containsTile`+`DrawRectangleRec`パターンで
  警告色のTileハイライトを表示する。既存の灰角大猪突進テスト3件に、予告直後の
  `lockedTiles`が実行時のTile列と一致することの確認を追加(意図的に
  `computeBoarChargeTiles()`を空配列へ差し替えて失敗を確認した上で復元)
- **継続課題**: Objective側の退場理由Filter(「Boss素材は許可した退場理由でのみ付与」)は
  依然未着手 - `ScriptedWithdrawal`は常に「撃破相当」として扱われ、`DefeatUnit`/
  `EliminateTeam`の`!unit.isAlive()`判定と挙動が一致するため、ゲートを足しても現状は
  何もフィルタしない。`Retreated`/`Escaped`/`Surrendered`を実際に使うBoss・敵が出た時点で
  Definition側にFilterを追加する

### M4項目5 増援Wave・予告・封鎖・延期・Save(データモデルと戦闘内接続は完了)

状態: **`ReinforcementWave`のデータモデル・検証・予告Event・Phase開始時の出現解決・必須Wave
による早期勝利防止・予告UI表示までは完了。地域Definition側の接続は薬草の沢の採取ルート増援のみ**

`reinforcement_rules.md`の共通仕様のうち、`BattleState`側の仕組みを実装:

- `jf::ReinforcementWave`/`ReinforcementState{Scheduled,Announced,Spawned,Prevented,
  Cancelled}`(`jf/battle/Reinforcement.hpp`)、`validateReinforcementWaves()`(id重複・
  出現Round・出現候補・Wave内Unit数1-4・合計上限を検証)
- `BattleState::addReinforcementWave()`/`announceReinforcements()`(予告Round到達で
  `ReinforcementAnnouncedEvent`を発行)/`resolveReinforcementsForPhase()`(出現Round到達時、
  候補Tileが全て塞がっていれば`Prevented`、そうでなければ出現させ行動済み扱いにして
  `ReinforcementResolvedEvent`を発行)を`beginPlayerPhase()`/`beginEnemyPhase()`から呼ぶ
- `hasPendingRequiredEnemyReinforcements()`: 必須Wave(`requiredForElimination`)が
  未解決の間、`EliminateTeam`が早期に成立しないようにする(旧「2ラウンド目より前に初期狼を
  倒しても、増援予告中は勝利にしない」という薬草の沢固有ルールを共通機構化したもの)
- 地域接続: `StageDescriptor::timedReinforcement`(薬草の沢の採取ルートで2ラウンド目に狼1体、
  `docs/regions/ashbough_forest.md`)を`BattleFactory.cpp`が`ExplorationOutcome::
  enableReinforcementWave`経由で`ReinforcementWave`へ変換する - 以前「増援の仕組み自体が
  未実装」として保留していたHollowの既知ギャップがこれで解消された
- 増援予告のUI表示(2026-07): `ReinforcementAnnouncedEvent`は`BattleMissionState::
  consumedEventIds`という書き込み専用の重複排除セットにしか記録されず、ペイロード自体を
  UI側から読み出す経路が無いため、`lastSeenAttackEvent`/`lowHpWarnedUnits()`と同じ
  「毎フレーム状態をポーリングし前フレームとの差分を検出する」方式を採用した。
  `src/main.cpp`に`reinforcementUiStates()`(Wave id→直近`ReinforcementState`のMap)を
  新設し、`drawGrid()`内で毎フレーム`battle.reinforcementWaves()`を走査、
  `Scheduled→Announced`の遷移を検知した回だけ`pushBattleMessage()`で
  `battle.reinforcement_announced`({round}引数)のバナーを1回表示する。表示する情報は
  `spawnRound`のみに限定し(`orderedSpawnCandidates`等の実出現Tileはデータ上「開示済み」
  という印が無いため非表示)、`Prevented`/`Spawned`/`Cancelled`後の追加バナーは対象外
  (今回のScope)
- **継続課題**: 他地域・他Stageからの`ReinforcementWave`データ接続、中断Save時の
  `ReinforcementWave`状態の永続化(現状は戦闘状態自体を保存しない既存方針に従い、
  再開時にSeed+Definitionから再生成する想定のまま)、Boss予告(`BossTelegraphChangedEvent`)
  側のUI表示は依然未配線(今回追加した予告UIの対象は増援Waveのみ)

### M4項目6・7 AI候補・Score・小隊予約・決定論的同点処理/兵種別Profile(簡略版を実装)

状態: **`enemy_ai_rules.md`が定義する完全仕様(8 Role、6 Faction、撤退/降伏候補、Object操作
候補)より小さい、5 Profileの簡略版を実装して通常Enemy経路(Wolf/Boss以外)へ接続済み**

- `jf::AiCandidate`(行動種別・目的地・対象・Score・任務進行・予測与ダメージ/被ダメージ・
  移動コスト・撃破可否)、`jf::AiProfile`(Wolf/Human/Defender/Ranged/Supportの5種、
  `damageWeight`等の重み)、`generateAiCandidates()`(到達可能マスごとに攻撃・支援・移動候補を
  生成)、`chooseBestAiCandidate()`+`candidateLess()`(Score→任務進行→撃破可否→被予測
  ダメージ→移動コスト→対象ID→目的地行/列→行動ID、という決定論的同点処理の全段を実装)
- `jf::AiSquadReservations`(停止マス・予約Damage・支援対象)を`BattleController`が
  Enemy Phase全体で1つ保持し(`enemyReservations_`)、`takeEnemyTurn()`へ渡す - 同じ対象へ
  複数体が過剰集中しないための小隊予約(`enemy_ai_rules.md`の該当項目)にあたる
  (`clear()`のタイミングは`beginEnemyPhase()`と同じ粒度)
- Boss(`AshenhornBoar`専用AI経路)以外の全Enemyがこの経路を通る。Wolfの専用群れAI関数
  (`takeWolfPackTurn`/`chooseWolfTarget`)は本項目の導入で完全にデッドコード化していたため
  削除し、Wolfも他のEnemyと同じ`takeEnemyTurn()`経路(`AiProfileId::Wolf`)を通ることを
  明記した。Wolf Pack ProfileのcohesionWeight、Ranged Profileの`preferredRange`維持
  ペナルティ、Support Profileの隣接負傷味方への回復(固定8 HP)などがEnemy行動の実際の差
  として現れる
- `docs/campaign_balance.md`「AI候補/Score導入後の実測」で、この変更により`jf_forest_balance`
  の3地点連続全滅率が39.8%→59.2%(目標35-45%レンジを大きく超過)まで悪化したことを確認 -
  敵側がより賢く(過剰集中回避・危険地形回避・役割別ポジショニング)なった分、単純に難度が
  上がっている。原因調査の結果、狼の攻撃力に対する古参守備兵とその他クラスのDEF差が過大
  だったことが判明し、`data/classes.json`のDEF調整で対応した(詳細は`campaign_balance.md`
  「古参守備兵への過度な依存とDEF調整」参照)
- **M4深掘り(2026-07)**: `AiProfileId`に`Bandit`を追加し、`profileFor()`へ専用の重み
  (`lowHpWeight`を他Profileより高く=低HP対象への攻撃を優先、`pursuitLimit`を短く=深追い
  しない)を実装。`enemy_ai_rules.md`「Faction差分」の野盗(低HP・離脱経路を高評価)のうち、
  現行の`AiCandidate`生成で表現できる範囲(低HP優先・追跡制限)だけを反映し、Loot Container/
  離脱経路の評価はObject/Exit認識が無いAI候補生成にまだ組み込めないため保留した。これは
  ドキュメントが定義する`AiFactionProfile`(Faction)+`AiRole`(Role)の2軸モデルではなく、
  既存の単一`AiProfileId`列挙へ1種類追加しただけの簡易対応である点に注意
- **撤退実装(2026-07)**: `enemy_ai_rules.md`「撤退と降伏」の撤退部分を実装。`Unit::hasExited`
  (新規bool)+`Unit::isPresent()`(=`isAlive() && !hasExited`)を追加。`isAlive()`自体はHPのみの
  判定のまま変更せず(既存の「生存=HPあり」前提の呼び出し箇所を壊さないため)、「まだ盤面上の
  脅威/対象か」を問う箇所だけ`isPresent()`へ置き換えた(`unitAt()`、`Movement.cpp`の
  `unitAtTile()`/`isStoppedByZoneOfControl()`/`computeTargetableTiles()`、
  `allEnemiesDefeated()`、`ObjectiveTracker.cpp`の`EliminateTeam`)。`DefeatUnit`は意図的に
  `isAlive()`のまま(単なる撤退では特定Boss撃破目標を満たさない)。
  `AiActionType::Retreat`を追加し、`generateAiCandidates()`へ`AiProfile::retreatHpPercent`
  (既定25、Wolfは20)以下のHPで撤退候補を生成するブロックを追加。撤退先は盤面右端列
  (`kGridCols-1`、`BattleFactory.cpp`の`kRightZoneMaxCol`と同じ「右端=Exit」慣習に合わせた)。
  `EnemyAI.cpp`側の解決で`hasExited=true`・`exitReason=UnitExitReason::Retreated`をその場で
  設定(HPは変更しないため`emitUnitDefeatedEvents()`は経由しない)。
  実装中に致命的な潜在バグ2件を発見・修正: `BattleState::isTeamDone()`と
  `BattleController::nextUnactedEnemy()`が`isAlive()`のままだと、撤退後も`isAlive()`は
  true・`hasActed`は次の`beginEnemyPhase()`でリセットされ続けるのに、`takeEnemyTurn()`側は
  `isPresent()`で即return nullptrするため、Enemy Phaseが永久に完了しなくなる(`isTeamDone()`が
  常にfalse、`nextUnactedEnemy()`が同じ幽霊ユニットを返し続ける)。両方revert→テスト失敗確認
  →復元の手順で検証済み。テストは新規5ブロック(撤退成功/MOV不足でブロック/必殺可能なら撤退
  より優先/`EliminateTeam`が撤退で達成される+`isTeamDone`のPhase跨ぎ回帰)を追加、全て通過。
  `jf_forest_balance`でAshbough Forestへの数値影響を確認したところ完全に同一(Tactical
  3地点連続 40.0%/60.0%、変化なし) — 狼のHP16・DEF2に対し現行の与ダメージが大きく、
  撤退閾値(HP20%以下)に達する前にほぼ即死するケースが大半のため
- **継続課題**: `AiFactionProfile`/`AiRole`の完全な2軸モデルへの再設計、`AiRole`の残り3種
  (Flanker/Engineer/Courier)、降伏候補の生成(未着手)、Objective/Object操作候補、
  難易度Profileによる候補保持数/先読み調整

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

状態: **項目3・4(解体・稼働枠モデルの廃止、建設済み施設の常時利用)が完了。項目6
(原子的Transaction)は調査の結果、現行アーキテクチャでは二重適用経路が存在しないと判明し
対象外とした。項目8(拠点段階進行・外観変化)は調査の結果、次段階(辺境集落)の解放条件が
M9地域「灰鉄採石場」の攻略を前提としており**保留**と判明。項目1・2(JSON Definition化、
旧ID Alias)は未着手。項目5・7は既存のC++ハードコード`FacilityNode`レジストリの範囲で
実質的に成立済み**

目的: 遠征成果を新しい戦術選択へ変換する拠点ループを完成させる。

正本:

- [`base_development.md`](base_development.md)
- [`facility_research.md`](facility_research.md)
- [`facility_data_contract.md`](facility_data_contract.md)
- [`facility_ui.md`](facility_ui.md)
- [`item_system.md`](item_system.md)

このMilestoneの正式仕様は全12兵種の武器分岐(現状6兵種のみ実装)・10地域にまたがる
施設研究ツリー(現状1〜2地域のみ実装)・JSON Definition化まで含む非常に大きな仕様。
ユーザーと相談の上、まず「解体・稼働枠モデルの廃止」という中核的なギャップだけに
今回のSliceを絞った(JSON Definition化・兵種武器分岐拡張・地域別施設コンテンツ拡張は
対象外のまま)。

実装Slice:

1. Facility、Research、Recipe JSONとLoader
2. 旧ID AliasとSchema移行
3. **解体・稼働枠モデルを廃止(2026-07、完了)**: `FacilityNode::occupiesFacilitySlot`
   (救護テント/訓練場/工作台/簡易鍛冶台の4施設)を対象に、`BaseState::
   facilitySlotCapacity()`(段階ごとに2/4/6という同時稼働枠)、
   `GameApp::dismantleFacilityNode()`(素材50%返却で解体)、
   `GameApp::rebuildFacilityNode()`(空き枠があれば無料で再建)を完全に削除。
   `Facilities.hpp`の`facilityNodeEligible()`と`main.cpp`の
   `facilityNodeBlockedReason()`から稼働枠上限チェックを削除し、
   `applySaveData()`の稼働枠超過時読込拒否も削除(unlockedかつ
   occupiesFacilitySlotでなければbuiltNodeIdsから除去する自己整合ロジックは
   防御的なものとして維持)。`unlockFacilityNode()`が元々
   unlockedNodeIds/builtNodeIds両方へ一括挿入していたため、これらを削除するだけで
   「一度建設した施設は恒久的に利用可能」という正式モデルへ自然に一致した
   (`BaseState`の構造・フィールド名変更は不要)
   - UI(`src/main.cpp`の`drawFacilityNodeRow()`)もDismantle/Rebuildボタンを削除し、
     「unlocked → 常に建設済み表示」「未unlockedかつeligible → Build/Unlockボタン」
     「それ以外 → 理由表示」の3分岐だけに単純化
   - `base_development.md`完了条件#12「UIに施設Lvや稼働状態を表示せず」も同じ趣旨の
     一部と判断し、`facilityLevel()`を使った「Lv N」表示(施設一覧カード、
     ホバーツールチップ、Forge詳細ページの3箇所)を「解放済み分岐 N」
     (`ui.facilities.branches_unlocked`、Formatter付きLocale Key)へ置き換えた。
     施設一覧画面の「施設枠: N/上限」表示も同じ理由で削除し、現在の開拓段階名の
     表示に置き換えた
   - 回帰テスト(`tests/test_battle.cpp`): 3回の実勝利だけで貯まる素材
     (wood:10、hide:5、herb:2)だけを使って4つの初期施設すべてを稼働枠を気にせず
     同時に建設できることを検証する形へ2つの既存テストブロックを1つへ統合・
     書き換え(旧テストはdismantle→rebuildのやり取りで空き枠を作る内容だった)。
     稼働枠チェックを意図的に復活させて新テストが実際に失敗することを確認した
     上で復元
   - **レビュー対応追記(2026-07)**: 上記完了時点では`BaseState::builtNodeIds`/
     `occupiesFacilitySlot`のフィールド名を意図的に変更しないと決めていたが、
     コードレビューで`facility_data_contract.md`が定める正式フィールド名
     `constructedFacilityIds`との不一致を指摘され、`builtNodeIds`を
     `constructedFacilityIds`へ改名した(`BaseState.hpp`/`Facilities.hpp`/
     `GameApp.cpp`/`SaveSystem.cpp`/`main.cpp`/`test_battle.cpp`全箇所。JSON
     キー名`"builtNodes"`は変更していないためセーブ互換に影響なし)。また
     `main.cpp`の`facilityIsActive()`が稼働中/未稼働という表示を出しており
     「稼働・停止状態を持たない」という正式仕様と矛盾していたため、
     `facilityIsConstructed()`へ改名し表示文言も建設済み/未建設へ変更した
4. **建設済み施設の常時利用(2026-07、完了)**: 上記のTransaction廃止の直接の帰結
   として、`equipWeaponForUnit()`/`equipTuningTraitForUnit()`等の
   `constructedFacilityIds`参照先が二度と縮まなくなったため、機能が施設状態に
   よって失われることが無くなった
5. 施設一覧、訪問、研究、製作、確認UI(既存のハードコードレジストリ+今回のUI単純化の
   範囲では実質的に成立。JSON Definition化後の再構築は未着手)
6. **原子的な建設・研究・製作Transaction(2026-07、調査の上「対象外」と判明)**:
   `facility_data_contract.md`が求める「連打やSave再試行でも同じTransaction IDを
   二重適用しない」の実害経路を洗った結果、現行アーキテクチャには存在しないと判断した。
   - `unlockFacilityNode()`(建設・研究共通): 冒頭の`facilityNodeEligible()`が
     `unlockedNodeIds.count(node.id)`を見て既に解放済みなら`false`を返すため、同じ
     ノードへの二重適用は呼び出し自体が構造的に成立しない
   - `craftItem()`(製作): 全素材を検証してから消費する2段階ループのため、コスト検証
     失敗時は何も消費しない(部分適用なし)
   - 入力経路: `main.cpp`の`clicked`は`IsMouseButtonPressed()`(そのフレームで押された
     瞬間だけtrueになるEdge-trigger)なので、1回の物理クリックで同じButtonが2回発火する
     経路が無い
   - Save再試行: `runAutoSaveTick()`はコマンドの再実行ではなく、その時点のメモリ上の
     状態を丸ごと再シリアライズしてディスクへ書き直すだけ - 「同じTransactionをもう
     一度適用する」動作自体が存在しない(Undo可能な操作ログ方式ではなく全状態Snapshot
     方式のため)
   - `facility_data_contract.md`のTransaction ID要件は、要求を再送しうるネットワーク
     経由の操作(M3-D Web同期)を想定したものと判断し、M3-D着手時に必要なら再評価する
7. ユニットページの装備変更(既存実装で成立。Forge分岐の兵種拡張は対象外)
8. **地域攻略による拠点段階と外観変化(2026-07、調査の上「保留」と判明)**:
   `OutpostStage`の`Encampment→PioneerOutpost`のみ実装済み。`base_development.md`
   (442-445行目の開拓段階進行表)によると次の`PioneerOutpost→FrontierSettlement`は
   「灰鉄採石場」攻略・採掘技術記録・安全帰還と、石材4/鉄鉱石4/木材4を要求する。
   灰鉄採石場は`M9 残り8地域`(状態: 設計済み、実データ未作成)所属の未実装地域で、
   「石材」「鉄鉱石」も現在ゲーム内に存在しない素材。`FrontierSettlement`向けの
   `FacilityNode`(訓練場・救護テント等の第2分岐)はこの将来段階を見越して
   既に登録済みだが、解放条件自体をM9データなしに正しく実装することはできない。
   `eligibleForOutpostStage()`の`next == PioneerOutpost`以外を`return false`で
   保留している既存コードのコメント(「Later stages' requirements are not defined
   yet」)通りの状態であり、これは未着手ではなく状態表記の「保留: 前提Milestone待ち」
   に該当すると判断した。それ以降の段階(開拓都市)・外観変化・共同施設研究も同様に
   M9以降の地域データが前提

完了Gate:

- 建設済み施設を切替なしで利用可能
- 研究・製作を再実行して素材を二重消費しない
- 旧Saveの施設・研究IDを正式IDへ移行
- 不正参照、研究循環、Locale欠落を起動時検出
- 施設説明、必要素材、効果が見切れない

## M6 沈黙した監視所群

状態: **完了(2026-07)**。M6-A/B(地点1・2、CinderwatchのRoute Graph化、地点3A/3Bの
分岐とキャンプII、地点3B旧兵舎の実コンテンツ)、M6-C項目1〜3(地点3A本仕様の一部、
地点5信号塔下層、地点6最後の信号、6地点すべて実コンテンツ化)、M6-D(最低保証報酬、
灰鉄採石場解放、4〜6周実測)が完了。地点3A/5/6の個別残作業はM6-C各項目の保留として
継続、加入候補はM7項目2へ、敵増援と任務AIの一般化はM4へ移管済み

目的: 共通データだけで最初の4〜6周型地域を完成させる。

正本:

- [`regions/cinderwatch_gate.md`](regions/cinderwatch_gate.md)
- [`campaign_route_graph.md`](campaign_route_graph.md)

実装Slice(M2のA/B/C/D方式で細分化):

M6以降のSlice運用:

- まずSystem(データ定義、Runtime State、勝敗/報酬/保存、再生成耐性、テスト)を実装する
- 同じSlice内で、実プレイ確認に必要なMinimum UIだけ接続する。Objective、Route、Object、
  報酬、成功/失敗条件をプレイヤーが認識できないUI不足はPolish扱いにしない
- 専用演出、見た目の作り込み、細かい快適化は各Slice末尾のPolishまたはM10へ送る
- UIだけを先行して作る項目は原則作らない。例外は、後続Systemの検証に必要な
  デバッグ/確認UIだけとする

コンポーネント分離メモ:

- 戦闘ルール、Objective、Battle Object、地形生成、Save、Locale、画面単位UIはおおむね
  分離済みで、現時点では大規模リファクタを先行しない
- `GameApp`は遠征状態遷移、Route、報酬、施設、Save適用、装備/Skill変更の入口が集中して
  太くなりつつある。必要になった範囲から`ExpeditionService`相当へ小さく切り出す運用を
  継続する(`ExpeditionService`着手済み、UI Screen State構造体化第1弾着手済み、Audio分離は
  実要件が無いため見送り済み - 詳細: [`implementation_status.md`](implementation_status.md#m6-沈黙した監視所群-実装詳細2026-07))

### M6-A 地点1(外門)・地点2(監視所)・キャンプI

状態: **完了(2026-07)**。地点1(シンダーウォッチ外門)・地点2(灰道の監視所)を
正本通りの内容で実装。詳細: [`implementation_status.md`](implementation_status.md#m6-沈黙した監視所群-実装詳細2026-07)。

### M6-B 地点3A/3B分岐・キャンプII・地点3B(旧兵舎)

状態: **分岐機構と地点3Bの戦闘コンテンツは完了(2026-07)。地点3A(物資庫)の本仕様
コンテンツ(護衛対象・工兵加入候補)と、両地点の加入候補報酬は未着手のまま保留**。
詳細: [`implementation_status.md`](implementation_status.md#m6-沈黙した監視所群-実装詳細2026-07)。

### M6-C項目1 地点3A(物資庫)本仕様コンテンツ(一部)

状態: **地形・敵編成・探索2択・物資箱確保の副目標は完了(2026-07)。工兵救出/
工作兵護衛・3つ目の探索選択・状態条件付き増援・加入候補は未着手のまま保留**。
詳細: [`implementation_status.md`](implementation_status.md#m6-沈黙した監視所群-実装詳細2026-07)。

### M6-C項目2 地点5(信号塔下層)本仕様コンテンツ

状態: **地形・敵編成・2つの操作可能Device(副信号機・主信号機)・軍旗保管箱の
副目標・地点6プレースホルダーへの分割は完了(2026-07)。6ラウンド制限・イベント
条件付き増援の正確なトリガー・3つ目の探索選択・軍旗記録discoveryは未着手のまま保留**。
詳細: [`implementation_status.md`](implementation_status.md#m6-沈黙した監視所群-実装詳細2026-07)。

### M6-C項目3 地点6(最後の信号、元守備隊長ボス)

状態: **地形・敵編成・ボス撃破の主目的・味方戦闘不能者0の副目標・`[行軍隊長]`探索選択・
ラウンド3増援は完了(2026-07)。ボス固有行動3種・主信号機の耐久/破壊敗北条件・
「元守備兵2人以上撤退」副目標・軍旗記録discoveryは未着手のまま保留**。
詳細: [`implementation_status.md`](implementation_status.md#m6-沈黙した監視所群-実装詳細2026-07)。

### M6-D 地域締め

状態: **完了(2026-07)**。地域の最低保証報酬、灰鉄採石場(最小プレースホルダー地域)の
解放、4〜6周の実戦計測、残り保留項目の移管先確定(前Slice でドキュメント側は完了済み)。
詳細: [`implementation_status.md`](implementation_status.md#m6-d-地域締め-実装詳細2026-07)。

1. 地域の最低保証報酬(6地点通しての資材下限保証、取り逃し分の最終地点回収) - 完了
2. 4〜6周の実戦計測(実測して初めてバランス調整の要否を判断する) - 完了、記録のみ
3. 灰鉄採石場(次地域)の解放 - 完了(最小プレースホルダー、本格コンテンツはM9)
4. 各地点の残り保留項目の正式な移管先を確定する:
   - 地点3A/5/6の個別残作業(工兵救出・工作兵護衛・6ラウンド制限・ボス固有行動3種など)は
     引き続きM6-C各項目の保留として扱う
   - 加入候補(辺境工兵・伝令騎兵・旗手)はM7項目2へ
   - 敵増援と任務AIの一般化はM4(Skill・AI・Boss共通化の残り)へ
   - 未整理のまま残るものはM10(公開品質)の仕上げへ

完了Gate:

- 地域固有のC++分岐なしで6地点を生成
- 各周回で最低1つの恒久成果を持ち帰れる
- 確保済み前半を一括通過して後半へ消耗を温存できる
- 熟練プレイの早期攻略を周回数で禁止しない

## M7 12兵種・仲間・会話

状態: **項目1(後半6兵種のClass・武器・固有能力・スキル)完了。項目2(加入経路)は
5/6兵種(重装兵・辺境工兵・伝令騎兵・旗手・辺境猟兵)まで完了。項目3(ユニットページと
装備共有)は武器重複装備の防止+全兵種向け装備スキル2枠UI+探索能力表示+比較パネル
まで完了、一覧(加入候補非表示は既存実装で対応済み)/連携作戦/特性・武器分岐の
他兵種拡張は未着手**。詳細:
[`implementation_status.md`](implementation_status.md#m7-12兵種仲間会話-実装詳細2026-07)。

項目2の加入基盤(Pending加入候補→安全帰還で恒久化→加入確定でRoster追加+Tier1スキル
自動装備、受け入れ枠段階)は`data/units.json`の`recruits`配列+`GameData::
recruitDefinition()`によって完全にデータ駆動化された - 新しい加入IDをJSONへ追加する
だけで加入確定・拠点UI・Save復元がすべて自動的に動く。辺境猟兵(`ranger_recruit`)は
灰枝の森地域完了(森の踏査記録registered)を条件として旗手と同型のPending経由で実装済み。
残り1兵種(戦闘魔導士`mage_recruit`)は灰鉄採石場の本格攻略(M9)に依存するため、
`recruits`配列への追加+候補付与条件の配線はその時点で行う。

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

## M8-A 商店

状態: **設計済み、未実装**

目的: 遠征前の消耗品補給と少量の基礎素材補助を追加し、遠征失敗後の立て直しを可能にする。
商店は地域探索、施設研究、鍛冶、工房、診療所の代替にしない。

正本:

- [`shop_system.md`](shop_system.md)
- [`item_system.md`](item_system.md)
- [`inventory_overflow.md`](inventory_overflow.md)
- [`base_development.md`](base_development.md)
- [`expedition_preparation_screen.md`](expedition_preparation_screen.md)

前提:

- M5の倉庫、製作、上限、施設研究が正式モデルへ移行済み
- M7のユニットページと装備共有が、商店で武器を売らない前提に沿って動作
- クラウンをSaveできる場所をM3またはM8-A内で追加する

実装Slice:

1. `ShopDefinition`、`ShopStockEntry`、価格表Version、在庫条件のデータ化
2. `BaseState`へ所持クラウン、商店在庫、入荷済み商品、購入済み情報商品を追加
3. 常時販売の基本消耗品4種
4. 研究後販売の解放済み消耗品
5. 少量の基礎素材販売と倉庫上限チェック
6. 情報商品を遠征準備画面へ反映
7. 商店UIの4タブ: 消耗品、素材、情報、便利品
8. 購入Transaction、クラウン不足、在庫不足、上限超過、Save失敗の失敗系
9. 価格・在庫・購入不可理由の日本語/英語Locale
10. 商店だけで武器、高品質素材、キー素材、Discoveryを入手できないValidation

完了Gate:

- 基本消耗品を購入して遠征バッグへ入れ、帰還・敗北後の所持数が正しい
- 研究前の商品は購入不可、研究後に入荷する
- 在庫0、クラウン不足、倉庫上限時にクラウンだけ減らない
- 武器、分岐武器、高品質素材、キー素材、Discovery、加入、兵種解放を購入できない
- 情報商品は隠し副目標、正確な乱数配置、未発見人物を表示しない
- Save/Loadでクラウン、在庫、購入済み情報商品が復元される
- 施設製作より便利だが割高で、遠征報酬と施設研究の価値を壊さない

## M8 遠征経済完成

状態: **初期6 Itemのみ一部実装**

正本:

- [`item_system.md`](item_system.md)
- [`expedition_rewards.md`](expedition_rewards.md)
- [`expedition_preparation_screen.md`](expedition_preparation_screen.md)
- [`shop_system.md`](shop_system.md)

実装Slice:

1. 消耗品6枠、探索道具2枠の準備画面
2. Item Buttonから使用可能一覧
3. 全消耗品と使用場所制限
4. 探索道具と兵種Choiceの関係
5. 保護箱、帰還信号弾、補給地点
6. 製作、倉庫、Bag、未使用品返却の統合試験
7. 商店購入品と製作品、戦利品、未使用持込品の所有権統合

完了Gate:

- 使用不能理由と対象を操作前に表示
- 遠征中は原則補充しない
- 使用済みと未使用持込品、Pendingを混同しない
- 帰還・敗北・中断再開でItem数が保存される

## M9 残り8地域

状態: **着手済み**。灰鉄採石場: M9-A(地域骨格5地点+分岐(3A/3Bどちらか1つで進行可、
`BranchCompletion::AnyMember`を新規実装)+キャンプ2箇所+地点1「崩落した搬入口」の
実コンテンツ)、M9-B(地点2「砕石段丘」、主目的OR条件用の
`primarySecureTileAlternative`を新規実装)、M9-C(地点3B「巻上機区画」、既存の
`OperateObject`/`objectPlacementRules`のみで実装、コード変更なし)、M9-D(地点5
「崩落核」、地域ボス「灰殻穿岩虫」= `UnitClass::AshironGrubworm`新規、
`takeGrubwormBossTurn()`をAshbough Forestの「灰角大猪」ボスAIパターン踏襲で新規実装、
「封鎖杭2箇所」AND条件は`surveyObjectiveId`ボーナス報酬へ近似)完了(2026-07)。
地点3A「旧採掘坑」・地点4「灰鉄鉱脈」はゲストユニット(護衛対象/操作可能な一時NPC、
現行エンジンに未実装、Cinderwatch`old_barracks`「伝令兵脱出」と同じギャップ)前提の
ため意図的に見送り。灰鉄採石場は地点5(ボス)まで到達可能なコンテンツが揃った状態。

黒水低湿地(第4地域): M9-E(地域骨格7地点+分岐(薬草洲/樹脂林、両方必須・順不同、
既存の`BranchCompletion::AllMembers`をそのまま流用)+キャンプ3箇所+地点1
「灰水の沈み道」の実コンテンツ)完了(2026-07)。地点1の毒蜘蛛は新規`UnitClass`を
作らずWolf相当のステータスを名前だけ差し替えて再利用(Ashiron Quarryの
「Rock Borer」=Bandit再利用と同じ前例)。毒状態異常(`StatusEffectType::Poison`)・
浅瀬地形(`TerrainType::Shallows`)は既存実装をそのまま流用でき、コード変更は
`RegionId::BlackwaterLowlands`の配線(`Region.cpp`/`RouteGraph.cpp`/
`ExpeditionService.cpp`)のみ。M9-F(地点2「葦原の分岐」、主目的OR条件を
`primarySecureTileAlternative`で表現、地形上書き/敵配置非公開は見送り、沼蛇も
Bandit再利用で毒攻撃は未実装のまま)、M9-G(地点3「薬草洲」、主目的OR条件
「3ラウンド防衛、または敵全滅」用に`primarySurviveRoundsAlternative`を新規実装
(`ObjectiveKind::SurviveRounds`自体は既存、OR合成パターンだけが新規)、2ラウンド目
増援は既存の`timedReinforcement`をそのまま流用、`[暁の衛生兵]`ルートは
`routeDiscoveries`で`marsh_emergency_medicine`を付与)完了(2026-07)。副目標
「採取者を撤退させない」/敗北条件「採取者の撤退」は、プレイヤー操作外の中立NPCを
配置・追跡する仕組み(地点3A「旧採掘坑」・地点4「灰鉄鉱脈」・Cinderwatchの
`old_barracks`と同じゲストユニット系ギャップ)が無いため意図的に見送った。M9-H
(地点4「樹脂林」)完了(2026-07)。「樹脂箱2個のうち1個以上を確保」という主目的の
OR代替(`surveyObjectiveId`+`surveyTileCount:2`は既に`ObjectiveGroupRule::Any`
実装済みだが、常にボーナス報酬経路であり主目的化には新規配線が要る)は、正本の報酬表を
確認したところクレート確保専用の追加報酬が無く実装差が薄いと判断し、標準
`EliminateTeam`のみで近似(コード変更無し)。地点3/4の`AllMembers`分岐は両方確保済み
で解決、Camp II以降へ進行可能になった。M9-I(地点5「黒水渡し」)は主目的「荷運び役2人
のうち1人以上を右端へ脱出」自体がゲストユニット系ギャップに依存していたため、まず
ゲストユニット護衛サブシステムを新規実装(完了、2026-07)。追加した型/関数:
`Unit::isGuest`(表示専用フラグ、team/AI/選択ロジックは通常のTeam::Playerユニットと
不変)、`BattleMissionState::guestUnitIds`、`BattleState::allGuestsLost()`
(`allPlayersDefeated()`と同じ形で`evaluateBattleOutcome()`の敗北ゲートへ追加、
guestUnitIdsの全員が`isPresent()==false`になった時点で部隊全滅とは独立に敗北)、
`StageDescriptor::GuestUnitData`/`guestUnits`(`BattleFactory.cpp`の`assembleScenario`
が`Team::Player`・`isGuest=true`で通常ユニットと同じ`units`ベクタへスポーンし
`guestUnitIds`へ登録)、`StageDescriptor::primaryEscapeUnitsAlternative`
(`primaryDefeatUnitId`と同じ「置換であって追加ではない」パターンで`EscapeUnits`を
主目的化、脱出タイルは既存`chooseHoldTile()`を流用)。`ObjectiveTracker.cpp`の
`EscapeUnits`クレジット判定は、`requiredEscapeCount`到達でCompleted確定した後も
`creditedTargetIds`への追記を続けるようガードを緩和(副目標「2人とも脱出」の判定に
必要、`SecureTile`側の挙動は無変更)。これを使って地点5を正本どおり実コンテンツ化
(`blackwaterCrossingStage()`、`Region.cpp`にC++直接記述、`guestUnits`/
`primaryEscapeUnitsAlternative`はJSONスキーマ未対応のためJSON化せず、
`data/regions.json`の旧プレースホルダーエントリは未使用のまま残置)。沼蛇=Bandit・
毒蜘蛛=Wolf再利用は地域内の既存前例に合わせて踏襲。荷物箱副目標は地点4と同じ
`surveyObjectiveId`+`SurveySuccess`RewardRuleパターンを1個用に流用。「荷物を減らして
渡る」の持込品一時封印、`[伝令騎兵]`ルートの護衛対象MOV+1・増援位置公開は該当
インフラが無いため意図的に見送り。`GameApp::proceedToCamp()`へ地点固有の副目標報酬
分岐(2人脱出→高品質薬草1、荷物箱確保→毒素材1)を追加、既存の`mergeLoot`ボーナス
パターンを踏襲。`tests/test_battle.cpp`へ4件追加(単体脱出勝利/ゲスト全滅による
部隊生存下の敗北/両者脱出ボーナス/荷物箱確保ボーナス)。全ビルド・既存テスト含め成功。
地点6〜7はプレースホルダーのまま、M6-B/C方式で1地点ずつ本格化する。残り6地域は未着手

目的: 完成済み共通基盤へ地域データを追加し、本編10地域を構築する。

前提: **M1-E コンテンツ追加基盤の完了**。M9では原則としてDefinitionとLocale、テストデータだけを
追加する。新Mechanicが必要な地点だけ、先に共通MechanicとしてC++実装してからDefinitionで参照する。

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
- 地域追加のために`GameApp`、`BattleController`、`main.cpp`へ地域名・地点名の分岐を追加していない
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
4. Audio基盤と最低限のBGM/SE
   - System: raylib AudioDeviceの初期化/終了、BGM/SE再生窓口、Scene別BGM切替、
     Master/BGM/SE音量、Mute、SaveData round-trip、Webでの初回ユーザー操作後開始
   - Minimum UI: Settings内の音量スライダー、Mute、テストSE、現在値の保存/復元
   - Content: Base/Camp/Battle/Explorationの最低限BGM、決定/キャンセル/攻撃/撃破/
     Objective成功/勝利/敗北の最低限SE
   - Polish: 専用ジングル、フェード、ミックス調整、地域別BGM、演出同期
   - 必要音源メモ(仮。正式なAudio ID/ファイル名は実装時に別途定義):
     - BGM: Title/起動画面、Base、Expedition Preparation、Exploration/Route選択、
       Camp、Battle通常、Battle危機/ボス、Victory、Defeat
     - UI SE: 決定、キャンセル/閉じる、無効操作、Settingsスライダー試聴、
       Save成功、Save失敗、Import/Export成功
     - 戦闘SE: ユニット選択、移動開始/停止、通常攻撃、弓/射撃、槍/突き、
       Skill発動、回復、ダメージ、回避/ミス、撃破、撤退、ターン切替
     - Objective/Route SE: Objective成功、Objective失敗、Object操作、Object破壊、
       SecureTile確保、HoldTile進行、増援予告、増援出現、探索Choice決定、
       Discovery獲得、Loot獲得、地域完了
     - 拠点/経済SE: 施設建設、研究解放、製作、装備変更、倉庫整理/破棄、
       商店購入/売却(M8-A以降)
5. 日本語・英語Locale完成
6. DesktopとWebの主要解像度UI試験
7. Emscripten Buildとブラウザ実機試験
8. GitHub Pages更新前後のSave継続試験
9. Export/Import、破損復旧、将来Schema拒否試験

公開Gate:

- 10地域を新規Saveから本編完了まで通せる
- 62地点に欠落Definition、到達不能、二重報酬がない
- 内部ID、代替文字、文字切れ、UI重なりがない
- Master/BGM/SE音量とMuteが保存され、Desktop/Webで音量0時に無音になる
- ChromeとSafariで保存継続
- 更新前Saveを更新後Buildで維持

## 直近の実装順

次の2 Sliceだけに集中する(M1-E「M9前ブロッカー」は全項目対応済み)。

1. **M9-I 黒水低湿地 地点5(黒水渡し)の実コンテンツ化**
   - `docs/regions/blackwater_lowlands.md`「5. 黒水渡し」どおりの内容
   - M9-Eで新設した`blackwater_crossing`プレースホルダーを置き換え
   - 地点3A「旧採掘坑」・地点4「灰鉄鉱脈」(灰鉄採石場側)はゲストユニット/護衛脱出
     サブシステムが実装されるまで引き続き保留
2. **M7項目3(残り) ユニットページ本体**
   - 差分プレビュー(武器/スキル/特性変更時の「現在/変更後/変わる戦術/失うもの」)
   - 連携作戦(交流区画の会話解放に依存する別システム)
   - 調整特性・武器分岐の他11兵種への拡張(現状Spearmanのみ実装)
   - 武器重複装備の防止・装備スキル2枠UI(全兵種)・探索能力表示・比較パネルは完了、
     一覧の加入候補非表示は既存実装(roster()が未加入候補を含まない)で対応済みと確認

この2 Sliceが通るまで、他Milestoneの先行実装(M8遠征経済、残り地域など)を並行実装しない。

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
