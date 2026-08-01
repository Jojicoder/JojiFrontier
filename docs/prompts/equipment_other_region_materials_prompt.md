# プロンプト: 武器/防具Lv2〜5の「他地域素材」を再設計させる

このファイルはプロンプトそのもの。以下を丸ごとコピーして使う。

---

JOJIFrontier(C++/raylib製タクティクスRPG)の武器/防具Lv2〜5強化コスト
(`data/weapon_leveling.json`/`data/armor_leveling.json`の`materialsByClass`)を再設計して
ください。

## 背景・問題

武器Lv2〜5・防具Lv2〜5の強化コストは、主材料(自分の地域の素材)に加えて`otherA`/`otherB`
(防具は`otherA`/`otherB`/`otherC`)という「他地域素材」を混ぜる設計になっている
(`docs/deep_layers.md`「Lv1〜5数値バランス設計」: "毎Lvに他地域素材を1種混ぜ、今いる
地域だけでは完結しない状態を作る")。

しかし実際のデータを確認したところ、**全12兵種とも`otherA`/`otherB`/`otherC`に
自分自身の所属地域の素材が入っていた**(バグ)。つまり「他地域に行かないと完結しない」
という設計意図が実現できていなかった。

さらに、`otherA`は武器のLv2・Lv3の両方で使い回され、`otherB`はLv4で使われる(防具は
`otherA`=Lv2、`otherB`=Lv3で追加、`otherC`=Lv4で追加、という積み増し方式)。この
「同じ他地域素材をLv2とLv3で使い回す」点も改善したい: **Lv2・Lv3・Lv4はそれぞれ別の
他地域**から素材を取ること。

### データ構造の変更(実装側で対応済み、素材の中身だけ設計してほしい)

- 武器`WeaponLevelMaterials`は`otherA`(Lv2・Lv3で共用)/`otherB`(Lv4)の2枚田から、
  `otherLv2`/`otherLv3`/`otherLv4`の3枚田へ分割する。
- 防具`ArmorLevelMaterials`は既に`otherA`/`otherB`/`otherC`の3枚田を持つが、意味を
  「Lv2用/Lv3用/Lv4用にそれぞれ独立した他地域1種」に変更する(現在の「積み増し」方式は
  維持してもよいが、3枠それぞれ異なる地域にする)。
- Lv5は従来どおり`rare_material`(希少素材、多地域で低頻度入手)のまま変更しない。

## 制約

- 各兵種の武器Lv2/Lv3/Lv4、防具Lv2/Lv3/Lv4は、**それぞれ異なる他地域**(自分の所属地域を
  除く)から1種ずつ選ぶこと。同じ他地域を2回使わない。
- 武器と防具で同じ他地域構成を使い回してよい(同じ兵種内で武器Lv2と防具Lv2が同じ他地域でも
  問題ない)。
- 各地域の「地域固有素材3種」(下記カタログ)から選ぶこと。他の素材カテゴリ(レア/キー素材)は
  対象外。
- 数量は現在の水準(Lv2=1個、Lv3=2個、Lv4=1個)を維持してよい。

## 対象12兵種の現状(全て自分の所属地域の素材になっているバグ状態)

`兵種 | 所属地域 | 武器otherA/otherB | 防具otherA/otherB/otherC`

```
MarchCaptain | 沈黙した監視所群 | belliron_chip/weathered_cord | belliron_chip/weathered_cord/watchglass_shard
VeteranGuard | 破砕された前線砦 | shattered_shield_stud/signal_fuse | shattered_shield_stud/signal_fuse/trench_plate
WatchArcher | 沈黙した監視所群 | watchglass_shard/belliron_chip | watchglass_shard/belliron_chip/weathered_cord
FrontierScout | 灰枝の森 | ashbark_strip/graymoss_thread | ashbark_strip/graymoss_thread/sootberry
Spearman | 灰鉄採石場 | grayiron_slag/veinstone_powder | grayiron_slag/veinstone_powder/quarry_chain_link
DawnChirurgeon | 埋没聖堂 | prayer_wax/censer_ash | prayer_wax/censer_ash/scripture_tile
HeavyInfantry | 灰鉄採石場 | veinstone_powder/quarry_chain_link | veinstone_powder/quarry_chain_link/grayiron_slag
FrontierEngineer | 地図外縁 | border_dust/waystone_fragment | border_dust/waystone_fragment/stareater_moss
MessengerCavalry | 風裂き高原 | thunderworn_stone/windcleft_feather | thunderworn_stone/windcleft_feather/highland_fleece
FrontierRanger | 黒水低湿地 | blackwater_peat/marshlight_spore | blackwater_peat/marshlight_spore/rot_reed_fiber
BannerBearer | 旧辺境集落 | hearthbrick/sootdyed_cloth | hearthbrick/sootdyed_cloth/crest_nail
BattleMage | 燼火峡谷 | ember_shell/firetrail_sand | ember_shell/firetrail_sand/kilnbone
```

## 地域固有素材カタログ(常在素材3種、`docs/material_list.md`)

| 地域 | 常在素材3種 |
|---|---|
| 灰枝の森 | ashbark_strip、graymoss_thread、sootberry |
| 沈黙した監視所群 | belliron_chip、weathered_cord、watchglass_shard |
| 灰鉄採石場 | grayiron_slag、veinstone_powder、quarry_chain_link |
| 黒水低湿地 | blackwater_peat、marshlight_spore、rot_reed_fiber |
| 風裂き高原 | thunderworn_stone、windcleft_feather、highland_fleece |
| 旧辺境集落 | hearthbrick、sootdyed_cloth、crest_nail |
| 燼火峡谷 | ember_shell、firetrail_sand、kilnbone |
| 埋没聖堂 | prayer_wax、censer_ash、scripture_tile |
| 破砕された前線砦 | shattered_shield_stud、signal_fuse、trench_plate |
| 地図外縁 | border_dust、waystone_fragment、stareater_moss |

同じ地域内で武器/防具が2兵種(監視所群=行軍隊長+監視弓兵、採石場=槍兵+重装兵)を抱えている
ため、この2地域はどちらの兵種の他地域選びにも使わない(自分の所属地域なので対象外)。

## 依頼内容

12兵種それぞれについて、次の形式で新しい割り当てを提案してください。

```
<兵種>:
  武器: Lv2=<地域>の<素材id>、Lv3=<地域>の<素材id>、Lv4=<地域>の<素材id>
  防具: Lv2=<地域>の<素材id>、Lv3=<地域>の<素材id>、Lv4=<地域>の<素材id>
  選定理由: (なぜその地域・素材を選んだか、簡潔に)
```

最後に、`data/weapon_leveling.json`/`data/armor_leveling.json`へ反映できる形の
JSON(`otherLv2`/`otherLv3`/`otherLv4`キー)もまとめて出力してください。

---

# 追加依頼: 武器/防具Lv6〜20にも同じ「他地域素材」を混ぜる設計

## 背景

`docs/deep_layers.md`「他地域深層素材の混合要求」(2026-08-01決定事項)により、Lv6〜20
(深層帯)にもLv2〜5と同じ考え方を適用する方針が決まっている: 自地域の深層素材
(`<地域>_deep_core`、代用不可)を主材料としつつ、**他地域の深層素材も少量追加要求**する
(代用ではなく追加混合)。ボス撃破チェックポイント(Lv9/Lv13/Lv20、自地域のボス素材が必須)
はこの対象外 - 既にそのLvだけ別枠で他地域と無関係な自地域ボス素材を要求しているため。

現状の`weaponDeepLevelUpCost()`(`WeaponLeveling.hpp`)は自地域の`deepMaterial`(共通素材)+
ボス素材(Lv9/13/20のみ)だけで構成されており、他地域深層素材の混合はまだ未実装
(コード中に`TODO(他地域深層の横展開時)`コメントあり)。

## 対象12兵種の所属地域と深層共通素材(`data/deep_layers.json`)

```
FrontierScout | 灰枝の森 | ashbough_deep_core
MarchCaptain | 沈黙した監視所群 | cinderwatch_deep_core
WatchArcher | 沈黙した監視所群 | cinderwatch_deep_core
Spearman | 灰鉄採石場 | ashiron_deep_core
HeavyInfantry | 灰鉄採石場 | ashiron_deep_core
FrontierRanger | 黒水低湿地 | blackwater_deep_core
MessengerCavalry | 風裂き高原 | windscar_deep_core
BannerBearer | 旧辺境集落 | oldsettlement_deep_core
BattleMage | 燼火峡谷 | ember_ravine_deep_core
DawnChirurgeon | 埋没聖堂 | dawn_sanctum_deep_core
VeteranGuard | 破砕された前線砦 | shattered_fort_deep_core
FrontierEngineer | 地図外縁 | mapped_edge_deep_core
```

(ボス素材3種は各地域ごとに`docs/material_list.md`「深層限定素材」参照。今回の追加混合の
対象外なので割愛。)

## 制約

- Lv6〜20のうち、**ボス撃破チェックポイントではない3つのLv**(目安: 深層前半の途中1つ、
  深層後半の途中1つ、最深層の途中1つ - 例えばLv8/Lv11/Lv17のような配置)を選び、それぞれ
  異なる他地域の深層共通素材(`<地域>_deep_core`)を少量(1〜2個)追加要求すること。
  Lv2〜5の設計と同じく、選んだ3つのLvは3つとも異なる他地域にすること。
- 自地域は使わない。既存のボス素材要求Lv(9/13/20)には触れない(そのまま)。
- 数量はLv2〜5より控えめにする(自地域の`deepMaterial`要求(2+(Lv-6)個)に対する「少量の
  追加」という位置づけを保つ - 1〜2個を目安)。
- 武器と防具で同じLv・同じ他地域構成を使ってよい。

## 依頼内容

12兵種それぞれについて、次の形式で提案してください。

```
<兵種>:
  Lv<N1>=<他地域>の<深層共通素材id>×<数量>
  Lv<N2>=<他地域>の<深層共通素材id>×<数量>
  Lv<N3>=<他地域>の<深層共通素材id>×<数量>
  選定理由: (なぜこの3つのLv・地域を選んだか)
```

最後に、`weaponDeepLevelUpCost()`/`armorDeepLevelUpCost()`(`WeaponLeveling.hpp`)へ
そのまま反映できる形で、`UnitClass`ごとの「{targetLevel, 他地域素材id, 数量}」の一覧も
まとめて出力してください。
