# 素材リスト

文書種別: **説明文書**
参照正本: [`item_system.md`](item_system.md)、[`inventory_overflow.md`](inventory_overflow.md)、
[`region_mission_data_contract.md`](region_mission_data_contract.md)、[`regions/`](regions/)

この文書は、実装済み/設計済み素材を見やすく確認するための一覧である。素材ID、上限、報酬、
消費条件の最終判断は各正本と`data/regions.json`、`data/locales/ja.json`を優先する。

## 通常素材

| ID | 表示名 | 主な入手地域 | 用途メモ |
|---|---|---|---|
| `wood` | 木材 | 灰枝の森、監視所群 | 初期施設、工作品、野営食 |
| `hide` | 獣皮 | 灰枝の森、風裂き高原 | 訓練場、探索道具、武器特性 |
| `herb` | 薬草 | 灰枝の森、黒水低湿地 | 治療品、診療所研究 |
| `iron` | 鉄材 | 監視所群、採石場、砦 | 工房、武器、設置物 |
| `stone` | 石材 | 採石場、聖堂、砦 | 拠点建設、施設拡張 |
| `old_gear` | 旧軍備 | 沈黙した監視所群 | 旧防衛線系の施設/装備材料 |
| `combustion_oil` | 燃焼油 | 灰鉄採石場 | 爆破、火器、工作系材料 |
| `building_material` | 建築材 | 旧辺境集落、聖堂、砦 | 拠点建設、施設拡張 |
| `food` | 食料 | 旧辺境集落、地図外縁 | 野営食、補給地点 |
| `cloth` | 織物 | 高原、集落、砦 | 医療品、信号具、探索道具 |
| `sulfur` | 硫黄 | 燼火峡谷 | 閃光筒、帰還信号弾、照明具 |
| `wetland_resin` | 湿地樹脂 | 黒水低湿地 | 煙幕筒、耐熱覆い、湿地系装備 |
| `ruin_fragment` | 遺跡片 | 聖堂、地図外縁 | 上位工房、魔導系製作 |
| `sanctum_equipment` | 聖堂器材 | 埋没聖堂 | 特殊医療、聖堂式装備 |
| `military_supplies` | 砦の軍需品 | 破砕された前線砦 | 最終施設、軍需系研究 |

## 高品質/専門素材

| ID | 表示名 | 主な入手地域 | 用途メモ |
|---|---|---|---|
| `quality_herb` | 上質な薬草 | 薬草の沢、黒水低湿地、聖堂 | 万能薬、特殊医療 |
| `quality_iron` | 高品質鉄材 | 採石場、聖堂、砦、地図外縁 | 最終施設、上位装備 |
| `hardwood` | 硬木 | 風裂き高原 | 騎兵装備、強風対策 |
| `riding_gear` | 騎具素材 | 風裂き高原 | 機動装備、騎兵武器 |
| `poison_material` | 毒素材 | 黒水低湿地 | 万能薬、毒地域向け治療 |
| `heat_resistant_material` | 耐熱素材 | 燼火峡谷 | 耐熱装備、熱地形対策 |
| `ash_crystal` | 灰晶 | 燼火峡谷 | 特殊鍛造、魔導工房 |
| `rare_material` | 希少素材 | 地図外縁 | 深層遠征、最終発展 |
| `frontier_edge_material` | 地図外縁専用素材 | 地図外縁 | 深層遠征、最終発展 |

## キー素材/地域進行素材

| ID | 表示名 | 主な入手条件 | 用途メモ |
|---|---|---|---|
| `ashenhorn_fang` | 灰角の大牙 | 灰角大猪を撃破して安全帰還 | 拠点発展キー |
| `ashveil_fang` | 灰霧の大牙 | 沈黙した監視所群のBoss攻略 | 拠点発展候補 |
| `ashenhorn_fragment` | 灰角の欠片 | 大猪を倒木へ衝突させて勝利 | 灰角系の調整素材 |
| `ashiron_shell` | 穿岩殻 | 灰鉄採石場Boss攻略 | 採石場Boss素材 |
| `signal_core` | 信号装置の中核部品 | 監視所群の信号系地点 | 次地域経路/信号技術 |
| `frontier_final_key` | 最終キー素材 | 地図外縁の主目的報酬 | 本編最終発展、深層遠征候補 |

## 地域固有素材(全10地域、通常素材3種ずつ)

[`material_redesign_proposal.md`](material_redesign_proposal.md)の全面再設計案から、
通常素材(基礎素材/レア素材/キー素材のうち「通常」区分)のみを全10地域分導入
(docs/implementation_status.md「素材システム全面再設計」#1)。既存の基礎素材
(木材/獣皮など)は置き換えず、各地点の`baseVictoryLoot`(勝利報酬)へ1個ずつ
並行追加している。レア/キー素材(ボス撃破限定などの上位区分)は未着手。

| 地域 | 素材ID | 入手地点(勝利報酬) | レシピ接続先(担当兵種) |
|---|---|---|---|
| 灰枝の森 | `ashbark_strip`、`graymoss_thread`、`sootberry` | 灰枝の林縁/薬草の沢/折れ木の縄張り | 辺境斥候(武器・防具Lv2〜5) |
| 沈黙した監視所群 | `belliron_chip`(×2)、`weathered_cord`(×2)、`watchglass_shard`(×2) | 6地点全て | 行軍隊長・監視弓兵(武器・防具Lv2〜5) |
| 灰鉄採石場 | `grayiron_slag`、`veinstone_powder`、`quarry_chain_link` | 崩落した搬入口/灰鉄鉱脈/巻上機区画 | 槍兵・重装兵(武器・防具Lv2〜5) |
| 黒水低湿地 | `blackwater_peat`、`marshlight_spore`、`rot_reed_fiber` | 灰水の沈み道/樹脂林/黒水渡し | 辺境猟兵(武器・防具Lv2〜5) |
| 風裂き高原 | `thunderworn_stone`、`windcleft_feather`、`highland_fleece` | 風見台/(登り口)/(荷馬車道) | 伝令騎兵(武器・防具Lv2〜5) |
| 旧辺境集落 | `hearthbrick`、`sootdyed_cloth`、`crest_nail` | 外囲柵/旧穀物庫/共同井戸 | 旗手(武器・防具Lv2〜5) |
| 燼火峡谷 | `ember_shell`、`firetrail_sand`、`kilnbone` | 峡谷入口/(岩棚)/硫黄の窪地 | 戦闘魔導士(武器・防具Lv2〜5) |
| 埋没聖堂 | `prayer_wax`、`censer_ash`、`scripture_tile` | 埋没参道/療養室/崩れた礼拝堂 | 暁の衛生兵(武器・防具Lv2〜5) |
| 破砕された前線砦 | `shattered_shield_stud`、`signal_fuse`、`trench_plate` | 外壁/信号広場/旧兵舎 | 古参守備兵(武器・防具Lv2〜5) |
| 地図外縁 | `border_dust`、`waystone_fragment`、`stareater_moss` | 最後の既知地点/壊れた監視塔/(石盆地) | 辺境工兵(武器・防具Lv2〜5) |

全30種、レシピへの接続が完了した(2026-07-31)。`data/weapon_leveling.json`/
`armor_leveling.json`(旧`WeaponLeveling.hpp`/`ArmorLeveling.hpp`のハードコード表、
docs/implementation_status.md「データ/ロジック分離方針」でJSON化済み)の
`materialsByClass`エントリを、各兵種の所属地域(1〜6兵種は`skill_system.md`
「Discoveryの地域配置」表、後半6兵種は既存の`otherA`/`otherB`が示す地域素材の
出典と同じ地域)に合わせて、汎用素材(quality_iron/riding_gear等)から地域固有の
常在素材へ差し替えた。12兵種を10地域へ割り当てる都合上、灰鉄採石場(槍兵・重装兵)と
沈黙した監視所群(行軍隊長・監視弓兵)の2地域だけ2兵種が同じ3種を共有している。
武器Lv2〜5のotherA/otherBと防具Lv2〜5のotherA/otherB/otherCの計5枠に、各地域の
3種をそれぞれ最低1回は使うよう配置しているため、全30種が実際に消費可能になった。

## 地域固有のレア/キー素材(全10地域、報酬テーブル接続済み)

`docs/implementation_status.md`「素材システム全面再設計」次の方針メモに基づき、
各地域のレア素材2種+キー素材1種、計30種を実際の報酬テーブルへ接続した。
方針どおり、キー素材はボス撃破/地域最終防衛達成、レア素材は危険ルート/
特殊採取地点/ボスドロップへ割り当てている。レシピへの接続はまだ未対応。

| 地域 | レア素材 | キー素材 | 入手地点 |
|---|---|---|---|
| 灰枝の森 | `ashhorn_sinew`(危険ルート)、`forestcore_sap`(特殊採取) | `heartwood_of_graybough` | 折れ木の縄張り(Boss)/薬草の沢 |
| 沈黙した監視所群 | `watchseal`、`old_aiming_device` | `silent_command_board` | 旧兵舎/信号塔下層/最後の信号(Boss) |
| 灰鉄採石場 | `grayiron_core`(特殊採取)、`resonant_stone`(特殊採取) | `quarrymaster_key` | 灰鉄鉱脈/崩落核(Boss) |
| 黒水低湿地 | `blackmarsh_gall`(危険ルート)、`mudbed_pearl` | `scale_of_the_sunken_lord` | 深泥の水源(Boss)/水門 |
| 風裂き高原 | `windbone`(Bossノーダメージ討伐)、`skyresonance_crystal`(危険ルート) | `horn_of_the_peakwarden` | 高原伝令所(Boss)/風見台 |
| 旧辺境集落 | `settler_silver`、`hearthward_stone`(危険ルート) | `first_settlement_tablet` | 共同集会所/旧穀物庫/夜明けの共同防衛 |
| 燼火峡谷 | `meltvein_glass`(特殊採取)、`flameeater_fang`(危険ルート) | `canyon_furnace_core` | 灰晶棚/赤熱の裂け目(Boss) |
| 埋没聖堂 | `martyrs_ring`、`crypt_holy_oil`(特殊採取) | `seal_sanctum_key` | 封印回廊/写本庫/夜明け祭壇(Boss) |
| 破砕された前線砦 | `officers_badge`、`siege_grapnel` | `frontline_command_seal` | 兵站庫/予備壁/切離命令庫(Boss) |
| 地図外縁 | `outerwild_core`(Bossドロップ)、`point_of_no_return_crystal`(特殊探索) | `edge_anchor` | 帰還拠点跡/地図外縁(Boss) |

レシピへの接続(武器・防具・施設・装備スキル)は別途対応が必要。

## 深層限定素材(`docs/deep_layers.md`)

本編クリア後の任意周回コンテンツ「深層/最深層」専用の素材。地域(ダンジョン)ごとに別物で、
他地域の素材で代用不可。武器/防具Lv6〜20の強化専用材料(`data/deep_layers.json`)。
**灰枝の森のみ実際の深層/最深層ダンジョン(戦闘・ボス)まで実装済み**で、残り9地域は
`docs/prompts/deep_layer_materials_prompt.md`で設計した素材id・名称のみ`data/
deep_layers.json`とロケールへ登録済み(実際の戦闘コンテンツは未実装 - 武器/防具Lv計算の
式自体は動く)。

| 地域 | 共通深層素材 | ボス素材(深層1体目 / 深層2体目 / 最深層) | 実装状況 |
|---|---|---|---|
| 灰枝の森 | `ashbough_deep_core` 灰枝深層核 | `ashenhorn_deep_tusk` 灰角深牙 / `ashenhorn_deep_horn` 灰角深角 / `ashenhorn_deep_relic` 灰角深遺物 | 戦闘・ボスまで実装済み |
| 沈黙した監視所群 | `cinderwatch_deep_core` 監視所深層核 | `march_captain_deep_clapper` 無音深鐘舌 / `march_captain_deep_lens` 千里深鏡 / `march_captain_deep_watchseal` 不眠深監印 | 素材idのみ |
| 灰鉄採石場 | `ashiron_deep_core` 灰鉄深層核 | `ashiron_grubworm_deep_mandible` 穿岩深顎 / `ashiron_grubworm_deep_carapace` 灰殻深甲 / `ashiron_grubworm_deep_heartstone` 山喰い深心石 | 素材idのみ |
| 黒水低湿地 | `blackwater_deep_core` 黒水深層核 | `marshfang_serpent_deep_fang` 沼牙深牙 / `marshfang_serpent_deep_scale` 黒沼深鱗 / `marshfang_serpent_deep_gall` 黒水深胆 | 素材idのみ |
| 風裂き高原 | `windscar_deep_core` 風裂き深層核 | `plateau_courier_deep_spur` 風越え深拍車 / `plateau_courier_deep_rein` 雷路深手綱 / `plateau_courier_deep_wayseal` 天駆け深路印 | 素材idのみ |
| 旧辺境集落 | `oldsettlement_deep_core` 旧集落深層核 | `raid_leader_deep_firebrand` 煤爪深火札 / `raid_leader_deep_banner` 家砕き深旗片 / `raid_leader_deep_hearthseal` 炉辺深奪印 | 素材idのみ |
| 燼火峡谷 | `ember_ravine_deep_core` 燼火深層核 | `redback_lizard_deep_tailspine` 赤背深尾棘 / `redback_lizard_deep_throatstone` 炉喰い深喉石 / `redback_lizard_deep_crownscale` 燼冠深鱗 | 素材idのみ |
| 埋没聖堂 | `dawn_sanctum_deep_core` 聖堂深層核 | `sanctum_retriever_deep_censer` 蝋盗り深香炉 / `sanctum_retriever_deep_seal` 灰祈り深封印 / `sanctum_retriever_deep_reliquary` 暁喰らい深聖櫃 | 素材idのみ |
| 破砕された前線砦 | `shattered_fort_deep_core` 破砕砦深層核 | `fort_captain_deep_shieldboss` 折れ盾深盾芯 / `fort_captain_deep_gatekey` 退かず深門鍵 / `fort_captain_deep_commandseal` 墓守深軍印 | 素材idのみ |
| 地図外縁 | `mapped_edge_deep_core` 外縁深層核 | `frontier_beast_deep_claw` 道外れ深爪 / `frontier_beast_deep_waystone` 標喰い深方位石 / `frontier_beast_deep_anchor` 地図端深楔 | 素材idのみ |

各地域とも「共通素材(通常戦闘で入手、進むほど量が増える)+ボス素材3種(撃破限定・ユニーク・
代用不可)」の2階建て構成。ボス役は各地域の既存UnitClass(元隊長/灰殻穿岩虫/沼牙の大蛇等)を
3体ともリスキンして使い回す想定(灰枝の森の`AshenhornBoar`と同じ前例)。拠点Lv15(中継拠点)/
Lv20(最奥拠点)昇格のコストにも各地域の共通深層素材を使う想定
(`BaseState::outpostLevelCheckpoints()`、現状は灰枝の森分のみ実装)。
詳細な戦闘別入手量・敵一覧(灰枝の森分)は
[`regions/ashbough_forest_bestiary.md`](regions/ashbough_forest_bestiary.md)、
Lv別の要求量一覧は[`equipment_levels.md`](equipment_levels.md)参照。

## 実用品寄りの戦利品

| ID | 表示名 | 主な入手地域 | 現行用途 |
|---|---|---|---|
| `gate_tools` | 関門工具 | 沈黙した監視所群 | 鍛冶場・施設Node素材 |
| `ash_road_map` | 灰街道の地図 | 沈黙した監視所群 | 地図室・鍛冶場Node素材 |
| `field_medicine` | 野戦医療資材 | 沈黙した監視所群 | 診療所Node素材 |
| `watch_ledger` | 監視所の台帳 | 沈黙した監視所群 | 司令所・鍛冶場Node素材 |
| `captains_seal` | 隊長の印章 | 沈黙した監視所群 | 地域攻略証明素材 |

## 注意

- 倉庫では素材上限とキー素材上限が別扱い。
- `military_supplies`は画面上で「砦の軍需品」と出す方が分かりやすい。
- 内部IDが画面へ出た場合はローカライズ漏れとして扱う。
