# 武器/防具Lv2〜5「他地域素材」再設計(設計メモ、未実装)

文書種別: **設計メモ**(まだデータ/コードには反映していない)

## 発覚した問題

`docs/material_list.md`「地域固有素材」の記述は、武器`otherA`/`otherB`・防具`otherA`/`otherB`/
`otherC`を「他地域の素材」として使う想定だった。しかし実際に`data/weapon_leveling.json`/
`data/armor_leveling.json`を確認すると、**全12兵種ともotherA/otherB(/otherC)へ自分自身の
所属地域の素材を入れていた**(例: FrontierScout=灰枝の森所属→otherA=`ashbark_strip`、
otherB=`graymoss_thread`、どちらも灰枝の森自身の素材)。

つまり現状のLv2〜5強化は「他地域へ行かないと完結しない」という設計意図([`deep_layers.md`]
(deep_layers.md)「Lv1〜5数値バランス設計」の"毎Lvに他地域素材を1種混ぜ"、[`material_list.md`]
(material_list.md)の"武器Lv2〜5のotherA/otherBと防具Lv2〜5のotherA/otherB/otherCの計5枠に
各地域の3種を配置")を実現できていない。Lv5の希少素材(`rare_material`、多地域で低頻度入手)を
除き、実質的に他地域への依存はゼロだった。

## 決定した修正方針(2026-08-01)

- Lv2・Lv3・Lv4それぞれで**異なる他地域**の素材を要求する(同じ他地域の使い回し不可)。
- そのためにはデータ構造自体の変更が必要:
  - 武器`WeaponLevelMaterials`は現在`otherA`(Lv2・Lv3で共用)/`otherB`(Lv4のみ)の2枚田
    ([`WeaponLeveling.hpp`](../include/jf/core/WeaponLeveling.hpp)) → `otherLv2`/`otherLv3`/
    `otherLv4`の3枚田へ分割する。
  - 防具`ArmorLevelMaterials`は既に`otherA`/`otherB`/`otherC`の3枚田を持つ
    ([`ArmorLeveling.hpp`](../include/jf/core/ArmorLeveling.hpp))が、実際の運用が
    Lv2=otherA、Lv3=otherA+otherB、Lv4=otherA+otherB+otherCという「積み増し」方式になって
    いる(`armorLevelUpCost()`のswitch文)。武器と同じ「各Lvで単一の他地域素材を追加」方式へ
    揃えるか、積み増し方式を維持したまま各枠を別地域にするかは実装時に選ぶ。
- Lv5は従来どおり`rare_material`(多地域で入手可能な希少枠)のままでよい。「毎回違う地域」の
  制約はLv2〜4の3段階に適用する(Lv5は特定の1地域に紐付かない汎用枠のため対象外)。

## 地域ペアリングの考え方(未確定・実装時に確定)

各兵種の所属地域は次のとおり(`skill_system.md`「Discoveryの地域配置」/`material_list.md`
「地域固有素材」より):

| 所属地域 | 兵種 |
|---|---|
| 灰枝の森 | 辺境斥候 |
| 沈黙した監視所群 | 行軍隊長、監視弓兵 |
| 灰鉄採石場 | 槍兵、重装兵 |
| 黒水低湿地 | 辺境猟兵 |
| 風裂き高原 | 伝令騎兵 |
| 旧辺境集落 | 旗手 |
| 燼火峡谷 | 戦闘魔導士 |
| 埋没聖堂 | 暁の衛生兵 |
| 破砕された前線砦 | 古参守備兵 |
| 地図外縁 | 辺境工兵 |

各兵種のLv2/Lv3/Lv4は、**自分の所属地域以外**から3つの異なる地域を選んで割り当てる。
本編の地域解放順(灰枝の森→監視所群→採石場→…→地図外縁)を輪環とみなし、「+1/+4/+7地域先」
を機械的に割り当てる案が候補(下表は辺境斥候のみ試算、他11兵種は実装時に同様の式で確定):

| Lv | 割り当て地域 | 候補素材(その地域の3種のいずれか) |
|---:|---|---|
| 2 | 沈黙した監視所群(+1) | `belliron_chip` |
| 3 | 風裂き高原(+4) | `thunderworn_stone` |
| 4 | 埋没聖堂(+7) | `prayer_wax` |
| 5 | (地域指定なし) | `rare_material`(変更なし) |

## 未確定事項(実装前に決めること)

- 残り11兵種それぞれの「+1/+4/+7」(または別の式)による具体的な地域割り当てと、その地域の
  3種のうちどれを使うか。
- 防具の「積み増し方式」を維持するか、武器と同じ「単一素材追加方式」へ揃えるか。
- 数量(現在の`weaponLevelRoundedQuantity`ベースの倍率)は据え置くか見直すか。
- 兵種によっては所属地域が同じペア(灰鉄採石場の槍兵/重装兵、監視所群の行軍隊長/監視弓兵)が
  存在する。同じペア内の2兵種で他地域の割り当てを変える(素材の偏り防止)か、揃えるか。
- 実装すると`tests/test_battle.cpp`の`command_sword`/`bulwark_maul`/
  `armor_march_captain_tier1`/`armor_battle_mage_tier2`等、既存の具体的な素材id/数量を
  検証しているテストが軒並み値を変える必要がある(材料idが変わるため)。

## 対象範囲

この修正は現在実装済みの12兵種全部の武器/防具Lv2〜5レシピに影響する(深層Lv6〜20の
`weaponDeepLevelUpCost()`とは別軸で、こちらは本編区間の話)。ユーザー方針により、まずこの
設計メモを作成し、実装は別途合意のうえで着手する。
