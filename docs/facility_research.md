# 施設研究ノード正本

文書種別: **正本**
正本範囲: 全施設研究ノードのID、条件、費用、効果、解放地域

状態: JOJIFrontier固有の施設研究仕様として確定。施設の建設・常時利用規則は
[`base_development.md`](base_development.md)、地点ごとの取得物は
[`stage_facility_progression.md`](stage_facility_progression.md)、個別武器・特性の製作費は
[`character_progression.md`](character_progression.md)を正本とする。JSON、C++、Saveの型とAlias、
検証は[`facility_data_contract.md`](facility_data_contract.md)を正本とする。

## 共通規則

- 研究には対応施設の建設、必要Discovery、必要素材、前提研究のすべてが必要
- Discoveryは条件として参照し、消費しない。表の素材だけを消費する
- 研究済みノードは恒久で、再研究、排他選択、有効化枠を設けない
- `製作解放`はレシピを開くだけで完成品を支給しない
- `恒久機能`は研究完了時から拠点UI・遠征準備・情報表示へ反映する
- 地域欄は最初に研究可能になる標準時期。先行取得できても前提条件を無視しない

## 司令所

司令所は常設基礎設備であり、研究に建設費を要求しない。

| 安定ID | 日本語名 | 効果 | 必要Discovery | 必要素材 | 前提研究 | 解放地域 | 種別 |
|---|---|---|---|---|---|---|---|
| `command_scout_network` | 偵察網 | 遠征準備で公開済み地点の敵カテゴリ、敵数範囲、初期配置帯を表示 | 偵察資料 | 木材2、鉄材1 | なし | 沈黙した監視所群 | 恒久機能 |
| `command_map_room` | 地図室 | 地域ルートの既知分岐、キャンプ、帰還可能地点、ボスまでの概算地点数を表示 | 森の踏査記録、監視記録 | 木材2、石材2 | `command_scout_network` | 沈黙した監視所群 | 恒久機能 |
| `command_expedition_planning` | 遠征計画室 | 深層情報、特殊遠征、増援予告資料、帰還設備情報を表示 | 信号技術資料、峡谷踏査記録 | 建築材2、高品質鉄材1、軍需品1 | `command_map_room`、`joint_route_engineering` | 燼火峡谷 | 恒久機能 |

## 訓練所

| 安定ID | 日本語名 | 効果 | 必要Discovery | 必要素材 | 前提研究 | 解放地域 | 種別 |
|---|---|---|---|---|---|---|---|
| `training_vanguard` | 前衛訓練 | 古参守備兵、槍兵、重装兵のTier 2研究を解放 | 前衛訓練資料 | 木材2、獣皮2 | 訓練場建設 | 沈黙した監視所群 | 恒久機能 |
| `training_mobility` | 機動訓練 | 辺境斥候、辺境猟兵、伝令騎兵のTier 2研究を解放 | 監視記録、高原街道図 | 木材2、獣皮2、織物1 | 訓練場建設 | 沈黙した監視所群 | 恒久機能 |
| `training_specialist` | 専門訓練 | 辺境工兵、旗手、暁の衛生兵、戦闘魔導士のTier 2研究を解放 | 野戦工作記録 | 木材2、鉄材2 | 訓練場建設 | 沈黙した監視所群 | 恒久機能 |
| `training_advanced` | 上位訓練 | 初期12兵種のTier 3研究を解放 | 上位防衛訓練記録 | 建築材2、高品質鉄材2、軍需品1 | 上記3系統のうち対応系統、`joint_unit_coordination` | 破砕された前線砦 | 恒久機能 |

## 鍛冶場

| 安定ID | 日本語名 | 効果 | 必要Discovery | 必要素材 | 前提研究 | 解放地域 | 種別 |
|---|---|---|---|---|---|---|---|
| `forge_weapon_branching` | 武器分岐鍛造 | 初期6兵種の第1・第2武器分岐レシピを研究可能にする | 採掘技術記録 | 鉄材2、木材2 | 簡易鍛冶台建設 | 灰鉄採石場 | 製作解放 |
| `forge_heavy_processing` | 重装加工 | 重装武器、防具部品、補強石突きのレシピを解放 | 重装加工記録 | 鉄材3、石材2 | `forge_weapon_branching` | 灰鉄採石場 | 製作解放 |
| `forge_heat_processing` | 耐熱加工 | 耐熱覆い、耐熱武器部品、峡谷用装備のレシピを解放 | 耐熱加工記録 | 耐熱素材3、湿地樹脂1 | `forge_heavy_processing`、`joint_modular_fabrication` | 燼火峡谷 | 製作解放 |
| `forge_special_materials` | 特殊素材加工 | 灰晶、魔物素材、遺跡片を使う横方向武器分岐を解放 | 特殊鍛造記録 | 耐熱素材2、灰晶2 | `forge_heat_processing` | 燼火峡谷 | 製作解放 |

## 診療所

| 安定ID | 日本語名 | 効果 | 必要Discovery | 必要素材 | 前提研究 | 解放地域 | 種別 |
|---|---|---|---|---|---|---|---|
| `clinic_field_medicine` | 野戦医療 | 野戦治療キットと野営食の医療レシピを解放 | 薬草群生地、野戦医療記録 | 木材1、薬草3 | 救護テント建設 | 沈黙した監視所群 | 製作解放 |
| `clinic_lifesaving` | 救命技術 | 救命包の製作と高級救命研究候補を解放 | 集団救護記録 | 薬草3、織物2 | `clinic_field_medicine` | 破砕された前線砦 | 製作解放 |
| `clinic_pharmacology` | 薬学 | 万能薬と毒地域向け治療レシピを解放 | 薬学記録 | 高品質薬草1、毒素材2、湿地樹脂1 | `clinic_field_medicine` | 黒水低湿地 | 製作解放 |
| `clinic_special_medicine` | 特殊医療 | 聖堂式留め具と特殊状態治療研究を解放 | 医療典籍 | 高品質薬草2、聖堂器材1 | `clinic_pharmacology`、`joint_medical_engineering` | 埋没聖堂 | 製作解放 |

## 工房

| 安定ID | 日本語名 | 効果 | 必要Discovery | 必要素材 | 前提研究 | 解放地域 | 種別 |
|---|---|---|---|---|---|---|---|
| `workshop_exploration` | 探索工作 | 採掘具、解体具、照明具のレシピを解放 | 採掘技術記録 | 木材2、石材1 | 工作台建設 | 灰鉄採石場 | 製作解放 |
| `workshop_combat` | 戦闘工作 | 煙幕筒と基本設置物レシピを解放 | 野戦工作記録 | 木材2、鉄材2 | 工作台建設 | 沈黙した監視所群 | 製作解放 |
| `workshop_advanced` | 高度工作 | 帰還信号弾、閃光筒、保護箱の研究候補を解放 | 信号技術資料 | 鉄材2、巻上機部品1 | `workshop_exploration`、`workshop_combat` | 灰鉄採石場 | 製作解放 |
| `workshop_traps` | 罠技術 | 鉄杭と地域罠対策道具を解放 | 罠技術記録 | 湿地樹脂2、硬木1 | `workshop_combat` | 黒水低湿地 | 製作解放 |
| `workshop_arcane` | 魔導工房 | 聖堂装置、遺跡片、焦点具用の工作レシピを解放 | 聖堂装置記録、上位魔法研究記録 | 遺跡片3、高品質鉄材1 | `workshop_advanced`、`joint_modular_fabrication` | 埋没聖堂 | 製作解放 |

## 宿舎

宿舎は常設基礎設備であり、拡張は人物受入上限と会話・連携機能を増やす。

| 安定ID | 日本語名 | 効果 | 必要Discovery | 必要素材 | 前提研究 | 解放地域 | 種別 |
|---|---|---|---|---|---|---|---|
| `quarters_extension_1` | 宿舎増築I | 重装兵・辺境猟兵を含む8人まで受入可能 | 灰枝の森攻略 | なし | なし | 灰枝の森 | 恒久機能 |
| `quarters_specialist_wing` | 専門区画 | 工兵・伝令騎兵・旗手を含む11人まで受入可能 | 野戦工作記録 | なし | `quarters_extension_1` | 沈黙した監視所群 | 恒久機能 |
| `quarters_expedition_annex` | 遠征別棟 | 戦闘魔導士を含む12人を受入可能 | 異常鉱脈記録 | なし | `quarters_specialist_wing` | 灰鉄採石場 | 恒久機能 |
| `quarters_social_wing` | 交流区画 | 連携作戦の研究と地域総括会話を解放 | 集落台帳、集団防衛資料 | なし | `quarters_expedition_annex` | 旧辺境集落 | 恒久機能 |

## 共同施設研究

共同研究には両方の施設または基礎設備が利用可能であること、`collective_facility_methods`の登録、
表のDiscoveryと素材が必要。研究後は恒久で、施設の組合せを毎回選び直さない。

| 安定ID | 日本語名 | 組合せ | 具体的効果 | 必要Discovery | 必要素材 | 前提研究 | 解放地域 | 種別 |
|---|---|---|---|---|---|---|---|---|
| `joint_medical_engineering` | 医療工作連携 | 診療所＋工房 | `clinic_special_medicine`と危険地形用保護道具の研究条件を解放 | 救護記録、聖堂装置記録 | 高品質薬草1、鉄材2、織物1 | `clinic_field_medicine`、`workshop_advanced` | 埋没聖堂 | 恒久機能 |
| `joint_modular_fabrication` | 共通規格加工 | 工房＋鍛冶場 | 耐熱加工、特殊素材加工、魔導工房で共通部品を使う研究条件を解放 | 重装加工記録、耐熱加工記録 | 鉄材2、建築材1、工房部品1 | `forge_heavy_processing`、`workshop_advanced` | 燼火峡谷 | 恒久機能 |
| `joint_unit_coordination` | 部隊連携訓練 | 訓練所＋宿舎 | 連携作戦1枠と上位訓練の研究条件を解放。能力値は直接上げない | 集落台帳、集団防衛資料 | 建築材2、織物2 | `quarters_social_wing`、訓練3系統のいずれか | 旧辺境集落 | 恒久機能 |
| `joint_route_engineering` | 経路支援計画 | 司令所＋工房 | 帰還信号弾の使用可能地点、補給設備、後半ルート情報を表示し遠征計画室の前提を満たす | 信号技術資料、峡谷踏査記録 | 建築材2、鉄材2、信号部品1 | `command_map_room`、`workshop_advanced` | 燼火峡谷 | 恒久機能 |

## データ受入条件

1. 全ノードIDが一意で、表示名変更後もIDを維持する。
2. 必要Discovery、素材ID、前提研究ID、解放地域IDが存在しない場合はデータ読込を失敗させる。
3. 前提研究グラフに循環を作らない。
4. 製作解放ノードは最低1つのレシピから参照される。
5. 恒久機能ノードはUIまたはゲーム処理へ接続され、説明だけの解放にしない。
6. 同じ研究を再実行して素材を二重消費できない。
7. 共同研究なしでも地域の主目的と安全帰還は達成できる。

## 現行実装IDからの移行

現行`Facilities.hpp`のIDは試作Saveで使用済みのため、Schema移行時に次の正式IDへ変換する。
正式IDを旧IDへ戻さず、旧IDは読込Aliasとして最低2 Schema世代保持する。

| 現行ID | 正式ID |
|---|---|
| `scout_network` | `command_scout_network` |
| `map_room` | `command_map_room` |
| `expedition_planning_room` | `command_expedition_planning` |
| `vanguard_training` | `training_vanguard` |
| `mobility_training` | `training_mobility` |
| `specialist_training` | `training_specialist` |
| `weapon_forging` | `forge_weapon_branching` |
| `heavy_reinforcement` | `forge_heavy_processing` |
| `special_material_crafting` | `forge_special_materials` |
| `field_medicine_branch` | `clinic_field_medicine` |
| `lifesaving_technique` | `clinic_lifesaving` |
| `pharmacology` | `clinic_pharmacology` |
| `exploration_tools` | `workshop_exploration` |
| `combat_tools` | `workshop_combat` |
| `advanced_crafting` | `workshop_advanced` |
| `barracks_expansion` | `quarters_extension_1` |
| `specialist_quarters` | `quarters_specialist_wing` |
| `social_quarters` | `quarters_social_wing` |

`training_advanced`、`forge_heat_processing`、`clinic_special_medicine`、`workshop_traps`、
`workshop_arcane`、`quarters_expedition_annex`、共同研究4種は新規IDでありAliasを持たない。
基礎施設IDは建設済み施設IDへ別途移行し、研究ノード集合へ混在させない。
