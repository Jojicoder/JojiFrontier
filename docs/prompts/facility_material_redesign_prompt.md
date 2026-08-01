# プロンプト: 拠点施設(非Forge)の製作費を再設計させる

このファイルはプロンプトそのもの。以下を丸ごとコピーして使う。

---

JOJIFrontier(C++/raylib製タクティクスRPG)の拠点施設ノードの製作費(`data/facilities.json`)を
再設計してください。

## 背景・問題

拠点施設は118ノードあり、うち95ノードは武器/防具の製作レシピ(`facility: "Forge"`、
武器分岐・防具Tier1)で、これは既に`docs/equipment_levels.md`で個別に扱っている。残り23ノードが
「拠点そのものの機能」を解放する施設(司令所/訓練場/工房/宿舎/診療所)で、今回はこの23ノードが
対象。

現状の23ノードは、ほぼ`wood`(木材)/`iron`(鉄材)/`hide`(獣皮)/`herb`(薬草)/`cloth`(織物)
といった汎用素材だけで構成されていて、個性がない。一方でゲームには地域ごとに個性ある素材が
大量に存在する(`docs/material_list.md`参照: 全10地域×3種の「地域固有素材」、全10地域×2種の
「地域固有レア素材」+1種の「キー素材」)。この個性ある素材を施設製作費にも混ぜて、
「拠点発展そのものが各地域の攻略と結びついている」という手触りを出したい。

## 制約

- **物語進行と矛盾しない範囲で素材を選ぶこと**。各施設ノードには`requiredStage`
  (Encampment→PioneerOutpost→FrontierSettlement→PioneerCity、拠点の開拓段階)と
  `requiredDiscoveries`(特定地域の特定条件達成で得るDiscovery)が設定されている。
  `requiredStage`が低い(＝ゲーム序盤で解放される)施設に、後半地域でしか手に入らない素材を
  要求してはいけない。目安: Encampment/PioneerOutpost段階の施設は灰枝の森・沈黙した監視所群
  ・灰鉄採石場あたりまでの素材、FrontierSettlement段階の施設はそこまでに加えて黒水低湿地・
  風裂き高原あたりまでの素材、というように段階的に解禁地域を広げること。
- **完全な代用ではなく、既存の汎用素材(wood/iron/hide等)は残しつつ、地域固有素材を1〜2種
  追加する形にする**(武器/防具Lv2〜5の「主材料は変えず他地域素材を追加で混ぜる」設計と
  同じ考え方)。
- 数量は既存の1〜4個の水準を大きく外れないこと(倍率だけ変えるのではなく、種類を増やす)。
- 既に`requiredDiscoveries`で特定地域と紐付いている施設(例: `mobility_training`は
  `cinderwatch_recon_records`)は、その地域の素材を優先的に使うこと。

## 対象23ノード(現状)

`id | facility | 日本語名 | requiredStage | requiredDiscoveries | 現在のmaterialCosts`

```
operations_tent | CommandPost | 作戦テント | Encampment | [] | []
scout_network | CommandPost | 偵察網 | PioneerOutpost | [cinderwatch_recon_records] | [watch_ledgerx1, woodx2, ironx1]
map_room | CommandPost | 地図室 | PioneerOutpost | [] | [ash_road_mapx1, woodx2, stonex2]
expedition_planning_room | CommandPost | 遠征計画室 | FrontierSettlement | [signal_tower_return_signal_records] | [captains_sealx1, building_materialx2, quality_ironx1, military_suppliesx1]
training_field | TrainingGround | 訓練場 | PioneerOutpost | [] | [woodx3, hidex2]
vanguard_training | TrainingGround | 前衛訓練 | PioneerOutpost | [] | [woodx2, hidex2, quality_ironx1]
mobility_training | TrainingGround | 機動訓練 | PioneerOutpost | [cinderwatch_recon_records] | [woodx2, hidex2, clothx1, riding_gearx1]
specialist_training | TrainingGround | 専門訓練 | FrontierSettlement | [] | [woodx2, ironx2, quality_herbx1, military_suppliesx1]
magic_training | TrainingGround | 魔導訓練 | FrontierSettlement | [] | [woodx2, ironx2, quality_herbx1, military_suppliesx1]
field_infirmary | Infirmary | 救護テント | PioneerOutpost | [herb_thicket_grounds] | [woodx2, herbx2]
field_medicine_branch | Infirmary | 野戦医療 | PioneerOutpost | [ironwatch_field_medicine_records] | [woodx1, herbx3, quality_herbx1]
lifesaving_technique | Infirmary | 救命技術 | PioneerOutpost | [] | [herbx3, clothx2, military_suppliesx1]
pharmacology | Infirmary | 薬学 | FrontierSettlement | [marsh_pharmacology_records] | [quality_herbx1, poison_materialx2, wetland_resinx1]
workshop_bench | Workshop | 工作台 | PioneerOutpost | [] | [woodx3, hidex1]
exploration_tools | Workshop | 探索工作 | PioneerOutpost | [] | [woodx2, stonex1, quality_ironx1]
combat_tools | Workshop | 戦闘工作 | PioneerOutpost | [] | [woodx2, ironx2, military_suppliesx1]
trapcraft | Workshop | 罠技術 | FrontierSettlement | [marsh_trapcraft_records] | [wetland_resinx2, hardwoodx1, poison_materialx1]
advanced_fieldwork | Workshop | 上位戦闘工作 | FrontierSettlement | [advanced_fieldwork_records] | [heat_resistant_materialx2, sulfurx1, ash_crystalx1]
advanced_crafting | Workshop | 高度工作 | FrontierSettlement | [signal_tower_return_signal_records] | [ironx2, signal_corex1, sulfurx1, clothx1]
communal_tent | Barracks | 共同テント | Encampment | [] | []
barracks_expansion | Barracks | 宿舎増築 | PioneerOutpost | [] | [building_materialx2, foodx1]
specialist_quarters | Barracks | 専門区画 | FrontierSettlement | [] | [building_materialx3, clothx1, military_suppliesx1]
social_quarters | Barracks | 交流区画 | PioneerOutpost | [] | [building_materialx2, clothx2]
```

`operations_tent`/`communal_tent`はコスト0の基礎設備(枠を消費しない初期解放施設)なので対象外
(現状維持)。

## 地域固有素材カタログ(参考、`docs/material_list.md`)

| 地域 | 常在素材3種(通常戦闘で入手) | レア素材2種 | キー素材1種 |
|---|---|---|---|
| 灰枝の森 | ashbark_strip、graymoss_thread、sootberry | ashhorn_sinew、forestcore_sap | heartwood_of_graybough |
| 沈黙した監視所群 | belliron_chip、weathered_cord、watchglass_shard | watchseal、old_aiming_device | silent_command_board |
| 灰鉄採石場 | grayiron_slag、veinstone_powder、quarry_chain_link | grayiron_core、resonant_stone | quarrymaster_key |
| 黒水低湿地 | blackwater_peat、marshlight_spore、rot_reed_fiber | blackmarsh_gall、mudbed_pearl | scale_of_the_sunken_lord |
| 風裂き高原 | thunderworn_stone、windcleft_feather、highland_fleece | windbone、skyresonance_crystal | horn_of_the_peakwarden |
| 旧辺境集落 | hearthbrick、sootdyed_cloth、crest_nail | settler_silver、hearthward_stone | first_settlement_tablet |
| 燼火峡谷 | ember_shell、firetrail_sand、kilnbone | meltvein_glass、flameeater_fang | canyon_furnace_core |
| 埋没聖堂 | prayer_wax、censer_ash、scripture_tile | martyrs_ring、crypt_holy_oil | seal_sanctum_key |
| 破砕された前線砦 | shattered_shield_stud、signal_fuse、trench_plate | officers_badge、siege_grapnel | frontline_command_seal |
| 地図外縁 | border_dust、waystone_fragment、stareater_moss | outerwild_core、point_of_no_return_crystal | edge_anchor |

地域の解放順(本編チェーン): 灰枝の森→沈黙した監視所群→灰鉄採石場→黒水低湿地→風裂き高原→
旧辺境集落→燼火峡谷→埋没聖堂→破砕された前線砦→地図外縁。

## 依頼内容

上記23ノードそれぞれについて、新しい`materialCosts`(既存の汎用素材+地域固有素材1〜2種)を
提案してください。出力は次の形式で、`data/facilities.json`へそのまま反映できるようにすること。

```
<id>:
  変更前: [...]
  変更後: [{"id": "...", "quantity": N}, ...]
  選定理由: (どの地域のどの段階の素材を、なぜ選んだか)
```
