# プロンプト: 残り9地域の深層限定素材を設計させる

このファイルはプロンプトそのもの。以下を丸ごとコピーして使う。

---

JOJIFrontier(C++/raylib製タクティクスRPG)の「深層/最深層」システム(`docs/deep_layers.md`)向けに、
残り9地域の深層限定素材を設計してください。

## 背景

深層/最深層は本編クリア後の任意周回コンテンツで、地域(ダンジョン)ごとに専用素材を持ちます。
他地域の素材で代用できません。各地域は「共通深層素材1種」+「ボス素材3種(ユニーク)」の
2階建て構成です。

- 共通深層素材: 通常戦闘(ザコ戦・ボス戦問わず)で入手でき、戦闘を重ねるほど1回の入手量が増える。
- ボス素材3種: 深層内のボス1体目・ボス2体目(深層完了)・最深層の最終ボスをそれぞれ撃破しないと
  入手できない。他のボス・他地域では代用不可。

武器/防具Lv6〜20の強化専用材料として使う。Lv9はボス1体目の素材、Lv13はボス2体目の素材、
Lv20は最深層ボスの素材が追加で1個必須。

## 既に実装済みの例(灰枝の森/AshboughForest)

| 種別 | 素材ID | 表示名(日本語) | 入手条件 |
|---|---|---|---|
| 共通深層素材 | `ashbough_deep_core` | 灰枝深層核 | 深層・最深層のどの戦闘でも入手可 |
| ボス素材(深層1体目) | `ashenhorn_deep_tusk` | 灰角深牙 | 「灰角大猪の眷属『片牙』」撃破限定 |
| ボス素材(深層2体目) | `ashenhorn_deep_horn` | 灰角深角 | 「灰角大猪の頭目『森ノ主』」撃破限定 |
| ボス素材(最深層) | `ashenhorn_deep_relic` | 灰角深遺物 | 「灰角大猪の古老『朽木の王』」撃破限定 |

灰枝の森の深層は既存の`AshenhornBoar`(灰角大猪)を3体ともリスキンして使い回している
(新規UnitClassは追加していない)。素材名は「地域の頭文字+deep_core」「元となる敵の名前+deep_+
部位/性質」という命名パターン。ボスの個体名は「灰角大猪の眷属『片牙』」のように
「[元の敵種]の[役職]『二つ名』」という形式。

## 依頼内容

以下の9地域それぞれについて、上と同じ形式(共通深層素材1種+ボス素材3種、英語ID+日本語表示名)を
設計してください。各地域の既存の世界観・地域固有素材・ボス候補と整合させること。

| 地域(内部ID) | 担当兵種 | 既存の地域固有素材(参考、`docs/material_list.md`) | ボス役の既存UnitClass(リスキン候補) |
|---|---|---|---|
| 沈黙した監視所群(cinderwatch_gate) | 行軍隊長・監視弓兵 | belliron_chip、weathered_cord、watchglass_shard | MarchCaptain(元隊長のリスキン前例あり) |
| 灰鉄採石場(ashiron_quarry) | 槍兵・重装兵 | grayiron_slag、veinstone_powder、quarry_chain_link | AshironGrubworm(灰殻穿岩虫) |
| 黒水低湿地(blackwater_lowlands) | 辺境猟兵 | blackwater_peat、marshlight_spore、rot_reed_fiber | MarshFangSerpent(沼牙の大蛇) |
| 風裂き高原(windscar_plateau) | 伝令騎兵 | thunderworn_stone、windcleft_feather、highland_fleece | PlateauCourierCaptain(高原運び手の隊長) |
| 旧辺境集落(old_frontier_settlement) | 旗手 | hearthbrick、sootdyed_cloth、crest_nail | RaidLeader(襲撃団頭領) |
| 燼火峡谷(ember_ravine) | 戦闘魔導士 | ember_shell、firetrail_sand、kilnbone | RedbackLizard(赤背の大蜥蜴) |
| 埋没聖堂(buried_dawn_sanctum) | 暁の衛生兵 | prayer_wax、censer_ash、scripture_tile | SanctumRetrievalLeader |
| 破砕された前線砦(shattered_march_fort) | 古参守備兵 | shattered_shield_stud、signal_fuse、trench_plate | FortGarrisonCaptain |
| 地図外縁(mapped_edge) | 辺境工兵 | border_dust、waystone_fragment、stareater_moss | FrontierBeast |

## 出力形式

地域ごとに、上の「灰枝の森」の表と同じ4行の表を作成。加えて、ボス3体それぞれの
個体名(「[元の敵種]の[役職]『二つ名』」形式、深層1体目/深層2体目/最深層用)も明記すること。
素材IDは`<地域を表す短い英語>_deep_core`、ボス素材IDは`<敵種の英語名>_deep_<部位/性質を表す
英単語>`のパターンに合わせる。他地域の素材IDと衝突しないこと(既存の全素材IDは
`docs/material_list.md`参照)。

出力は日本語で、`data/deep_layers.json`へそのまま追記できる形(regionId、deepMaterialId、
classId、layerBossMaterialIds配列)も併記すること。
