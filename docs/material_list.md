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

| 地域 | 素材ID | 入手地点(勝利報酬) |
|---|---|---|
| 灰枝の森 | `ashbark_strip`、`graymoss_thread`、`sootberry` | 灰枝の林縁/薬草の沢/折れ木の縄張り |
| 沈黙した監視所群 | `belliron_chip`(×2)、`weathered_cord`(×2)、`watchglass_shard`(×2) | 6地点全て |
| 灰鉄採石場 | `grayiron_slag`、`veinstone_powder`、`quarry_chain_link` | 崩落した搬入口/灰鉄鉱脈/巻上機区画 |
| 黒水低湿地 | `blackwater_peat`、`marshlight_spore`、`rot_reed_fiber` | 灰水の沈み道/樹脂林/黒水渡し |
| 風裂き高原 | `thunderworn_stone`、`windcleft_feather`、`highland_fleece` | 風見台/(登り口)/(荷馬車道) |
| 旧辺境集落 | `hearthbrick`、`sootdyed_cloth`、`crest_nail` | 外囲柵/旧穀物庫/共同井戸 |
| 燼火峡谷 | `ember_shell`、`firetrail_sand`、`kilnbone` | 峡谷入口/(岩棚)/硫黄の窪地 |
| 埋没聖堂 | `prayer_wax`、`censer_ash`、`scripture_tile` | 埋没参道/療養室/崩れた礼拝堂 |
| 破砕された前線砦 | `shattered_shield_stud`、`signal_fuse`、`trench_plate` | 外壁/信号広場/旧兵舎 |
| 地図外縁 | `border_dust`、`waystone_fragment`、`stareater_moss` | 最後の既知地点/壊れた監視塔/(石盆地) |

`ashbark_strip`(辺境斥候Tier1防具)以外は現状レシピ未接続で、倉庫に貯まるのみ。
レシピへの接続は各地域を代表する装備を選んで今後対応する。

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
